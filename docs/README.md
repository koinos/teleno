# Teleno Node CLI Guide

This section documents the native `teleno_node` binary for operators who run it
outside the Koinos One GUI. `teleno_node` is a community-driven monolithic
rewrite of the Koinos node; the official reference implementation remains the
microservices-based stack. It is written for observer-first operation: start
safe, verify the node, then enable higher-risk behavior only after the required
checks pass.

## Pages

- [Install Or Build](install-or-build.md) - build, locate, or run the native
  binary.
- [Quickstart](quickstart.md) - start a safe observer node and verify RPC.
- [Configuration](configuration.md) - config files, profiles, basedirs,
  feature flags, and ports.
- [Running Observer Nodes](running-observer-node.md) - normal observer runtime
  operations.
- [Running Producer Nodes](running-producer-node.md) - carefully gated producer
  runtime operations.
- [Backup And Restore CLI](backup-restore-cli.md) - native backup, restore,
  public bootstrap, and deletion workflows.
- [RPC Endpoints](rpc-endpoints.md) - JSON-RPC, gRPC, P2P, and local admin API
  exposure rules.
- [Logs And Diagnostics](logs-and-diagnostics.md) - logs, readiness checks,
  storage reports, and diagnostic commands.
- [Command Reference](command-reference.md) - validated command snippets and
  option notes.
- [Troubleshooting](troubleshooting.md) - failure modes and recovery-first
  actions.

## Safety Model

Use observer mode until the node proves it can follow the selected network.
Producer mode is optional and must be enabled only after database health,
network identity, peer health, producer address, VHP, and producer-key checks
pass.

Commands in this guide are labeled when they write local state, write config,
delete backup data, activate restores, or enable block production. Any mainnet
producer action, transaction signing/submission, VHP burn, producer
registration, default-account change, or config write targeting a producer is
high-risk and requires a fresh explicit operator decision.

Public bootstrap restore means a public read-only backup source. It does not
mean public administrative access. Backup admin endpoints are local-only and
bearer-token protected.

## Deep References

The manual keeps workflows concise. For implementation details, start with
these rendered manual references:

- [Current Monolithic Node Architecture](../developers/deeper-references/monolith-architecture.md)
- [Monolith Service Coverage](../developers/deeper-references/monolith-service-coverage.md)

Additional engineering source paths outside the manual source tree:

- [`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md`](https://github.com/koinos/koinos-one/blob/main/docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md)
- [`docs/current/backup-restore/PUBLIC_BOOTSTRAP_RESTORE.md`](https://github.com/koinos/koinos-one/blob/main/docs/current/backup-restore/PUBLIC_BOOTSTRAP_RESTORE.md)
- [`docs/operations/START_TELENO_NODE.md`](https://github.com/koinos/koinos-one/blob/main/docs/operations/START_TELENO_NODE.md)
- [`docs/operations/TELENO_NODE_CONTAINER.md`](https://github.com/koinos/koinos-one/blob/main/docs/operations/TELENO_NODE_CONTAINER.md)

## Normal Workflow

1. Build or locate `teleno_node`.
2. Create or select a basedir.
3. Start as an observer with JSON-RPC bound to loopback unless public RPC is
   intentional.
4. Verify logs, head info, peers, and disk usage.
5. Configure backup and restore paths before relying on the node.
6. Consider producer mode only after the producer checklist passes.
