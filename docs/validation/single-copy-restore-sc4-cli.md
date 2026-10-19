# Single-copy restore: SC4 standalone CLI integration

Date: 2026-10-02. Scope: SC4 of the
[single-copy restore plan](../implementation/single-copy-backup-restore-plan.md).

**SC4 is implemented and locally exercised. Its all-green exit gate is not
satisfied:** broader CTest retains the existing SC0 SIGBUS, the dependency
bootstrap is blocked by SDK path handling, and container smoke cannot run
without a Docker daemon. This is not Linux/VPS, historical replay, 4 GB RAM,
40 GB disk, release or production qualification.

## Environment and preservation

- Repository `/Users/pgarcgo/code/teleno`, branch
  `codex/reward-reinvestment-macos`, HEAD
  `cfd2eb83b8080ca49911555108ce05f0a9068f01`, with pre-existing SC0–SC3
  and user changes plus this uncommitted SC4 work.
- Native identity: `teleno_node 1.3.0-dev.0+cfd2eb83b808-dirty`. No version bump,
  commit, push, tag, image publication or release was performed.
- macOS 26.5 (25F71), arm64 Mac16,10, 10 logical CPUs,
  17,179,869,184 bytes RAM, internal APFS. Initial available filesystem space:
  53,073,540 KiB. This is not a constrained 40 GB environment.
- AppleClang 16.0.0, C++20, Release (`-O3 -DNDEBUG`), CMake 4.2.3,
  Ninja 1.13.2; RocksDB 8.8.1, Boost 1.83.0, OpenSSL 3.0.12, libssh 0.12.0.
  Existing patched cpp-libp2p source:
  `c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`.
- Fresh native tree `build-sc4-20261002` was configured from this repository
  using its existing local dependency installs. Standard, fresh and UBSan
  caches all identify `/Users/pgarcgo/code/teleno`, not `teleno-extract`.
  A successful native configure/build is not a successful clean dependency
  bootstrap.

Ignored evidence root, abbreviated E:

```text
build-single-copy-evidence/sc4-20261002-sf5pZQ/
```

E includes the initial worktree status and tracked diff, SHA-256 hashes of all
pre-existing tracked/nonignored untracked files, environment, dependency hashes,
native/UBSan CMake caches, reproduction manifest and command logs. Modified
baseline files are the listed intentional SC4 extensions; unrelated baseline
files were not replaced or reset. In particular `README.md`, the high-throughput
plan, `AGENTS.md` and `CLAUDE.md` retain their initial contents; the latter two
remain byte-for-byte identical. Existing SC3 validation documentation is
historical evidence and remains unchanged.

Only new, owned, ignored scratch datasets were used. No server was contacted,
no existing node/data was operated, no producer keys were used, and no public
transactions or block production were attempted. HTTP tests use loopback;
test signatures use a deterministic, publicly known synthetic fixture seed,
not an operational signing key.

## CLI contract and invariants

### Explicit standalone actions

Before ordinary node initialization, `--backup-restore-mode direct` dispatches
exactly one of preflight, stage or activation. `--backup-restore-resume` modifies
stage only. Stage and activation require `--backup-output` for a private sibling
attempt, separate metadata scratch, an existing reviewed config, and an
independently expected chain ID. The shared parent must already exist.

Stage streams directly into the SC2 private attempt and stops at
`download_complete`. It does not open a database, validate it, create a target
DB or make it ready to activate. Explicit activation invokes SC3 read-only
validation and recoverable same-filesystem publication, or resumes its existing
activation intent. It never invokes a recursive copy fallback.

Repository mode remains the default. Direct combined convenience restore,
repository fetch/SFTP, mixed actions, orphan resume and runtime feature overrides
(including producer enablement) are rejected. Runtime backup admin has no direct
mode endpoint. The CLI does not start runtime services or clear recovery holds.

### Pinned source and phase recovery

Resume/activation read a bounded untrusted identity hint, require the original
normalized URL, network, chain and HTTPS/signature policy, then reacquire and
authenticate the pinned concrete snapshot metadata. An omitted ID means that
concrete ID; explicit `latest` or another ID is refused. Engines recheck full
identity, generation, phase and hashes under their existing lifetime leases.

This version has **no cached offline trust proof**. Original pinned source
metadata must remain available for activation, including interrupted activation
recovery. Unavailable or changed metadata blocks progress while preserving the
sole DB and journal/intent. Payload redownload is forbidden after validation
begins; repeat explicit activation, not stage/resume. This is a stated
availability dependency, not permission to weaken provenance checks.

The target config is not overwritten. Activation keeps the same DB inode,
publishes observer holds and leaves unfinished-intent startup refusal intact.
An idempotent `activated` receipt proves prior completed publication, not a new
health check of a database subsequently used by a node. Start and independently
validate the observer before considering operational readiness.

Consensus execution, normal `remove_object()`, replay/root rules, historical
exceptions, RocksDB durability, pruning and retention policy are unchanged.

### Reporting and cancellation

Successful preflight retains the SC1 v1 schema. Stage/activation and handled
direct-command failures use `teleno-direct-restore-cli` v1. Text contains a short
label followed by the same JSON fields. Reports include source trust, concrete
identity/digests, phase, configured policy, declared bounds, stage counters,
observer holds and explicitly unverified qualification gates.

On a refusal `observer_hold` is null (unknown), not a false claim that an
existing hold was removed; readiness remains false. A returned pre-validation
stage reports false; successful activation reports the engine's actual hold.

Allocation is a post-command `st_blocks * 512` sample, **not peak disk usage**.
It reacquires the common lease; a busy gate or changed tree makes the sample
unavailable, without reporting a completed activation as failed. File allocation
does not include external source storage, all filesystem metadata or OS usage.

Exit codes: 0 success, 2 unsuccessful structured preflight, 1 handled refusal or
incomplete operation, 130 cooperative interruption. SIGINT/SIGTERM cancellation
is connected to the bounded transfer/validator path. Synchronous metadata fetch,
blocking kernel I/O and physical power-loss timing remain unqualified. Unknown
options/parser errors outside direct dispatch may still use ordinary stderr.
Handled direct errors omit arbitrary transport/config exception text; their
phase and durable attempt state remain available for diagnosis.

## Verification evidence

The pre-change focused baseline passed **12/12** (`baseline-focused.log`). The
SC4 workflow adds one registered CTest test; it exercises **43 commands** against
real nine-column-family signed native genesis snapshots and a signed synthetic
inconsistent checkpoint. This is structural/local validation, not historical
execution or public peer synchronization.

| Check | Actual result | Evidence under E |
| --- | --- | --- |
| Standard native rebuild, tests enabled | Passed | `build-final-reporting.log` (earlier full build: `build-all.log`) |
| Fresh native configuration and complete build | Passed, initial 149 build steps | `configure-clean.log`, `build-clean.log`, `build-clean-final-reporting.log` |
| Focused backup/direct suites | 13/13 passed, final 31.51 s | `focused-final-reporting.log` |
| Standard full CTest | 27/28 passed, final 38.92 s | `ctest-final-reporting.log` |
| Fresh full CTest | 27/28 passed, final 47.24 s | `ctest-clean-final-reporting.log` |
| Final CLI script, standard native | 43 commands passed, included in focused suite | `focused-final-reporting.log` |
| UBSan engine and matching native CLI | 2/2 passed, final 13.80 s | `ubsan-final-reporting.log` |
| Final CLI script, UBSan native | 43 commands passed, included in UBSan suite | `ubsan-final-reporting.log` |
| State/replay regression fixtures | 5/5 passed, 0.94 s | `replay-focused.log` |
| Benchmark-script functional tests | 4/4 passed, 2.606 s; no throughput measurement | `benchmark-tests.log` |
| Native identity and help | Passed, standard and fresh trees | `version.log`, `help.log`, `version-clean.log`, `help-clean.log`, final delivery logs |
| Koinos One docs-only handoff | `mkdocs build --strict` passed, final 0.33 s | `koinos-one-docs-final.log` |
| Canonical test-enabled dependency bootstrap | Blocked, exit 1 | `canonical-build.log` |
| Docker smoke prerequisite | Blocked, no daemon; no container smoke executed | `docker.log` |

The first new CLI run failed to parse the expected JSON failure because a
non-standard metadata exception escaped the initial adapter boundary. The
adapter now catches all handled direct-operation exceptions and emits its
sanitized failure schema. `cli-first.log` retains that failed iteration;
subsequent final workflows pass. It is not hidden as a successful first run.

UBSan instruments the node/adapter and fixture helper, not rebuilt third-party
libraries. `vptr` checks are excluded because the linked RocksDB build disables
RTTI. No ASan/TSan, representative RSS profiling or CPU/storage benchmarks were
executed as part of SC4. Existing SC3 tests still pass 4,102 checks, covering 90
fault boundaries per injected-exception/process-exit mode; inherited safety
coverage is not new proof of physical power-loss behavior.

### End-to-end cases

- Metadata-only signed preflight; stage-only payload without DB open, markers,
  object repository or `.pre-restore/`; JSON/text equivalent fields.
- Same-input resume reuses completed files. Explicit latest/wrong ID, changed
  source or HTTPS policy are rejected. Mutating `latest` cannot redirect an
  existing attempt or idempotent activation.
- Loopback truncated transfer pauses; explicit resume succeeds without refetching
  latest. SIGINT while waiting for an object pauses and exits 130; resume succeeds.
- Actual cross-process lifetime lease refuses competing stage before creating an
  attempt. Target/payload modifications and unavailable signed metadata fail
  closed. A signed but incoherent DB fails offline validation and remains in
  `validating`, with source replay refused thereafter.
- Fixture process exits 99 after DB rename, leaving `activation_pending`;
  ordinary startup is refused, metadata unavailability preserves the same sole
  DB, and explicit activation recovers the matching generation/inode/hold.
- Completed activation preserves reviewed target config and the independent
  source. Observer startup on the restored genesis target reports ready, honors
  its hold despite a producer-enabled config, and shuts down cleanly via SIGINT.
  P2P, RPC and optional services are disabled; no public peers or producer keys.
- Default repository restore still follows its existing cache/stage/activation
  workflow. Existing service/admin tests remain passing.

Detailed command records, stdout/stderr, summary, journals, configs and DB trees:

```text
build-single-copy-evidence/sc4-cli-d6uwsyou/  # final standard native CLI
build-single-copy-evidence/sc4-cli-erc97wjf/  # final UBSan native CLI
build-single-copy-evidence/sc3-unit-150375849/ # final UBSan engine fault run
build-single-copy-evidence/sc4-cli-t0vfs7ys/  # preceding sample detailed below
```

Each run publishes `summary.json`, actual snapshot manifest/inventory SHA-256,
chain identity, request counts and independent-source preservation. Snapshot
file bytes may differ between freshly generated RocksDB fixtures; these hashes
identify each run, not a universal deterministic SST hash. Source preservation
is verified against its own before/after file hashes.

One representative preceding stage (`sc4-cli-t0vfs7ys`) transferred 13 files,
86,357 logical bytes, with a 184,320-byte allocated attempt sample. Its target
sample was 4,096 bytes before activation; after activation the target sample was
135,168 bytes and the attempt 69,632 bytes. Budget recommendation remained
11,006,705,664 bytes, including the unchanged 10 GiB reserve and allowances.
These small fixture/post-command samples establish neither peak usage nor
real-backup savings or 40 GB feasibility.

## Exact blockers and remaining risks

1. `teleno_restore_baseline_harness_test` retains the documented SC0 truncated
   transfer SIGBUS (native exit -10). It is the sole failure in both full suites.
   SC4's controlled direct transfer/resume test passes, but does not repair or
   waive that separate baseline fixture failure. Existing audit remediation is
   outside this milestone's authority.
2. The requested test-enabled dependency script was attempted with fresh local
   scratch overrides to preserve existing sources. Its Hunter `koinos_cmake`
   subbuild exits 1: `make: /Volumes/My: No such file or directory`, followed by
   `Build step for koinos_cmake failed: 2`. The existing Xcode SDK path contains a
   space. A supported bootstrap/toolchain path is still needed; no toolchain
   relocation or unrelated build remediation was performed.
3. Docker client 29.2.1 is installed, but `docker info` cannot connect to its
   daemon socket. No image build or container test is claimed. A running,
   authorized Docker environment is required. Direct containers must share the
   target/attempt parent and lifetime-gate inode with consistent paths/UIDs;
   mounting only the target does not establish exclusion.
4. SC5 still requires an identified trusted representative snapshot and an
   authorized isolated Linux/server-equivalent environment with measured total
   disk/RAM limits. Historical range/differential validation, advancing verified
   public head, sustained 4 GB/40 GB operation and filesystem power-loss behavior
   were not run. No capacity/consensus-parity claim follows from these fixtures.
5. Source metadata availability is required for this CLI's recovery. Resume trust
   caching, direct runtime admin, app integration, producer activation and release
   publication remain intentionally absent.

## Files changed by SC4

This list describes this task, not every pre-existing dirty file against HEAD.

Native source/test work:

- `src/main.cpp`
- `src/CMakeLists.txt`
- `src/backup/direct_restore.cpp`
- `src/backup/direct_restore.hpp`
- `src/backup/direct_restore_cli.cpp` (new)
- `src/backup/direct_restore_cli.hpp` (new)
- `tests/backup/direct_activation_test.cpp`
- `tests/cli/direct_restore_workflow_test.py` (new)

Native documentation:

- `CHANGELOG.md`
- `docs/README.md`
- `docs/backup-restore-cli.md`
- `docs/command-reference.md`
- `docs/configuration.md`
- `docs/implementation/single-copy-backup-restore-plan.md`
- `docs/memoria.md`
- `docs/operations/container.md`
- `docs/release-builds.md`
- `docs/running-observer-node.md`
- `docs/troubleshooting.md`
- `docs/validation/single-copy-restore-sc4-cli.md` (new)
- `tools/backup/README.md`

Required Koinos One documentation-only handoff, extending its existing SC3 edits:

- `docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md`
- `docs/manual/koinos-one/backup-and-restore.md`

Koinos One application code, packaging, version and submodule pin are unchanged.
Integration would require a reviewed, committed Teleno revision containing this
currently uncommitted work and matching native build identity; HEAD alone does
not identify SC4. No new app feature or released binary is claimed.

## SC4 progress and exit status

- **Implemented:** standalone preflight/stage/resume/explicit activation, pinned
  metadata policy, phase-aware recovery, bounded reporting/cancellation and
  regression coverage over the existing SC2/SC3 engines.
- **Verified locally:** signed 43-command CLI workflows, focused suites, fresh
  native build, UBSan, observer-held synthetic reopen, default repository and
  service/admin regression, docs-only frontend handoff, worktree preservation.
- **Not all exit criteria proven:** broader CTest is not green, full dependency
  bootstrap and applicable container smoke remain blocked. Do not mark the plan
  or a repository-wide Definition of Done complete.
- **Next boundary:** SC5 capacity/Linux qualification and SC6 release handoff
  require their own authorization. No VPS, deployment, release, data cleanup,
  production or audit-remediation work was performed as a side effect.

Reproduction commands and exact local dependency paths are in E's
`reproduction.md`; operator commands are in
[the CLI guide](../backup-restore-cli.md#direct-stage-resume-and-explicit-activation-sc4).
