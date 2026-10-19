# LR2 V1-B: local VM allocation failure

Date: 2026-10-04 (Europe/Berlin).
Status: **local checkpoint and repository build/regression gates verified;
V1/LR2 NOT qualified**.

Linux Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB physical RAM remains the
primary optimization and qualification target. This portable correctness fix
is diagnosed on Mac; it selects no Linux memory/admission defaults and does not
prove whole-node memory, catch-up, durability, recovery or historical parity.

## Scope and reproducible inputs

Repository `/Users/pgarcgo/code/teleno`, branch `codex/reward-reinvestment-macos`,
HEAD `cfd2eb83b8080ca49911555108ce05f0a9068f01`, dirty `1.3.0-dev.0`.
Mac 26.5 / Apple Clang 16 / arm64 / 16 GiB / 10 CPUs / CMake 4.2.3,
Release `-O3`; Boost 1.83, RocksDB 8.8.1 and cpp-libp2p revision unchanged.
The intended generated build/source/fixture/temporary/evidence placement is
`/Volumes/My Passport/teleno-lr2-EEMOFW/` with explicit
`TMPDIR`, `TELENO_TEST_ARTIFACT_ROOT`, `TELENO_TEST_BUILD_ROOT` and
`PYTHONDONTWRITEBYTECODE=1`. Volume absence stops producing work. Existing node
data, installed libraries and other repositories' runtime code are untouched.

Initial 162-file hashes in `evidence/v1b-worktree-start.json` match final V1-A;
pre-existing README/high-throughput plan are preserved. Evidence uses `v1b-*`.
No live server/VM, deployment, source-trust change, history/durability reduction,
producer operation, commit/push/release or GUI/submodule/package integration.

One repeat violated the placement contract: its command-local `env` applied to
the CLI collector but not the following CTest command. CTest used default
internal scratch locations. The run was stopped (exit 143), not counted as a
pass. Seventeen fresh test directories were identified by exact names, birth
interval and inodes, then relocated to the owned external
`evidence/v1b-interrupted-env-artifacts-VH72qy/` and file/hash verified:
339,090,468 logical bytes. No pre-existing directory was selected or removed;
these are recoverable synthetic fixtures, not existing node data. Fixture
children finished, and a process check found no remaining node/test process.
No new surviving matching system-temp directory or Python bytecode was found.
Exact original/destination paths and hashes are in
`v1b-environment-interruption.json`. The interrupted log is retained;
`v1b-cached-final-ctest-external.log` is a separate repeat with explicit env
on CTest itself. This is a recorded execution error, not a runtime improvement.

Pinned [Koinos Fizzy archive](https://github.com/koinos/fizzy/archive/b9bf7feaa8009a3d4f4bdd49245a5cd55d122055.tar.gz):

| Identity | Value |
| --- | --- |
| Upstream fork revision / package version | `b9bf7feaa8009a3d4f4bdd49245a5cd55d122055` / 0.8.0 |
| Archive SHA1 (original Hunter pin) | `d4e682dca504a831b30274fbec0b154b8b7adfd1` |
| Enforced archive SHA256 | `40975339c2e8a0f7aa1693866945becb40a65e3c0cdee862cc1ee760b99b7533` |
| `lib/fizzy/capi.cpp` preimage SHA256 | `e4fe84df817ad13dda075138ccf0dd97868c307ed5815a81868a73b74a737774` |
| Patched `capi.cpp` SHA256 | `ff5034e6de9dd84d1ffbf961ee19f8063dc39ed811ecbba3651f37e5e20c8e8b` |
| Patch SHA256 | `dd4466f681bd1c32807199056e0aefc43ffb42e42305281022475fb7f0a62bfc` |

## Defect and implementation

The pinned `fizzy_create_execution_context()` and metered equivalent allocate
inside `noexcept` without catching failure. The preserved V1-A binary's isolated
`--context-fault` installs its own terminate handler and exits **77**, reproduced
again in V1-B. This is not an actual machine OOM or a successful test. The original
exit and binary are preserved; no system protection or live process is changed.

Teleno's patch adds only an explicit `<new>` include and a `std::bad_alloc`
catch returning null in both constructors. Successful depth/tick initialization,
ABI, borrowing immutable-module ownership and execution semantics are unchanged.
All three native CMake entrypoints include `cmake/TelenoFizzy.cmake`, rather than
reuse an uncorrected imported library. Existing Hunter installations are not
overwritten. Archive verification, exact pre/postimage checks and a patch-hash
source/build identity prevent an unexpected source from being silently patched.
Offline archive selection still enforces the same digest; source-directory
overrides and an already-provided Fizzy target fail closed.

The recipe populates sources without adding the upstream root to the parent;
an isolated upstream child build preserves platform/compiler checks and avoids
modifying the parent's Hunter cache/options. Compiler/toolchain/platform and
sanitizer flags propagate; the child has two build jobs by default, not a node
runtime limit. CMake routing follows its official
[FetchContent](https://cmake.org/cmake/help/v3.19/module/FetchContent.html) and
[ExternalProject](https://cmake.org/cmake/help/v3.19/module/ExternalProject.html)
contracts. These documentation references are not validation on CMake 3.19 or
other unexecuted generators/platforms.

### Local exhaustion must not become a consensus reversion

Existing VM exceptions derive from `chain::reversion_exception`. Simply
returning null and keeping the old `create_context_exception` would let local
context exhaustion become an accepted reverted transaction. Likewise, CAPI
parse/instantiate memory failures previously became VM reversion exceptions.
The native backend now translates those allocation errors and null context to
`std::bad_alloc`, outside the consensus exception hierarchy. Invalid bytecode,
deterministic instantiation errors and WASM traps keep their existing behavior.
Transaction prevalidation/operation catches rethrow allocation failure without
a debug assertion or converting it into a successful/reverted receipt.

P2P gossip/sync catches local allocation failures separately from invalid input:
it does not score/penalize the peer or remember a failed chain attempt. Gossip
does not retry internally; normal sync polling keeps its existing backoff and
cancellation. No replay exception, fork detection or invalid-input scoring is
relaxed. Failures after already-successful canonical publication and durable
commit still require separate allocator qualification.

Controller source discards unfinalized state on a non-Koinos failure before
finalization; VM construction/execution precedes external block publication.
The scoped tests below prove VM and transaction dispatch, not an exhaustive
allocator sweep of controller persistence/index publication, persistent OOM,
or all replay fallback/error-cleanup paths. Those broader gates remain required.

## Focused tests actually executed

- Both context constructors: one-shot allocation returns null; no tracked
  allocation survives; explicit retry preserves depth 251, metered ticks 1000
  and unmetered ticks `INT64_MAX`. Current `--context-fault` exits **0**, not 77.
- Native real-Fizzy execution sweeps **35 ordinary C++ allocation boundaries**:
  each injected failure propagates `std::bad_alloc`, charges no executed ticks,
  releases tracked owners and allows an explicit unfailed retry with identical
  metering. An uninjected terminal case executes normally. Invalid bytecode
  still throws the deterministic parse exception. Empty IDs disable cache
  retention in this sweep; optional-cache faults remain covered by V1-A tests.
- **Six controlled backend dispatch cases**: allocation failure during
  authorization or operation under transaction application, block application
  and block proposal propagates locally, publishes no transaction receipt and
  leaves the parent root unchanged. Failed anonymous attempts are discarded.
  This fake backend tests classification, not a real Fizzy/controller OOM.
- Fake P2P chains exercise repeated gossip block/transaction allocation refusal,
  unchanged accepted-ID caches/head and explicit redelivery, plus a sync refusal
  followed by normal poll recovery without disconnection. No live transport or
  actual network/node is used; final results are recorded below.
- Existing parse/shared-owner/cache/loader lifetime, 800 concurrent executions,
  20 nested calls, fresh mutable memory, host-error cleanup and tick regressions
  remain exercised. No parsed-byte/global-instance/queue bound follows.
- Recipe tests cover pinned preimage/application/idempotence, altered-input
  refusal without modification, missing-patch preservation and the three
  entrypoints' routing. Their final CTest result is recorded below.

Instrumentation counts ordinary C++ requests, excluding aligned new, direct
malloc/execution stacks, usable size, fragmentation, cross-thread frees and
allocator retention. It does not prove a heap/RSS ceiling or machine OOM recovery.
No new CPU/RSS percentage improvement is claimed. V1-A measurements remain
historical; this checkpoint is a portable failure-path correction.

## Commands and current gate log

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/build'
export PYTHONDONTWRITEBYTECODE=1
cmake -S /Users/pgarcgo/code/teleno -B "$TELENO_TEST_BUILD_ROOT" \
  -DTELENO_FIZZY_ARCHIVE='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence/v1a-source-I09vWF/fizzy-source.tar.gz'
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 2
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/koinos_fizzy_lifetime_test" --context-fault
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
```

Initial focused build and CTest **3/3**, 5.05 seconds, exit 0; the subsequently
extended controlled transaction-dispatch executable exits 0. Preserved context
probe exits 77; corrected probe exits 0. Initial patch hunk counts were corrected
after `git apply --check` rejected a corrupt hand-written diff; the corrected
patch passes and yields the recorded postimage. No production source was patched
by that failed check. Required dependency workflow and both all-target builds
exit 0; four current CLI checks exit 0. Final focused CTest **5/5**, 20.95 seconds,
passes, including local P2P refusal/redelivery/poll recovery. Separate UBSan
CTest **13/13**, 49.30 seconds, passes: 12 native tests plus one Python recipe
test. Undefined/vptr exclusion is unchanged; Fizzy is now instrumented through
the child build, but other prebuilt dependencies are not. Integrated ASan/TSan
are not passed. Five preserved context probes exit 77, five corrected probes
exit 0; raw outcomes/hashes are `v1b-context-comparison.json`, not a benchmark.
Strict Koinos One MkDocs exits 0 (0.42 seconds), documentation only. Docker info
exits 1 because the daemon socket is absent; no container smoke is executed.

An initial full cached CTest before the P2P extension passes **47/47**, 156.41
seconds. The interrupted later repeat is not a pass. The final external-root
cached repeat passes **47/47**, 133.06 seconds, exit 0, with explicit external
roots (`v1b-cached-final-ctest-external.log`). The separate-dependency full suite
also passes **47/47**, 168.39 seconds, exit 0 (`v1b-dependency-ctest.log`).
These are current V1-B runs, not results inferred from V1-A.

The required dependency-script invocation used a separate owned dependency tree
and the verified native architecture shell:

```bash
env TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp' \
  TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence' \
  TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/dependency-workflow-alias-build' \
  KOINOS_DEPS_ROOT=/Users/pgarcgo/code/teleno/build-single-copy-evidence/lr2-dependencies-EEMOFW \
  HUNTER_ROOT=/Users/pgarcgo/code/teleno/build-single-copy-evidence/lr2-dependencies-EEMOFW/hunter \
  KOINOS_NODE_BUILD_DIR='/Volumes/My Passport/teleno-lr2-EEMOFW/dependency-workflow-alias-build' \
  CMAKE_GENERATOR=Ninja MAKE=/usr/bin/make \
  LIBSSH_SOURCE_TARBALL='/Volumes/My Passport/teleno-lr2-EEMOFW/dependencies-Uya3ee/downloads/libssh-0.12.0.tar.xz' \
  KOINOS_BUILD_TESTS=ON JOBS=2 PYTHONDONTWRITEBYTECODE=1 \
  /usr/bin/arch -arm64 /bin/bash ./scripts/build-cpp-libp2p-koinos.sh
```

For the second build/test/CLI sequence, the exported `TELENO_TEST_BUILD_ROOT`
is the separate-dependency directory above. The UBSan tree is the owned `ubsan/`
sibling, using `-fsanitize=undefined -fno-sanitize=vptr
-fno-omit-frame-pointer` for C/C++ and executable linking. Focused CTest selection
matches `fizzy_lifetime|cache_invariants|controller_delta|p2p_one_peer_sync|fizzy_dependency_recipe`.
The UBSan log lists its exact 13 selected tests. Native source caches name this
repository; Fizzy/dependency child caches correctly name their owned sources.
Standard and Hunter-copy configuration/build executed; standalone routing has
recipe coverage, not a separately executed complete standalone build. CMake
3.19, other generators and Linux have not been verified by these Mac runs.

### Changed files, identity and preservation

The checkpoint changes 30 Teleno files (20 extended pre-existing dirty files,
five previously clean tracked files and five new files). Other 142 files from
the 162-file starting dirty manifest are unchanged; none are removed. The ten
additional dirty paths are not all newly created files. `README.md` and the
high-throughput plan retain their starting SHA256 values. Exact paths, hashes
and cumulative goal inventory are `v1b-source-preservation.json`; build/cache/
dependency/binary hashes and flags are `v1b-build-manifest.json`.

- Guidance/release: `AGENTS.md`, `CLAUDE.md`, `CHANGELOG.md`.
- Build: `CMakeLists.txt`, `CMakeLists.hunter.txt`, `CMakeLists.standalone.txt`,
  `cmake/TelenoFizzy.cmake`, `cmake/ApplyFizzyContextPatch.cmake`,
  `cmake/patches/fizzy-context-allocation.patch`, `src/CMakeLists.txt`.
- Runtime: `src/koinos/vm_manager/fizzy/fizzy_vm_backend.cpp`,
  `src/koinos/chain/system_calls.cpp`, `src/p2p/p2p_node.cpp`.
- Tests: `tests/vm/fizzy_lifetime_test.cpp`, `tests/chain/controller_delta_test.cpp`,
  `tests/p2p/p2p_one_peer_sync_test.cpp`, `tests/scripts/fizzy_dependency_recipe_test.py`.
- Documentation: `docs/install-or-build.md`, `docs/configuration.md`,
  `docs/logs-and-diagnostics.md`, `docs/running-observer-node.md`,
  `docs/release-builds.md`, `docs/memoria.md`,
  `docs/implementation/low-resource-mainnet-observer-plan.md`,
  `docs/implementation/teleno-audit-remediation-plan.md`,
  `docs/performance/teleno-software-audit.md`,
  `docs/validation/low-resource-lr2-completion-matrix.md`,
  `docs/validation/low-resource-lr2-memory.md`,
  `docs/validation/low-resource-lr2-vm-lifetime.md`, this report.

Both native binaries report `1.3.0-dev.0+cfd2eb83b808-dirty`:

| Build | Native SHA256 |
| --- | --- |
| Cached dependencies | `d2dc7097dddc0156405174a4d54b3616a0f53be8f0e3ed78d4f6b5d537b4e52f` |
| Separate dependencies | `364ce704b3ce5a58117ebf8887d256d4c7c4a3069bca43766c75b103acda95ef` |

Koinos One documentation-only handoff extends its two already-dirty files:
`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
`docs/manual/koinos-one/backup-and-restore.md`. Native development is not a
packaged GUI capability. There is no new committed integration pin; a future
authorized integration must pin reviewed source, rebuild/stage its binary and
run its own package checks. No other Koinos One path is changed.

Final documentation checks: 164 relative file references resolve, no missing
targets; both repositories' `git diff --check` exit 0 and the native guidance
files are byte-identical. `v1b-doc-checks.json` records the two handoff hashes
and protected-file checks. The cumulative goal inventory is 113 Teleno paths,
not 113 new files; the current dirty/untracked inventory is 172 paths.

## Remaining gates and rollback

V1 still requires parsed-memory attribution, compatible byte/waiter/instance
policy, representative diverse-contract Linux CPU/latency/I/O measurements,
functioning integrated ASan/TSan and historical/differential proof. M1/P2/R1/S1
remain in the [completion matrix](low-resource-lr2-completion-matrix.md).
No whole-server policy is selected from these Mac diagnostics. LR5's full-server
long soak is separate. The historical range before 30,504,202 through beyond
32,789,378 has **not** been executed against an authenticated pinned corpus and
identified reference/checkpoints in this checkpoint.

Missing prerequisites: an authorized isolated Ubuntu/x86_64/ext4
2-vCPU/4-decimal-GB whole-machine environment, authenticated pinned full-history
backup plus immutable successors/reference/checkpoints, functioning integrated
ASan/TSan and a container daemon for applicable smoke. These exact blockers do
not waive gates or authorize a live VPS, Linux VM creation or source-trust changes.

Rollback requires a reviewed follow-up source/build change retaining local
allocation classification and constructor safety, not silently returning to an
uncorrected library. Rebuild and run equivalent regressions and reference checks.
There is no schema/database migration or chain-data rollback. Preserve all
unrelated worktree changes.

**Every applicable LR2 Definition of Done item proven: NO.**
