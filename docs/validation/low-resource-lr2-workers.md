# LR2 sync-worker ownership and external development data

Date: 2026-10-03. Status: **local functional checkpoint verified; LR2 NOT complete**.

This worker report records its original checkpoint/build identity. A later
[admission checkpoint](low-resource-lr2-admission.md) reproduces/fixes the native
refusal boundary and pending diagnostics, with newer current-source evidence.
The integration risk described below was subsequently reproduced, not left as
a hypothesis. Third fresh dependency attempt failed at OpenSSL recursive Make;
the fourth resumes with command-local MAKE and Ninja, not yet a passed gate.
Linux VPS qualification is primary; all Mac measurements here are supplementary.

## Scope and provenance

The explicit LR2 persistent goal authorizes remaining repository-local LR2
implementation, bounded diagnostics and documentation. Its separate follow-up
selects My Passport for development data. Source remains in Teleno; no live node,
producer, archival history, state objects, source trust, reserve, durability,
LR3/LR4, GUI/package/pin, commit/push or release changed. The
[completion matrix](low-resource-lr2-completion-matrix.md) retains every open gate.

Branch `codex/reward-reinvestment-macos`; HEAD
`cfd2eb83b8080ca49911555108ce05f0a9068f01` plus preserved dirty SC/LR work.
Native identity `1.3.0-dev.0+cfd2eb83b808-dirty`. Local arm64/macOS 26.5,
Apple Clang 16, 10 CPUs/16 GiB, cached Boost 1.83/RocksDB 8.8.1 and cpp-libp2p
`c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`. This is not the selected Linux host.

New cleanly configured native and UBSan trees are under the owned external
`/Volumes/My Passport/teleno-lr2-EEMOFW/` run. Their CMake source is this exact
repository, not another workspace. Cached dependencies remain inputs; they were
not rebuilt/instrumented from empty. My Passport was verified mounted/writable,
APFS/USB with about 381 GiB available; source/internal dependencies were not moved.
No existing chain/state data was relocated or deleted.

Development-data placement is recorded in identical `AGENTS.md`/`CLAUDE.md`
and `docs/memoria.md`. Source preservation captures 111 starting dirty/untracked
files. At the P1 checkpoint, 77 remain byte-identical, 34 are deliberately
extended and five files are new. The subsequent additive M1 characterization
test extends one more existing file (76 unchanged / 35 extended / five new).
README and the high-throughput implementation plan remain byte-identical.
`final-source-preservation.json` names each changed file and its hash; tracked
diff/whitespace checks and 117 local documentation links pass. New-file
no-index whitespace checks emit no diagnostics (exit 1 indicates the added
file diff, not a test failure). No files are staged, committed or pushed.

Evidence is its `evidence/` subdirectory. The ignored repository alias
`build-single-copy-evidence/lr2-completion-qKjj7h/` points there. Initial hashes,
source preservation, configure argv and logs identify actual correspondence;
these artifacts are not included in a normal clone. Native SHA-256:
`26ff10df621a6bd4ae3a0147222d09a5b288f02a6a034f9121ab10773cbb9b30`.
Measurement/test SHA-256:
`76d683e435a4ea7b0146896900b8d088bbe4ec4a5dca3b0c6f16c12585ecef8a`.

## Reproduction and implemented invariants

Before the correction, a new actual P2PNode/fake-transport test held a handshake
RPC, emitted disconnect, and called stop asynchronously. Stop returned before
release: CTest exit 8 with the assertion requiring a blocked stop failing
(`p1-reproduction.log`, 0.51 seconds). This confirms missing stop ownership;
it does not itself prove a production use-after-free or a detected data race.

- No disconnected peer-sync thread is detached. Current peer generations and
  all owned workers have separate registries; retired work survives disconnect
  until finished and joined. Completed workers are reaped on admission, stats
  and the existing acquisition cycle; stop drains every generation.
- Internal defaults bound owned generations and connected peer states to 64.
  Retired and draining work counts too. Reconnect may overlap a cancelled old
  generation only within that capacity. A full/zero capacity closes the excess
  connection without recording a peer error. No protocol validity or archival
  retention limit is changed. These are internal P2POptions, not YAML/CLI/GUI.
- Per-generation cancellation is checked after remote responses, after acquiring
  sync serialization, before each block and on error paths. A released response
  for a cancelled generation cannot start the next operation in the tested
  interleaving. Already-started application may finish; cancellation is not a
  rollback and no instantaneous linearization guarantee is claimed.
- Start/stop serialize, duplicate start/stop is harmless, and failed startup
  drains registered work/subscriptions before permitting retry. Workers publish
  completion after their last node access; thread-launch registration failure
  unwinds ownership. Allocation/thread-launch failure was reviewed, not injected.
- Stop joins outstanding calls **before** transport teardown to avoid racing
  borrowed runtime/host fields. Blocking transport/service calls can delay stop;
  this is not a universal shutdown deadline or forced RPC cancellation. Real
  transport callback/drain/cancellation qualification remains open under P2.
- Peer snapshots/read-only worker_stats expose connected/owned/retired counts,
  limit and cumulative capacity refusals. Thread stacks, strings, transport/
  decoded buffers and allocator overhead are not counted bytes or an RSS cap.

Existing normal block sync, checkpoint/LIB, mismatch rejection, fork-bomb,
accepted-ID cache and pending-duplicate behavior remain covered. State backend,
Merkle computation, VM execution and receipt/replay semantics were not changed.

## Tests and measurement limitations

Added gated handshake/returned-batch cancellation, disconnected stop ownership,
reconnect overlap, duplicate connections/start/stop, capacity refusal/recovery,
zero capacity, startup-failure retry and 200 repeated connect/disconnect cycles.
Counters used across fake transport threads are now atomic; fixture gating uses
condition variables. The controlled stop waits 250 ms before releasing its own
RPC, then requires completion within two seconds. This proves that interleaving,
not cancellation of arbitrarily blocked real I/O. Every tested drained instance
has zero owned workers. No external peer or live node is involved.

Five fresh-process `--measure-workers` executions use the same two-worker limit,
controlled initial overlapping generations and 200 gated churn cycles. Each
records observed high water 2, final owned count 0 and one capacity refusal.
There is no retained block corpus/input vector. The initial overlap/reaping is
warm-up; elapsed measurement covers churn plus its stop/restart cleanup, not
isolated CPU cost. Logging and test scheduling can dominate this workload.

| After-only metric, n=5 | Median | Min–max | MAD |
| --- | --- | --- | --- |
| Elapsed microseconds | 413,884 | 411,668–415,946 | 1,031 |
| Process peak RSS bytes, Darwin getrusage | 7,471,104 | 7,421,952–7,487,488 | 0 |
| Owned generation high water / final | 2 / 0 | Constant | 0 |

Raw final-source records: `review-worker-1..5.log`; earlier executions remain
retained as `worker-measure-1..5.log`. No passing before-version resource
measurement exists: its ownership assertion fails. Therefore **no RSS/CPU
improvement percentage is claimed**. Peak RSS includes the test/runtime/allocator,
not retained heap, whole-node state/VM/RPC/DB, cgroup or host MemAvailable.
A short 200-cycle test is not a sustained overload/retention soak or evidence
for the 2-GiB/2.5-GiB memory contract. The correction intentionally waits for work
that old stop did not own; correct draining can increase shutdown latency.

Five final-binary offline Harbinger genesis observer processes also exit 0.
Each uses a fresh external basedir, ordinary loopback health RPC, idle expiry
and partial-read shutdown, with production, P2P and gRPC disabled. Median
reported process peak RSS is 26,165,248 bytes (26,099,712–26,247,168; MAD 49,152).
Median shutdown is 0.071468 seconds (0.070418–0.095401; MAD 0.001050).
Raw samples, exact argv and matching binary hash are in
`review-offline-observer/results.json`; `review-measurement-summary.json`
records both summaries. This short genesis workload has no catch-up/history,
active VM/P2P mixture, compaction load or host/cgroup budget measurement.
It is **not** whole-node, mainnet, Linux or full-history capacity qualification.

## External artifact selection

All subsequent new build, fixture, scratch and evidence outputs use My Passport:

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/build'
export PYTHONDONTWRITEBYTECODE=1
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 4
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
"$TELENO_TEST_BUILD_ROOT/koinos_p2p_one_peer_sync_test" --measure-workers
```

For a new run, create a fresh owned root first and configure from this source;
do not reuse these paths concurrently. Explicit roots must be absolute,
non-root, existent directories without final symlinks/parent traversal. Fixture
children remain fresh and confined after canonicalization; known-source markers,
metadata/signatures and restore checks remain intact. External build selection
requires both explicit root selections; native diagnostic tools additionally
require matching CMake home/test-enabled configuration and contained binaries.
No external path is inferred as authorized merely because a file exists there.

The first external full suite exposed diagnostic assumptions: compiler/log
records required source-relative paths, and Python file URIs percent-encoded
My Passport's space while the existing native file-source contract expects a
literal filesystem path. Corrected **fixture inputs/report paths**, not public
URI decoding or runtime trust. Original failures remain in `external-ctest.log`
and `external-storage-retests.log`. A root-helper unit test also initially
misclassified a parent-traversal path; explicit traversal is now refused and
28/28 diagnostic helper tests pass. Production restore/durability policy and
input identity declarations were not changed. General percent-encoded file-URI
support is not implemented/claimed by this checkpoint.

## Actual verification

| Check | Result | Artifact |
| --- | --- | --- |
| Starting cache/state/P2P/mempool baseline | 5/5, 16.01 s | baseline-focused.log |
| Baseline failing disconnected worker reproduction | Exit 8, required stop ownership assertion | p1-reproduction.log |
| Fresh external configure/full build | Exit 0, same source/cache settings | external-configure.log, checkpoint-build.log |
| Full suite after external-fixture corrections | 38/38, 112.56 s | external-ctest-corrected.log |
| Subsequent full suite | 38/38, 129.94 s | final-ctest.log |
| Final checkpoint source/test suite | 38/38, 107.74 s | review-ctest.log |
| Final P2P/cache/path/admission focused tests | 4/4, 16.51 s | final-focused.log |
| Separate final-source UBSan component tests | 3/3, 21.10 s | review-ubsan-tests.log |
| M1 characterization plus existing mempool tests | Native 1/1, 0.49 s; UBSan 1/1, 0.56 s | m1-characterization-test.log, m1-characterization-ubsan-test.log |
| Full suite after M1 characterization | 38/38, 151.25 s | m1-final-ctest.log |
| UBSan after the M1 test addition | 3/3, 19.64 s | m1-final-ubsan-tests.log |
| Native version/help | Both exit 0, dirty identity above | review-version.log, review-help.log |
| External root and diagnostic helper units | 28/28 | external-path-tests-final.log |
| Five after-only worker churn processes | All exit 0, measured counts/RSS above | review-worker-1..5.log |
| Five offline genesis observer processes | All exit 0, limited measurements above | review-offline-observer/results.json |
| Koinos One strict MkDocs, external output | Exit 0, 0.56 s | koinos-one-docs.log |
| Docker availability | Exit 1, daemon unavailable; smoke not executed | docker-info.log |

UBSan flags are `-fsanitize=undefined -fno-sanitize=vptr -fno-omit-frame-pointer`
and halt-on-error. Prebuilt dependencies are not fully instrumented; vptr is not
qualified. This checkpoint did not repeat the Mac ASan initialization failure
or TSan early-exit attempts, or waive them. Their previous failures are context, not
current passes. Functioning integrated ASan/TSan and real transport teardown
remain required. The complete dependency build script **has not yet completed
successfully in this checkpoint**; fresh external attempts expose build-path
issues, retained as `external-dependency-workflow.log` (Make splits the external
Xcode executable path) and `external-dependency-ninja-workflow.log` (Ninja builds
libssh, then Hunter refuses a root containing spaces). The subsequent attempt
uses the ignored `build-single-copy-evidence/lr2-dependencies-EEMOFW` link to a
fresh external dependency directory, with Ninja; all generated dependency data
remains on My Passport. Its result must be recorded separately. No existing
dependency tree, Xcode installation, volume name or protection was changed.
Cached native CMake build/CTest/CLI passes do not satisfy a successful full
dependency workflow or the still-required goal gates.

No current historical state-delta audit, full differential or representative
mainnet/Linux memory/catch-up run was executed. The eligible authenticated pinned
full-history corpus, matching pre-range checkpoints and successor/reference
inputs remain missing. A fresh isolated Linux environment is also needed;
Docker has no daemon and the only listed Colima VM is stopped/unrelated. It was
not operated. Missing historical/sanitizer checks are not passes; LR5's separate
24-hour/seven-day whole-server soak remains pending.

## Changed files and handoff

Teleno changes during this checkpoint (earlier dirty work is preserved):

- `docs/implementation/low-resource-mainnet-observer-plan.md`.
- `docs/memoria.md`.
- `docs/validation/low-resource-lr2-memory.md`.
- `docs/validation/low-resource-lr2-p2p-retention.md`.
- `tests/artifact_root.hpp`.
- `tests/backup/direct_activation_test.cpp`.
- `tests/backup/direct_restore_harness_test.py`.
- `tests/backup/direct_restore_test.cpp`.
- `tests/backup/durable_publication_test.cpp`.
- `tests/backup/restore_plan_test.cpp`.
- `tests/cli/direct_restore_preflight_test.py`.
- `tests/cli/direct_restore_workflow_test.py`.
- `tests/scripts/fixture_paths_test.py`.
- `tests/scripts/lr2_memory_test.py`.
- `tests/storage/persistence_failure_test.cpp`.
- `tools/backup/direct_restore_fixture.cpp`.
- `tools/backup/record_sc0_baseline.py`.
- `tools/backup/restore_baseline_fixture.cpp`.
- `tools/backup/validate_single_copy_restore.py`.
- `tools/diagnostics/fixture_paths.py`.
- `tools/diagnostics/measure_lr2_memory.py`.
- `AGENTS.md`.
- `CHANGELOG.md`.
- `CLAUDE.md`.
- `docs/README.md`.
- `docs/configuration.md`.
- `docs/implementation/teleno-audit-remediation-plan.md`.
- `docs/logs-and-diagnostics.md`.
- `docs/performance/teleno-software-audit.md`.
- `docs/release-builds.md`.
- `docs/running-observer-node.md`.
- `src/CMakeLists.txt`.
- `src/p2p/p2p_node.cpp`.
- `src/p2p/p2p_node.hpp`.
- `src/p2p/types.hpp`.
- `tests/backup/backup_sftp_test.cpp`.
- `tests/p2p/p2p_one_peer_sync_test.cpp`.
- `docs/validation/low-resource-lr2-completion-matrix.md`.
- This new report: `docs/validation/low-resource-lr2-workers.md`.

An additional M1 characterization-only follow-up extends
`tests/mempool/mempool_adapter_test.cpp`. It proves that the 2,000-record response
limit does not bound retained pending work: 2,001 synthetic independent-account
records remain available by ID until TTL expiry, with logical RC/nonce cleanup.
Native focused test 1/1 passes in 0.49 seconds
(`m1-characterization-build.log`, `m1-characterization-test.log`). It is not an
aggregate budget, physical memory reclamation measurement, concurrent/fork
qualification or protocol-valid transaction workload. No production mempool
or admission behavior changes in this follow-up; M1 remains open.

Before introducing a capacity refusal, the native admission boundary must be
corrected and tested atomically: `controller_impl::submit_transaction()` emits
`koinos.transaction.accept` after execution, `MonolithRpcClient::broadcast()`
dispatches the synchronous EventBus, and main's mempool subscriber currently
catches/logs insertion exceptions. Adding a cap only in that subscriber could
therefore return a successful submission while retaining no transaction. This
is a code-supported integration risk, not a reproduced current-node capacity
failure (no aggregate cap exists yet). Reservation, duplicate/account concurrency,
fork replication, temporary records, post-admission notification failures and
unwind need explicit tests before installing the policy. Count/TTL or head-only
accounting cannot substitute for physical retained-fork and working-set budgets.

Koinos One documentation only:
`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
`docs/manual/koinos-one/backup-and-restore.md`. Its instructions/worktree were
read and existing edits preserved through additive documentation. No app code,
version/changelog, binary, packaging or submodule changed. There is no new native
commit to pin: integration must later deliberately commit/pin/stage/test the
intended native identity, under separate authorization.

## Checkpoint exit and remaining work

Local functional worker and external-data checkpoint passes; **supported
lifetime/race, full transport shutdown and whole-node LR2 exits do not**.
Required remaining local work: aggregate mempool admission/fork accounting and
atomic local capacity handling, RPC output/working-set backpressure, VM parsed/
active bytes and duplicate parsing, borrowed state-value lifetime and all other
P2P consumers. Do not represent the count cap as a 4-GB memory profile.

Rollback is review of only this checkpoint's source/tooling hunks in a stopped
owned test checkout, preserving the earlier dirty SC/LR work and all evidence.
There is no database migration/reset. Removing ownership would restore the
reproduced detached-worker shutdown risk. Never use a whole-worktree reset or
delete chain data to roll back. External run data remains retained.

**Every applicable LR2 Definition of Done item proven: NO. Goal remains active.**
