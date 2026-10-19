# Database and backup size reduction assessment

Date: 2026-09-28. Status: **ANALYSIS ONLY — no runtime, retention, database,
backup, or producer configuration changes made.**

## Conclusion

There are two substantial opportunities with different guarantees:

1. Share immutable SST files between the active database and local recovery
   snapshots. The current deployment has approximately **29.18 GB of candidate
   duplicate SST content**. Sharing could bring its current approximately
   **58.80 GB** data-directory footprint toward **29.62 GB**, before subsequent
   compaction and retention changes. This is a metadata-based projection, not
   an implemented or measured migration. An independent remote backup is still
   necessary if protection from shared-file corruption or disk loss is required.
2. Separate full validation and production from indefinite local history
   retention. Approximately **99.2% of the active database directory is block
   history storage**, while current-state SST files occupy approximately
   **50.27 MB**. A producer with bounded local history is a promising product
   direction, but its minimum safe storage requirement has not been established.

For a node retaining all current history and APIs, larger compression units,
more compact record/index encoding, and an immutable historical archive are
the relevant database improvements. Simply enabling Zstandard is not useful:
it is already enabled and present in every inspected SST.

All GB/MB figures below use decimal units. Measurements describe this
deployment and this date, not a universal node-size promise.

## 1. Evidence and method

The authorized production host was inspected through read-only SSH, filesystem
metadata, existing backup manifests, bounded immutable SST reads, and loopback
RPC. The running binary reported `teleno_node 1.2.0+b8dab4c08f99`. The local
review checkout was `8a13a5084b6874d9f61475ea6a20860183e4b2cb`; the inspected
storage manager, block-store implementation, checkpoint manager, snapshot
repository, and chain indexer were unchanged between those commits.

The live profile enabled block production and full block verification;
account-history, transaction-store, and contract-metadata services were
disabled. Small historical index files nevertheless remained on disk.
The chain advanced from height 39,757,029 to 39,757,109 during inspection.

The filesystem was ext4. The inspection did not open the active RocksDB
database through a second database process, trigger flush or compaction,
create a checkpoint, delete anything, or restart the producer. Existing
`sst_dump --command=none --show_properties` inspected the properties of 479
SST files without scanning their contents. All property reads succeeded.
The source database and backup payloads remain on the operator's host.

### Physical footprint

| Component | Allocated size | Interpretation |
| --- | ---: | --- |
| Active database directory | 29.4816 GB | Includes SSTs, WAL, and RocksDB diagnostic logs |
| `blocks` SSTs | 29.2463 GB | 465 files; approximately 99.2% of the directory |
| `chain_state` SSTs | 50.2743 MB | Current-state tables; not the whole working set or a deployable producer image |
| Remaining optional index SSTs | 19.7386 MB | Old account-history and transaction-index data |
| RocksDB diagnostic logs | 87.1055 MB | 191 files; a small housekeeping opportunity |
| WAL allocation | 73.8222 MB | Must retain durability and recovery guarantees |
| Local backup tree | 29.3158 GB | Two completed snapshots share repository objects |
| Entire node data directory | 58.7988 GB | Includes the active database, backups, and small auxiliary files |

The inventory is a live observation, not an atomic database checkpoint. SST
properties count physical entries and versions, not proven distinct canonical
blocks. Their aggregate uncompressed block keys and values total 93.8954 GB;
the approximately 29.25 GB SST footprint already reflects substantial compression.

### Backup overlap

The September 27 and September 28 snapshots contain 481 and 486 file entries.
They share 480 content-addressed objects, representing 29.2909 GB. The unique
object payload across both snapshots is 29.3107 GB. The older snapshot adds
only **77,166 payload bytes** beyond the newer snapshot. Deleting that older
snapshot would therefore recover very little space, plus small metadata and
allocation overheads.

Matching backup manifest paths and sizes against active SST files found 468
candidate shared files totaling 29,176,223,609 bytes. None shared an inode with
its backup object. For three candidate pairs, the first and last 64 KiB were
also identical. This is evidence of duplication, not a full hash verification
of all candidate bytes; a migration must verify every file it shares.

## 2. Bounded compression experiment

To avoid a production rewrite, a small experiment operated entirely in
memory. It read the first 500 records from each of eight block SST files:
`008381`, `008443`, `008505`, `008566`, `008629`, `008693`, `008757`, and
`008819`. The 4,000 records span heights 19,152 through 39,571,203. This is an
exploratory sample of hash-ordered tables, not a random or workload-weighted
sample of the whole chain.

The sample contained 8,193,748 bytes of protobuf values and 136,000 bytes of
user keys. Receipts represented 6,115,837 bytes, or **74.64% of raw values**.
3,868 records had no explicit transactions (96.7%). Such blocks still carry
consensus information, and their application can change state and rewards;
they must not simply be discarded.

For reproducibility, each sample record was framed as little-endian 32-bit key
length, 32-bit value length, key, and value. The concatenated stream was split
into independent fixed-size chunks and compressed with libzstd 1.5.5.
Receipt removal omitted protobuf field 4 of `block_record`; skip-pointer
removal omitted field 5. Neither transformed stream was installed or tested
as a valid database.

| Experiment | Compressed sample bytes | Reduction against baseline |
| --- | ---: | ---: |
| Baseline: 64 KiB chunks, Zstd level 3 | 2,656,896 | — |
| 64 KiB chunks, Zstd level 9 | 2,546,668 | 4.15% |
| 256 KiB chunks, Zstd level 3 | 2,373,915 | 10.65% |
| 1 MiB chunks, Zstd level 3 | 2,277,900 | 14.26% |
| Without receipts, 64 KiB/level 3 | 1,460,207 | 45.04% |
| Without skip pointers, 64 KiB/level 3 | 2,477,798 | 6.74% |

These are measured reductions for the framed sample only. They exclude SST
index/restart encoding, realistic cache behavior, migration space, and
replacement indexes. The current SST compression level is the RocksDB default
sentinel; level 3 is the experimental baseline, not a claim that every existing
SST was produced with an explicitly configured level 3. The live blocks CF has
64 KiB table blocks and no compression dictionary configured.

The savings are not additive. Applying sample percentages to the entire
database would be a hypothesis requiring a representative offline rebuild.

## 3. Candidate designs

### A. Share immutable SSTs for local recovery; retain independent backup

The native checkpoint uses RocksDB's checkpoint facility. On the same
filesystem, that facility can hard-link immutable SST files. The next step,
`copy_or_link_object()` in the snapshot repository, currently calls
`std::filesystem::copy_file()`. Consequently, the content-addressed backup
repository deduplicates snapshots against one another while keeping another
physical copy of files still present in the active database.

A proposed local recovery repository can hard-link verified immutable SSTs
from a consistent checkpoint and copy mutable metadata/WAL files. This can
preserve historical data, block serving, and local rollback while eliminating
most current duplication. Files must remain immutable through every supported
restore and repair path; publication, garbage collection, corruption handling,
and link counts need explicit tests.

This changes the protection boundary. A shared SST is not a second independent
copy: corruption of that inode affects the live DB and local snapshot. Keep an
independent verified copy on another storage device or remote repository where
that protection is required. A reflink-capable filesystem would provide
copy-on-write behavior but still shares physical storage initially; the
observed ext4 deployment does not offer a simple reflink substitute.

Expected benefit: the **29.18 GB candidate overlap** is the strongest current
local-space opportunity. As compaction replaces SSTs, older snapshots pin old
files and the overlap changes. The approximately 29.62 GB projected combined
footprint is a snapshot estimate, not a permanent ceiling. This optimization
does not reduce the standalone active DB or bytes of an independent remote copy.

### B. Tune compression where historical data is cold

Benchmark 128/256 KiB table blocks, higher bottommost compression levels, and
trained Zstd dictionaries. The sample suggests larger compression units are a
more promising first experiment than raising compression level alone.

Larger blocks require more decompression and cache space for small reads.
Changes must be measured under P2P history requests, catch-up, restart, and
production load. Dictionary training and higher levels also consume resources.
Existing SSTs do not shrink just because configuration changes; rewriting
them needs temporary space and can create another generation of backup objects.
Avoid a live full-compaction experiment on the producer.

Evidence: **4–14% savings in the sample**, depending on the experiment. No
whole-database saving has yet been measured. RocksDB documents the relevant
[compression settings](https://github.com/facebook/rocksdb/wiki/Compression)
and [dictionary support](https://github.com/facebook/rocksdb/wiki/Dictionary-Compression).

### C. Store irreversible history in stable, height-ordered archive segments

Keep mutable recent blocks and forks in RocksDB. After finality, move canonical
history into immutable segments ordered by height, with seekable compressed
frames and checksums. A separate hash-to-location index retains block-by-ID
queries. Commit archive publication and index transitions atomically before
retiring the old records.

Current hash ordering mixes distant heights, as the sample demonstrates.
Chronological storage could expose repeated producers, structured receipt
fields, and related state updates to the compressor. It also makes old segments
stable across future updates, reducing compaction churn and backup duplication.
The sample does not quantify the extra benefit of chronological ordering.

Further experiments can use column-oriented receipt fields, per-segment
address dictionaries, delta-coded heights/timestamps, and shared representations
of repeated metadata. Preserve exact logical block/receipt content, unknown
fields, signatures, state changes, and RPC behavior. Block signatures and other
high-entropy cryptographic values impose a real compression floor.

For an immutable segment, a static perfect-hash lookup plus compact offsets
is worth comparing with an LSM hash index. Non-members must still be rejected
by verifying the requested full block ID against the retrieved record. Recent
forks need their own mutable index. Index cost must be included in every saving.

This is the strongest architectural path for retaining a full local archive,
but is a storage-format project with migration and compatibility work.

### D. Separate old receipts from signed block history

Receipts are the largest raw component in the sample. Keep recent receipts
locally and move old receipts to a compressed archive or optional remote tier.
Signed historical blocks can remain local for peer service and re-execution.

Removing receipts reduced the compressed sample by **45.04%**. This is evidence
for separating their lifecycle, not authorization to delete them. The current
indexer requests receipts, and checked delta replay uses them. Historical RPC,
restore, catch-up, and index rebuilding need explicit behavior when receipts
are archived or unavailable. Recomputing a receipt requires the appropriate
prior state and deterministic execution; it is not a free lookup.

Archiving receipts elsewhere reduces the producer's local footprint, but does
not by itself reduce total system storage. Losslessly compressing or changing
their representation is a separate experiment. Keep an archival mode for
operators who need complete historical receipt service.

### E. A full validator/producer with bounded local history

The follow-up [bounded-history producer design](../implementation/bounded-history-producer-design.md)
traces the concrete dependencies, durable pruning rule, bootstrap trust,
legacy-peer constraints, storage budget, and implementation gates.

The small state-table footprint motivates a deliberate producer profile:
retain all current state and contract code, chain metadata, the required recent
fork/recovery window, and enough ancestry data to validate and serve the
supported protocol. Validate every newly received block locally. Treat older
historical serving as a separate role, with optional archival fetch.

This is not an existing configuration switch. Determine and test the exact
history dependencies of startup/indexing, P2P ancestry lookup, fork handling,
producer operation, contract system calls, and recovery. Prune only safely
irreversible data, and define honest history-availability responses and peer
behavior. Never replace full current state with an account-specific subset.

An illustrative storage model is:

`state + required metadata/indexes + retained block/receipt window + WAL/headroom`.

The inspected block SSTs average approximately 657 bytes per physical entry;
100,000 similarly sized records would represent about 66 MB of block data.
This arithmetic explains why a **sub-GB research target** is worth testing,
but it is not a minimum requirement, forecast, or verified working node size.
Required ancestry/index structures, different workloads, and safe recovery
may raise it substantially. A bounded-history node cannot promise full
historical service or genesis replay from its own local files.

### F. Remove redundant local structure, not protocol information

The block store keeps a block ID as key and inside the record, embeds the full
block and receipt, and adds skip-list ancestor IDs. Some repeated data could
be reconstructed from a versioned local encoding or replaced by an archive's
height/offset index. The full protocol representation must still be available
at the API boundary.

Dropping skip pointers saved **6.74% in the sample before replacement-index
cost**. It is not directly deployable: current ancestor lookup requires them.
Do not infer savings by multiplying every repeated hash occurrence by 34 bytes;
compression already removes some repetition.

The SST properties contain 44,481,837 physical block entries at a canonical
head near 39.76 million. Investigate obsolete versions, retained fork blocks,
and file liveness on an offline copy. That difference alone does not prove
4.72 million removable blocks. Finalized noncanonical forks could have a
retention policy only after canonicality and lookup dependencies are established.

## 4. Low-value or misleading approaches

- Deleting the older of the two current backups: only about 77 KB of unique
  payload is attributable to it; deduplication is already effective there.
- Disabling optional services: already disabled; their remaining SSTs total
  only about 20 MB. Their deletion is not a solution to a 29 GB database.
- Clearing diagnostic logs: approximately 87 MB, useful housekeeping but minor.
- Disabling WAL or weakening synchronization: changes durability rather than
  solving the historical-data footprint.
- Wrapping existing Zstd-compressed SST files in a ZIP or another compressor:
  no large gain should be assumed without measurement.
- Removing transaction-free blocks: they still advance consensus and can change
  state, so transaction count does not determine whether a block is disposable.
- Treating remote storage as compression: it relocates the footprint and adds
  availability/latency requirements; it does not automatically reduce total bytes.
- Claiming the 50 MB state SSTs are a complete producer: WAL, metadata, history
  dependencies, forks, recovery, and operating headroom remain unaccounted for.

## 5. Recommended investigation and implementation order

1. **Local snapshot sharing prototype.** Use an isolated fixture to preserve
   checkpoint consistency, immutable SST sharing, copied mutable files, and an
   independent backup. Exercise restore, corruption, interrupted publication,
   retention, and compaction. Measure physical disk allocation across both trees
   together, counting shared inodes once. This has the largest supported
   immediate local-space hypothesis without changing historical query coverage.
2. **Lossless full-history compression experiment.** On a representative offline
   dataset, compare existing settings with larger blocks, higher cold-data
   compression, and dictionaries. Measure CPU, cache misses, read latency,
   write amplification, compaction peaks, backup growth, and restore time.
3. **Receipt and archive design spike.** Quantify field-level and chronological
   encoding opportunities; specify a versioned, lossless archive reader and
   crash-safe handoff from mutable storage. Preserve a reference reader for
   differential checks and migration rollback.
4. **Bounded-history producer proof.** Establish the smallest required ancestry
   and recovery set, then prove fresh-block validation, PoB production on a
   controlled test network, reference-node acceptance, forks, restart, restore,
   missing-archive behavior, and peer compatibility. Report the achieved size
   only after these tests pass.

The existing [audit remediation plan](../implementation/teleno-audit-remediation-plan.md)
already includes WP6 storage observability and backup boundaries. WP1 storage
integrity and relevant lifecycle/cache fixes remain prerequisites for a safe
storage migration. This assessment does not start that remediation or authorize
production retention changes.

Any later migration needs a measured peak-space budget, a recoverable original,
old/new format compatibility where required, and observer-first validation.
The production host is not the place to discover migration disk requirements.

## Source references

- [RocksDB manager and per-CF settings](../../src/storage/rocksdb_manager.cpp)
- [Block record construction and reads](../../src/block_store/block_store.cpp)
- [Ancestor skip-list structure](../../src/block_store/skip_list.hpp)
- [Checkpoint creation](../../src/backup/checkpoint_manager.cpp)
- [Content-addressed snapshot repository and physical copying](../../src/backup/snapshot_repository.cpp)
- [Indexer and receipt replay dependencies](../../src/koinos/chain/indexer.cpp)
- [Backup/restore operator contract](../backup-restore-cli.md)
- [RocksDB checkpoint semantics](https://github.com/facebook/rocksdb/wiki/Checkpoints)
- [Koinos block-record schema](https://github.com/koinos/koinos-proto/blob/master/koinos/block_store/block_store.proto)
- [Koinos block and receipt schema](https://github.com/koinos/koinos-proto/blob/master/koinos/protocol/protocol.proto)
