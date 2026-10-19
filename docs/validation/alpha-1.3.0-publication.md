# Native 1.3.0-alpha.1 publication qualification

Date: 2026-10-19. Channel: experimental native alpha.

The [public prerelease](https://github.com/koinos/teleno/releases/tag/teleno-node-v1.3.0-alpha.1) publishes the accumulated work from
`codex/lr2-memory-hardening`. The branch remains separate from `main`.
Publication does not deploy or restart a running node, modify a pool or wallet,
or update Koinos One's package/submodule.

## Exact released implementation

- Source/tag target: `2380fcb545ed056a548adbcd5e1cffcefe61ecc0` / `teleno-node-v1.3.0-alpha.1`.
- Native identity: `teleno_node 1.3.0-alpha.1+2380fcb545ed`.
- Linux amd64 ELF: SHA-256 `2e451a565e7ceece54e24c02bd8b2ef9573c7b67661c9746bb05a7d05ffe573e`,
  45,066,736 bytes, built/tested on Ubuntu 24.04.
- Image: `ghcr.io/koinos/teleno:1.3.0-alpha.1`,
  immutable manifest `sha256:1cc0183516ab2275cf64dea084eca9e0be6244c2faa9c7c4052877debf6350a0`. Its ELF hash equals the downloadable runtime.
- Explicit image aliases also include `teleno-node-v1.3.0-alpha.1` and
  `sha-2380fcb545ed056a548adbcd5e1cffcefe61ecc0`. Verified `latest`, `beta` and `1.2.1` digests remain unchanged.
- Release metadata records the clean commit, compiler, actual linked static
  archives and all 433 committed source-file hashes. Source archive content
  matches the clean hosted build manifest exactly.

The release contains the Linux runtime/configuration/documentation bundle,
exact source, qualification logs/JUnit, build metadata and `SHA256SUMS`.
Assets are unsigned. Checksums, release asset digests and downloaded contents
are verified after upload. No operational data or custody/access material is
included. See [the detailed update notes](../releases/1.3.0-alpha.1.md).

## Current-source checks

- [Clean hosted native build](https://github.com/koinos/teleno/actions/runs/39209756856): **64/64 normal CTest entries**,
  including producer/head/mempool, backup/restore/activation, state/cache,
  protobuf/Fizzy, RPC/P2P and the isolated compatibility/cold-restart regression.
  Version/help and transaction-benchmark unit checks pass.
- [Verified native image validation](https://github.com/koinos/teleno/actions/runs/39211505720): package
  only the successful same-commit trusted Build artifact; verify the complete
  source manifest, ELF architecture/hash, version and native test report.
  Container version/help, exact ELF hash and producer-consent refusal pass.
- [Tag image publication](https://github.com/koinos/teleno/actions/runs/39211736781): the same verification and smoke
  checks pass before explicit alpha image tags are pushed. Registry labels and
  immutable manifest identity are verified independently afterward.
- Optional macOS reinvestment tools: 18 local unit entries pass with mocked
  transport and the installed public token ABI; no signing, network call,
  installer or Keychain access occurs. The tool files are byte-identical
  between the test source and final release source.
- Koinos One's locally coordinated producer manual passes strict MkDocs.
  Its pre-existing changes, package and native pin remain outside this release.

These are normal build/regression and container checks. They do not close
integrated ASan/UBSan/TSan, whole-node capacity or live final-binary gates.

## Failures retained and corrected

1. Fresh `3d4e51b` native/container builds fail during dependency bootstrap:
   enclosing Git skips the Fizzy extended patch; a function-scoped Protobuf
   version is unavailable to its directory check. `f320f5c` fixes both contexts
   without changing archive/dependency revisions or expected patched C++ hashes.
   Thirteen focused recipe checks and a real pinned-package scope probe pass.
2. [f320f5c native run](https://github.com/koinos/teleno/actions/runs/39201561326)
   builds the node and passes CLI/benchmark checks, then fails linking the
   restored compatibility fixture. `720ec2a` adds its required mempool library.
   The separate [full source container run](https://github.com/koinos/teleno/actions/runs/39201561337)
   passes build/CLI/producer guard; it retains its own predecessor identity.
3. [720ec2a native run](https://github.com/koinos/teleno/actions/runs/39207867846)
   passes 63/64 tests. The unchanged compatibility regression exposes a real
   startup regression: 64,000-byte external syscall result capacity instead of
   published native 1.2.1's 131,072. `2380fcb` restores that published value.
   The final clean suite passes the large result, exact boundary, oversize
   refusal, later read, finalized receipt and cold observer restart checks.

Original logs/failures remain preserved. No test or detector is removed to
obtain the passing result.

## Predecessor observations and remaining issue 9

The measured producer ELF remains
`0bd4c3666a93a3b43d7c4c7e72776d9d935eec1a33cdeec22572ac71022d9447`.
Its finite 60-minute commissioning and separate irreversible own mainnet block
40,323,375 remain valid only for that source, binary, input and duration.
The alpha additionally restores the 131,072-byte syscall capacity in `main.cpp`;
therefore its runtime source differs from that measured predecessor. Exact
release identity and current CI results do not replace final-binary VPS checks.

[Issue 9](https://github.com/koinos/teleno/issues/9) stays open for aggregate
memory/workspace and lifecycle accounting, integrated diagnostics, the full
required historical differential interval, representative loaded producer and
simultaneous workloads, sustained 24-hour/seven-day behavior and final-source
restart/recovery/resource qualification. The selected-input full-history
40-GB restore with an 8-GiB reserve and finite mainnet producer milestones stay
separately recorded; they are not claims of unlimited future storage capacity.

## Operator use and rollback

Verify `SHA256SUMS`, unpack to a separate location, inspect version/help and
follow [observer operation](../running-observer-node.md). Preserve the old
executable, configuration and complete backups. Never let two processes open
the same database. Check codec/configuration compatibility before attempting
an executable downgrade; switching binaries does not validate a database
downgrade. Mainnet producer activation and financial actions require their
existing explicit operator authorization and identity checks.

After publication the development branch advances to `1.3.0-dev.1`; the alpha
tag remains pinned to the clean, tested release source above.
