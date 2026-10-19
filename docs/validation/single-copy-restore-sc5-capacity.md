# Single-copy restore: SC5 local measurements and qualification blockers

Date: 2026-10-02. Scope: the local part of
[SC5](../implementation/single-copy-backup-restore-plan.md).

**Measurement tooling and synthetic comparison are implemented. SC5 is not
complete, and neither 40 GB profile is qualified.** The 1 GiB APFS experiment
rejects staging at the unchanged reserve. The correctly sized 40 GB experiment
is blocked before image creation by host backing-space safety. Representative
snapshot/range, Linux/ext4 and sustained whole-server evidence remain missing.

## Scope, environment and preservation

The request "Next step" authorizes owned local diagnostics, not live VPS changes,
producer activation, audit remediation, cleanup, commit/push or release. This
work adds no runtime feature/default, consensus change or retention policy.
The infrastructure-review guardrail keeps starting, sustained sync and capacity
qualification separate. Provider comparison, producer health and community
review are outside this task; no live hosts were inspected.

Repository: /Users/pgarcgo/code/teleno, branch codex/reward-reinvestment-macos,
HEAD cfd2eb83b8080ca49911555108ce05f0a9068f01 plus existing SC0–SC4/user work and
these uncommitted diagnostics. Binary:
teleno_node 1.3.0-dev.0+cfd2eb83b808-dirty. macOS 26.5 (25F71), arm64 Mac16,10,
10 logical CPUs, 17,179,869,184 bytes RAM; APFS, not an isolated 4 GB machine.
AppleClang 16.0.0, CMake 4.2.3, Ninja 1.13.2, Release -O3/-DNDEBUG; existing
RocksDB 8.8.1, Boost 1.83.0 and patched cpp-libp2p dependencies reused.
Driver Python 3.12.13; unit helpers also passed on system Python 3.9.

Standard build, fresh native build-sc4-20261002 and separate UBSan
build-sc3-ubsan-20261002 have repository-local CMake caches. No foreign workspace
build was used. The full dependency bootstrap was not rerun: its recorded SC4
SDK-space blocker is still unresolved; these are rebuilds of native/test code
using installed local dependencies, not a successful clean dependency bootstrap.

Ignored evidence root E:

```text
build-single-copy-evidence/sc5-20261002-9qiMaA/
```

E contains the starting worktree patch/status, SHA-256 of all pre-existing
tracked/nonignored untracked files, frontend-doc starting patch/hashes, command
logs and reproduction manifest. README.md, AGENTS.md, CLAUDE.md, the throughput
plan and unrelated baseline files remain unchanged; AGENTS/CLAUDE remain
identical. Intentional edits extend overlapping source/test/docs rather than
resetting user work. Existing SC0–SC4 reports remain unchanged.
All snapshots, nodes, logs, images and targets are newly owned ignored scratch
data. No existing chain/state or backup was removed, and retained images/data
were not cleaned up to make capacity tests pass.

## Implemented experiment

tools/backup/validate_single_copy_restore.py verifies a local test-enabled build,
creates a unique evidence directory, and invokes the native CLI and its matching
fixture helper. The helper adds:

- --sc5-prepare: signed real nine-CF Harbinger genesis, plus 8,192 deterministic
  4,096-byte optional contract-metadata rows (32 MiB, xorshift seed 0x5c0005,
  no compression). No consensus state object/root is invented for this padding.
- --sc5-inspect: freshly authenticated fixture metadata and existing-only
  read-only checksum/CF/genesis inspection under the common lifetime lease.
  These interfaces are confined to repository-local ignored scratch paths.

One snapshot is reused for an excluded warm-up pair and five measured pairs,
alternating mode order. Repository fetch/cache/stage/activation is compared with
direct stage/validation/activation. Both receive an additional checked read-only
inspection. Exact DB file hashes match before opening; activation preserves the
DB inode. Direct creates no payload repository or second DB. Validation reports
match across modes and after the tested restarts.

The last pair performs two clean observer restarts per mode; direct additionally
exits via SIGKILL after readiness and recovers on another clean start. Every
runtime/optional network service and block production is disabled. Recovery
holds persist; no keys, peers, transactions or blocks are used. The recorded
range is **genesis height 0 only**, not a deterministic historical replay range.

The source remains outside the destination volume and unchanged. Fixture
preparation uses ordinary RocksDB checkpoint SST sharing in the independent
source tree; destination restore never requests hard-link/clone shortcuts.
Distinct target files and sampled filesystem usage do not establish exclusive
APFS physical extents, so that limitation is explicit.

The driver records exact argv, exit, stdout/stderr, elapsed time, wait4 child
peak RSS, driver peak RSS, file/directory st_blocks allocation with inode
deduplication, and filesystem total/available bytes. It samples after each tree
traversal plus a 20 ms sleep and at command boundaries. This is an observed peak,
not an upper bound on all transient allocations or physical write amplification.
Short-command timings include measurement/polling overhead; caches are not
purged, and concurrent host activity is not controlled.

## Actual results

Final standalone experiment:
build-single-copy-evidence/sc5-run-wf4ey1k5/ (SYNTHETIC_PASS).
Five measured samples per mode; warm-ups excluded. Summary distributions below
are restore/inspection phases, excluding observer-restart phases.

| Metric | Direct median (min–max; MAD) | Repository median (min–max; MAD) |
| --- | --- | --- |
| Sampled case allocation, bytes | 34,336,768 (constant; 0) | 68,562,944 (constant; 0) |
| Child peak RSS, bytes | 25,837,568 (25,526,272–26,329,088; 294,912) | 22,069,248 (20,987,904–22,151,168; 81,920) |
| Summed measured phase time, seconds | 0.35173 (0.27134–0.38383; 0.02014) | 0.22929 (0.17765–0.26834; 0.02922) |

Paired allocation savings are **34,226,176 bytes** in all five pairs, one
fixture payload's worth rather than a mainnet-size estimate. Direct was not
faster or lower-RSS in this short run; its additional validation work remains
part of the measured path. No performance or memory improvement is claimed.
Free-space observations reflect retained preceding pairs and other host work:
direct median minimum 45,619,109,888 bytes, repository 45,550,403,584 bytes.
These are not isolated-capacity results.

The independent source is 102,787,085 logical bytes, 68,710,400 allocated bytes
across 50 files (inode-deduplicated), outside the measured destination. The
Python driver peak is 29,294,592 bytes. Native/inspection child RSS is separate
from preparation, driver, OS, kernel cache and colocated services.

Final observer-phase maxima: direct 26,509,312 RSS bytes and 34,906,112 allocated
case bytes; repository 26,181,632 RSS bytes and 68,898,816 allocated case bytes.
These are brief genesis starts, not steady-state RSS, memory-growth or 24-hour
measurements. Checked reopen reports contain 9 CFs, 8,215 checked records,
7 genesis state objects, height/revision 0 and identical ID/root. WAL input is
0: no WAL rotation/compaction/catch-up qualification follows.

Input identity is recorded in the run's fixture.json/summary.json, signed
manifest/inventory, source tree, per-command logs and runs.json. For this run:

```text
backup_id: sc3-native-fixture
chain_id: 0x122067703e2929121059ca3f591aa8e50fb19d747b26e3b73ed66ff7cde629d28749
manifest_sha256: e998722df71dd609f4bd3476f403bc8d36e7b187814281e55bba3fb1251d0549
inventory_sha256: 3e0b40dca2e315d1461d5e97e3c46b980d6fe0fb80f70741238b2dcf5423c50b
```

### Capacity outcomes

| Profile | Observed result | Claim permitted |
| --- | --- | --- |
| Unconstrained Mac host, tiny fixture | Five paired restores and brief observer recovery pass | Synthetic workflow/disk comparison only |
| New 1 GiB APFS image | Effective total 1,073,700,864 bytes; initial available 1,070,313,472 bytes; minimum preflight passes but below recommendation; stage exits 1 at reserve check | Correct refusal, no target/attempt payload |
| Correct 40,000,000,000-byte APFS request | Blocked before image creation; host available 44,386,250,752 bytes at final attempt vs required 50,737,418,240 bytes | No 40 GB data-volume result |
| Linux/ext4 | No configured Linux instance or Docker daemon | Unverified |
| 2-vCPU / 4-GB / 40-GB Ubuntu-equivalent machine | OS/runtime/snapshot/workload not instantiated | Unverified, no whole-server claim |

The 1 GiB run is sc5-run-1mbvykj6, status CAPACITY_REJECTED, failure_phase staging,
exit 2 from the driver. Its required reserve was not lowered and its independent
source is preserved. Metadata/config scratch is retained; no attempt or target
payload exists. Its owned image was detached non-forcibly and its backing file
retained. The 40 GB blocked run is sc5-run-ryrjacb4. The conservative host guard
requires full sparse-image backing plus 10 GiB, even though a small fixture uses
much less. Do not bypass it or delete existing evidence to obtain a pass.

### Development failures and corrections

- The first run sc5-run-1fjctvd3 inspected readiness in stdout only while the
  native log was in stderr. The owned node was identified and stopped via SIGINT;
  the harness recorded BLOCKED. Readiness now checks both streams; subsequent
  clean/abrupt cycles pass. No external process was stopped.
- Initial image commands used a b suffix as if it meant bytes; hdiutil treats
  it as blocks. Runs sc5-run-8z8dukep and sc5-run-4x7akvhj therefore have oversized
  virtual filesystems and are **not bounded-capacity evidence** despite their
  synthetic workflow passes. Images were detached and retained. The tool now
  uses explicit sectors and rejects measured geometry above the declared limit;
  a unit regression covers this. Corrected profiles are the only capacity
  results used above.

## Build and test evidence

| Check | Actual outcome | E log |
| --- | --- | --- |
| Starting focused baseline | 13/13 passed | baseline-focused.log |
| Native/test rebuild and identity/help | Passed, repository-local dependencies | build-final.log, version.log, help.log |
| New resource unit/paired tests | 2/2 CTest, 7 unit checks, five measured pairs; 5.66 s | sc5-final.log |
| Final backup/direct/resource suites | 15/15 passed, 31.89 s | focused-final.log |
| Final standard full suite | 29/30, 44.71 s; only pre-existing SC0 SIGBUS | ctest-delivery.log |
| Existing fresh native tree rebuilt/final full suite | 29/30, 48.84 s; same SC0 failure | build-fresh-native.log, ctest-fresh-delivery.log |
| Matching UBSan engine/native/helper/resource tests | 4/4 passed, 19.88 s | ubsan-build.log, ubsan-tests.log |

The final source-hash artifact logging/unit bytecode setting rerun passed 2/2,
4.89 s (sc5-artifact-final.log, sc5-run-7907oc0a). Its source-before-sha256.json
and database-input-sha256.json expose the captured input hashes. Diagnostic
bytecode created in this task was moved into ignored E; pre-existing SC0
bytecode was not changed.

UBSan excludes vptr because linked RocksDB disables RTTI; third-party dependencies
are not reinstrumented. Existing SC3 fault coverage still reports 4,102 checks.
No ASan/TSan or new clean dependency bootstrap is claimed. Frontend docs-only
handoff is checked separately with mkdocs build --strict, recorded in E.
This report does not erase SC4's remaining full-suite/bootstrap/container gates.

## Exact missing prerequisites and reviewable next actions

1. Supply an explicitly selected native snapshot, concrete backup ID/height,
   expected network/genesis identity and trusted **public** verification material,
   plus deterministic successor blocks/reference outcomes. No private signer or
   producer key is needed. Current fixtures cannot replace this requirement.
2. Supply enough authorized Mac scratch capacity for full backing plus reserve,
   source storage and measured writes. At the final 40 GB attempt, the guard is
   short by 6,351,167,488 bytes before additional fixture work. Existing evidence
   is retained; no cleanup is authorized.
3. Select/provision a disposable Linux/ext4 environment and matching Linux
   native build. Docker info reports its daemon socket unavailable; limactl list
   reports no instance. No VM provisioning, Linux build or VPS operation was
   performed. A Mac binary does not establish Linux behavior.
4. Run the chosen full-size snapshot at least twice when resources permit,
   then deterministic replay/root/receipt comparison and at least 24 hours of
   representative observer workload, two clean restarts and injected interruption.
   Record actual WAL/compaction events, growth, minimum free space and peak/steady
   RSS. None of those representative checks were executed here.
5. Reassess and address the separate authorized audit gates before operational
   qualification. Current source still has unchecked state-CF Flush results and
   non-NotFound reads mapping to absence (TQ-01), and separate record/topology
   writes (TQ-02); TQ-10/WP6 observability/publication work remains unresolved.
   This task inspected those paths but did not implement or rerun their fault
   reproductions. The existing SC0 truncated-transfer SIGBUS also remains.

The local tool's exact repeatable commands are in E/reproduction.md and
[its contract](../../tools/backup/README.md#sc5-local-capacity-experiment).
For any later real-input qualification, first review paths/source/network,
OS/runtime footprint, Linux binary identity and no-production config, then
execute SC4 preflight/stage/activation only in a new isolated target. Do not
pass real input into this synthetic-only helper or copy a recipe onto a live VPS.
Live server operation and release/publication remain separate authorizations.

## SC5 progress and exit status

Changed in Teleno:

- tools/backup/validate_single_copy_restore.py (new)
- tests/scripts/validate_single_copy_restore_test.py (new)
- tests/backup/direct_activation_test.cpp (diagnostic helper extensions only)
- src/CMakeLists.txt (two diagnostic CTest registrations)
- tools/backup/README.md
- docs/validation/single-copy-restore-sc5-capacity.md (new)
- docs/implementation/single-copy-backup-restore-plan.md
- docs/backup-restore-cli.md
- docs/README.md
- docs/memoria.md
- CHANGELOG.md

Koinos One docs-only handoff extends its pre-existing changes in
docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md and
docs/manual/koinos-one/backup-and-restore.md. No GUI/packaging/version/submodule
or app changelog change occurred. Native identity/HEAD above includes
uncommitted work, not a new revision to pin or published feature.

Implemented: bounded local measurement driver, same-input comparison,
independent source preservation, exact-byte/inode/checked-genesis parity,
observer-only restart/interruption, measured-capacity verification and safe
reserve rejection. Tested outcomes and failed iterations are preserved.

**SC5 exit criteria remain unsatisfied.** Local synthetic disk savings are
measured, but representative Mac/40 GB, Linux and whole-server profiles are
blocked/unverified. No historical/differential consensus, sustained operation,
power-loss or complete repository-scoped Definition of Done is proven.
SC6 and external release actions were not started.
