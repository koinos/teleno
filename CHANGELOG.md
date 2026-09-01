# Changelog

All notable changes to the Teleno node runtime are documented in this file.
Release tags use the form `teleno-node-v<version>`; the version source of
truth is the `VERSION` file at the repository root.

## [1.3.0-dev.0] - Unreleased

Nothing yet.

## [1.2.0] - 2026-08-29

### Added

- A dependency-free transaction TPS benchmark with bounded concurrent load,
  target-rate pacing, admission and inclusion throughput, latency percentiles,
  explicit chain-ID and confirmation gates, secret-free signed-workload input,
  owner-only JSON/Markdown reports, and a simulated RPC test suite.
- Trustworthy fast receipt replay matching `koinos-chain v1.5.2`: fresh
  writable-node Merkle roots, tombstone-preserving delta application,
  successor-header validation, at most one controlled full-execution fallback
  per invalid receipt, and the exact historical mainnet state-root exception.

### Changed

- Fast indexing now retains one pending block so every replayed historical
  block with an available successor is validated before finalization. Empty
  replay queues wait asynchronously without blocking or hot polling, all
  handlers use the configured chain executor, and oversized block-store
  responses use bounded nonblocking backpressure so catch-up also progresses
  with one chain worker. Startup now refuses to enter ready state if indexing
  is cancelled or fails before reaching the block-store head. During recovery
  indexing, only the block store and chain controller start; P2P, RPC,
  mempool, and block production remain stopped until the backlog is validated,
  preventing concurrent controller mutation during replay fallback.
- The state-delta audit now models receipt replay semantics for duplicate keys
  and inventories the resulting full-execution fallbacks separately from
  consensus exceptions and unexplained mismatches.
- Observer-first restore startup preserves the explicitly configured
  `verify-blocks` mode instead of claiming that forced full verification can
  repair state already finalized by an older binary. A persistent recovery
  hold now keeps block production disabled across restarts until a later
  explicit `--enable block_producer` activation.
- Container builds now propagate the requested parallel-job limit through
  nested dependency builds and embed an explicitly supplied hexadecimal
  `VCS_REF` in the native binary, keeping its version identity aligned with
  the OCI revision label even when the build context excludes `.git`.

## [1.1.0] - 2026-07-09

First standalone release after the split from
[koinos-one](https://github.com/koinos/koinos-one). The history of the node
sources was preserved during the extraction.

### Added

- Standalone repository layout: node C++ sources, CMake projects, tests, and
  tools at the repository root; native dependency build scripts under
  `scripts/`; node config templates and per-network example genesis data under
  `config/`; the operator CLI manual under `docs/`.
- Independent SemVer versioning through the root `VERSION` file, validated by
  all three CMake variants. Builds report
  `teleno_node <version>+<git-commit>[-dirty]` via `--version`, and
  `git_version.h` carries the `teleno-node-v<version>` release tag.
- Container image published as `ghcr.io/koinos/teleno` (previously
  `ghcr.io/pgarciagon/teleno-node` from the koinos-one repository), with
  version tags derived from release tags and a `beta` tag tracking `main`.
- CI: a native build workflow (Ubuntu build, CLI smoke checks, ctest with the
  C++ test suite enabled) alongside the container build/smoke/publish
  workflow.
- MIT license, README, and repository scaffolding.

### Changed

- `scripts/build-cpp-libp2p-koinos.sh` defaults are repository-root relative
  (`NODE_DIR` is the repo root, dependencies build into `.deps/`); everything
  remains overridable via environment variables.
- The Hunter build variant now defines `KOINOS_BUILD_TESTS` (default ON at
  configure level; the build script passes it through the
  `KOINOS_BUILD_TESTS` environment variable, default OFF) so CI can compile
  and run the C++ test suite.
- The Hunter source copy uses tar with `.git`/`.deps`/build-dir excludes so
  in-repo dependency builds cannot recurse into themselves, and works in
  minimal container build environments without rsync.
- GMP is propagated through the redirected `libsecp256k1::secp256k1` imported
  target so static link order stays correct for every consumer on Linux.

### Notes

- Koinos One consumes this repository as the `node/teleno-node` git submodule
  and stages the built binary into its packaged releases.
