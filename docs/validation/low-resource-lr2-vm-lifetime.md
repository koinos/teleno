# LR2 V1-A: parsed-module lifetime and cold-parse working set

Dates: 2026-10-03 to 2026-10-04 (Europe/Berlin).
Status: **local correction verified; V1/LR2 NOT qualified**.

Subsequent [V1-B](low-resource-lr2-vm-allocation.md) corrects the terminating
context-allocation path. The exit-77 commands/results in this V1-A report refer
to its preserved binary, not the current corrected test binary. Earlier failures
and unexecuted Linux/historical/sanitizer gates remain evidence, not passes.

Linux Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB physical RAM is the primary
optimization and qualification target. This portable native checkpoint uses the
Mac only for development diagnostics. It does not select Linux limits or prove
the whole-node RSS, host-memory, catch-up, durability or recovery gates.

## Scope, source and owned evidence

Source: `/Users/pgarcgo/code/teleno`, branch `codex/reward-reinvestment-macos`,
HEAD `cfd2eb83b8080ca49911555108ce05f0a9068f01`, dirty `1.3.0-dev.0`.
Environment: macOS 26.5 / Apple Clang 16 / arm64 / 16 GiB / 10 CPUs;
CMake/CTest 4.2.3, Release `-O3`, tests and cpp-libp2p ON. Boost 1.83 and
RocksDB 8.8.1 remain unchanged. CMake caches identify this exact source checkout,
not another workspace. Existing dependency installs are read-only inputs.

All newly generated builds, diagnostic output, test databases and temporary
data remain under the task-owned `/Volumes/My Passport/teleno-lr2-EEMOFW/` run:
`build/`, `dependency-workflow-alias-build/`, `ubsan/`, `tmp/`, `evidence/` and
`koinos-one-v1a-manual-site/`. The ignored repository alias
`build-single-copy-evidence/lr2-completion-qKjj7h/` points to that evidence.
No existing node data, backup, live server, producer or other repository runtime
is opened or changed. Documentation-only Koinos One coordination is separate
from GUI/package/submodule integration. No commit, push, release or deployment.

`v1a-worktree-start.json` preserves the 159 pre-existing dirty/untracked file
hashes and verifies no drift from the M1-G final manifest. README and the
high-throughput plan remain protected. Final preservation/build hashes and
complete changed-file inventory are recorded below and in `v1a-*` artifacts.

### Verify the linked dependency, not an upstream ownership comment

Hunter pins Koinos Fizzy commit `b9bf7feaa8009a3d4f4bdd49245a5cd55d122055`,
package version 0.8.0. Its source archive SHA1 is
`d4e682dca504a831b30274fbec0b154b8b7adfd1`, verified against Hunter's config.
Read-only source was obtained in the fresh owned evidence child
`v1a-source-I09vWF/`; neither the installed dependency nor another repository
was edited. Provenance: [pinned Koinos Fizzy source archive](https://github.com/koinos/fizzy/archive/b9bf7feaa8009a3d4f4bdd49245a5cd55d122055.tar.gz).

In that fork, `lib/fizzy/capi.cpp` returns an owned raw parsed module;
`lib/fizzy/instantiate.hpp` holds `const Module*`, and instances borrow that
module. Destroying an instance does not destroy its module. The installed C
header's ownership-transfer comment is stale for this fork. Existing runners
correctly retain a shared module owner until their instance/context is freed.
Cloning/releasing the cached module based on the comment would be incorrect.

## Reproduced defects and implemented invariants

1. **Parsed-module leak on shared-owner allocation failure.** The old
   `parse_bytecode()` returned `make_shared(module_guard(raw))` without a guard
   for `raw`. In a fresh empty-WASM process, allocation ordinal 1 rejects shared
   ownership after parse: one allocation / 344 requested C++ bytes remains
   alive, test exit 1. Ordinal 0 and uninjected ordinal 2 exit 0. Artifacts:
   `v1a-before-parse-{0,1,2}.log`, preserved `v1a-before-fizzy-lifetime-test`.
   A nonallocating unique owner now protects the module until shared ownership
   succeeds. All three cases end with zero tracked allocations and exit 0.
   `module_guard` is noncopyable to prevent duplicate raw ownership.
2. **Cache insertion retired old entries before fallible preparation.** This
   ordering is established by source inspection; the old path was not executed
   in the cache fault sweep. New list/map/key allocations now precede eviction;
   a rejected allocation rolls
   back only the staged entry, retaining existing membership/recency. Four
   injected allocation boundaries preserve both existing owners and allow a
   normal retry; successful insertion still obeys LRU/entry capacity. The
   preparation temporarily holds at most one extra entry under the cache lock.
3. **Disposable cache failure could fail usable VM work.** `get_or_load()`
   catches only `std::bad_alloc` from cache publication, returning the already
   owned parsed module. It does not swallow parse, instance, execution or host
   failures. Publication remains optional; no invalid module is substituted.
4. **Concurrent cold callers duplicated immutable parse payloads.** One
   backend-local mutex serializes cold parsing and rechecks the cache after
   waiting. Retained same-ID cold callers share one owner; warm hits do not
   acquire/wait for the parser mutex. Empty IDs and a zero-size cache still do
   not retain modules. Publication failure or eviction can cause a subsequent
   reparse: this is not a permanent per-ID deduplication guarantee.

The parser/cache locks are released before instance creation, execution and
host callbacks. Each execution retains its module and creates a fresh mutable
instance. Existing memory-page limit 512, call-depth limit 251, instruction
metering, contract results and host exception propagation remain unchanged.
No transaction ordering, root, receipt, replay exception, persistence, archival
retention, source trust or protocol resource limit is changed.

This policy bounds **simultaneous parsing to one per backend**, not parsed-cache
bytes, waiting callers, active instances, malloc-backed stacks or total memory.
Cold distinct-ID calls serialize too; mutex fairness/cancellation and maximum
waiting time are not guaranteed. Waiting callers can retain bytecode and other
state. Cache entries remain 32. Linux workload evidence and compatible queue/
byte/instance policies remain required; no whole-node capacity claim follows.

### Additional confirmed dependency defect: execution-context OOM termination

The pinned `fizzy_create_metered_execution_context()` is `noexcept` but performs
an uncaught `make_unique`. A one-shot failed allocation invokes termination
instead of returning null or propagating a controlled error. The isolated
`--context-fault` process replaces only its own terminate handler with `_Exit(77)`
to record that path without a crash/core; observed exit **77**. It is not a pass,
real machine OOM, runtime modification or changed system protection.

**Not fixed by V1-A.** A reviewed dependency correction and fresh dependency
build/retry/instance cleanup checks are necessary before V1/LR2 closure. Do not
claim that the runner's existing null check handles this terminating path.
`v1a-context-fault.log` plus the command exit evidence and pinned source provide
the reproduction; required functioning ASan/TSan remain separate gates.

## Focused regression coverage

New `koinos_fizzy_lifetime_test` exercises raw/shared ownership failures,
four cache-publication allocation boundaries, optional-retention failure/retry,
eight gated cold callers, warm access during a paused cold load, caller ownership
through eviction, parse failure followed by retry and zero-retention behavior.
Real execution covers 20 warm runs, eight concurrent callers × 100 runs,
20 nested host calls with fresh instance memory and identical metering,
host exception propagation/cleanup and rejection of missing `_start`.
The existing allocation-free cache-hit/concurrent LRU test also passes.

Ordinary C++ allocation tracking uses a fixed thread-local table. It excludes
aligned new, direct malloc/execution stacks, usable allocation size, fragmentation
and allocator retention. Measurements with tracking are single-threaded; no
cross-thread free accounting is claimed. Controlled burst tests disable tracking.
Fixture TLS/thread stacks, linked runtime and allocator contribute to RSS.
These synthetic WASM fixtures are not historical or mainnet workloads.

## Measurements: five serialized processes per group

No build or other test job competed with these short samples. Inputs, module
count, data sections and warm-up are identical for the preserved before/current
single-thread binary comparisons. Timing is not CPU-isolated and short sample
noise precludes a general throughput percentage. Raw 45 process samples and
median/min/max/MAD are in `v1a-comparison.json` and the individual JSON files.

Cold: 16 different IDs with 128 KiB data each. Warm: same 16-load warm-up plus
80 hits. Uncached: 16 runs with empty IDs. Before/after results are identical:

| Metric | Cached cold/warm | Uncached |
| --- | --- | --- |
| Retained ordinary C++ requested bytes after initial 16 runs | 2,108,016 | 0 |
| Peak ordinary C++ requested bytes | 2,239,556 | 263,171 |
| Total requested bytes, cold / warm | 8,412,208 / 18,937,328 | 8,410,288 |
| Final tracked live allocations after backend destruction | 0 | 0 |
| Protocol meter ticks, cold / warm | 48 / 288 | 48 |

Cold elapsed microseconds: before median 449 (398–871; MAD 51), after 362
(352–377; MAD 8). Warm: 706 (661–847; 45) versus 654 (623–774; 31).
Uncached: 221 (201–244; 6) versus 207 (203–227; 4). These observations do not
establish a throughput gain. Cached cold median peak RSS is 11,649,024 versus
11,665,408 bytes; warm 11,649,024 versus 11,681,792; uncached 9,437,184 versus
9,469,952. **No ordinary single-thread heap/RSS reduction is demonstrated.**

### Controlled cold-burst mechanism, not a natural latency benchmark

The **current binary** contrasts the former get/parse/put strategy with
`get_or_load()` under a controlled cold gate. Eight callers hold all returned
owners; each module has a 2 MiB data section. The preparation gate forces the
former path's callers past their miss before publication; timed duration begins
only when that gate opens. This is not an old production binary, natural queue
latency, consensus throughput or an executed Linux benchmark.

| Metric | Former strategy, same ID | Coalesced strategy, same ID | Coalesced strategy, 8 IDs |
| --- | --- | --- | --- |
| Parses / unique caller-held modules, all samples | 8 / 8 | 1 / 1 | 8 / 8 |
| Peak active parsers median (min–max; MAD) | 8 (6–8; 0) | 1 (1–1; 0) | 1 (1–1; 0) |
| Process peak RSS bytes median (min–max; MAD) | 43,237,376 (41,025,536–52,740,096; 2,211,840) | 22,970,368 (19,759,104–24,035,328; 1,064,960) | 32,423,936 (32,407,552–33,619,968; 16,384) |
| Duration after gate µs median (min–max; MAD) | 1,711 (1,583–2,002; 128) | 841 (738–972; 100) | 2,826 (2,635–2,911; 60) |

Distinct-ID parsing has no deduplication benefit and serialization introduces
latency. Those columns do not constitute equivalent-work before/after proof for
diverse IDs. Measure that workload and contention/queues on Linux before choosing
the complete VM policy. No cache-byte/global active-instance bound is proven.

## Reproduction and checkpoint validation

Set these explicitly for every data-producing command (replace the build root
with the separate dependency or UBSan tree when appropriate):

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/build'
export PYTHONDONTWRITEBYTECODE=1
cmake --build "$TELENO_TEST_BUILD_ROOT" --target koinos_fizzy_lifetime_test koinos_cache_invariants_test --parallel 2
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" -R 'koinos_(fizzy_lifetime|cache_invariants)_test' --output-on-failure
"$TELENO_TEST_BUILD_ROOT/koinos_fizzy_lifetime_test" --parse-fault 1
"$TELENO_TEST_BUILD_ROOT/koinos_fizzy_lifetime_test" --measure warm
"$TELENO_TEST_BUILD_ROOT/koinos_fizzy_lifetime_test" --burst after
"$TELENO_TEST_BUILD_ROOT/koinos_fizzy_lifetime_test" --context-fault
```

Run `cold`, `warm`, `uncached` and each `before`, `after`, `diverse` burst five
times in fresh processes. The context-fault command deliberately exits 77;
do not count it as a successful validation or run it against any live process.

Focused build exits 0; focused CTest **2/2**, 2.08 seconds. An initial UBSan
command used nonexistent `koinos_admission_atomicity_test` and exits 1; the
correct target is `koinos_mempool_admission_atomicity_test`. Both logs are kept.
Initial WASM fixture body-size declaration was corrected before measurements;
it is not a production bug or discarded failure evidence. Full checkpoint
build/CTest/UBSan/CLI/docs results and artifact hashes are appended after actual
execution, never inferred from M1-G's older binaries.

The first cached full-suite attempt passed 45/46 in 179.22 seconds, exit 8:
the first preflight subprocess exceeded its existing 15-second deadline while
the separate dependency build was also active. No VM execution occurs on that
preflight path. An isolated unchanged recheck passes 1/1 in 1.23 seconds.
The Mac startup delay's cause is **not attributed**; competing build activity
is context, not proven causation. The original failure log remains, and a fully
serialized repeat is recorded separately without changing deadlines/checks.

### Actual final gates and native identities

| Check | Executed result | Artifact prefix/file |
| --- | --- | --- |
| Required `KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh` | Exit 0, separate owned dependency/configuration path, ARM64 shell | `v1a-dependency-workflow.log` |
| Cached / separate dependency all-target builds | Both exit 0 | `v1a-cached-all-build.log`, `v1a-dependency-all-build.log` |
| First cached full CTest | 45/46, exit 8, 179.22 s; preflight subprocess timeout retained | `v1a-cached-ctest.log` |
| Unchanged preflight recheck | 1/1, exit 0, 1.23 s | `v1a-preflight-recheck.log` |
| Fully serialized cached full CTest | 46/46, exit 0, 120.20 s | `v1a-cached-ctest-serialized.log` |
| Separate dependency full CTest | 46/46, exit 0, 162.24 s | `v1a-dependency-ctest.log` |
| Separate supported UBSan subset | 12/12, exit 0, 45.39 s; vptr/dependencies excluded | `v1a-ubsan-tests.log`, `v1a-ubsan-build-corrected.log` |
| Four actual native version/help checks | All exit 0, `1.3.0-dev.0+cfd2eb83b808-dirty` | `v1a-cli-status.log`, `v1a-{cached,dependency}-{version,help}.log` |
| Strict Koinos One manual build | Exit 0, 3.19 s, external output | `v1a-koinos-one-docs.log` |
| Docker prerequisite | Exit 1: daemon socket absent; smoke NOT executed | `v1a-docker-info.log` |
| Pinned execution-context allocation failure | Isolated terminate handler exit 77; unresolved defect, NOT a pass | `v1a-context-status.log` |

Required workflow configuration (the preceding external-root exports apply):

```bash
KOINOS_DEPS_ROOT=/Users/pgarcgo/code/teleno/build-single-copy-evidence/lr2-dependencies-EEMOFW \
HUNTER_ROOT=/Users/pgarcgo/code/teleno/build-single-copy-evidence/lr2-dependencies-EEMOFW/hunter \
KOINOS_NODE_BUILD_DIR='/Volumes/My Passport/teleno-lr2-EEMOFW/dependency-workflow-alias-build' \
TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/dependency-workflow-alias-build' \
LIBSSH_SOURCE_TARBALL='/Volumes/My Passport/teleno-lr2-EEMOFW/dependencies-Uya3ee/downloads/libssh-0.12.0.tar.xz' \
CMAKE_GENERATOR=Ninja MAKE=/usr/bin/make KOINOS_BUILD_TESTS=ON JOBS=2 \
/usr/bin/arch -arm64 /bin/bash ./scripts/build-cpp-libp2p-koinos.sh
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 2
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
```

Set `TELENO_TEST_BUILD_ROOT` explicitly to the intended tree before the last four
commands; command-local assignments on the script do not persist in the shell.
Both configurations name `/Users/pgarcgo/code/teleno` in `CMAKE_HOME_DIRECTORY`.
This workflow uses pinned existing dependency sources/installs, not an empty
dependency-cache rebuild or a Linux artifact. Warnings about deprecated Boost/
YAML interfaces and duplicate static libraries are retained, not build errors.

Separate UBSan flags: `-fsanitize=undefined -fno-sanitize=vptr
-fno-omit-frame-pointer`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
It runs VM/cache, transaction-work, controller, state-value lifetime, write-batch,
retained-memory, pending-record, P2P, admission/cleanup and RPC tests, with
uninstrumented dependencies. This is neither ASan leak/lifetime qualification nor
TSan race qualification. Current-source ASan/TSan and Linux checks remain unrun;
earlier runtime/tooling failures are context, not waivers or passes.

SHA-256:

- Cached native: `5c8925b616d7ac574304e45cd1ab076d0eceeef5bb81c187195889b0eba584ca`.
- Separate dependency native: `9abc06f1a57a8a9934caa544dfd7b68876b8881266611a2ee21f9a7dd58ae343`.
- Current/preserved after VM test: `3b11fd831a2534f6589930553b8528b74550edd0617dc7f59912b911f5a61078`.
- Preserved before VM test: `8cf4ca65160878f4d6391f1d2e97dab1825406c72957de223f573c19a222ebd8`.

The cached and separate native binaries differ because their dependency/build
paths differ; both are ARM64, same dirty source/version. HEAD is the base commit,
not a commit containing these uncommitted features. **No valid new submodule
integration pin or releasable artifact is supplied.** Review/commit/build/package
integration requires separately authorized work.

### Complete checkpoint file inventory and preservation

Twenty Teleno files changed in V1-A:

- `AGENTS.md`, `CLAUDE.md`, `CHANGELOG.md`.
- `docs/README.md`, `docs/configuration.md`, `docs/logs-and-diagnostics.md`,
  `docs/release-builds.md`, `docs/running-observer-node.md`, `docs/memoria.md`.
- `docs/implementation/low-resource-mainnet-observer-plan.md`,
  `docs/implementation/teleno-audit-remediation-plan.md`,
  `docs/performance/teleno-software-audit.md`.
- `docs/validation/low-resource-lr2-completion-matrix.md`,
  `docs/validation/low-resource-lr2-memory.md`,
  `docs/validation/low-resource-lr2-vm-lifetime.md` (new).
- `src/CMakeLists.txt`, `src/koinos/vm_manager/fizzy/fizzy_vm_backend.cpp`,
  `src/koinos/vm_manager/fizzy/module_cache.cpp`,
  `src/koinos/vm_manager/fizzy/module_cache.hpp`.
- `tests/vm/fizzy_lifetime_test.cpp` (new).

Koinos One changes only its two already-dirty documentation files:
`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
`docs/manual/koinos-one/backup-and-restore.md`. Its instructions/worktree were
read before additive edits; no app changelog, code, config generation, submodule,
binary, package or release was changed. Source links are handoff references,
not evidence that uncommitted documents are published or packaged.

Compared with the V1-A starting manifest, 142 other pre-existing files are
unchanged; 17 already-dirty files are deliberately extended, one previously
clean tracked VM source changes, and two new files are added. No pre-existing
file is removed or discarded; out-of-scope bytes remain unchanged. Current
worktree has 162 dirty/untracked files;
103 belong to the cumulative LR2 goal inventory, not all to this checkpoint.
Exact hashes/inventory are in `v1a-source-preservation.json`.
README SHA-256 remains
`b05852f8e80a0e51cba6a47e1f36d26fd665d5246315d9dc8d36b99eee43c419`;
high-throughput plan remains
`4ca6127c5ea756250317436c1e4d2a9423825dda442256d50a9dc9acbc1e7a8c`.
Final documentation/link/whitespace/guidance checks are recorded in
`v1a-final-checks.json`; first counts remain separately preserved.
The first cumulative-inventory assertion expected 101; actual 103 also includes
the two cache files first extended by this goal in V1-A. The report/assertion is
corrected, and `v1a-first-source-preservation.json` retains the earlier output.

## Remaining gates and rollback

V1 still requires parsed-memory attribution and justified cache-byte,
waiting/admission/instance capacity/lifetime policy, actual diverse execution
output fixtures and representative Linux mixed-workload/latency proof. The
context-creation terminating allocation path requires a dependency correction.
Other M1/P2/R1/S1 requirements remain in the [completion matrix](low-resource-lr2-completion-matrix.md).

Next independent V1 checkpoint: correct the two allocating context-creation
functions in a newly owned build of the exact pinned dependency, without
changing its borrowing-module semantics or modifying existing installs.
Verify direct creation failures, runner instance/module cleanup, unchanged
retry/metering and controlled local failure classification. Later queue/instance
admission must not deadlock nested calls or convert resource exhaustion into
consensus invalidity for canonical validation. These are planned requirements,
not implemented capabilities or authority to update a live runtime.

Missing prerequisites: functioning integrated ASan/TSan, isolated authorized
Ubuntu/x86_64/ext4 2-vCPU/4-decimal-GB whole-machine environment, authenticated
pinned full-history backup and immutable successor/reference/checkpoint inputs.
Current historical audit/differential range has **not** been run here. Local
controller/state/root regressions do not replace it. LR5's long soak is separate.
No container smoke is claimed without an available daemon.

Rollback is a reviewed source change removing cold-parse coalescing while
retaining strong raw ownership and failure-atomic optional caching, followed by
the same regressions and Linux comparisons. There is no DB/schema migration or
chain-data rollback. Do not discard other worktree edits.

**Every applicable LR2 Definition of Done item proven: NO.**
