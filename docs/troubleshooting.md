# Troubleshooting

Start with observer-safe checks. Do not clear chain data, delete state, or
enable production as the first response to a failure.

## Binary Not Found

Check repository build output:

```bash
ls -l node/teleno-node/build/teleno_node
```

If it is missing, build:

```bash
./scripts/build-cpp-libp2p-koinos.sh
```

For a focused rebuild:

```bash
cmake --build node/teleno-node/build --target teleno_node --parallel
```

## Wrong Config Or Basedir

Startup logs print the resolved paths:

```text
[node] basedir: ...
[node] config: ...
```

If the wrong config is loaded, stop the node and restart with explicit paths:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --disable block_producer
```

## JSON-RPC Does Not Answer

Check the listener:

```bash
lsof -nP -iTCP:18122 -sTCP:LISTEN
```

Check config and CLI override:

```yaml
jsonrpc:
  listen: 127.0.0.1:18122
```

Check the request:

```bash
curl -sS http://127.0.0.1:18122/ \
  -H 'content-type: application/json' \
  --data '{"jsonrpc":"2.0","id":1,"method":"chain.get_head_info","params":{}}'
```

If running in Docker, confirm the host publish rule includes loopback when
local-only access is intended:

```bash
-p 127.0.0.1:18122:18122
```

## No Peers Or No Sync Progress

Check:

- P2P feature is enabled.
- P2P listen port is not blocked when inbound connectivity is needed.
- Seed peers belong to the selected network.
- `p2p.peer-log-interval-seconds` is not `0` if you expect periodic snapshots.
- Logs show connection attempts, connected peers, or peer errors.
- `chain.get_head_info` height changes over repeated samples.

## Backup Dry Run Fails

Run:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-dry-run \
  --backup-json
```

Common causes:

- `backup.local.enabled=true` without `backup.local.directory`.
- Remote backup enabled without SSH settings.
- Missing private key, password, passphrase, or known-hosts file.
- Admin API enabled with an unreadable or empty token file.
- Non-absolute paths in an operator profile that expects absolute paths.

## Restore Fails Preflight

Do not retry by deleting the active DB. Check:

- selected backup ID;
- target basedir;
- free disk space;
- local repository path;
- remote SFTP credentials or public HTTPS URL;
- signature-required and public key path for public bootstrap;
- object hash or size mismatch messages.

Fetch public objects without activation to isolate transport issues:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config config/testnet-public-bootstrap-observer.yml \
  --backup-public-fetch \
  --backup-id latest \
  --backup-json
```

## Restore Activated But Node Is Not Producing

That is expected. Native restore forces observer-first recovery and disables
block production for the first start after restore. Start as an observer,
verify database and network health, then repeat the producer activation gate if
this basedir is intended to produce.

## Persistent Merkle Mismatch

If logs report `block previous state merkle mismatch` or another persistent
state merkle mismatch:

- Preserve the existing state DB.
- Do not clear `chain/blockchain`.
- Do not start from an empty state DB as the first action.
- Do not force a fresh full resync as the first action.
- Collect logs and run `--storage-report`.
- Attempt validation-based recovery first.
- Move or delete state only after explicit operator approval and evidence that
  recovery failed.

## Producer Key Or Address Looks Wrong

Stop immediately and return to observer mode:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --disable block_producer
```

The binary can create a missing producer hot key. A newly created key is not
proof of mainnet registration. Verify producer address, hot key, on-chain
registration, and VHP status before any production restart.

## Backup Admin API Returns Unauthorized

Check the token file configured at `backup.admin.token-file`. It must exist and
contain the same bearer token used by the caller:

```bash
export ADMIN_TOKEN="$(cat /absolute/path/to/admin.token)"
curl -sS http://127.0.0.1:18088/admin/backup/config \
  -H "authorization: Bearer $ADMIN_TOKEN"
```

Do not solve token errors by exposing the admin API or disabling protection.

## When To Rebuild

Rebuild the native binary when:

- source under
  [`node/teleno-node/`](https://github.com/pgarciagon/koinos-one/tree/main/node/teleno-node)
  changed;
- dependency build scripts changed;
- `teleno_node --help` does not include expected backup flags;
- packaged verification reports a missing or stale native binary.

Use:

```bash
cmake --build node/teleno-node/build --target teleno_node --parallel
```
