# Logs And Diagnostics

`teleno_node` writes operational logs to stdout/stderr. The logs are the first
place to verify config, feature flags, storage, RPC listeners, P2P progress,
backup events, and shutdown behavior.

## Useful Log Lines

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
| `[node] teleno_node ready` | Runtime is ready for normal operation. |
| `[metrics] head_height=...` | Periodic head, peer, mempool, and RSS metrics. |
| `[node] teleno_node shutdown complete` | Clean shutdown completed. |

## Capture Logs

!!! warning "Local log write"
    This writes a log file under the selected basedir.

```bash
mkdir -p "$BASEDIR/logs"
./node/teleno-node/build/teleno_node \
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

## Storage Report

`--storage-report` opens storage, prints layout metadata and RocksDB column
family estimates, then exits:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --storage-report
```

Use it to confirm the shared DB path, chain storage layout, compression
selection, and column family estimates.

## Backup Diagnostics

Validate backup config without opening RocksDB or connecting to SSH:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-dry-run \
  --backup-json
```

List local snapshots without opening RocksDB:

```bash
./node/teleno-node/build/teleno_node \
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
./node/teleno-node/build/teleno_node \
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
