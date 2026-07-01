# RPC Endpoints

`teleno_node` exposes public Koinos RPC surfaces and, when enabled, a separate
local backup admin API. Treat them as different security boundaries.

## Endpoint Summary

| Surface | Default | Default state | Intended exposure |
| --- | --- | --- | --- |
| P2P | `/ip4/0.0.0.0/tcp/8888` | enabled | Public node networking. |
| JSON-RPC | `0.0.0.0:8080` | enabled | Local by config override unless public RPC is intentional. |
| gRPC | `0.0.0.0:50051` | disabled | Local or protected infrastructure only. |
| Backup admin API | `127.0.0.1:18088` | disabled | Local-only and bearer-token protected. |

The default JSON-RPC listener is broad. Operator configs should usually set
`jsonrpc.listen` to `127.0.0.1:<port>` or pass `--jsonrpc-listen`.

## JSON-RPC

Loopback JSON-RPC config:

```yaml
jsonrpc:
  listen: 127.0.0.1:18122
```

CLI override:

```bash
--jsonrpc-listen 127.0.0.1:18122
```

Health query:

```bash
curl -sS http://127.0.0.1:18122/ \
  -H 'content-type: application/json' \
  --data '{"jsonrpc":"2.0","id":1,"method":"chain.get_head_info","params":{}}'
```

JSON-RPC routes public Koinos methods into chain, block store, mempool,
transaction store, contract meta store, and account history when those
components are enabled. It does not expose native backup or restore admin
methods.

## gRPC

gRPC is disabled by default:

```yaml
features:
  grpc: false
```

If enabled, bind it intentionally:

```yaml
grpc:
  listen: 127.0.0.1:50051

features:
  grpc: true
```

Do not enable public gRPC without an explicit access-control and monitoring
plan. Current implementation notes and parity caveats are tracked in
`docs/current/monolith/SERVICE_COVERAGE.md`.

## P2P

P2P uses libp2p transport when available. Public P2P listening is normal for a
node:

```yaml
p2p:
  listen: /ip4/0.0.0.0/tcp/8888
```

For local testnet or multiple local nodes, choose a separate port:

```yaml
p2p:
  listen: /ip4/0.0.0.0/tcp/18888
```

P2P logs peer connection activity and periodic connected-peer snapshots at
`p2p.peer-log-interval-seconds`.

## Backup Admin API

The backup admin API is local-only and token-protected. It starts only when
enabled in `backup.admin`:

```yaml
backup:
  admin:
    enabled: true
    listen: 127.0.0.1:18088
    token-file: /absolute/path/to/admin.token
    jobs: 1
```

The token file must exist and be non-empty. Startup fails closed when the
configured token file cannot be read.

Check local health:

```bash
curl -sS http://127.0.0.1:18088/health
```

Call a protected endpoint:

```bash
export ADMIN_TOKEN="$(cat /absolute/path/to/admin.token)"
curl -sS http://127.0.0.1:18088/admin/backup/config \
  -H "authorization: Bearer $ADMIN_TOKEN"
```

Implemented local admin routes include:

| Route | Purpose |
| --- | --- |
| `GET /health` and `GET /healthz` | Local health checks. |
| `GET /admin/backup/config` | Sanitized backup config. |
| `GET /admin/backup/snapshots/local` | Local snapshot inventory. |
| `GET /admin/backup/snapshots/remote` | Private SFTP remote inventory. |
| `POST /admin/backup/create` | Create backup through the running node. |
| `POST /admin/backup/upload-latest` | Upload latest local snapshot to private SFTP. |
| `POST /admin/backup/delete` | Delete selected local/private snapshot. |
| `GET /admin/backup/status` | Operation status summary. |
| `POST /admin/backup/cancel` | Cancel cancellable operations. |
| `POST /admin/backup/restore/*` | Fetch, preflight, stage, or activate private restore. |
| `GET /admin/backup/public/*` | Public bootstrap config and snapshot listing. |
| `POST /admin/backup/public/*` | Public bootstrap fetch, preflight, stage, or activate. |

Do not publish this API, proxy it publicly, or share bearer tokens. Public
bootstrap restore uses public backup objects, not public admin endpoints.

## Docker Exposure

Inside a container, a service may need to bind to `0.0.0.0` so Docker can
publish it. Keep host exposure scoped:

```bash
-p 127.0.0.1:18122:18122
```

Publishing without the host IP, such as `-p 18122:18122`, can expose JSON-RPC
on all host interfaces.
