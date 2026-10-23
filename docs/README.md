# Teleno Node CLI Guide

This section documents the native `teleno_node` binary for operators who run it
outside the Koinos One GUI. `teleno_node` is a community-driven monolithic
rewrite of the Koinos node; the official reference implementation remains the
microservices-based stack. It is written for observer-first operation: start
safe, verify the node, then enable higher-risk behavior only after the required
checks pass.

## Pages

- [Native 1.3.0 release notes](releases/1.3.0.md) - M1 memory admission and
  Fogata P2P lifetime corrections, exact-build checks and remaining issue-9 gates.
- [Fogata P2P Lifetime Correction](validation/fogata-gossip-publication.md) -
  reproduced publication and listener defects, isolated checks and limits.
- [Native 1.3.0-alpha.2 Release Notes](releases/1.3.0-alpha.2.md) -
  exact correction scope and experimental release boundaries.
- [Native 1.3.0-alpha.2 publication](validation/alpha-1.3.0-alpha.2-publication.md) -
  exact clean source, Linux ELF, same-binary image, tests and remaining gates.
- [Native 1.3.0-alpha.1 publication](validation/alpha-1.3.0-publication.md) -
  exact clean source, Linux executable, container, normal tests and remaining gates.
- [Native 1.3.0-alpha.1 release notes](releases/1.3.0-alpha.1.md) -
  detailed accumulated updates, selected VPS/mainnet evidence and remaining
  experimental qualification limits.
- [Linux VM Cache Policy](validation/linux-vm-cache-policy.md) -
  retained-capacity, CPU and latency tradeoffs with distinct valid VM programs;
  reference-VPS and whole-node qualification remain separate.
- [State Cache Key Ownership Experiments](validation/state-cache-key-ownership-prototypes.md) -
  two external layouts, paired memory/CPU/latency results and working component
  diagnostics; native integration and physical cache bounds remain open.
- [Current Container Runtime Checks](validation/current-container-runtime.md) -
  seven isolated cases with the frozen native binary and an existing image;
  repository image build and release packaging remain open.
- [Controlled Native Producer](validation/controlled-native-producer.md) -
  actual producer/controller/shared-storage construction and clean reopen on
  an isolated federated chain; PoB and full producer qualification remain open.
- [Fogata Producer Heap-Corruption Investigation](validation/fogata-gossip-publication.md) -
  preserved 2026-10-21 failure, reproducible GossipSub publication race and
  isolated candidate checks; historical-crash attribution and final qualification
  remain open.
- [Retained WAL Disk Budget](validation/wal-retention-budget.md) -
  isolated repeated retention/recovery comparisons and the optional native
  flush trigger; complete disk and runtime qualification remain open.
- [Direct Restore Validation Scan Reuse](validation/direct-restore-scan-reuse.md) -
  retained ownership locks and a fresh proof avoid one duplicate scan; final
  validation and full qualification remain required.
- [Native Block Compression Policy](validation/native-block-compression-policy.md) -
  opt-in persistent Zstd level/dictionary settings, default-reader compatibility
  and scoped native tests; full restore and whole-node gates remain open.
- [Direct Restore Read Buffer](validation/direct-restore-read-buffer.md) -
  bounded transport reads and paired CPU/write measurements on Linux builder/VPS.
- [Reference VPS Restore and Observer](validation/reference-vps-observer.md) -
  completed full restore, short catch-up/restart, legacy agreement and the
  remaining strict whole-device disk shortfall.
- [Linux RSS Counter](validation/linux-rss-counter.md) -
  resident-versus-virtual reporting correction and independent memory checks.
- [Historical Checkpoint Eligibility](validation/historical-checkpoint-eligibility.md) -
  independently anchored starting state, exact isolated copy and current native
  boundary checks; full-range differential qualification remains open.
- [Historical Source Bridge](validation/historical-source-bridge.md) -
  controlled read-only protobuf transport and paired Linux full-VM receipt comparisons.
- [Optional Larger Block Dictionary](validation/larger-block-dictionary.md) -
  bounded 128-KiB setting, five repeated lossless comparisons and CPU/I/O tradeoffs.
- [Lossless Archive Storage Experiment](validation/lossless-archive-storage-experiment.md) -
  full-size byte equivalence, complete structural continuity and Linux CPU/read
  tradeoffs; actual 40-GB restore and whole-node operation remain unqualified.
- [LR2 SDK Address Retention](validation/low-resource-lr2-sdk-addresses.md) -
  Current address ownership, expiry collection, bounded retention and paired
  resource measurements; whole-node qualification remains open.
- [LR2 Peer Advice and Owned Snapshots](validation/low-resource-lr2-peer-snapshots.md) -
  bounded optional advice, caller-held projection credits and a reproduced
  snapshot/stop race; SDK/active metadata and aggregate gates remain open.
- [LR2 Explicit Native Dial Ownership](validation/low-resource-lr2-p2p-dials.md) -
  reproduced stale admission, finite retained dial owners and scoped failure/
  generation checks; implicit SDK and aggregate transport gates remain open.
- [LR2 Peer Bookkeeping](validation/low-resource-lr2-peer-bookkeeping.md) -
  network-address scores, bounded retained error records and failure-safe optional
  candidates; current component evidence and unfinished aggregate gates.
- [Complete-Backup Restore Within 40 GB](validation/restore-40gb-current-baseline.md) -
  corrected public checkpoint identity and actual-VPS native reserve refusal;
  the whole-server restore remains required and unqualified.
- [LR2 Outgoing Peer RPC Ownership](validation/low-resource-lr2-p2p-client.md) -
  reproduced complete-response deadline failure, owned client cancellation,
  local capacity handling and adaptive read-only batches; full P2 remains open.
- [LR2 Fragmented Peer RPC Reads](validation/low-resource-lr2-p2p-reads.md) -
  owned incremental framing, constant-stack skipping and stop/restart checks;
  aggregate transport and whole-node qualification remain open.
- [LR2 Complete Peer RPC Writes](validation/low-resource-lr2-p2p-writes.md) -
  reproduced short-write truncation, unchanged complete frames and scoped
  callback ownership/error checks; transport capacity and Linux gates stay open.
- [Contract ABI Index Recovery Proposal](implementation/contract-abi-index-recovery-plan.md) -
  missing KFS ABI observations, source findings and a proposed metadata-only
  repair; no recovery command or live maintenance implemented by this work.
- [LR2 Host Memory and Diagnostic Startup](validation/low-resource-lr2-host-memory.md) -
  reproduced unaligned production writes, order-preserving correction and
  standalone sanitizer startup attribution; Linux/whole-node gates remain open.
- [LR2 RPC Batch Working Set](validation/low-resource-lr2-rpc-batch.md) -
  per-item complete-body serialization, exact ordered failure behavior and
  measured allocation reduction; aggregate/Linux qualification remains open.
- [LR2 Completed RPC Body Lifetimes](validation/low-resource-lr2-rpc-bodies.md) -
  reproduced idle-response retention, portable owner cleanup and scoped
  capacities/fixture measurements; aggregate output and Linux gates remain open.
- [LR2 Local VM Allocation Failure](validation/low-resource-lr2-vm-allocation.md) -
  pinned dependency correction, local allocation-error classification and
  actual build/fault evidence; not whole-machine OOM or Linux qualification.
- [LR2 VM Lifetime and Cold Parsing](validation/low-resource-lr2-vm-lifetime.md) -
  parsed-owner/cache failure safety, coalesced cold-load evidence and unresolved
  dependency/parsed-byte/Linux gates; not whole-node qualification.
- [Mempool Copy Ownership](validation/mempool-admission-copy-ownership.md) -
  allocation-failure leak reproduction, arena cleanup and candidate validation.
- [Protobuf Unknown-Field Ownership](validation/protobuf-unknown-field-ownership.md) -
  pinned runtime publication correction, controlled Linux diagnostics and current
  native build evidence; full copy/merge and whole-node qualification remain open.
- [Source-Export Diagnostic Provenance](validation/source-export-provenance.md) -
  verified-manifest SC0 collection without Git metadata, unchanged native inputs
  and explicit fresh regression scope.
- [LR2 Pending-Record Ownership](validation/low-resource-lr2-pending-records.md) -
  shared immutable payloads and opt-in lifetime-owned reservations; other
  consumers and Linux whole-node qualification remain open.
- [LR2 Failure-Atomic Cleanup](validation/low-resource-lr2-cleanup-atomicity.md) -
  reproduced partial expiry/removal, checked decoding and prepared publication;
  not aggregate capacity, persistent-commit or Linux qualification.
- [LR2 Transaction-Work Reservations](validation/low-resource-lr2-transaction-work.md) -
  opt-in pre-execution count/wire admission and local-overload classification;
  not aggregate fork or whole-node Linux memory qualification.
- [LR2 Retained Fork Inventory](validation/low-resource-lr2-retained-memory.md) -
  shared-owner/key/tombstone accounting, empty-head versus retained ancestry,
  and scoped diagnostic costs; not total heap or an admission cap.
- [LR2 Failure-Atomic Admission](validation/low-resource-lr2-admission-atomicity.md) -
  multi-fork allocation-failure reproduction/correction and scoped allocation
  measurements; not an aggregate admission or Linux memory qualification.
- [LR2 State Read Ownership](validation/low-resource-lr2-state-ownership.md) -
  reproduced read-eviction lifetime fault, immutable owners/clone sharing,
  bounded retained payload and scoped measurements, including regressions.
- [LR2 Completion Matrix](validation/low-resource-lr2-completion-matrix.md) -
  active goal, remaining admission/lifetime/resource gates and external data policy.
- [M1 Aggregate-Memory Repair](validation/low-resource-lr2-m1-repair.md) -
  finite C3/C4 source, test and measurement evidence with explicit remaining gates.
- [M1 Shorter-Predecessor Read](validation/low-resource-lr2-m1-nonce-read.md) -
  reproduced nonce/count read overflow, frozen C9 correction and scoped Linux
  qualification; M1 and whole-node qualification remain open.
- [M1 Isolated Physical-VPS Qualification](validation/low-resource-lr2-m1-isolated-vps.md) -
  closed frozen-C9 full-backup/observer sequence, incomplete observation,
  ownership coverage audit and metric reproduction; M1 remains open.
- [M1 Memory-Optimization Objective](implementation/issue-9-m1-aggregate-goal.md) -
  application allocation/retention optimization, bounded aggregate admission,
  exclusive OVH tests, physical measurements and unchanged completion gates.
- [M1 R15 Final-Candidate Qualification](validation/low-resource-lr2-m1-r15.md) -
  completed scoped M1 gate, exact source/build identities, native/sanitizer
  results, five pairs per measured workload and full-history observer/recovery.
- [M1 JSON Cleanup](validation/low-resource-lr2-m1-json-cleanup.md) -
  reproduced post-acceptance allocation abort, allocation-free cleanup and
  14/14 scoped normal OVH development checks; final qualification remains open.
- [Legacy Applicability of Teleno Findings](implementation/legacy-applicability-of-teleno-findings.md) -
  source-supported cross-architecture hypotheses and bounded reproduction plans;
  no legacy service failure is claimed without a matching test.
- [LR2 Native Admission](validation/low-resource-lr2-admission.md) -
  refusal propagation, notification ordering and accurate pending diagnostics;
  global memory limits and Linux qualification remain open.
- [LR2 Sync Worker Ownership](validation/low-resource-lr2-workers.md) -
  disconnected-worker reproduction, cancellation/count limits and actual local tests.
- [LR2 P2P Accepted-ID Retention](validation/low-resource-lr2-p2p-retention.md) -
  bounded disposable caches, duplicate/fork tests and isolated memory tradeoffs.
- [LR2 Memory Hardening](validation/low-resource-lr2-memory.md) -
  local cache/RPC fixes, resource measurements and outstanding whole-node gates.
- [LR1 Source And Persistence Prerequisites](validation/low-resource-lr1-prerequisites.md) -
  backup-writer correction, storage/publication tests and blocked mainnet source gate.
- [LR0 Full-History Resource Baseline](validation/low-resource-lr0-baseline.md) -
  verified local measurement tooling, actual failing baseline and qualification blockers.
- [SC6 Single-Copy Restore Review Handoff](validation/single-copy-restore-sc6-handoff.md) -
  supported development contract, blocking gates and unexecuted release sequence.
- [SC5 Single-Copy Restore Measurements](validation/single-copy-restore-sc5-capacity.md) -
  synthetic comparison, bounded-image rejection and missing qualification gates.

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
- [Release Builds](release-builds.md) - native release build identity and
  versioning.
- [Transaction TPS Benchmark](performance/transaction-tps-benchmark.md) -
  safe, reproducible transaction admission and block-inclusion throughput
  measurement.
- [TPS Optimization Assessment](performance/tps-optimization-assessment.md) -
  measured bottlenecks, current-architecture optimization opportunities, and
  the boundary where parallel execution would require a redesign.
- [Database And Backup Size Reduction Assessment](performance/database-size-reduction-assessment.md) -
  production storage measurements, bounded compression experiments, snapshot
  sharing, and archival or bounded-history producer designs.
- [Bounded-History Producer Design](implementation/bounded-history-producer-design.md) -
  deferred research into limited local history, durable pruning, restart and
  peer compatibility; not the active full-history Teleno requirements.
- [Chain v1.5.2 Fast Replay Correctness Implementation Plan](implementation/chain-v1.5.2-fast-replay-plan.md) -
  implementation and validation work for tombstone-preserving checked replay,
  one-block lookahead, one-time re-execution, and the exact historical
  state-root exception.
- [Chain v1.5.2 Fast Replay Release Checklist](release-chain-v1.5.2-fast-replay-checklist.md) -
  authorization-gated observer canary, native/container publication, consumer
  handoff, and producer rollout commands.
- [iOS and Android Producer Feasibility and Implementation Plan](implementation/teleno-mobile-feasibility-and-implementation-plan.md) -
  native PoB producer port, minimal app with external funds custody, individual
  publisher requirements, store constraints, and implementation/release gates.
  [Spanish translation](implementation/teleno-mobile-feasibility-and-implementation-plan.es.md).
- [Archived Session Context](memoria.md) - point-in-time decisions,
  verified node comparison, pending Telegram draft, and implementation gates.
- [Single-Copy Restore Plan](implementation/single-copy-backup-restore-plan.md) -
  SC0–SC6 gates for backup-based observer launch without a second full payload.
- [Low-Resource Mainnet Observer Plan](implementation/low-resource-mainnet-observer-plan.md) -
  LR0 local baseline and proposed LR1–LR6 for 2 CPU / 4 GB RAM / 40 GB whole-server operation;
  complete history, measured budgets and lossless savings, not qualified support.
- [teleno_light Concept](implementation/teleno-light-concept.md) -
  separate deferred full-state/bounded-history node idea, not an implemented
  product or a fallback for the full-history requirements.
- [LR2 VM Cache Capacity](validation/low-resource-lr2-vm-capacity.md) -
  optional parsed-cache capacity policy, ownership/config regressions and
  measured reparsing tradeoffs; not a whole-node/Linux memory qualification.
- [SC3 Offline Validation And Activation](validation/single-copy-restore-sc3-activation.md) -
  local consistency/process-recovery evidence and outstanding qualification.
- [SC4 Standalone Direct Restore CLI](validation/single-copy-restore-sc4-cli.md) -
  signed local workflow/regression evidence, supported commands and retained blockers.

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

- [Current Monolithic Node Architecture](https://github.com/koinos/koinos-one/blob/main/docs/manual/developers/deeper-references/monolith-architecture.md)
- [Monolith Service Coverage](https://github.com/koinos/koinos-one/blob/main/docs/manual/developers/deeper-references/monolith-service-coverage.md)

Additional engineering source paths outside the manual source tree:

- [`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md`](https://github.com/koinos/koinos-one/blob/main/docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md)
- [`docs/current/backup-restore/PUBLIC_BOOTSTRAP_RESTORE.md`](https://github.com/koinos/koinos-one/blob/main/docs/current/backup-restore/PUBLIC_BOOTSTRAP_RESTORE.md)
- [`docs/operations/start-node.md`](operations/start-node.md)
- [`docs/operations/container.md`](operations/container.md)

## Normal Workflow

1. Build or locate `teleno_node`.
2. Create or select a basedir.
3. Start as an observer with JSON-RPC bound to loopback unless public RPC is
   intentional.
4. Verify logs, head info, peers, and disk usage.
5. Configure backup and restore paths before relying on the node.
6. Consider producer mode only after the producer checklist passes.
