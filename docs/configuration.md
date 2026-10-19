# Configuration

`teleno_node` reads one YAML config file and then applies CLI overrides. The
default basedir is `~/.koinos`; the default config path is
`<BASEDIR>/config.yml`.

## Basedir

The basedir owns node data:

```text
<BASEDIR>/
  config.yml
  db/
  chain/
  block_producer/
  .teleno-native-backups/
```

Important rules:

- Use one basedir per network and role.
- Stop the node before moving, restoring, or manually editing database paths.
- Do not reuse a producer basedir as an observer test sandbox.
- Backups do not include wallet files or producer private keys.

## Config Selection

Run with an explicit basedir and config path:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml"
```

If `--config` is omitted, the binary reads `$BASEDIR/config.yml`. If `--basedir`
is omitted, it uses `~/.koinos`.

## Config Profiles

The repository includes operator-oriented templates:

| Template | Purpose |
| --- | --- |
| [`config/testnet-public-bootstrap-observer.yml`](../config/testnet-public-bootstrap-observer.yml) | Testnet observer with public bootstrap settings. |
| [`config/testnet-public-bootstrap-observer.container.yml`](../config/testnet-public-bootstrap-observer.container.yml) | Container testnet observer variant. |
| [`config/mainnet-public-bootstrap-observer.yml`](../config/mainnet-public-bootstrap-observer.yml) | Mainnet observer with public bootstrap settings. |
| [`config/prodnet-docker-producer.yml`](../config/prodnet-docker-producer.yml) | Guarded Docker producer template. |

!!! warning "High-risk config write"
    Copying or editing a config file writes local operational policy. Review
    network, ports, feature flags, backup paths, and producer fields before
    starting the node.

Create a working config copy when you need a persistent local profile:

```bash
cp config/testnet-public-bootstrap-observer.yml "$BASEDIR/config.yml"
```

## Feature Flags

Feature flags live under `features:` and can also be overridden with
`--enable` and `--disable`.

Default feature state in the current binary:

| Feature | Default |
| --- | --- |
| `chain` | enabled |
| `mempool` | enabled |
| `block_store` | enabled |
| `p2p` | enabled |
| `jsonrpc` | enabled |
| `grpc` | disabled |
| `block_producer` | disabled |
| `contract_meta_store` | enabled |
| `transaction_store` | enabled |
| `account_history` | disabled |

Use CLI disables for safe observer starts:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --disable block_producer grpc
```

## Core Sections

Minimal observer config shape:

```yaml
global:
  log-level: info
  fork-algorithm: pob

chain:
  verify-blocks: true
  jobs: 2

p2p:
  listen: /ip4/0.0.0.0/tcp/18888
  peer-log-interval-seconds: 60

jsonrpc:
  listen: 127.0.0.1:18122

features:
  chain: true
  mempool: true
  block_store: true
  p2p: true
  jsonrpc: true
  grpc: false
  block_producer: false
```

Common sections:

| Section | Purpose |
| --- | --- |
| `global` | log level, fork algorithm, global jobs, RPC allow/deny lists. |
| `chain` | verification, worker count, resource and pending transaction limits. |
| `mempool` | native in-memory pending-record owner/capacity admission policy. |
| `p2p` | listen address, seeds, peer discovery, peer logging, checkpoints. |
| `jsonrpc` | JSON-RPC listen address and worker count. |
| `grpc` | gRPC listen address and worker count when enabled. |
| `block_producer` | producer hot key, producer address, resource bounds, production policy. |
| `backup` | native backup, local repository, SFTP, public restore, scheduler, admin API. |
| `rocksdb` | cache, compression, block size, WAL flush trigger, and background job tuning. |
| `features` | component enablement. |

`chain.jobs` selects the number of workers used by the dedicated startup
indexing `io_context`. The default is `2`; `1` is supported and retains bounded
nonblocking backpressure for block-store responses larger than the consumer
queue.

### Transaction execution-work admission (LR2 development)

Two opt-in `chain` settings bound simultaneous standalone transaction submissions:

```yaml
chain:
  transaction-work-max-jobs: 0
  transaction-work-max-wire-bytes: 0
```

Each axis defaults to `0` (unlimited); both zero disable reservation accounting.
Values must be unsigned decimal integers; invalid values fail configuration
loading even outside strict preflight. No qualified 4-GB profile values are
selected yet. Choose them only from representative Ubuntu/x86_64 measurements,
not Mac fixtures. The byte axis is the sum of protobuf-serialized transaction
inputs currently executing, **not allocated heap or a process-RAM ceiling**.

Reservation precedes structural validation, the controller's transaction copy,
state overlay and execution. It remains held through native admission and
accepted-work notification, and releases on return or failure. Dry runs and
disabled-mempool submissions still consume work. There is no waiting queue or
automatic retry. Local overload is a retryable refusal, not consensus invalidity:
JSON-RPC returns `-32005` with local/retryable error data, gRPC returns
`RESOURCE_EXHAUSTED`, and P2P does not penalize or cache the refused transaction.

Canonical block validation/replay is not subject to this standalone-submission
gate. RPC/P2P input decoding occurs before it; returned receipts, pending
responses, parsed VM instances and retained mempool forks remain additional.
See [M1-E scope and evidence](validation/low-resource-lr2-transaction-work.md).
This does not close aggregate mempool or whole-node admission requirements.

### Retained pending-record admission (LR2 development)

Two independent opt-in limits cover live immutable serialized pending records:

```yaml
mempool:
  pending-record-max-owners: 0
  pending-record-max-capacity-bytes: 0
```

Both default to zero/unlimited; both zero disable accounting, not owner sharing.
Values require unsigned decimal integers even outside strict preflight. A record
is encoded once per admission and its immutable owner is shared across eligible
forks, clones, ordinary squash and in-memory LIB publication. Limits count unique
live owners, including retained ancestors and external C++ holders, not visible
head transaction count. Equal bytes from independent admissions are not deduped.

Reservation charges the encoded-size lower bound before encoding, then actual
`std::string::capacity()` before publishing any fork. Failure releases credits;
local exhaustion is retryable JSON-RPC `-32005` with `pending_record_capacity`,
or gRPC `RESOURCE_EXHAUSTED`, not consensus invalidity or P2P misconduct. No
accepted work is evicted, queued or automatically retried. Head expiry may leave
the budget full until normal LIB/pruning or last-owner release. Canonical block
validation/replay and cleanup do not acquire new pending-record credits.

This bounds only the specified record owners/reservations. Map keys, indexes,
RC/nonce records, tombstones, containers/control blocks, input protobufs and
encoding/growth workspace are additional; capacity counts include inline string
storage and are not heap/RSS totals. In particular an encoding buffer may grow
above its initial wire-size reservation before growth is refused. Persistent
backends copy through their existing write/batch path; no format/durability or
history policy changes. Select any VPS values from representative Linux
whole-node measurements, not Mac diagnostics. See [M1-G evidence](validation/low-resource-lr2-pending-records.md).
Complete aggregate/cleanup-headroom and whole-node qualification remain open.

### JSON-RPC memory and lifecycle policy (LR2 development)

`jsonrpc.jobs` retains its dual worker/session limit (default `4`, `0` maps to
one, maximum `64`). Sessions now use owned asynchronous sockets, not detached
threads. `jsonrpc.io-timeout-seconds` is `30` by default and accepts `1..300`.
It bounds each complete HTTP read and write, including idle keepalive and a
partially sent request; expiry closes the connection. Healthy keepalive requests
remain supported. Request body/header limits remain 1 MiB / 8 KiB. Excess
connections receive a best-effort HTTP 503 and close without another worker.

Stop closes accepted sockets and drains handlers before releasing dependencies.
An already-admitted service call runs to completion: a disconnect is not a
rollback, and the I/O timeout is not a consensus execution timeout. Shutdown can
therefore wait on the underlying service; deadlines require worker progress.
There is no new response-size limit or claim that all RPC service outputs have
a proven byte budget. Bind RPC deliberately; this does not change exposure.

The [R1-A follow-up](validation/low-resource-lr2-rpc-bodies.md) destroys completed
request bodies after service execution and response bodies after the write
callback, instead of retaining an already-sent response during idle keepalive.
Cancellation keeps an in-flight write's body alive until completion. Responses
remain complete, buffered pipeline requests survive and no new setting is added.
Native C++ body-capacity attribution excludes partial ingress, JSON/protobuf/
service/serialization work and transport buffers; it is not a memory limit.

The [R1-B follow-up](validation/low-resource-lr2-rpc-batch.md) serializes/releases
individual batch result trees into the full HTTP body and moves results into
pre-dispatch success envelopes. It adds no setting, response truncation or
post-service capacity rejection. Final response capacity and service/input/
transport workspace remain additional, not a proven aggregate byte ceiling.

### Experimental block compression policy

Issue #9 development adds two opt-in settings for the `blocks` column family.
Both default to `0`, preserving existing compression options. The 64-KiB
data-block default, other families, record encoding and durability are unchanged.

| Setting under `rocksdb` | Accepted values | Effect |
| --- | --- | --- |
| `blocks-zstd-level` | `0` or `1..19` | `0` uses the linked Zstd default; a nonzero value selects the level for new block tables, including bottommost compaction. |
| `blocks-zstd-dictionary-bytes` | `0`, `32768`, `65536`, `131072` | `0` retains the library's no-dictionary default; a nonzero value permits a dictionary up to that size per table. |

Values must be unsigned integers. Invalid scalars, ranges and non-Zstd codec
combinations fail config loading. With either setting nonzero, the storage
manager also requires actual linked Zstd support before creating its DB
directory; it refuses codec fallback even when `require-compression` is false.
Read-only preflight validates the configuration but reports linked codec support
as `not_checked`; successful preflight does not establish runtime support.

The following experimental example uses the profile measured in the
[lossless archive experiment](validation/lossless-archive-storage-experiment.md):

```yaml
rocksdb:
  blocks-compression: zstd
  blocks-zstd-level: 9
  blocks-zstd-dictionary-bytes: 65536
  max-background-jobs: 1
  require-compression: true
```

Dictionary training samples up to 1 MiB from an 8-MiB buffer per active table
builder. The dictionary can be smaller or absent when insufficient useful
samples are available. Compression workspaces, concurrent builders and other
node allocations are additional; these values are not a hard RSS bound.
The sampled VPS rewrite uses about 85% more CPU than the matching level-3
control; workload CPU, I/O and latency still need measurement.

These settings affect future flushes/compactions. Opening a database does not
rewrite its existing tables. Existing Zstd-capable readers can read the encoded
tables without these settings; future writes use that reader's own configured
policy. Keep the selected profile in the active config after restore/restart.
These settings alone do not create a compact backup or qualify a 40-GB server.
Development [`--compact-rewrite`](command-reference.md) explicitly rewrites
existing bottommost tables when used with `--compact-db` and one family
selection. It needs a stopped copy and temporary disk capacity. Generate a
native backup afterward; see the [rewrite evidence](validation/native-archive-rewrite.md)
and earlier [policy validation](validation/native-block-compression-policy.md).

### Retained WAL and the disk budget

`rocksdb.max-total-wal-size` optionally selects a WAL flush trigger in bytes.
The default `0` keeps the linked library's automatic policy. Nonzero values are
soft triggers for normal flushing and WAL reclamation, not hard WAL/disk limits.
They do not disable WAL, weaken synchronization, prune blocks or remove receipts.

For example, `max-total-wal-size: 67108864` selects a 64-MiB trigger. Five
isolated VPS pairs observe approximately 75% less peak retained WAL, with a
small write-CPU increase and faster synced-WAL process-exit recovery. Those
are key/value mechanism measurements, not a qualified node profile. Earlier
flushing can change CPU, I/O, compaction and latency. Preserve operational and
recovery headroom and measure the complete workload. See the
[WAL validation report](validation/wal-retention-budget.md).

The aggregate memtable setting does not bound retained WAL files. Zero's
automatic WAL policy derives from individual family buffers and counts; the
linked RocksDB 8.8.1 low-memory example with nine families, 8-MiB buffers and
two buffers per family derives a 576-MiB trigger. RocksDB reclaims WAL files
after safe flushing; retain every WAL needed for recovery.

This setting requires the new standalone native build; older binaries may
ignore an unknown YAML key. Check native identity, the `storage.wal` preflight
observation and the saved runtime options. Preflight does not open the DB or
measure actual WAL usage. No complete 40-GB restore or producer qualification
follows from selecting this setting.

### RocksDB memory accounting

Existing defaults remain: shared block cache 256 MiB, aggregate memtable flush
threshold 256 MiB, per-CF buffer 64 MiB, three buffers, four background jobs and
Zstd where supported. The linked RocksDB 8.8.1 `db_write_buffer_size` already
applies across all CFs in this one DB. It triggers flushes; it is not a hard RSS
ceiling. A shared WriteBufferManager for the same DB would provide equivalent
flush tracking unless additional stall policy were selected; no new manager,
strict cache admission or durability change is introduced here.

`--storage-report` adds cache capacity/usage/pinned usage, aggregate threshold,
all-memtable and table-reader estimates. The shared cache is counted once;
pinned usage is a subset, not extra memory to add. Missing estimates are
`unavailable`, not zero. This offline report opens the DB; stop its owner first.
It does not measure runtime RSS, page cache, VM, queues or total host memory.

The separate state-object cache now retains at most its 64-MiB **payload**
budget, not a heap/RSS budget. Immutable read owners follow values through
consumption; oversized values bypass cache retention without clearing unrelated
warm entries. A zero-byte internal/test budget retains no entries. Map/list/key/
string overhead and caller-held or evicted values are additional. Backend
constructor budgets are internal APIs, not new CLI/YAML settings. Parsed WASM
cache retention has a separate optional capacity setting described below. See the
[LR2 report](validation/low-resource-lr2-memory.md) for remaining bounds and
qualification gates. These fixes do not select or qualify a 4-GB profile.

State map clones share immutable value payloads, with independent map entries
and replacement-on-write. Existing DB/node locks remain required. Native C++
state read APIs now return owners rather than raw borrowed pointers; consumers
must retain those results until done. This changes neither serialized DB formats
nor roots, RPC schemas, retention or durability. See
[state ownership evidence](validation/low-resource-lr2-state-ownership.md),
including the measured small-value cost and still-required Linux qualification.

### VM parse/cache lifetime (LR2 development)

The native backend retains at most 32 parsed modules. Optional
`chain.vm-module-cache-max-capacity-bytes` additionally limits the cache's
**attributed retained capacity**, not total heap or RSS. Default `0` leaves
bytes unlimited while retaining the 32-entry limit. The setting is a strict
unsigned 64-bit integer; malformed, negative and overflowing values fail config
loading. No low-resource profile/default is selected by Mac measurements.

The charge includes the pinned immutable parsed-module object and all nested
vector/string capacities, its guard and indexed key/value storage. Small-string
capacity is conservatively counted, including terminators. Allocator, tree/list
node and shared-control-block overhead, temporary parsing/staging, queued callers,
evicted caller-held modules and active mutable instances are additional.
This accounting depends on the exact pinned Fizzy layout and must be reviewed
when that dependency changes. See [V1-C evidence](validation/low-resource-lr2-vm-capacity.md).

Entries that cannot fit or be accounted without overflow bypass disposable
retention and remain owned/executable. A new oversized entry does not flush
unrelated useful entries; an oversized replacement invalidates its old same-ID
owner to avoid stale reuse. Reducing the budget can increase parsing CPU and
latency. It does not reject a valid execution, change metering or prune history.

Cold parses now serialize per backend and recheck the cache; retained same-ID
callers share an immutable module. Warm hits do not wait for the parser lock.
The lock is released before execution/host callbacks, and mutable instances
remain independent. Parse/shared-owner allocation failure frees the module;
cache-retention allocation failure preserves existing entries and uses the
owned module without retention. Parse/instance/host failures still propagate.

Only cache capacity has the above YAML setting; this is not a complete profile
or global RSS cap. Distinct
cold IDs serialize too; waiting callers and active instances remain additional
memory. [V1-B](validation/low-resource-lr2-vm-allocation.md) corrects the pinned
dependency's terminating context-allocation path in all native build recipes.
Parse/instance/context allocation failure aborts the attempt as a local error,
not a consensus reversion. This is not an admission/RSS or machine-OOM guarantee.
See also [V1-A evidence](validation/low-resource-lr2-vm-lifetime.md).
Choose the complete policy from representative Linux workloads; Mac fixtures
do not qualify the VPS or establish throughput improvements.

### P2P sync-worker lifecycle (LR2 development)

LR2 development also owns all P2P sync-worker generations until joined,
including disconnected peers. The internal `P2POptions` limit is 64 simultaneous
owned generations (including retired work) and 64 connected peer states;
it is not a YAML/CLI option or a low-resource profile. Capacity closes excess
connections without adding an error score. Disconnect cancels further work at
RPC/block boundaries; already-started service work is not rolled back. Stop
waits for outstanding calls before releasing transport/service dependencies.
No universal cancellation/shutdown deadline or hard worker-memory byte cap is
claimed. See [worker evidence](validation/low-resource-lr2-workers.md).

### Outgoing Peer RPC lifecycle (LR2 development)

The internal native client retains six-second open/write phases and now applies
one six-second deadline to the **complete response**, without renewing it for
fragments. An additional 19-second external wait handles a blocked I/O runner;
synchronous DNS resolution precedes that wait and remains separately unbounded.
The existing logical response ceiling is 64 MiB. These are internal transport
policies, independent of `jsonrpc.io-timeout-seconds`, with no new YAML control
or low-resource profile selected here.

Stop retires the client generation and cancels its calls before closing
connections. A local lifecycle/receive-capacity failure does not add peer error
scores. On receive-capacity failure during block fetch, later ordinary sync
iterations reduce that peer generation's requested count, down to one, before
any block from the failed batch is applied. Protocol validation and the complete
history requirement are unchanged. A single oversized block remains a local
capacity failure. Aggregate queues/SDK/decoded/caller memory, admitted synchronous
server work, full lifecycle concurrency and target-host performance remain open.
See [P2-C evidence](validation/low-resource-lr2-p2p-client.md).

### Explicit native dial admission (LR2 development)

The concrete cpp-libp2p transport has internal positive limits of 32 explicit
native dial owners and 1 MiB charged peer-string capacity. Startup seeds share
this path. This is not a YAML/CLI/GUI setting or qualified VPS profile. Job
credits follow preparation, queued work and retained callbacks across cancellation
and restart; they return only when the last owner releases them. Capacity
refusal remains local without peer blame.

Charged retained/provisional peer strings exclude copy workspace, parsed/DNS/SDK
state, native known-peer caches, implicit GossipSub/RPC/DHT/inbound dials, control queues
and caller memory. Synchronous DNS and full SDK cancellation remain open. See
[P2-E evidence](validation/low-resource-lr2-p2p-dials.md) for the exact contract
and current validation status.

### Native peer advice and snapshots (LR2 development)

Concrete C++ configuration retains at most 1,024 optional advice records / 1 MiB
peer-string capacity. Oldest unprotected advice can be discarded; already-retained
startup seeds, DHT bootstrap roots and connected records are protected. Authoritative configuration
and active metadata survive optional cache refusal. SDK stores remain separate.

Known/connected native projections share four simultaneous caller/constructing
owners, 4,096 aggregate vector slots and 4 MiB attributed vector/string capacity.
The move-only result retains credits until its storage dies, including across
transport restart. Queries use locked native observations rather than external
SDK snapshots. Scalar connected count allocates no projection. Local saturation
can temporarily fail a metadata query; it neither truncates a successful result
nor scores peers. Caller copies, active/SDK metadata, construction workspace and
allocator/RSS totals are additional. These positive internal settings add no
YAML/CLI/GUI controls or qualified VPS profile. See
[P2-F evidence](validation/low-resource-lr2-peer-snapshots.md).

### SDK address retention and collection (LR2 development)

The concrete native transport additionally uses an SDK-compatible address store
with internal limits of 1,024 peers, 8,192 addresses, 8,192 retained vector slots,
4 MiB of address/peer-ID encoding lengths and 64 addresses per peer. One shared
policy spans runtime generations; retained old repositories keep their credits.
Each address has one owned representation rather than duplicate expiry-map and
ordered-vector copies. Preparation finishes before optional peer eviction and
publication, so a failed allocation preserves the preceding address state.

Already-retained configured roots and live connections are protected from
optional eviction. Capacity bypass does not alter authoritative configuration,
penalize peers or truncate a successful query. Add/upsert TTL refresh, address
order, failed-dial rotation and DNS bootstrap remain distinct behaviors.
An owned timer collects expired addresses, empty protocol sets and closed
connections every 60 seconds; stop retires and cancels that generation.

These are positive C++ `Libp2pTransport::Config` settings, without new YAML,
CLI or GUI controls. Encoding lengths and vector slots are not physical heap
or RSS. Staging/index/allocator overhead, inherited DNS history, other SDK
stores/queues and caller copies remain additional. See
[P2-G status and measurements](validation/low-resource-lr2-sdk-addresses.md).
The development policy is not a qualified whole-node VPS profile.

### Peer-error bookkeeping (LR2 development)

Native YAML `p2p.max-peer-error-records` defaults to `4096` and
`p2p.peer-error-key-capacity-bytes` to `1048576`. Both must be positive integers;
zero does not disable these limits. They bound retained score records and the
sum of retained key string capacities, including small-string capacity. Map
nodes, caller input, SDK/connection queues and allocator overhead are additional.
They are not a qualified low-resource profile or GUI/CLI controls.

Scores use the first network address component rather than changing ports or
trailing peer IDs. IP-wide scoring can group clients sharing a NAT. Admission
projects decay from the last score-changing event, saturates additions and
preserves every nonzero score. Bounded maintenance removes only disposable zero
scores. Full capacity or allocation failure refuses new unknown bookkeeping
locally, without peer penalties; existing records remain evaluable. Legitimate
new and seed addresses can also be refused until capacity becomes available.

Optional candidate advice is trimmed after all mutations, including when
discovery is disabled. Configured seeds and connected candidates stay protected;
the effective count limit is the larger of the configured limit and protected
count. Its existing zero limit means unlimited. Candidate string capacity is
reported, not byte-bounded. See [P2-D evidence](validation/low-resource-lr2-peer-bookkeeping.md)
for scope, reference-policy differences and unfinished aggregate gates.

### Chain verification behavior

`chain.verify-blocks: true` re-executes every indexed block and remains the
maximum-validation mode. `chain.verify-blocks: false` replays stored receipt
deltas, but the fast path is still checked: block H remains pending until H+1
provides the consensus-signed expectation for H's state-delta root. A receipt
that cannot reproduce that expectation is discarded while still writable and
the block is re-executed exactly once. Any mismatch after re-execution stops
indexing.

The repository profiles continue to select `verify-blocks: true`. After a
native restore, `.backup-just-restored` establishes the persistent
`.backup-observer-recovery` production hold. The hold keeps block production
disabled across restarts without overriding the explicitly configured
verification mode. After the observer validation gate passes, a deliberate
restart with `--enable block_producer` releases the hold. The first startup
after restore cannot release it.

## Ports And Listeners

Current binary defaults:

| Surface | Default | Exposure note |
| --- | --- | --- |
| P2P | `/ip4/0.0.0.0/tcp/8888` | Public listener is normal for node networking. |
| JSON-RPC | `0.0.0.0:8080` | Bind to `127.0.0.1` unless public RPC is intentional. |
| gRPC | `0.0.0.0:50051` | Disabled by default; bind tightly if enabled. |
| Backup admin API | `127.0.0.1:18088` | Local-only and bearer-token protected when enabled. |

Prefer explicit loopback binding for local RPC:

```bash
--jsonrpc-listen 127.0.0.1:18122
```

For Docker, the service may bind to `0.0.0.0` inside the container while the
host publish rule remains loopback-only:

```bash
-p 127.0.0.1:18122:18122
```

## Backup Config

Native backup is configured under `backup:`. A local-only repository example:

```yaml
backup:
  enabled: true
  node-id: testnet-observer-1
  workspace: /absolute/path/to/basedir/.teleno-native-backups/workspace
  local:
    enabled: true
    directory: /absolute/path/to/basedir/.teleno-native-backups/repository
    retention-count: 7
```

Validate backup config before relying on it:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-dry-run
```

## Config Change Verification

### Direct restore source policy

Opt-in standalone `--backup-restore-mode direct` uses the reviewed config's
`backup.public-restore`: `enabled`, `base-url`, expected `network`,
`require-https`, `signature-required`, `signature-public-key-file`, finite
`retries` (at most ten) and `timeout-seconds` (1–300). An explicit
`--backup-public-url` selects the public read-only source without weakening the
remaining policy. Credentials/query/fragment URLs are unsupported. Keep trusted
verification keys outside snapshot payloads. `--backup-expected-chain-id` is
required for stage/activation and is not a new persistent config setting.

Direct mode never generates, imports or overwrites active configuration, never
starts runtime/admin services, and rejects runtime feature overrides. Metadata
scratch, target and attempt must be disjoint; attempt and target are siblings on
one filesystem with an accessible common owner-only gate. Resume/activation must
retain the original source, network, HTTPS/signature requirements, chain identity
and pinned metadata. The default repository mode and GUI-generated profiles are
unchanged. See [Direct Restore CLI](backup-restore-cli.md#direct-stage-resume-and-explicit-activation-sc4).

### Runtime checks

After any config change:

1. Review network, basedir, ports, and feature flags.
2. Run `--backup-dry-run` if backup settings changed.
3. Start with `--disable block_producer` unless production is intentionally
   being activated.
4. Check logs for config path, enabled features, listeners, and readiness.
5. Query `chain.get_head_info` through the intended JSON-RPC address.
