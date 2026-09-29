# Single-copy backup restore implementation plan

Date: 2026-09-29. Status: **PROPOSED — documentation only; implementation and
capacity validation have not started.**

Planning checkout: `49b2a36f0255dfe1e0e601351c903e5989434889`, with pre-existing
tracked and untracked work. This plan describes the inspected working tree,
not a clean release. Workspace: `/Users/pgarcgo/code/teleno` on the Mac.

## 1. Goal and authorization boundary

Implement an opt-in native restore path that starts a **new observer node**
from a trusted native backup without first retaining a second complete local
copy of the backup payload. Preserve the complete database, protocol behavior,
required durability, and observer-first recovery.

Separately determine whether an identified snapshot can be restored, reopened,
and advanced within a measured **40,000,000,000-byte storage budget**. A smaller
restore footprint is not proof of sustainable operation on a 40 GB disk.

The planning request authorized documentation only. A subsequent explicit
request authorized committing and pushing this plan, not implementing it.
Later implementation requires authorization. No runtime code, configuration,
node, database, backup, release, or deployment is changed by preparing or
publishing this document. Do not contact private servers or activate production
as a side effect.

All future experiments must use repository-local scratch directories and
fixtures. Large artifacts belong under an ignored `build-single-copy-evidence/`
directory, not in committed documentation. A capacity-test volume may be mounted
under that scratch directory; never format an existing device or use a live
basedir. Public snapshot reads and isolated read-only network catch-up must use
an explicitly selected source and scope; they are not authorization to operate
an existing node. No producer keys or transaction submission are needed.

## 2. Evidence and present behavior

| Current component | Inspected behavior | Consequence |
| --- | --- | --- |
| `fetch_public_restore_snapshot()` / `fetch_missing_objects()` in `src/backup/public_restore.cpp` | Fetch metadata and missing content-addressed objects into a local backup repository | The whole downloaded payload remains local before staging |
| `stage_local_restore_snapshot()` in `src/backup/snapshot_repository.cpp` | Copy each object into a partial staging tree, verify hashes, then publish the stage | Backup payload and staged database coexist |
| `activate_staged_restore_snapshot()` | Preserve previous paths under `.pre-restore/`; move the staged DB into place | Existing data is intentionally retained |
| `move_path_with_copy_fallback()` | Attempt rename, then recursive copy on failure | A low-space path must not silently fall back to copying |
| `estimate_restore_space()` | Budget restored data, runtime files, applicable archive/existing data, 128 MiB metadata allowance; recommend another `max(10 GiB, DB / 5)` | These are current heuristics, not measured 40 GB qualification |
| CLI public restore in `src/main.cpp` | Fetch, stage, activate, and provide an observer start command | Direct mode must be deliberately routed around object caching |
| Backup service/admin and activation supervisor | Separate callers reuse restore operations | Existing callers need compatibility coverage even if direct mode is CLI-only |

The September 28 local size assessment,
`docs/performance/database-size-reduction-assessment.md`, records 29.4816 GB
active database, 29.3158 GB local backups, and 58.7988 GB total node data. That
assessment is untracked and is not included with this plan's publication. These
are historical decimal-byte observations of one deployment, not fresh
measurements of the selected restore snapshot. Current-state SSTs alone are
not a deployable full node. SC0 must collect reproducible sizing evidence
regardless of whether the local assessment is available.

For scale, 29.48 GB plus 10 GiB reserve already exceeds 40 GB before additional
metadata or an operating system. Do not lower a safety allowance just to make
the target pass. Re-read the actual manifest and measure the chosen workload.

Related work:

- [Backup and restore operator contract](../backup-restore-cli.md).
- [Quality remediation plan](teleno-audit-remediation-plan.md), especially WP1
  storage error/atomicity contracts and WP6 transfer/publication durability.
- [Chain v1.5.2 replay plan](chain-v1.5.2-fast-replay-plan.md).
- Local-only `docs/implementation/bounded-history-producer-design.md`: an
  untracked proposal not included with this publication. It describes a separate,
  unimplemented storage project, not a prerequisite for copying fewer bytes.

## 3. Selected design and exclusions

### First version: direct materialization into a private stage

Keep the existing repository-based mode as the default. Add a direct mode for
public HTTP(S) native snapshots, with `file://` and loopback HTTP sources for
isolated tests. Reuse existing transport and verification logic through small
interfaces; do not duplicate signature validation or invent a new backup format.

The direct mode downloads each manifest entry straight into its final relative
location within an owned staging tree. A partial file is renamed into that
location after successful size/hash checks. No second full object repository
is created. The complete stage becomes the active database by same-filesystem
rename, with recoverable publication of runtime files and observer markers.

Use one transfer at a time initially and bounded streaming buffers. Large
objects must not be loaded into RAM. Multiple destinations with an identical
content hash still count as separate final files: do not hard-link mutable
files merely because their contents happen to match. Download them sequentially
if necessary; single-copy refers to avoiding a second complete database image,
not eliminating required duplicate contents in the database format itself.

The source backup remains unchanged and independently recoverable. This does
not mean running the node inside a writable backup repository, nor does it
make snapshot bootstrap equivalent to independent validation from genesis.

### Initial compatibility boundary

- Accept only a fresh target: no active or previous database, `chain/blockchain`,
  conflicting restore attempt, recovery hold from another generation, or other
  managed data that would need replacement. Permit an explicitly reviewed
  config and required runtime inputs without overwriting them.
- Recheck ownership and emptiness under a restore lock immediately before
  activation. A RocksDB LOCK check alone does not serialize two restores or a
  concurrent first node startup; define a common startup/restore exclusion gate.
- Reject cross-filesystem staging and activation. Do not invoke the current
  recursive-copy fallback in direct mode.
- Leave local-repository and private-SFTP restore behavior unchanged in v1.
  A local source on the same disk still occupies space and must be counted.
- Do not expose direct mode through a running node's admin API in v1. Preserve
  existing admin behavior and reject unsupported combinations explicitly.
- Do not change pruning, historical retention, RocksDB compression, consensus,
  resource accounting, WAL policy, or fsync guarantees.
- Do not automatically delete backups, abandoned user data, or `.pre-restore/`.

SST hard-link sharing is a separate optional follow-up. RocksDB checkpoints
[share immutable SSTs on the same filesystem while copying mutable metadata](https://github.com/facebook/rocksdb/wiki/Checkpoints).
It can reduce local recovery-snapshot duplication, but shared files are not an
independent backup and retained snapshots can pin obsolete SST generations.
Direct download does not depend on APFS clones, reflinks, or hard links.

## 4. Correctness and recovery contract

### Trusted and pinned input

Resolve `latest` once, then persist the selected backup ID, manifest/inventory
digests, network/chain identity, source identity, format version, and relevant
compatibility data. Never resolve `latest` again during a resumed attempt.

Preserve configured signature requirements and HTTPS policy. Validate an
authenticated manifest/inventory before trusting its file digests. Qualified
public-source tests must use a trusted signing key or an independently supplied
trusted digest; a checksum downloaded from the same untrusted source is not
authentication. Document the source trust assumption explicitly, without
quietly tightening or weakening legacy-mode behavior.

Reject unsupported formats, contradictory inventory totals, unsafe or duplicate
destination paths, case-folding collisions, symlink escapes, and arithmetic
overflow. Budget metadata size, entry count, object sizes, total bytes, retries,
timeouts, and cancellation. The transfer limit applies while receiving bytes,
not only after an oversized response has filled the disk.

### Durable state machine

Use a versioned, small journal outside the database subdirectory, bound to the
target and attempt ID. Only directories created and marked by this attempt may
be resumed or cleaned by it. Proposed states:

`planned -> downloading -> download_complete -> validating -> verified -> activation_pending -> activated`

Cancellation and failure preserve an explicit resumable or failed state; they
are never aliases for `verified`. Store progress per file rather than rewriting
an unbounded manifest on every received chunk. Specify checked writes,
file/directory synchronization, and rename ordering for each supported platform.
Rename alone is not a power-loss durability guarantee.

On resume, verify input identity and re-hash completed files before reuse. The
first version may restart an incomplete individual file instead of implementing
HTTP range resume. Reclaim only its own partial file before retrying; never
accumulate multiple failed generations. Detect externally changed completed
files. Corrupt input must not produce a completion marker or canonical target.

### Activation and first startup

Validate file completeness and network/genesis identity, then check that the
staged database opens coherently in an isolated offline validation step with
production, networking, and optional writers disabled. If this step writes WAL
or recovery output, account for it and journal that transition; do not keep
claiming the opened files have the original source hashes. Confirm source bytes
before opening, and validate database contents and recovery identity afterward.

Resume is phase-specific. Once offline validation has begun, do not redownload
source files over a database that recovery may have changed. Recover and recheck
that owned database through the validation path, or mark the attempt failed and
retain diagnostics. A fresh download then requires an explicitly selected new
attempt and its space budget, or approved cleanup of the failed owned attempt.

Publish the recovery hold/activation intent durably before the database becomes
eligible for startup. Recovery after a crash between renames must deterministically
finish publication or remain blocked, without deleting the only valid database.
An existing `RESTORE_STAGE_COMPLETE` file is not sufficient authority to activate
an unrelated, modified, or incompletely validated stage. Bind activation to the
validated journal generation and revalidate necessary artifacts.

Keep `.backup-just-restored` and `.backup-observer-recovery` semantics. Every
restart after any activation interruption must either refuse startup or remain
observer-only, even if an existing configuration enables production. Do not
automatically release the hold. Observer progress must use normal verified
replay: fresh roots, exact historical exception, required lookahead, at most
one full reexecution, and no canonical/index effects after rejection.

All read/write/flush/open failures must remain visible. Before operational
qualification, reassess the relevant WP1/WP6 audit findings at the current
checkout. Prototype file transfer can proceed independently; unresolved storage
or publication failures block claims of safe end-to-end restore. Do not silently
expand this feature into unrelated RPC/cache remediation.

## 5. Space and memory accounting

Define and report quantities in bytes, with explicitly labeled GB/GiB displays:

- `S`: allocated final database and required runtime payload, conservatively
  derived from validated inventory and filesystem allocation overhead.
- `J`: metadata, journal, directory entries, and bounded diagnostic logs.
- `T`: additional transfer temporary space. A partial file that becomes its
  final file is part of `S`, not an extra complete object cache.
- `V`: peak additional offline validation/recovery writes.
- `R`: operational reserve for WAL, compaction, catch-up, logs, and growth.
- `E`: existing usage on the destination filesystem, including any local source
  backup, installed runtime, preserved data, or unrelated user files.

Model incremental restore demand as `S + J + T + V`; model qualification against
total capacity as `E + S + J + T + V + R`, refining mutually exclusive peaks
only with measured evidence. When resuming, subtract verified bytes already
allocated by this attempt from additional demand rather than double-counting
them. Keep logical size, allocated size, and filesystem available space distinct.

Recheck available space at bounded intervals and before expensive transitions.
ENOSPC must leave a recoverable attempt and an actionable diagnostic. Do not
silently consume the operating reserve. Preserve the conservative existing
recommendation until an independently reviewed workload-based policy replaces
it; report a below-recommendation condition instead of promising 40 GB support.

No fixed node-size ceiling is promised for an archival node that keeps growing.
Report observed minimum free space, peak usage, steady-state usage, test duration,
and growth over the exact processed range. Disk-capacity success and memory
boundedness are separate results; report transfer buffers, metadata footprint,
and peak RSS so disk savings do not hide payload-sized memory allocation.

## 6. Work packages and exit gates

Use the `SC` prefix to distinguish this work from existing audit/replay packages.
Complete each checkpoint and record evidence before moving to dependent work.

### SC0 — Baseline and reproducible fixtures

**Files/components:** current backup tests; proposed fixture/measurement helper
under `tools/backup/`; ignored evidence manifest.

1. Read `AGENTS.md`, relevant restore/storage plans, and current dirty changes.
   Record commit, dependency/compiler versions, build flags, OS, filesystem,
   RAM, source snapshot identity, available disk, and exact test configuration.
2. Verify build caches point into this checkout, not `teleno-extract` or another
   workspace. Preserve foreign/pre-existing build trees; use a clean local
   configuration after reviewing supported build-script options.
3. Run the existing baseline. Establish a tiny valid multi-column-family
   RocksDB snapshot and bounded local transfer fixtures, including signed input.
4. Measure current fetch/stage/activation allocation with matching contents.
   Characterize relevant known audit failures separately from new regressions.

**Exit:** baseline results and fixture hashes recorded; current duplication
reproduced; unresolved storage/durability blockers classified. **Risk:** a tiny
fixture proves workflow behavior, not full-node capacity. **Rollback:** no
runtime change; preserve evidence and source fixtures.

### SC1 — Mode contract, shared metadata validation, and preflight

**Files/components:** `public_restore.cpp/.hpp`, `snapshot_repository.cpp/.hpp`,
new focused direct-restore component if needed, `src/main.cpp`, backup tests.

1. Extract a validated, immutable restore specification from metadata acquisition
   without requiring a fully populated local object repository.
2. Add a mode-aware space estimator and target eligibility/ownership checks.
   Make metadata-only planning available without downloading payloads or
   opening the target DB; identify any small scratch writes in its output.
3. Specify the opt-in CLI, JSON schema, and error/exit behavior. A proposed
   selector is `--backup-restore-mode direct`; it **does not exist yet**. Keep
   the legacy default. Provide separate preflight and stage-only actions, plus
   explicit activation of a validated attempt; finalize names before coding.
4. Add tests for modes, mixed/unknown flags, incompatible layouts, bad metadata,
   space arithmetic, existing targets, and source/target filesystem accounting.

**Exit:** deterministic preflight gives the correct mode, selected input,
eligibility, required/available bytes, recommendation, and rejection reason;
legacy tests pass. **Risk:** accidentally changing existing callers.
**Rollback:** leave default routing unchanged and remove the unactivated opt-in.

### SC2 — Bounded direct transfer and resumable staging

**Files/components:** public transport helpers, proposed direct-restore module,
`tests/backup/backup_public_restore_test.cpp`, proposed direct-restore tests.

Implement streamed download into final stage-relative paths, incremental
size/hash enforcement, pinned metadata, the journal, and safe file-level resume.
Handle cancellation, finite retries, truncated/oversized responses, hash failure,
disk full, duplicate content, and source changes. Never consume or alter a source
repository. Metadata may be cached; a second complete payload may not be.

**Exit:** hashes match the legacy stage for identical input; interruption/resume
redownloads only incomplete or invalid files; no whole-object RAM buffering or
whole-payload cache is present; allocation accounting matches the model.
**Risk:** partial-file ownership and source drift. **Rollback:** preserve failed
attempts, disable opt-in, and use a separate legacy target with its own budget.

### SC3 — Offline validation and crash-recoverable activation

**Files/components:** activation helpers, `restore_activation_supervisor.*` as
needed for safe shared validation, startup exclusion/marker handling in
`src/main.cpp`, backup snapshot and recovery tests.

Implement the validation and durable activation contract in section 4. Explicitly
reject populated targets and cross-device moves. Make startup/restore exclusion
effective across processes. Test every journal, sync, marker, and rename boundary
with injected failures and process interruption, including a config that enables
production. Retain the established legacy `.pre-restore/` behavior unchanged.

**Exit:** failure leaves a valid resumable stage or coherent observer-held target;
no overwritten user data, automatic copy fallback, premature success, or producer
startup; clean and interrupted reopen tests pass. **Risk:** highest-risk package;
file-level atomic operations are not a multi-file transaction. **Rollback:**
recover the owned attempt using its journal, not by deleting target chain data.

### SC4 — CLI integration and regression coverage

**Files/components:** `src/main.cpp`, CLI/config parsing only where necessary,
`src/CMakeLists.txt`, `tests/backup/`, `tests/cli/`, existing service/admin tests.

Wire preflight, stage-only, resume, and explicit activation through the common
engine. Report source trust, attempt state, measured bytes, observer hold, and
limitations in text and JSON. Keep direct mode unavailable from runtime admin
until separately designed. Do not persist source credentials in journals/logs.
Update operator/configuration/command documentation and CHANGELOG for actually
implemented behavior; do not change release identity merely for this feature.

**Exit:** end-to-end CLI tests verify both modes and unsupported combinations;
focused suites, broader CTest, version/help, and applicable container smoke pass.
**Risk:** concurrent edits already exist in main/config/CMake/docs; integrate
carefully without replacing user changes. **Rollback:** retain legacy mode and
source-format compatibility; no automatic migration or data cleanup.

### SC5 — Mac capacity experiment and Linux portability qualification

**Files/components:** proposed `tools/backup/validate_single_copy_restore.py`,
test harness registration as appropriate, ignored logs/metrics, a new validation
report under `docs/validation/` when results exist.

1. Run small functional/failure tests first. Then create a disposable capacity-
   limited filesystem using a new image or quota, with image and mount paths
   confined to scratch. Record the effective usable capacity, not just its label.
   Check host free space too; a sparse image must not exhaust the host.
2. Keep the independent fixture source outside that destination volume and
   disclose its storage. Test direct destination writes without clone/hard-link
   shortcuts that would conceal duplicate allocations. Collect filesystem free
   space and allocated-file metrics, including file replacement/open-file effects.
3. Compare legacy and direct modes on the same snapshot in a sufficiently large
   volume first. Then test **40 GB data-volume** capacity. Record the exact
   failure phase if preflight, validation, activation, or catch-up cannot fit.
4. Use at least five paired runs of short fixtures; report median, spread, peak
   bytes, minimum free space, RSS, and elapsed time. Repeat a representative
   full-size restore at least twice if resources permit; disclose actual count.
5. Reopen, advance through a recorded deterministic replay range, and compare
   canonical IDs, state results/roots, and relevant receipts with legacy restore
   of the same input. Do not force compaction in a live node to create evidence.
6. Run at least a 24-hour isolated representative observer workload, two clean
   restarts and one injected process interruption. Exercise bounded catch-up,
   WAL rotation and compaction; record whether those events actually occurred.
   If unavailable, mark those operating-space checks unverified. Use fixtures
   first; any public catch-up requires the source/scope selection described above.
7. Repeat critical durability and capacity checks in a disposable Linux VM with
   an ext4 data volume. Mac/APFS evidence alone does not prove Linux behavior.

Two different claims need different experiments:

- **40 GB data volume:** includes all destination node data, temporary files,
  restore metadata, runtime logs and local backups if enabled. Excludes host OS
  and build/source files; report the exclusion prominently.
- **40 GB complete machine disk:** requires a separate disposable VM image
  containing OS, installed runtime/dependencies, logs, and node data within the
  measured total. A 40 GB mounted test volume does not establish this claim.

**Exit:** measured single-copy benefit and an explicit pass/fail/blocked result
for each capacity profile. A capacity rejection is a valid result, not permission
to weaken guarantees. **Risk:** insufficient fixture coverage, host space, missing
Linux tooling, or unobserved compaction peaks. **Rollback:** stop only owned test
processes; preserve reports, then remove only explicitly approved scratch data.

### SC6 — Documentation and release-readiness handoff

**Files/components:** this plan; `docs/backup-restore-cli.md`,
`docs/command-reference.md`, `docs/configuration.md`,
`docs/running-observer-node.md`, `docs/troubleshooting.md`,
`docs/release-builds.md`, `CHANGELOG.md`, and `docs/README.md` where applicable.

Document supported modes, fresh-target restriction, source trust, exact tested
snapshot/ranges, resume/cleanup ownership, failure recovery, separate backup
requirements, and qualified storage claims. Link evidence and record blockers.
Do not advertise an untested CLI or describe 40 GB as a universal minimum.

**Exit:** reviewable source/docs/test package with all required gates proven or
explicitly blocked. **Risk:** overstating a successful small-fixture test.
**Rollback:** retain current operator instructions until behavior ships; feature
release and any external deployment/publication remain pending authorization.

## 7. Required test matrix and execution commands

| Area | Minimum cases and expected result |
| --- | --- |
| Input identity | Valid signed fixture; missing/invalid required signature; wrong chain; changed pinned inventory; changing `latest`; unsupported version. Invalid inputs never activate |
| Paths and counts | Relative-path violations, duplicate/case-colliding entries, symlinked destination, overflow, excessive metadata, repeated hashes at distinct paths. Reject or handle within the declared budget |
| Transfer | Truncation, extra bytes, timeout, cancellation, hash mismatch, finite retries, ENOSPC, failed writes/sync. No false completion or unbounded growth |
| Resume | Crash mid-file, after rename but before journal sync, stale journal, changed completed file, source outage. Revalidate and safely resume or fail |
| Activation | Crash at every publication boundary; two restores; startup race; unexpected target data; cross-device request. No overwrite, hidden full copy, or production activation |
| Persistence | Multi-CF DB, WAL-dependent recovery, corrupted/missing SST, incompatible layout, validation writes, clean/unclean reopen. Report failure; never infer success from metadata alone |
| Compatibility | Legacy backup/local/SFTP/public/service/admin paths; optional indexes; normal replay and historical replay fixtures. Preserve logical results and observer holds |
| Resources | Large file with bounded buffers; many files; same-volume local source; reserve depletion; allocated versus apparent bytes. Include all destination storage |

Future implementation workflow, not commands executed for this planning task:

```bash
KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R '^koinos_backup_'
ctest --test-dir build --output-on-failure -R 'koinos_(state_db|controller_delta|indexer_lookahead|historical_fast_replay)'
ctest --test-dir build --output-on-failure
./build/teleno_node --version
./build/teleno_node --help
```

Confirm tests were discovered, built, and actually run; a zero-match filter is
not passing evidence. Add direct-mode tests to CTest and include their names in
the recorded commands. Run supported ASan/UBSan checks in separate local builds,
and relevant lifecycle/concurrency checks where tooling supports them. Container
smoke must use disposable mounts and disabled production. Record unsupported
checks rather than inventing results. No full native build is required merely
to add this proposal.

The harness must record its exact image/quota creation, mount, workload, metric
sampling, and cleanup commands, tool versions, seeds, source digests, and exit
codes. Process-kill fault injection is not proof of physical power-loss safety;
state the level of crash testing actually performed.

## 8. Acceptance, open prerequisites, and progress log

Feature Definition of Done:

- [ ] Default restore behavior remains compatible; direct mode is explicit.
- [ ] Direct mode preserves all required snapshot contents with no second full
  local payload, and uses bounded memory/transfer resources.
- [ ] Input identity, provenance policy, file integrity, coherent DB reopen, and
  unchanged protocol/replay outcomes are verified at the stated evidence level.
- [ ] Fresh-target restrictions, process exclusion, resume, disk-full handling,
  checked durability, and interrupted activation tests pass.
- [ ] Observer recovery holds survive all tested activation/startup failures.
- [ ] Relevant existing storage/publication defects are resolved or documented
  as blockers; no unresolved defect is hidden behind a successful download.
- [ ] Native/focused/full tests, CLI identity/help, Mac/Linux qualification, and
  applicable container checks have recorded actual outcomes.
- [ ] English operator documentation and a reproducible evidence report exist.

Additional **40 GB claim gate**, separate from feature implementation:

- [ ] Selected source snapshot, input range, target filesystem, actual usable
  capacity, OS/runtime exclusions, reserve policy, and optional services pinned.
- [ ] Full restore, validation, activation, reopen, and bounded sustained observer
  work fit the stated budget without reducing durability or required data.
- [ ] Peak/steady allocation, minimum free bytes, growth, WAL and compaction
  evidence recorded; data-volume and whole-machine claims kept separate.
- [ ] Any failed or missing check blocks that claim. No indefinite lifetime or
  all-future-snapshots guarantee is inferred from the finite experiment.

Open prerequisites before qualification: an identified representative trusted
snapshot and deterministic successor-block fixture; enough Mac scratch space;
an isolated Linux/ext4 test environment; agreed platform durability semantics;
and current disposition of the relevant audit findings. These have not been
verified or provisioned by this documentation task.

| Package | Current status | Evidence / next gate |
| --- | --- | --- |
| Planning | Complete as a proposal | Current restore code and prior sizing report inspected; this file added |
| SC0 | Not started | Record reproducible build/test and duplication baseline |
| SC1 | Not started | Validate interface, metadata-only preflight, and budget model |
| SC2 | Not started | Prove bounded, resumable direct materialization |
| SC3 | Not started | Prove recovery and observer-only activation |
| SC4 | Not started | Integrate CLI and pass compatibility suites |
| SC5 | Not started | Measure Mac/Linux results; qualify or reject each 40 GB claim |
| SC6 | Not started | Complete implemented-behavior documentation and review handoff |

For each implemented package append: files changed; invariants; commands and
actual outcomes; artifact paths; remaining risks/blockers; and exit-gate status.
Do not mark implementation complete merely because this plan exists. No build,
restore, capacity experiment, or historical/differential validation was run to
prepare this document.
