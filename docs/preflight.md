# Local Startup Preflight

Run the native preflight before starting a CLI-managed node:

```bash
./build/teleno_node \
  --basedir /path/to/node-data \
  --config /path/to/config.yml \
  --preflight
```

Add `--preflight-json` for a machine-readable report. An existing configuration
file is required, even when the intended basedir does not exist yet. Run the
check as the same user and inside the same container/service environment as
the intended node. To check an observer override, add `--disable block_producer`;
use that same override when actually starting the observer.

The command does not create directories, write files, open RocksDB, generate
keys, process restore activation, contact peers, or start services. Numeric TCP
listeners are temporarily bound, without listening or sending traffic, then
released before exit. These probes can briefly reserve configured ports. All
probe sockets are retained together to detect conflicts between configured
listeners. Do not use this command as a health check for an already running
node: occupied listener ports will fail.

## Report and Exit Codes

Each check has a stable `id`, `status`, `observed` description and suggested
`action`. JSON schema version 1 includes `scope`, `no_known_blockers`,
`runtime_readiness_verified` (always false), `exit_code` and `checks`.

| Status | Meaning |
| --- | --- |
| `pass` | The specifically described observation succeeded. |
| `warning` | Review the observation; it does not prove a startup failure. |
| `fail` | A known local blocker or unsafe normal-start condition was found. |
| `not_checked` | This check cannot establish the stated requirement. |
| `error` | Required inspection or command processing could not complete. |

| Exit code | Meaning |
| --- | --- |
| `0` | No known blocker in completed checks; warnings and unverified requirements may remain. |
| `1` | At least one check failed. |
| `2` | Invalid preflight command combination or inspection error, without a reported failed check. |

Standard CLI parsing errors (for example, an unknown option or malformed CLI
number) are handled before the report and may produce the normal stderr error
instead of JSON. `--help` and `--version` retain their usual precedence.

A zero exit code is **not** proof that the node can synchronize, sustain load,
or safely produce blocks. Results are a point-in-time observation; permissions,
free space and port ownership may change before startup.

## Checks in This Version

- Strict types for recognized scalar configuration values, YAML section shapes,
  selected string lists, feature names and feature booleans. The ordinary
  startup parser retains its existing permissive behavior.
- Effective observer/producer selection after CLI feature overrides, reset
  detection, fork-algorithm recognition and producer resource bounds.
- OS access checks on the data directory or nearest existing ancestor and
  existing storage subdirectories, plus free bytes on the base volume.
- The configured automatic/explicit WAL flush trigger is reported as
  `storage.wal: not_checked`. It is a soft policy; actual retention, runtime
  application, flush costs and complete disk capacity are not measured.
- Reported logical CPUs, host physical memory, configured block cache, and the
  process open-file soft limit. These are observations, not benchmark results.
- Bind probes for enabled P2P numeric `/ip4/.../tcp/...` or `/ip6/.../tcp/...`,
  JSON-RPC, gRPC and backup admin listeners. RPC exposure is flagged;
  non-loopback backup administration fails this local-only profile.
- Producer key existence, readability and permission bits, using the startup
  path-resolution rules. Key contents and configured producer addresses are
  never included in the report. No key is generated or matched on chain.

Unsupported endpoint syntax and hostname resolution are reported as unchecked.
Port zero is allowed but flagged because it selects an ephemeral port. Bind
probes do not test inbound reachability through a firewall or NAT.

## Explicit Limits

This version does not validate every YAML key, every component dependency,
network/genesis identity, RocksDB compression availability, existing database
integrity, lock ownership, recovery markers, available RAM, container/cgroup
resource limits, CPU quotas, or sustainable throughput. A LOCK file on disk is
not treated as evidence of a running instance. No arbitrary minimum RAM or
disk capacity is asserted. Space on separate child mounts is not budgeted.
Directory access checks do not replace an actual write/fsync test.

Capacity planning must include chain growth, compaction, backup retention and
restore staging. Use `--backup-restore-preflight` separately for a particular
local backup's restore sizing, and `--backup-dry-run` for backup planning.
Preflight cannot be combined with backup or maintenance command modes.

After completing prerequisite checks, follow [observer validation](running-observer-node.md).
Producer operation additionally requires the [producer activation gate](running-producer-node.md).
