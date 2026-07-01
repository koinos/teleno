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

## Foreground Start

!!! warning "Local state write"
    Starting an observer writes local chain and runtime state under `$BASEDIR`.

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --log-level info \
  --disable block_producer
```

The node logs enabled features, thread topology, storage layout, indexing
status, P2P activity, metrics, and readiness.

## Background Start

Use your host service manager for long-running operation. For an ad hoc local
session, redirect logs explicitly:

```bash
mkdir -p "$BASEDIR/logs"
./node/teleno-node/build/teleno_node \
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

Observer P2P checks:

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
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --storage-report
```

Backups can be created through the local admin API while the node is running
when that API is enabled. Direct CLI backup creation is better treated as a
stopped-node or maintenance-window operation.
