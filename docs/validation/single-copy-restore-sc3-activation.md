# Single-copy restore: SC3 offline validation and activation

Date: 2026-10-02. Scope: SC3 of the
[single-copy restore plan](../implementation/single-copy-backup-restore-plan.md).

**The internal SC3 engine is implemented and locally tested. Operational CLI
integration, Linux qualification, and backup launch on the intended 40 GB server
remain pending.** This report does not establish historical consensus parity,
physical power-loss durability, or production readiness.

## Environment and preservation

- Repository: `/Users/pgarcgo/code/teleno`, branch
  `codex/reward-reinvestment-macos`, HEAD
  `cfd2eb83b8080ca49911555108ce05f0a9068f01` plus pre-existing SC0–SC2/user work
  and these uncommitted SC3 changes.
- Native identity: `teleno_node 1.3.0-dev.0+cfd2eb83b808-dirty`.
- macOS 26.5 (25F71), arm64, Mac16,10, 10 logical CPUs, 17,179,869,184 bytes RAM;
  internal APFS. This is not a 40 GB constrained environment.
- AppleClang 16.0.0.16000026, C++20, arm64 Release, CMake 4.2.3, Ninja 1.13.2.
  RocksDB 8.8.1, Boost 1.83.0; cpp-libp2p source revision
  `c03bf7c44c4fd30a390f3ef93bd1eab482e686a6` with existing repository patches.
- Fresh native tree: `build-sc3-final-20261002`; separate UBSan tree:
  `build-sc3-ubsan-20261002`. Their caches and the standard `build` cache have
  `CMAKE_HOME_DIRECTORY=/Users/pgarcgo/code/teleno`, not another workspace.
- The user authorized the next local milestone. No live node, VPS, producer
  configuration, producer key, transaction, deployment, commit, push, tag,
  publication, release, or Koinos One package/submodule change was involved.

Ignored evidence root, abbreviated E:

```text
build-single-copy-evidence/sc3-20261002-qmA8fG/
```

E contains the initial tracked diff, worktree status, environment/toolchain
records, dependency library hashes, CMake caches, and command output. The saved
SHA-256 check confirms `README.md` and
`docs/performance/KOINOS_HIGH_THROUGHPUT_IMPLEMENTATION_PLAN.md` are unchanged
from the beginning of this task. `AGENTS.md` and `CLAUDE.md` remain identical.
Existing overlapping SC2 changes were extended rather than reset. There is no
claim of a complete before/after hash manifest for every pre-existing untracked
file. All diagnostic datasets remain in ignored, newly created owned directories.

## Implemented invariants

### Existing-only offline validation

`validate_direct_restore_attempt()` binds the immutable specification, paths,
generation, and manifest/inventory hashes. It requires a fresh same-filesystem
target, rehashes the payload, checks reserve space, and durably records
`validating` before opening any database. Failed validation preserves the sole
stage; source replay is forbidden once validation has begun.

The new validator opens all nine existing unified column families with
`OpenForReadOnly`, without runtime services, writable recovery, creation,
compaction, or flush. Read-only WAL recovery occurs in memory. It checks:

- Independently selected expected chain ID against the parsed genesis hash and
  persisted state chain ID; expected identity uses native `0x` multihash hex.
- Unified schema, required metadata, numeric encodings, state object count,
  persisted ID/root/header/revision, and genesis checkpoint contents.
- Every readable key/value, iterator status and database checksum; protobuf block
  and receipt consistency, highest-block ancestry, persisted checkpoint receipt
  and header, and the state head-block object.
- Checked close statuses, cancellation, and unchanged inventory hashes after
  the read-only open. A verification record binds the same generation and hashes.

Genesis input above 32 MiB, aggregate inventory WAL input above 128 MiB,
oversized parsed records, unsupported layouts and recovery needing a writable
open fail closed. The WAL limit is **not** a measured RSS bound or 4 GB memory
qualification. Existing runtime storage error/flush defects in audit WP1 remain
unrepaired; this validator uses checked RocksDB calls instead of those paths.

This is trusted-backup storage and identity consistency, not recomputation of
historical execution. Persisted roots are checkpoint/delta commitments; hashing
all current objects as an invented replacement root would be incorrect.
Validation explicitly reports `historical_replay_validated: false`. Consensus,
normal object removal, receipt replay and historical exceptions are unchanged.

### Lifetime-held exclusion and durable activation

POSIX node startup and direct stage/validation/activation share a lifetime-held
sidecar `flock`, outside the basedir. The gate inode is never unlinked. A busy or
unsafe gate is refused before database initialization. It is advisory: operators
must still stop unmanaged writers that do not honor this gate.

Before target mutation, activation publishes a durable, exclusive sidecar intent
bound to the generation, verified report, DB device and inode. While an intent
exists, ordinary startup refuses before opening the database. Explicit internal
offline recovery accepts only its matching owned attempt, never a guessed DB or
source redownload.

The target may be absent or contain only reviewed matching runtime inputs and
configuration, then this generation's markers and exact DB. Unknown/populated
targets, foreign markers, different DB inodes, changed payloads and target races
are refused. Snapshot `config.yml` is not imported and active configuration is
not overwritten. Legacy `.pre-restore/` copy behavior is unchanged. A subsequent
legacy restore preserves the prior direct receipt alongside the old database,
so its plain-text recovery marker is not rejected as a foreign direct generation.
An added cross-mode regression verifies DB/config preservation and observer-first
startup. This does not repair the legacy publication/durability audit WP6 risks.

The DB is moved once by exclusive same-filesystem rename: macOS `RENAME_EXCL`
or Linux `RENAME_NOREPLACE`. Unsupported exclusive rename or cross-device moves
fail without a copy/overwrite fallback. Parents are synced with checked results.
The inode and DB hashes are retained across interruption; target reopening must
match the verification report before durable `activated` and intent removal.

Observer hold, one-shot marker and receipt bind the same generation and digests.
Marker consistency is checked before node DB startup. First startup remains an
observer even if configuration or that first invocation enables production.
Later configuration-only restarts retain the hold. The pre-existing separately
explicit producer-release policy remains; no producer activation was exercised.
An already activated receipt is idempotent completion evidence, not a new claim
that a subsequently used database still matches its old snapshot.

## Focused verification

The new native test creates real nine-CF RocksDB datasets from the checked-in
Harbinger genesis. A nonzero checkpoint fixture deliberately uses synthetic
block/state commitments: it exercises structural consistency, **not** historical
block validity. A checkpoint containing 14,235 bytes of real WAL exercises
read-only WAL recovery, hashing preservation and activation/reopen.

The final verbose run recorded **4,102 always-on checks** (including the artifact
write), 90 activation boundaries and 34 validation boundaries. Each boundary was
tested both by injected exception and by abrupt exit of a fresh executed worker:
180 activation and 68 validation interruption cases. Each owned attempt then
recovered without a second DB or replaced user configuration. `summary.json`
records 4,101 checks before its own successful write adds the last check.

Covered negative cases include corrupt/missing SST, missing CFs, wrong/unchecked
expected chain, changed payload after verification, cancellation, insufficient
space, populated targets, target race, foreign markers, competing attempts and
held common gates. Pending-intent and foreign-generation startup failures are
checked against the actual native executable. Crash workers use `exec`, not
RocksDB calls against inherited post-fork thread-pool state.

The native observer smoke uses the activated genesis dataset with production
enabled in configuration but no keys. P2P, RPC, mempool and optional services are
explicitly disabled. It reached `teleno_node ready`, logged the active recovery
hold, shut down cleanly on SIGINT with exit zero, and retained the hold. No
network sync or block production is established by this smoke.

Representative final artifacts:

```text
build-single-copy-evidence/sc3-unit-1026078775/
  summary.json
  fixture/spec-input.json
  observer-smoke.log
  wal-attempt/validation.json
build-single-copy-evidence/sc3-unit-357381626/  # UBSan run
```

For the first root, manifest SHA-256 is
`8030b827b4de9a1f95d817938c2003534ea46cd89905d418c22307bbf6579d2e`,
inventory SHA-256 is
`0a94ab511bd5de1f0a1787ea87ea4693250521ce19e0ad273419f1c4a766841f`,
and testnet chain ID is
`0x122067703e2929121059ca3f591aa8e50fb19d747b26e3b73ed66ff7cde629d28749`.
Generated RocksDB files, randomized attempt IDs and hashes vary between runs;
each run saves its actual input specification and boundary list.

## Build and test commands/results

| Check | Actual result | Evidence in E |
| --- | --- | --- |
| Pre-change focused backup/direct suite | 11/11 passed | `baseline-focused.log` |
| Dependency script, tests enabled, isolated source/build overrides | Blocked in Hunter configuration: `make: /Volumes/My: No such file or directory` | `canonical-build.log` |
| Fresh Release configuration/full native build | Passed, 148 initial build steps and final compatibility rebuild | `configure-final-clean.log`, `build-final-clean.log`, `build-compatibility-final.log` |
| `cmake --build build --parallel 4` | Passed | `build-standard-compatibility.log` |
| `ctest --test-dir build --output-on-failure` | 26/27 passed; SC0 truncated-transfer SIGBUS | `ctest-standard-compatibility.log` |
| Final focused backup/direct suite | 12/12 passed | `focused-standard-compatibility.log` |
| Fresh-tree full CTest | 26/27 passed; same baseline failure | `ctest-compatibility-final.log` |
| Final SC3 verbose test | Passed, 4,102 checks | `sc3-compatibility-final.log` |
| Separate UBSan SC3 build/run | Passed, no diagnostic; vptr excluded | `ubsan-configure-final.log`, `ubsan-build-compatibility.log`, `ubsan-test-compatibility.log` |
| Standard and fresh native `--version` / `--help` | Passed | `version-standard-compatibility.log`, `help-standard-compatibility.log`, `version-final.log`, `help-final.log` |
| Transaction benchmark-script unit tests | 4/4 passed | `benchmark-tests.log` |
| Koinos One `mkdocs build --strict` | Passed | `koinos-one-docs.log` |
| `docker info` / container smoke prerequisite | Blocked: local Docker socket unavailable; no smoke run | `docker.log` |

The full-suite failure is the previously recorded SC0 truncated HTTP fixture,
not a newly passing test or an SC3 fix. Standard-tree reproduction artifacts:
`build-single-copy-evidence/sc0-run-if0f5dss/truncated.log`. The diagnostic
harness still reports native signal SIGBUS, exit code -10. Broader validation
must not be described as all green.

The dependency script was actually invoked with `KOINOS_BUILD_TESTS=ON` and
repository-local scratch overrides for its source/build paths, to avoid its
destructive re-preparation of existing dependency directories. Its Xcode/SDK
path contains a space, and the Hunter subbuild fails before dependency rebuild.
The successful alternative used a fresh repository-local native configuration
and existing repository-local dependencies, not a successful clean dependency
bootstrap. Full commands, cache and hashes are retained in E.

Both Hunter and cpp-libp2p auxiliary include paths must be supplied on the first
fresh configure. An initial fresh configuration omitted the latter and failed
Boost.Asio overload resolution; a second, correctly configured clean tree built
successfully. Initial test/log expectation and compile errors were fixed;
intermediate stale-binary passes are not counted as final validation.

UBSan instrumented repository code and the test, not third-party dependency
libraries or the ordinary native smoke child. RocksDB has no RTTI; the initial
vptr-instrumented attempt could not link. The successful separate configuration
uses `-fsanitize=undefined -fno-sanitize=vptr -fno-omit-frame-pointer` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. ASan and TSan were not run.

Normal reproduction after a test-enabled compatible configure:

```bash
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
ctest --test-dir build -R '^(koinos_backup_|teleno_direct_restore)' --output-on-failure
ctest --test-dir build -V -R '^koinos_backup_direct_activation_test$'
./build/teleno_node --version
./build/teleno_node --help
/usr/bin/python3 -m unittest -v tests/scripts/benchmark_transaction_tps_test.py
```

## Files changed for SC3

Native implementation/tests:

- `src/CMakeLists.txt`
- `src/main.cpp`
- `src/core/storage_lease.cpp`, `src/core/storage_lease.hpp` (new)
- `src/core/restore_startup.cpp`, `src/core/restore_startup.hpp`
- `src/backup/offline_restore_validation.cpp`,
  `src/backup/offline_restore_validation.hpp` (new)
- `src/backup/direct_restore.cpp`, `src/backup/direct_restore.hpp`
- `src/backup/snapshot_repository.cpp` (retire/preserve prior direct receipt)
- `tests/backup/direct_activation_test.cpp` (new)

Teleno documentation:

- `CHANGELOG.md`, `docs/README.md`, `docs/backup-restore-cli.md`
- `docs/command-reference.md`, `docs/running-observer-node.md`
- `docs/release-builds.md`
- `docs/implementation/single-copy-backup-restore-plan.md`
- `docs/memoria.md`, `tools/backup/README.md`
- `docs/validation/single-copy-restore-sc3-activation.md` (this report)

Documentation-only Koinos One handoff:

- `docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md`
- `docs/manual/koinos-one/backup-and-restore.md`

Those app documents distinguish internal native capabilities from current GUI
and packaged releases. No new committed Teleno revision is available to pin;
the current dirty build identity above is not a package integration. Deliberate
submodule pinning, binary staging/rebuild and app package checks require separate
authorization.

## SC3 progress and exit status

- **Implemented:** read-only verification, sole-DB preservation, generation-bound
  intent/markers, common cross-process exclusion, exclusive rename, interrupted
  internal recovery and observer-first startup. Listed local fault/reopen/legacy
  regression gates pass; configuration and DB inode preservation are tested.
- **Not proven:** actual cross-device filesystem refusal on mounted Linux
  volumes, Linux `renameat2`/directory-sync behavior, physical power loss, true
  disk exhaustion, Windows support, blocked-kernel-I/O timing, and long-running
  recovery on representative historical snapshots. Exceptions/process exits
  around sync calls are not physical I/O failure or power-loss experiments.
- **Exact prerequisites still missing:** an isolated equivalent Linux host and
  test filesystem setup; Docker daemon for container smoke; a coherent immutable
  representative snapshot/archive and identified reference execution for
  historical/differential validation; a capacity-limited 40 GB dataset/host
  experiment with measured OS/runtime, compaction and headroom. No historical
  replay, state-delta audit or differential run was performed in SC3.
- **Existing blockers:** SC0 diagnostic SIGBUS and dependency-bootstrap SDK path;
  runtime audit WP1 remediation is a separate authorized task, not silently fixed.
- **Exit assessment:** local internal SC3 code and process-interruption tests are
  delivered. Not every portability/durability/deployment gate is proven, and the
  complete single-copy Definition of Done is **not** satisfied. SC4 CLI integration
  is next; SC5 must explicitly qualify or reject the 40 GB server goal without
  weakening reserve, protocol, or durability requirements.
- **External actions:** commit/push, release/tag/publication, Linux/VPS deployment,
  Koinos One pin/packaging and block-production activation remain unexecuted and
  require their applicable explicit authorizations. There is no operator-facing
  direct activation command to run before SC4.
