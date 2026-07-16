# Teleno Agent Project Guide

Last updated: 2026-07-10

This file contains operational guidance for agents working in the standalone
Teleno repository. Detailed architecture, operator procedures, validation
reports, and implementation plans belong in `docs/`, not in this file.

Keep `AGENTS.md` and `CLAUDE.md` byte-for-byte identical.

## Active Project Boundary

- Active repository: `/Users/pgarcgo/code/teleno`
- Active remote: `https://github.com/koinos/teleno.git`
- Primary branch: `main`
- Product and binary: Teleno / `teleno_node`
- This repository owns the monolithic native node: C++ source, CMake projects,
  tests, tools, node configuration, native dependency build scripts, the
  Docker image and CI, CLI operator documentation, native backup and restore,
  and independent native releases.
- Koinos One is the separate desktop application at
  `https://github.com/koinos/koinos-one`. It consumes this repository as the
  `node/teleno-node` git submodule and owns the Electron/React GUI, desktop
  orchestration, app packaging, first-run UX, the MkDocs app manual, and the
  Koinos One product version.
- Make node-only changes here. Changes to the Koinos One GUI, app packaging,
  first-run assistant, or app manual belong in `koinos-one` unless the user
  explicitly asks for coordinated cross-repository work.
- Knodel and the legacy Koinos microservice stack are separate. Use them as
  compatibility references when relevant; do not edit or operate them as a
  side effect of Teleno work.

## Project Mission

Teleno is a community-driven monolithic Koinos node implemented as one native
binary. It must remain compatible with the Koinos protocol and mainnet
behavior. The microservices-based Koinos stack remains the reference
implementation, so optimizations must not create protocol divergence.

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
material, not as authority to modify the Koinos One app during node-only work.

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

When a Teleno change affects Koinos One packaging or user-visible behavior,
report the exact Teleno commit and native build identity needed for the
submodule update. The Koinos One repository must bump its submodule pin
deliberately, rebuild or stage the native binary, run its own package checks,
and update its app changelog/manual when appropriate. Do not silently alter the
consumer repository from a node-only task.
