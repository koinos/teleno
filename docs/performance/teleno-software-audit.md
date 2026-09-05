# Teleno Quality, Stability, and Resource-Efficiency Review

Date: 2026-09-05. Source: `main` at
`9936a78d482716be1f2ca4bae81a3d786981bbe0`.

## Scope and outcome

This is a functional correctness, memory, and disk-efficiency review, not an
intrusion assessment. It contains **no production fixes and no achieved
optimization claims**. The accompanying
[remediation plan](../implementation/teleno-audit-remediation-plan.md) is proposed
work, not authorization to execute it.

The most actionable results are:

- State reads convert injected I/O errors into missing objects, and state flushes
  discard injected failures.
- A failed block metadata write leaves a stored block that an apparently
  successful retry does not repair. The inconsistency survives reopen.
- Cache clearing and oversized insertions can hang; module-cache replacement
  can evict a still-needed entry prematurely.
- JSON-RPC sessions remain usable after `stop()` returns, and one idle persistent
  connection occupies the sole slot in a one-slot functional test.
- Optional index handlers exhibit duplicate history, silent failed writes, and
  ABI updates from non-head/reverted uploads. Their recovery and fork contracts
  need explicit regression coverage.
- The object-cache workload retains about 66 MiB RSS for 10.77 MiB of accounted
  key payload. A million warm hits allocate two million times.
- In the synthetic RocksDB workload, sampled file size reaches 63.38 MiB while
  SSTs occupy 8.56 MiB; a retained WAL explains most of the difference. This is
  not a measured mainnet storage-saving opportunity yet.

Normal CTest passes 20/20, as does a separate UBSan build with the `vptr` check
excluded. Passing existing tests does not cover the newly reproduced failures.
Full historical replay, full differential validation, long-running node growth,
and ASan/TSan coverage are **not proven by this review**.

All new experiments use synthetic values, repository fixtures, or fresh local
testnet-genesis basedirs. P2P and production are disabled in node smoke tests.
No live deployment, wallet, producer configuration, existing chain database, or
other project was modified. No network discovery, intrusion testing, protection
bypass, commit, push, deployment, or release was performed.

## Baseline and environment

Same-checkout evidence collected during this review was retained across the
scope clarification after rechecking the commit and unchanged source worktree.
Earlier performance and release reports are historical context only.

| Item | Verified value |
| --- | --- |
| Branch / revision | `main` / `9936a78d482716be1f2ca4bae81a3d786981bbe0` |
| Native identity | `teleno_node 1.3.0-dev.0+9936a78d4827-dirty` |
| Pre-existing changes | Modified `README.md`; untracked high-throughput plan |
| Machine | Apple M4, 10 logical CPUs, 16 GiB RAM |
| OS / filesystem | macOS 26.5, build 25F71; internal APFS SSD |
| Space at baseline | Approximately 80 GiB available on a 460 GiB volume |
| Memory pressure caveat | Approximately 11 GiB swap already used by the host; not an otherwise idle dedicated benchmark machine |
| Compiler / build | AppleClang 16.0.0, native arm64; CMake 4.2.3 / Ninja; Release, `-O3 -DNDEBUG` |
| Test assertions | Test targets explicitly enable assertions with `-UNDEBUG` |
| Dependencies | RocksDB 8.8.1, zstd 1.5.7, GMP 6.3.0, libssh 0.12.0, Boost 1.83.0, Protobuf 3.17.3, gRPC 1.31.0, OpenSSL 3.0.12 |
| cpp-libp2p | `c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`, repository build-script patches applied |
| Reference chain source | Local reference checkout `0ae99eced8b585c4145424e9c2a28f667796cc66` |
| Reference state DB source | Local reference checkout `3a1c904e61afbff59e167f50175519e68046e090` |
| Baseline build | `build/`; cache source path resolves to this repository |
| Fresh native build | `build-audit-20260905/native/`; newly configured, using repository-local dependency prefixes |
| Diagnostic build | `build-audit-20260905/native-undefined/`; `-fsanitize=undefined -fno-sanitize=vptr -fno-omit-frame-pointer` |

The reference revisions are source identities, **not evidence that differential
execution ran during this review**. Dependency packages were reused from local
build-script installs; they were not all rebuilt with sanitizers.

The preserved user-file SHA-256 values are:

```text
README.md
b05852f8e80a0e51cba6a47e1f36d26fd665d5246315d9dc8d36b99eee43c419
docs/performance/KOINOS_HIGH_THROUGHPUT_IMPLEMENTATION_PLAN.md
4ca6127c5ea756250317436c1e4d2a9423825dda442256d50a9dc9acbc1e7a8c
```

`AGENTS.md` and `CLAUDE.md` remain byte-for-byte identical. Neither user file was
edited. The previous `build/` tree contains historical evidence and was not
deleted or reset. All three inspected CMake configurations point to this repository,
not `teleno-extract`.

### Documentation versus implementation

The four requested plans/assessments were read, together with the relevant
install, configuration, quickstart, observer, diagnostics, recovery, backup,
and container documentation. The high-throughput plan includes proposed work;
it is not a completion record. Current code already has a shared RocksDB
manager with nine column families and a shared block cache. Recommending those
as if they were missing would be incorrect.

The July TPS assessment used an M2, 8 GiB RAM, a private producer workload, and
different source/configuration. Its admission and inclusion figures are not
current measurements. This review did not run a producer or submit workload
transactions; it makes no current TPS-capacity claim.

## Execution and data-path map

| Path and principal source | Ownership, work, and persistence boundaries | Examination and remaining gap |
| --- | --- | --- |
| Admission: `src/jsonrpc/jsonrpc_server.cpp`, `src/grpc/grpc_server.cpp`, `src/koinos/chain/controller.cpp`, `src/mempool/mempool_adapter.hpp` | JSON/Protobuf conversion, validation and execution contexts, account nonce/resource checks, calls into mempool; request/session lifetime matters | Source traced; mempool/gRPC functional tests pass; no representative multipayer sustained admission profile |
| Mempool: `src/koinos/mempool/mempool.cpp`, `state.cpp`, `block_applicator.cpp` | State-node ownership across forks; per-node locks; serialized records and pending-account scans; configured expiration and default per-account pending limit | Source and adapter tests; aggregate byte budget and expiration under sustained load need measurement |
| Block execution: `src/koinos/chain/controller.cpp` | Writable state, VM calls, receipt generation, block-store RPC, state finalization/LIB commit, then broadcasts | Controller tests pass; exceptional persistent-write ordering needs broader injected-failure coverage |
| Checked replay: controller, `src/koinos/chain/indexer.cpp`, `rectify.hpp` | Receipt-derived pending state, genuinely fresh root, exact scar matching, one retained lookahead block, one controlled full fallback | Six focused replay/storage tests pass; full archive and differential runs not repeated |
| State: `src/koinos/state_db/state_db.cpp`, `state_delta.cpp`, `backends/rocksdb/` | Parent-linked deltas, tombstones, key/hash temporary vectors, shared cache, batch commit with synchronous durability | Fresh-root/durability tests and actual backend error injection; read lifetime and post-write-error in-memory recovery remain hypotheses |
| Block storage: `src/block_store/block_store.cpp` | Serialized block/receipt and separate highest-topology metadata; shared RocksDB column families | Partial-write/retry/reopen failure reproduced; an atomic batch is missing at this boundary |
| Indexes: `src/transaction_store/`, `src/account_history/`, `src/contract_meta_store/` | Synchronous EventBus handlers; per-service locks; transaction copies, address sequence/record writes, ABI replacement | Direct handler reproductions; no evidence that rejected controller attempts emit these events |
| In-process adapters: `src/core/monolith_rpc_client.hpp`, `event_bus.hpp`, `src/main.cpp` | Signals pass const references, but controller RPC/broadcast adapters still serialize/parse; synchronous handlers share caller latency | Repeated conversion and duplicate block-store notification traced; end-to-end cost not measured |
| RPC sessions: `src/jsonrpc/jsonrpc_server.cpp` | Acceptor workers plus detached per-session threads; synchronous socket reads; session count tied to worker count | Loopback lifecycle/capacity reproduction; detached-thread destruction safety needs supported race/lifetime tooling |
| P2P: `src/p2p/p2p_node.cpp/.hpp` | Peer sync/reconnect threads, peer locks, candidate limits, serialized peer RPC, retained seen-ID sets | Mocked sync/codec tests pass; seen-ID pruning absent in source; no live P2P or long-duration growth test |
| WASM: `src/koinos/vm_manager/fizzy/` | Shared parsed-module cache, parse outside cache lock, new execution instance, bounded VM memory/call depth | Duplicate cache insertion reproduced; diverse-contract RSS and instantiation profile missing |
| Startup/shutdown: `src/main.cpp`, `src/core/service_registry.hpp` | Shared DB owns handles; services borrow them; recovery indexing starts before external services; reverse stop order | Isolated genesis start/clean stop/restart passes; in-flight RPC lifetime not joined by server stop |
| Backup/restore: `src/backup/checkpoint_manager.cpp`, `snapshot_repository.cpp`, `public_restore.cpp`, `backup_service.cpp` | Checkpoint WAL synchronization; content-addressed files, streaming hashing/copy, staging, markers, preserved restore target | Seven backup CTest targets pass; physical power-loss and representative large-restore peak usage not tested |

No lock-contention profile or application-wide allocation attribution was
obtained. This limits architectural recommendations; it does not prevent
local correctness findings supported by direct reproductions.

## Current measurements

### Method

Artifacts live in the already ignored `build-audit-20260905/` directory, denoted
`A` below. Source-only probe files and runners are retained there. Commands,
UTC start times, exit codes, elapsed time, and child resource counters are in
paired `.json`/`.log` files.

Cache and RocksDB short benchmarks use one excluded warm-up process followed
by five sequential fresh-process measurements. Tables report median, minimum,
maximum, and median absolute deviation (MAD). No CPU affinity, host memory
isolation, or filesystem-cache purge was used. Reopened database reads are
application-cache cold, **not guaranteed physical-disk cold**.

The cache probe compiles the actual cache source. The storage probe calls the
actual RocksDB manager with production defaults, not the full consensus path.
Its repeated deterministic 512-byte value is intentionally compressible
across records. Neither microbenchmark is a realistic mainnet block mix.

### Memory and allocations

| Measurement | Median | Range | MAD |
| --- | ---: | ---: | ---: |
| Cache RSS before fill | 1,540,096 B | constant | 0 |
| Cache RSS after 300,000 negative entries | 69,206,016 B | 69,173,248–69,238,784 B | 0 B |
| Cache RSS after `clear()` | 69,255,168 B | 69,222,400–69,287,936 B | 16,384 B |
| Accounted key payload | 11,288,890 B | constant | 0 |
| One million warm hits, 10,000 keys | 0.272132 s | 0.271591–0.274554 s | 0.000496 s |
| `operator new` calls during those hits | 2,000,000 | constant | 0 |
| Storage-process RSS after reopened and warm reads | 149,192,704 B | 148,520,960–149,405,696 B | 81,920 B |

The cache's configured payload budget is 64 MiB. Null-value entries use
32-character prefixes plus decimal suffixes. The RSS increment is about six
times the counted key payload, reflecting map/list/string overhead and
allocation behavior. RSS remaining high after clear is **not proof of a leak**:
the containers are cleared and the allocator may retain freed pages.

Hit timing excludes key construction and cache filling. A diagnostic global
allocation counter adds the same atomic-counting overhead to every run; do not
treat this timing as an uninstrumented production latency. The source's list
erase/push and duplicated heap-backed key account for two allocations per hit.
An additional bounded 30-million-hit run took 8.184 s, allocated 60 million
times, and ended at about 4.81 MiB RSS. This is not a long-duration leak test.

An isolated genesis observer, with P2P, production, gRPC, and JSON-RPC disabled,
reported flat sampled RSS of 24,412,160 B for five samples over ten seconds.
After clean restart, the five samples were 25,280,512 B. Initial readiness was
0.171 s and restart readiness 0.057 s; both stopped cleanly. These are two
smoke observations, not five-repetition startup benchmarks or mainnet sizing.

### Storage and I/O

Workload: 100,000 distinct keys, one 512-byte repeated deterministic value,
54,888,890 logical key/value bytes, default manager tuning, `chain_state` CF.
After writes, the probe checks `FlushWAL(true)`, waits for a CF flush, compacts,
closes/reopens, and verifies every value in two read passes.

| Measurement | Median | Range | MAD |
| --- | ---: | ---: | ---: |
| 100,000 default-option puts | 0.202043 s | 0.201307–0.226621 s | 0.000736 s |
| WAL synchronization + state CF flush | 0.113181 s | 0.111231–0.122662 s | 0.000729 s |
| Explicit state CF compaction | 0.003705 s | 0.002889–0.003862 s | 0.000157 s |
| Reopen | 0.048879 s | 0.048715–0.053802 s | 0.000164 s |
| First 100,000 reopened reads | 0.138889 s | 0.133638–0.141179 s | 0.002290 s |
| Second 100,000 warm reads | 0.118422 s | 0.114304–0.124375 s | 0.004118 s |

Median within-run latencies for batches of 1,000 puts are p50 1.984 ms,
p95 2.290 ms, and p99 2.504 ms. These are **batch**, not individual-operation,
percentiles. They use the nearest-rank observations from 100 batches.

| Sampled database phase | Apparent file bytes | Allocated bytes | SST bytes | WAL bytes |
| --- | ---: | ---: | ---: | ---: |
| Before flush | 57,447,711 | 57,462,784 | 0 | 57,301,430 |
| After state flush | 66,452,198 | 66,473,984 | 8,978,765 | 57,301,430 |
| After state compaction | 66,454,802 | 66,473,984 | 8,978,765 | 57,301,430 |
| After reopen/read | 9,303,688 | 9,330,688 | 8,980,147 | 189 |

The same 100,000 values are verified after reopen. No data is deleted to obtain
the last row. The large pre-reopen WAL remains even after compacting the state
CF; other CF lifecycle/flush boundaries must be considered. These are sampled
sizes, not a continuously measured peak. Do not compare only SST size with a
whole live database or claim that restarting is an optimization.

The puts use ordinary asynchronous write options; the final explicit WAL sync
does not turn their timing into per-block synchronous commit throughput.
macOS child input/output-operation counters returned zero and were not useful
for physical I/O attribution. **Actual bytes written, write amplification,
compaction stalls, disk latency, and growth per real block remain unmeasured.**

## Findings register

### Stored-data roles and retention constraints

| Data | Role under the current implementation | Constraint on a proposed optimization |
| --- | --- | --- |
| `chain_state`, chain metadata, root/head/LIB state | Required consensus state and durable recovery identity | Preserve objects, roots, atomicity and reopen semantics; no pruning/reset shortcut |
| Block/receipt records and block topology | Durable archive used by current replay and block-query APIs | Historical bytes are not automatically disposable; changing retention changes recovery/API capability |
| Transaction, account-history and contract-meta CFs | Potentially rebuildable optional indexes | Prove source completeness, fork semantics, rebuild cost and API behavior before replacement or removal |
| Storage layout/version and recovery markers | Required operational identity and activation state | Preserve format detection, migration safety and observer-first behavior |
| WAL and unflushed state | Required durability until RocksDB releases it | Never delete manually; measure legitimate flush/retention boundaries |
| SST/block cache, object cache, parsed modules | SSTs are durable; the corresponding in-memory caches are disposable representations | Distinguish durable files from caches; cache eviction must preserve value lifetime and may increase I/O |
| Completed backups and content-addressed objects | Operator-owned recovery assets | Retention/deletion is a separate policy/action, with reference checks and recovery-space accounting |
| New scratch, staging, partial downloads and diagnostic files | Task-owned temporary/rebuildable artifacts, or unfinished recovery work | Prove exact ownership and recovery status before cleanup; this review preserves them |
| Runtime logs | Diagnostic/operational evidence, with deployment-specific capture | Review capture/rotation configuration before estimating growth; no log-volume or cleanup saving measured here |

### Register

Evidence labels: **R** = reproduced behavior in an isolated actual-code test;
**M** = measured resource cost; **H** = source-supported hypothesis requiring
further measurement or a behavioral contract. Severity describes potential
functional/operational impact, not an externally assessed security rating.

| ID | Finding | Category / severity | Evidence / confidence | Proposed package |
| --- | --- | --- | --- | --- |
| TQ-01 | State read/flush errors are not propagated | Correctness/durability, high | R, high | WP1 |
| TQ-02 | Partial block persistence is not repaired by retry | Persistence/recovery, high | R, high | WP1 |
| TQ-03 | RPC sessions outlive stop and idle sessions consume all slots | Lifecycle/memory, high | R for lifecycle/capacity; H for use-after-destruction | WP2 |
| TQ-04 | Object-cache clear and oversize insertion can hang | Cache correctness, high | R, high | WP3 |
| TQ-05 | Duplicate module-cache insertion corrupts effective LRU capacity | Cache correctness, medium | R, high | WP3 |
| TQ-06 | Optional index failure and redelivery semantics are incomplete | Index consistency, high | R for outputs; fork/API expectations need confirmation | WP4 |
| TQ-07 | Seen block/transaction sets have no pruning policy | Retained memory, medium | H, high for source growth; runtime rate unmeasured | WP5 |
| TQ-08 | Cache payload accounting omits substantial overhead; hits allocate | Memory/CPU, medium | M, high for measured workload | WP3 |
| TQ-09 | Internal serialization and repeated notification add work | Simplification/memory, medium | H, medium | WP7 |
| TQ-10 | Shared-CF WAL lifetime complicates disk sizing | Storage/observability, medium | M for example; tuning benefit unproven | WP6 |
| TQ-11 | Backup download bounds and publication durability need proof | Recovery/resources, medium | H, medium | WP6 |
| TQ-12 | Borrowed state values and failed-commit ownership need investigation | Lifetime/error recovery, high if confirmed | H, medium | WP1 investigation gate |

### TQ-01 — State error propagation

- Location: `rocksdb_backend::get()` and `flush()` in
  `src/koinos/state_db/backends/rocksdb/rocksdb_backend.cpp`.
- Trigger/root cause: a non-NotFound `Get` failure falls through to `nullptr`;
  both `Flush` status results are discarded.
- Reproduction: `runtime-probe-release state-read-error <new-path>` reports
  `injected_io_error_returned_null=1`. `state-flush-error` reports
  `failed_flushes_returned_success=2`. See `A/state-read-error.*` and
  `A/state-flush-error.*`.
- Impact: callers cannot distinguish a missing object from a failed read, or
  a successful flush from a failed one. No end-to-end consensus divergence or
  physical data loss was induced. CPU/memory/disk savings are not the purpose.
- Correction: throw/report typed errors for non-NotFound statuses and check
  every flush result; define safe fatal/retry handling at callers and shutdown.
- Complexity/risk: small local changes, medium integration risk because callers
  previously observed success. No format change; depends on failure contracts.
- Acceptance: injected read failure never becomes absence; failed flush is
  observable; missing keys remain missing; no head advancement on failed commit;
  existing durability and checked-replay tests remain green.

### TQ-02 — Block record/topology atomicity

- Location: `BlockStore::add_block()`, `put_record_bytes()`, and
  `update_highest_block()` in `src/block_store/block_store.cpp`.
- Trigger/root cause: record and highest topology use separate writes. Inject
  failure into the metadata CF after the record succeeds. Retry sees the record
  and returns early without updating metadata.
- Reproduction: `runtime-probe-release block-atomic <new-path>` reports
  `after_successful_retry_stored=1 highest=0` and, after reopen,
  `reopened_stored=1 reopened_highest=0`. See `A/blockstore-reopen.*`.
- Impact: a successful retry leaves archive topology inconsistent with stored
  data, potentially truncating recovery discovery. This tests the storage API
  with a synthetic block, not a successfully validated mainnet block.
- Correction: one checked cross-CF WriteBatch for the required atomic unit;
  define how existing partial records are diagnosed/repaired without deleting
  data or changing fork/highest-topology semantics.
- Complexity/risk: medium; source-compatible layout possible, but interrupted
  archives and retry idempotency need tests. Depends on WP1 error contracts.
- Acceptance: each injected write failure leaves a coherent pre/post state;
  retry and reopen converge to stored block plus correct topology. Benefit is
  integrity; no throughput gain is yet measured.

### TQ-03 — RPC lifecycle and idle capacity

- Location: `JSONRPCServer::do_accept()`, `handle_session()`, `stop()` in
  `src/jsonrpc/jsonrpc_server.cpp`.
- Trigger/root cause: detached sessions capture `this`; synchronous reads have
  no session idle deadline; `stop()` joins only acceptor/io workers. Session
  admission capacity equals configured worker count.
- Reproduction: `runtime-probe-linked rpc <unused-loopback-port>` with one slot:
  first health request returns 200; a second connection returns 503 while the
  first is idle; the first returns 200 again **after stop returned**.
  See `A/rpc-linked.*` and `A/rpc-lifecycle.*`.
- Evidence caveat: the standalone probes subsequently exit with SIGBUS during
  Boost error construction on socket EOF. A fresh full node handles two ordinary
  health requests, connection closes, and shutdown successfully (`A/node-rpc.*`).
  The probe crash is not established as a production crash or a proven dangling
  `this` dereference. Lifecycle/capacity observations precede that failure.
- Impact: idle clients retain session threads/sockets and exhaust slots;
  destruction after stop has a source-supported lifetime hazard. No exploit or
  external endpoint was tested.
- Correction: explicit session ownership, bounded idle/request work, cancellation
  and joining/draining before dependent services are released; separate worker
  count from the capacity policy if needed. Do not merely raise thread counts.
- Complexity/risk: medium; preserve HTTP keepalive, RPC responses, cancellation
  semantics and administrative access boundaries.
- Acceptance: stop completes within a test deadline with zero live sessions;
  idle sessions expire; well-behaved clients regain capacity; repeated
  start/stop and in-flight request tests pass under supported lifetime tooling.

### TQ-04 — Object-cache invariants

- Location: `object_cache::put()` and `clear()` in
  `src/koinos/state_db/backends/rocksdb/object_cache.cpp`.
- Trigger/root cause: `clear()` does not reset `_cache_size`; `put()` has no
  oversize/empty-LRU guard before repeatedly removing `_lru_list.back()`.
- Reproduction: capacity 64 bytes, insert a 61-byte entry, clear and reinsert;
  or insert a 66-byte entry into the empty cache. Both bounded runs fail to
  terminate within five seconds, consuming approximately one CPU core.
  See `A/cache-clear-bounded.*`, `A/cache-oversize-ubsan.*`.
- Impact: invalid empty-list access and loss of forward progress at this API.
  Reachability of an oversized entry under default protocol limits is not
  established; the small-capacity reproduction does not prove it remotely.
- Correction: reset accounting, make capacity arithmetic overflow-safe, define
  zero-capacity behavior, and bypass/reject an individually oversized cache
  entry without rejecting otherwise valid state data.
- Complexity/risk: small; preserve normal removal and all replay tombstones.
- Acceptance: empty/clear/reuse/replacement/oversize tests terminate and preserve
  map/list/accounting invariants; roots and persisted values remain identical.

### TQ-05 — Module-cache duplicate replacement

- Location: `module_cache::put_module()` in
  `src/koinos/vm_manager/fizzy/module_cache.cpp`; get/parse/put sequence in
  `fizzy_vm_backend.cpp`.
- Trigger/root cause: replacing an existing ID leaves the old list entry.
  Parsing outside the cache lock also permits two misses to insert the same ID.
- Reproduction: capacity two; insert A, A, B using one valid empty WASM module.
  A is missing even though only two distinct IDs were inserted. Exit 3 denotes
  the reproduced incorrect result, not a passing software test.
  See `A/module-duplicate-release.*` and `A/module-duplicate-ubsan.*`.
- Impact: avoidable reparsing and eviction; real contract-cache miss rates and
  memory impact are unmeasured.
- Correction: idempotent replacement/move-to-front, unique LRU membership, and
  explicit zero-capacity handling; evaluate single-flight parse only if needed.
- Complexity/risk: small/low for LRU correction; concurrency/VM reuse proposals
  are separate. Preserve execution instances and protocol resource accounting.
- Acceptance: duplicate and concurrent same-ID insertion retain correct
  membership/capacity; unchanged execution results; no speculative speedup claim.

### TQ-06 — Optional index consistency

- Locations: `AccountHistory::handle_block_accepted()`/`next_sequence()`,
  `TransactionStore::add_included_transaction()`, and
  `ContractMetaStore::handle_block_accepted()`.
- Reproduction: `runtime-probe-release indexes <new-path>` delivers the same
  synthetic block twice and observes two history records. An injected
  transaction-index Put error returns normally with zero indexed records.
  An upload in a non-head block changes the queried ABI to `orphan-abi`; an
  upload with a reverted receipt changes it to `reverted-abi`.
  See `A/index-integrity.*`.
- Root causes: no history delivery checkpoint/idempotency marker; sequence and
  entry writes are separate; transaction Put statuses are ignored; ABI updates
  inspect operations without checking head/reverted state.
- Impact: duplicate growth and incomplete query results. The ABI observations
  are confirmed, but canonical versus accepted-fork API semantics require a
  reference/contract decision before choosing a fix. Account history is a
  limited optional block-oriented index, not full service parity.
- Important boundary: direct handler calls do not prove that rejected replay
  attempts broadcast events. Existing controller/indexer rejection tests pass.
- Correction: checked batches and resumable/indexed-height checkpoints, explicit
  redelivery behavior, fork-aware ABI policy, and truthful health/error reporting.
- Complexity/risk: medium to high; depends on an index consistency/API contract.
  Asynchronous indexing and new persistent layouts must not be slipped into a
  local error-handling patch.
- Acceptance: duplicate delivery has the documented effect, no partial sequence
  allocation survives failure, errors are visible, recovery converges, and
  canonical/fork/reverted ABI fixtures match the agreed behavior.

### TQ-07 — Retained P2P seen IDs

- Location: `_seen_blocks`/`_seen_transactions` in `src/p2p/p2p_node.hpp`; inserts
  in sync/gossip handlers. Source search finds no erase/clear path for either
  set. LIB handling prunes the fork watchdog, not these sets.
- Trigger: sustained distinct successfully handled IDs over one process life.
- Evidence: source-supported unbounded cardinality; no measured node RSS slope.
  Reproduce source inspection with `rg -n '_seen_' src/p2p/p2p_node.*`.
- Impact/benefit: each retained string/tree node costs memory; the long-run rate
  and attainable memory saving are unknown. Do not reuse the object-cache
  amplification factor as an estimate for this different container.
- Proposal: bounded deduplication windows informed by LIB/fork and transaction
  expiration semantics, with counters, eviction tests, and duplicate-work limits.
- Complexity/risk: medium; premature eviction can increase work or affect gossip.
  Acceptance requires a fake-transport sustained workload proving a plateau and
  unchanged sync, fork, and duplicate-handling behavior. Depends on WP5 metrics.

### TQ-08 — Cache overhead and hit allocations

- Location: object-cache map/list/accounting and both caches' hit paths.
- Evidence: five-run measurements above; source `erase`/`push_front` recreates
  list nodes and copies keys. See `A/memory-summary.json`, `A/hits-summary.json`.
- Impact: significant uncounted memory and allocator traffic, even on hits.
- Proposal: list splice and reuse of the located map iterator; truthful entry
  overhead accounting and measured bounds; consider avoiding duplicated keys
  only after preserving ownership/iterator safety.
- Expected benefit: zero list/key allocations on the measured warm-hit path is
  a testable target, **not an implemented result**. Memory savings are not yet
  quantified. Smaller caches can increase RocksDB reads and VM parse work.
- Complexity/risk: small to medium; depends on TQ-04/TQ-05 invariants. Acceptance
  requires unchanged LRU/results, zero hit allocations for this fixture, and
  before/after memory, miss, and I/O measurements with noise accounted for.

### TQ-09 — In-process conversions and repeated persistence notification

- Location: `src/core/monolith_rpc_client.hpp`, controller RPC/broadcast call
  sites, `BlockStore::handle_block_accepted()`, and `src/main.cpp` event wiring.
- Evidence: code trace shows serialize/parse around synchronous in-process
  calls, future/promise wrappers, and a later accepted-block notification after
  the controller has already requested block persistence. EventBus itself
  passes const references; it is not a serialized broker.
- Impact: temporary buffers/copies and repeated lookup work. CPU time, bytes per
  block, and allocation share are unmeasured; this is not a proven bottleneck.
- Proposal: instrument these boundaries first; then consider typed adapters or
  avoiding genuinely redundant notification work while preserving error timing,
  receipt availability, subscriber order, and compatibility interfaces.
- Complexity/risk: medium; depends on WP1/WP4 contracts. Acceptance requires a
  profile attributable to these paths, identical outputs/events, and reproducible
  allocation reduction. No broad architecture rewrite is justified yet.

### TQ-10 — WAL lifecycle and disk observability

- Location: `src/storage/rocksdb_manager.cpp`, shared CF options and statistics;
  block/state/index write and flush boundaries.
- Evidence: the storage workload's 57.3 MB WAL persists through state CF flush
  and compaction, then disappears during close/reopen lifecycle. See
  `A/storage-summary.json`. Cross-CF WAL dependencies are a hypothesis to trace,
  not a license to force repeated global flushes.
- Current bounds: shared 256 MiB block cache, 64 MiB per-CF write-buffer setting,
  three-buffer setting, 256 MiB DB write-buffer setting, four background jobs.
  These are capacities/settings, not an additive measured RSS total or a hard
  process-memory limit. The object cache is additional.
- Impact: SST-only reports understate working space; aggressive compaction or
  flushing may increase I/O and stalls. Missing property reads currently map to
  zero, conflating unsupported statistics with real zero values.
- Proposal: expose WAL/live/obsolete bytes, memtable usage, cache usage,
  compaction/flush/stall statistics and availability; measure realistic workloads
  before changing tuning or retention defaults.
- Complexity/risk: low for read-only metrics, medium/high for tuning. Acceptance:
  comparable logical states, measured peak allocation and I/O, unchanged sync
  guarantees and reopen/recovery behavior. Storage-saving percentage is unknown.

### TQ-11 — Backup download and publication boundaries

- Locations: synchronous download/read loop in `src/backup/public_restore.cpp`;
  `write_file_atomic()` and completion/staging publication in
  `src/backup/snapshot_repository.cpp`.
- Evidence: downloads use an effectively unlimited HTTP body bound; stream
  expiry is configured around synchronous operations, while the installed Beast
  `basic_stream.hpp` documents expiry for asynchronous operations. Publication
  writes a temporary stream and renames without explicit post-write status or
  file/directory synchronization in that helper.
- Trigger/impact: interrupted/stalled local fixture transfers, storage write
  errors, or a crash during marker publication could consume unnecessary space,
  delay cancellation, or expose incomplete durable metadata. End-to-end failure
  and power-loss outcomes were not reproduced; these remain hypotheses.
- Existing strengths: streaming hashing uses a 1 MiB buffer, checkpoints check
  WAL synchronization, content-addressed objects can be reused, and restore
  staging/observer markers are tested. Do not replace streaming with whole-file
  buffering or remove validation.
- Proposal: bounded local stalled-transfer tests; size-aware budgets/cancellation;
  injected stream failures and an explicit durable publication contract. Ordinary
  regression fixtures are sufficient; external probing is unnecessary.
- Complexity/risk: medium; requires backup/recovery contract and platform tests.
  Acceptance: bounded completion/cancellation and space, no false success on
  writes, valid old-or-new published snapshot, preserved observer-first recovery.

### TQ-12 — State ownership under eviction and failed commit

- Locations: `rocksdb_backend::get()` returns a raw value pointer after releasing
  cache ownership/lock; `state_delta::commit()` resets the root backend pointer
  before the final persistent batch succeeds.
- Hypotheses: concurrent read-caused eviction may outlive a borrowed pointer;
  an exception during commit may leave in-memory ownership inconsistent even
  when RocksDB rejects the batch atomically. API lifetime guarantees and outer
  locking must be considered; neither failure is confirmed here.
- Impact if confirmed: memory lifetime faults or unsafe retry/recovery, not a
  quantified resource-saving opportunity.
- Proposal: explicit interleaving and batch-failure tests against actual APIs,
  followed by ownership-preserving changes only if the hypothesis holds.
- Complexity/risk: medium/high, consensus-adjacent. Depends on supported lifetime
  tooling and fault hooks. Acceptance includes no stale/dangling value access,
  well-defined post-failure state, unchanged roots, and differential gates before
  any successful-execution or commit-order behavior is changed.

## Build and test evidence

Commands are relative to the repository root. The paired artifact prefix gives
the full command, output, result, and timing. Diagnostic failure reproductions
must not be confused with passing regression tests.

| Command/check | Actual result | Artifact prefix under A |
| --- | --- | --- |
| `KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh` | PASS, 51.99 s, native arm64 rerun | `baseline-dependencies-arm64` |
| `cmake --build build --parallel` | PASS, 0.70 s | `baseline-build` |
| `ctest --test-dir build --output-on-failure` | PASS 20/20, 22.07 s CTest time | `baseline-ctest` |
| `./build/teleno_node --version` | PASS, identity above | `baseline-version` |
| `./build/teleno_node --help` | PASS | `baseline-help` |
| Fresh configure / build, parallel 4 | PASS, 133 steps, 63.82 s build | `fresh-configure`, `fresh-build` |
| Fresh CTest / CLI | PASS 20/20, 23.84 s; version/help pass | `fresh-ctest`, `fresh-cli` |
| Focused controller/indexer/fixture/storage CTest | PASS 6/6, 1.33 s | `focused-tests` |
| `/usr/bin/python3 -m unittest -v tests/scripts/benchmark_transaction_tps_test.py` | PASS 4/4 | `baseline-python` |
| Separate UBSan build, excluding vptr | PASS build; CTest 20/20, 23.66 s; halt-on-error enabled | `ubsan-build-no-vptr`, `ubsan-ctest` |
| Fresh offline genesis node + restart | PASS, two clean exits; ten total RSS samples | `node-idle` and per-run logs |
| Fresh offline node health requests | PASS, two 200 responses and clean shutdown | `node-rpc` |
| Existing Linux/arm64 image version/help, network disabled | PASS; reports `1.3.0-dev.0+9936a78d4827` | `container-version`, `container-help` |
| Final normal CTest, repository-local temporary directory | PASS 20/20, 16.02 s | `scope-ctest` |
| Final UBSan CTest, repository-local temporary directory | PASS 20/20, 16.76 s, same vptr exclusion | `scope-ubsan-ctest` |
| Final benchmark-script tests, repository-local temporary directory | PASS 4/4 | `scope-python` |

The final reruns set `TMPDIR` to `build-audit-20260905/tmp-review` explicitly,
keeping framework-created temporary fixtures inside the repository. Differences
between suite durations are not optimization results.

The cached container image was not rebuilt or pulled during its smoke check.
Matching embedded source identity is useful smoke evidence, not verification
of a newly built release artifact or deployed service.

### Diagnostic limitations and setup failures

- The first dependency wrapper used an x86_64 Python and inherited Rosetta
  architecture. Its attempt was stopped; the successful rerun used native
  `/usr/bin/python3`. Rejected setup output is retained separately.
- The initial UBSan link failed because the installed RocksDB library lacks
  RTTI symbols. Disabling **only UBSan vptr instrumentation** permitted a full
  current-source build/test run. This does not validate vptr behavior or
  sanitize all prebuilt dependencies.
- A legacy local `pkg-config` stalled during the diagnostic configure. The
  retry used explicit local CMake dependency packages and
  `PKG_CONFIG_EXECUTABLE=/usr/bin/false`; no system tool was modified.
- ASan fails before `main()` with `sanitizer_malloc_mac.inc:189` on this
  macOS/toolchain. TSan also fails during runtime initialization before `main()`.
  Independent `hello-asan`/`hello-tsan` probes reproduce the environment failures.
  These are not findings against Teleno, and no ASan/TSan clean bill is claimed.
- LLDB did not progress past launching the isolated probe; the owned diagnostic
  processes were stopped. A bounded `sample` attempt produced no usable profile
  before its timeout. Allocation counting/timing are available; stack-sampling
  attribution is not. No permissions or protections were changed.
- Earlier probe compile errors and the RPC EOF SIGBUS are retained as diagnostic
  limitations. They do not negate the separately recorded functional outputs
  or justify claiming a full-node crash.

## Historical and recovery evidence boundary

Current fixture tests validate the checked-in blocks 30,504,202/30,504,203 and
32,789,377/32,789,378, their manifests/hashes, receipt-derived behavior, and the
exact exception. Controller and indexer tests cover rejected replay, controlled
fallback, lookahead and target handling; pending-root/durability tests cover
tombstones, root freshness, flush and reopen.

This is **fixture-level current evidence**, not a current replay of the full
range. The existing replay plan and validation report describe older full-range
and reference runs. Those results have not been reclassified as new results.

The missing prerequisite for current full differential validation is a verified,
repository-local immutable archive containing the full blocks/receipts across
both historical boundaries (starting before 30,504,202 and extending beyond
32,789,378), plus matching verified pre-range checkpoints. Existing local
checkpoint/journal artifacts are incomplete substitutes: the receipt journal
cannot supply full blocks for full reexecution. Its source archive is not an
authorized repository-local input. This review did not open or copy that
external source, modify the original checkpoints, or run a live catch-up.

Therefore **no new full-history parity, no zero-unexplained-mismatch claim over
the historical range, and no mainnet memory/disk-growth claim is made**.

## Coverage, blockers, and unresolved questions

| Required area | Current evidence | Exact remaining prerequisite or uncertainty |
| --- | --- | --- |
| Baseline/source/build/tests | Recorded and revalidated; normal and UBSan tests pass | Prebuilt dependency internals are not wholly instrumented |
| Functional bugs | Actual cache/backend/block-store/index/RPC reproductions | New reproductions are diagnostic artifacts, not committed regression tests |
| Admission/mempool | Source path and functional tests | Representative signed fixture workload on an isolated non-producing execution harness; no public submission permitted |
| Block/replay/reorg | Controller/indexer/fixture tests and code tracing | Complete local archive and matching checkpoints for full historical/differential replay |
| Memory bounds/lifetimes | Cache measurements, module reproduction, source ownership review | Supported ASan/TSan environment; long-duration mixed workload and per-queue/VM metrics |
| Disk/flush/reopen | Five-run synthetic RocksDB comparison and failure injection | Representative block mix, actual write counters/stall metrics, multi-level compaction history |
| P2P/RPC | Mocked P2P suite and isolated loopback lifecycle check | Sustained fake-transport dedup/memory tests; RPC teardown attribution |
| WASM | Actual module cache test and call-path review | Diverse realistic contract modules, parse/instance/cache-byte attribution |
| Backup/restore/recovery | Source review, checkpoint/snapshot/public-restore/service tests | Bounded stalled-transfer fixtures, write-fault hooks, crash-consistency contract and supported isolated crash harness |
| Simplification | Concrete map/list, adapter, write-boundary candidates | End-to-end allocation/profile evidence before broad conversion/concurrency changes |

Peak disk space during a real full replay, compaction, backup and restore is not
quantified. The microbenchmark table must not be used as a capacity estimate.
Real block growth, long-duration leaks, lock contention, and physical write
amplification remain open measurements, not zeros.

## Reproduction and artifact handling

From the repository root, use `/usr/bin/python3` on this host. Preserve the
existing evidence; use a new output name and a non-existing scratch directory
when rerunning mutation probes. The runners reject existing state paths.

```bash
# Rebuild the standalone runtime probe using the current configured libraries.
/usr/bin/python3 build-audit-20260905/compile_probe.py release

# Examples: new paths only, no node processes or external connections.
/usr/bin/python3 build-audit-20260905/run.py repeat-read-error \
  ./build-audit-20260905/runtime-probe-release state-read-error \
  build-audit-20260905/repeat-read-error-state
/usr/bin/python3 build-audit-20260905/run.py repeat-block-atomic \
  ./build-audit-20260905/runtime-probe-release block-atomic \
  build-audit-20260905/repeat-block-atomic-state

# Focused existing regression suite.
ctest --test-dir build-audit-20260905/native \
  -R 'controller_delta|indexer_lookahead|historical_fast_replay|rocksdb_manager|state_db' \
  --output-on-failure
```

Probe/build scripts: `run.py`, `compile_probe.py`, `configure_fresh.py`,
`cache_probe.cpp`, `runtime_probe.cpp`, `fault_db.hpp/.cpp`, `measure.py`,
`node_smoke.py`, `observer.yml`, `sanitizer_hello.cpp`, and `profile_cache.py`.
The original cache compiler command is recorded in `compile-cache-release.json`.
`measure.py` uses fixed names; preserve/move the complete experiment directory
or adapt its output prefix before rerunning it. Do not delete its state trees.

The source-only reproduction bundle and evidence manifest are local ignored
artifacts: `reproduction-sources.tar.gz` and `evidence-manifest.json` under A.
The complete retained diagnostic directory occupies approximately 1.1 GiB,
including separate builds and synthetic databases. It was not cleaned up.
They are not shipped by a normal repository clone. Promote minimized regression
fixtures into `tests/` only in a separately authorized implementation task.
Raw tool logs can contain local toolchain paths; publish only reviewed,
sanitized extracts. No operational credentials are needed for reproduction.

## Review progress and completion

| Review package | Files changed | Evidence/result | Exit / remaining risk |
| --- | --- | --- | --- |
| R1 baseline and instruction review | Ignored environment/build logs only | Source/worktree identity, dependency build, fresh build, 20/20 CTest, CLI | Satisfied; original changes preserved |
| R2 functional/resource tracing | Ignored probe source and scratch fixtures | TQ-01–TQ-06 reproductions; all execution paths mapped | Satisfied as review; hypotheses explicitly retained |
| R3 memory/disk measurements | Ignored benchmark drivers/results | Warm-up + five samples; allocation counts, RSS, flush/compaction/reopen sizes | Satisfied for stated workloads; production sizing unproven |
| R4 diagnostic and compatibility review | Ignored sanitizer configs/logs | UBSan 20/20, focused 6/6, Python 4/4; ASan/TSan and historical gaps identified | Satisfied as qualified coverage; no broad parity/lifetime proof |
| R5 report and remediation planning | This report and the linked remediation plan | Ranked findings, first three packages, acceptance/rollback gates; `finalize_evidence.py` verifies artifacts, source preservation and local links | Review deliverables complete; production work not started |

Completion means the requested review areas have evidence or exact documented
gaps and the plan is ready for review. It does **not** mean all bugs were found,
all compatibility gates passed, or any memory/disk optimization was achieved.
