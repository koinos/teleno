# Teleno Agent Project Guide

## Concise communication

- Keep replies and progress updates very concise by default.
- Lead with the result. Use short, clear sentences and short bullet points.
- Give one idea per bullet. Avoid repetition, long introductions, and unnecessary detail.
- Preserve essential risks, uncertainty, evidence, and confirmation requests.
- Expand only when asked or when necessary for safety or correctness. Do not shorten verbatim text or required document content.


Last updated: 2026-10-16

This file contains operational guidance for agents working in the standalone
Teleno repository. Detailed architecture, operator procedures, validation
reports, and implementation plans belong in `docs/`, not in this file.

Keep `AGENTS.md` and `CLAUDE.md` byte-for-byte identical.

## Active Project Boundary

- Active repository: `/Users/pgarcgo/code/teleno`
- Active remote: `https://github.com/koinos/teleno.git`
- Primary branch: `main`
- LR2 memory, ownership and lifecycle work belongs on
  `codex/lr2-memory-hardening`, as explicitly requested on 2026-10-06.
  Keep it separate from `main` until an explicit integration request. Existing
  mixed, uncommitted work is preserved; review its scope before committing.
- Product and binary: Teleno / `teleno_node`
- This repository owns the monolithic native node: C++ source, CMake projects,
  tests, tools, node configuration, native dependency build scripts, the
  Docker image and CI, CLI operator documentation, native backup and restore,
  and independent native releases.
- Koinos One is the separate desktop application at
  `https://github.com/koinos/koinos-one` and the GUI frontend for Teleno.
  It consumes this repository as the
  `node/teleno-node` git submodule and owns the Electron/React GUI, desktop
  orchestration, app packaging, first-run UX, the MkDocs app manual, and the
  Koinos One product version.
- Make native node changes here. Koinos One GUI implementation, app packaging,
  and first-run implementation changes require explicitly scoped
  cross-repository work. Relevant Koinos One documentation updates are part of
  Teleno maintenance, as described in the documentation handoff below.
- Knodel and the legacy Koinos microservice stack are separate. Use them as
  compatibility references when relevant; do not edit or operate them as a
  side effect of Teleno work.

## Project Mission

Teleno is a community-driven monolithic Koinos node implemented as one native
binary. It must remain compatible with the Koinos protocol and mainnet
behavior. The microservices-based Koinos stack remains the reference
implementation, so optimizations must not create protocol divergence.

Current resource-adaptation work must preserve complete canonical history from
genesis and all block/receipt records present in the selected backup. Do not
meet a disk budget by pruning history, omitting receipts, or switching to a
bounded-history node. `teleno_light` is a separate deferred concept, not a
fallback, current native product, or authorized implementation. See
`docs/implementation/low-resource-mainnet-observer-plan.md` for the active
full-history requirements and `docs/implementation/teleno-light-concept.md`
for the separate idea.

The node must support safe observer operation first. Block production, backup
administration, restore activation, and other higher-risk behavior must remain
explicit and carefully gated.

All project documentation and committed user-facing text must be written in
English, even when discussion with the user happens in another language.

## Repository Map

- Native runtime source: `src/`
- C++ tests: `tests/`
- Native tools and migration/audit utilities: `tools/`
- CMake entrypoints: `CMakeLists.txt`, `CMakeLists.hunter.txt`, and
  `CMakeLists.standalone.txt`
- Dependency and native build scripts: `scripts/`
- Runtime configuration templates and genesis data: `config/`
- Container assets: `Dockerfile`, `docker/`, and `.dockerignore`
- CI and release workflows: `.github/workflows/`
- Version source of truth: `VERSION`
- Native release notes: `CHANGELOG.md`
- Operator documentation entrypoint: `docs/README.md`

## Documentation Map

- Install and build: `docs/install-or-build.md`
- Quickstart: `docs/quickstart.md`
- Configuration: `docs/configuration.md`
- Observer operation: `docs/running-observer-node.md`
- Producer operation and activation gate: `docs/running-producer-node.md`
- Backup and restore: `docs/backup-restore-cli.md`
- RPC and admin endpoint exposure: `docs/rpc-endpoints.md`
- Logs and diagnostics: `docs/logs-and-diagnostics.md`
- Command reference: `docs/command-reference.md`
- Recovery-first troubleshooting: `docs/troubleshooting.md`
- Container operation: `docs/operations/container.md`
- Native release identity and checklist: `docs/release-builds.md`

Read the relevant documentation before making broad architecture, storage,
backup, restore, producer, RPC exposure, container, or release decisions. Keep
the affected documentation and examples current when behavior changes.

Some deeper architecture and parity references still live in `koinos-one` and
are linked from `docs/README.md`. Treat those as cross-repository reference
material. Follow the documentation handoff below; reference links do not
authorize changes to Koinos One application code or packaging.

## Open TODOs

- [ ] Address the bugs and memory/disk-efficiency findings in the
  [software review](docs/performance/teleno-software-audit.md), following the
  [remediation plan](docs/implementation/teleno-audit-remediation-plan.md).
  Prioritize WP1 storage integrity, WP2 RPC lifecycle, and WP3 cache correctness
  and memory efficiency. LR1 implements a scoped subset of WP1/WP6 and cache
  clear accounting. LR2 adds scoped cache invariants/hit allocation fixes, owned
  RPC sessions, DB memory accounting and bounded disposable P2P accepted-ID
  retention; whole-node bounds and integrated
  lifetime/race qualification remain open. The audit is not fully remediated.
  LR2 also corrects native admission refusal/metrics and state read ownership;
  immutable clone payloads and bounded retained cache payload do not bound
  caller-held/fork memory or prove Linux/ASan/TSan qualification.
  M1-C adds locally verified failure-atomic write-only multi-fork admission;
  aggregate capacity and block/LIB allocator safety remain open.
  M1-D inventories indexed map owners/keys/tombstones across retained forks on
  demand; string capacities are not heap/RSS totals or admission ceilings.
  M1-E adds opt-in standalone transaction-work count/input-wire reservations
  and local-overload classification; retained-fork, ingress/output and whole-node
  memory bounds and Linux profile selection remain open.
  M1-F adds scoped failure-atomic fork-head expiry/removal and checked cleanup
  decoding; its map-only publication is not block/LIB, persistent-commit or
  whole-machine OOM qualification. Aggregate capacity remains open.
  M1-G shares immutable pending records across forks and adds opt-in live-owner/
  capacity reservations; head expiry does not free ancestor-held credits. This
  excludes keys/indexes/RC/nonces/tombstones, encoding workspace and other
  consumers; it is not a complete aggregate memory or Linux qualification.
  V1-A protects parsed-module ownership through allocation failure and makes
  cache publication optional/failure-atomic, with coalesced cold parsing. Parsed
  byte/instance/queue policy and Linux tradeoffs remain open. V1-B corrects the
  pinned Fizzy context-allocation termination through a shared hash-pinned build
  recipe; native allocation failures remain local, not consensus reversions.
  Scoped fault cleanup does not prove machine-wide OOM, integrated races or LR2.
  V1-C adds optional attributed parsed-module cache capacity and native metrics;
  default bytes remain unlimited/count 32. Oversize bypass preserves execution.
  Allocator, caller-held, active-instance/queue and Linux memory gates remain.
  V1-D corrects reproduced unaligned host result-count stores with byte copies
  at the original sites; alias ordering and historical behavior must remain.
  D1 historical Mac standalone programs reproduce ASan/TSan startup failure
  before main. Linux clean/known-fault controls now pass; TSan requires child-
  only ASLR-off. P2-A helper/codec and P2-B scoped real-transport sanitizer
  checks pass; prebuilt SDK libraries remain uninstrumented. Integrated
  state/mempool/VM/RPC and full lifecycle/concurrency qualification remain open.
  R1-A releases HTTP bodies after service/write completion and attributes their
  capacities; service/JSON/serialization/output admission and Linux gates remain.
  R1-B serializes/releases batch results individually into a complete HTTP body,
  preserving strict UTF-8 failure dispatch and avoiding result-tree copies.
  The final body, input/services and aggregate admission remain unbounded axes.
  P2-A completes real Peer RPC short writes without changing frame bytes and
  retains each write owner through completion. Aggregate transport capacity,
  full cancellation/shutdown and Linux qualification remain open. P2-B adds
  owned fragmented reads, constant-stack MessagePack skipping and scoped real
  TCP/Noise/Yamux stop/restart checks; native Mac/Linux regressions pass. This
  does not bound aggregate queues or qualify lifecycle concurrency/whole-node use.
  P2-C owns outgoing RPC phases and complete-response deadlines, cancels
  retired client calls and keeps late borrowed buffers alive. Local receive
  capacity narrows later block batches without peer scoring. Small decoded
  results shed large ignored capacity. Final Mac/Linux native checks, separate
  scoped ASan/UBSan/TSan, VPS components and runtime-container CLI/hash pass.
  Aggregate SDK/decoded/caller memory, DNS/server lifecycle and whole-node gates
  remain open.
  P2-D bounds error-record count/attributed key capacity, groups scores by network
  address and corrects idle decay/overflow/query rounding. Optional candidates
  trim with discovery off; seeds/connected metadata stay protected. Allocation
  refusal is local without peer blame. Mac/Linux native regressions and paired
  component measurements, separate scoped ASan/UBSan/TSan, VPS components and
  isolated runtime-container CLI/hash checks pass.
  Protected candidate counts can exceed the configured limit; candidate/SDK/
  caller bytes and whole-node gates remain open.
  P2-E adds native explicit dial lifetime count/peer-capacity reservations,
  failure-atomic setup and seed/stop publication; supported seed aliases share
  admission. Controlled TSan fixture lifetime correction preserves detection.
  Mac/Linux 57/57 native checks, nine fixtures per separate sanitizer, 12 VPS
  component commands and runtime-container CLI/hash pass. Implicit GossipSub/
  RPC/DHT/inbound, SDK/native known peers, control/caller aggregates, DNS/full
  cancellation and whole-node qualification remain open.
  P2-F bounds optional advice and lifetime-owned native metadata projections,
  removes external SDK reads and corrects a reproduced query/stop host race.
  A hash-pinned TSan-only temporary DI mutex lifetime overlay retains real-cycle/
  destroy-held detection. Final Mac/Linux 59/59 native, eleven fixtures per
  separate sanitizer, 14 VPS commands and runtime CLI/hash pass. Active/SDK
  metadata, implicit work, other queues/caller copies and whole-node gates remain.
  Obtain authorization before other packages. Keep this TODO open until the required fixes and
  verification gates are completed and documented.

## Build And Test Workflow

The complete repository-local dependency build is:

```bash
./scripts/build-cpp-libp2p-koinos.sh
```

It places dependencies under `.deps/` and the native build under `build/`.
For a configured tree, use a focused rebuild when appropriate:

```bash
cmake --build build --target teleno_node --parallel
```

The build script defaults `KOINOS_BUILD_TESTS` to `OFF`. For a full test-enabled
build matching CI:

```bash
KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

At minimum, verify the binary identity and CLI surface after relevant native,
build, dependency, or release changes:

```bash
./build/teleno_node --version
./build/teleno_node --help
```

Run focused tests for the changed component, then broader CTest when shared
runtime, storage, networking, backup/restore, producer, build, or dependency
behavior changes. Do not claim successful validation for tests that were not
built or run.

## LR2 Development Data Placement

The primary optimization and qualification target is the user-designated Linux
baseline VPS, including producer operation: Ubuntu/x86_64/ext4, 2 vCPUs,
4 decimal GB physical RAM, 40 GB-class disk and no swap, as verified for the
replacement server on 2026-10-15. The first 1-vCPU/2-GB server was explicitly
retired by the user; do not contact it or use it as a fallback. Do not enlarge
the mandatory baseline. See `docs/implementation/issue-9-goal.md`.
Use Mac results only as development diagnostics; they cannot close Linux memory,
catch-up, durability or recovery gates. Prefer portable changes justified by
Linux measurements rather than APFS/macOS-specific performance tuning. This
priority does not authorize live VPS operation or creation of a Linux VM.

The subsequent 2026-10-15 request authorizes isolated development/qualification
on the designated baseline and persistent SSH access setup, including necessary
password recovery. Keep access details and private host inventory outside source
and public reports. This does not activate mainnet production or waive network,
producer-target, key/registration, observer-first and recovery gates.

The user explicitly selected the mounted My Passport volume for new LR2
builds, diagnostic artifacts, test databases and temporary data. Keep source
and committed documentation in this repository, but use a fresh owned run
under `/Volumes/My Passport/` for generated development data. Set `TMPDIR`,
`TELENO_TEST_ARTIFACT_ROOT` and `TELENO_TEST_BUILD_ROOT` explicitly and disable
Python bytecode writes for diagnostics. Test-root selection does not authorize
live data access, restore activation or source-trust changes. External CMake
caches must still name this exact source repository. Read existing pinned
dependencies as inputs; do not relocate/delete existing chain databases or
user data. If the volume is absent, stop data-producing work rather than
silently falling back to the internal disk. Record exact owned paths in the
LR2 completion matrix and project memory, not operational credentials.

## Versioning And Release Identity

- `VERSION` is the native SemVer source of truth. It must contain SemVer
  without build metadata.
- Development state uses a prerelease value such as `1.2.0-dev.0`.
- `teleno_node --version` reports
  `<version>+<git-commit>[-dirty]`.
- Native release tags use `teleno-node-v<version>`.
- The native version is independent of the Koinos One desktop app version; do
  not keep the two version streams in lockstep.
- `CHANGELOG.md` documents user-visible and operator-visible native behavior.
  Keep its unreleased version aligned with `VERSION`.

When making a native release, treat versioning, release notes, tests, binary
identity, CLI smoke checks, and container verification as required release
work. Drop the development prerelease only for the actual release, date the
matching changelog section, and create or push the release tag only after the
source, changelog, binary, and intended artifacts agree. Immediately after a
release, advance the working tree to the next intended development version and
start a new unreleased changelog section.

## Protocol And Compatibility Guardrails

- Preserve Koinos protocol compatibility and mainnet behavior. Do not trade
  compatibility for local performance or implementation convenience.
- Treat the legacy microservice stack as the behavioral reference where Teleno
  parity is uncertain.
- Keep compatibility evidence, migration tooling, and validation reports when
  they prove protocol parity, client compatibility, storage durability, or
  restore safety.
- Do not revive GarageMQ or legacy microservice build/start/packaging surfaces
  inside this repository.
- Do not claim full legacy service parity without checking the current
  implementation and the linked service-coverage references. Account history
  and other optional services must be described according to their actual
  current feature coverage.

## Mainnet Safety Guardrails

- Never put private producer addresses, private keys, wallet material, bearer
  tokens, SSH credentials, private host inventory, or other local operational
  secrets in committed code, tests, logs, examples, or documentation.
- Public material must use placeholders such as
  `<YOUR_MAINNET_PRODUCER_ADDRESS>`.
- Do not perform hidden or background mainnet mutations.
- Do not transfer funds away from a protected producer address.
- Mainnet producer registration, VHP burns, producer setup changes,
  default-account changes, producer-targeted config writes, transaction
  signing/submission, and block-production activation require a fresh explicit
  user request, confirmation of the network and target, and a reviewable plan
  or dry run first.
- Before any chain-mutating operation, verify the network, basedir, config,
  signer, target address, and operation type.
- Start and validate restored or newly configured nodes as observers. Enable
  block production only after database health, network identity, head progress,
  peer health, producer address, VHP, and producer-key checks pass.

If local-only operational memory exists, read it when the task requires that
context, keep it untracked, and never quote or copy its private details into
public repository files.

## Backup, Restore, And Recovery Guardrails

- Native backup and restore behavior is owned by `teleno_node`; update
  `docs/backup-restore-cli.md` with behavior changes.
- Keep backup administration local-only unless the user explicitly scopes a
  different deployment. Admin endpoints must remain loopback-bound and
  bearer-token protected by default.
- Public bootstrap is a public read-only backup source, not public admin API
  exposure.
- Backups must not include wallet files or producer private keys.
- Stop unmanaged processes that use the same basedir before direct CLI restore
  or restore activation.
- Preserve observer-first recovery markers and behavior after restore.

If the node reports `block previous state merkle mismatch` or another
persistent state-merkle mismatch:

- preserve the existing state database;
- do not clear `chain/blockchain`;
- do not start from an empty state database as the first action;
- do not force a fresh full resync as the first action;
- collect diagnostics and attempt validation-based recovery first;
- move or delete state only after explicit user approval and evidence that
  validation-based recovery failed.

## Runtime And Deployment Cautions

- Repository work does not authorize changes to live servers or running nodes.
- Check the actual basedir, config path, selected network, process ownership,
  service path, and current runtime state before touching a deployment.
- A host may run legacy services alongside observer or producer deployments.
  Do not assume process or data ownership from a hostname alone.
- Keep JSON-RPC and gRPC exposure intentional. Keep admin surfaces local and
  protected; do not solve connectivity or authorization failures by exposing
  privileged endpoints.
- Prefer observer-safe diagnostics. Do not delete chain state, activate a
  restore, or enable production as the first troubleshooting action.

## Koinos One Handoff

Koinos One is Teleno's GUI frontend. When a Teleno change affects desktop users
or operators, update the relevant documentation in both repositories as part
of the same task. This includes the Koinos One app manual and, where relevant,
its changelog for changes to configuration, CLI behavior, backup/restore,
recovery, safety gates, resource requirements, or user-visible node behavior.

Before editing Koinos One documentation, locate its checkout, read its own
agent instructions completely, inspect its worktree, and preserve all existing
changes. Keep documentation in English and distinguish native capabilities
from functionality actually exposed by the GUI or included in a packaged
release. If the checkout or required evidence is unavailable, report the exact
blocker and the pending documentation updates; do not claim the handoff is
complete.

When a Teleno change affects Koinos One packaging or user-visible behavior,
report the exact Teleno commit and native build identity needed for the
submodule update. An authorized integration must deliberately update the
submodule pin, rebuild or stage the native binary, and run Koinos One's own
package checks. Documentation coordination alone does not authorize GUI or
packaging code changes, submodule updates, commits, pushes, releases,
publication, deployments, live-node operations, or block-production activation.
