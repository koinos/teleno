# LR2 M1-E: pre-execution standalone transaction-work reservations

Date: 2026-10-03. Status: **local implementation; M1/LR2 NOT complete**.

Primary optimization/qualification target: Ubuntu/x86_64/ext4, 2 vCPUs and
4 decimal GB physical RAM. Mac/ARM64/APFS tests below are development diagnostics,
not VPS qualification. No new low-resource defaults are selected from them.

## Boundary and implementation

The controller previously copied a submitted transaction, created an execution
context/anonymous state, performed mempool/resource checks and executed it before
native insertion. Failure-atomic insertion alone cannot bound simultaneous work
at that earlier boundary. Per-account pending limits also do not bound execution
across many independent senders or dry-run requests.

Native `TransactionWorkBudget` adds nonblocking reservation of a job and the
protobuf input's `ByteSizeLong()` at entry to `submit_transaction`, before even
structural validation or the controller's copy. The lease is the first local
variable, so execution context, temporary receipt/notification and state unwind
before credit releases. It remains held during native insertion and observer
notification, and releases on ordinary, validity or allocation failure. Return
receipts held by callers are outside this lifetime.

`chain.transaction-work-max-jobs` and `chain.transaction-work-max-wire-bytes`
each default to zero/unlimited. Both zero disable tracking without allocating
a lease or sizing inputs. Explicit values are strictly unsigned decimal;
invalid values fail normal configuration loading rather than silently selecting
unlimited. Each positive axis is checked atomically under one mutex. Counter
overflow refuses without wrapping, even for an unlimited axis. Lease allocation
precedes accounting; it cannot leak credit if allocation fails. Leases are
move-only owners and can safely outlive the budget itself. No queue, automatic
retry, deadline or forced execution cancellation is introduced.

Dry runs, JSON-RPC/gRPC/P2P standalone submissions and disabled-mempool wiring
share this controller boundary. Canonical block validation, transaction execution
inside blocks and replay/fallback do not use it. The policy never alters protocol
resource limits, validity, metering, ordering, roots, receipts, head/LIB behavior,
history, durability or observer-first recovery. Refused standalone work performs
no insertion/accepted-work notification and cannot advance the canonical head.

The typed `LocalCapacityExceeded` is a local retryable refusal, not a Koinos
consensus/transaction failure. Its static message itself needs no C++ allocation;
RPC response serialization still allocates outside this budget. Responses:

- JSON-RPC: `-32005`, unchanged request ID, no successful result, and error data
  `{retryable: true, reason: "transaction_work_capacity", scope: "local"}`.
- gRPC: `RESOURCE_EXHAUSTED`, not `INVALID_ARGUMENT` or generic internal error.
- P2P: no misconduct score, disconnect, accepted-ID cache entry or hidden retry
  for this refusal. A later delivery can undergo ordinary validation. Existing
  exact-pending duplicate handling and invalid-input penalties are preserved.

## Accounting exclusions and tradeoffs

This is a strict configured **concurrent job/input-wire bound**, not physical
memory or retained mempool capacity. Deserialized ingress already exists before
the gate; protobuf/string capacity, copies, receipts/serialization/output queues,
VM instances, execution overlays, canonical block work and retained fork/index/
nonce/RC metadata remain additional. There is no claim that the configured wire
bytes are allocated heap bytes or that these limits alone meet 4-GB operation.
M1-D's on-demand fork scan is not called on this hot path.

Enabled calls take short mutex sections, compute input wire size and allocate one
lease; caller execution occurs outside the mutex. Disabling the policy preserves
the allocation-free/no-size-computation reservation path. A finite work budget
can reduce simultaneous standalone work but cause local retry responses and lower
concurrent admission throughput. Actual CPU/latency/whole-node memory benefit and
appropriate VPS values require comparable Linux workload measurements. No
percentage improvement is inferred from passing fixtures or fewer active jobs.

O(1) native metrics expose enablement, current/peak jobs/input wire bytes,
cumulative refusals and both limits. Disabled zero metrics mean **untracked**,
not that no requests are executing. They are not summed with RSS.

## Regression coverage

- New reservation test: disabled no-allocation path; independent zero/unlimited
  axes; exact combined count/byte boundary; oversized input; uint64 overflow;
  one-shot calling-thread C++ lease-allocation failure with unchanged counters;
  move/outlive-budget/no-allocation release; 2,000 distinct synthetic accounts;
  4,000 concurrent attempts in eight joined workers and final zero credits.
  It does not exercise dependency malloc, aligned allocation, machine OOM or
  signed mainnet transaction throughput.
- Controller: refusal precedes validation even on an unopened controller; invalid
  input releases credits; a deterministically signed no-operation genesis
  transaction produces byte-identical receipts with/without the gate; insertion
  failure permits readmission; success/duplicate/post-insertion observer failure
  keep accounting and notification order; disabled-mempool notification-only
  behavior; full checked replay fallback succeeds while work is saturated.
  The fixture uses deterministic test keys, not producer keys or a public node.
- Configuration: defaults, positive axes and sixteen malformed value/key cases.
- JSON-RPC loopback: overload code/ID/local-retry data/no result and later success
  on the same keepalive connection; prior lifecycle regressions retained.
- gRPC loopback: capacity status then later success; existing service/validity
  status tests retained.
- Fake P2P: repeated local refusal is neither cached nor retried/disconnected,
  then normal acceptance/deduplication after recovery. Actual transport working
  sets/callback ownership and supported race tooling remain separate gates.

## Current environment and commands

Branch `codex/reward-reinvestment-macos`, HEAD
`cfd2eb83b8080ca49911555108ce05f0a9068f01`, dirty development
`1.3.0-dev.0+cfd2eb83b808-dirty`. Apple Clang 16, macOS 26.5 (25F71), ARM64,
16 GiB RAM, CMake/CTest 4.2.3, Release `-O3`, tests/cpp-libp2p ON. Existing pinned
Boost 1.83/RocksDB 8.8.1 and cpp-libp2p
`c03bf7c44c4fd30a390f3ef93bd1eab482e686a6` are read inputs. Both build caches name
this source repository. Owned external root:
`/Volumes/My Passport/teleno-lr2-EEMOFW/`, initially about 347 GiB free, APFS/USB.
All new generated data stays there; no existing node/database or dependency data
is relocated, deleted or operated.

All producing commands use the following environment, with `build` replaced by
`ubsan` or `dependency-workflow-alias-build` for that specific configuration:

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/build'
export PYTHONDONTWRITEBYTECODE=1
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 4
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
```

Focused selector:
`^(koinos_transaction_work_budget_test|koinos_config_test|koinos_controller_delta_test|koinos_p2p_one_peer_sync_test|koinos_jsonrpc_session_lifecycle_test|koinos_grpc_server_test)$`.
UBSan also includes existing admission-atomicity, state-value-lifetime and retained
memory tests, with `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. The separate
build uses `-fsanitize=undefined -fno-sanitize=vptr -fno-omit-frame-pointer`;
dependencies are uninstrumented and vptr is explicitly excluded. This is not
ASan/TSan or whole-program lifetime/race proof.

## Execution log and evidence

Evidence prefix: the owned `evidence/` directory above, exposed by ignored
`build-single-copy-evidence/lr2-completion-qKjj7h/`.

- Initial focused build exits 1: duplicate test-local `attempts` identifier.
  Renamed it without changing test expectations; original log is retained.
- Corrected focused build exits 0; focused CTest **6/6, 27.52 seconds**, exit 0.
- All-target, broader CTest, separate UBSan, required dependency workflow,
  current binary identities and final documentation checks are recorded below
  only after execution. These gates are not inferred from M1-D's older results.

Artifacts: `m1e-worktree-start.json` (144 pre-existing file hashes),
`m1e-focused-build.log`, `m1e-focused-build-2.log`, `m1e-focused-tests.log`.

### Five-process functional resource characterization

Five identical after-only processes ran the new reservation regression binary
with `/usr/bin/time -l`, after all builds and the first full/UBSan suites ended.
Each processes 2,000 distinct synthetic inputs (2,104,890 tracked wire bytes in
total) and 4,000 concurrent attempts. Every run's concurrent subcase peaks at
three leases/30 input bytes under the test's deliberately tiny limits, and ends
at zero jobs/bytes. Refusals vary with scheduling (3,256 / 3,223 / 3,164 / 3,268 /
3,327); they are not invalid-transaction counts or a throughput benchmark.

Whole regression-process peak RSS, n=5, median **8,536,064 bytes**, min–max
8,519,680–8,536,064, MAD 0 at the reported sample granularity. This includes
startup and the fixture driver, not an isolated allocation delta or steady-state
`teleno_node`. Inputs bypass chain execution in this binary; the signed no-op
controller test is separate. There is no paired old/new baseline, Linux/RAM
qualification or measured optimization gain here. Sub-second process timing is
startup/scheduling/resolution sensitive and is not used for latency/CPU claims.
Artifacts: `m1e-work-{1..5}.log`. Reproduction:

```bash
/usr/bin/time -l "$TELENO_TEST_BUILD_ROOT/koinos_transaction_work_budget_test"
```

### Final current-source verification

| Command / configuration | Actual result | Artifact |
| --- | --- | --- |
| Cached all-target rebuild, final header | Exit 0 | `m1e-all-build-final.log` |
| Cached full CTest | **43/43**, exit 0, 169.04 s | `m1e-full-ctest.log` |
| Separate UBSan target rebuild / subset | Exit 0 / **9/9**, exit 0, 38.51 s | `m1e-ubsan-build-final.log`, `m1e-ubsan-tests.log` |
| Required test-enabled dependency script | Exit 0, reused owned pinned dependencies | `m1e-required-dependency-workflow.log` |
| Separate dependency all-target build | Exit 0 | `m1e-dependency-all-build.log` |
| Separate dependency full CTest | **43/43**, exit 0, 167.23 s | `m1e-dependency-ctest.log` |
| Both native binaries, `--version` and `--help` | Four separate commands exit 0 | `m1e-{build,dependency-workflow-alias-build}-{version,help}.log` |
| Koinos One strict MkDocs, external output | Exit 0, 0.35 s | `m1e-koinos-one-mkdocs.log` |
| Local Markdown file targets | 137 checked, no missing files; anchors/remote URLs excluded | `m1e-doc-links.log` |
| Docker smoke prerequisite | Exit 1, local daemon socket absent; no smoke run or VM start | `m1e-container-precondition.log` |

Both full suites and UBSan were run serially after builds. Functioning current
ASan/TSan and Linux/historical checks
are **not run/proven**, not replaced by UBSan or the Mac suites. Existing Mac
ASan initialization failure/TSan exit without attribution remain documented in
earlier LR2 reports; repeating them does not qualify integrated lifetimes.

Required dependency command (same TMPDIR/artifact/no-bytecode settings above):

```bash
TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/dependency-workflow-alias-build' \
KOINOS_DEPS_ROOT='/Users/pgarcgo/code/teleno/build-single-copy-evidence/lr2-dependencies-EEMOFW' \
HUNTER_ROOT='/Users/pgarcgo/code/teleno/build-single-copy-evidence/lr2-dependencies-EEMOFW/hunter' \
KOINOS_NODE_BUILD_DIR='/Volumes/My Passport/teleno-lr2-EEMOFW/dependency-workflow-alias-build' \
CMAKE_GENERATOR=Ninja MAKE=/usr/bin/make \
LIBSSH_SOURCE_TARBALL='/Volumes/My Passport/teleno-lr2-EEMOFW/dependencies-Uya3ee/downloads/libssh-0.12.0.tar.xz' \
KOINOS_BUILD_TESTS=ON JOBS=2 \
/usr/bin/arch -arm64 /bin/bash ./scripts/build-cpp-libp2p-koinos.sh
```

The space-free dependency alias resolves to the existing owned external tree;
no internal-disk relocation or clean-from-empty build is claimed. Earlier all-
target/UBSan build logs are retained alongside final rebuilds after removing
allocation from the local exception message. No deadline, assertion or test
expectation was relaxed. Initial hash inspection used an incorrect `src/` binary
path; the correct build-root test executable was subsequently hashed below.

Native identity (both builds): `1.3.0-dev.0+cfd2eb83b808-dirty`, ARM64. SHA-256:

- Cached `teleno_node`:
  `86260b8e67421263547c594db2521473a23f51ff8d719afdc4d6c5eb5e2b7a27`.
- Separate dependency `teleno_node`:
  `176d0cd9d3d194fecd9284703e6262481908569fcabe750a46df1af10d6d3e34`.
- Cached reservation test:
  `4a3c224f9ec411276276a8428fe15134159ff254141265fb8b5d5d73735c8173`.

These identify local dirty source, not a release/integration pin. Preserve them
with the exact source manifest; deliberate commit/pin/build/package integration
requires separate authorization.

## Files and pending gates

Runtime/test files in this checkpoint:

- `src/core/transaction_work_budget.hpp` (new), `src/core/monolith_client.hpp`,
  `src/core/monolith_rpc_client.hpp`, `src/core/config.hpp`, `src/core/config.cpp`,
  `src/koinos/chain/controller.cpp`, `src/main.cpp`.
- `src/jsonrpc/jsonrpc_server.cpp`, `src/grpc_server/grpc_server.cpp`,
  `src/p2p/p2p_node.cpp`, `src/CMakeLists.txt`.
- `tests/core/transaction_work_budget_test.cpp` (new), `tests/core/config_test.cpp`,
  `tests/chain/controller_delta_test.cpp`, `tests/p2p/p2p_one_peer_sync_test.cpp`,
  `tests/jsonrpc/session_lifecycle_test.cpp`, `tests/grpc/grpc_server_test.cpp`.

Documentation files in this checkpoint:

- `AGENTS.md`, `CLAUDE.md`, `CHANGELOG.md`, `docs/README.md`.
- `docs/configuration.md`, `docs/rpc-endpoints.md`, `docs/logs-and-diagnostics.md`,
  `docs/release-builds.md`, `docs/memoria.md`.
- `docs/implementation/low-resource-mainnet-observer-plan.md`,
  `docs/validation/low-resource-lr2-completion-matrix.md`,
  `docs/validation/low-resource-lr2-transaction-work.md` (new).

Total **29 Teleno checkpoint files**. Koinos One changes only
`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
`docs/manual/koinos-one/backup-and-restore.md`. Native options are not claimed
GUI-exposed or packaged; no submodule, binary, config generator, app/version
or release changes are made.

The start inventory has 144 dirty/untracked files. SHA-256 comparison preserves
123 other pre-existing files unchanged, extends 21 in scope, and adds five
previously clean tracked files plus three new files; no files are removed.
README/high-throughput plan hashes remain respectively
`b05852f8e80a0e51cba6a47e1f36d26fd665d5246315d9dc8d36b99eee43c419` /
`4ca6127c5ea756250317436c1e4d2a9423825dda442256d50a9dc9acbc1e7a8c`.
`m1e-source-preservation.json` records the exact 29-file checkpoint, cumulative
goal inventory and full 152-file dirty-source hashes. Both `git diff --check`
and guidance byte-identity checks pass; nothing is staged, committed or pushed.

Remaining: aggregate retained-fork/metadata/transient/consumer policies and Linux
profile values; block/LIB/prune/removal allocator safety; supported integrated
ASan/TSan; eligible independently authenticated/pinned full-history backup,
pre-30,504,202 checkpoint and successor/reference corpus through beyond
32,789,378; current historical delta audit/differential; isolated Ubuntu/x86_64/
ext4 whole-machine 2-vCPU/4-decimal-GB environment. No historical or differential
run was performed here. No VM creation or live-server operation is authorized.
Container smoke requires a functioning isolated daemon; it is not assumed.
LR5's long-server soak remains separate. No pruning, source-trust/restore/reserve
change, producer activation, commit/push/tag/release/deployment/publication.

Rollback is removal/zeroing of the opt-in native work settings in a reviewed
configuration after stopping its owner; defaults and persistent formats are
unchanged. No stored history needs conversion or deletion.

**Was every applicable LR2 Definition of Done item proven? NO.**
