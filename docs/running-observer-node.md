# Running Observer Nodes

An observer syncs, validates, stores, and serves chain data without producing
blocks. This is the normal mode for first-run setup, restore validation,
diagnostics, and public RPC backends.

## Start Checklist

- Binary path is known and verified with `--version`.
- Basedir is dedicated to the selected network.
- Config path is explicit.
- `features.block_producer` is false or `--disable block_producer` is present.
- JSON-RPC and gRPC exposure match the operator intent.
- P2P port is reachable if inbound peers are desired.
- Disk space is sufficient for sync or restore.
- Backup plan is validated if backup is enabled.
- On POSIX, the basedir parent's owner-only sibling storage gate is accessible
  and no other node/restore process holds it. Never delete its lock file to
  bypass exclusion.

Before starting, run the [local startup preflight](preflight.md) with the same
basedir, configuration and CLI overrides. Its report does not replace runtime
health verification below.

## Foreground Start

!!! warning "Local state write"
    Starting an observer writes local chain and runtime state under `$BASEDIR`.

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --log-level info \
  --disable block_producer
```

The node logs enabled features, thread topology, storage layout, indexing
status, P2P activity, metrics, and readiness. It enters ready state only after
startup indexing reaches the block-store head; cancellation or validation
failure stops startup and exits non-zero.

An unfinished direct-restore activation intent also refuses startup **before**
database initialization. Preserve the sole staged/target DB and its journal;
explicit activation recovery must finish the same verified generation through
SC4's standalone CLI, with pinned metadata still available. Never use transfer
resume once validation has begun. Neither local genesis/CLI smoke nor ordinary
startup preflight qualifies a 40 GB server. See
[Backup And Restore CLI](backup-restore-cli.md#direct-stage-resume-and-explicit-activation-sc4).

When the state database trails an existing block-store head, startup enters a
recovery indexing gate. Only the block store and chain controller run while
that backlog is validated. P2P, JSON-RPC, gRPC, mempool, and block production
remain stopped until indexing succeeds, so RPC health checks can refuse the
connection during this phase. Use the indexing and recovery-gate log lines for
progress and do not treat temporary RPC unavailability as a request to expose
or bypass a privileged endpoint.

Native LR2 development can additionally limit attributed parsed-module cache
retention with `chain.vm-module-cache-max-capacity-bytes`; default zero keeps
bytes unlimited and count 32. Oversized modules remain executable without
retention. Smaller budgets can increase parsing work; cache-only metrics are
not total RSS, catch-up proof or a qualified Linux/4-GB profile. Review
[configuration](configuration.md#vm-parsecache-lifetime-lr2-development) and
[V1-C evidence](validation/low-resource-lr2-vm-capacity.md) before selecting a
policy. This does not change restore trust, retained history or producer gates.

Native V1-D development also removes undefined unaligned host result-count
stores while preserving in-bounds byte addresses and overlapping argument/result
ordering. It adds no operator setting or resource-policy change. See
[host-memory evidence](validation/low-resource-lr2-host-memory.md); local
regressions do not qualify Linux whole-node capacity or historical parity.

## Background Start

Use your host service manager for long-running operation. For an ad hoc local
session, redirect logs explicitly:

```bash
mkdir -p "$BASEDIR/logs"
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --log-level info \
  --disable block_producer \
  >"$BASEDIR/logs/teleno_node.log" 2>&1 &
```

Record the process ID and stop it with `SIGINT` or `SIGTERM` so the node can
shut down cleanly.

## Verify Runtime Health

Check readiness in logs:

```bash
grep -F "[node] teleno_node ready" "$BASEDIR/logs/teleno_node.log"
```

Check local JSON-RPC:

```bash
curl -sS http://127.0.0.1:18122/ \
  -H 'content-type: application/json' \
  --data '{"jsonrpc":"2.0","id":1,"method":"chain.get_head_info","params":{}}'
```

Check that height advances over time:

```bash
for i in 1 2 3; do
  curl -sS http://127.0.0.1:18122/ \
    -H 'content-type: application/json' \
    --data '{"jsonrpc":"2.0","id":1,"method":"chain.get_head_info","params":{}}'
  sleep 30
done
```

## P2P Operation

The P2P component handles peer dialing, peer acquisition, Peer RPC sync, and
block/transaction gossip. The current binary logs connected-peer snapshots at
the configured `p2p.peer-log-interval-seconds` value. Set it to `0` to disable
periodic snapshots; connect/disconnect and handshake logs still appear.

Development LR2 replaces lifetime-growing accepted-ID sets with disposable
FIFO caches. Block IDs are limited to 4,096 entries / 2 MiB of owned string
capacity, transaction IDs to 65,536 / 8 MiB; expiry is 600 seconds, checked on
cache access or a stats/log snapshot. Hits do not renew age or FIFO order.
These internal defaults are not YAML/CLI settings. Tree/list nodes, allocators,
transport queues and decoded messages are additional memory, not those byte
counters. This is not a process RSS cap or a qualified 4-GB profile.

LIB removes old block cache entries; historical traffic at/below the current
LIB is ignored before populating the fork watchdog. Evicted live blocks still
go through the controller's existing idempotent validation path. A block-store
record alone is never new proof of validation. Fork-bomb checks for live blocks
remain active independently of cache eviction.

Exact current mempool-ID lookups suppress pending transaction duplicates,
including admission races; lookup failure is not treated as a known ID. An
evicted/expired transaction absent from the current mempool is validated again,
so a still-valid transaction can be readmitted after expiry/reorg. A rejected
unknown or no-longer-valid transaction follows ordinary peer scoring; there is
no permanent exemption for every historically seen transaction, generic nonce
exception, new transaction-history index or validation bypass. With mempool
disabled/unavailable, that exact pending-ID shortcut is unavailable.

Stop clears retained IDs and disconnects the owned EventBus subscriptions.
Normal peer snapshots include accepted-ID counts, owned ID-capacity bytes,
capacity evictions and expiry counters. See the
[scoped P2P report](validation/low-resource-lr2-p2p-retention.md) for evidence,
limitations and remaining whole-node memory work. All blocks and receipts in
the archive are retained unchanged.

Observer P2P checks:

Current LR2 development owns and joins disconnected sync generations too,
with an internal 64-worker/connected-state count limit. Reconnect can overlap
a cancelled old call only while that limit has room. Capacity closure has no
peer-error penalty; normal validation and fork detection remain unchanged.
Stop waits for all owned generations before stopping the transport. A pending
RPC/service can delay stop; no universal deadline or whole-node byte budget is
proven. See [the worker report](validation/low-resource-lr2-workers.md).

- listen address matches the intended network and host firewall;
- seed peers are valid for the selected network;
- logs show peer connection attempts or connected peers;
- `chain.get_head_info` height advances after peers connect.

## JSON-RPC Operation

JSON-RPC is the primary client surface for wallets, explorers, CLIs, and local
checks. Keep it local unless you intentionally run public infrastructure:

```yaml
jsonrpc:
  listen: 127.0.0.1:18122
```

When public JSON-RPC is intentional, put it behind explicit firewall, proxy,
rate-limit, and monitoring policy. Public JSON-RPC must not be confused with
the backup admin API; backup and restore controls are not exposed through
public JSON-RPC.

## Development memory admission

Standalone LR2 offers separate opt-in transaction-work job/input-wire limits
and pending-record owner/capacity limits. The latter counts immutable records
retained across forks and ancestors, not just visible-head entries; ordinary
expiry may not recover capacity until normal LIB progression. Local overload
is retryable, not a consensus failure or peer penalty; no accepted transaction
is silently evicted. Canonical validation/replay is not gated by these limits.
See [configuration](configuration.md#retained-pending-record-admission-lr2-development)
and [M1-G evidence](validation/low-resource-lr2-pending-records.md). Other memory
and cleanup headroom still need budgets; no values or low-resource defaults are
qualified from Mac tests. Linux VPS operation remains an unproven separate gate.

Native VM development also protects parsed-module ownership on allocation
failure and makes disposable cache insertion failure-atomic/optional. Cold
parsing is serialized per backend; warm hits remain independent and executions
use fresh mutable instances. Distinct cold IDs can incur extra latency. See
[V1-A evidence](validation/low-resource-lr2-vm-lifetime.md) and
[V1-B allocation correction](validation/low-resource-lr2-vm-allocation.md).
The pinned dependency now handles context-allocation failure, and native VM
allocation errors abort locally rather than becoming accepted reversions.
Parsed-byte/instance/queue bounds and representative Linux qualification remain
open. This does not qualify LR2 or
change archival history, replay validation, durability or producer activation.

## Stop Procedure

Stop the process gracefully:

```bash
kill -INT "$TELENO_NODE_PID"
```

Wait for:

```text
[node] teleno_node shutdown complete
```

Do not run restore activation, database compaction, or manual database moves
while another `teleno_node` process has the basedir open.

## Routine Maintenance

Run diagnostics with the node stopped unless the command is explicitly a
runtime API call:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --storage-report
```

Backups can be created through the local admin API while the node is running
when that API is enabled. Direct CLI backup creation is better treated as a
stopped-node or maintenance-window operation.
