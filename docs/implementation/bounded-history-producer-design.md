# Bounded-history Teleno producer: feasibility and implementation design

Date: 2026-09-28. Status: **PROPOSED — source analysis and an in-memory indexing
experiment only. No pruned node has been built, migrated, or enabled.**

User clarification (2026-10-03): this research is **not** the active Teleno
resource-adaptation plan. Ordinary Teleno must retain complete history from
genesis. Bounded history belongs to the separate deferred
[`teleno_light` concept](teleno-light-concept.md); neither it nor producer
qualification is authorized by the current full-history planning task.

## 1. Decision

A Teleno producer with full current state and bounded local block history is
technically plausible without changing Koinos consensus. The inspected producer
uses current chain state, PoB contract state, a production key, and pending
transactions. The state database already commits an irreversible root and keeps
newer state branches separately. Indefinite block/receipt retention is largely
a storage, recovery, and peer-service requirement.

The recommended first version keeps **250,000 recent blocks with their receipts**,
plus every block needed above the durable irreversible root, bounded reader
pins, and a small set of verified historical checkpoint IDs. It fully executes
new blocks and preserves all current account, contract, and system state.
Receipts remain enabled in the first version to avoid combining pruning with
a second recovery redesign.

Three release gates dominate:

1. Prove that state persistence and the retained replay tail survive crashes.
2. Make ancestor lookup and new-record construction retention-aware.
3. Establish compatibility with actual legacy peer behavior, including requests
   for historical checkpoints during handshake.

Sub-GB active storage is a useful experimental target for the measured workload,
not an established node requirement or guarantee. State growth, prolonged lack
of finality, forks, and larger blocks can exceed it.

## 2. Evidence and scope

This design reviews checkout `8a13a5084b6874d9f61475ea6a20860183e4b2cb`. It uses
the September 28 production measurements from the
[database-size assessment](../performance/database-size-reduction-assessment.md):
29.48 GB active DB, 29.25 GB block SSTs, and 50.27 MB current-state SSTs.
Those measurements are the baseline; this follow-up did not alter or repeat
the production experiment. No production processes, configuration, databases,
backups, or keys were changed.

| Dependency | Inspected behavior | Consequence |
| --- | --- | --- |
| `BlockProducer::next_block`, `fill_block`, and PoB helpers | Read head/root, resources, pending transactions, effective VHP, and PoB metadata through current-state APIs | No direct archival block-store dependency found in block assembly |
| `controller_impl::apply_block` | Applies against an available parent state, checks previous-root linkage, stores the block, finalizes, and advances committed state | Preserve all state required by valid forks; pruning must follow successful durable commit |
| `state_delta::commit` | Merges state changes and metadata into a synchronous backend write batch | The durable root is the restart foundation; a reported head is not a substitute |
| State database open | Reopens the persisted root as its initial head | Blocks after that root must be replayable before normal services start |
| `indexer` and startup gate | Replay from persisted state to block-store target before P2P/RPC/producer startup | Retain a continuous recovery tail and validate the target's ancestry |
| `get_block` / `get_block_field` thunks | Read the block in the current execution context | These calls do not expose an arbitrary historical block database |
| P2P sync | Resolves local LIB ancestry and asks a peer for blocks above LIB | Retain the local anchor and select peers that can serve the missing range |
| Block-store skip list | Ancestor IDs are stored inside full historical records; insertion also resolves distant ancestors | Deleting old records alone breaks both some queries and future insertion |
| Backup checkpoint | Captures the shared RocksDB database | Backup metadata must describe retained coverage and a self-contained recovery point |

The current-state requirement includes account balances and nonces, VHP balance
history that the contract itself retains, PoB seed/difficulty/registration data,
contract bytecode, contract storage, system-call dispatch, and governance state.
Pruning block history never authorizes pruning these state objects.

Koinos system calls can be overridden by contracts. The inspected native calls
and state-based contract behavior support this design, but a release must also
test the deployed system-contract versions and supported upgrades.
[Official system-call reference](https://docs.koinos.io/architecture/system-calls/).

## 3. Validation and bootstrap trust

There are two distinct bootstrap paths:

- **Validate from genesis, prune incrementally.** Every block is executed before
  its old stored representation is retired. This can retain independent
  validation provenance while avoiding permanent archival disk usage. It does
  not make initial synchronization instant.
- **Start from an operator-trusted state snapshot.** Validate snapshot integrity
  and identity, then fully execute subsequent blocks. This inherits the trust
  assumptions of the snapshot; it is not independent genesis validation.

An important implementation detail prevents a stronger claim: in
`state_delta::calculate_merkle_root`, a child state's root is calculated from
its local changes and tombstones. Its child backend is a map of that delta;
the parent carries older state. Commit subsequently stores the previously
calculated root as metadata alongside the merged state, and reopening loads
that metadata. Consequently, matching a snapshot's reported root to a header
does **not** by itself authenticate every object in the downloaded database.

A snapshot must therefore have provenance, authenticated file digests, network
identity, a durable-root identity, and a versioned digest over the complete
exported state for source/destination equality checks. That full-state digest
is an integrity/provenance mechanism, not a new consensus commitment. A checksum
supplied by the same untrusted snapshot source is not independent authentication.
Likewise, agreement from several RPC endpoints on head/root cannot certify all
untouched state objects. Do not silently turn a snapshot-based node into a
claimed trustless bootstrap.

The safest first migration uses an operator's own already-validated state.

## 4. What is retained

| Data | Initial policy |
| --- | --- |
| Complete current state and durable root metadata | Always retain; no account-specific subset |
| Canonical block bodies and receipts in the recent window | Retain |
| Every accepted branch needed above the durable root | Retain until no longer needed for consensus/recovery |
| Durable-root block and necessary anchor metadata | Retain explicitly |
| Historical configured checkpoint IDs | Retain a bounded, verified anchor table; not every old header |
| Older block bodies and receipts | Eligible for pruning under the rules below |
| Optional transaction/account-history indexes | Disabled in this profile; public APIs disclose that limitation |
| Production key | Separate protected storage; excluded from chain backup/export |

Do not retain a full-chain hash/height index as an unnoticed dependency. At
39,757,109 heights, even one uncompressed 32-byte digest per height is about
1.27 GB before metadata. All persistent indexes in the compact profile must
either follow the history window or have an explicitly bounded anchor purpose.

## 5. Retention rule and safety invariants

Define:

- `H`: verified selected head height.
- `L`: consensus-reported last irreversible height.
- `D`: height of the state root whose objects and metadata are durably committed.
- `W`: desired history window in blocks; initially 250,000.
- `P`: lowest height pinned by an active bounded read/export, or infinity.
- `F`: current earliest retained full-block height.

The desired next floor is:

```text
desired_floor = max(1, H - W + 1)
safe_candidate = min(desired_floor, D, P)
```

Only advance `F` monotonically within the same database generation. Eligibility
for deletion is `height < F`; the durable-root block is retained because
`F <= D`. Genesis and very early heights require an explicit initialization
case with no pruning. Read actual consensus finality; never hardcode the
observed approximately 60-block head/LIB distance as a safety guarantee.

Additional invariants:

1. `D <= L <= H` and matching root ancestry are established before floor advance.
   A stored height without the matching ID/state identity is insufficient.
2. Every block needed from `D + 1` to the selected recovery tip is present,
   parent-linked, and replayable. Relevant reversible branches remain available.
3. A durability or block-store write failure stops pruning and production
   readiness; it cannot become a successful empty storage response.
4. Readers that need stable coverage pin their range before work starts. Pins
   have deadlines and cancellation; a slow client cannot retain history forever.
5. Restore creates a distinct generation. A lower restored `D` is never accepted
   underneath a previously advanced floor in the old generation.
6. Increasing `W` does not recreate deleted data. Advertise actual coverage until
   a separately validated backfill or restore expands it.

The retained total is at least the desired window **and** the complete necessary
tail above `D`. If finality stops advancing, that tail grows. A hard byte ceiling
cannot override consensus or recovery requirements: apply backpressure and a
controlled readiness/shutdown policy before storage exhaustion. Bounded history
does not mean the complete node has an unconditional fixed-size bound.

## 6. Storage implementation

### Minimal first format

Keep the existing hash-keyed block records for the retained range. Add:

- A height index keyed by `(big-endian height, block ID)` for enumerating records
  eligible for pruning, including fork records.
- Retention metadata: format version, generation, logical floor, GC cursor,
  durable-root identity, verified recovery target/frontier, and coverage.
- A small checkpoint-anchor table with provenance and canonical ancestry scope.

Update a new block record and its height index atomically. Recovery target
metadata must correspond to successfully finalized chain selection, not merely
the largest block-store height observed. The existing separate record/topology
writes and failure cases in WP1 must be corrected before this feature ships.

This first version deliberately keeps receipts and the recent skip list. A
later height-ordered segment format can improve reclamation, but is not needed
to prove the compact producer.

### Retention-aware ancestor pointers

When constructing a new record, generate the normal power-of-two ancestor
targets, but stop before a target below the retained floor. Preserve the
original pointer ordering; remove only the unavailable suffix. Existing old
pointers may become dangling after floor advance, but queries whose target is
at or above the floor must never follow such pointers. Below-floor queries go
to explicit coverage/anchor handling before traversal begins.

This requires special handling for genesis, floor equality, direct-parent
links, and checkpoint-only queries. Returning a stored checkpoint ID is allowed
only for a head whose connection to the checkpoint's canonical ancestry has
already been established. Never substitute a canonical ID for an unrelated fork.

An in-memory translation of the source skip-list was exercised with a rolling
128-record window. Unmodified insertion first failed at height 512 with floor
384 because it tried to traverse missing height 256. Clipping insertion targets
at the floor inserted all 4,096 modeled blocks and passed **16,378 ancestor
lookup checks**. This validates the indexing idea in a linear model only; it
does not test C++, forks, persistence, concurrency, or protocol compatibility.

### Crash-safe logical pruning and physical reclamation

1. Capture and validate the durable-root identity and all relevant pins under
   coordinated state/storage locks.
2. Durably publish the new logical coverage floor and a GC generation/cursor.
   Requests below that floor now return an explicit coverage result.
3. Delete eligible block/index entries in checked, bounded atomic batches,
   persisting progress. Do not delete by SST filename, age, or guessed contents.
4. Reclaim physical space through measured compaction/reclamation policies.
   Record logical bytes, obsolete physical bytes, WAL, snapshots, and free space
   separately. Do not assume a Delete immediately shrinks the filesystem.

A crash after step 2 but before reclamation should leave extra physical data,
not a promise to serve data already gone. Readers use a consistent DB snapshot
and coverage generation. Batch deletion must be idempotent across restart.
Reject reinsertion of retired historical data through ordinary storage paths;
an explicit validated backfill is a different operation.

The production SSTs mix many heights because keys are hashes. Initial migration
should therefore build a fresh compact DB from a consistent source checkpoint,
rather than deleting old SST files or expecting tiny in-place compaction peaks.

## 7. Restart, forks, and backup

Reopening state starts from `D`, not necessarily the head that existed before
shutdown. The existing startup gate must remain: recover first, then start
P2P, external submission APIs, and the producer.

Before replay, verify:

- State objects and metadata belong to one valid durable checkpoint.
- Root block ID, header, revision, and recorded root metadata agree.
- The retained tail begins at `D + 1` and connects to `D`.
- Recovery targets are descendants of `D`; ties and stored branches are resolved
  through the normal fork-choice rules.
- Pruned, absent, and corrupt records are distinguishable; a hole is an error,
  never a successful short replay.

Persist enough validated frontier information to reconstruct required accepted
branches after restart. The current highest-height-only block-store target
must not become proof of canonicality. If some safe-to-reacquire branches are
intentionally omitted from a compact export, document that and start from the
exported durable root as an observer before returning to production.

For the first offline migration, an especially simple export target is the
durable root itself: copy complete state at `D`, metadata, the recent canonical
window ending at `D`, and verified checkpoint anchors into a new DB. Set its
recovery tip to `D`; catch up by fully validating peer blocks after startup.
Do not label the export as containing the source's newer in-memory head.

Each compact backup must be self-contained for its declared recovery point:
state, metadata, coverage/floor, anchor table, and any required replay tail must
come from one consistent checkpoint. An older compact snapshot does not require
the live node to retain its entire intervening history forever. However, its
eventual recovery depends on access to a peer/archive that can serve the gap.
Keep that dependency explicit in the backup policy and test it.

Snapshot restore is observer-first. Missing/corrupt state does not authorize
resetting to genesis, inventing an anchor, trusting an arbitrary replacement
snapshot, or enabling production. Full genesis replay requires external block
availability once local history has been pruned.

## 8. Peer and RPC compatibility

This is the main interoperability obstacle, separate from consensus validity.
The reference `koinos-p2p` v1.3.0 handshake requests configured historical
checkpoint ancestors. Its sync flow also asks for the ancestor at the requesting
node's LIB and then requests subsequent block bodies. The inspected protocol
has no earliest-history-height field in those requests/responses.

Consequences:

- Keeping the local LIB is enough for some outgoing sync operations, but does
  not make arbitrary incoming historical requests satisfiable.
- A bounded checkpoint table can answer known old checkpoint-ID requests when
  the requested head has verified ancestry. It cannot serve arbitrary old bodies.
- A node far behind the retention floor needs a different history provider.
- Outbound-only dialing does not solve this: established peer connections can
  carry requests in both directions.
- Empty `GetBlocks` responses are not a safe universal compatibility strategy.
  The inspected legacy sync code indexes the last returned block without an
  empty-length guard on that path. Below-coverage requests need tested errors,
  not invented payloads or empty-success responses.

For a controlled first prototype, use an explicitly qualified peer set with
archival availability, known checkpoint requirements, and observed reciprocal
request behavior. This is a deployment constraint, not proof of compatibility
with every existing public peer configuration.

For broad deployment, specify a backward-compatible optional history-capability
exchange advertising body/receipt coverage, anchor capability, and retention
mode. New peers can avoid unsuitable providers and treat unavailable history
as an availability condition. Legacy peers still need explicit qualification
or a carefully bounded archival service path. A P2P extension is not a consensus
fork, but it requires coordinated implementation and deployment.

The first version should not synchronously fetch arbitrary old history in its
block-validation or producer path. Optional archive access needs timeouts,
resource limits, independently checked IDs/ancestry, and explicit failure
behavior. It must never become an unverified consensus input or an unbounded
request-amplification service.

Expose local coverage through an additive diagnostic API, for example:

```text
mode: bounded
earliest_full_block: F
earliest_receipt: F
durable_root: {height, id}
selected_head: {height, id}
retained_window_target: W
history_format_version: 2
```

These fields and API names are proposals, not existing functionality. For an
old ID that is no longer indexed, do not claim to know its former height or
canonicality: report that it is unavailable locally and disclose coverage.
Distinguish this from a known height explicitly outside retention and from a
missing record inside a promised retained range.

## 9. Storage budget

The observed archive averages approximately 657.47 compressed bytes per physical
block entry. This includes the historical workload and is not a measurement of
a recent compact window. Using it only as a sizing hypothesis:

| Retained blocks | Estimated block/receipt payload | Payload plus measured state SSTs |
| ---: | ---: | ---: |
| 100,000 | 65.75 MB | 116.02 MB |
| 250,000 | 164.37 MB | 214.64 MB |
| 1,000,000 | 657.47 MB | 707.74 MB |

Add bounded indexes, WAL, logs, retained forks, compaction slack, snapshots,
restore staging, and operational reserve. RAM/cache settings are a separate
budget. Two independent compact backups still consume additional space even
when each is much smaller than the current archival backup.

A useful **experimental acceptance target**, on a representative current-workload
fixture, is less than 1 GB for the active 250,000-block profile after settled
compaction. Backup and migration peak footprints must be measured separately.
At a future average of 4 KiB per retained block, that same window alone requires
1.024 GB before state or overhead. No fixed total-storage guarantee follows.

Specify retention in blocks first. Calendar coverage depends on actual block
cadence; do not promise a number of offline days from a block count alone.
An offline node can recover after longer than its own window if an appropriate
peer still serves the missing interval. If every reachable provider has pruned
past its checkpoint, recovery requires other history or an explicitly trusted
newer snapshot.

## 10. Implementation work packages

| Package | Concrete change | Acceptance evidence |
| --- | --- | --- |
| B0: persistence prerequisites | Complete relevant WP1/WP2/WP3/WP6 prerequisites; expose durable-root identity and write errors | Failure injection establishes coherent root, records, topology, and reopen behavior |
| B1: coverage model and dry-run planner | Versioned retention metadata, height index, pins, target/root checks; no deletion | Exact candidate inventory; no reversible/replay-required data eligible |
| B2: compact export and startup | Build a new compact DB at a verified durable root; validate state equality and tail coverage | Independent reopen and reference replay succeed; original source preserved |
| B3: ancestor and API semantics | Clip skip-list construction, validate floor/anchors, expose coverage and typed failures | Boundary, power-of-two, fork, unknown-ID, and range tests pass |
| B4: durable pruning and GC | Advance logical floor, delete in bounded batches, compact under resource budgets | Crash at every phase preserves recovery; disk accounting matches physical reclamation |
| B5: peers and checkpoints | Qualified legacy peers, retention-aware peers, archival recovery policy | No malformed/empty success, avoidable disconnect loops, or fabricated ancestry |
| B6: producer qualification | Full verification and controlled-network PoB operation; backup/restore cycles | Independent reference accepts produced blocks through canonical inclusion and irreversibility |

Likely implementation surfaces are `IBlockStore`, `BlockStore`, the RocksDB
manager and metadata, controller/indexer startup, P2P request handling, native
backup manifests/preflight, and producer readiness. Defaults remain archival
until the compact mode passes its gates. Existing databases and backups must
not be silently interpreted under a new coverage contract.

## 11. Required adversarial tests

- Crashes before/after durable-root commit, logical-floor publication, every GC
  batch, backup publication, export completion, and restore activation.
- I/O failures that previously looked like missing keys or successful block
  storage; recovery-target metadata ahead of valid records.
- Forks above `D`, tied heads, a candidate that diverges before `D`, and a peer
  advertising an old checkpoint on an unrelated branch.
- Ancestor lookups at `F - 1`, `F`, `D`, and `H`; insertion at powers of two
  after several rolling retention cycles.
- Restart with missing first replay block, incomplete last response, stale
  highest-block metadata, missing receipt, and a tail from another branch.
- Long-running readers, cancellation, concurrent backup and GC, and disk-full
  conditions while finality is stalled.
- Legacy peers with old checkpoints, peers far behind the window, unknown
  checkpoint heights, and peers returning unavailable or incorrect history.
- Restore from an older compact backup after local history has advanced;
  successful archive recovery and explicit failure when the gap is unavailable.
- Exported state with an altered untouched object: the test must reject the
  provenance/integrity mismatch even if stored root metadata was copied intact.
- Identical next-block validity, receipts, state mutations, and fork choice
  against an archival/reference node on the same representative input stream.

The checks above have **not** been run. The only new experiment in this analysis
is the in-memory skip-list model described in section 6.

## 12. Rollout recommendation

Start with an offline compact export and an observer that follows the same
stream as an archival reference. Measure actual disk, restart time, and peer
behavior before testing production on a controlled network. Only then prepare
a separately authorized mainnet observer canary and producer activation plan.

Preserve the original database/backup for rollback. A pruned DB cannot recreate
an archive merely by changing its mode flag. Turning compact mode off requires
validated backfill or an archival restore if full historical service is desired.

No new consensus, custodial wallet, external transaction signer, or mobile UI
is required for this storage feature. Mobile power/lifecycle and store policy
remain separate feasibility gates.

## Source references

- [Producer implementation](../../src/block_production/block_producer.cpp)
- [Controller application and durable-root advancement](../../src/koinos/chain/controller.cpp)
- [State-delta commit and root calculation](../../src/koinos/state_db/state_delta.cpp)
- [State database root/fork management](../../src/koinos/state_db/state_db.cpp)
- [RocksDB state backend and synchronous batch](../../src/koinos/state_db/backends/rocksdb/rocksdb_backend.cpp)
- [Indexer recovery](../../src/koinos/chain/indexer.cpp)
- [Startup recovery gate](../../src/main.cpp)
- [Block store and ancestor lookup](../../src/block_store/block_store.cpp)
- [Skip-list algorithm](../../src/block_store/skip_list.hpp)
- [Teleno peer request and handshake paths](../../src/p2p/p2p_node.cpp)
- [Legacy peer handshake and synchronization, v1.3.0](https://github.com/koinos/koinos-p2p/blob/v1.3.0/internal/p2p/peer_connection.go)
- [Legacy peer RPC protocol, v1.3.0](https://github.com/koinos/koinos-p2p/blob/v1.3.0/internal/rpc/peer_rpc_service.go)
- [Existing remediation prerequisites](teleno-audit-remediation-plan.md)
