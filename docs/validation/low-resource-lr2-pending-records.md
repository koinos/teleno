# LR2 M1-G: shared pending records and lifetime-owned reservations

Date: 2026-10-03. Status: **local implementation; M1/LR2 NOT complete**.

The primary target is Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB physical RAM.
Mac/ARM64/APFS results below are development diagnostics and portable-behavior
evidence, not Linux resource qualification or a source of VPS defaults. Full
genesis history, receipts, roots, protocol accounting and durability remain
required. No live node/VPS, VM creation, producer, trust/reserve, LR3/LR4, git
publication or GUI/package integration is authorized by this checkpoint.

## Measured problem and implementation contract

The later [protobuf copy-ownership correction](mempool-admission-copy-ownership.md)
owns the temporary admission record in an arena after reproducing an
allocation-failure leak in the pinned dependency. Its candidate tests and
remaining qualification gates are separate from the original results below.

The preserved M1-F implementation serialized and separately stored the same
pending record in every eligible fork. The five before processes below confirm
four payload owners per admission in the fixed four-mutable-state fixture.
Expiry empties the current heads but does not remove the older records; charging
only visible-head entries would release credits before actual ownership ended.

M1-G encodes a record once per admission and shares immutable ownership across
eligible forks. Equal-content independent admissions are not globally deduped.
The new factory contains a genuinely const string, without a mutable alias.
State map clones, ordinary squash and in-memory LIB publication preserve that
owner. Additive owned insertion requires an immutable caller-owned value; existing
ordinary insertion still copies. Persistent backends use their existing copying
write/batch path. Serialized bytes, key order, byte-accounting, roots, normal
remove/tombstones and persisted format/durability are unchanged.

Two optional `mempool` settings independently limit live unique record owners
and charged string capacity. Both zero/unlimited disables accounting, not sharing:

```yaml
mempool:
  pending-record-max-owners: 0
  pending-record-max-capacity-bytes: 0
```

Strict unsigned values apply outside preflight too. No qualified 4-GB values
are chosen. These are local policy limits, not changed Koinos resource limits.

Before encoding, an owned lease reserves one owner and the wire-size lower
bound. Allocation precedes committing counters. After encoding, it charges
actual `std::string::capacity()` before fork publication. Each stage checks
overflow/remaining capacity under a mutex; no waiting queue or per-admission
all-fork accounting scan is added. Failure releases the lease. A completed
record carries it through ancestor, clone, map-LIB and external caller lifetime;
the last owner frees the string before returning credits. Accounting state can
outlive the budget wrapper safely. Peak/refusal counters are process-lifetime;
refusals saturate rather than wrap. O(1) snapshots are exposed in native metrics.

The existing M1-C all-fork preflight/zero-new publication remains. No ancestor
is pruned early, accepted work evicted, transaction silently dropped or canonical
block/replay gated. Cleanup does not acquire new record credits and can run at
this ceiling, although its separate workspace is not yet globally budgeted.
Head expiry can legitimately leave this budget full until normal LIB/last-owner
release. A caller must use its own bounded retry policy after local refusal.

Local exhaustion preserves JSON-RPC `-32005`, original request ID and
`{retryable: true, reason: "pending_record_capacity", scope: "local"}`; gRPC uses
`RESOURCE_EXHAUSTED`. P2P applies no misconduct score, cache entry or automatic
retry. The prior work-budget reason remains `transaction_work_capacity`.
Rejected native admission emits no accepted observer notification or receipt.
Ordinary invalidity retains its usual behavior when local capacity permits work.

### Exact accounting exclusions and tradeoffs

This is one aggregate payload axis, **not complete M1 or a hard process-memory
bound**. Map keys/entries, transaction/nonce/RC indexes, tombstones, containers,
control blocks, parsed input/receipt objects, VM, transport and RPC output are
additional. String capacities include inline storage; they are not allocator
or RSS measurements. While encoding, counters reserve only the wire lower bound;
the temporary buffer can grow beyond it before growth is checked/refused. General
encoding and cleanup headroom remain required, including emergency cleanup.

Enabled accounting adds a lease/control block and mutex operations per unique
record; tiny single-fork payloads need Linux overhead measurements. Disabled
accounting has no accounting-state/lease allocation; shared payload creation
still allocates. Ordinary in-memory squash avoids a value copy but still needs
map/iterator work. Persistent writes still copy and preserve batching/durability.
No disk saving or CPU/mainnet throughput gain is claimed from this checkpoint.

## Focused regression evidence

- Factory independent count/capacity axes, exact boundary, disabled allocation
  behavior, equal bytes/distinct owners, alias release, final-owner recovery,
  capacity-growth refusal, overflow and size mismatch, wrapper-outlived value.
- Three injected factory allocation failures each unwind to zero credits and
  allow retry. Eight threads run 4,000 factory attempts with peak <=4 owners and
  <=1,024 charged bytes, final zero. This is functional concurrency, not TSan.
- Shared insertion matches ordinary byte accounting/fresh pending roots;
  ordinary squash and map LIB retain the charged owner. Close leaves caller-only
  ownership charged/valid; last release returns it. Persistent copy, sync commit
  and two opens reproduce exact bytes/root in a fresh external scratch DB.
- Four-fork multi-account capacity, empty-head ancestor retention, cleanup at
  capacity, normal LIB reclamation and explicit readmission; a surviving active
  record remains charged through LIB. Fork-specific nonce exclusion and duplicate
  refusal preserve credits. Separate native capacity-byte and concurrent
  admission checks pass: actual 8,303-byte capacity boundary on this Mac build,
  and eight native threads/80 independent candidates yield exactly four accepts
  and 76 local refusals, peak four owners and four records on every checkpoint.
  The 8,303 bytes are a fixture-derived boundary, not a portable VPS setting.
- Enabled four-fork admission sweeps ordinary calling-thread `new/new[]`:
  **335 injected unchanged rejections**, each successful explicit retry; seven
  successful post-publication logging faults and the noninjected end case.
  Existing work stays charged and every published candidate appears on all four
  checkpoints. This is not dependency malloc/aligned/race/hardware-OOM proof.
- Actual signed controller/native admission preserves the original receipt and
  state root, observes record refusal before accepted notification, releases
  work credits and permits explicit retry after last-record removal. Canonical
  checked replay/full fallback succeeds with both record and work budgets full.
- Loopback JSON-RPC/gRPC and fake-transport P2P exercise both local refusal reasons,
  recovery, no accepted cache/misconduct and no hidden retry. No external peer
  probing or public transaction submission is performed.

Initial mistakes are retained, not erased: the first focused build used the
nonexistent target `koinos_node_config_test` and exited 1. The corrected build
passes. The first seven-test run exits 8 (6/7, 31.86s): a new competing-root
fixture supplied a hashed-zero parent rather than the actual genesis root ID.
Correcting that fixture makes the pending-record test pass (1/1, 2.97s).
No runtime validation or expectation was weakened. Final current-source results
are recorded below only after completion, not inferred from the earlier suite.

## Five-process before/after characterization

Same compiler/dependencies/Release settings, fixed seed/time/nonce/accounts,
64 records with 65,536 signature bytes each, four mutable states/eight indexed
states. Setup precedes the warmed inventory measurement; processes run serially
with no competing build or tests. Five separate processes per implementation.
The preserved before binary is not overwritten by the changed build.

| Quantity | Before median (min–max; MAD) | After median (min–max; MAD) |
| --- | --- | --- |
| Unique live indexed values before expiry | 1,028 (constant) | 836 (constant) |
| Unique logical payload bytes before expiry | 16,809,400 (constant) | 4,205,044 (constant) |
| Unique string-capacity bytes before expiry | 16,824,344 (constant) | 4,218,824 (constant) |
| Unique logical payload bytes after head expiry | 8,404,704 (constant) | 4,203,252 (constant) |
| Unique string-capacity bytes after head expiry | 8,412,216 (constant) | 4,210,376 (constant) |
| Inventory C++ allocation attempts | 1,050 (constant) | 858 (constant) |
| Inventory requested C++ bytes | 50,432 (constant) | 45,824 (constant) |
| Warm inventory elapsed µs | 53 (47–107; 6) | 42 (41–46; 1) |
| Entire fixture process peak RSS bytes | 34,930,688 (34,881,536–35,127,296; 49,152) | 18,972,672 (18,939,904–19,103,744; 32,768) |

Before expiry, 192 duplicated record owners disappear, but all 1,028 map entries
and their key/nonce/resource behavior remain. Both implementations still have
older records after head expiry. Both end with the same LIB inventory: two
indexed states, one 2-byte metadata owner/22-byte capacity, 256 tombstones.
This proves less unique payload retention in this fixture, not less data or
earlier ancestor pruning. RSS covers the entire isolated fixture process, not
a full node or Linux. Scan allocation/workspace savings reflect fewer deduped
owners; timing differences are short Mac samples, not VPS policy evidence.
Requested bytes are allocation-request sums, not simultaneously retained heap.

## Environment, reproducible commands and artifacts

Source `/Users/pgarcgo/code/teleno`, branch `codex/reward-reinvestment-macos`, HEAD
`cfd2eb83b8080ca49911555108ce05f0a9068f01`; dirty `1.3.0-dev.0` development.
Apple Clang 16/macOS26.5/ARM64/16GiB; CMake/CTest4.2.3, Release `-O3`, tests and
cpp-libp2p ON, Boost1.83/RocksDB8.8.1, cpp-libp2p
`c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`. All three CMake caches name this
exact source repository, not another workspace. APFS/USB had about 342 GiB free.
Existing pinned dependencies are inputs; this is not a clean-from-empty rebuild.

All generated data is under owned `/Volumes/My Passport/teleno-lr2-EEMOFW/`:
`build/`, `ubsan/`, `dependency-workflow-alias-build/`, `tmp/`, `evidence/` and
`koinos-one-m1g-manual-site/`. Existing chain/user data is not relocated/deleted.
Ignored repo alias `build-single-copy-evidence/lr2-completion-qKjj7h/` points to
external evidence; `lr2-dependencies-EEMOFW/` points to owned external dependencies.

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/build'
export PYTHONDONTWRITEBYTECODE=1
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 2
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
"$TELENO_TEST_BUILD_ROOT/koinos_mempool_pending_record_budget_test"
for task_sample in 1 2 3 4 5; do
  "$TELENO_TEST_BUILD_ROOT/koinos_retained_memory_test" --measure-retention \
    >"$TELENO_TEST_ARTIFACT_ROOT/m1g-after-retention-${task_sample}.log" 2>&1 \
    || exit "$?"
done
```

Required dependency workflow uses command-local `KOINOS_BUILD_TESTS=ON`, `JOBS=2`,
`CMAKE_GENERATOR=Ninja`, `MAKE=/usr/bin/make`, `KOINOS_DEPS_ROOT` and `HUNTER_ROOT`
under the owned ignored dependency alias, plus
`KOINOS_NODE_BUILD_DIR`/test build root set to the external separate tree and
`LIBSSH_SOURCE_TARBALL` set to the existing owned
`dependencies-Uya3ee/downloads/libssh-0.12.0.tar.xz`. It runs
`/usr/bin/arch -arm64 /bin/bash ./scripts/build-cpp-libp2p-koinos.sh`, then a
separate all-target `cmake --build`, CTest and both CLI checks. The selected
shell architecture resolves the Mac build environment; it is not Linux proof.

Separate UBSan tree uses `-fsanitize=undefined -fno-sanitize=vptr
-fno-omit-frame-pointer`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` and
the explicit external test/build/tmp roots. Dependencies are uninstrumented;
no vptr/ASan/TSan coverage is inferred. Functional concurrency does not replace
integrated lifetime/race instrumentation.

Evidence prefix `m1g-*`: initial 154-file `worktree-start.json`, preserved
`before-retained-memory-test`, five `before-retention-*`/`after-retention-*` logs,
`comparison.json`, factory/native `record-proof.log`, failed `focused-build.log`
and `focused-tests.log`, corrected build/test logs, required dependency/all-target
and UBSan logs. `m1g-comparison.json` includes all ten raw samples and median/
min/max/MAD calculations. Scratch DB paths printed by tests are external owned
children; fixtures never open existing node data.

Before binary SHA-256:
`f0f8232f5983f3ce143c4e3000f5659328cba133bced8819aa52fafe52be3680`.
Current identities, hashes, final command results and source-preservation
inventory are recorded in the final gate section after execution.

## Changed-file inventory

Checkpoint implementation/test files (new files marked explicitly):

- `src/CMakeLists.txt`
- `src/core/config.cpp`, `src/core/config.hpp`
- `src/core/local_capacity.hpp` (new), `src/core/transaction_work_budget.hpp`
- `src/jsonrpc/jsonrpc_server.cpp`
- `src/koinos/mempool/mempool.cpp`, `src/koinos/mempool/mempool.hpp`
- `src/koinos/mempool/pending_record_budget.hpp` (new)
- `src/koinos/state_db/backends/backend.cpp`, `src/koinos/state_db/backends/backend.hpp`
- `src/koinos/state_db/backends/map/map_backend.cpp`, `src/koinos/state_db/backends/map/map_backend.hpp`
- `src/koinos/state_db/state_db.cpp`, `src/koinos/state_db/state_db.hpp`
- `src/koinos/state_db/state_delta.cpp`, `src/koinos/state_db/state_delta.hpp`
- `src/main.cpp`
- `tests/chain/controller_delta_test.cpp`, `tests/core/config_test.cpp`
- `tests/grpc/grpc_server_test.cpp`, `tests/jsonrpc/session_lifecycle_test.cpp`
- `tests/mempool/pending_record_budget_test.cpp` (new)
- `tests/p2p/p2p_one_peer_sync_test.cpp`, `tests/storage/retained_memory_test.cpp`

Documentation/guidance:

- `AGENTS.md`, `CLAUDE.md`, `CHANGELOG.md`, `docs/README.md`
- `docs/configuration.md`, `docs/logs-and-diagnostics.md`, `docs/rpc-endpoints.md`
- `docs/running-observer-node.md`, `docs/release-builds.md`, `docs/memoria.md`
- `docs/implementation/low-resource-mainnet-observer-plan.md`
- `docs/implementation/teleno-audit-remediation-plan.md`
- `docs/performance/teleno-software-audit.md`
- `docs/validation/low-resource-lr2-completion-matrix.md`
- `docs/validation/low-resource-lr2-pending-records.md` (new, this report)

Documentation-only Koinos One handoff:

- `docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md`
- `docs/manual/koinos-one/backup-and-restore.md`

## Final local gates and preservation

| Current command / configuration | Actual result |
| --- | --- |
| Cached all-target build, external selected tree | Exit 0 |
| Required test-enabled dependency script, external owned dependency/node roots | Exit 0 |
| Separate dependency all-target build | Exit 0 |
| Serialized cached full CTest | **45/45, 179.20s, exit 0** |
| Serialized separate dependency full CTest | **45/45, 181.78s, exit 0** |
| Separate supported UBSan build/subset | **11/11, 64.63s, exit 0** |
| Cached and separate `teleno_node --version`, `--help` | All four exit 0 |
| Koinos One strict MkDocs, external site | Exit 0, 0.78s |
| Docker info smoke prerequisite | Exit 1, local daemon socket absent; smoke unexecuted |

Both native binaries identify `1.3.0-dev.0+cfd2eb83b808-dirty`:

- Cached native: `219fbfdc352db853d62c78d4caa5f0b754aa0a50723ea0df7f24503d37b14aab`.
- Separate dependency native: `c3297b0acabb41e0fc197409bec10f6752403fd054d2b77bd8eedde4a177dcf2`.
- Current pending-record test: `83f44a36691ef012fd625aaa12642ca1b93974457bf8e7875016720542c3ff4c`.
- Current retention test: `1f682aaae0214a54b8db1730790e84b45a1c3f3ce329df1ae90fac357aaaa27d`.

Logs `m1g-full-ctest.log`, `m1g-dependency-ctest.log`, `m1g-ubsan-tests.log`
and their `*-ctest-details.log` artifacts preserve actual output, fault counts,
native concurrent proof and external fixture paths. Detailed build/version/help/
container logs retain warnings and unsupported checks. Earlier 44-test/M1-F
results are historical, not proof of this changed source.

The checkpoint changes 40 Teleno files listed above and two Koinos One docs.
Initial 154 dirty/untracked files are hashed; 119 other pre-existing files remain
unchanged, 35 are deliberately extended, one previously clean tracked backend
file is modified and four new files are added. No files are removed; protected
README/high-throughput hashes are unchanged. Cumulative goal inventory is 98
Teleno files, separate from the complete 159-file dirty worktree. Exact hashes/
classification are `m1g-source-preservation.json`; other milestones/user edits
are not presented as newly implemented M1-G work.

Final `git diff --check` in both repositories, identical AGENTS/CLAUDE and local
link/whitespace checks pass; 15 Teleno docs/182 local file references at the first
final check. Later evidence-section links are rechecked in `m1g-final-checks.json`.
Only the two declared Koinos One docs are dirty there; both git indexes remain
unchanged/unstaged. File-link checks are not remote publication/anchor checks.

## Remaining requirements and exit

Complete metadata/fork/caller/workspace/cleanup-headroom budgets, block/LIB
allocation-failure behavior, transport/RPC/VM/state policies, functioning
integrated ASan/TSan, eligible pinned full-history/successor/reference inputs,
and isolated Ubuntu/x86_64/ext4 whole-machine memory/catch-up/restart validation
remain required. Historical state-delta audit/differential range has **not**
been executed for this checkout: local scar/fallback fixtures are not historical
parity. No current Linux RSS, host MemAvailable or faster-than-arrival proof is
claimed. LR5's long full-server soak remains separate, not waived or absorbed.

Koinos One handoff extends only its two already-dirty backup/operator docs,
distinguishing native development from GUI/package capabilities. Strict MkDocs
passes, external output, 0.78s; no GUI code/settings/config generator, package,
staged binary, submodule pin, app version/changelog or release changes. There
is no committed integration pin: dirty HEAD is not a releasable native revision.

**Every applicable M1/LR2 Definition of Done item proven: NO.**
