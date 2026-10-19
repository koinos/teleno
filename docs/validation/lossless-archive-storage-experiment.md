# Lossless archive storage experiment

16 October 2026. Status: **complete byte comparison and structural continuity pass**.
Issue #9 remains active. This is an isolated storage experiment, not an
implemented native export command, selected runtime default or qualified
40-GB restore/producer deployment.

## Input and method

The pinned public backup is
`20261015T191906Z-ms-1792091946641-files-494`. All **497 files /
29,721,611,361 bytes** were downloaded into fresh owned My Passport storage,
with exact declared lengths and SHA-256 verification. The manifest/inventory
SHA-256 values are respectively
`0bd864df4ea7a526f6096dd5d0f952e42062e8eb4a0d84742565c013f483fdd5` and
`e1b83cb557e800554d89bb0f347f4c87795120ea7a93a9c2b4ba50f83d8f82bb`.
The public pointer still selected this generation when checked at the start.
Earlier independently authenticated writer provenance is retained in the
[restore assessment](restore-40gb-current-baseline.md). Checksums and that
provenance do not establish historical execution validity or native signatures.

The source already uses Zstandard, with 64-KiB `blocks` data blocks and no
dictionary. Its persisted compression level is the library-default sentinel
32767. Enabling compression alone therefore cannot close the remaining gap.

The experimental profile keeps **64-KiB data blocks**, uses Zstandard level 9,
a 64-KiB dictionary, 1 MiB of training data and an 8-MiB dictionary-buffer
limit. It uses one compression/background worker. The complete-copy source and
destination each share a 64-MiB block cache across their column families.
These scoped cache/buffer limits exclude other allocations and are not a
whole-node memory bound.

The offline tool opens the immutable source database read-only and resolves
its complete latest-key view. It copies original key/value bytes across all
nine column families into sorted SSTs, ingests them into a fresh database and
creates an immutable checkpoint. It never reserializes block/receipt records.
Unknown protobuf fields, binary values, fork records and receipts are retained.
It does not prune history or replace a live database.

The complete database view is essential: source `chain_state` tables include
10,177 point tombstones. Independently rewriting each physical SST could
resurrect deleted state. The preliminary single-SST tool refuses deletions,
range deletions and merge operands before creating an output. The complete-DB
fixture exercises overwritten values, point/range deletion, binary/large values,
unknown protobuf fields, empty column families, close/reopen and output refusal.

Each completed output SST is checksum-verified before ingestion. Fresh read-only
reopen, full checksum checking and a complete pointwise key/value/count/SHA-256
comparison for every family now pass.
The original input remains available. Obsolete internal cell versions and
tombstones are resolved by RocksDB; this does not remove visible historical
block/receipt records.

## Linux measurements

Forty-two complete-table comparisons on three source SSTs cover 306,453 unique
block/receipt records. They are a sample of the archive, not full-corpus proof.
The actual source SST used on both Linux hosts is 67,164,375 bytes / 102,240
records, SHA-256
`826904f885b3134075dc99e813d3c94bd10c9dbe1d507c5d4d1543505db5d063`.
Every sampled rewrite preserves its complete logical record digest.

The authorized larger Linux builder passes 41 commands, including the synthetic
nine-family database fixture and sampled reads/writes. The mandatory
**2-vCPU / 4,000,034,816-byte-RAM / ext4 / no-swap VPS** separately passes 52
commands: the SST fixture, fifteen rewrites and thirty-six lookup invocations.
The VPS does **not** run the synthetic unified-DB fixture. An inherited label in
its first summary was corrected with the original summary preserved.

The following are medians of three VPS runs. Rewrite CPU includes the complete
process, including parsing, hashing and verification. Lookup CPU and p95 cover
2,048 deterministic positive lookups on the second pass, with the filesystem
warm and RocksDB data caching disabled.

| Data block / level / dictionary | File saving versus original | Rewrite CPU (s) | Lookup CPU (s) | Lookup p95 (µs) |
| --- | ---: | ---: | ---: | ---: |
| 64 KiB / 3 / none | -0.190% | 4.42 | 0.1404 | 95.34 |
| 64 KiB / 9 / 32 KiB | 16.045% | 8.25 | 0.1486 | 105.13 |
| **64 KiB / 9 / 64 KiB** | **17.066%** | **8.19** | **0.1426** | **102.50** |
| 512 KiB / 9 / none | 15.425% | 7.21 | 0.5828 | 413.87 |
| 1 MiB / 9 / none | 17.321% | 7.45 | 1.1368 | 772.69 |

The rewritten 64-KiB/level-3 control uses the same filter policy as the other
outputs; the original source has no Bloom-filter bytes. This explains the
small control-size increase. Orders alternate between repetitions. The
64-KiB/dictionary profile costs about 85% more rewrite CPU than that control,
while its warm lookup CPU changes by about 1.5%. The 1-MiB alternative saves
only another 0.255 percentage points on this table and costs about eight times
the control's warm lookup CPU.

For the selected profile, second-pass read syscall volume is 34,615,981 bytes
versus 41,845,423 for the control; it is 551,392,651 for 1-MiB blocks. These
`/proc/self/io` `rchar` values are **not physical device I/O**. Separate
first-pass measurements retain actual `read_bytes`, block-cache hits/misses,
CPU and latency. A file-local `DONTNEED` hint precedes each invocation; it does
not guarantee a physically cold device. There is no host-wide cache flush.

VPS host `MemAvailable` stays at least **3,305,189,376 bytes** during these
commands. The selected rewrite's maximum thin-launcher process RSS is
46,678,016 bytes, versus 25,022,464 for the control. These are component
processes, not `teleno_node` RSS or mixed producer/catch-up qualification.

## Complete-copy result

All nine families have been copied. The source view contains **45,076,063
visible block records**, including selected noncanonical records, rather than
only the durable chain's 40,236,770 heights. Other visible counts are:
`default` 0, `block_meta` 1, `contract_meta` 0, `transaction_index` 3,031,
`account_history` 461,934, `chain_state` 917,316, `chain_metadata` 5 and
`storage_metadata` 13. Every count is pointwise verified. All 45,076,063 block
records contain receipts; no record/block/receipt identity mismatch is observed.
The complete key-ordered block-record digest is
`c5953863ebfa9909adda21b26ace7e88bc1d3c63766259d7990a42205a54856b`.

The completed checkpoint's measured database size is **24,840,258,810 bytes**,
against **29,720,960,254 bytes** in the input: **4,880,701,444 bytes / 16.422%
smaller**. The three unchanged runtime files add 651,107 bytes. The input files'
sizes, modification times and modes remain unchanged. This is a fully compared
experimental copy, not a successful VPS restore, published backup or operator
capacity recommendation.

On My Passport, copy construction takes 1,644.33 seconds and the pointwise
comparison 1,481.50 seconds. Total checksum/reopen/copy/comparison coordination
takes 3,809.88 seconds; child CPU totals 783.75 seconds and process peak RSS is
114,966,528 bytes. Other isolated development reads/builds share the external
disk during this run. These Mac diagnostics do not qualify Linux performance
or whole-node memory.

Native `BlockStore` reads against both immutable copies match checkpoint
identity and all 68 blocks/receipts in six ranges: 1–4, 1,000–1,003,
20,118,385–20,118,388, 30,504,200–30,504,207, 32,789,370–32,789,385 and
40,236,739–40,236,770. Height, within-range parents and ID lookups pass; the
complete native response bytes match. This proves selected historical access,
not full-chain continuity or replay across the transition ranges.

The full view contains **45,075,809 receipts with empty state roots**; only 254
have nonempty roots. Thirty-six queried historical receipts have empty roots
in both copies, while recent queried receipts have completed roots. No roots
or other receipt fields are repaired
or discarded. See the separately scoped
[receipt completion correction](block-receipt-completion.md). A complete
source/copy equality check cannot turn missing historical evidence into a
historical execution pass.

## Whole-server disk model

The [15 October model](restore-40gb-current-baseline.md) required at least about
13.93% lossless payload reduction. The measured physical saving exceeds that
threshold, but the actual VPS's current development files also occupy space.
Use fresh available bytes, per-file allocation rounding, OS/runtime, metadata,
temporary files and the **unchanged** recovery reserve when planning the
complete restore. A positive size estimate is not measured whole-server peak
use or a successful stage/recovery/activation.

Applying the existing direct-mode arithmetic to the 380 candidate/runtime files
gives 24,860,360,704 allocated payload bytes, 134,217,728 bytes each for
metadata/journal and validation, and the unchanged 10,737,418,240-byte reserve:
**35,866,214,400 bytes recommended**. A fresh post-component VPS observation
leaves only **22,351,872 bytes** above that recommendation; the older 15 October
observation would leave 728,702,976. This narrow current margin is not an
operating recommendation. Account for, externally preserve and retire unused
owned development duplicates before the actual qualification where appropriate;
do not lower the reserve, ignore OS/runtime files or enlarge the reference host.
No native restore preflight/stage of this derived candidate has run.

## Complete structural history and public anchor

After successful copy validation, the external-sort audit verifies the SHA-256
header ID of **every one of the 45,076,063 records**, reproduces the original
raw block-record digest and sorts 54 bounded 64-MiB chunks. It follows the
selected head through **all 40,236,770 heights to the first network block** and
the zero SHA-256 genesis parent. Every height has the matching parent ID and a
receipt. The other **4,839,293 selected records**, including forks and records
above the durable head, remain in the database. Canonical receipts with empty
state roots number 40,236,608; this check does not execute those receipts.

The descending topology digest is
`f7fcc81a3836a7759d1402a1b10df1d03d06d1a182f85eff64ff10306ae09505`.
Its fixed 80-byte row contains little-endian height (8), ID (34), parent (34),
receipt/root-presence flags (1) and zero padding (3). The complete raw
key/value digest binds these structural results to the original archive.
Audit elapsed time is 909.97 seconds, with 208.22 seconds of child CPU and
111,640,576-byte peak RSS. Candidate file sizes/modification times are unchanged.
These are Mac component measurements, not Linux whole-node qualification.

At 09:17 UTC on 16 October, read-only calls to the configured
[public reference RPC](https://api.koinos.io/) match the mainnet chain ID and
resolve the selected block as an ancestor of its reported height 40,253,076.
The canonical successor at 40,236,771 commits to the selected state ID and
state Merkle root in its header. The reference response omits the selected
receipt's root, so the root anchor uses that successor header. This is observed
public-reference agreement, not independently established implementation
provenance, signature verification or historical execution parity.

Synthetic multi-chunk fork/future-record, missing-matching-parent,
missing-receipt and wrong-genesis-parent controls pass. An initial Mac fixture
startup timeout and successful unchanged-binary retry remain recorded; the
timeout has no established source attribution.

## Remaining qualification

At this experiment's source checkpoint, the native runtime did not expose the
measured level/dictionary settings. A subsequent
[native policy change](native-block-compression-policy.md) adds opt-in settings
for future writes/compactions, preserving existing defaults. Sustained disk
savings still require native/Linux workload qualification.
The experimental tool is not an operator migration command. Required next
work includes supported native export/recovery tests, historical/reference
execution replay, the
actual single-copy VPS restore, interruption/restart, advancing observer head
and sustained mixed producer/resource measurements. Full goal completion
remains unqualified. Source signatures remain absent.

## Reproducibility and preservation

Owned evidence is under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/disk-465t9_bg/`:
the verified original is `downloaded-source/`, the candidate was created at
`full-small-block-profile/db/`, and program sources/commands, fixtures, every
sample, process/host measurements and failed attempts remain there.
`full-copy-result.json` records successful terminal validation;
`continuity-result.json` and `continuity-audit/report.json` record successful
terminal structural validation; `public-rpc-anchor-result.json` records the
public-reference check. The 90-minute per-phase
owned-process deadline and mounted-volume /
70-GiB free-space guard do not change the target's reserve policy.

Later on 16 October, only that newly generated comparison checkpoint and its
linked `working-db/` duplicate are retired after the complete native backup
also passes raw equality. All 377 checkpoint files are hashed and match the
earlier preservation metadata; all 374 working SSTs have the same inodes as
those checked files. Original input and actual native backup remain intact.
All reports, complete file inventories, unique controls, non-block tables and
two representative block tables remain; exact retained samples total
177,220,250 bytes. This is not a complete physical archive of the prototype or
a partial DB to start. The second linked-directory retirement releases about
24.66 GB on My Passport. Receipts are under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/development-space-preservation-20261016/`.

The database prototype uses pinned RocksDB 8.8.1. Its working database's WAL
caused the initial strict read-only reopen fixture to refuse; the corrected
tool first creates a complete immutable checkpoint. Linux builds also needed
the actual transitive static dependencies and explicit JSON string extraction.
These failures are preserved; no dependency or runtime implementation was
patched to hide them.

The larger-builder checkpoint has 186 member/hash-verified files, archive SHA-256
`e8d1fc1370cff9128ace27a3431e4ee4b98821f311110c8b012d225e439beb3e`.
The VPS checkpoint has 181 verified files, archive SHA-256
`2f53c9d7084731f6709a686c5555ef2a8b2296eef25084e4466573e55f177841`.
Only then were 18/19 duplicate owned SST fixture files retired on the respective
hosts. Logs, programs, inventories, external copies and operational data remain.
No native runtime source/default, installed publisher, GUI/submodule, live
producer, repository publication or source-trust policy is changed by this
experiment.
