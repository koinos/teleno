# Command Reference

These commands are validated against the current `teleno_node --help` surface
and current operations docs. Replace paths with local values before running.

## Binary

```bash
./node/teleno-node/build/teleno_node --version
./node/teleno-node/build/teleno_node --help
```

## Safe Observer Start

!!! warning "Local state write"
    Writes node state under `$BASEDIR`.

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --jsonrpc-listen 127.0.0.1:18122 \
  --log-level info \
  --disable block_producer grpc
```

## JSON-RPC Check

```bash
curl -sS http://127.0.0.1:18122/ \
  -H 'content-type: application/json' \
  --data '{"jsonrpc":"2.0","id":1,"method":"chain.get_head_info","params":{}}'
```

## Backup Commands

Validate backup config:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-dry-run \
  --backup-json
```

List local backups:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-list \
  --backup-json
```

Create local backup:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-create-local \
  --backup-json
```

Public restore list:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config config/testnet-public-bootstrap-observer.yml \
  --backup-public-list \
  --backup-public-url https://testnet.koinosfoundation.org/backups/testnet/teleno-bootstrap \
  --backup-json
```

!!! danger "High-risk restore activation"
    The restore command stages and activates database contents under `$BASEDIR`.

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-restore \
  --backup-id latest \
  --backup-json
```

## Storage Commands

Print storage report:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --storage-report
```

!!! warning "High-risk storage mutation"
    Compaction changes local RocksDB files. Run with the node stopped.

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --compact-db \
  --all
```

## Producer Start

!!! danger "High-risk production activation"
    This can sign and produce blocks when producer config is enabled and valid.

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --log-level info
```

Use only after the producer activation gate in
[Running Producer Nodes](running-producer-node.md).

## CLI Options

| Option | Purpose | Risk note |
| --- | --- | --- |
| `--help`, `-h` | Print help. | Read-only. |
| `--version`, `-v` | Print binary version. | Read-only. |
| `--basedir`, `-d` | Select data directory. | Affects where state is read/written. |
| `--config`, `-c` | Select config file. | Affects all runtime behavior. |
| `--log-level`, `-l` | Override log level. | Runtime-only. |
| `--enable` | Enable one or more components. | High-risk if enabling `block_producer` or public RPC surfaces. |
| `--disable` | Disable one or more components. | Useful for safe observer mode. |
| `--jobs`, `-j` | Override chain worker threads. | Runtime tuning. |
| `--p2p-listen` | Override P2P listen address. | Exposure-sensitive. |
| `--jsonrpc-listen` | Override JSON-RPC listen address. | Exposure-sensitive. |
| `--storage-report` | Print storage metadata and exit. | Read-oriented diagnostic. |
| `--migrate-chain-db-to-unified-rocksdb` | Migrate legacy chain state. | High-risk storage mutation. |
| `--rollback-unified-chain-db-migration` | Roll back preserved legacy chain DB. | High-risk storage mutation. |
| `--require-rocksdb-compression` | Fail if configured compression is unsupported. | Startup validation. |
| `--compact-db` | Compact shared RocksDB. | High-risk storage mutation. |
| `--compact-cf` | Select column families for compaction. | High-risk storage mutation. |
| `--all` | Compact all column families with `--compact-db`. | High-risk storage mutation. |
| `--backup-dry-run` | Validate backup config. | Read-only validation. |
| `--backup-checkpoint` | Create RocksDB checkpoint. | Backup write. |
| `--backup-create` | Create configured local backup and optional remote upload. | Backup write and possible SFTP write. |
| `--backup-create-local` | Create local object-store backup. | Backup write. |
| `--backup-upload-latest` | Upload latest local snapshot to private SFTP. | Remote write. |
| `--backup-list` | List local snapshots. | Read-only inventory. |
| `--backup-list-remote` | Fetch remote metadata and list snapshots. | Reads remote, writes local metadata cache. |
| `--backup-public-list` | Fetch public metadata and list snapshots. | Reads public source, writes local metadata cache. |
| `--backup-public-fetch` | Fetch public metadata and missing objects. | Local repository write. |
| `--backup-public-restore` | Fetch, preflight, stage, and activate public restore. | High-risk restore activation. |
| `--backup-public-url` | Override public bootstrap source URL. | Source selection. |
| `--backup-delete` | Plan or delete selected backup. | High-risk when confirmed. |
| `--backup-scope` | Select `local`, `remote`, or `both` for delete. | Deletion scope. |
| `--backup-delete-confirm` | Execute delete when it matches `--backup-id`. | High-risk deletion. |
| `--backup-restore` | Restore configured local/private backup. | High-risk restore activation. |
| `--backup-restore-preflight` | Check restore readiness and disk space. | Read-oriented validation. |
| `--backup-restore-fetch` | Fetch private remote snapshot objects. | Local repository write. |
| `--backup-restore-stage` | Build restore staging directory. | Local staging write. |
| `--backup-restore-activate` | Activate staged restore. | High-risk restore activation. |
| `--backup-output` | Select checkpoint output or restore staging directory. | Path-sensitive. |
| `--backup-id` | Select snapshot ID or `latest`. | Selection-sensitive. |
| `--backup-json` | Emit JSON for backup modes. | Automation output. |
