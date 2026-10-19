# Command Reference

## Direct Restore CLI

`--backup-restore-mode` selects `repository` (default) or `direct`. Direct mode
accepts exactly one action: `--backup-restore-preflight`,
`--backup-restore-stage`, or explicit `--backup-restore-activate`. All require a
reviewed existing config, separate `--backup-metadata-dir`, and a configured
public source or `--backup-public-url`. Stage/activation require a sibling owned
attempt via `--backup-output` and independently selected
`--backup-expected-chain-id`; expected identity remains optional for preflight.
`--backup-restore-resume` is only a stage modifier. Initial selection can resolve
`latest` once; resume/activation omit `--backup-id` or use the pinned concrete ID,
and explicitly reject `latest`. Activation re-authenticates pinned metadata and
uses read-only DB validation/exclusive rename, not a second payload cache.

Direct actions also accept `--backup-restore-reserve-profile standard` (default,
minimum 10 GiB) or `experimental-8gib` (minimum 8 GiB). Both retain the greater
20%-of-database term. The experimental profile must be explicit on every action
and is bound to the attempt; mismatched resume/activation is refused. It does
not change ordinary/repository defaults or establish a safe minimum. See the
[experimental profile](backup-restore-cli.md#experimental-reserve-profile).

`--backup-json` emits versioned fields; text labels the same fields. Exit 0 is
completion of the selected action, not producer readiness or 40 GB qualification;
2 is preflight target/space rejection, 1 failure/refusal, 130 cooperative
cancellation. An interrupted validating/pending attempt recovers through explicit
activation, never source replay. Source metadata must be available and unchanged.
Direct runtime admin, SFTP/fetch and automatic combined restore remain unsupported.
The common gate and unfinished-intent refusal protect ordinary startup. See
[workflow, recovery, schemas and limitations](backup-restore-cli.md#direct-stage-resume-and-explicit-activation-sc4).

## Performance Benchmarks

The transaction TPS harness is dry-run by default and consumes unique,
pre-signed JSONL transactions:

```bash
python3 scripts/benchmark-transaction-tps.py \
  --workload /path/to/workload.jsonl \
  --concurrency 8 \
  --target-tps 100 \
  --max-transactions 500
```

See [Transaction TPS Benchmark](performance/transaction-tps-benchmark.md) for
live-submission safety gates, inclusion verification, workload requirements,
and result interpretation.

These commands are validated against the current `teleno_node --help` surface
and current operations docs. Replace paths with local values before running.

## Binary

```bash
./build/teleno_node --version
./build/teleno_node --help
```

## Local Startup Preflight

```bash
./build/teleno_node --basedir "$BASEDIR" --config "$BASEDIR/config.yml" --preflight
./build/teleno_node --basedir "$BASEDIR" --config "$BASEDIR/config.yml" --preflight --preflight-json
```

Checks local prerequisites without writing files or starting services. Numeric
TCP ports are briefly bound without listening. Exit `0` means no known blocker
in completed checks; `1` means a failed check; `2` means an invalid combination
or inspection error. Warnings and unchecked requirements remain visible.
See [Local Startup Preflight](preflight.md) for coverage and limitations.

## Safe Observer Start

!!! warning "Local state write"
    Writes node state under `$BASEDIR`.

```bash
./build/teleno_node \
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
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-dry-run \
  --backup-json
```

List local backups:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-list \
  --backup-json
```

Create local backup:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-create-local \
  --backup-json
```

Public restore list:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config config/testnet-public-bootstrap-observer.yml \
  --backup-public-list \
  --backup-public-url https://testnet.koinosfoundation.org/backups/testnet/teleno-bootstrap \
  --backup-json
```

!!! danger "High-risk restore activation"
    The restore command stages and activates database contents under `$BASEDIR`.

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-restore \
  --backup-id latest \
  --backup-json
```

## Storage Commands

Print storage report:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --storage-report
```

!!! warning "High-risk storage mutation"
    Compaction changes local RocksDB files. Run with the node stopped.

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --compact-db \
  --all
```

To apply the configured compression policy to existing bottommost tables, add
`--compact-rewrite`. Ordinary compaction may leave those tables unchanged.
This explicit development option requires an existing `BASEDIR/db/CURRENT`
and exactly one selection: `--all` or valid `--compact-cf` names. Invalid
combinations fail before a storage lease or basedir is created. Other
maintenance modes cannot be combined with the rewrite.

Use a stopped, isolated copy on a larger build/export host with enough room
for the old and replacement tables. Compaction rewrites physical storage and
can temporarily increase disk use and CPU. It is not a way to make an
oversized archive fit in place on the 40-GB target. Create and verify a native
backup after the rewrite; preserve the original until recovery is qualified.
See the [native rewrite evidence](validation/native-archive-rewrite.md).

## Producer Start

!!! danger "High-risk production activation"
    This can sign and produce blocks when producer config is enabled and valid.

```bash
./build/teleno_node \
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
| `--compact-rewrite` | With `--compact-db`, explicitly rewrite existing bottommost tables using configured compression. | Offline storage mutation; requires an existing DB and one selection. |
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
| `--backup-restore-mode` | `repository` default or opt-in `direct`. | Direct is standalone POSIX CLI only. |
| `--backup-metadata-dir` | Separate direct metadata scratch. | Small owned temporary writes; not object cache. |
| `--backup-expected-chain-id` | Independently selected direct input identity. | Required for direct stage/activation. |
| `--backup-restore-resume` | Resume a direct stage with its pinned input. | Stage-only; no latest reselection or post-validation replay. |
| `--backup-restore-preflight` | Check restore readiness and disk space. | Read-oriented validation. |
| `--backup-restore-fetch` | Fetch private remote snapshot objects. | Local repository write. |
| `--backup-restore-stage` | Build restore staging directory. | Local staging write. |
| `--backup-restore-activate` | Activate staged restore. | High-risk restore activation. |
| `--backup-output` | Select checkpoint output or restore staging directory. | Path-sensitive. |
| `--backup-id` | Select snapshot ID or `latest`. | Selection-sensitive. |
| `--backup-json` | Emit JSON for backup modes. | Automation output. |
