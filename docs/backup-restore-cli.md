# Backup And Restore CLI

Native backup and restore are owned by `teleno_node`. The CLI can validate
backup config, create local snapshots, upload private SFTP backups, list
snapshots, fetch public bootstrap snapshots, restore, stage, activate, and
delete selected backups.

Implementation details live in
`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
`docs/current/backup-restore/PUBLIC_BOOTSTRAP_RESTORE.md`.

## Safety Rules

- Stop unmanaged node processes before direct CLI restore or restore
  activation.
- Use the local admin API for runtime backup operations only when it is
  intentionally enabled and token-protected.
- Keep backup admin endpoints on loopback.
- Never expose private SFTP credentials, SSH keys, password files, bearer
  tokens, wallet files, or producer private keys.
- Backups do not include producer private keys or wallet files.
- Restored nodes must start as observers first.
- Public bootstrap means public read-only source, not public admin control.

## Validate Backup Config

`--backup-dry-run` validates config without opening RocksDB or connecting to
SSH:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-dry-run
```

Machine-readable output:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-dry-run \
  --backup-json
```

## Create A Local Backup

!!! warning "Local state and backup write"
    This command opens the node database and writes a native snapshot to the
    configured local repository. Run it with no other unmanaged process using
    the same basedir, or use the local admin API when the running node owns the
    backup service.

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-create-local \
  --backup-json
```

Create the configured backup and upload to private SFTP when
`backup.remote.enabled=true`:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-create \
  --backup-json
```

## List Backups

Local repository:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-list \
  --backup-json
```

Private SFTP remote metadata:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-list-remote \
  --backup-json
```

Remote listing fetches only metadata into the local repository cache. It does
not download every object until selected fetch or restore.

## Public Bootstrap Restore

Public restore reads an HTTP(S) backup repository and writes into the local
native backup repository before staging and activation.

List public testnet metadata:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config config/testnet-public-bootstrap-observer.yml \
  --backup-public-list \
  --backup-public-url https://testnet.koinosfoundation.org/backups/testnet/teleno-bootstrap \
  --backup-json
```

Fetch metadata and missing objects without activation:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config config/testnet-public-bootstrap-observer.yml \
  --backup-public-fetch \
  --backup-public-url https://testnet.koinosfoundation.org/backups/testnet/teleno-bootstrap \
  --backup-id latest \
  --backup-json
```

!!! danger "High-risk restore activation"
    This command stages and activates a restored database under `$BASEDIR`. The
    previous DB/runtime paths are preserved under `.pre-restore/`, and the next
    node start must remain observer-first.

Restore public testnet bootstrap:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-public-restore \
  --backup-public-url https://testnet.koinosfoundation.org/backups/testnet/teleno-bootstrap \
  --backup-id latest \
  --backup-json
```

If the selected config path does not exist, public restore writes an
observer-safe config for the selected network. Review the generated config
before starting long-running operation.

## Restore From Local Or Private SFTP Backup

Preflight local restore:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-restore-preflight \
  --backup-id latest \
  --backup-json
```

Fetch private SFTP remote snapshot into the local repository:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-restore-fetch \
  --backup-id latest \
  --backup-json
```

!!! danger "High-risk restore activation"
    `--backup-restore` fetches remote data when enabled, runs preflight, stages,
    activates, and exits. Run it only after confirming the basedir, network,
    selected backup ID, and available disk space.

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-restore \
  --backup-id latest \
  --backup-json
```

After restore, start as an observer:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --disable block_producer
```

## Delete Backups

Delete is dry-run by default:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-delete \
  --backup-id "$BACKUP_ID" \
  --backup-scope local
```

!!! danger "High-risk backup deletion"
    The confirmation value must exactly match the selected backup ID. The value
    `latest` is intentionally rejected for deletion.

Execute deletion:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-delete \
  --backup-id "$BACKUP_ID" \
  --backup-delete-confirm "$BACKUP_ID" \
  --backup-scope local
```

Use `--backup-scope remote` or `--backup-scope both` only after verifying
private SFTP settings and retention policy.

## Local Admin API

The backup admin API is optional, loopback-only, and bearer-token protected. It
is meant for local orchestration while the node is running. See
[RPC Endpoints](rpc-endpoints.md) for endpoint and exposure details.

## Restore Verification

After restore:

1. Confirm restore output reports success.
2. Confirm `.pre-restore/` contains preserved previous runtime paths when
   applicable.
3. Start with `--disable block_producer`.
4. Wait for `[node] teleno_node ready`.
5. Query `chain.get_head_info`.
6. Confirm head height advances.
7. Only then consider producer checks, if this basedir is intended for
   production.
