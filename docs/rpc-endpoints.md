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

LR2 native development adds owned sessions and
`jsonrpc.io-timeout-seconds: 30` (range `1..300`). Idle/partial reads and blocked
writes expire; keepalive works within that deadline. `jobs` remains the worker
and active-session limit (default four; maximum 64); excess connections get a
best-effort 503 then close. Stop drains sessions and already-admitted service
work before releasing dependencies. It does not forcibly interrupt execution or
roll back a submitted request on disconnect. See
[configuration](configuration.md#json-rpc-memory-and-lifecycle-policy-lr2-development).

[R1-A](validation/low-resource-lr2-rpc-bodies.md) additionally releases the request
after dispatch and the response after completed writing, while preserving full
responses, pipelining and healthy keepalive. Stop/cancellation does not release a
body still borrowed by a write or undo admitted service work. This selects no
output-size limit or Linux profile and adds no endpoint/configuration setting;
service/JSON/serialization and aggregate output admission remain open.

The current M1 development repair also empties owned request/response JSON trees
without allocating during cleanup. A reproduced allocation failure after
transaction acceptance previously aborted in a `noexcept` JSON destructor.
The scoped OVH regression now closes the failed response, preserves the
accepted call and allows a later successful call. A disconnected response does
not imply that transaction acceptance was rolled back. This repair adds no RPC
method, changes no successful envelope/encoding and is not yet a qualified
whole-node release. See the [JSON cleanup evidence](validation/low-resource-lr2-m1-json-cleanup.md).

The unreleased M1 HTTP refusal repair uses a separate 64-MiB shared gate for
conservative response-workspace permits when ordinary admission is full.
Actual response allocations stay attributed to M1 until their last owner
releases them; gate permits are policy credits, not a second physical-memory
measurement. Original IDs and local retryable error fields are preserved.
If that gate is full, the connection closes without executing refused work.
Earlier accepted batch work remains accepted even if no complete response can
be delivered. This development path still requires final Linux qualification;
it is not a GUI setting or a selected whole-node memory profile.

The same development work preserves resource exceptions during base58 input
normalization: a capacity or allocation refusal must not be treated as an
alternative encoding and deliver changed native address bytes. Normal format
fallback and successful encodings remain unchanged. JSON dispatch also avoids
redundant encoded-input copies. Its scoped Linux checks and paired process
results do not establish whole-node resource limits.

[R1-B](validation/low-resource-lr2-rpc-batch.md) serializes one batch result at a
time into the complete retained HTTP body, without keeping all earlier result
trees or copying returned trees into success envelopes. Response bytes/order
are preserved. Strict invalid-UTF8 serialization still executes later batch
calls before closing without partial output; disconnect does not imply rollback.
This is not network streaming or an output-size reservation. The final body and
simultaneous service/input/transport memory still require aggregate policy.

## Native transaction admission (LR2 development)

In current native LR2 development, enabled mempool insertion finishes before
accepted-transaction observers are notified. Ordinary insertion refusal now
propagates to the submitting controller instead of being caught/logged as a
successful bridge return. A post-insertion observer failure is counted/logged
without retrying already-retained work. RPC schemas and disabled-mempool
notification-only behavior are unchanged; this is not a new aggregate overload
policy or complete memory bound. See [the checkpoint](validation/low-resource-lr2-admission.md).

The [M1-C follow-up](validation/low-resource-lr2-admission-atomicity.md) corrects
partial mempool insertion after C++ allocation failure across mutable forks.
All fallible write/reset preparation precedes publication; debug-log failure
after publication does not report rejection for retained work. Per-fork nonce
conflicts and resource rules are unchanged. This limited write-only map batch
does not establish atomicity for block/LIB/prune/removal failures, aggregate
capacity, peer overload classification or a global memory limit.

The [M1-E follow-up](validation/low-resource-lr2-transaction-work.md) adds
opt-in pre-execution standalone transaction-work count/input-wire-byte limits
under `chain`. Both default to zero/unlimited. Exhaustion returns a JSON-RPC
error `-32005`, preserving the request ID, with
`data: {retryable: true, reason: "transaction_work_capacity", scope: "local"}`;
it is not a transaction receipt or consensus rejection. gRPC uses
`RESOURCE_EXHAUSTED`. A client may retry later with its own bounded/backoff
policy; the node does not queue or automatically retry refused work. P2P does
not assign misconduct score or an accepted-ID cache entry for this refusal.
Normal validity failures retain their existing classification. Configuration
does not change endpoints or protocol schemas. Returned-output, ingress and
retained-fork memory remain outside this work budget; Linux profile selection
and whole-node qualification are still pending.

The [M1-G follow-up](validation/low-resource-lr2-pending-records.md) additionally
shares immutable pending records across eligible forks and provides opt-in
live-record owner/capacity limits under `mempool`. Local refusal uses the same
JSON-RPC `-32005`/retryable/local contract with reason `pending_record_capacity`,
and the same gRPC/P2P handling. No receipt/result is returned on refusal and no
partial insertion/accepted notification follows. Expiry can empty the visible
head while ancestors still hold charged records. Those limits exclude other
mempool structures and encoding workspace; they are not a complete memory cap
or qualified VPS profile. Defaults remain unlimited; endpoints are unchanged.

## Development receipt completeness

The native development block-store path completes a previously missing
`state_merkle_root` when the validated controller publishes an otherwise
identical accepted receipt. Existing blocks, execution fields, roots and topology
are preserved; contradictory completion is refused. This is not historical
archive repair or an independent proof of the root. The correction has component
evidence, with Linux/whole-node and producer update acceptance still pending.
See [the receipt-completion checkpoint](validation/block-receipt-completion.md).

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
plan. Current implementation notes and parity caveats are tracked in the
[Monolith Service Coverage](https://github.com/koinos/koinos-one/blob/main/docs/manual/developers/deeper-references/monolith-service-coverage.md)
reference.

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

Native development bounds optional peer advice and known/connected projections
with internal C++ policies. Queries read native published metadata; they are not
a complete SDK address/connection inventory. Projection credits follow the
returned storage, including across transport restarts. Capacity refusal is local
and does not score peers or return a truncated successful result. Periodic
discovery waits its normal interval before another attempt.

The protected `GET /admin/p2p/peers` provider uses those projections. A provider
failure uses the existing HTTP 500 error response with `ok: false`; formatting,
serialization and caller copies remain outside the projection ledger. The
route's loopback/token protection is unchanged. See the
[P2-F contract and exclusions](validation/low-resource-lr2-peer-snapshots.md).

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
