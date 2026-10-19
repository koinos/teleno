# LR2 M1-C: failure-atomic multi-fork admission

Date: 2026-10-03. Status: **local correction verified; aggregate M1 and LR2 remain open**.

## Target, scope and identity

The primary target remains Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB RAM.
These Mac ARM64/APFS diagnostics are supplementary correctness and allocation
evidence, not Linux, whole-node, mainnet throughput or 4-GB qualification.
Source: `/Users/pgarcgo/code/teleno`, branch `codex/reward-reinvestment-macos`,
HEAD `cfd2eb83b8080ca49911555108ce05f0a9068f01`, dirty `1.3.0-dev.0`.
Generated data is exclusively in the task-owned
`/Volumes/My Passport/teleno-lr2-EEMOFW/`; ignored evidence alias:
`build-single-copy-evidence/lr2-completion-qKjj7h/`.

This follows [M1-A/B](low-resource-lr2-admission.md) and
[S1](low-resource-lr2-state-ownership.md), preserving those changes. It is not
an aggregate capacity policy, pre-execution reservation, all-consumer budget or
claim of failure atomicity for block/LIB handling, prune or removal. No live
node, VM, VPS, chain data, keys, producer, trust policy, deployment, release,
commit/push, GUI implementation or submodule integration was operated.

## Reproduced failure

The library preflighted anonymous overlays for each mutable mempool state, then
committed them one at a time. Ordinary validation refusal unwound before that
loop, but a C++ allocation failure during parent writes or overlay reset could
leave records/indexes/nonces/resources changed after a rejected admission. The
allocating map `begin()`/`end()` wrappers were also declared `noexcept`, turning
some allocation failures into process termination rather than unwind.

The deterministic fixture has four mutable states: root pending state, block 1
pending state, and two competing height-2 pending states. One existing synthetic
transaction reserves 100 RC; the candidate requests another 100 of 1,000. IDs,
payer and signatures force heap-backed strings. A calling-thread, one-shot
`operator new/new[]` failure is injected at every ordinal during admission.
Snapshots compare returned pending protobuf bytes, pending nonce/count and RC
availability/reservation at each accessible checkpoint. They never dereference
released memory. Inputs bypass chain/signature validation.

The baseline sweep executed **455 processes**: 454 injected failures plus the
noninjected end case. **65 cases exited 91** for a rejected call with changed
visible state; **56 exited 86** through the test terminate handler. The remaining
334 exited 0. The JSON artifact records every ordinal and actual result.
Examples: ordinal 339 rejects with changed state before any candidate ID is
visible; ordinal 344 leaves the candidate in only some states; ordinal 327
terminates. These are confirmed local bugs, not suspected patterns.

Artifacts: `m1c-baseline-build.log`, `m1c-baseline-faults.json`.
The original executable was not preserved before its rebuild. For reproducible
supplementary confirmation, the same test object was linked to preserved
prior-S1 fresh-dependency mempool/state libraries, with remaining dependencies
from the cached tree. `m1c-reconstructed-baseline-link.json` records exact argv;
`m1c-reconstructed-baseline-{339,327}.log` reproduces exit 91/86. This reconstructed
binary is not mislabeled as the original baseline executable. The inadvertently
copied already-fixed binary is named `m1c-first-fixed-admission-test`.

Preserved baseline static-library SHA-256:

- Mempool: `3e43a3fa01400bfdc2dc95d3af2fff24cb4ccd4093a2af4603740af60de9de7e`.
- State DB: `6bdf1d07893e1f46760eea2c6c1981d189185f2e360f881054000236403a555e`.

## Correction and invariants

`anonymous_state_node::commit_all_writes()` is an additive, explicitly limited
native C++ API for write-only overlays with distinct writable **map state-node** parents.
It rejects null/uninitialized overlays, removals, finalized parents, duplicate
parents, anonymous parents and nonmap parents before publication. Requiring
named database parents prevents a nested anonymous parent from aliasing a
consumed source overlay. It is not a RocksDB/durable
transaction and does not replace general anonymous `commit()` or durable LIB
commit. Callers must exclude readers/writers for the whole operation.

All vectors, validation and fresh reset overlays are prepared before parent
mutation. The mempool acquires every relevant node lock while already holding
the database unique lock. After preparation, map node handles transfer existing
allocated key/value nodes into parents; replacements move immutable owners.
Fixed default allocators, string comparison/hash and shared ownership require
no new allocations or user callbacks in this publication phase. Parent
tombstones for written keys are removed exactly as in the original anonymous
squash; unrelated tombstones remain. Reset overlay ownership moves only after
all transfers. No full retained-state map clones or second value copies are
required for admission atomicity. Work scales with changed keys and fork count,
not a copy of every previously retained transaction.

Map iterator wrappers no longer claim `noexcept` while allocating. After
publication, debug-log allocation failure cannot report rejection for retained
work. An unrecordable debug log is best-effort; the existing bridge's counted
post-admission notification policy is unchanged.

Protocol/wire schemas, serialization, ordering, per-fork nonce conflicts,
resource accounting, source history, normal `remove_object()`, replay tombstones,
fresh pending roots, exact scar/lookahead/fallback rules, canonical validation
and persistence/fsync policy are unchanged. A nonce-conflicting fork still
retains its original pending transaction while a nonce-free competing fork may
accept the new ID. There is no generic root-mismatch bypass or retry loop.

### Regression evidence

The fixed sweep covers **364 cases**: **363 injected** plus one noninjected end
case. **356 reject with unchanged snapshots and candidate absent everywhere**,
then successfully readmit the same ID/nonce with correct RC. Seven failures in
post-publication best-effort logging still return success with the candidate
present everywhere; the final noninjected case succeeds. **No invalid case or
termination** occurs. Final artifact: `m1c-named-parent-fixed-faults.jsonl`.
Earlier `m1c-final-fixed-faults.jsonl` predates the final named-parent guard.
The CTest test also
covers fork-specific nonce conflict following block inclusion.

The state test directly observes **zero C++ new/new[] calls during map transfer**
and identical shared owners before/after transfer. It covers later invalid
overlays, duplicate/anonymous parents, null, finalized and nonmap parents without earlier
parent mutation; successful reset/reuse; immutable read ownership; roots and
tombstones equivalent to original anonymous commit; empty batches.

An initial state-test oracle incorrectly used direct parent puts over tombstones
instead of the original anonymous commit. The initial cached/fresh suites and
UBSan subset therefore fail that new assertion; logs remain as failed evidence.
`m1c-atomic-diagnostic-build.log` and the corrected test explain the difference.
Only the test oracle was corrected; no normal tombstone/root semantics changed
to make it pass. Final results below supersede those initial runs.

This is not a supported ASan/TSan run or proof against real machine OOM: C
`malloc`, aligned allocations, dependency internals, simultaneous failures,
every workload branch, block/LIB/removal/prune failure and races are outside
this fault probe. The no-allocation publication contract is limited to the
fixed map/key/allocator implementation, not arbitrary backend/comparator types.

## Scoped allocation comparison

Five alternating processes per implementation, equal fixture and admission
warm-up: 64 prefilled synthetic records with 64-KiB signatures, four mutable
states, then one candidate with a 128-KiB signature. Baseline uses the preserved
S1 libraries and identical current measurement object; link argv is in
`m1c-measure-baseline-link.json`. C++ allocation requests are counted only during
the candidate call, including logging; these are **not retained-heap bytes**.
Process RSS includes fixture/runtime/dependencies. Regression tests ran
concurrently; the short timing/peak samples do not isolate driver/host noise.

| Metric | Baseline median (min–max; MAD) | Atomic median (min–max; MAD) |
| --- | --- | --- |
| C++ allocation attempts | 454 (454–454; 0) | 363 (363–363; 0) |
| Requested C++ allocation bytes | 2,384,376 (same; 0) | 1,856,920 (same; 0) |
| Admission time | 225 us (207–236; 7) | 208 us (192–238; 14) |
| Process peak RSS | 36,126,720 bytes (36,012,032–36,257,792; 32,768) | 35,749,888 bytes (35,602,432–35,766,272; 16,384) |

The measured request reduction is **527,456 bytes and 91 allocation attempts**
for this one synthetic candidate. Timings overlap, and the small peak difference
does not prove a whole-node/RSS/Linux improvement. No production percentage
gain or low-resource default is inferred. Actual persistent/fork/caller-held
memory, many-account scaling and overload policy still require accounting.
Artifact: `m1c-named-parent-measurements.json` includes all ten final raw samples
and dispersion. Earlier `m1c-measurements.json` predates the named-parent guard;
the final measured allocation-request counts/bytes are unchanged.

## Build and test evidence

Current final results are recorded after the corrected oracle; pending entries
are not passes.

| Check | Result | Artifact |
| --- | --- | --- |
| Cached-dependency native build, all targets | Exit 0 | `m1c-named-parent-build.log` |
| Fresh-dependency native build, all targets | Exit 0 | `m1c-named-parent-clean-build.log` |
| Final serialized cached full CTest | 41/41, exit 0, 112.52 seconds | `m1c-serialized-current-ctest.log` |
| Final serialized fresh-dependency full CTest | 41/41, exit 0, 114.42 seconds | `m1c-serialized-clean-ctest.log` |
| Final UBSan subset | 7/7, exit 0, 68.80 seconds | `m1c-named-parent-ubsan-tests.log` |
| Version/help, both native trees | Exit 0, `1.3.0-dev.0+cfd2eb83b808-dirty` | `m1c-named-parent-{version,help}.log`, `m1c-named-parent-clean-{version,help}.log` |
| Required dependency script rerun, before final parent guard | Exit 0; unchanged dependencies, tests ON, external roots; native builds above verify final source | `m1c-required-dependency-workflow.log` |
| Final Koinos One strict MkDocs, documentation only | Exit 0, 0.42 seconds | `m1c-final-koinos-one-mkdocs.log` |

Final native SHA-256 (after named-parent guard):

- Cached inputs: `809ca980b6acf04e06dff644e31ef25e41dc245f85045501c694a58f3d8d30e3`.
- Fresh dependencies: `c7ecc38a8e4f852076a1cb38d57c39ec4f1727a16191cdcdc55397388fa3e912`.
- Admission test: `9b0908db48bdef3a3aa34bb0949623d176ef4199af59749a415a845a692aeadc`.

Intermediate corrected suites pass 41/41 (138.93 / 160.62 seconds) before the
final parent guard. After that guard, concurrently run suites each pass 40/41:
cached preflight subprocess exceeds 15 seconds; fresh SC5 `direct-1-stage`
returns a preserved, paused/incomplete attempt under its 2-second transfer
configuration. Native mempool/atomic/state tests pass in both. Artifacts:
`m1c-named-parent-{ctest,clean-ctest}.log` and
`sc5-run-l5gy5xr2/{summary.json,direct-1-stage.stdout,commands.json}`. USB/build/
test contention is consistent with these observations, not independently
profiled as the sole cause. Both final suites pass sequentially without changing
timeouts, retries, restore trust or test expectations; failures remain recorded.
The paused fixture attempt is preserved, not activated or repaired in place.

The three external caches name the exact source checkout, Release/tests ON,
with no other-workspace references. Separate UBSan excludes vptr and complete
dependency instrumentation as documented in S1. Functioning ASan/TSan and a
Linux diagnostic runtime are still unavailable, not waived. Docker smoke cannot
run without its daemon; no unrelated VM was started.

### Reproduction commands

From this repository, select the externally owned build and scratch paths:

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/build'
export PYTHONDONTWRITEBYTECODE=1
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 4
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/koinos_mempool_admission_atomicity_test"
"$TELENO_TEST_BUILD_ROOT/koinos_mempool_admission_atomicity_test" --measure-admission
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
```

Repeat with external `dependency-workflow-alias-build/` for the freshly built
dependencies. For UBSan select `ubsan/`, set
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, and run CTest regex
`mempool_(adapter|admission_atomicity)|atomic_write_batch|state_value_lifetime|state_db_(durability|pending_root)|controller_delta`.
The required dependency workflow uses the successful S1 command-local
Ninja/`MAKE=/usr/bin/make` and space-free external dependency alias, with tests ON;
its current rerun log is separately identified above.

Actual required dependency command (follow with full build/CTest on the selected
external tree, as above):

```bash
TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp' \
KOINOS_DEPS_ROOT='/Users/pgarcgo/code/teleno/build-single-copy-evidence/lr2-dependencies-EEMOFW' \
HUNTER_ROOT='/Users/pgarcgo/code/teleno/build-single-copy-evidence/lr2-dependencies-EEMOFW/hunter' \
KOINOS_NODE_BUILD_DIR='/Volumes/My Passport/teleno-lr2-EEMOFW/dependency-workflow-alias-build' \
CMAKE_GENERATOR=Ninja MAKE=/usr/bin/make \
LIBSSH_SOURCE_TARBALL='/Volumes/My Passport/teleno-lr2-EEMOFW/dependencies-Uya3ee/downloads/libssh-0.12.0.tar.xz' \
KOINOS_BUILD_TESTS=ON JOBS=4 PYTHONDONTWRITEBYTECODE=1 \
/usr/bin/arch -arm64 /bin/bash ./scripts/build-cpp-libp2p-koinos.sh
```

The alias resolves into the task-owned external dependency directory, not a
different source checkout. No host/global Make configuration was changed.

## Changed files and remaining gates

Runtime/tests:

- `src/CMakeLists.txt`.
- `src/koinos/mempool/mempool.cpp`.
- `src/koinos/state_db/backends/map/map_backend.hpp`, `map_backend.cpp`.
- `src/koinos/state_db/state_delta.hpp`, `state_delta.cpp`.
- `src/koinos/state_db/state_db.hpp`, `state_db.cpp`.
- `tests/mempool/admission_atomicity_test.cpp` (new).
- `tests/storage/atomic_write_batch_test.cpp` (new).

Documentation: this new report, completion matrix, active plan, project memory,
prior admission-report qualification note, native docs index/RPC/release docs,
CHANGELOG, identical AGENTS/CLAUDE and scoped audit/remediation status. Koinos
One handoff extends only the existing current backup implementation document and
manual backup/restore page. No binary, submodule, app version or packaged feature
was updated; the dirty HEAD/build hashes are not a committed integration pin.
The ignored preservation manifest records cumulative goal changes separately
from pre-existing work; README and the throughput plan remain unchanged.

Next M1 work must account aggregate records, shared payloads, map/tombstone/index
metadata, retained forks, staged admission, transient chain execution and
caller-held values; add atomic capacity reservation/local refusal classification
without consensus-invalid labels or peer penalties. Many-account, large-valid-
input, concurrent capacity, expiry/reorg/LIB/restart and reclamation evidence
remain required. Block/prune/removal allocator atomicity is not proven here.
R1/P2/V1/S1 lifecycle and consumer budgets, working integrated sanitizers,
authenticated immutable full-history checkpoint/successor/reference inputs and
representative isolated Linux whole-node qualification remain open. No current
historical state-delta audit/differential validation was run; previous reports
and synthetic scar fixtures do not prove full parity. LR5 long soak remains a
separate milestone. Whole-audit TODO remains open.

**Every applicable LR2 Definition of Done item proven: NO.**

## Cumulative LR2-goal source inventory

The current goal changed **75 Teleno files**, not 75 new files: 45 extend
pre-existing dirty/untracked work and 30 are newly modified tracked/new files.
Another **66 pre-existing files remain byte-identical**, including README and
the high-throughput plan; none were removed. The M1-C checkpoint itself changes
23 of these files. No staged changes or commits exist. The ignored manifest
`m1c-source-preservation.json` includes baseline/current hashes, classifications
and validation results; this list excludes unrelated preserved work.

- `AGENTS.md`.
- `CHANGELOG.md`.
- `CLAUDE.md`.
- `docs/README.md`.
- `docs/configuration.md`.
- `docs/implementation/low-resource-mainnet-observer-plan.md`.
- `docs/implementation/teleno-audit-remediation-plan.md`.
- `docs/logs-and-diagnostics.md`.
- `docs/memoria.md`.
- `docs/performance/teleno-software-audit.md`.
- `docs/release-builds.md`.
- `docs/rpc-endpoints.md`.
- `docs/running-observer-node.md`.
- `docs/validation/low-resource-lr2-admission-atomicity.md`.
- `docs/validation/low-resource-lr2-admission.md`.
- `docs/validation/low-resource-lr2-completion-matrix.md`.
- `docs/validation/low-resource-lr2-memory.md`.
- `docs/validation/low-resource-lr2-p2p-retention.md`.
- `docs/validation/low-resource-lr2-state-ownership.md`.
- `docs/validation/low-resource-lr2-workers.md`.
- `src/CMakeLists.txt`.
- `src/core/monolith_rpc_client.hpp`.
- `src/koinos/mempool/mempool.cpp`.
- `src/koinos/mempool/mempool.hpp`.
- `src/koinos/state_db/backends/backend.hpp`.
- `src/koinos/state_db/backends/iterator.cpp`.
- `src/koinos/state_db/backends/iterator.hpp`.
- `src/koinos/state_db/backends/map/map_backend.cpp`.
- `src/koinos/state_db/backends/map/map_backend.hpp`.
- `src/koinos/state_db/backends/map/map_iterator.cpp`.
- `src/koinos/state_db/backends/map/map_iterator.hpp`.
- `src/koinos/state_db/backends/rocksdb/object_cache.cpp`.
- `src/koinos/state_db/backends/rocksdb/object_cache.hpp`.
- `src/koinos/state_db/backends/rocksdb/rocksdb_backend.cpp`.
- `src/koinos/state_db/backends/rocksdb/rocksdb_backend.hpp`.
- `src/koinos/state_db/backends/rocksdb/rocksdb_iterator.cpp`.
- `src/koinos/state_db/backends/rocksdb/rocksdb_iterator.hpp`.
- `src/koinos/state_db/backends/types.hpp`.
- `src/koinos/state_db/merge_iterator.cpp`.
- `src/koinos/state_db/merge_iterator.hpp`.
- `src/koinos/state_db/state_db.cpp`.
- `src/koinos/state_db/state_db.hpp`.
- `src/koinos/state_db/state_db_types.hpp`.
- `src/koinos/state_db/state_delta.cpp`.
- `src/koinos/state_db/state_delta.hpp`.
- `src/main.cpp`.
- `src/p2p/p2p_node.cpp`.
- `src/p2p/p2p_node.hpp`.
- `src/p2p/types.hpp`.
- `tests/artifact_root.hpp`.
- `tests/backup/backup_sftp_test.cpp`.
- `tests/backup/direct_activation_test.cpp`.
- `tests/backup/direct_restore_harness_test.py`.
- `tests/backup/direct_restore_test.cpp`.
- `tests/backup/durable_publication_test.cpp`.
- `tests/backup/restore_plan_test.cpp`.
- `tests/cli/direct_restore_preflight_test.py`.
- `tests/cli/direct_restore_workflow_test.py`.
- `tests/mempool/admission_atomicity_test.cpp`.
- `tests/mempool/mempool_adapter_test.cpp`.
- `tests/p2p/p2p_one_peer_sync_test.cpp`.
- `tests/scripts/fixture_paths_test.py`.
- `tests/scripts/lr2_memory_test.py`.
- `tests/storage/atomic_write_batch_test.cpp`.
- `tests/storage/cache_invariants_test.cpp`.
- `tests/storage/persistence_failure_test.cpp`.
- `tests/storage/state_db_durability_test.cpp`.
- `tests/storage/state_db_pending_root_test.cpp`.
- `tests/storage/state_value_lifetime_test.cpp`.
- `tools/backup/direct_restore_fixture.cpp`.
- `tools/backup/record_sc0_baseline.py`.
- `tools/backup/restore_baseline_fixture.cpp`.
- `tools/backup/validate_single_copy_restore.py`.
- `tools/diagnostics/fixture_paths.py`.
- `tools/diagnostics/measure_lr2_memory.py`.
