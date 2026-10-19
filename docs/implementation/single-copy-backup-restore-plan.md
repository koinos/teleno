# Single-copy backup restore implementation plan

Date: 2026-09-29. Updated: 2026-10-03. Status: **SC0 baseline collected with
recorded blockers; SC1 planning, SC2 staging and SC3 internal offline validation/
observer activation implemented. SC3 verification is recorded separately;
SC4 standalone CLI integration is implemented with recorded validation/blockers;
SC5 local measurement tooling is implemented with synthetic evidence; actual
capacity/Linux qualification remains blocked; SC6 documentation handoff is
prepared with release readiness blocked.**

Resource adaptation follow-up (2026-10-03): the
[low-resource mainnet observer plan](low-resource-mainnet-observer-plan.md)
adds LR0–LR6 for the selected 2-vCPU / 4-GB / 40-GB whole-server target.
LR0 local baseline tooling is implemented with [recorded results and blockers](../validation/low-resource-lr0-baseline.md);
LR1–LR6 remain proposed and full-history capacity qualification remains blocked.
It keeps SC results/gates intact and, following the user's clarification,
requires complete history from genesis with lossless storage improvements only.
[`teleno_light`](teleno-light-concept.md) is a separate deferred concept, not
a fallback for this target. No low-resource profile, retention change or
replacement reserve policy is implemented yet.

Planning checkout: `49b2a36f0255dfe1e0e601351c903e5989434889`, with pre-existing
tracked and untracked work. This plan describes the inspected working tree,
not a clean release. Workspace: `/Users/pgarcgo/code/teleno` on the Mac.

Latest qualification update (2026-10-15): the user reaffirmed the full-backup
40 GB whole-server requirement. The designated replacement VPS is now available
for isolated qualification, without production activation. The separately
updated observer publisher now provides checkpoint-verified public identity.
Current native VPS metadata preflight passes source/minimum checks, but the
unchanged space model has a 4.140 GB deficit and stage refuses before payload or
target creation. Full historical/source-signature and capacity gates remain.
See [current evidence](../validation/restore-40gb-current-baseline.md).
Earlier authorization and host observations below retain their historical
context; the current [issue 9 contract](issue-9-goal.md) controls the active scope.

## 1. Goal and authorization boundary

Implement an opt-in native restore path that starts a **new observer node**
from a trusted native backup without first retaining a second complete local
copy of the backup payload. Preserve the complete database, protocol behavior,
required durability, and observer-first recovery.

Separately determine whether an identified snapshot can be restored, reopened,
and advanced within a measured **40,000,000,000-byte storage budget**. A smaller
restore footprint is not proof of sustainable operation on a 40 GB disk.

### Intended server outcome (user clarification, 2026-10-01)

The intended result is to launch Teleno from an independently stored native
backup on the user-selected Linux VPS shown in the supplied screenshot:
**2 vCPUs, 4 GB RAM and 40 GB local disk**. Treat this as the whole-server disk
profile, including Ubuntu, installed runtime, logs and restore metadata, not
just a separate 40 GB data volume. The independent backup must not need a
second complete local payload before startup. Start as an observer, validate
database/network identity and demonstrate advancing verified head after restore.

These are screenshot-advertised resources, not measured usable bytes, kernel,
CPU architecture, filesystem, free space or runtime readiness. Qualification
must measure the actual server-equivalent environment, peak/steady RSS within
4 GB, disk reserve and sustained operation. A snapshot that cannot fit safely
must be rejected; do not weaken durability, validation or reserves to pass.
No universal or indefinite 40 GB capacity guarantee is implied. Keep host names,
addresses and credentials out of public documents. Repository implementation
does not authorize connecting to, deploying on or modifying that server.

Subsequent read-only observations on 2026-10-03 established Linux x86_64/ext4,
2 processors, 4,000,043,008 bytes RAM, a 39,956,590,592-byte root filesystem and
36,146,835,456 available bytes at the recorded sample. Those facts supersede
the screenshot-only resource uncertainty, not the missing qualification. The
selected Foundation snapshot declares about 29.40 GB, has source-identity/trust
blockers, and exceeds that free space under the existing reserve model. See the
low-resource plan for exact evidence, assumptions and proposed adaptation.

The planning request authorized documentation only; a subsequent request
authorized committing and pushing the plan. The request "Implement SC0"
authorizes baseline tooling, builds and isolated synthetic restore fixtures.
The subsequent "Implement next milestone" request authorizes SC1 metadata
planning and tests, not SC2–SC6, unrelated production remediation, live-node
operation or publication of a release. The SC0 tools exercise existing APIs
against new scratch data without changing runtime defaults or implementing
direct restore.
The 2026-10-01 follow-up authorizes SC2 bounded direct transfer and resumable
staging in isolated local fixtures, plus this server goal and memory update.
SC3 activation, SC4 operational CLI integration, SC5 qualification and SC6 release
handoff remain later milestones; no server operation or publication is authorized.
Do not contact private servers or activate production as a side effect.

The 2026-10-02 "proceed with the next milestone" request authorizes SC3 native
offline validation, crash-recoverable activation and isolated local verification.
It does not authorize operational CLI integration, server access, deployment,
commit/push, release, data cleanup or block-production activation. Required
Koinos One documentation handoff is documentation-only, not app integration.

All future experiments must use repository-local scratch directories and
fixtures. Large artifacts belong under an ignored `build-single-copy-evidence/`
directory, not in committed documentation. A capacity-test volume may be mounted
under that scratch directory; never format an existing device or use a live
basedir. Public snapshot reads and isolated read-only network catch-up must use
an explicitly selected source and scope; they are not authorization to operate
an existing node. No producer keys or transaction submission are needed.

The subsequent 2026-10-02 request authorizes SC4 standalone CLI integration,
end-to-end local regression and documentation handoff only. It does not authorize
SC5/SC6 execution, audit remediation, server/live-node actions, production,
commit/push, package/submodule integration or publication.

The subsequent "Next step" request authorizes SC5 local measurement tooling,
owned synthetic experiments and documentation. It does not authorize live VPS
access, producer operation, audit remediation, cleanup of existing datasets,
commit/push or release. Missing representative inputs/platforms remain explicit
qualification blockers, not permission to substitute a tiny fixture.

The 2026-10-03 "Next step" continues local work with the independent SC6
documentation/checklist preparation and bounded existing verification. It does
not waive preceding SC4/SC5 gates or authorize missing-source selection, VM/VPS
provisioning, audit remediation, cleanup, app integration, commit/push or release.

The subsequent request to adapt requirements to the selected server authorizes
the LR planning/documentation supplement. It proposes a separately measured
operational reserve and resource optimization, not weakened consensus,
durability, authentication or automatic pruning. The user's subsequent
clarification makes complete history mandatory and moves bounded history to
the separate deferred `teleno_light` concept. It does not authorize runtime
implementation, server writes, deployment, publisher changes or publication.

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

These exclusions also govern the current full-history LR follow-up. LR may
implement lossless archival storage improvements and an independently qualified
resource policy in separate milestones; complete history, consensus, required
state/tail, source-trust, WAL/fsync and preservation boundaries remain unchanged.
Bounded retention belongs only to a separately authorized future project. Do not lower
the current reserve to pass SC5; any replacement must satisfy LR's measured
policy and compatibility gates before it can qualify a different profile.

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
3. Specify the opt-in CLI, JSON schema, and error/exit behavior. SC1 implements
   `--backup-restore-mode direct --backup-restore-preflight`, with separate
   `--backup-metadata-dir` and optional `--backup-expected-chain-id`. The default
   remains `repository`. Reserve `--backup-restore-stage` for stage-only work
   and `--backup-restore-activate` for explicit validated-attempt activation;
   direct invocations of those actions fail closed until SC4 integrates the
   SC2 engine with SC3 validation/activation.
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

Implementation detail (2026-10-02): validation uses `OpenForReadOnly` for every
existing CF, including in-memory WAL recovery; it does not invoke writable
recovery or runtime services. It rehashes pinned files before/after opening,
checks genesis/expected chain ID, unified schema, every iterator and checksum,
state size/header/ID/root and stored checkpoint ancestry. This is trusted-backup
storage/identity consistency, **not** a recomputation of historical consensus
or proof that the existing WP1 runtime storage defects are repaired. The expected
chain ID must be independently selected in native `0x` multihash hex form.
Geneses over 32 MiB, WAL input over 128 MiB, oversized block records or recovery
requiring a writable open are refused with the sole stage preserved.

Validation report, activation intent and observer markers bind the same attempt
generation and manifest/inventory hashes. A lifetime-held POSIX sidecar gate
excludes node startup and direct restore across processes. The intent is durable
outside the target before markers/runtime/DB placement. A DB is moved once by
exclusive same-filesystem rename; no copy or overwrite fallback exists. An
unfinished intent blocks node startup until explicit offline recovery completes.
SC4 connects these engines to explicit CLI actions below; internal fault/fixture
test functions are still not operator tools.

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

Implementation detail (2026-10-02): exactly one direct action is supported:
preflight, stage, or explicit activation. `--backup-restore-resume` modifies only
stage; `--backup-output` identifies the private sibling attempt. Stage/activation
require an independently expected chain ID. Resume/activation omit backup ID or
select the pinned concrete ID and explicitly reject `latest`. The first CLI
version re-authenticates pinned metadata from the original available source;
it does not persist an offline trust proof, alter provenance policy or replay
payload after validation. Text/JSON expose phase, trust, declared limits,
post-command allocation samples, observer hold and unverified gates. Repository
remains the default; direct runtime admin, SFTP, combined convenience restore,
feature overrides and unsupported combinations fail closed. See the
[SC4 report](../validation/single-copy-restore-sc4-cli.md) for actual outcomes,
not an all-green or deployment-ready claim.

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
8. Qualify the intended **2-vCPU / 4-GB-RAM / 40-GB whole-server** profile from
   section 1 using an Ubuntu-equivalent OS/runtime footprint. Record actual
   usable disk, architecture, filesystem, OS/service memory, peak and steady
   node RSS, swap allocation and catch-up/head progress. Resource claims must
   cover the full server, not just transfer buffers or an empty data volume.

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
- [ ] The selected input and restored node preserve complete canonical history
  from the first network block and all backed-up historical block/receipt records;
  full-corpus/continuity evidence, not merely sampled replay, proves retention.
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
and current disposition of the relevant audit findings. SC0 provides a tiny
synthetic baseline, not these representative capacity prerequisites.

| Package | Current status | Evidence / next gate |
| --- | --- | --- |
| Planning | Complete as a proposal | Current restore code and prior sizing report inspected; this file added |
| SC0 | Baseline complete with recorded blockers | [Build/test, signed fixtures and measured duplication](../validation/single-copy-restore-sc0-baseline.md); canonical build workflow and diagnostic error-formatting discrepancy recorded |
| SC1 | Metadata-planning exit gates satisfied; broader blockers recorded | [Contract, 68 unit checks, 9/9 focused tests, UBSan and 23/24 broader tests](../validation/single-copy-restore-sc1-preflight.md) |
| SC2 | Local staging exit gates satisfied; shared/operational blockers recorded | [Streaming, pinned resume, 11/11 focused tests and 25/26 broader tests](../validation/single-copy-restore-sc2-staging.md); no server qualification |
| SC3 | Internal engine implemented; local validation and remaining gates recorded | [Offline consistency, process-fault recovery and observer startup report](../validation/single-copy-restore-sc3-activation.md); operational/portability qualification remains pending |
| SC4 | Standalone CLI implemented; all-green exit gate remains blocked | [Signed fixture CLI, pinned resume/recovery, regression and build report](../validation/single-copy-restore-sc4-cli.md); full-suite/environment and operational qualification remain gated |
| SC5 | Local tooling/synthetic comparison implemented; qualification blocked | [SC5 measurements and capacity blockers](../validation/single-copy-restore-sc5-capacity.md); no 40 GB or Linux claim |
| SC6 | Documentation checklist prepared; release readiness blocked | [SC6 review handoff](../validation/single-copy-restore-sc6-handoff.md); preceding qualification gates remain open |
| LR0–LR6 | LR0 local baseline implemented; LR1–LR6 proposed, qualification blocked | [Low-resource mainnet observer plan](low-resource-mainnet-observer-plan.md) and [LR0 evidence](../validation/low-resource-lr0-baseline.md); no 40 GB or mainnet memory claim; `teleno_light` separate/deferred |

For each implemented package append: files changed; invariants; commands and
actual outcomes; artifact paths; remaining risks/blockers; and exit-gate status.
Do not mark implementation complete merely because this plan exists. The
original planning task ran no build or restore experiments. SC0 subsequently
ran a fresh local native build, existing tests and isolated synthetic restore
measurements; its [progress log](../validation/single-copy-restore-sc0-baseline.md#sc0-progress-log-and-exit-criteria)
records files, invariants, actual commands/results, evidence and blockers.
No 40 GB capacity or historical/differential qualification was performed.

SC1 subsequently added the opt-in metadata-only CLI, immutable validated input,
fresh-target inspection and mode-aware budget, without object acquisition or DB
opening. Its [progress log](../validation/single-copy-restore-sc1-preflight.md#remaining-blockers-and-sc1-progress-log)
lists changed files, invariants, actual commands/results and retained artifacts.
At the SC/LR0 baseline, SC0's truncated-response diagnostic was the only broader
CTest failure. The subsequent [LR1 prerequisites report](../validation/low-resource-lr1-prerequisites.md)
records its correction, current suite results and scoped persistence/publication
fixes. ASan initialization and container availability were baseline blockers;
they are not implied resolved by local LR1 tests.
SC2's [progress log](../validation/single-copy-restore-sc2-staging.md#sc2-progress-log-and-exit-status)
records bounded transfer, pinned metadata/journal, rehash-based resume, fault
coverage and the remaining baseline/environment blockers. The intended outcome
is backup-based observer launch on the user-selected 2-vCPU, 4-GB-RAM, 40-GB
Linux server; that outcome is not proven by local staging or SC3's genesis smoke.
SC3's [progress log](../validation/single-copy-restore-sc3-activation.md#sc3-progress-and-exit-status)
records its actual verification and missing qualification.
SC4's [progress log](../validation/single-copy-restore-sc4-cli.md#sc4-progress-and-exit-status)
records local CLI evidence, blocked checks and the pending representative/Linux
gates. SC5's [progress log](../validation/single-copy-restore-sc5-capacity.md#sc5-progress-and-exit-status)
records five paired local measurements and an actual small-volume reserve
rejection. Representative/Linux/40 GB qualification remains pending. SC6's
[documentation handoff](../validation/single-copy-restore-sc6-handoff.md#sc6-progress-and-verification-record)
records preserved boundaries and a reviewable unexecuted verification/release
sequence; it does not mark dependent readiness gates complete.
