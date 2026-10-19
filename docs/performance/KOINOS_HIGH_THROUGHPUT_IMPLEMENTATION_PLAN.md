# Koinos High-Throughput Implementation Plan

Status: Proposed research and implementation plan
Date: 2026-07-12
Scope: Teleno and the legacy Koinos microservice node
Reference Teleno commit: `acecdaeb72395b4e8e6997bc649eafb2048f2960`

## 1. Executive Summary

Koinos can gain substantial throughput without becoming a different
blockchain. The recommended first path is to preserve linear Koinos blocks,
Proof of Burn (PoB), Mana/resource-credit (RC) accounting, transaction and
receipt formats, existing contract state, state Merkle roots, fork choice, and
existing APIs while changing how a node performs the work internally.

The target architecture combines:

1. a measured, bounded pipeline for admission, signature verification,
   execution, persistence, indexing, and propagation;
2. a lower-copy in-process Teleno hot path and a batched but API-compatible
   legacy hot path;
3. deterministic optimistic parallel execution in Teleno, with dynamic
   read/write-set discovery, multiversion state, validation, conflict replay,
   and commit in the existing transaction order;
4. the legacy chain service as an independent sequential consensus oracle;
5. optional, non-authoritative state-access hints to improve scheduling;
6. resource-limit changes only after heterogeneous validators demonstrate
   replay and propagation headroom; and
7. no consensus or DAG replacement unless compatible execution and networking
   have first been proven insufficient.

This is not a claim that Koinos can immediately match a published Solana TPS
number. TPS figures from different workloads and confirmation definitions are
not comparable. The current private Teleno evidence establishes approximately
451 admitted simple transfers per second and approximately 180 transfers per
second while the corresponding private-chain blocks were flowing. It does not
establish geographically distributed inclusion, finality, or mainnet capacity.

The highest-confidence compatible gain is expected from removing repeated
execution and global serialization, then applying deterministic speculative
execution. An eight-core implementation can plausibly provide roughly 2.5-5x
execution speedup for mostly independent transactions, based on Amdahl-style
modeling with 80-90% parallelizable work; this is a modeled range, not a
benchmark result. Contended payer and hot-contract workloads may remain near
1x and can be worse if replay is uncontrolled. Block resource budgets and the
three-second target interval remain independent ceilings.

The exact point at which a consensus change becomes necessary is therefore a
gate, not a date: only after (a) Teleno and legacy produce byte-identical
consensus results, (b) compatible parallel execution meets its scaling target,
(c) heterogeneous validators validate and relay maximum tested blocks within
the safety margin, and (d) confirmed and irreversible TPS is still limited by
a consensus-visible rule. Mandatory access declarations, changed transaction
validity, changed nonce/RC semantics, a different fork choice, or a different
state model require a coordinated consensus upgrade. A full DAG or Sui-like
object chain would effectively replace Koinos and is rejected.

The evidence supports a near-term compatible ambition of **hundreds of
confirmed simple-transfer TPS on the benchmark hardware class**, provided
parallel replay, propagation, and governed resource budgets all validate. It
does not yet support a claim of thousands or tens of thousands of finalized
TPS. At the measured transfer charge, 10,000 TPS with a three-second interval
would require about 24.6 billion compute units and 9.45 MB of raw transaction
bytes per block before protocol overhead—approximately 273 times the private
benchmark's effective compute budget. Reaching that region would require
measured reductions in physical execution cost, far more parallel hardware
utilization, propagation improvements, safe resource reparameterization, and
possibly the Phase 9 consensus change. This is the honest boundary between a
credible roadmap and a marketing number.

## 2. Research Method and Six Iterations

This plan was produced through six explicit passes. Later passes revise earlier
ideas rather than treating the first architecture as the answer.

### Iteration 1 — Current Architecture Audit

The actual client-to-irreversibility path was traced in Teleno and in the
legacy JSON-RPC, chain, mempool, producer, P2P, block-store, and RabbitMQ
services. Exact commit pins and code entry points are recorded in Sections 3,
4, and 11. The audit found repeated validation/execution, a globally serialized
mempool insertion path, sequential canonical execution and Merkle generation,
synchronous post-acceptance handlers, and extensive legacy process/AMQP
serialization.

### Iteration 2 — Candidate Generation

Six architectures were considered: sequential pipelining, Block-STM-style
optimistic execution, mandatory access declarations, a hybrid hints-plus-OCC
model, DAG-assisted dissemination/dependency scheduling, and full DAG/object
consensus. They are compared in Section 7 and exercised against all Koinos
semantics in Section 8.

### Iteration 3 — Compatibility Review

Every work package is classified using one of six compatibility classes in
Section 10. The review establishes a large implementation-only zone: Teleno
may execute speculatively, discard conflicting results, and commit in canonical
order so long as its observable result is exactly the current sequential
result. The old node can validate the resulting block sequentially without
knowing that speculation occurred.

### Iteration 4 — Quantitative Model

Section 6 separates measured values, derived values, design targets, and
speculation. The minimum-capacity model covers admission, execution, block
resources, propagation, validator replay, and finality for independent,
contended, contract, DeFi, mixed, and adversarial workloads.

### Iteration 5 — Adversarial Review

Each candidate was attacked for nondeterminism, replay storms, malicious
hints, RC/nonce races, root divergence, crash recovery, reorganization,
heterogeneous validators, partitions, and centralization. The result is a
bounded-replay scheduler, ignored-by-default hints, sequential fallback,
dual-client differential gates, and hard resource limits. See Section 16.

### Iteration 6 — Final Synthesis

The recommendation is a staged hybrid: comparable benchmarks, compatible
pipeline work, deterministic OCC in Teleno, sequential legacy verification,
optional hints, and only then measured network or governed resource changes.
Mandatory declarations and consensus/DAG changes are contingency tracks, not
the default roadmap.

## 3. Current Benchmark Baseline

### 3.1 Evidence and Limitations

The committed baseline consists of:

- [`transaction-tps-benchmark.md`](transaction-tps-benchmark.md);
- [`tps-optimization-assessment.md`](tps-optimization-assessment.md); and
- [`scripts/benchmark-transaction-tps.py`](../../scripts/benchmark-transaction-tps.py).

The benchmark implementation was introduced at Teleno commit
`a2db3d5d034aec4b12ab6a94f73d98ad7041f802`; the assessment is present at the
reference commit at the top of this document. The original temporary run
directory is no longer available, so the committed report and benchmark source
are the retained evidence. This reduces independent raw-artifact
reproducibility and is corrected in Phase 0.

Environment: Apple M2, eight logical CPUs, 8 GiB RAM, producer/seed/witness on
one host, loopback networking, disposable private PoB genesis, simple KOIN
transfers, independent pre-funded test payers, 90% producer resource ceiling,
and a private block compute limit of 100,000,000. No public-network or mainnet
transactions were sent.

> **Amendment A1 (2026-07-12 TPS review).** Two baseline validity risks must be
> closed in Phase 0. First, the load generator is a single Python process; the
> GIL and per-request JSON serialization can cap offered load before the node
> saturates, so admission ceilings such as the 451 TPS figure are bounded by
> the client as well as the node. Phase 0 must demonstrate generator headroom
> (multi-process or compiled generator, recorded client CPU) before any
> admission number is treated as a node limit. Second, the 8 GiB host is
> memory-constrained for a workload that will later add MVCC version storage;
> Phase 0 must record memory pressure, swap activity, and RocksDB block-cache
> hit rates so later phases are not compared against a silently paging
> baseline.

### 3.2 Measured Results

| Measurement | Result | Meaning | Not proven |
| --- | ---: | --- | --- |
| Best admission run | 451.749 TPS | 1,000 successful JSON-RPC admissions | Block inclusion or finality |
| Private block-flow window | about 180 TPS | 1,000 transfers in 11 blocks over 5.55 s | Mainnet cadence or WAN validation |
| Witness detection | about 49 TPS | Witness caught up while private producer created many blocks | Steady-state witness or finality |
| Compute per transfer | about 820,225 units | 820,224,836 total / 1,000 | Other contracts/workloads |
| Network per transfer | about 315 bytes | Canonical transaction resource usage | Propagation overhead |
| Disk per transfer | 35 bytes | State resource accounting | Physical write amplification |
| Maximum observed block | 100 transfers | Under the private producer configuration | Protocol maximum |

The default four-session RPC configuration rejected 234 of 500 attempts. With
64 sessions and a simultaneously changed pending limit, subsequent 500- and
1,000-transaction workloads were fully admitted. Because both settings changed
together, causality must be isolated in the next benchmark.

### 3.3 Derived Capacity Under the Private Resource Configuration

For the measured transfer cost, the private block configuration implies:

```text
producer compute budget = 100,000,000 * 0.90 = 90,000,000 units/block
transfers/block          = floor(90,000,000 / 820,225) = 109
at a 3 s target interval = 109 / 3 = 36.3 transfers/s
```

The observed maximum of 100 is consistent with compute being the first block
resource limit. The 36.3 value is a derived resource-model result, not measured
mainnet TPS. It also shows why faster admission or execution alone cannot raise
sustainable throughput when the block resource budget is unchanged.

Koinos documentation defines three resources (compute, network, disk) and a
three-second mainnet target interval. Current mainnet limits must be captured
from `chain.get_resource_limits` at benchmark time rather than copied into this
plan as a timeless constant.

## 4. Current Legacy Architecture

### 4.1 Reference Repositories and Commits

The audit used the following upstream commit pins. These pins make the findings
reproducible and avoid treating local build artifacts as source evidence.

| Service | Repository | Audited upstream commit |
| --- | --- | --- |
| Chain | `koinos/koinos-chain` | `0ae99eced8b585c4145424e9c2a28f667796cc66` |
| Mempool | `koinos/koinos-mempool` | `2aa415fc75fd40038509f06e66688ab505cf8df8` |
| Block producer | `koinos/koinos-block-producer` | `8896d7aabbe9e0d154a5f7860920e95d17fd8cb4` |
| P2P | `koinos/koinos-p2p` | `e2267ba230960b5e4100c16ad84c42cfc12eec4b` |
| Block store | `koinos/koinos-block-store` | `f195441ff2eaa757645a7997e96957c91bf930ac` |
| JSON-RPC | `koinos/koinos-jsonrpc` | `2c9433c67f2f60c920525a6c4bd3e15b0b51d94a` |
| Protocol definitions | `koinos/koinos-proto` | `1221179aac1416a5aec7ecbd5d5cfd0bd1ab76af` |

### 4.2 Transaction Path

```text
HTTP JSON-RPC
  -> Go JSON/protobuf reflection and wrapper construction
  -> RabbitMQ RPC to chain
  -> chain wrapper parse and dispatch
  -> full transaction validation and speculative receipt
  -> RabbitMQ RPCs to mempool for resources/nonces/counts
  -> chain transaction-accepted broadcast
  -> mempool parse, fork-aware insertion and broadcast
  -> P2P parse and gossip
  -> producer AMQP queries and sequential candidate selection
  -> chain propose_block AMQP RPC
  -> complete sequential block re-execution
  -> state delta, receipt list and state Merkle root
  -> block-accepted RabbitMQ fan-out
  -> block store, mempool, P2P and index consumers
  -> peer validation, fork choice and LIB progression
```

Important code entry points include:

- `koinos-jsonrpc/internal/jsonrpc.go::HandleRequest`, which dynamically
  converts JSON and calls the AMQP client;
- `koinos-chain/src/koinos_chain.cpp`, whose request handler parses service
  wrappers, dispatches the controller, and serializes responses;
- `koinos-chain/src/koinos/chain/controller.cpp::submit_transaction` and
  `::apply_block`;
- `koinos-chain/src/koinos/chain/system_calls.cpp::apply_block` and
  `::apply_transaction`;
- `koinos-mempool/src/koinos_mempool.cpp` and
  `src/koinos/mempool/mempool.cpp::add_pending_transaction`;
- `koinos-block-producer/src/koinos/block_production/block_producer.cpp`;
- `koinos-p2p/internal/node/node.go`; and
- the block-store AMQP request and broadcast handlers.

Commit-pinned source references:

- [legacy JSON-RPC request dispatch](https://github.com/koinos/koinos-jsonrpc/blob/2c9433c67f2f60c920525a6c4bd3e15b0b51d94a/internal/jsonrpc.go);
- [legacy chain service dispatch](https://github.com/koinos/koinos-chain/blob/0ae99eced8b585c4145424e9c2a28f667796cc66/src/koinos_chain.cpp),
  [controller](https://github.com/koinos/koinos-chain/blob/0ae99eced8b585c4145424e9c2a28f667796cc66/src/koinos/chain/controller.cpp),
  and [system calls](https://github.com/koinos/koinos-chain/blob/0ae99eced8b585c4145424e9c2a28f667796cc66/src/koinos/chain/system_calls.cpp);
- [legacy mempool service](https://github.com/koinos/koinos-mempool/blob/2aa415fc75fd40038509f06e66688ab505cf8df8/src/koinos_mempool.cpp)
  and [mempool implementation](https://github.com/koinos/koinos-mempool/blob/2aa415fc75fd40038509f06e66688ab505cf8df8/src/koinos/mempool/mempool.cpp);
- [legacy block producer](https://github.com/koinos/koinos-block-producer/blob/8896d7aabbe9e0d154a5f7860920e95d17fd8cb4/src/koinos/block_production/block_producer.cpp); and
- [legacy P2P node](https://github.com/koinos/koinos-p2p/blob/e2267ba230960b5e4100c16ad84c42cfc12eec4b/internal/node/node.go).

### 4.3 Confirmed Bottlenecks and Required Instrumentation

| Region | Evidence from code | Consequence | First intervention |
| --- | --- | --- | --- |
| JSON/protobuf conversion | Dynamic reflection and wrapper serialization per RPC | CPU/allocation overhead | Generated fast path, persistent connections, batches |
| RabbitMQ request path | Chain repeatedly calls mempool over AMQP | Copies, scheduling, queues, latency variance | Batched RPC and optional co-located hot core |
| Admission execution | Chain applies the full transaction before acceptance | Work is repeated during proposal/validation | Cache safe preprocessing; later OCC |
| Mempool insertion | Unique state DB lock while updating relevant fork nodes | Independent transactions serialize | Sharded metadata and staged fork update |
| Producer fill | Sequential pending scan and resource accounting | Single producer-core ceiling | Snapshot plus parallel preclassification |
| Block execution | `apply_block` iterates canonical transactions | Main execution ceiling | Deterministic speculative engine |
| Persistence/Merkle | Sequential delta squash, sort/hash and write | Commit-tail latency | Parallel prepare, canonical merge, batched write |
| Broadcast consumers | Separate services parse the same payload | Multiplied CPU/copy cost | Versioned batch broadcasts |

The legacy implementation remains essential: its sequential behavior is the
independent compatibility oracle. The initial plan does not remove RabbitMQ or
merge every service. It introduces an optional high-performance deployment
profile that keeps the same external service APIs and messages while allowing
chain, mempool, and producer hot-path adapters to share a process or use
versioned batches. A normal microservice deployment must remain supported.

## 5. Current Teleno Architecture

### 5.1 Transaction Path

Teleno co-locates node components, removes most process boundaries, and uses
direct chain adapters. The current hot path is nevertheless still shaped by
the legacy RPC interfaces:

```text
JSON-RPC/gRPC
  -> direct ChainAdapter
  -> controller::submit_transaction
  -> state DB shared lock and anonymous state
  -> repeated serialized compatibility calls to in-process mempool
  -> complete transaction execution and receipt
  -> synchronous EventBus transaction-accepted handlers
  -> mempool insertion and P2P gossip
  -> producer pending scan
  -> controller::propose_block
  -> complete sequential block execution
  -> state Merkle, finalization, RocksDB commit
  -> synchronous block-accepted handlers and P2P relay
```

### 5.2 Exact Hot-Path Evidence

| Component | File/function at reference commit | Finding |
| --- | --- | --- |
| JSON-RPC | `src/jsonrpc/jsonrpc_server.cpp::dispatch_chain` | JSON/protobuf conversion, then direct chain call |
| Adapter | `src/main.cpp::ChainAdapter::submit_transaction` | Direct controller invocation |
| Compatibility RPC | `src/core/monolith_rpc_client.hpp::rpc/broadcast` | In-process payload is still parsed/serialized |
| Events | `src/core/event_bus.hpp` | Boost signals dispatch synchronously |
| Admission | `src/koinos/chain/controller.cpp::submit_transaction` | Mempool queries plus full apply before acceptance |
| Mempool | `src/koinos/mempool/mempool.cpp::add_pending_transaction` | Global unique DB lock across relevant fork nodes |
| Producer | `src/block_production/block_producer.cpp::fill_block` | Sequential candidate scan and proposal retry |
| Canonical execution | `src/koinos/chain/system_calls.cpp::apply_block/apply_transaction` | Sequential transactions and operations |
| State delta | `src/koinos/state_db/state_delta.cpp::squash/commit/merkle_root` | Canonical merge, key sort/hash, sync write |
| VM cache | `src/koinos/vm_manager/fizzy/module_cache.cpp` | Mutex-protected LRU, only 32 modules |
| VM instantiation | `src/koinos/vm_manager/fizzy/fizzy_vm_backend.cpp` | New runner/instance/context per execution |
| P2P receive | `src/p2p/p2p_node.cpp::on_gossip_*` | Gossip callbacks submit synchronously; global seen lock |
| Indexing | `src/transaction_store`, `src/account_history`, `src/contract_meta_store` | Synchronous block fan-out and per-item writes |

The controller/system-call and mempool code is materially shared with legacy,
which is valuable for compatibility but also preserves legacy synchronization
assumptions. Teleno currently uses one effective P2P I/O thread; chain, RPC, and
peer-sync jobs have separate thread settings, but adding threads cannot bypass
the unique locks and sequential state transition.

All Teleno findings above are pinned to commit
`acecdaeb72395b4e8e6997bc649eafb2048f2960`. Direct source references include
[JSON-RPC dispatch](https://github.com/koinos/teleno/blob/acecdaeb72395b4e8e6997bc649eafb2048f2960/src/jsonrpc/jsonrpc_server.cpp),
[monolith compatibility RPC](https://github.com/koinos/teleno/blob/acecdaeb72395b4e8e6997bc649eafb2048f2960/src/core/monolith_rpc_client.hpp),
[chain controller](https://github.com/koinos/teleno/blob/acecdaeb72395b4e8e6997bc649eafb2048f2960/src/koinos/chain/controller.cpp),
[system calls](https://github.com/koinos/teleno/blob/acecdaeb72395b4e8e6997bc649eafb2048f2960/src/koinos/chain/system_calls.cpp),
[mempool](https://github.com/koinos/teleno/blob/acecdaeb72395b4e8e6997bc649eafb2048f2960/src/koinos/mempool/mempool.cpp),
[state delta](https://github.com/koinos/teleno/blob/acecdaeb72395b4e8e6997bc649eafb2048f2960/src/koinos/state_db/state_delta.cpp),
[Fizzy backend](https://github.com/koinos/teleno/blob/acecdaeb72395b4e8e6997bc649eafb2048f2960/src/koinos/vm_manager/fizzy/fizzy_vm_backend.cpp),
and [P2P node](https://github.com/koinos/teleno/blob/acecdaeb72395b4e8e6997bc649eafb2048f2960/src/p2p/p2p_node.cpp).

> **Amendment A11 (2026-07-12 TPS review, pass 2).** The hot-path evidence
> above omits a read-side cost: state reads traverse the per-block
> `state_delta` chain from the queried node back toward the irreversible
> root, so point and range lookups pay up to one map probe per non-squashed
> ancestor block — roughly the head-to-irreversible distance in the worst
> case — and larger blocks deepen every delta in that chain. OCC will add an
> MVCC overlay on top of the same stack. Phase 0 must instrument the
> read-depth distribution and per-read latency as a function of delta-chain
> depth, and Phase 2 should evaluate a flattened read snapshot or memoized
> lookup structure at fork points that leaves squash/commit/reorg semantics
> untouched.

### 5.3 Consensus-Visible Serial Semantics

The following must remain identical in the compatible phases:

- transaction order in the block;
- transaction failure/success and reverted state;
- payer resolution, RC charge, nonce verification and nonce update;
- nested call behavior and system-call resolution at each call point;
- ordered operation receipts, transaction receipts, events and logs;
- block resource consumption and failure indices;
- final state objects and state Merkle root;
- block ID, transaction Merkle root and previous-state root;
- fork application, rollback and irreversible state.

Parallel work is safe only if it is observationally equivalent to the current
sequential `apply_block` loop.

## 6. Quantitative Performance Model

### 6.1 Capacity Equation

For workload `w` and node profile `h`:

```text
sustainable_tps(w, h) = min(
  admission_capacity,
  execution_capacity,
  block_resource_capacity,
  propagation_capacity,
  validator_replay_capacity,
  finality_capacity)
```

Where:

```text
block_resource_capacity = min(
  compute_budget / mean_compute_per_tx,
  network_budget / mean_canonical_bytes_per_tx,
  disk_budget / mean_state_bytes_per_tx) / target_block_interval
```

`finality_capacity` means the highest offered rate at which the configured
quorum of heterogeneous observers remains caught up and the p95 time to LIB
stays inside the service objective. Finality is not inferred merely from a
producer returning success.

### 6.2 Evidence Labels

- **Measured**: result from retained benchmark artifacts on named hardware.
- **Derived**: arithmetic using measured inputs and explicit configuration.
- **Modeled**: prediction with stated assumptions, requiring validation.
- **Target**: acceptance threshold for a future phase.
- **Speculation**: architectural possibility with insufficient evidence; it
  cannot justify activation.

### 6.3 Parallel Speedup Model

For parallel fraction `p`, workers `n`, scheduling overhead `o`, conflict rate
`c`, and replay amplification `r(c)`:

```text
raw_speedup       = 1 / ((1 - p) + p / n)
effective_speedup = raw_speedup * (1 - o) / r(c)
replay_amplifier  = executed_attempts / committed_transactions
```

Illustrative eight-worker values before overhead/replay are 2.5x at `p=0.75`,
3.33x at `p=0.80`, and 4.71x at `p=0.90`. These numbers set expectations; only
the benchmark matrix can determine Koinos's actual `p`, `o`, and `r`.

> **Amendment A9 (2026-07-12 TPS review, pass 2).** Arithmetic correction: at
> `n = 8`, `p = 0.75` gives `1 / (0.25 + 0.75/8) = 2.91x`, not 2.5x. A 2.5x
> raw speedup corresponds to `p ≈ 0.69` at eight workers. The 3.33x and 4.71x
> values are correct. The executive-summary range of 2.5-5x remains reasonable
> (it spans roughly `p = 0.69` to `p = 0.90` before overhead), but the
> incorrect illustrative value must not be propagated into gate math.

### 6.4 Workload Model

| Workload | Expected conflicts | Likely first limit | Compatible parallel potential |
| --- | --- | --- | --- |
| Independent simple transfers | Low if payer and recipients differ | RC/system contracts, compute budget | High after shared system-state handling |
| One contended payer | Nonce, payer RC, balance | Canonical account serialization | Near 1x unless transactions are chained safely |
| Independent contract calls | Depends on object spaces | WASM/cache/state reads | Medium to high |
| DeFi hot contract | Shared pools, price state, events | Write conflicts and retries | Low without contract redesign |
| Mixed realistic | Mixed | Execution, Merkle, propagation | Moderate |
| Adversarial conflicts | Intentionally high | Replay/validation CPU | Must degrade to bounded sequential work |

Even apparently independent KOIN transfers may touch shared system-contract or
resource-market state. The trace, not the operation name, determines conflict.

### 6.5 Design Targets, Not Claims

Phase gates use the following initial targets:

- admission: no rejection below the proven sustainable inclusion rate; p99
  bounded under 2x overload through backpressure;
- compatible pipeline: at least 2x the 451-TPS local admission baseline on the
  same hardware and workload, without changing block results;
- independent execution: at least 3x sequential replay on an eight-core test
  host and at least 50% efficiency from 4 to 8 workers;
- mixed execution: at least 2x sequential replay;
- adversarial execution: no more than 1.25x CPU amplification versus an
  explicitly selected sequential mode after the fallback threshold fires;
- propagation: p99 full-block receive plus validation below 50% of the target
  interval on the slowest supported validator profile;
- finality: zero sustained observer lag and no statistically significant fork
  or LIB regression at the target load.

Targets must be revised from Phase 0 data; missing a target is evidence for a
design review, not permission to mislabel a partial metric.

> **Amendment A2 (2026-07-12 TPS review).** The "2x the 451-TPS local admission
> baseline" target must be restated against the Phase 0 re-measured baseline
> obtained with proven generator headroom (Amendment A1). If the corrected
> baseline differs materially from 451 TPS, the 2x multiplier applies to the
> corrected number; carrying the historical constant forward would misstate
> the Phase 1 gate.

> **Amendment A10 (2026-07-12 TPS review, pass 2).** The target list omits
> initial sync/catch-up capacity, historically the first decentralization
> casualty of high-TPS chains. The private benchmark already hints at the
> problem: the witness-detection figure of about 49 TPS against about 180 TPS
> of private block flow suggests catch-up replay can lag sustained production,
> and a fresh node that replays slower than the live inclusion rate never
> joins the network. Add a target: a baseline-profile node starting from an
> empty state database must sync/replay at no less than five times the proven
> sustained inclusion rate. Record this as a Section 16 metric and make the
> catch-up margin an explicit Phase 8 gate before any governed budget
> increase.

### 6.6 Worked Scenarios

The following calculations deliberately use the private benchmark's
100,000,000 compute-unit block limit, 90% producer ceiling, approximately
820,225 compute units and 315 canonical bytes per simple transfer. They do not
describe current mainnet capacity.

| Scenario at 3 s cadence | Effective compute budget | Compute-limited transfers/s | Raw transaction bytes/block | Interpretation |
| --- | ---: | ---: | ---: | --- |
| Private benchmark budget | 90 million | 36.3 | about 34 KiB at 109 tx | Faster execution cannot exceed this resource gate |
| 4x effective budget | 360 million | 146.3 | about 135 KiB at 439 tx | Requires replay/propagation proof and governed change |
| 8x effective budget | 720 million | 292.6 | about 270 KiB at 878 tx | Existing 451 admission becomes another visible gate |
| 10,000 transfers/s target | 24.607 billion required | 10,000 by definition | about 9.45 MB | About 273x the 90m effective budget; physical execution and WAN replay must independently support it |
| 100,000 transfers/s target | 246.068 billion required | 100,000 by definition | about 94.5 MB | Not a credible near-term compatible target on baseline validators |

Compute units are deterministic accounting units, not CPU cycles. Multiplying
the budget does not make execution faster; lowering a resource charge without
proving lower physical cost would only weaken protection. Conversely, an
optimized implementation can provide physical headroom that a later governed
budget change exposes safely.

> **Amendment A13 (2026-07-12 TPS review, pass 2).** The 10,000 TPS boundary
> discussion must also state the storage and bandwidth consequences, because
> they bind independently of compute: at 315 canonical bytes per transfer,
> 10,000 TPS is about 9.45 MB per block and roughly 272 GB per day of block
> data before receipts and indexes, plus about 30 GB per day of accounted
> state growth at 35 disk bytes per transfer. Archive cost, initial-sync time
> (Amendment A10) and state-database growth therefore belong in any credible
> high-TPS narrative alongside execution, propagation and replay.

Initial normalized execution hypotheses for an eight-worker OCC engine are:

| Workload | Modeled speedup over optimized serial | Confidence before Phase 0/4 | Reason |
| --- | ---: | --- | --- |
| Independent transfers | 2.5-5x | Medium-low | High parallel fraction, but shared RC/system state may conflict |
| Contended payer | 0.8-1.1x | Medium | Nonce, RC and balance form a serial dependency chain |
| Independent contracts | 2-4x | Low | Depends on WASM share, cache and object-key independence |
| DeFi hot contract | 0.8-1.5x | Medium-low | Shared pool/price writes dominate |
| Mixed realistic | 1.5-3x | Low | Must be derived from an observed application mix |
| Adversarial conflict | at least 0.8x target | Design target | Bounded fallback limits overhead to 1.25x serial CPU |

These ranges are planning inputs, not promised results. A credible end-to-end
claim must substitute measured admission, execution, block-resource,
propagation, replay and finality values into the minimum equation. For example,
even a modeled 900 TPS executor and 900 TPS admission path yield only about 146
TPS under the illustrative 4x compute budget before propagation and finality
are considered.

## 7. External Architecture Research

### 7.1 Solana/Agave

Solana's transaction pipeline separates receive, signature verification,
sanitization, fee/age checks, account loading, execution, and commit. Sealevel
uses declared account access and locks to schedule non-conflicting
transactions. A cost model includes signatures, writable account locks,
loaded data, and compute; per-account limits prevent a single hot account from
consuming an entire block. Programs are cached, but instructions within one
transaction remain ordered. Leader knowledge enables Gulf Stream forwarding,
and Turbine distributes erasure-coded shreds through a tree. Solana also
overlaps leader processing, PoH recording, replay and vote processing; its
Tower-style voting/finality path is consensus-specific and is not an
implementation-only optimization for PoB.

Adaptable to Koinos now:

- stage separation, bounded queues and backpressure;
- parallel batch signature/hash verification;
- program/module cache improvement;
- scheduler cost estimation and hot-key metrics;
- erasure-coded or chunked block propagation after measurement.

Not directly portable:

- mandatory account lists conflict with Koinos contracts' dynamic object-space
  and nested-call access unless transaction validity changes;
- deterministic future-leader forwarding assumes leader knowledge that current
  PoB competition does not provide in the same form;
- Solana's compute/account limits and fee market cannot replace Mana/RC without
  changing Koinos economics and validity.

### 7.2 Firedancer

Firedancer demonstrates that a second, independently implemented validator can
remain protocol-compatible while radically changing internal architecture. It
uses dedicated tiles for networking, QUIC, verification, deduplication,
packing, execution, shreds, storage and other jobs. Tiles communicate through
shared-memory queues; high-end profiles assign job ownership to specific cores
and NUMA nodes. Its Linux networking path can use AF_XDP and zero-copy UMEM.
The Frankendancer transition also demonstrates incremental deployment: replace
and validate subsystems while interoperating with the established client,
rather than requiring one all-at-once rewrite.

Adaptable:

- fixed-stage topology, bounded rings, ownership and per-stage metrics;
- independent compatible implementation and differential replay;
- batch verification and low-copy buffers;
- configurable core/NUMA affinity for an optional Linux performance profile.

Constraints:

- kernel bypass, locked huge pages and exclusive cores are unsuitable as the
  default Koinos One/macOS profile and increase validator centralization;
- Koinos must first optimize ordinary sockets and copies; AF_XDP is a later
  optional backend, not a consensus feature.

### 7.3 Aptos Block-STM

Block-STM executes a block's preset canonical transaction order optimistically.
Transactions discover read/write sets dynamically against multiversion state.
Read validation detects when an earlier transaction changed a dependency;
invalidated transactions get a new incarnation and re-execute. Commit remains
equivalent to serial execution in the original order.

This is the strongest match for Koinos because it does not require contracts to
declare all dynamic accesses. Koinos-specific work is still substantial:

- every object read, range iterator, system-contract access and nested call
  must participate in version tracking;
- events, receipts, RC and nonces must remain private to an incarnation until
  canonical commit;
- range reads (`get_next_object`/`get_prev_object`) need phantom protection;
- replay must be bounded and deterministic under contention;
- state-root computation must merge the final canonical versions only.

### 7.4 Sei

Sei's optimistic concurrency approach similarly executes dynamically,
discovers conflicts, and re-executes to produce sequentially equivalent state.
Its documentation also emphasizes contract design that avoids shared writes,
and separates current features from proposed asynchronous/pipelined work.

Adaptable lessons are conflict telemetry, sequential fallback, and parallel-
friendly contract guidance. Async consensus/execution overlap must not allow a
Koinos producer to publish a block before the state root and receipts are fully
known under current validity rules.

### 7.5 Narwhal, Bullshark, Mysticeti, Sui, and Avalanche

Narwhal separates reliable transaction dissemination from ordering using a
DAG. Bullshark orders a dissemination DAG with BFT consensus. Mysticeti is a
low-latency DAG consensus protocol. Sui uses object ownership/dependencies to
avoid global ordering for many owned-object transactions, but shared objects
still require agreement. Avalanche's later X-Chain linearization is a useful
warning: a DAG without canonical ordering complicates shared state, state sync,
and integrations.

Koinos can borrow a dependency DAG inside one deterministic executor or,
later, a dissemination DAG before PoB ordering. Replacing PoB/fork choice with
Bullshark/Mysticeti or replacing Koinos object spaces with Sui objects changes
chain semantics and is outside this goal.

### 7.6 Other Relevant Techniques

- Software transactional memory contributes versioned reads, validation and
  aborts, but Koinos requires deterministic scheduling/fallback rather than
  timing-dependent commits.
- Actor/object partitioning is useful for caches and contract design guidance,
  but cannot assume a contract owns all state it may access.
- Sharded mempools and databases help only when fork, payer, range-read and
  canonical-commit invariants are explicit.
- Batch cryptographic APIs, io_uring/QUIC, erasure coding and zero-copy buffers
  address admission/propagation, not contract conflict.
- A parallel sparse-Merkle builder can prepare leaf hashes in parallel, but
  must reproduce the exact existing Koinos root algorithm and key ordering.

## 8. Candidate Design Comparison

| Candidate | Description | Compatibility | Independent-work gain | Hot-state behavior | Decision |
| --- | --- | --- | ---: | --- | --- |
| A | Optimized sequential semantics plus Firedancer-style pipeline | Implementation-only | Medium | Predictable 1x execution | Implement first |
| B | Dynamic Block-STM/OCC, canonical serial-equivalent commit | Implementation-only if byte-identical | High | Replay then fallback | Core recommendation |
| C | Mandatory declared access and deterministic locking | Hard fork if enforced | Very high when accurate | Explicit serialization | Contingency only |
| D | Optional hints plus OCC and sequential fallback | Optional API extension | High | Bounded degradation | Recommended mature form |
| E | Internal dependency or dissemination DAG; linear blocks | Internal DAG compatible; network DAG coordinated | Medium/network-dependent | Does not solve hot execution | Conditional later phase |
| F | Full DAG consensus or object-centric chain | Hard fork/new-chain territory | Potentially high | Different semantics | Reject |

### 8.1 Semantic Handling by Candidate

| Concern | A | B/D | C | E | F |
| --- | --- | --- | --- | --- | --- |
| Dynamic/nested calls | Existing serial behavior | Trace actual reads/writes; revalidate | Must declare conservatively or fail | Unchanged | New model |
| System-contract state | Existing serial behavior | Version as normal state; special hot-key handling | Must be declared | Unchanged | Likely redesigned |
| Events/receipts | Existing order | Buffer per incarnation; emit at canonical commit | Canonical lock order | Unchanged | Potentially reordered |
| Failure/reversion | Existing behavior | Discard private writes/events; replay | Existing semantics if declared | Unchanged | Protocol-specific |
| RC/payer/nonce | Serial | MVCC plus canonical validation; no speculative charge escape | Explicit locks needed | Unchanged | Likely redesigned |
| Merkle root | Existing | Canonical merged write set only | Existing if execution same | Unchanged | Different state |
| Fork/reorg | Existing | Per-fork version domain; discard abandoned work | Existing if declared | Dissemination cache pruned | Different fork choice |
| Crash recovery | Existing DB | Persist only canonical committed block; speculation disposable | Existing | DAG cache reconstructible | New recovery protocol |
| Hot contracts | Serial | Detect, throttle and fallback | Serialize by declaration | No execution benefit | Model-dependent |

## 9. DAG Feasibility Analysis

| DAG use | Implementation detail? | Validity/semantics | Upgrade required | Recommendation |
| --- | --- | --- | --- | --- |
| 1. Internal transaction-dependency DAG | Yes, if derived from actual accesses and canonical order | No change if serial-equivalent | None | Use as an OCC scheduler optimization |
| 2. Transaction-dissemination DAG before ordering | No, it changes P2P protocol but need not change blocks | No block/fork change if PoB still orders linear blocks | Coordinated dual-stack binary deployment | Prototype only after propagation is limiting |
| 3. Block-production DAG resolved to one linear chain | Partly consensus-visible | Changes proposal/fork behavior and likely validity timing | Coordinated hard fork | Reject unless a later consensus research program proves need |
| 4. Full DAG consensus | No | Replaces PoB ordering/finality/fork choice | Hard fork; effectively different consensus chain | Reject for this goal |
| 5. Sui-like object-centric execution DAG | No | Changes state ownership, transaction dependencies and shared-object rules | Hard fork/state-model migration; effectively new blockchain | Reject |

An internal dependency DAG is useful because edges can mean “transaction `j`
must observe the committed version produced by earlier transaction `i`.” It is
not a consensus DAG and is never serialized into a block. A dissemination DAG
can reduce duplicated data transfer and decouple availability from order, but
it adds certificate/storage/DoS complexity and does not make a hot contract
parallel. Neither justifies changing PoB before ordinary chunked propagation,
compression, batching, and topology improvements are measured.

## 10. Protocol Compatibility and Hard-Fork Matrix

Each row has exactly one classification:

1. implementation-only and protocol-compatible;
2. compatible after an optional client/API extension;
3. coordinated binary upgrade without changed consensus results;
4. forkless governed system-contract upgrade;
5. consensus-breaking hard fork; or
6. effectively a new blockchain and outside the goal.

| Proposed change | Class | Why |
| --- | ---: | --- |
| Metrics, traces, benchmark harness and replay corpus | 1 | No network or validity effect |
| Persistent RPC connections, bounded queues and backpressure | 1 | Existing request/response semantics |
| Batch signature/hash verification | 1 | Same algorithms and verdicts |
| Larger/sharded WASM module cache | 1 | Cache only; same VM result |
| In-process typed Teleno adapters replacing serialized compatibility calls | 1 | Same component contract and response |
| Mempool metadata sharding with canonical fork locks | 1 | Mempool is non-consensus if accepted set semantics remain |
| Off-critical-path optional indexes | 1 | Consensus state commit remains synchronous |
| Parallel leaf preparation with identical Merkle root | 1 | Byte-identical root required |
| Teleno OCC with canonical serial-equivalent commit | 1 | Produced block remains valid to sequential legacy |
| Internal dependency DAG derived from execution trace | 1 | Ephemeral scheduler state only |
| Optional access hints ignored for validity | 2 | Old clients omit them; nodes may distrust them |
| Versioned batch AMQP messages with dual legacy support | 3 | Fleet-wide feature deployment, same results |
| New P2P batch/chunk/erasure-coded propagation with dual stack | 3 | Peers need support, but blocks/fork choice remain unchanged |
| Optional Linux AF_XDP/NUMA performance backend | 3 | Operational binary capability; ordinary backend remains |
| Governed block resource-market parameter adjustment | 4 | Koinos system-contract governance path; no hidden binary change |
| Governed PoB parameter adjustment within existing contract semantics | 4 | Existing PoB system-contract mechanism, subject to safety proof |
| Mandatory/enforced read/write declarations | 5 | Adds transaction/block validity conditions |
| Changing nonce, RC charge, payer, receipt or event order | 5 | Existing sequential validity changes |
| Asynchronous block publication before final root/receipts | 5 | Current block validation contract changes |
| PoB fork-choice/finality replaced with block DAG consensus | 5 | Preserves history only through coordinated consensus fork |
| Object-centric state replacing Koinos object spaces | 6 | Requires new transaction/state model and application migration |
| New genesis, history reset or incompatible chain identity | 6 | Explicitly forbidden |

Class 1 still requires extensive differential proof. “Implementation-only” is
not a license to produce a different receipt or root. Class 4 may be forkless
at the protocol mechanism level but still requires governance approval,
operational coordination, and validator-capacity evidence. If a system-contract
change depends on a new native thunk or changes the effective interpretation of
historical transaction validity, reclassify it as class 3 or 5 after a formal
consensus review.

### 10.1 Exact Consensus-Upgrade Boundary

No hard fork is needed while all of the following remain true:

1. transactions valid before remain valid and transactions invalid before
   remain invalid;
2. a block built by Teleno is accepted by the pinned legacy sequential chain;
3. both clients compute identical block/transaction receipts, event order,
   resource usage, failed-transaction indices, and state Merkle root;
4. transaction/block wire formats and fork-choice rules are unchanged; and
5. old clients can participate without interpreting optional hints.

A coordinated consensus upgrade is necessary at the first proposal that
violates any of these conditions. The likely boundary is enforcing access
lists, changing canonical commit order, altering nonce/RC conflicts, publishing
partial/asynchronous blocks, or changing PoB/fork choice. Merely running work in
parallel is not that boundary.

## 11. Recommended Target Architecture

```text
                         existing JSON-RPC / gRPC APIs
                                     |
                         bounded admission gateway
                                     |
              +----------------------+----------------------+
              | decode/sanitize | batch verify | dedup/cache |
              +----------------------+----------------------+
                                     |
                    sharded fork-aware mempool metadata
                                     |
                  deterministic canonical block planner
                                     |
          +--------------------------+--------------------------+
          | OCC worker 0 | OCC worker 1 | ... | OCC worker N-1  |
          +--------------------------+--------------------------+
                                     |
                MVCC overlay + read/range/write tracking
                                     |
               validate in canonical order; bounded replay
                                     |
                      deterministic serial fallback
                                     |
        canonical receipt/event/RC/nonce merge and Merkle builder
                                     |
                 atomic consensus-state RocksDB write batch
                    /                                \
          propagation pipeline             async optional indexes
                    |                                |
             legacy/Teleno peers            RPC/history consumers
```

### 11.1 Core Invariants

- Block order is fixed before speculative execution.
- A worker reads the newest visible version from a lower transaction index,
  otherwise the pre-block snapshot.
- Every read records `(object_space, key, observed_version)`; range reads also
  record bounds and a fingerprint/version capable of detecting phantoms.
- Writes, events, logs, receipts, resource counters, nonce updates and nested
  call state remain private to a transaction incarnation.
- Validation and final commit advance in block order.
- A changed dependency aborts the incarnation and reschedules it.
- Replay count, CPU time and conflicting-key cardinality are bounded. Crossing
  a deterministic local threshold switches the remaining suffix to sequential
  execution. The threshold affects performance only, never the result.
- Only the canonical final write set reaches the persistent state database.
- Crash recovery starts from the last atomic canonical commit; speculative
  state is disposable.
- An optional hint can accelerate initial scheduling but cannot suppress
  tracking, make a transaction valid, or alter the result.

### 11.2 Koinos-Specific Semantics

**Dynamic and nested calls.** The execution context instruments state system
calls below the contract boundary. A nested contract needs no declaration;
its actual accesses are added to the same incarnation trace.

**System contracts.** System-contract object reads and writes participate in
MVCC. Global resource-market objects may become hot; the scheduler should
measure them separately and may serialize only the affected transactions.

> **Amendment A3 (2026-07-12 TPS review).** Two scheduler refinements are
> added. (1) Hot global counters: resource-market utilization objects are
> read-modify-write on nearly every transaction and will serialize naive OCC.
> The scheduler should support a deterministic commutative-delta path for
> explicitly whitelisted hot keys: per-incarnation deltas are accumulated and
> folded in canonical transaction order at commit, and the fold must reproduce
> the byte-identical final value and receipts of sequential execution. Where
> executed logic actually branches on the read value (for example price
> recalculation), the commutative path is invalid and those transactions
> serialize; the differential corpus decides, not the operation name. This
> remains class 1 only under byte-identical results. (2) Same-payer chains:
> transactions from one payer are guaranteed nonce-dependent, so the planner
> should pin each payer chain to a single worker in nonce order, converting
> certain conflicts into scheduled dependencies and reserving speculation for
> cross-payer parallelism.

**RC and payer.** Payer resolution and RC checks run within the transaction's
snapshot. Canonical validation repeats any dependency-sensitive checks. A
speculative attempt cannot reserve or consume durable RC.

**Nonce and replay.** The payer/account nonce is a normal versioned conflict.
Transactions from one account form a natural chain. The mempool may recognize
the chain for scheduling, but canonical validation remains authoritative.

**Events and receipts.** Buffers are indexed by canonical transaction and
operation position. Aborted incarnation buffers are destroyed. Publishing
occurs only from the accepted incarnation in current order.

> **Amendment A12 (2026-07-12 TPS review, pass 2).** Event sequence numbers
> are a hidden serial dependency: the chain assigns block-scoped,
> monotonically increasing sequence values across all transactions' events,
> so a transaction's final sequence numbers depend on every earlier
> transaction's event count. During speculation, an incarnation must record
> events with only its local (transaction index, operation index,
> intra-transaction ordinal) position; global sequence values are assigned at
> canonical commit. Assigning them speculatively would either force
> artificial conflicts between otherwise independent transactions or silently
> diverge receipts.

**Failures.** A contract exception produces the same reverted state and
receipt as sequential execution. A conflict abort is internal and must never
appear as a transaction failure.

**Merkle root.** The final canonical delta is passed to the existing Merkle
algorithm. Parallel hashing is introduced only behind a differential proof
that every root is identical.

**Forks/reorganizations.** Each candidate block executes against its parent's
immutable snapshot and owns an isolated MVCC domain. Work for abandoned forks
is canceled; only existing fork-choice code selects canonical state.

## 12. Repository, Component, and Code Ownership Map

| Work area | Preferred repository/component | Primary code surfaces | Compatibility owner/test |
| --- | --- | --- | --- |
| Benchmark specification and runner | `koinos/teleno` | `docs/performance`, `scripts/benchmark-transaction-tps.py`, new replay tools | Teleno CI + legacy replay CI |
| Admission pipeline | `koinos/teleno` | `src/jsonrpc`, `src/grpc_server`, new `src/pipeline` | Existing RPC conformance |
| Typed in-process interfaces | `koinos/teleno` | `src/core/monolith_rpc_client.hpp`, `src/interfaces`, adapters in `src/main.cpp` | Serialized-adapter A/B tests |
| Mempool concurrency | Shared logic, developed first in Teleno | `src/koinos/mempool`, `src/mempool` | Legacy mempool behavior corpus |
| Canonical planner/producer | `koinos/teleno` | `src/block_production` | Legacy producer selection comparison |
| OCC scheduler and MVCC overlay | `koinos/teleno` | new `src/execution`, `src/koinos/state_db` integration | Legacy chain sequential oracle |
| Execution instrumentation | `koinos/teleno` shared Koinos framework code | `src/koinos/chain/system_calls.cpp`, execution context/state node | Receipt/root differential test |
| WASM runtime/cache | `koinos/teleno` | `src/koinos/vm_manager/fizzy` | VM conformance corpus |
| State/Merkle/persistence | `koinos/teleno` | `src/koinos/state_db`, RocksDB backend | Snapshot/reorg/crash tests |
| P2P propagation | `koinos/teleno` first; `koinos/koinos-p2p` for legacy | `src/p2p`; legacy `internal/node` | Mixed-version network test |
| Legacy JSON-RPC | `koinos/koinos-jsonrpc` | `internal/jsonrpc.go`, `cmd/koinos-jsonrpc` | API golden tests |
| Legacy chain | `koinos/koinos-chain` | service handler, controller/system calls | Consensus oracle |
| Legacy mempool | `koinos/koinos-mempool` | service handler and mempool library | Admission/mempool corpus |
| Legacy producer | `koinos/koinos-block-producer` | block production | Candidate block comparison |
| Legacy block store | `koinos/koinos-block-store` | AMQP handlers and storage | Block API conformance |
| Message schemas | `koinos/koinos-proto` | versioned RPC/broadcast messages | Old/new decoder matrix |
| Governed parameters | Koinos system contracts/governance repos | resource and PoB system contracts | Governance proposal + network simulation |
| SDK access hints | Koinos proto plus supported SDKs | optional extension fields/methods | Old client omission tests |

Shared code must not be copied indefinitely between Teleno and legacy. Where
feasible, consensus-critical fixtures and pure libraries should be versioned
once and consumed by both. Independent implementations of the final validation
path are still desirable; shared test vectors are not a substitute for client
diversity.

## 13. Detailed Implementation Phases

### Phase 0 — Comparable Evidence and Consensus Corpus

**Objective.** Establish reproducible legacy/Teleno baselines and a permanent
corpus against which every later optimization is judged.

**Repositories/components.** Teleno benchmark scripts/docs; legacy deployment
manifests; chain, producer, mempool and P2P metrics adapters; no consensus code
change.

**Code-level changes.** Extend the runner with persistent HTTP, offered/admitted/
included/irreversible clocks, per-stage telemetry import, block-resource
snapshots, repeat seeds, workload manifests and artifact hashing. Add a block
replay tool that emits canonical receipts, events, failed indices, resource
usage, state-object deltas and roots from both clients. Capture full Git hashes,
configuration, hardware, OS, storage and clean-tree status.

**Protocol impact.** Class 1; none.

**Legacy/Teleno impact.** Both run the same signed private-network workloads
and historical block corpus. Legacy is not modified beyond opt-in metrics.

**Prerequisites.** Isolated private network, sanitized offline-signed workload,
representative macOS and Linux hosts, WAN-emulation profile, retained artifacts.

**Test strategy.** Unit-test the load generator; dry-run chain-ID controls;
cross-check transaction IDs and block scans; replay the same snapshots twice;
verify artifact checksums and no secret fields.

**Required benchmark.** All workloads in Section 15 at one, four, eight, and
maximum safe concurrency; admission-only, broadcast, inclusion, independent
validation and LIB modes; at least five repetitions after warm-up.

**Security risks.** Accidental live-chain submission or secret capture. Keep
exact chain-ID confirmation, private-network-only policy and secret scanner.

**Rollback.** Remove metrics/runner changes; no state migration.

**Definition of done.** Two clients replay an agreed corpus identically; every
published number has raw JSON, environment manifest, logs/metrics and a clear
confirmation definition; the current 451/180 observations are either
reproduced or explicitly superseded.

**Approval.** Maintainer approval for code; no user/governance or mainnet
approval. Running any public-network benchmark requires a separate explicit
request and policy review.

### Phase 1 — Observable Bounded Pipeline

**Objective.** Remove non-consensus admission and scheduling bottlenecks before
parallel state execution.

**Repositories/components.** Teleno JSON-RPC/gRPC, P2P ingress, controller
prechecks, metrics; later legacy JSON-RPC worker and AMQP client.

**Code-level changes.** Introduce bounded queues for decode, sanitize,
signature/hash verification, deduplication and chain submission; persistent
connection support; batch cryptographic calls; explicit overload responses;
per-stage queue depth/latency/CPU; immutable transaction envelopes to avoid
copies. Do not parallelize state-sensitive checks yet.

> **Amendment A4 (2026-07-12 TPS review; revised in pass 2).** Koinos
> transactions are signed with secp256k1 ECDSA using public-key recovery;
> ed25519-style algebraic batch verification does not apply. In this plan
> "batch verification" must mean multi-core parallel verification over
> batches, reused secp256k1 contexts and precomputed tables, and a
> verification-verdict cache. Pass-2 correction: the transaction ID hashes
> the transaction header only and does not commit to the signature bytes, so
> a verdict cache keyed by ID alone would let a copy of a known-valid ID
> carrying altered or invalid signatures bypass verification. The cache key
> must be the pair (transaction ID, hash of the exact signature bytes). With
> that key, block validation may skip re-verifying signatures already
> verified during admission or gossip of the identical signed payload,
> removing one of the repeated-work items in Section 4.3 with no consensus
> effect.

**Protocol impact.** Class 1.

**Legacy/Teleno impact.** Same RPC responses and validation verdicts. Legacy
adopts generated fast paths/batching only after Teleno proves behavior.

**Prerequisites.** Phase 0 profiles identify the real saturation point and a
memory budget for every queue.

**Test strategy.** Golden request/response tests, malformed/adversarial input,
queue saturation, cancellation, shutdown, duplicate submission, signature
batch boundary, thread sanitizer and deterministic verdict comparison.

**Required benchmark.** Admission with and without broadcast; payload-size
matrix; valid/invalid signature mixes; 1x/2x overload soak; report p50/p95/p99,
queue depth, rejection reason and resident memory.

**Security risks.** Unbounded memory, unfair clients, verification bypass in a
batch, starvation. Enforce queue/tenant limits and individual verdict checks.

**Rollback.** Runtime flag selects the original synchronous ingress; queues
drain before shutdown.

**Definition of done.** At least 2x the same-host baseline admission target or
documented evidence that a later stage is now limiting; zero verdict drift;
bounded memory and graceful overload for 24 hours.

**Approval.** Maintainer release approval only; no separate user or governance
approval is required.

### Phase 2 — Teleno Low-Copy Hot Path, Mempool, VM, and Storage

**Objective.** Remove Teleno's inherited in-process serialization, global
mempool contention and synchronous non-consensus work.

**Repositories/components.** `src/core`, `src/interfaces`, `src/mempool`,
vendored mempool, VM manager, state DB, transaction/account/contract indexes.

**Code-level changes.** Add typed direct mempool/block-store operations while
retaining serialized conformance adapters; split mempool immutable snapshots
from fork-update commit; shard payer/transaction indexes with a documented lock
order; cache safe admission prechecks; enlarge/shard module cache and evaluate
safe VM instance pools; prepare RocksDB write batches; dispatch optional indexes
from a committed-block journal outside consensus latency.

> **Amendment A5 (2026-07-12 TPS review).** Physical WASM execution cost is the
> largest single lever behind the Section 6.6 capacity gap, and Fizzy is an
> interpreter. Promote open question 4 to an explicit Phase 2 workstream:
> profile Fizzy instantiation versus instruction execution after cache warm-up,
> and evaluate at least one protocol-equivalent compiled backend (tiered JIT or
> AOT) behind the existing VM interface. Hard constraint: compute-unit metering
> must remain bit-identical to Fizzy's deterministic accounting across the full
> VM conformance corpus; a backend that changes metering is class 5, not an
> optimization. The 32-entry mutex-protected module cache remains the cheap
> first win already noted.

> **Amendment A6 (2026-07-12 TPS review).** Add an explicit RocksDB commit-path
> item: measure WAL fsync policy against the required durability contract,
> instrument write stalls and compaction debt during soaks, evaluate prefix
> bloom filters for object-space iterators, and separate consensus state from
> rebuildable index data in distinct column families so index compaction can
> never stall canonical commit. Extend the Phase 2 definition of done with: no
> compaction- or stall-induced p99 canonical-commit latency excursion above the
> stage budget during the 24-hour soak.

**Protocol impact.** Class 1.

**Legacy/Teleno impact.** Teleno-only implementation first. Run typed and
serialized adapters in shadow comparison. Consensus state and required block
store durability remain synchronous.

**Prerequisites.** Phase 1 tracing, formal mempool invariants, index recovery
journal design, exact classification of consensus versus optional data.

**Test strategy.** Mempool fork/reorg/property tests; lock-order and race tests;
adapter golden tests; VM corpus; crash between consensus commit and optional
index update; index rebuild; RocksDB fault injection.

**Required benchmark.** Independent and same-payer admission, fork-heavy
mempool, module-cache hit/miss, block commit with each optional index enabled
and disabled, cold/warm restart, 24-hour mixed soak.

**Security risks.** Stale pending resources/nonces, deadlocks, index data loss,
unsafe reusable VM state. Favor immutable snapshots, generation counters and
rebuildable indexes; do not pool VM instances until memory reset is proven.

**Rollback.** Feature flags for typed adapters, sharding, VM pool and async
indexes; journal allows rebuild from canonical block store.

**Definition of done.** No global lock dominates independent admission; adapter
results are identical; optional indexes do not extend canonical commit; restart
and reorg invariants pass; performance gain is retained in sustained tests.

**Approval.** Maintainer approval only; no separate user or governance approval
is required.

### Phase 3 — Legacy High-Performance Compatibility Profile

**Objective.** Preserve an independent legacy validator while removing avoidable
RabbitMQ and serialization overhead for operators who need higher throughput.

**Repositories/components.** Legacy JSON-RPC, chain, mempool, producer, P2P,
block store, `koinos-proto`, deployment definitions.

**Code-level changes.** Add versioned batch RPC/broadcast envelopes, generated
JSON/protobuf fast paths, correlation batches, payload-reference/zero-copy
where process safety permits, and an optional co-located chain+mempool+producer
hot-core process behind unchanged external service endpoints. Retain the normal
multi-process profile and old messages during transition.

**Protocol impact.** Class 3 for fleet deployment; consensus results unchanged.

**Legacy/Teleno impact.** Mixed old/new message compatibility and same block
validation. Teleno must speak old and new envelopes during the rollout window.

**Prerequisites.** Phase 0 identifies IPC share; schema compatibility plan;
Phase 2 typed interfaces provide a reference without forcing code identity.

**Test strategy.** Old/new producer-consumer matrix, unknown-field handling,
RabbitMQ reconnect/redelivery/order tests, process crash/restart, co-located
versus distributed differential replay, rolling-upgrade test.

**Required benchmark.** Per-message CPU/copies/latency, batch sizes, queue
pressure, distributed versus co-located inclusion/replay, mixed-version WAN.

**Security risks.** Batch amplification, redelivery ambiguity, loss of process
isolation, version skew. Bound batch bytes/items, preserve message IDs and
idempotency, document the isolation trade-off.

**Rollback.** Negotiate old envelopes; disable co-location; roll services back
independently without state format change.

**Definition of done.** Sequential legacy remains a first-class deployable
validator; rolling upgrade succeeds; all profiles produce identical results;
IPC is no longer the dominant profiled limit in the high-performance profile.

**Approval.** Maintainer and operator approval for coordinated binary rollout;
no separate end-user or governance approval is required.

### Phase 4 — Deterministic OCC Prototype in Teleno

**Objective.** Prove that Koinos transactions can execute speculatively in
parallel while producing the exact sequential result.

**Repositories/components.** New Teleno execution scheduler/MVCC overlay;
chain execution context/system calls; state DB; test-only block replay command.

**Code-level changes.** Assign canonical transaction indices; implement
versioned point reads/writes, range-read phantom detection, per-incarnation
events/receipts/resources, dependency validation, deterministic rescheduling,
bounded replay and sequential suffix fallback. Start as replay-only/shadow mode;
it must not produce blocks or change persistent state.

**Protocol impact.** Class 1 if and only if differential results are identical.

**Legacy/Teleno impact.** Legacy executes sequentially and supplies expected
outputs. Teleno runs serial and OCC paths side by side on the same block.

**Prerequisites.** Phase 0 corpus; formal inventory of every state/range access,
system call, native thunk and execution side effect; immutable pre-block state.

**Test strategy.** Model-based MVCC tests; nested calls; system-contract
replacement during a block; failures/reversions; events/logs; payer/nonce/RC;
range phantoms; duplicate keys; forks/reorgs; random schedules; sanitizer and
fault injection; millions of generated blocks compared to serial.

**Required benchmark.** Worker scaling 1/2/4/8/16; every Section 15 workload;
conflict rate, attempts/commit, validation cost, fallback frequency, root-build
time and memory. Report execution-only separately from end to end.

**Security risks.** Hidden side effects, nondeterministic dependency ordering,
phantoms, replay DoS, RC double use, incorrect reverted events. The prototype
cannot leave shadow mode on any mismatch.

**Rollback.** Disable/delete the shadow executor; no persistent format change.

**Definition of done.** Zero mismatches across historical, generated,
adversarial and reorg corpora; at least 3x independent and 2x mixed execution
target on the reference eight-core profile; bounded degradation on hot state;
a reviewed proof of serial equivalence and access-completeness inventory.

**Approval.** Maintainer/security review. Explicit product-owner/user approval
is required before promoting the prototype from research/shadow status; no
governance approval is required while results remain identical.

### Phase 5 — Production OCC, Canonical Commit, and Legacy Oracle

**Objective.** Make OCC the optional Teleno block execution/production engine
without changing any consensus result.

**Repositories/components.** Teleno controller, producer, scheduler, state DB,
metrics/config; legacy chain in CI and canary topology.

**Code-level changes.** Integrate canonical block planner; schedule execution
from immutable parent state; commit accepted incarnations in order; compute the
existing root; make speculative data disposable; add startup recovery and
per-block fallback; producer verifies its own result with serial shadow mode in
canary; validators can choose OCC or serial mode.

> **Amendment A7 (2026-07-12 TPS review; revised in pass 2).** Add optional
> producer pipelining across blocks: while block N propagates, the producer
> may speculatively pre-execute a block N+1 candidate against block N's
> committed state, discarding the work on fork or mempool change. Nothing is
> published early and block validity timing is unchanged, so this remains
> class 1; it reclaims the commit-tail and propagation window during which
> the execution engine currently idles. Pass-2 caveat: under PoB the same
> producer does not reliably win consecutive blocks, so the benefit is
> probabilistic — proportional to the node's share of block wins — and for
> non-winning nodes the work degrades into warm-cache validation of the next
> block. Measure realized benefit against wasted work before enabling by
> default.

**Protocol impact.** Class 1; operationally coordinated but no consensus rule
change. Any result mismatch blocks release.

**Legacy/Teleno impact.** Legacy validates every canary block sequentially.
Mixed Teleno serial/OCC and legacy networks are required.

**Prerequisites.** Phase 4 definition of done; Phase 2 state/commit cleanup;
capacity and memory limits; security audit.

**Test strategy.** Full private PoB networks with rotating/competing producers;
partition/rejoin; deep forks; crash/kill at each scheduler/commit boundary;
snapshot restore; slow validator; deterministic build replay; mixed binaries.

**Required benchmark.** Multi-host confirmed and irreversible matrix, resource
budgets unchanged; compare serial Teleno, OCC Teleno and legacy replay; 72-hour
soak with fork injection and hot/adversarial periods.

**Security risks.** A rare root divergence can fork the network; speculative
memory explosion; producer deadline miss. Enforce block-local memory budgets,
early fallback, serial canaries and automatic quarantine on mismatch.

**Rollback.** Runtime/config switch to serial execution; preserve the existing
database format; reject/stop production on self-check mismatch; old clients can
validate all blocks.

**Definition of done.** Mixed clients agree through extended canary testing;
OCC meets targets without increasing fork/LIB regressions; crash recovery is
identical; release documentation labels OCC optional until sufficient soak.

**Approval.** Maintainer and security approval plus explicit operator opt-in for
the first production-capable release. No separate end-user or governance
approval is required because consensus outputs are unchanged.

### Phase 6 — Optional Access Hints and Parallel-Friendly SDK Guidance

**Objective.** Reduce wasted speculative attempts without making declarations
part of transaction validity.

**Repositories/components.** `koinos-proto`, Teleno RPC/mempool/scheduler,
legacy clients for ignore/forward behavior, supported SDKs and contract docs.

**Code-level changes.** Define a versioned optional hint envelope outside the
signed consensus transaction, or an explicitly non-consensus RPC extension.
Represent point/range read/write expectations conservatively. Track hint
accuracy and charge admission resources/rate-limit abusive submitters locally;
always instrument actual execution. Add SDK tooling to profile contract access
and advise against unnecessary hot shared objects.

**Protocol impact.** Class 2. Hints cannot affect transaction ID, validity,
resource receipt, execution, block bytes or root.

**Legacy/Teleno impact.** Old clients omit hints; old nodes ignore them; legacy
services may forward the opaque optional envelope but validate normally.

**Prerequisites.** Production-quality dynamic access tracing from Phases 4-5
and evidence that scheduling/replay—not another stage—is limiting.

**Test strategy.** Missing, partial, excessive, contradictory and malicious
hints; old/new RPC clients; same transaction with different hints; guarantee
identical block output and transaction ID; privacy review for leaked access
patterns.

**Required benchmark.** OCC with no, correct, conservative and adversarial
hints across mixed/hot workloads; scheduling time, replay, memory and fairness.

**Security risks.** DoS by false hints, fingerprinting, oversized metadata and
implicit centralization around private schedulers. Bound/ignore hints, never
trust them, and make raw no-hint performance a supported path.

**Rollback.** Stop accepting/forwarding hints; consensus data is unaffected.

**Definition of done.** Old clients interoperate; changing or removing a hint
cannot change output; realistic hints provide a repeatable net benefit after
validation overhead; malicious hints are no worse than bounded no-hint
fallback.

**Approval.** API/proto and SDK maintainer approval; no separate end-user or
governance approval is required because hints are optional and non-consensus.

### Phase 7 — Propagation and Replay Pipeline

**Objective.** Ensure larger sustainably executable blocks can be disseminated,
validated and stored within the PoB timing safety margin.

**Repositories/components.** Teleno P2P and block pipeline; legacy
`koinos-p2p`, chain and block store; `koinos-proto` network messages.

**Code-level changes.** First add batched transaction gossip, compact duplicate
suppression, chunked block transfer, early header/signature checks, pipelined
download/validation and topology metrics. Only if loss/propagation remains
limiting, prototype Reed-Solomon chunks and an availability/dissemination DAG.
Add an optional Linux profile for receive/send batching, affinity and later
AF_XDP; retain portable sockets.

> **Amendment A8 (2026-07-12 TPS review).** Insert mempool-assisted compact
> block relay (BIP-152-style) ahead of chunking and erasure coding in the
> intervention order. Because transactions are already gossiped before
> inclusion, a block can be announced as a header plus short transaction-ID
> list, with a fetch round-trip only for missing transactions. This removes
> most block-propagation bytes at far lower complexity than Reed-Solomon
> chunks or a dissemination DAG, and must be measured first under the same
> RTT/loss matrix before either heavier mechanism is prototyped.

**Protocol impact.** Class 3. Linear blocks, PoB and fork choice are unchanged.

**Legacy/Teleno impact.** Dual-stack capability negotiation; mixed peers fall
back to the existing protocol. Both clients must reconstruct identical bytes.

**Prerequisites.** Multi-region Phase 5 evidence that propagation or replay is
the limiting minimum; bandwidth/loss/CPU profiles; abuse model and wire-version
plan.

**Test strategy.** Mixed-version mesh, packet loss/reorder/duplication,
malformed chunks, inconsistent erasure pieces, eclipse/topology attacks,
backpressure, partition healing, bandwidth caps, slow disk and state sync.

**Required benchmark.** Block sizes and transaction mixes over controlled
10/50/100/250 ms RTT and 0/0.1/1/5% loss; p50/p95/p99 first-byte, reconstruction,
validation, relay and LIB; bandwidth amplification per peer.

**Security risks.** Coding-amplification DoS, unavailable data, memory pressure,
peer discrimination and high-end network centralization. Authenticate block
identity early, cap incomplete assemblies and preserve ordinary fallback.

**Rollback.** Capability flag disables new gossip/chunks; existing block
protocol remains available throughout deployment.

**Definition of done.** Slowest supported profile receives and validates p99
test blocks in less than half the target interval with bounded memory/bandwidth;
mixed peers remain healthy; a dissemination DAG is adopted only if it beats
simpler chunking materially under the same fault model.

**Approval.** Coordinated binary release and operator rollout approval; no
separate end-user or governance approval while consensus results stay
unchanged.

### Phase 8 — Governed Resource-Capacity Expansion

**Objective.** Convert demonstrated execution/propagation headroom into higher
sustainable block inclusion without outrunning acceptable validators.

**Repositories/components.** Resource system contract, governance process,
Teleno and legacy producer configuration/telemetry, public test network.

**Code-level changes.** No hidden client override. Prepare a governed proposal
changing only existing resource-market/block-budget parameters; add dashboards
and automatic lag/fork alarms. If parameters cannot be changed entirely through
existing contracts/thunks, stop and reclassify the missing mechanism.

**Protocol impact.** Class 4 if the existing governed system-contract mechanism
is sufficient.

**Legacy/Teleno impact.** Both already obtain current resource limits through
the same system call. Both must demonstrate capacity before activation.

**Prerequisites.** Phases 0-7 gates; multiple hardware tiers; public testnet
soak; economic analysis of RC pricing and state growth; governance proposal.

**Test strategy.** Stepwise limit increases, heterogeneous validators,
full/empty/hot/adversarial blocks, forks, partitions, state growth, sync and
restore, RC price curves and producer packing.

**Required benchmark.** Confirmed and irreversible TPS at 1.25x, 1.5x and 2x
budgets, never jumping directly to the maximum; replay/propagation p99 and node
lag over at least one representative long-duration window.

**Security risks.** Slow-validator exclusion, longer forks, state bloat, RC
economic distortion and producers publishing blocks near deadlines. Use staged
governance values and conservative acceptance margins.

**Rollback.** A pre-reviewed governed proposal restores the previous
parameters; operators retain compatible binaries; alarms define the trigger.

**Definition of done.** Governance approves; testnet and canary measurements
meet replay, propagation, fork, finality and decentralization thresholds; both
clients accept/produce within limits; monitoring and rollback proposal are
ready before activation.

**Approval.** Explicit Koinos governance approval and operator coordination are
required. No mainnet action is authorized by this document.

### Phase 9 — Conditional Mandatory Access Declarations

**Objective.** Evaluate a consensus-visible scheduler only if the compatible
path cannot meet an agreed throughput target because dynamic conflicts or
tracking overhead are conclusively dominant.

**Repositories/components.** Protocol definitions, chain system calls,
Teleno/legacy chain and mempool, producer, all SDKs/wallets, governance and
activation tooling.

**Code-level changes.** Define signed access-list syntax, wildcard/range rules,
maximum size, nested-call behavior, system-contract coverage, exact mismatch
validity/error, cost charging, version gate and pre-activation simulation. A
transition format must allow existing applications to submit transactions or
derive conservative lists automatically.

**Protocol impact.** Class 5; consensus-breaking hard fork because enforcement
changes transaction/block validity.

**Legacy/Teleno impact.** Every validator, producer, RPC, SDK and application
must understand activation semantics. Legacy must implement the new validator
before activation or cease being a valid oracle.

**Prerequisites.** Published Phase 5-8 evidence; agreed target; formal
specification; application census; at least two independent implementations;
governance and long testnet period.

**Test strategy.** Under/over-declaration, dynamic/nested calls, upgrades,
ranges, deliberate aliases, payer/RC/nonce, old transactions, activation block,
reorg across activation and every major deployed contract.

**Required benchmark.** Compare OCC alone, optional hints and mandatory lists
on identical hardware/workloads; include list construction/client latency and
hot/adversarial cases. The change must provide a material end-to-end gain, not
only scheduler microbenchmarks.

**Security risks.** Application breakage, invalid declarations, new DoS surface,
centralized simulation providers and consensus split at activation.

**Rollback.** Before activation, cancel. After activation, rollback itself is
another coordinated consensus action; therefore an emergency disable rule and
old-format transition behavior must be in the specification.

**Definition of done.** Only satisfied if compatible approaches miss the
approved target, mandatory lists deliver a material confirmed/finalized gain,
existing applications have a compatible path, two clients agree, and the
community approves the hard fork. Otherwise the phase is rejected.

**Approval.** Explicit governance/community, release, operator and application
ecosystem approval. This plan does not authorize implementation or activation.

### Phase 10 — Consensus DAG/Object Model Decision Gate

**Objective.** Record, rather than hide, the ultimate boundary of this project.

**Repositories/components.** None under the recommended roadmap.

**Code-level changes.** None planned. A block DAG/full DAG consensus would
replace PoB ordering/finality; an object model would replace general object-
space semantics.

**Protocol impact.** A block/full DAG is class 5; a Sui-like object migration,
new genesis, or history reset is class 6.

**Legacy/Teleno impact.** Both would require new consensus implementations;
existing applications could not be presumed compatible.

**Prerequisites.** A separate charter explicitly overriding the current goal.

**Test strategy.** Not applicable because implementation is rejected. Any
future separately chartered effort would require a complete consensus,
migration, application-compatibility and two-client validation program.

**Required benchmark.** None under this plan. A future separate charter must
not reuse Koinos throughput claims or these implementation phases as
authorization.

**Security risks.** Chain split, identity loss, application/state migration,
and misleading users that the result is still unchanged Koinos.

**Rollback.** Reject before implementation.

**Definition of done.** This phase is complete when the project records the
decision not to pursue it under the high-throughput Koinos plan.

**Approval.** A new community mandate would be required; current decision is
rejection.

## 14. Work-Package Dependencies and Ordering

```text
Phase 0 evidence
  -> Phase 1 bounded pipeline
      -> Phase 2 Teleno hot-path cleanup
          -> Phase 4 OCC prototype
              -> Phase 5 production OCC
                  -> Phase 6 optional hints (only if replay data supports it)
                  -> Phase 7 propagation (only if network/replay is limiting)
                      -> Phase 8 governed capacity expansion

Phase 0 -> Phase 3 legacy performance profile
Phase 3 ---------------------> legacy oracle for Phases 4-8

Phases 5-8 fail agreed target specifically due to dynamic scheduling
  -> Phase 9 hard-fork evaluation

Phase 10 full DAG/object model: rejected and outside the dependency graph
```

Parallel work is allowed only where evidence remains comparable: legacy
message batching can proceed beside Teleno hot-path work after Phase 0, and
benchmark workload development can continue throughout. Resource limits must
not be raised to conceal weak replay or propagation.

## 15. Testing and Differential-Validation Strategy

### 15.1 Three Oracles

1. **Sequential Teleno oracle:** the current executor on a fixed snapshot.
2. **Sequential legacy oracle:** independently packaged chain service at its
   pinned commit.
3. **Protocol artifact oracle:** block bytes, receipts, events, resources,
   failed indices, object delta and state root stored in the corpus.

OCC promotion requires all three to agree. Where historical client behavior
differs due to a known bug fix, the fixture must name the activation/version
rule; it must never be silently normalized.

### 15.2 Test Layers

- Unit tests for MVCC version selection, dependency edges, validation,
  incarnation lifecycle, range phantoms and deterministic fallback.
- Property tests generate canonical transaction sequences and randomized worker
  schedules, then compare against serial execution.
- Contract conformance covers nested calls, system-contract replacement,
  callbacks, authorization, exceptions, logs/events and iterator access.
- Differential replay covers historical blocks, recent snapshots, generated
  blocks and intentionally invalid blocks.
- Network tests cover mixed clients, competing PoB producers, latency/loss,
  partitions, duplicates, reordering and rolling upgrades.
- Fault injection kills the process before/after state write, metadata update,
  root publication, propagation and index journal acknowledgement.
- Long soak tests detect memory growth, queue drift, validator lag and rare
  conflicts.
- Deterministic-build CI runs the same corpus with multiple worker counts,
  scheduling seeds, compiler modes, CPU vendors and operating systems.

### 15.3 Required Equality

For each block, compare byte-for-byte where serialized canonically:

```text
transaction verdicts and failure indices
operation and transaction receipts
event/log content and order
compute/network/disk resource usage
payer and maximum payer RC fields
post-block object key/value set
state Merkle root
block and transaction Merkle roots
head/LIB/fork-choice outcome after sequences and reorgs
```

Performance success can never waive equality.

## 16. Benchmark Matrix

### 16.1 Workloads

| ID | Workload | Required variants | Primary question |
| --- | --- | --- | --- |
| W1 | Simple KOIN transfer | independent payer/recipient; one recipient | Best-case and recipient contention |
| W2 | Payer contention | one payer sequential nonces; 8/64 payer groups | Nonce/RC serialization |
| W3 | Independent contract | separate object spaces/keys; warm/cold module | VM/state parallel scaling |
| W4 | Nested contract calls | 2/4/8 depth; shared/independent state | Dynamic trace completeness |
| W5 | DeFi hot state | shared pool/order book; partitioned pools | Realistic conflict ceiling |
| W6 | Mixed realistic | transfers, reads, writes, uploads/calls by measured proportions | End-to-end capacity |
| W7 | Read/range heavy | point reads and iterators with insert/delete phantoms | MVCC/range validation |
| W8 | Failure/reversion | failures before/after writes/events/nested calls | Side-effect isolation |
| W9 | Adversarial conflict | common keys, misleading hints, long executions | Replay/DoS fallback |
| W10 | Propagation blocks | compressible/incompressible, min/max tx sizes | Network/replay capacity |

Use public deployed contract bytecode and sanitized state snapshots where
legally and operationally appropriate, but execute on isolated networks.

### 16.2 Dimensions and Metrics

Dimensions: client `legacy/Teleno`; executor `serial/OCC`; workers `1/2/4/8/16`;
hardware `consumer Mac/baseline Linux/performance Linux`; storage
`consumer SSD/datacenter NVMe`; RTT/loss profiles; block budget; cold/warm;
single/multiple producers; and fork/partition injection.

Metrics:

- offered, admitted, rejected, included, independently validated and
  irreversible TPS;
- end-to-end latency distributions for admission, inclusion and LIB;
- per-stage service time, queue time, CPU/core, allocation, RSS and I/O;
- canonical block resource usage versus physical CPU/bytes/write amplification;
- conflicts, validation failures, attempts/commit and fallback suffix length;
- block propagation/reconstruction/replay/commit time;
- mempool depth/drain, peer bandwidth, fork rate, head/LIB lag and state growth;
- energy/power where hardware centralization is being evaluated.

Each result reports median, p95/p99, dispersion and failure count over at least
five steady-state runs. A saturation result is valid only when the generator has
headroom and the mempool returns to its pre-test level.

### 16.3 Confirmed and Finalized TPS Definitions

- **Confirmed/included TPS:** unique accepted transaction IDs observed in
  canonical blocks by an independent witness, divided by the steady-state
  observation interval. Report reorged inclusions separately.
- **Irreversible/finalized TPS:** those transactions after the witness reports
  their block at or below LIB, divided by the end-to-end interval from first
  submission to final LIB advancement. Report finality latency separately.

Neither metric includes rejected requests in the numerator. Both include
network and witness work and must identify the confirmation policy.

## 17. Adversarial Review and Security/Consensus Risks

| Attack/failure | Candidate exposed | Required control | Release gate |
| --- | --- | --- | --- |
| Timing-dependent commit | B/D | Fixed canonical indices; commit cursor | Random-schedule equality |
| Conflict storm | B/D | Replay/memory budget; sequential suffix | CPU amplification bound |
| False access hint | D | Hint never authoritative; bound size/rate | Same result/no worse than fallback |
| Undeclared access | C | Exact validity and conservative wildcard | Two-client hard-fork tests |
| Range phantom | B/D | Range version/fingerprint validation | Insert/delete property tests |
| RC double spending | B/D | Private incarnation accounting; canonical recheck | Same-payer/system-state corpus |
| Nonce race | All parallel | Version nonce; canonical validation | Payer-chain tests |
| Event/receipt reorder | B/D | Per-incarnation indexed buffer | Byte equality |
| State-root divergence | B/D | Canonical merged delta; serial shadow | Automatic producer stop |
| Reorg across work | B/D/E | Parent-scoped execution domain; cancel fork | Deep-fork fault test |
| Crash after partial work | B/D | Persist canonical atomic batch only | Kill-point recovery |
| Slow validator | A-E | Hardware tiers; conservative block margin | No head/LIB lag |
| Network partition | E | Existing PoB/fork choice; bounded DAG cache | Partition/rejoin test |
| State growth | All | Disk resource/economic model; sync/restore test | Long-duration projection |
| Hardware centralization | A-E | Portable profile; publish tier results | Governance threshold |
| Legacy/Teleno drift | All | Continuous differential CI/canary | Zero unexplained mismatch |

The most important revision from the red-team pass is that retries cannot be
unbounded or governed by wall-clock races. A node may decide locally to execute
the remaining canonical suffix sequentially at any time because both paths
must return the same result. For predictable DoS cost, the production policy
uses fixed per-block attempt/memory ceilings and exposes them in metrics.

## 18. Hardware and Decentralization Trade-Offs

Three supported profiles prevent a benchmark machine from silently becoming a
validator requirement:

| Profile | Purpose | Features |
| --- | --- | --- |
| Portable | Koinos One/macOS and ordinary nodes | Normal sockets, OS scheduling, bounded workers, consumer SSD |
| Baseline validator | Decentralized production/verification floor | Modern 8-core CPU, adequate RAM, SSD/NVMe, no kernel bypass |
| Performance | Optional high-throughput Linux | More cores/RAM, NUMA affinity, batched I/O; optional AF_XDP later |

Exact minimums must come from Phase 0 rather than this plan. Every resource-
limit proposal must pass on the baseline profile with p99 replay plus
propagation comfortably below the block interval. The performance profile may
increase local admission and provide headroom, but cannot be the only profile
capable of following the chain.

Core pinning and busy polling reduce jitter but consume energy and impair
co-tenancy. Huge pages and locked memory increase operational complexity.
Kernel bypass is Linux/NIC-specific and can introduce security/maintenance
costs. These are opt-in deployment techniques, never protocol assumptions.

## 19. Migration, Activation, Rollback, and Failure Containment

### 19.1 Activation Sequence

1. Merge measurement changes with all optimizations disabled.
2. Enable pipeline/hot-path features in shadow or canary modes.
3. Publish differential results and artifact hashes.
4. Release OCC disabled by default; enable on private networks.
5. Enable OCC for non-producing observers; compare against legacy.
6. Allow opt-in producers with serial self-check and automatic stop.
7. After soak, make OCC the recommended Teleno mode while retaining serial.
8. Deploy optional protocol batches through negotiated mixed-version support.
9. Only after all capacity gates, prepare a governed resource proposal.
10. Treat any mandatory access or consensus change as a separate specification
    and activation program.

No phase migrates historical state or changes chain identity. Existing
databases remain readable. New non-consensus caches/indexes must be rebuildable.

### 19.2 Failure Containment

- Per-block executor circuit breaker falls back to serial before a deadline.
- A receipt/root mismatch quarantines the candidate and stops block production;
  it does not publish the faster result.
- Observers may log a mismatch and re-run serially; producers fail closed.
- Bounded queues reject/backpressure rather than exhaust memory.
- Speculative data lives in block-scoped arenas and is dropped on fork/cancel.
- Consensus commit and optional index journals have independent recovery paths.
- P2P capability negotiation always retains the current protocol until the
  coordinated rollout is complete.
- Monitoring alarms on replay amplification, validation time, queue saturation,
  fork rate, witness lag and LIB regression.

### 19.3 Rollback Levels

| Level | Trigger | Action | State impact |
| --- | --- | --- | --- |
| Feature | Performance/regression | Disable pipeline/shard/cache option | None |
| Executor | OCC mismatch/deadline | Select sequential execution | None |
| Binary | Operational regression | Roll back compatible release | Existing DB retained |
| Network feature | Peer issue | Disable negotiated batches/chunks | Linear block protocol retained |
| Governed parameter | Validator lag/fork/RC issue | Execute pre-reviewed governance rollback | Parameter change only |
| Hard fork proposal | Pre-activation risk | Cancel/postpone activation | None |

## 20. Phase Acceptance Summary

| Phase | Non-negotiable acceptance evidence |
| --- | --- |
| 0 | Reproducible raw artifacts and legacy/Teleno consensus corpus |
| 1 | Bounded overload, zero verdict drift, admission bottleneck moved/measured |
| 2 | No dominant global independent-work lock; safe restart/reorg/index rebuild |
| 3 | Mixed old/new rolling deployment and identical sequential results |
| 4 | Zero differential mismatches; scaling and adversarial fallback targets |
| 5 | Multi-host mixed-client canary, crash/fork safety and serial rollback |
| 6 | Hints optional/untrusted, old-client compatible and measurably beneficial |
| 7 | p99 propagation+replay safety margin on baseline hardware and mixed peers |
| 8 | Governance approval plus staged testnet capacity/economic evidence |
| 9 | Compatible path proven insufficient, two implementations, ecosystem-ready hard fork |
| 10 | Rejection recorded under this project charter |

The detailed definition of done in each phase is authoritative. Tasks may be
created directly from each phase's components and code-level changes, but no
implementation begins under this research deliverable.

## 21. Rejected Alternatives and Reasons

1. **Raise block limits first.** Rejected because block limits are only one
   term in the minimum-capacity equation; slow replay/propagation would increase
   forks and centralization.
2. **Publish the 451 TPS admission figure as Koinos TPS.** Rejected because it
   excludes inclusion, WAN validation and finality.
3. **Run transactions in parallel and accept completion order.** Rejected
   because timing-dependent state, receipts and roots are nondeterministic.
4. **Trust client access hints.** Rejected because malicious/incomplete hints
   can cause wrong scheduling or DoS. Hints remain optional and verified.
5. **Require Solana-style access lists immediately.** Rejected as the first
   path because dynamic nested Koinos access makes it application-breaking and
   consensus-visible; OCC can preserve compatibility.
6. **Shard consensus state before measuring OCC.** Rejected because cross-shard
   dynamic calls, atomicity and composability add a much larger protocol change.
7. **Replace RabbitMQ everywhere.** Rejected as premature. Batched/co-located
   profiles target measured hot paths while retaining service operability.
8. **Make AF_XDP/high-end hardware mandatory.** Rejected for portability,
   security and decentralization. It can be an optional later backend.
9. **Use a transaction DAG as a universal speed solution.** Rejected because a
   DAG schedules known independence but cannot parallelize a truly hot key.
10. **Replace PoB with Bullshark/Mysticeti.** Rejected because that changes
    ordering, finality and fork choice; it is not merely a faster Koinos node.
11. **Adopt a Sui-like object model.** Rejected because it changes state and
    transaction semantics and requires application migration.
12. **Reset history or launch a faster Koinos-derived chain.** Rejected by the
    project objective and because it would not improve the existing Koinos
    network.

## 22. Open Questions

1. Which system-contract and resource-market objects are touched by every
   simple transfer, and what fraction of that state can be read-only or updated
   through a deterministic aggregate without changing semantics?
2. Do `get_next_object` and `get_prev_object` expose enough internal bounds to
   implement efficient, exact phantom detection, or is a state-backend range
   generation index required?
3. What part of admission execution can be safely reused during block execution
   across fork/head changes without trusting stale state?
4. Is Fizzy instantiation or WASM instruction execution dominant after cache
   warm-up? Would another protocol-equivalent backend pass the VM corpus?
5. Which block-accepted indexes are operationally required before RPC success,
   and which can be journaled/rebuilt asynchronously?
6. What are current mainnet resource limits and distributions of transaction
   compute/network/disk usage, block fullness, fork rate and time to LIB?
7. What baseline validator hardware does the community consider acceptable?
8. How much client diversity is required before OCC production is enabled by
   default, and who owns the independent differential canaries?
9. Can optional access hints be carried entirely outside the signed transaction
   and forwarded without privacy or amplification problems?
10. Does ordinary chunked/batched propagation meet future block targets, making
    erasure coding or a dissemination DAG unnecessary?
11. Which resource/PoB parameters are fully governable with current system
    contracts and native thunks, and which would require a binary activation?
12. What explicit confirmed and irreversible TPS target, workload mix, hardware
    floor and decentralization budget defines “credibly compete on speed”?

## 23. Primary-Source Bibliography

### Koinos

- Koinos Developer Hub, [System calls](https://docs.koinos.io/architecture/system-calls/).
- Koinos Developer Hub, [Smart contracts](https://docs.koinos.io/overview/smart-contracts/).
- Koinos Developer Hub, [Proof of Burn](https://docs.koinos.io/architecture/proof-of-burn/).
- Koinos Developer Hub, [Resources](https://docs.koinos.io/architecture/resources/).
- Koinos Developer Hub, [Finality](https://docs.koinos.io/exchanges/finality/).
- Koinos Developer Hub, [Running a node](https://docs.koinos.io/validators/guides/running-a-node/).
- Koinos source repositories: [Teleno](https://github.com/koinos/teleno),
  [chain](https://github.com/koinos/koinos-chain),
  [mempool](https://github.com/koinos/koinos-mempool),
  [block producer](https://github.com/koinos/koinos-block-producer),
  [P2P](https://github.com/koinos/koinos-p2p),
  [block store](https://github.com/koinos/koinos-block-store),
  [JSON-RPC](https://github.com/koinos/koinos-jsonrpc), and
  [protocol definitions](https://github.com/koinos/koinos-proto).

### Solana and Firedancer

- Solana, [Transaction pipeline](https://solana.com/docs/core/transactions/transaction-pipeline).
- Solana, [Program execution](https://solana.com/docs/core/programs/program-execution).
- Solana, [Compute budget and fees](https://solana.com/docs/core/fees/compute-budget).
- Solana, [Whitepaper](https://solana.com/solana-whitepaper.pdf).
- Solana, [Gulf Stream](https://solana.com/news/gulf-stream--solana-s-mempool-less-transaction-forwarding-protocol).
- Solana forum, [Turbine improvements](https://forum.solana.com/t/feature-turbine-improvements-1-14-17/200).
- Anza, [Agave source](https://github.com/anza-xyz/agave).
- Firedancer, [Architecture overview](https://docs.firedancer.io/guide/firedancer.html).
- Firedancer, [Transaction ingress](https://docs.firedancer.io/guide/protocol/transaction_ingress.html).
- Firedancer, [Performance tuning and tiles](https://docs.firedancer.io/guide/tuning.html).
- Firedancer, [Net tile and AF_XDP](https://docs.firedancer.io/guide/internals/net_tile.html).
- Firedancer, [Source repository](https://github.com/firedancer-io/firedancer).

### Parallel Execution

- Gelashvili et al., [Block-STM: Scaling Blockchain Execution by Turning
  Ordering Curse to a Performance Blessing](https://arxiv.org/abs/2203.06871).
- Aptos Labs, [Aptos whitepaper](https://aptosnetwork.com/whitepaper/aptos-whitepaper_en.pdf).
- Aptos Labs, [Aptos Core source](https://github.com/aptos-labs/aptos-core).
- Aptos Labs, [Standardizing verifiable performance benchmarks](https://aptosnetwork.com/currents/standardizing-verfiable-performance-benchmarks).
- Sei, [Parallelization engine](https://www.docs.sei.io/learn/parallelization-engine).
- Sei, [Sei Giga](https://www.docs.sei.io/learn/sei-giga).
- Sei, [Giga specifications](https://docs.sei.io/learn/sei-giga-specs).
- Sei, [Optimizing contracts for parallelization](https://docs.sei.io/evm/best-practices/optimizing-for-parallelization).

### DAG and Object-Centric Systems

- Danezis et al., [Narwhal and Tusk: A DAG-based Mempool and Efficient BFT
  Consensus](https://arxiv.org/abs/2105.11827).
- Spiegelman et al., [Bullshark: DAG BFT Protocols Made Practical](https://arxiv.org/abs/2201.05677).
- Mysten Labs, [Mysticeti: Low-Latency DAG Consensus with Fast Commit Path](https://docs.sui.io/paper/mysticeti.pdf).
- Mysten Labs, [The Sui Smart Contracts Platform](https://docs.sui.io/paper/sui.pdf).
- Mysten Labs, [Sui Lutris](https://docs.sui.io/paper/sui-lutris.pdf).
- Avalanche, [Consensus architecture](https://build.avax.network/docs/nodes/architecture/consensus).
- Avalanche, [Cortina: X-Chain linearization](https://build.avax.network/blog/cortina-x-chain-linearization).

## Conclusion

Koinos does not need a new chain to begin competing seriously on performance.
It needs disciplined measurement, a lower-overhead pipeline, deterministic
parallel execution that is provably serial-equivalent, independent legacy
validation, and only then carefully governed capacity increases. This path
preserves mainnet history, existing contracts, PoB, Mana/RC, APIs and chain
identity for as long as engineering evidence allows.

The most credible DAG contribution is internal dependency scheduling, with a
possible later dissemination DAG only if propagation data justifies it. A full
DAG consensus or object-centric replacement does not satisfy the stated goal.
The roadmap therefore maximizes compatible gains first and makes the hard-fork
boundary explicit, measurable and avoidable rather than silently crossing it.

## Appendix A — 2026-07-12 TPS Review Amendment Summary

The following amendments were added inline by a blockchain TPS optimization
review on 2026-07-12, in two passes. Pass 1 added A1-A8; pass 2 added A9-A13
and revised A4 and A7 in place. Each is marked as a quoted
`Amendment A<N> (2026-07-12 TPS review)` block.

| ID | Location | Amendment |
| --- | --- | --- |
| A1 | Section 3.1 | Baseline validity: prove load-generator headroom (single Python process may cap the 451 TPS figure) and record memory pressure/swap/block-cache telemetry on the 8 GiB host before treating admission ceilings as node limits. |
| A2 | Section 6.5 | Rebase the "2x the 451-TPS" Phase 1 gate on the Phase 0 re-measured baseline with proven generator headroom instead of the historical constant. |
| A3 | Section 11.2 | Scheduler refinements: deterministic commutative-delta handling for whitelisted hot resource-market counters (byte-identical results required), and pinning same-payer nonce chains to one worker in nonce order. |
| A4 | Phase 1 | Clarify batch signature verification: secp256k1 ECDSA recovery admits no ed25519-style algebraic batching; use multi-core parallel verification, reused contexts, and a transaction-ID-keyed verdict cache reused by block validation. |
| A5 | Phase 2 | Promote VM backend evaluation (open question 4) to an explicit workstream: profile Fizzy, evaluate a protocol-equivalent JIT/AOT backend, with bit-identical compute-unit metering as a hard constraint. |
| A6 | Phase 2 | Add RocksDB commit-path work: WAL fsync policy versus durability contract, write-stall/compaction telemetry, prefix bloom filters, separate column families for consensus versus rebuildable index data, plus a no-stall soak criterion in the definition of done. |
| A7 | Phase 5 | Add optional producer pipelining: speculatively pre-execute the block N+1 candidate while block N propagates, discarding on fork; class 1 because nothing is published early. |
| A8 | Phase 7 | Insert mempool-assisted compact block relay (BIP-152-style) ahead of chunked transfer and erasure coding in the propagation intervention order, measured under the same RTT/loss matrix first. |
| A9 | Section 6.3 | Arithmetic correction: eight-worker raw speedup at p=0.75 is 2.91x, not 2.5x; a 2.5x speedup corresponds to p≈0.69. The 2.5-5x summary range stands, but the wrong value must not enter gate math. |
| A10 | Section 6.5 | Add an initial sync/catch-up target: a fresh baseline-profile node must replay at no less than five times the sustained inclusion rate (the 49 TPS witness figure versus about 180 TPS block flow already hints at the risk); explicit Phase 8 gate. |
| A11 | Section 5.2 | State reads traverse the per-block `state_delta` chain toward the irreversible root, so read cost grows with head-to-LIB depth and block size; instrument read depth in Phase 0 and evaluate a flattened read snapshot in Phase 2 without changing squash/commit/reorg semantics. |
| A12 | Section 11.2 | Event sequence numbers are block-scoped and serial; incarnations record only local positions and global sequence values are assigned at canonical commit, never speculatively. |
| A13 | Section 6.6 | State the 10,000 TPS storage/bandwidth boundary: about 9.45 MB per block, roughly 272 GB/day of block data plus about 30 GB/day of accounted state growth; archive and sync costs join the honest boundary. |

Pass 2 also revised two pass-1 amendments in place: A4, because the Koinos
transaction ID hashes only the header and does not commit to signature bytes,
so a verification-verdict cache must key on (transaction ID, signature-bytes
hash) rather than the ID alone; and A7, adding the PoB caveat that producer
pipelining pays off only in proportion to consecutive block wins.

The amendments change instrumentation requirements, gate definitions, numeric
corrections, and implementation ordering only. None of them alters the
compatibility classification framework, the consensus-upgrade boundary in
Section 10.1, or the rejection of mandatory declarations and DAG consensus as
default paths.
