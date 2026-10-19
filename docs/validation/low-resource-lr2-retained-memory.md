# LR2 M1-D: indexed fork-retention inventory

Date: 2026-10-03. Status: **local diagnostic implementation; M1/LR2 NOT complete**.

The primary target remains Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB physical
RAM. This portable inspection checkpoint selects no runtime limit from Mac
measurements and does not qualify the VPS. Complete history/receipts, protocol,
durability and observer-first behavior remain unchanged.

## Problem and implementation

Visible-head pending count is not retained memory. Expiry/removal updates current
fork heads; older mutable states can retain records until LIB advancement.
Immutable map clones share value owners but copy keys/map nodes. Counting each
reference as another payload overcounts clones; deduplicating equal contents
incorrectly merges independent owners.

The additive C++ `database::get_retained_memory_usage(unique_lock)` scans indexed
deltas without copying payloads/protobufs or allocating node/virtual-iterator
wrappers. It requires that database's owned exclusive lock and exclusion of
writes through unlocked handles. There is deliberately no shared-lock overload:
general state-node writes can occur under shared DB locks. Null/foreign/unowned
locks and closed databases throw, rather than return misleading zero inventory.

`mempool::get_retained_memory_usage()` takes its private exclusive DB lock, used
by all its writers, and adds retained node-mutex count. This is on-demand C++
inspection only: no new RPC/CLI/config/periodic metric, no admission/block hot-path
call, no capacity policy or wire/storage change. It blocks private mempool
operations for the scan; avoid frequent or per-request sampling.

Inventory covers indexed writable/finalized nodes, distinct map/non-map backends,
physical map entries and key size/capacity, references versus distinct immutable
value owners and their size/capacity, tombstone key size/capacity and bucket counts.
Backend/owner identity is deduplicated, not equal content. Non-map payloads are
explicitly `unmeasured_backends`; no RocksDB iteration or invented byte estimate.

String capacities include inline storage and exclude terminators. They are **not
allocated heap bytes**, cannot be summed with string object sizes without overlap,
and vary by standard library. Empty bucket counts do not imply allocated buckets.
Excluded: container/index/control-block/header/ID/mutex overhead, allocator
fragmentation/retention, caller-only evicted/replaced values, unindexed anonymous,
execution/admission overlays, RocksDB memory and scan workspace. This inventory
is not total retained heap/RSS or a strict admission ceiling.

Cost is O(entries) time and O(distinct owners/backends) temporary pointer sets;
the workspace retains no value owners after return. Checked counters throw before
overflow. Scan allocation failure unwinds without state mutation. A future
hot-path policy requires justified incremental/reservation accounting, not an
unqualified full scan on every transaction.

## Regression coverage

The new `koinos_retained_memory_test` checks:

- Equal-content independent owners versus shared clones; physical copied keys;
  replacement/erase and normal/preserved/cloned tombstones.
- Unindexed overlays and caller-only replaced owners excluded but still valid;
  pending roots/state results unchanged by inspection and owners valid after close.
- Shared backends counted once; non-map payload unmeasured; invalid locks, closed
  database and overflow rejected.
- Fresh external persistent fixture close/reopen and unchanged stored bytes,
  without pretending persistent/cache payload is measured by map inspection.
- Four mutable mempool states, independent accounts, expiry and LIB; retained
  ancestry despite empty head; indexed ancestry/mutex cleanup on LIB.
- **1,050** one-shot calling-thread C++ `new/new[]` scan failures followed by an
  unfailed scan: identical retained inventory, mutexes and 64 visible records
  after every failure. Excludes dependency malloc/aligned-new, real OOM and
  block/LIB/removal/prune mutation-failure qualification.
- Concurrent inspection/admission/expiry with an explicitly owned/joined reader
  and readiness/failure cleanup. Functional overlap is not integrated TSan proof.

The initial focused test passes 1/1 (3.91 seconds). Final focused suite after
adding concurrency passes **8/8, 18.44 seconds**. Further build/regression evidence
is recorded in the final verification section below.

## Five-process retention characterization

After builds finished, five processes use identical synthetic four-mutable-state
fixtures, 64 independent accounts/transactions, 65,536 signature bytes per record,
fixed nonce/time. They bypass signed chain execution: not mainnet-valid input or
throughput proof. One unmeasured inventory call warms the path, then one scan is
timed. Expiry and synthetic LIB follow in the same process/build.

All five report the same counters:

| Quantity | Before expiry | After expiry | After LIB |
| --- | ---: | ---: | ---: |
| Head-visible records | 64 | 0 | 0 |
| Indexed states / mutexes | 8 / 4 | 8 / 4 | 2 / 1 |
| Map entries / distinct value owners | 1,028 / 1,028 | 516 / 516 | 1 / 1 |
| Distinct value logical bytes | 16,809,400 | 8,404,704 | 2 |
| Distinct value capacity bytes | 16,824,344 | 8,412,216 | 22 |
| Tombstones / logical key bytes | 0 / 0 | 512 / 21,188 | 256 / 10,594 |

An empty head still retains 8,404,704 logical payload bytes in this fixture;
256 tombstones remain after LIB. This characterizes existing lifetime behavior,
not a demonstrated leak, allocator reclamation or new memory saving.

Warmed scan, n=5: median **56 microseconds**, min–max 48–58, median absolute
deviation 1. Exactly **1,050 allocation requests / 50,432 requested C++ bytes**
per scan in every sample; these are temporary scan/control allocations, not
retained heap bytes. Process peak RSS median 35,127,296 bytes, min–max
35,078,144–35,241,984, MAD 49,152, covering the entire synthetic process, not
isolated scan RSS. No before/after implementation speedup or memory reduction is
claimed. Larger-input/Linux allocator and lock costs remain unmeasured.

## Environment and reproduction

Branch `codex/reward-reinvestment-macos`, HEAD
`cfd2eb83b8080ca49911555108ce05f0a9068f01`, dirty native
`1.3.0-dev.0+cfd2eb83b808-dirty`. Mac ARM64/macOS 26.5, Apple Clang 16, 16 GiB RAM,
Release `-O3`, tests ON. CMake caches name this exact source, not another workspace.
All new generated data is in the owned
`/Volumes/My Passport/teleno-lr2-EEMOFW/` tree, APFS/USB, about 350 GiB free at
inspection. Existing dependencies are read inputs; no existing chain/user data
is moved/deleted. The ignored repository evidence link resolves to external data.

Current tool versions: Apple Clang `16.0.0 (clang-1600.0.26.3)`, CMake/CTest
`4.2.3`, cpp-libp2p revision `c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`.
The existing pinned workflow uses Boost 1.83 and RocksDB 8.8.1; dependency/source
configuration remains distinct from Linux feature/resource qualification.

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/build'
export PYTHONDONTWRITEBYTECODE=1
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 4
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
for task_sample in 1 2 3 4 5; do
  "$TELENO_TEST_BUILD_ROOT/koinos_retained_memory_test" --measure-retention \
    >"$TELENO_TEST_ARTIFACT_ROOT/m1d-retention-${task_sample}.log" 2>&1 || exit "$?"
done
```

Focused CTest expression:

```text
^(koinos_retained_memory_test|koinos_mempool_admission_atomicity_test|koinos_mempool_adapter_test|koinos_atomic_write_batch_test|koinos_state_value_lifetime_test|koinos_state_db_pending_root_test|koinos_state_db_durability_test|koinos_cache_invariants_test)$
```

Select the `ubsan/` external build for separate UBSan tests, with
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. It retains the documented
`-fno-sanitize=vptr` exclusion and cached uninstrumented dependencies; no ASan/
TSan equivalence claimed. Select `dependency-workflow-alias-build/` for the
separate dependency-workflow configuration.

Artifacts under external `evidence/`, also via
`build-single-copy-evidence/lr2-completion-qKjj7h/`: `m1d-retention-1.log` through
`m1d-retention-5.log`, focused/build/regression logs and preservation manifest.
Initial compile failures remain in `m1d-focused-build.log` (nested namespace),
`m1d-focused-build-2.log` (test namespace ambiguity) and `m1d-all-build.log`
(this Apple standard library lacks `std::jthread/stop_token`). Explicit
qualification and a portable owned/joined `std::thread` fix these; no concurrency
test, timing gate or semantic expectation was disabled. The first corrected
focused build and final all-target/UBSan builds exit 0.

## Complete checkpoint file list

Runtime/tests (12):

- `src/CMakeLists.txt`
- `src/koinos/mempool/mempool.cpp`, `src/koinos/mempool/mempool.hpp`
- `src/koinos/state_db/backends/map/map_backend.cpp`,
  `src/koinos/state_db/backends/map/map_backend.hpp`
- `src/koinos/state_db/state_db.cpp`, `src/koinos/state_db/state_db.hpp`
- `src/koinos/state_db/state_db_types.hpp`
- `src/koinos/state_db/state_delta.cpp`, `src/koinos/state_db/state_delta.hpp`
- `src/koinos/state_db/retained_memory.hpp`
- `tests/storage/retained_memory_test.cpp`

Documentation/guidance (10): this report, `AGENTS.md`, `CLAUDE.md`, `CHANGELOG.md`,
`docs/README.md`, `docs/logs-and-diagnostics.md`, `docs/release-builds.md`,
`docs/memoria.md`, `docs/implementation/low-resource-mainnet-observer-plan.md`,
`docs/validation/low-resource-lr2-completion-matrix.md`.

Koinos One documentation-only handoff: its two already-dirty
`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
`docs/manual/koinos-one/backup-and-restore.md`. No GUI/app/package code, submodule
pin, staged binary or product version is changed.

## Remaining gates and rollback

Aggregate indexed/fork/metadata/caller/transient admission limits and reservations,
pre-execution local capacity refusal, block/LIB/prune/removal allocator safety,
P2/R1/V1/S1 budgets and functioning integrated ASan/TSan remain required.
Representative whole-node memory/catch-up on an authorized isolated actual
4-decimal-GB/2-vCPU Linux/ext4 machine is unproven. Creating a VM or operating
a live VPS/unrelated environment remains outside current authority. LR5 long
soak is separate. No low-resource defaults or profile are qualified here.

Eligible authenticated/pinned full-history checkpoint/corpus from before
30,504,202 through beyond 32,789,378, roots/successors and identified references
remain missing. Current historical state-delta audit/differential validation
was **not executed**, and no full historical parity is claimed. Container smoke
and functioning ASan/TSan are not replaced by prior reports or local fixtures.

Rollback removes this additive diagnostic API/test/docs while preserving existing
user work; no database migration, deletion or resync is needed. No producer,
live node, source-trust, publication, commit/push/release, LR3/LR4 or GUI/package
integration occurred.

## Final verification and preservation

All results below were executed for this checkpoint, not copied from earlier
reports. The two full suites ran sequentially; no test deadlines/trust/retry or
expectations were weakened. Exact logs remain in external `evidence/`.

| Gate | Actual result | Artifact |
| --- | --- | --- |
| Corrected focused build / initial single test | exit 0; 1/1, 3.91 s | `m1d-focused-build-3.log`, `m1d-focused-tests.log` |
| Final all-target cached build / focused suite | exit 0; 8/8, 18.44 s | `m1d-all-build-2.log`, `m1d-final-focused-tests.log` |
| Cached full CTest | exit 0; 42/42, 156.73 s | `m1d-full-ctest.log` |
| Separate UBSan build / subset | exit 0; 8/8, 25.99 s, vptr/dependency exclusions above | `m1d-ubsan-build.log`, `m1d-ubsan-tests.log` |
| Required dependency workflow rerun | exit 0, same owned external dependency tree, not from empty | `m1d-required-dependency-workflow.log` |
| Separate dependency-tree all-target build / full CTest | exit 0; 42/42, 170.91 s | `m1d-dependency-tree-all-build.log`, `m1d-dependency-tree-ctest.log` |
| Both native `--version` / `--help` checks | four individual commands exit 0 | `m1d-{version,help}.log`, `m1d-dependency-{version,help}.log` |
| Koinos One strict MkDocs | exit 0, 0.47 s; external output `koinos-one-m1d-manual/` | `m1d-koinos-one-mkdocs.log` |
| Source whitespace / AGENTS–CLAUDE identity | both checkouts exit 0; guidance byte-identical | preservation manifest |
| Local Markdown references | 117 checked, no missing files; anchors/remote URLs not checked | preservation manifest |
| Docker precondition | info exit 1, local API socket absent; no daemon/VM started or smoke run | preservation manifest |
| Current ASan/TSan, Linux whole node, historical differential | not executed/qualified; exact missing inputs described above | no pass claimed |

The required workflow used:

```bash
KOINOS_DEPS_ROOT='/Users/pgarcgo/code/teleno/build-single-copy-evidence/lr2-dependencies-EEMOFW' \
HUNTER_ROOT='/Users/pgarcgo/code/teleno/build-single-copy-evidence/lr2-dependencies-EEMOFW/hunter' \
KOINOS_NODE_BUILD_DIR='/Volumes/My Passport/teleno-lr2-EEMOFW/dependency-workflow-alias-build' \
CMAKE_GENERATOR=Ninja MAKE=/usr/bin/make \
LIBSSH_SOURCE_TARBALL='/Volumes/My Passport/teleno-lr2-EEMOFW/dependencies-Uya3ee/downloads/libssh-0.12.0.tar.xz' \
KOINOS_BUILD_TESTS=ON JOBS=4 \
  /usr/bin/arch -arm64 /bin/bash ./scripts/build-cpp-libp2p-koinos.sh
```

The external environment exports above were also set, with
`TELENO_TEST_BUILD_ROOT` selecting `dependency-workflow-alias-build/`. The
space-free ignored dependency alias resolves to the same owned My Passport
directory; `MAKE` is command-local, not a host policy change. Prior initial
space-path failures are retained in S1/M1-C reports, not repeated or reclassified
as passes. Final cache/source/test-ON checks still match this repository.

Both binaries report `1.3.0-dev.0+cfd2eb83b808-dirty`. SHA-256:

- Cached native: `0756811006d85807398459c45e0bb35e134111779019c2eaa0e11d48af2fc01d`.
- Separate dependency native: `b156524f8fa4ca8a06586e927b51924d346fafd510a0eda2de634602ac862be3`.
- Cached retention test: `a547ca3baca76710e410b2ec68652587e249122c17c42093c4f4ffeb3d6b5a59`.

These supersede earlier checkpoint hashes for this dirty source, not a committed
submodule pin or packaged release. Koinos One integration/release/deployment and
an eligible new independently verifiable backup require separate authorization.

`m1d-source-preservation.json` records the complete 78-file cumulative goal
inventory, the 22-file current checkpoint, current dirty hashes, commands/gates,
measurements and the two documentation-only Koinos One hashes. Current worktree
has 144 dirty/untracked files: 19 pre-existing files extended, three added, 122
other pre-existing files unchanged, none removed. Protected README SHA-256
`b05852f8e80a0e51cba6a47e1f36d26fd665d5246315d9dc8d36b99eee43c419` and high-throughput
plan `4ca6127c5ea756250317436c1e4d2a9423825dda442256d50a9dc9acbc1e7a8c` stay unchanged.
No staged/committed/pushed changes or deletion of existing data is performed.

Scoped on-demand inventory/functional checkpoint exit: **satisfied locally**.
Aggregate capacity, lifetime, historical and Linux whole-node exits: **not satisfied**.

**Every applicable LR2 Definition of Done item proven: NO. Goal remains active.**
