# Single-copy restore: SC2 bounded staging and resume

Date: 2026-10-01. Scope: SC2 of the
[single-copy restore plan](../implementation/single-copy-backup-restore-plan.md).

**Local SC2 staging gates pass. The feature is not ready for deployment.**
Direct stage/resume/activation remain unavailable from `teleno_node`; the new
library is exercised by isolated diagnostic tools. No target DB is opened or
activated. Launch and sustained operation on the intended 2-vCPU, 4-GB-RAM,
40-GB Linux server remain unproven, including its OS/runtime footprint.

## Environment and authorization

- Repository: `/Users/pgarcgo/code/teleno`, branch
  `codex/reward-reinvestment-macos`, commit
  `cfd2eb83b8080ca49911555108ce05f0a9068f01` plus captured pre-existing/SC2 work.
- Native identity: `teleno_node 1.3.0-dev.0+cfd2eb83b808-dirty`.
- macOS 26.5 (25F71), Mac16,10, 10 logical CPUs, 17,179,869,184 bytes RAM,
  internal APFS, not a capacity-limited volume.
- AppleClang 16.0.0.16000026, C++20, arm64, Release (`-O3`, `-DNDEBUG`);
  CMake 4.2.3, Ninja 1.13.2, Python 3.9.10 under Rosetta. Mach-O binaries and
  native compiler records, not Python's architecture, establish arm64.
- Verified repository-local `build-sc0-baseline`, tests/cpp-libp2p enabled.
  Dependencies unchanged, including RocksDB 8.8.1 and Boost 1.83.0. No foreign
  native cache or workspace was reused. UBSan uses the separate repository-local
  arm64 Release `build-sc1-sanitize`, `-fsanitize=undefined` and frame pointers.
- The user requested documenting the server launch goal and implementing the
  next milestone. No server access, deployment, producer activation, data
  cleanup, commit/push/tag or release was performed. Server identifiers are
  intentionally absent from public documentation. Screenshot resources are
  not measurements of actual usable disk, architecture or runtime readiness.

Ignored evidence root, abbreviated E:

```text
build-single-copy-evidence/sc2-20261001-xSpQIO/
```

`environment-before.json`, `environment-after.json`, `worktree-before.patch`,
`preservation-check.json` and `untracked-preservation-check.json` record the
baseline and preservation. Twenty non-overlapping pre-existing files are
byte-for-byte unchanged, including README.md, the high-throughput plan, main,
core/config/preflight and SC0/SC1 reports. Modified pre-existing untracked files
retain their original bytes apart from the documented additions/budget change.
Shared tracked edits were integrated without discarding existing work;
legacy transport lines are unchanged. AGENTS.md/CLAUDE.md remain identical.

## Implementation and invariants

### Private, non-runnable attempt

`stage_direct_restore_snapshot()` accepts a validated immutable SC1 specification
and explicit source/target/attempt. A new attempt is exclusively created with
owner-only permissions as a disjoint sibling of a fresh target on one filesystem.
Existing/unowned attempts are rejected unless explicit resume succeeds. Source,
target and attempt cannot overlap; symlink/hard-link/special/unknown entries and
cross-filesystem attempt entries fail closed. There is no recursive-copy fallback.

The bounded versioned `ATTEMPT.json` pins selected ID, manifest/inventory SHA-256,
source, network/chain, format/layout, node version, trust status, target/attempt
paths and a random generation ID. The generation also binds `state.json` and
per-file progress records. An attempt flock serializes this engine's writers,
but is **not** the common startup/restore exclusion gate required by SC3.

Files are materialized beneath `payload/`; the canonical target remains absent
or unchanged. The only transfer temporary is `partial/current.part`. No complete
object cache, archive, clone or hard-link sharing is created. Exact inventory
paths are preserved, including required runtime inputs and duplicate content at
distinct destinations. No `RESTORE_STAGE_COMPLETE`, activation or observer-start
marker is emitted; `download_complete` is only byte-integrity completion.

### Bounded transfer and visible failure

The opt-in object transport reuses existing URL/object layout and certificate
configuration helpers without changing legacy fetch defaults. It delivers at
most 64 KiB at a time through an incremental size/hash/checked-write sink. HTTP
headers are capped at 16 KiB and the parser wire buffer at 128 KiB. Content length,
chunked-body limit, final byte count and SHA-256 must agree with the inventory.

One object transfer runs at a time, at most `max(1, retries)` attempts with a
configured cap of ten; retries discard only the owned partial. Async DNS,
connect, TLS and HTTP operations share an object-attempt deadline (1–300 seconds),
which is not reset by reads or redirects. Network cancellation is polled at
50 ms intervals. HTTPS requires a loaded trust store, peer and hostname checks;
HTTPS redirect policy and the five-redirect limit remain enforced. Tests used
file and loopback HTTP, not external HTTPS or a real DNS outage.

Cancellation/write/size/hash/network/sync/rename failures cannot publish completion
or a canonical DB. Recoverable failures persist `paused`; if recording failure
itself fails, the error propagates instead of returning success. Initial orphan
directories without a committed descriptor are retained and refused, not adopted.
Errors are reported through the result/exception; the state keeps a bounded
incomplete-transfer reason without copying arbitrary exception details into it.

Files and journal temporaries use exclusive/no-follow opens, checked writes,
file sync/close, rename and directory sync. This is an implemented POSIX ordering
contract, not measured physical power-loss durability. Blocked kernel filesystem
I/O and resolver teardown under a real DNS failure remain timing qualifications;
metadata acquisition still uses SC1's bounded but synchronous transport.

### Pinned, phase-aware resume

The diagnostic caller reads the bounded identity hint, selects the concrete
backup ID, re-acquires/authenticates metadata and passes the validated specification
back to the engine. The engine compares the complete pinned descriptor. It never
selects `latest` during resume. Changed source/identity/hash/policy, foreign state
generation and unsupported states are rejected before payload mutation.

Only `planned`, `downloading`, `paused` and `download_complete` accept source
replay. `validating`, `verified`, `activation_pending` and `activated` are refused
so future recovery cannot overwrite a DB changed by offline validation. A crash
after descriptor publication but before the first state is recoverable only
when payload/partial/progress are empty.

Every existing destination is re-hashed before reuse and budget subtraction;
progress claims alone are never trusted. Rename-before-progress gaps recover
from verified files. Explicit resume removes only its own incomplete partial and
invalid inventory files, then retransfers missing/changed files. It does not
touch the independent source or user/canonical databases. All files and target
eligibility are rechecked before recording `download_complete`.

### Accounting

SC1's estimator now budgets allocated per-file journal records, not just 512-byte
logical records, with overflow checks. The existing `max(10 GiB, DB/5)` reserve
and 128 MiB validation allowance remain; the latter is still unqualified.
Staging requires the recommended budget, not merely SC1's minimum success code.

Resume subtracts only re-hashed complete payload allocations, removes owned
invalid bytes before computing incremental demand, and rechecks available space
at file boundaries and at most every 8 MiB during writes. The actual kernel write
path still reports ENOSPC if other activity defeats the observations. A declared
budget is not a filesystem reservation or a full-node growth guarantee.

## Actual build and test results

Commands ran from the repository with parallelism four. E contains the logs.

| Check | Actual result | Artifact |
| --- | --- | --- |
| Pre-edit focused backup/SC1 CTest | 9/9 passed, 2.95 s, exit 0 | `baseline-focused.log` |
| `cmake --build build-sc0-baseline --parallel 4` after final source | Exit 0 | `build-final-source.log` |
| `ctest --test-dir build-sc0-baseline --output-on-failure -R '^(koinos_backup_|teleno_direct_restore)'` | 11/11 passed, 17.28 s, exit 0 | `ctest-focused-final-source.log` |
| `ctest --test-dir build-sc0-baseline --output-on-failure` | 25/26 passed, 29.31 s, exit 8; only existing SC0 diagnostic failed | `ctest-all-final-source.log` |
| Native SC2 unit within full CTest | 1,106 always-on checks passed, including object-generation digest checks; approximately 12.5 MB peak process RSS | CTest details; unit root below |
| SC1 budget regression | 69 checks passed; added allocated-journal regression | CTest details |
| Separate UBSan build of SC2 unit and fixture | Exit 0 | `ubsan-build-final-source.log` |
| `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./build-sc1-sanitize/koinos_backup_direct_restore_test` | 1,106 checks passed, exit 0, no UBSan report | `ubsan-unit-final-source.log` |
| Signed harness with UBSan fixture and native legacy fixture | Exit 0, no UBSan report | `ubsan-harness-final-source.log` |
| Native `--version` / `--help` | Exit 0; identity above, operational CLI remains metadata-only | `version-final.log`, `help-final.log` |
| `docker info --format '{{.OSType}}'` | Exit 1, local daemon socket missing; container smoke not run | `docker.log` |
| Diff, preservation, Python syntax and documentation links | Passed | preservation/check manifests |

Reproduction for the signed harness:

```bash
python3 tests/backup/direct_restore_harness_test.py \
  --fixture build-sc0-baseline/koinos_direct_restore_fixture \
  --legacy build-sc0-baseline/koinos_restore_baseline_fixture --repository .
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  python3 tests/backup/direct_restore_harness_test.py \
  --fixture build-sc1-sanitize/koinos_direct_restore_fixture \
  --legacy build-sc0-baseline/koinos_restore_baseline_fixture --repository .
```

UBSan instruments the relevant repository code, not all dependencies or the
whole node. ASan was not rerun; SC1's recorded runtime-initialization abort
provides no ASan coverage. ThreadSanitizer/leak checking were not run. The
canonical dependency script was not rerun for unchanged dependencies; SC0's
Xcode-path/Make blocker remains documented, not claimed as passed.

The first harness draft incorrectly parsed the legacy fixture's log stdout as
JSON. The runner was corrected to parse only the new fixture's JSON output;
`harness-first.log` retains that harness failure. It was not an application bug.

## Fixture, failure and footprint evidence

Final native harness root:

```text
build-single-copy-evidence/sc2-harness-tbe6znau/
```

`summary.json` records 27 commands, request paths, unchanged source hashes and
no node startup. Each command records argv, elapsed time, outputs and actual
exit. The nine-file signed SC0 fixture has backup ID
`20261001T083337Z-ms-1790843617682-files-7`, manifest SHA-256
`dda28f8080dcb8e78d5e0396968fd24acb94bd064394cdea6879b23112c3b1d5`
and inventory SHA-256
`3934bb0c23c80171d383530af2b69e7e6775045e7f4a856bc6062d530956fd80`.
It is valid synthetic RocksDB data, **not a valid Koinos chain**.

Coverage includes valid signed file/HTTP inputs, exact legacy-stage byte equality,
rehash-only resume with zero object requests, repair fetching exactly one changed
file, process exits after descriptor/mid-file/rename, and resume with a changed
`latest` endpoint that must never be requested. Negative transfers cover
truncation, declared/chunked excess bytes, checksum changes, stalls, slow trickle,
outage, redirect loops, active/idle cancellation and insufficient space.

Ordinary failed object cases issued exactly two requests (the configured cap).
Redirect loops issued twelve requests: six per attempt. Stalled and slow-trickle
cases rejected in approximately 2.31 s; idle cancellation rejected in 0.33 s.
Targets and activation markers were absent on every success/failure. Partial
counts stayed at zero or one. No SIGBUS occurred on the new transport path.

Native unit coverage also includes zero-length files, duplicate content at
different paths/inodes, wrong descriptor/hash/generation/state, lock contention,
unowned/populated/symlinked attempts, write/sync/rename/journal fault injection,
reserve depletion and resume without double-counting complete data. Fault hooks
simulate operation failures; an actually full filesystem and injected hardware
I/O errors were not used.

One warm-up plus five direct HTTP samples on one source/host/build:

| Observation | Actual result |
| --- | --- |
| Payload logical bytes | 6,479,964, identical in all samples |
| Direct attempt allocated bytes, including journal | 6,553,600 in all five samples |
| Legacy repository plus target allocation after stage, one comparison | 6,524,928 + 6,516,736 = 13,041,664 |
| Direct native peak RSS median, min–max | 14,401,536; 14,286,848–14,434,304 bytes |
| Direct subprocess elapsed median, min–max | 0.22412; 0.21978–0.22930 seconds |

There is only one paired legacy measurement here; do not infer a speedup or a
production percentage saving. Its stage hashes were checked before DB reopen.
Direct allocation is observed after download, not a sampled absolute transient
peak. `st_blocks * 512` does not prove exclusive APFS physical extents. Sources,
checkpoint preparation, other attempts, Python driver, builds and OS remain on
the host outside the per-attempt observations and count toward whole-host use.

Large-object unit root: `build-single-copy-evidence/sc2-unit-2534096837/`.
The test streams one 64 MiB object into two independent files plus tiny/empty
inputs: 134,217,755 payload bytes, 134,254,592 allocated attempt bytes and
12,500,992 bytes peak native process RSS. No payload-sized RAM representation
or extra complete cache is present. This supports bounded streaming for this
fixture, not a complete-node 4 GB memory bound or many-file worst-case proof.

## SC2 progress log and exit status

SC2 changes:

- `src/backup/direct_restore.hpp`, `src/backup/direct_restore.cpp` (new engine).
- `src/backup/public_restore.hpp`, `src/backup/public_restore.cpp` (opt-in stream).
- `src/backup/restore_plan.cpp` (allocated journal budget), `src/CMakeLists.txt`.
- `tests/backup/direct_restore_test.cpp`,
  `tests/backup/direct_restore_harness_test.py` (new),
  `tests/backup/restore_plan_test.cpp` (one budget regression).
- `tools/backup/direct_restore_fixture.cpp` (new), `tools/backup/README.md`.
- `docs/backup-restore-cli.md`, `docs/command-reference.md`, `CHANGELOG.md`.
- `docs/implementation/single-copy-backup-restore-plan.md`, `docs/memoria.md`.
- `docs/validation/single-copy-restore-sc2-staging.md` (this report, new).
- A user-requested local memory extension note outside the repository records
  the same server goal; no host identifier or credential was published.

Invariants proven locally: exact source/legacy-stage bytes, no full cache,
single bounded partial, independent duplicate-content files, pinned and rehashed
resume, finite tested network failures, visible injected storage failures,
unchanged source and canonical target, no DB opening/activation/producer startup,
and passing legacy focused tests. Existing legacy transport/default routes were
not replaced. No consensus ordering, roots, WAL or pruning policy changed.

**SC2 exit:** satisfied for the isolated Mac/file/HTTP staging contract, with
shared and operational blockers recorded. **Whole-feature Definition of Done:**
not satisfied. **Server launch/40 GB gate:** not satisfied.

Final broader failure: `teleno_restore_baseline_harness_test`, truncated object,
SIGBUS/exit -10, `build-single-copy-evidence/sc0-run-v3s050rx/`. The existing
diagnostic Boost/error-formatting discrepancy remains unexplained; passing the
new error path does not repair it or turn the full suite green. Container smoke,
ASan, Linux/ext4/cross-device fault experiments, real ENOSPC and physical
power-loss qualification are missing. Existing storage/publication audit findings
remain unremediated; metadata/DNS/filesystem worst-case timing is not fully proven.

SC3 must implement coherent offline DB/genesis validation, durable observer holds
and activation with a common startup/restore gate before SC4 exposes operational
CLI commands. SC5 must qualify actual server-equivalent OS/disk/RAM, real chain
snapshot and successor blocks, reopen, validated advancing head, compaction/WAL
peaks and sustained operation. No full historical range or reference differential
validation was executed here; existing replay regression fixtures passing is not
historical parity. No server was contacted and no 40 GB promise is made.
