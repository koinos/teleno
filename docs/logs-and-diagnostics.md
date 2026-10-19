# Logs And Diagnostics

`teleno_node` writes operational logs to stdout/stderr. The logs are the first
place to verify config, feature flags, storage, RPC listeners, P2P progress,
backup events, and shutdown behavior.

## Peer RPC local refusal (LR2 development)

The outgoing native client distinguishes local lifecycle/receive-capacity
refusal from peer protocol errors. Sync workers do not add peer scores for those
local conditions. A receive-capacity refusal narrows the next read-only block
request before any application from the failed batch; it is not block invalidity
or evidence of remote misconduct. Normal sync backoff still applies. Complete
response deadlines and cancellation release owned work without replaying the
RPC. These corrections do not bound synchronous DNS, SDK/decoded aggregates or
all server work. See [P2-C evidence](validation/low-resource-lr2-p2p-client.md).

## Explicit native dial owners (LR2 development)

Concrete cpp-libp2p C++ `dial_stats()` exposes jobs, retained/provisional peer
string capacity, high waters/limits, local capacity refusals, setup/completion
exceptions and optional advice-retention bypasses. Credits span restart
generations and remain charged while old callers/SDK callbacks retain owners.
These are not connection count, complete heap/RSS, or a new RPC/CLI/GUI metric.
Other transport implementations do not expose this typed snapshot. See
[P2-E evidence](validation/low-resource-lr2-p2p-dials.md), including unbounded
SDK/known-peer/control and implicit GossipSub/RPC/DHT/inbound dial axes.

## Native peer metadata (LR2 development)

Concrete cpp-libp2p C++ `peer_advice_stats()` reports retained optional advice
count/string capacity, limits, evictions, capacity/allocation bypasses and valid
SDK observations. `peer_snapshot_stats()` reports shared live/constructing
projection jobs, slots, attributed capacity, peaks/limits and local refusals.
Move-only results retain their credits across stop/restart until storage dies.
Scalar connected count and projections describe native published observations,
rather than the raw SDK connection inventory. No new RPC/CLI/GUI metric is added.

Periodic bookkeeping adds `discovery-snapshot-refusals` separately from
allocation refusals. The worker waits its usual interval and retries on its
next cycle without peer scoring. These counters do not bound active/SDK metadata,
all caller copies or total heap/RSS. See
[P2-F evidence](validation/low-resource-lr2-peer-snapshots.md).

## SDK address metadata (LR2 development)

Concrete cpp-libp2p C++ `sdk_address_stats()` reads a stable shared policy,
without accessing mutable host/SDK state from the caller thread. It reports
retained peer/address counts, vector-slot capacity, binary/text encoding lengths,
current repository owners, limits/high waters, evictions, capacity/allocation
bypasses, observer/protection/reentrant failures and expiry/collection activity.
Maintenance run, collector-failure and timer-admission-failure counters distinguish
successful collection from merely configuring an interval.

Retired generations remain charged until storage dies. These counters exclude
private string capacities, allocator overhead, inherited DNS history, other
stores/queues and caller copies; they are neither heap/RSS nor new RPC/CLI/GUI
metrics. The [P2-G report](validation/low-resource-lr2-sdk-addresses.md) separates
original SDK reproductions, current-source tests and paired mechanism results
from the remaining whole-node/producer/40-GB restore gates.

## Peer bookkeeping (LR2 development)

Periodic `[p2p] Peer bookkeeping:` logs report candidate count/string capacities,
protected candidates, effective count limit, evictions and candidate/discovery
allocation refusals. They also report error-record count, attributed key
capacity, configured limits, local-capacity refusals and allocation refusals.
A locked native C++ snapshot includes the configured candidate/seed counts and
expiry/saturation/sweep counters. No new RPC or CLI diagnostic is added.

Capacities exclude map/node overhead, caller owners and transport/SDK memory;
they are not heap/RSS or an aggregate budget. Protected candidates can raise
the effective count limit. Local score-bookkeeping refusal is not peer misconduct
or consensus invalidity. Recovery after a one-shot discovery allocation fault
does not prove behavior under machine-wide OOM. See
[P2-D evidence](validation/low-resource-lr2-peer-bookkeeping.md).

## Useful Log Lines

Native VM memory diagnostics require more than process RSS or the 32-entry
module-cache limit. [V1-A evidence](validation/low-resource-lr2-vm-lifetime.md)
separates tracked allocation requests, controlled cold-parser counts and fixture
RSS from total heap/Linux qualification. Warm hits bypass the cold-parser lock;
distinct cold calls may wait and retain additional state. [V1-C](validation/low-resource-lr2-vm-capacity.md)
adds cache-only retained-capacity attribution and optional retention limits;
active instances, waiters and allocator overhead remain additional.
The preserved V1-A binary's context-allocation fault terminates its isolated
process with exit 77. [V1-B](validation/low-resource-lr2-vm-allocation.md) uses a
pinned corrected dependency; its context constructors return null under scoped
allocation failure and native execution propagates a local `std::bad_alloc`,
not a reverted/accepted receipt. These one-shot faults do not prove cleanup
under persistent machine-wide OOM or establish a whole-node memory ceiling.

Periodic native `[metrics]` lines include `vm_cache_available`,
`vm_cache_entries`, `vm_cache_capacity_bytes`, `vm_cache_max_capacity_bytes`,
`vm_cache_lookup_hits`, `vm_cache_lookup_misses`, `vm_cache_evictions` and
`vm_cache_retention_bypasses`. Availability is explicit, not an unavailable
estimate reported as measured zero. Capacity is a conservative cache charge,
not physical heap/RSS. A zero byte maximum means bytes unlimited (32 entries
still apply). Lookup counters include the cold-lock recheck, not just executions
or parse counts; bypassed modules may parse again on each call. Evictions include
replacement retirement; `clear()` resets current capacity/entries while preserving
cumulative counters. These are log/C++ diagnostics, not a new RPC/CLI endpoint.

[R1-A HTTP-body diagnostics](validation/low-resource-lr2-rpc-bodies.md) expose
completed-request/materialized-response capacities and combined high water via
the native C++ server API, not an HTTP/CLI endpoint. After completed writing,
healthy idle keepalive holds neither body; pending writes retain their owner
through cancellation. Partial ingress, headers, transport buffers, JSON/protobuf/
service/serialization allocations and allocator retention are additional. These
transition counters are not total heap/RSS or an output admission ceiling.

[R1-B batch evidence](validation/low-resource-lr2-rpc-batch.md) separately tracks
ordinary C++ allocations on one fixture worker while processing twelve results.
Per-item serialization removes retained prior JSON trees, not the final HTTP
body. The test driver, allocator overhead/retention, aligned/direct allocations
and whole-node/Linux behavior are outside this attribution. It is not a new
runtime diagnostic endpoint or a memory cap.

Look for:

| Log text | Meaning |
| --- | --- |
| `[node] teleno_node ... starting` | Process started and build identity was printed. |
| `[node] basedir:` | Resolved data directory. |
| `[node] config:` | Resolved config path. |
| `[features] ... enabled` | Effective feature flags after config and CLI overrides. |
| `[runtime] Thread topology:` | Effective worker counts. |
| `[chain] State DB opened` | Chain state storage opened. |
| `[chain] Indexing complete` | Startup indexing finished. |
| `[chain] Indexer failed; refusing to enter ready state:` | Startup catch-up was cancelled or failed validation. Components stop and the process exits non-zero without logging ready. |
| `delta_replay_fallback height=... id=...` | A stored receipt failed its successor-root check and the block is being fully re-executed once. |
| `Delta replay re-execution fallbacks: ...` | Number of controlled fallbacks during a completed fast-index run. |
| `[block_producer] Restore recovery hold active ...` | Production is disabled until observer validation passes and a later explicit activation releases the hold. |
| `[block_producer] Restore recovery hold released ...` | A post-validation restart explicitly requested `--enable block_producer`. |
| `[node] teleno_node ready` | Runtime is ready for normal operation. |
| `[metrics] head_height=...` | Periodic head, peer, mempool, and RSS metrics. |
| `[node] teleno_node shutdown complete` | Clean shutdown completed. |

Current Linux development builds derive `rss_bytes` from the second
`/proc/self/statm` field (resident pages) and the runtime page size. Older
builds read the first field, virtual size, and can substantially overstate RSS.
The correction changes reporting, not memory allocation. Use independent
kernel `VmRSS`/`VmHWM`, timed process peaks and concurrent host `MemAvailable`
for capacity qualification; see the [RSS validation](validation/linux-rss-counter.md).

## Producer Handoff Diagnostics

Unreleased native producer handoff diagnostics add `producer_running`,
`producer_candidates_prepared`, `producer_pob_evaluations`,
`producer_head_retries`, `producer_mempool_retries` and `producer_errors` to
periodic `[metrics]` lines. Counters accumulate within one process and restart
from zero with that process. Preparation/evaluation count successfully returned
producer attempts; they are not produced-block counts. A temporary unavailable
parent or observed head change defers assembly for 10 ms without fabricating a
transaction response. After 100 consecutive deferrals the normal warning/error
path exposes the persistent failure. A recovery message identifies a subsequently
completed attempt. See [validation](validation/native-producer-handoff.md).

## Capture Logs

!!! warning "Local log write"
    This writes a log file under the selected basedir.

```bash
mkdir -p "$BASEDIR/logs"
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --log-level info \
  --disable block_producer \
  >"$BASEDIR/logs/teleno_node.log" 2>&1
```

Tail the log:

```bash
tail -f "$BASEDIR/logs/teleno_node.log"
```

## JSON-RPC Diagnostic Query

```bash
curl -sS http://127.0.0.1:18122/ \
  -H 'content-type: application/json' \
  --data '{"jsonrpc":"2.0","id":1,"method":"chain.get_head_info","params":{}}'
```

Run it more than once to confirm the head advances while syncing.

When `verify-blocks: false`, a successful run may report the known controlled
fallback at historical block 30,504,202. The exact historical signed-root scar
at block 32,789,377 is accepted only for its immutable block/root triple and
does not require fallback. The validated archive also contains 70,730 receipts
from 34,686,822 through 35,526,515 whose duplicate key cannot be reconstructed
by receipt replay; each is therefore fully re-executed once. Compare fallback
counts and heights with the committed validation report. Any unclassified
fallback, failed re-execution, or state-Merkle mismatch must be investigated
before release or producer activation.

If startup indexing is cancelled or fails, Teleno does not enter ready state.
Preserve the state database and investigate the first indexer error; do not
use the absence of a ready log as a reason to clear chain state.

After restore, the `.backup-observer-recovery` file and the corresponding hold
log line are expected. Their presence is a safety state, not a producer error.

## Storage Report

`--storage-report` opens storage, prints layout metadata and RocksDB column
family estimates, then exits:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --storage-report
```

Use it to confirm the shared DB path, chain storage layout, compression
selection, and column family estimates.

With opt-in block Zstd parameters, startup also logs the configured level,
dictionary size, training bytes and buffer bytes per active table builder.
Level `32767` in that log means the linked library's default. This describes
future writes, not the compression of every existing SST. `--storage-report`
still reports codec selection rather than level/dictionary settings. Use the
active config and startup log together; preflight does not probe linked codec
support. See [compression policy](configuration.md#experimental-block-compression-policy).

LR2 development also prints `memory.*` shared-cache capacity/usage/pinned usage,
aggregate memtable flush threshold and supported memtable/reader estimates.
Missing estimates are `unavailable`. Pinned cache bytes are a subset of cache
usage; the shared cache is counted once, not once per column family. These
properties are not total RSS or a hard memory cap, and opening a stopped DB for
this report does not measure a running node's warm working set. Stop the basedir
owner before using it. See [LR2 evidence](validation/low-resource-lr2-memory.md).

Native LR2 metrics now use a nonmaterializing visible-head pending count for
`pending_txs`; the former zero-record request expression always printed zero.
This is an O(n) logical count, not retained-fork bytes or an admission cap.
`transaction_notification_failures` counts observer exceptions after successful
mempool insertion: retained work is not retried or reported as rejected for that
reason. Review warnings and observer behavior; signal dispatch may stop at the
throwing observer. See [admission evidence](validation/low-resource-lr2-admission.md).

Current state-object cache payload bounds exclude immutable owners held by
readers after eviction and all map/list/control-block overhead. Clone sharing
does not bound fork count or caller lifetimes. Neither offline DB accounting
nor a lower synthetic clone peak is a process-memory guarantee. See
[state ownership evidence](validation/low-resource-lr2-state-ownership.md).

Native development also offers on-demand C++ indexed-fork retention inspection,
not a new runtime RPC/CLI or periodic metric. It deduplicates shared immutable
map owners and reports copied keys/tombstones and non-map backends as unmeasured.
An exclusive scan uses temporary memory and blocks private mempool operations;
string capacities are not allocated heap/RSS or an admission ceiling. Empty-head
expiry still leaves retained ancestors. See
[M1-D evidence and exclusions](validation/low-resource-lr2-retained-memory.md).

The opt-in standalone transaction-work policy adds `transaction_work_enabled`,
`transaction_work_jobs`, `transaction_work_wire_bytes`, the corresponding
`peak_jobs`/`peak_wire_bytes`, cumulative `refusals`, and configured
`max_jobs`/`max_wire_bytes` fields (each prefixed `transaction_work_`). These
are O(1) locked counters, not a retained-fork scan. When disabled, zero counters
mean accounting is off, **not no transactions are executing**. Input wire
bytes exclude parsed-input overhead, state, VM, receipts and returned outputs.
Peaks/refusals are process-lifetime counters. Review
[M1-E scope](validation/low-resource-lr2-transaction-work.md); do not sum these
wire bytes with RSS or interpret them as whole-node memory usage.

The opt-in retained-record policy adds `pending_record_enabled`, `owners`,
`capacity_bytes`, `peak_owners`, `peak_capacity_bytes`, `refusals`, `max_owners`
and `max_capacity_bytes` (all prefixed `pending_record_`). These are O(1) locked
lifetime counters, not all-fork scans. Disabled/zero counters mean untracked,
not an empty mempool. Encoding initially reserves wire-size bytes, then charges
actual string capacity before publication. Ancestors/caller-held owners keep
credits after head expiry; final-owner destruction returns them. Remaining
metadata/workspace and other consumers are not included. Do not add these
capacities to RSS or mistake them for whole-node memory. See
[M1-G scope](validation/low-resource-lr2-pending-records.md).

## Backup Diagnostics

Native development M1-F prepares fork-head mempool expiry/removal before any
publication. An allocation/decoding failure is an error, not partial cleanup
reported as success; unchanged work can be retried later by its caller. This is
not an automatic retry, block/LIB or persistent-commit guarantee. Head expiry
still retains ancestor records until their normal lifecycle ends. Prepared
tombstone buckets require temporary memory and hashing work; this correction
does not establish aggregate memory limits. See
[cleanup evidence](validation/low-resource-lr2-cleanup-atomicity.md).

LR2 P2P peer snapshots also print connected/owned/retired sync workers, the
internal worker limit and cumulative capacity refusals. Retired generations
still count until their threads finish and are joined; they are not detached.
These counts exclude transport buffers and are not heap/RSS totals. Cancellation
is checked between operations, not forced into an admitted execution. See
[worker and external test-storage evidence](validation/low-resource-lr2-workers.md).

Validate backup config without opening RocksDB or connecting to SSH:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-dry-run \
  --backup-json
```

List local snapshots without opening RocksDB:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-list \
  --backup-json
```

## Listener Checks

macOS and Linux:

```bash
lsof -nP -iTCP:18122 -sTCP:LISTEN
lsof -nP -iTCP:18088 -sTCP:LISTEN
```

Expected observer setup:

- JSON-RPC listens on the intended loopback port.
- Backup admin listens only when explicitly enabled.
- gRPC is absent unless explicitly enabled.

## Process Checks

```bash
pgrep -fl teleno_node
```

Only one process should own a given basedir. Stop unmanaged processes before
restore activation, compaction, migration, rollback, or direct CLI backup
creation.

## Public Bootstrap Diagnostics

List the configured public source:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config config/testnet-public-bootstrap-observer.yml \
  --backup-public-list \
  --backup-json
```

If public restore fails, check:

- HTTPS reachability;
- signature requirements and public key path;
- disk-space preflight;
- object hash mismatch errors;
- whether the selected backup ID exists.

## Producer Diagnostics

For producer mode, review the block producer log lines before accepting the
run:

- private key path;
- derived public address;
- configured producer address;
- production loop start;
- produced block height;
- repeated warnings from block production.

Unexpected producer address or key output is a stop condition.
