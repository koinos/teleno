# Teleno Quality and Resource-Efficiency Remediation Plan

Date: 2026-09-05. Status: **PROPOSED — production implementation not started**.

Baseline: `main`, `9936a78d482716be1f2ca4bae81a3d786981bbe0`.
Evidence and reproduction details:
[Teleno software review](../performance/teleno-software-audit.md).

## Objective and authority

Fix reproducible functional bugs, improve memory lifetimes and boundedness,
and reduce unnecessary disk consumption/I/O without changing Koinos behavior,
durability, or observer-first recovery. CPU measurements are supporting evidence
for memory/storage tradeoffs, not an independent throughput target.

This document is an implementation proposal. The review authorized inspection,
local tests, and documentation only. It did not authorize these production
changes, live-node work, external runtime access, publication, or data cleanup.

Before implementation, re-read `AGENTS.md`, inspect the current worktree, and
preserve unrelated changes. The review's modified README and untracked
high-throughput plan belong to the user. Work only in Teleno; reference
implementations are behavioral evidence, not infrastructure to import.

## Why these first three packages

| Order | Package | Evidence-based reason | Measurable target |
| --- | --- | --- | --- |
| 1 | WP1: storage errors and atomic persistence | Actual I/O failures become absence/success; partial block persistence survives retry and reopen | Every injected failure is visible; retry/reopen yield coherent records and topology |
| 2 | WP2: owned RPC session lifecycle | A session still responds after stop and an idle connection occupies the only slot | Zero active sessions after stop; bounded shutdown and idle expiry; capacity recovers |
| 3 | WP3: cache correctness and memory work | Clear/oversize operations hang, duplicate modules evict incorrectly, warm hits allocate twice, payload accounting understates RSS | Terminating invariant-preserving operations; unique LRU entries; zero allocations on the measured warm-hit fixture |

These packages address established correctness failures before speculative
architecture changes. WP4 follows immediately: index behavior is important,
but canonical/fork semantics and persistent redelivery policy require a clearer
contract before implementation. No percentage reduction in RAM or disk is
promised in advance.

## Shared implementation and verification rules

For every package:

1. Record the exact starting commit, dirty changes, relevant dependency versions,
   configuration, workload, and previous evidence. Do not silently replace
   user changes or depend on a foreign CMake cache.
2. Minimize and promote the applicable ignored diagnostic reproduction into a
   proper repository regression test. Confirm that it fails for the stated
   reason on the baseline before fixing code. Diagnostic runner exit zero is
   not itself a passing behavior assertion.
3. Implement one reviewable behavior change at a time. Do not combine error
   propagation with format migrations or asynchronous execution redesigns.
4. Run focused tests, broader CTest when shared behavior is affected, binary
   identity/help, and supported diagnostic builds. Record unsupported checks.
5. For resource claims, use identical logical datasets, durability, cache
   conditions, build settings, and instrumented/uninstrumented configuration.
   Use warm-up plus at least five paired repetitions, medians and dispersion;
   report results within noise as inconclusive.
6. Keep a package log: files changed, invariants, actual test/measurement results,
   artifact paths, remaining uncertainty, and exit-criterion status.
7. Update affected English operator/configuration documentation and CHANGELOG
   only when implemented behavior changes. Do not change VERSION or publish a
   release merely because a package passes.

The mandatory compatibility invariants are:

- Identical valid transaction handling, ordering, receipts, events, resource
  accounting, state objects, roots, fork choice, head and LIB behavior.
- Ordinary `remove_object()` semantics remain unchanged. Tombstone-preserving
  removal remains specifically for historical serialized replay.
- Pending roots remain genuinely fresh. The historical scar remains matched by
  its exact documented block/root constants, with no generic mismatch bypass.
- One-block lookahead and final-target behavior remain correct; at most one
  full reexecution occurs; failed validation has no canonical advancement or
  partial external/index side effects.
- Persistence guarantees, crash recovery, and coherent indexes are not traded
  for smaller files or faster local benchmarks.
- Recovery/restore remains observer-first. No producer activation, transaction
  signing/submission, existing chain reset, or live deployment is part of this
  plan's repository implementation.

## WP1 — Propagate storage errors and make block persistence atomic

Findings: TQ-01, TQ-02; investigate TQ-12 without assuming it is confirmed.

Likely components/files:

- `src/koinos/state_db/backends/rocksdb/rocksdb_backend.cpp/.hpp`
- `src/koinos/state_db/state_delta.cpp` and affected controller error paths
- `src/block_store/block_store.cpp/.hpp`
- `tests/storage/`, new block-store regression tests, `src/CMakeLists.txt`
- Recovery/operator documentation where externally visible errors change

Approach:

1. Add deterministic failing-Get, failing-Flush, and failing-Write adapters around
   isolated RocksDB instances. Keep synthetic fault support out of production
   configuration. Fail each write position, not just the successful path.
2. Distinguish NotFound from I/O/corruption errors. Check all required flush
   results. Define error behavior for callers, `close()`, destructors, shutdown,
   and recovery: introducing exceptions must not create termination hazards.
3. Write the block record and required topology metadata in one checked cross-CF
   batch, with the existing required durability. Preserve the highest-block and
   fork-selection semantics; highest archive topology is not automatically the
   canonical chain head.
4. Define a diagnosis/repair policy for already partial records. Do not silently
   infer missing metadata from one record, clear the database, or add an
   unbounded repair loop. A repair that changes existing archives is separately
   reviewable and must preserve original data.
5. Inject failure into state commit and inspect post-failure object ownership.
   If TQ-12 is confirmed, stage ownership changes until persistence succeeds or
   enter an explicit unusable/fatal state; do not retry a half-mutated context.

Preconditions/dependencies: agree on absence versus storage-error contracts,
shutdown behavior, and the atomic record/topology unit. No prerequisite format
migration is assumed.

Acceptance:

- Missing keys still return absence; injected non-NotFound reads never do.
- Every required failed flush/write reaches a defined error boundary.
- A failed block write leaves coherent pre-state or complete post-state, never
  a successful retry with stored height 1 but highest topology 0.
- Retry, clean reopen, and isolated interrupted-write tests converge without
  deleting data or advancing canonical state after failed validation.
- No newly throwing destructor or unsafe rollback; controller, lookahead,
  historical-fixture and durability tests pass.
- Changes to successful commit semantics require the differential gate below.

Rollback: keep layout compatible where possible; restore the preceding code in
an isolated verification checkout if tests regress. Preserve every produced
scratch database and its diagnostics. Do not roll back by resetting user state.

Risk/uncertainty: medium; expanding error visibility can expose assumptions in
callers. Physical power-loss behavior is not proven by an injected Status alone.

## WP2 — Make RPC sessions owned, cancellable, and bounded

Finding: TQ-03.

Likely files: `src/jsonrpc/jsonrpc_server.cpp/.hpp`, appropriate config fields
if needed, new `tests/jsonrpc/` lifecycle tests, `src/CMakeLists.txt`, RPC and
configuration documentation.

Approach:

- Track accepted sessions and their socket/thread lifetimes explicitly.
- Stop acceptance, cancel/close owned sessions, and join/drain work before
  dependent services or the server object are released.
- Add an explicit, bounded idle/read/write policy. Preserve legitimate keepalive
  use; separate execution workers from connection/work capacity if justified.
- Keep response/request limits and overload results intentional. Do not solve
  the reproduced saturation merely by raising defaults.
- Reproduce lifecycle behavior using the real server library and full-node smoke
  harness. Resolve the standalone probe's EOF SIGBUS separately; do not call it
  a proven production lifetime bug without attribution.

Preconditions: documented cancellation semantics for in-flight read-only and
mutation requests, ownership order with service registry, and client-visible
deadline behavior. Only self-created loopback fixtures are required.

Acceptance:

- A deterministic test observes zero live sessions after `stop()` returns.
- Idle sockets expire and cannot permanently retain every available slot.
- Stop/destruction completes within a fixture-defined deadline with active,
  idle, partially read, and disconnected ordinary clients.
- Repeated start/stop and well-formed keepalive requests work as documented.
- Supported ASan/TSan/lifetime checks pass; unsupported environments remain
  explicitly pending. Memory/thread/socket counts return to their documented
  steady-state bounds.
- RPC shape, service availability, and access boundaries remain compatible.

Rollback: revert the isolated implementation in a test checkout; keep the old
configuration parsable or provide documented compatibility. No live service
restart or producer changes are authorized.

Risk: medium. An asynchronous internal implementation is optional, not a
mandated rewrite; explicit ownership and bounded behavior are the requirements.

## WP3 — Correct cache invariants and reduce allocation overhead

Findings: TQ-04, TQ-05, TQ-08; TQ-12 lifetime contract is a prerequisite for
changes to uncached value ownership.

Likely files:

- `src/koinos/state_db/backends/rocksdb/object_cache.cpp/.hpp`
- `src/koinos/state_db/backends/rocksdb/rocksdb_backend.cpp/.hpp` if needed
- `src/koinos/vm_manager/fizzy/module_cache.cpp/.hpp`
- Cache tests and bounded allocation/RSS benchmark fixtures

Approach:

1. Correct clear/reset accounting, overflow checks, empty-list handling, and
   zero/oversized-capacity behavior.
2. Establish ownership for values not retained in the cache. **Do not introduce
   an oversize bypass that destroys the last value owner before a backend caller
   consumes its raw pointer.** Keep protocol-valid large values usable.
3. Enforce one LRU entry per key/module ID, including replacement and concurrent
   same-ID cache misses. Avoid duplicate parsing only if measurement supports it;
   never reuse mutable execution instances without separate proof.
4. Replace hit-path erase/push with stable list movement and reuse the located
   map iterator. Validate iterator safety before reducing duplicated keys.
5. Report configured payload budget separately from estimated entry overhead,
   entry count, and actual resident memory. Define a tested effective capacity
   policy; do not claim allocator RSS is a strictly enforceable cache budget.

Acceptance:

- Clear/reuse, zero capacity, oversized entry, replacement, eviction, and
  capacity-arithmetic tests terminate and preserve cache invariants.
- Insert A, A, B into capacity two retains both A and B with correct recency.
- Concurrent lookup/replacement never exposes freed values or invalid iterators.
- Warm hits for the existing heap-backed-key fixture require zero list/key
  allocations after setup, compared with the measured two per hit.
- Before/after five-run memory measurements report both logical data and RSS.
  A memory reduction is claimed only if observed; retained allocator pages are
  distinguished from live-object growth.
- Cache miss rate, underlying read/parse work and latency do not regress beyond
  measured noise without an explicit accepted tradeoff.
- State values, roots, replay exceptions, receipts and VM resource accounting
  remain unchanged.

Rollback: retain compatible APIs and no persistent layout change; restore the
previous code only in test runs if needed. In-memory caches may be recreated by
ordinary process lifecycle, not by deleting persistent state.

Risk: low/medium for LRU operations; higher for ownership changes. Byte-budget
defaults should not be reduced before hit-rate and workload evidence exists.

## WP4 — Make optional indexing consistent and recoverable

Finding: TQ-06; prepares for any later allocation/batching optimization.

Likely files: `src/account_history/`, `src/transaction_store/`,
`src/contract_meta_store/`, EventBus wiring in `src/main.cpp`, new index tests,
optional-service coverage and recovery documentation.

Approach:

- Establish whether each API exposes canonical state, all accepted forks, or a
  documented combination. Obtain offline reference fixtures for ABI changes,
  reverted transactions, forks/reorgs and repeated delivery before selecting
  canonical-only behavior.
- Check read/write/parse failures; expose unhealthy/incomplete index status.
- Batch sequence and record updates; establish idempotent delivery and durable
  progress/checkpoint semantics.
- Test resumption/rebuild from a verified block source without deleting the
  source or hiding incomplete results.
- Keep synchronous ordering initially. Moving indexing to a worker queue is a
  different architectural proposal requiring read-after-write and lag contracts.

Acceptance: failure at every write position produces no partial sequence/record
state; redelivery follows the specified idempotency policy; retry/restart
converge; ABI fork/reverted behavior matches agreed fixtures; rejected controller
attempts emit no partial or duplicate index side effects. Quantify index bytes
per logical transaction/address for the same workload before claiming savings.

Dependencies: WP1 error/atomicity policy and documented optional-service API
contracts. Layout changes require a separate migration and rollback design.

Rollback: preserve old-format compatibility or retain a verified pre-migration
copy with an explicit operator procedure. Never discard an index without
assessing API availability, rebuild time and disk-space requirements.

Risk: medium/high. Handler-level observations do not prove current rejected
replay leaks events; keep that distinction in the tests and release notes.

## WP5 — Bound retained IDs and instrument memory growth

Finding: TQ-07; expands the memory coverage gaps for P2P, mempool and VM caches.

Likely files: `src/p2p/p2p_node.cpp/.hpp`, metrics in `src/main.cpp`, relevant
mempool/VM metrics, fake-transport and cache tests.

Approach:

- Add read-only counters for retained IDs, queue depth, cache entries/estimated
  bytes, eviction, outstanding work and active sessions.
- Use a non-retaining fake chain/transport so the driver does not itself retain
  all test transactions and invalidate memory attribution.
- Define block deduplication lifetime relative to fork/LIB behavior and
  transaction deduplication lifetime relative to validity/expiration. Use a
  bounded policy only after documenting what re-admission/eviction may do.
- Run bounded steady and burst workloads, including expiry, duplicate IDs,
  reorgs, peer replacement and clean shutdown.

Acceptance: entry cardinality and live memory plateau at documented bounds;
resource reclamation occurs after expiration; valid sync/gossip behavior is
unchanged; duplicate work is bounded. Report duration, offered work, accepted
work, evictions, RSS and allocation trends; distinguish useful cache retention
from leaks. No real P2P network or producer is required.

Dependencies: baseline observability and deduplication semantics. Rollback:
revert the policy/configuration in isolated tests while retaining metrics and
evidence; do not alter protocol validity rules to reduce memory.

Risk: medium; excessive eviction can shift memory savings into CPU/network work.
The currently unmeasured node growth rate must not be invented.

## WP6 — Explain disk growth and harden local backup/recovery boundaries

Findings: TQ-10, TQ-11; builds on WP1 and WP4.

Likely files: `src/storage/rocksdb_manager.cpp/.hpp`, storage reporting,
`src/backup/public_restore.cpp`, `snapshot_repository.cpp`, checkpoint/service
tests, storage/backup operator documentation.

Approach:

1. Add available/unavailable-aware statistics for WAL, live/obsolete SSTs,
   memtables, cache usage, flush/compaction/stall counters and background errors.
   Never silently report an unsupported metric as zero.
2. Repeat the measured cross-CF WAL-lifetime case and identify exactly which
   outstanding CF writes pin the WAL. Include a dataset large enough for
   multiple flushes/levels before tuning compression or compaction.
3. Measure the same logical state at comparable phases: live pre-flush, durable
   flush, settled compaction, checkpoint, backup, staged restore and reopen.
   Record apparent/allocated bytes, continuously sampled peak space and all
   temporary/preserved copies. Track driver and fixture storage separately.
4. Reproduce stalled/incomplete local transfers with finite synthetic fixtures;
   test cancellation, expected-size budgets and cleanup ownership. No external
   server testing is necessary or authorized.
5. Define and test durable publication of snapshot inventory/completion/latest
   markers. Inject stream/write failures; add isolated crash tests on supported
   filesystems. Preserve existing target data and observer-first activation.
6. Tune only after identifying a measured avoidable cost. Account for cache
   misses, compression CPU, write amplification, recovery time and storage
   retention semantics. Do not force global flush/compaction as a blanket fix.

Acceptance:

- Reports reconcile logical data, SST/WAL, logs/backups/temp bytes and allocated
  space; metric availability is explicit.
- Proposed savings compare identical data, durability and compaction phases.
- Transfer and staging work obey documented size/time bounds and preserve
  unrelated files; all writes required for success are checked.
- A failed/interrupted publication leaves a valid old or new recoverable state,
  with no false completed snapshot or unintended producer activation.
- Existing backup/storage suites pass, with new failure/peak-space coverage.

Dependencies/blockers: observable physical I/O and a representative repository
fixture; explicit crash-durability contract and supported isolated crash harness.
Do not change production fsync, WAL policy, retention or format to evade them.

Rollback: prefer read-only observability first; keep old backups/checkpoints and
format readers usable. Any migration gets a distinct rollback procedure and
separately approved disk budget. No existing database or backup cleanup occurs
as part of rollout preparation.

Risk: medium/high. The current 63.38 MiB sampled live versus 8.87 MiB reopened
example is a sizing lesson, not an established percentage-saving target.

## WP7 — Simplify measured representation and lifecycle costs

Finding: TQ-09; follow-on memory/storage simplifications from earlier packages.

Likely files: `src/core/monolith_rpc_client.hpp`, controller call sites,
EventBus wiring, `src/block_store/block_store.cpp`, affected tests/benchmarks.

Approach: obtain a profile/allocation trace for a representative offline block
and transaction workload. Attribute time and bytes to actual conversions,
promises, copies and duplicated calls. Then replace only proven unnecessary
boundaries with typed/local interfaces or eliminate redundant persistence
notifications while retaining the external protocol/API boundary.

Acceptance: identical observable call/error/event order and data; fewer measured
allocations or retained representations; no unexplained memory, latency or disk
regression. The driver must not be the bottleneck. Cache-root recomputation
cannot be removed merely because it looks repeated: freshness is mandatory.

Dependencies: WP1/WP4 contracts, representative input corpus and attributable
profile. Without these, complete the measurement package but do not implement
typed execution/concurrency/indexing architecture changes.

Rollback: keep an equivalent tested adapter path during review; remove obsolete
paths only once callers and error behavior are covered. Do not reintroduce
GarageMQ or legacy microservice packaging.

Risk: medium. No broad rewrite, protocol-limit increase, parallel transaction
execution, or claimed theoretical speedup is justified by this review.

## Compatibility and final verification gates

Repository build/test gate after shared-runtime changes:

```bash
KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/teleno_node --version
./build/teleno_node --help
/usr/bin/python3 -m unittest -v tests/scripts/benchmark_transaction_tps_test.py
```

Use a correctly configured local build. Dependency acquisition, if unavailable
within the permitted environment, is a stated prerequisite rather than authority
to access an external runtime or modify system protections.

| Gate | Required proof | Current review status |
| --- | --- | --- |
| Baseline | Commands, source identity, normal/focused tests, CLI | Proven for reviewed checkout; not for future fixes |
| Regression | Baseline-failing reproductions become passing tests for the actual bug | Diagnostic reproductions exist; production fixes/tests not implemented |
| Memory correctness | Supported lifetime/race tooling plus interleaving tests | UBSan excluding vptr passes; ASan/TSan environment unavailable |
| Resource improvement | Same inputs, phases, guarantees and tooling; at least five samples; measured benefit outside noise | Current baseline measured; improvements unimplemented |
| Persistence/recovery | Injected failures, retry, reopen and supported crash checks | Selected failures reproduced; full fault/crash matrix remains open |
| Historical/differential | Checked-fast, full execution and exact official reference agree across relevant real checkpoints | Not executed in this review; local archive/checkpoint prerequisite missing |
| Release/operations | Exact source/artifact identity, operator docs, native/container tests, authorized rollout plan | Not authorized; no release or live operation attempted |

Before consensus-adjacent changes, prepare a verified repository-local immutable
archive starting before 30,504,202 and extending beyond 32,789,378, matching
pre-range checkpoints, and fresh independent scratch copies for each mode.
Identify the source block IDs and fixture hashes. Use the existing
`tools/historical_indexing_validator.cpp`, `tools/state_delta_replay_audit.cpp`,
and `tools/reference/chain-v1.5.2/reference_validator.cpp` according to their
checked CLI help and source-read-only contracts.

Compare chain ID, head/LIB, block IDs, roots, receipts/resource accounting,
selected contract reads, fallback counts/heights, side effects, stop/reopen,
scar neighbors and final-target continuation using local fixtures. Reference
versions must be pinned to the verified Chain v1.5.2/state-db v1.2.1 revisions.
Do not substitute the smaller receipt journal for full-execution input or a
fixture test for full-range parity. No live catch-up is permitted in this review.

Do not proceed with a database-format, commit-order, execution-concurrency or
asynchronous-indexing change until its corresponding proof is available. A
missing prerequisite is an explicit gate, not a reason to weaken validation.

## Definition of Done for future remediation

- Each implemented finding has a baseline reproduction, regression test,
  reviewed fix, documented behavior and actual verification evidence.
- All unaffected protocol and recovery invariants hold; required differential
  checks are executed rather than inferred from existing green tests.
- Memory/disk improvements are measured on equivalent states and workloads;
  allocator retention, cache tradeoffs and I/O costs are disclosed.
- No silent storage error, unbounded retry, unexplained mismatch, unsafe
  post-stop work, or partial index state is accepted as an optimization.
- Package logs list changed files, commands, results, artifacts, blockers and
  exit status. Blocked architectural packages remain explicitly pending.
- Changes are reviewable without credentials or private operational inventory.
- Publication, deployment, data migration/cleanup and producer activation remain
  separately authorized operations. Repository test success is not release
  authorization or evidence of live-node health.

These remediation criteria are **not satisfied by writing this plan**. The
review deliverables can be complete while the proposed remediation remains
unimplemented and the listed verification prerequisites remain open.

## Planning progress log

| Work | Files changed during review | Result | Exit status |
| --- | --- | --- | --- |
| Evidence collection and ranking | Ignored probes/logs; companion review report | Reproductions and qualified measurements linked to TQ-01–TQ-12 | Ready for review |
| First-three-package selection | This document | Integrity, RPC ownership, cache correctness/resource targets | Ready for review |
| Remaining work and gates | This document | Index contracts, memory bounds, disk/backup tests, measured simplification and compatibility prerequisites | Proposed; execution not started |
| Production implementation | None | No fixes, no format changes, no deployment | Not authorized by the review |
