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
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml"
```

If `--config` is omitted, the binary reads `$BASEDIR/config.yml`. If `--basedir`
is omitted, it uses `~/.koinos`.

## Config Profiles

The repository includes operator-oriented templates:

| Template | Purpose |
| --- | --- |
| [`config/testnet-public-bootstrap-observer.yml`](https://github.com/pgarciagon/koinos-one/blob/main/config/testnet-public-bootstrap-observer.yml) | Testnet observer with public bootstrap settings. |
| [`config/testnet-public-bootstrap-observer.container.yml`](https://github.com/pgarciagon/koinos-one/blob/main/config/testnet-public-bootstrap-observer.container.yml) | Container testnet observer variant. |
| [`config/mainnet-public-bootstrap-observer.yml`](https://github.com/pgarciagon/koinos-one/blob/main/config/mainnet-public-bootstrap-observer.yml) | Mainnet observer with public bootstrap settings. |
| [`config/prodnet-docker-producer.yml`](https://github.com/pgarciagon/koinos-one/blob/main/config/prodnet-docker-producer.yml) | Guarded Docker producer template. |

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
./node/teleno-node/build/teleno_node \
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
| `p2p` | listen address, seeds, peer discovery, peer logging, checkpoints. |
| `jsonrpc` | JSON-RPC listen address and worker count. |
| `grpc` | gRPC listen address and worker count when enabled. |
| `block_producer` | producer hot key, producer address, resource bounds, production policy. |
| `backup` | native backup, local repository, SFTP, public restore, scheduler, admin API. |
| `rocksdb` | cache, compression, block size, and background job tuning. |
| `features` | component enablement. |

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
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-dry-run
```

## Config Change Verification

After any config change:

1. Review network, basedir, ports, and feature flags.
2. Run `--backup-dry-run` if backup settings changed.
3. Start with `--disable block_producer` unless production is intentionally
   being activated.
4. Check logs for config path, enabled features, listeners, and readiness.
5. Query `chain.get_head_info` through the intended JSON-RPC address.
