# LR2 native admission and pending-count checkpoint

Date: 2026-10-03. Status: **local correctness checkpoint verified; M1 and LR2 remain open**.

This is the historical M1-A/B checkpoint. Subsequent
[M1-C](low-resource-lr2-admission-atomicity.md) reproduces and corrects partial
multi-fork insertion after C++ allocation failure, with current test/build
evidence. The all-fork allocator gap below describes the earlier checkpoint;
aggregate capacity and whole-node qualification still remain open.

## Scope and primary target

The primary optimization and qualification target is the Linux VPS:
Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB physical RAM. Mac/APFS results below
are supplementary development diagnostics, not Linux whole-node qualification.
No live VPS, producer, existing VM, backup publication or deployment was operated.

Source is the dirty Teleno checkout at `/Users/pgarcgo/code/teleno`, branch
`codex/reward-reinvestment-macos`, HEAD
`cfd2eb83b8080ca49911555108ce05f0a9068f01`, VERSION `1.3.0-dev.0`.
Generated data is under `/Volumes/My Passport/teleno-lr2-EEMOFW/`; its
`evidence/` directory is also reachable through ignored repository alias
`build-single-copy-evidence/lr2-completion-qKjj7h/`.
The primary build uses Release, Apple Clang 16, ARM64, tests and cpp-libp2p ON,
this exact source repository and previously installed dependency inputs.
It is not a fresh all-dependency or Linux build.

## M1-A: refusal before accepted-work notification

The original native wiring dispatched accepted-work observers through EventBus.
Main's mempool subscriber caught and logged insertion exceptions. A failed
insertion could therefore return from the bridge successfully and publish an
accepted-work event while retaining no transaction.

A regression copied that wiring with the real adapter/library: pending payer
resources 400, maximum 1,000; a second transaction requests 700. The pool refuses
the insertion, but the assertion requiring refusal propagation fails.
`m1-admission-reproduction.log` records CTest exit 8, assertion `refused`,
0.65 seconds. This is a reproduced library/bridge integration bug, not a
public-network experiment or full signed controller submission.

The native bridge now has an optional admission callback. Main enables it only
when the mempool service is enabled. It parses the accepted-transaction message,
requires transaction and receipt fields, inserts through the adapter first,
and only then notifies accepted-work observers. Ordinary admission exceptions
propagate before notification. No RPC/protobuf schema or protocol limit changes.

After successful insertion, an observer exception must not turn retained work
into a failed submission or cause a second insertion. The bridge contains those
notification exceptions, logs a warning and increments
`transaction_notification_failures`. Boost signal dispatch is unchanged:
a throwing observer may prevent later observers from running. There is no
notification retry or promise of delivery to every observer.

Disabled mempool retains the original notification-only mode. Admission still
runs with a configured callback and no EventBus. Nontransaction event exception
behavior is unchanged. The callback relies on the library's admission semantics;
catastrophic allocation failure across multiple fork commits is **not** proven
atomic by this checkpoint.

Focused tests cover:
- Resource refusal: no notification, candidate absent, reservation unchanged.
- Removal followed by successful retry: no rejected nonce/index residue.
- Duplicate refusal: no second notification, count or resource debit.
- Notification sees the retained record; throwing observer is counted, retained
  work remains successful, subsequent notification can succeed.
- Malformed admission message and disabled/no-observer modes.
- Nontransaction notification exception propagation.
- Concurrent admission: two 400-resource candidates with 400 already reserved
  and maximum 1,000; exactly one succeeds, one is refused, one notification is
  emitted and 800 remains reserved.

These tests use synthetic library inputs and bypass signatures/chain execution.
They do not prove all-fork failure atomicity, reorg compatibility, global byte
capacity, peer refusal classification or pre-execution memory reservation.

## M1-B: accurate, nonmaterializing pending diagnostics

The previous metrics expression was `get_pending_transactions(0).size()`.
The actual implementation treats zero as a zero-record request, so it always
reported zero even with retained transactions. The regression explicitly proves
that behavior with 2,001 retained independent-account records.

The new `pending_transaction_count()` obtains the existing database/head-node
locks and scans visible pending keys without parsing/copying transaction payloads.
It reports the actual visible-head count; it is O(n), not a constant-time counter.
The metric now uses this method. It is neither physical retained-fork/allocator
accounting nor an admission budget. Compared with the previous incorrect zero
counter, it performs additional work. No payload-copy reduction versus that old
metrics expression is claimed.

The characterization verifies count 0, 2,001 and 0 around insertion and expiry.
A 2,000 response limit still does not bound retention. Logical cleanup does not
establish physical fork/delta reclamation.

### Scoped comparison of two correct count paths

Five alternating fresh processes per method; 128 synthetic records, each with
128 KiB arbitrary signature bytes, one same-mode warm-up and 100 timed counts.
Materialized comparison requests up to 2,000 records, enough for this fixture;
the new path scans without payload materialization. Inputs are not protocol-valid
signed workloads. Process peak RSS includes fixture construction and runtime
overhead, not just the timed count. Dependency compilation was concurrent;
driver overhead was not separately profiled.

| Correct count method | Median time, 100 counts | Min–max time | Time MAD | Median process peak RSS | RSS min–max |
| --- | --- | --- | --- | --- | --- |
| Materialized response | 8,706,422 us | 8,660,291–8,733,034 us | 8,676 us | 52,953,088 bytes | 52,740,096–53,329,920 bytes |
| Visible-key count | 45,134 us | 43,354–48,648 us | 634 us | 33,488,896 bytes | 33,456,128–33,538,048 bytes |

Evidence: `m1-count-materialized-{1..5}.log`,
`m1-count-new-{1..5}.log`, and `m1-measurement-summary.json`.
This compares two correct alternatives, **not** historical before/after native
metrics, Linux performance, whole-node RSS or mainnet capacity.

An initial diagnostic mistakenly treated limit zero as “all records” and failed
its 128-record assertion (exit 134, `m1-count-legacy-1.log`). It is retained as
failed evidence; the corrected comparison is separately identified.

## Current-source build and test evidence

| Check | Actual result | Artifact |
| --- | --- | --- |
| Build all cached-dependency native targets, parallel 4 | Exit 0 | `m1-current-build.log` |
| Full CTest | 38/38, exit 0, 206.74 seconds | `m1-current-ctest.log` |
| UBSan cache/P2P/mempool | 3/3, exit 0, 20.94 seconds | `m1-current-ubsan-build.log`, `m1-current-ubsan-tests.log` |
| Native version | Exit 0, `1.3.0-dev.0+cfd2eb83b808-dirty` | `m1-current-version.log` |
| Native help | Exit 0 | `m1-current-help.log` |
| Correct-count comparison | Five samples per method, all exit 0 | Logs above |

Native SHA-256:
`198b251507a0532f8407539189b0e3b45416c8bd45b3b0f3afdc172418f9ef3c`.
Current mempool test SHA-256:
`3b003c19c852c876f842c567482c4b88d93a95762eb194e1aa9a08569c04ead6`.

Reproduction commands, from this repository:

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/build'
export PYTHONDONTWRITEBYTECODE=1
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 4
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
"$TELENO_TEST_BUILD_ROOT/koinos_mempool_adapter_test" --measure-count-materialized
"$TELENO_TEST_BUILD_ROOT/koinos_mempool_adapter_test" --measure-count
```

UBSan uses the separate external `ubsan/` tree,
`-fsanitize=undefined -fno-sanitize=vptr -fno-omit-frame-pointer`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` and CTest regex
`mempool_adapter|p2p_one_peer_sync|cache_invariants`. Vptr checks are excluded;
cached dependencies are not fully instrumented. This does not establish
ASan/TSan qualification.

### Fresh dependency workflow

The required build script has not yet passed its fresh dependency gate.
Attempts are retained in `dependency-workflow-manifest.json`:
1. External Make build fails because recursive Make splits the Xcode path.
2. Ninja build passes libssh but Hunter rejects a root containing spaces.
3. A space-free ignored alias points to owned external storage; Boost builds,
   but OpenSSL recursive Make still splits the Xcode path. Exit 1.
4. A bounded probe proves command-local `MAKE=/usr/bin/make` resolves recursive
   invocation without changing Xcode/system tools. The script resumes with
   Ninja, that environment and all dependency/build data external. At this
   checkpoint it is **running**, not counted as successful.

Exact environment/argv and failures are in the manifest. Current log is
`external-dependency-make-workflow.log`; probe is `recursive-make-probe.log`.
No failure or in-progress build is relabeled as a pass.

## Changed files and handoff

Runtime:
- `src/core/monolith_rpc_client.hpp`
- `src/main.cpp`
- `src/koinos/mempool/mempool.cpp`
- `src/koinos/mempool/mempool.hpp`
- `tests/mempool/mempool_adapter_test.cpp`

Documentation adds this report and updates the completion matrix, active plan,
project memory, identical AGENTS/CLAUDE, changelog, operator RPC/diagnostics,
docs index and release gates. The earlier worker report is historical evidence;
its native hash is not the current admission build identity.
README and the high-throughput implementation plan retain their initial hashes.
No file is reset/deleted or staged for commit.

Koinos One receives documentation-only additions in its current implementation
reference and manual backup/restore page. No app code, binary, package, GUI
setting or submodule pin changes. The exact current native identity above is a
dirty development build, not a new commit or shipped desktop capability.

Koinos One strict MkDocs build passes (exit 0, 0.66 seconds), with output in
external `koinos-one-m1-manual/` and log `m1-koinos-one-mkdocs.log`. Its worktree
contains only the same two documentation files; no integration is performed.

## Remaining work, rollback and exit

M1 remains open: aggregate transaction/index/nonce/account/fork physical
accounting, defensible capacity and transient reservations, pre-execution local
admission, all-fork failure/reorg/readmission tests and compatible local overload
classification. Notification ordering alone does not bound memory.

Other LR2 gates remain: RPC output and VM active/parsed working sets, state
borrowed-pointer ownership, real transport/peer retention, functioning integrated
ASan/TSan, representative Linux whole-node RSS/host/catch-up/recovery and current
historical audit/differential runs. Missing prerequisites are a functioning
authorized isolated Linux environment and authenticated pinned full-history
checkpoint/successor/reference corpus covering before 30,504,202 through beyond
32,789,378. No historical run or unexplained-mismatch acceptance occurred.
LR5's separate server soak remains pending.

Rollback reviews only this checkpoint's hunks in a stopped owned test checkout,
preserving existing work and evidence; no storage migration/reset is introduced.
Removing the admission hook restores the reproduced swallowed-refusal bug.

**Every applicable LR2 Definition of Done item proven: NO. Goal remains active.**

## Cumulative goal changed-file inventory

The 46 files below are this goal's P1/external-data and M1 changes relative to
its captured dirty baseline, not the entire worktree diff against HEAD. Prior
user changes outside these files remain intact. Machine-readable preservation
proof is `m1-source-preservation.json`: 74 initial files unchanged, 37 extended,
three previously clean tracked files modified, six new untracked files,
no initial file removed and no unexpected initial-file change. Protected README
and high-throughput-plan hashes remain unchanged. There is no staged commit.
The two Koinos One documentation files are listed separately above.

- `docs/implementation/low-resource-mainnet-observer-plan.md`
- `docs/memoria.md`
- `docs/validation/low-resource-lr2-completion-matrix.md`
- `docs/validation/low-resource-lr2-memory.md`
- `docs/validation/low-resource-lr2-p2p-retention.md`
- `tests/artifact_root.hpp`
- `tests/backup/direct_activation_test.cpp`
- `tests/backup/direct_restore_harness_test.py`
- `tests/backup/direct_restore_test.cpp`
- `tests/backup/durable_publication_test.cpp`
- `tests/backup/restore_plan_test.cpp`
- `tests/cli/direct_restore_preflight_test.py`
- `tests/cli/direct_restore_workflow_test.py`
- `tests/scripts/fixture_paths_test.py`
- `tests/scripts/lr2_memory_test.py`
- `tests/storage/persistence_failure_test.cpp`
- `tools/backup/direct_restore_fixture.cpp`
- `tools/backup/record_sc0_baseline.py`
- `tools/backup/restore_baseline_fixture.cpp`
- `tools/backup/validate_single_copy_restore.py`
- `tools/diagnostics/fixture_paths.py`
- `tools/diagnostics/measure_lr2_memory.py`
- `AGENTS.md`
- `CHANGELOG.md`
- `CLAUDE.md`
- `docs/README.md`
- `docs/configuration.md`
- `docs/implementation/teleno-audit-remediation-plan.md`
- `docs/logs-and-diagnostics.md`
- `docs/performance/teleno-software-audit.md`
- `docs/release-builds.md`
- `docs/running-observer-node.md`
- `src/CMakeLists.txt`
- `src/p2p/p2p_node.cpp`
- `src/p2p/p2p_node.hpp`
- `src/p2p/types.hpp`
- `tests/backup/backup_sftp_test.cpp`
- `tests/p2p/p2p_one_peer_sync_test.cpp`
- `docs/validation/low-resource-lr2-workers.md`
- `tests/mempool/mempool_adapter_test.cpp`
- `src/core/monolith_rpc_client.hpp`
- `src/koinos/mempool/mempool.cpp`
- `src/koinos/mempool/mempool.hpp`
- `src/main.cpp`
- `docs/rpc-endpoints.md`
- `docs/validation/low-resource-lr2-admission.md`

Markdown local-file link check: 129 references resolve; anchor contents and
remote link availability are not included in that check. Native/Koinos One
`git diff --check` pass and AGENTS/CLAUDE compare byte-identical.
