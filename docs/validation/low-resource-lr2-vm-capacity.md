# LR2 V1-C: retained parsed-module cache capacity

Date: 2026-10-04 (Europe/Berlin).
Status: **cache policy, focused/measurement and repository local gates verified.
V1/LR2 NOT qualified.**

Supplementary 2026-10-17 [Linux policy measurements](linux-vm-cache-policy.md)
now quantify five tracked and five probe-disabled triplets with distinct valid
programs and actual code-hash IDs. They preserve this report's earlier Mac
evidence and do not select a production default or close V1.

## Scope and source correspondence

Primary qualification remains Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB
physical RAM. This portable cache policy is tested on the development Mac,
not a VPS/VM or live node. Full genesis history, all block/receipt records,
consensus validation, resource accounting, durability, source trust and
observer-first recovery are unchanged. No LR3/LR4, production, deployment,
commit/push/release, GUI or package integration.

Source `/Users/pgarcgo/code/teleno`, branch `codex/reward-reinvestment-macos`,
HEAD `cfd2eb83b8080ca49911555108ce05f0a9068f01` plus preserved dirty development,
version `1.3.0-dev.0`. Starting 175 dirty/untracked hashes are recorded in
`/Volumes/My Passport/teleno-lr2-EEMOFW/evidence/v1c-worktree-start.json`;
they matched finalized R1-B. Mac 26.5 (25F71), arm64, 16 GiB/10 CPUs,
Apple Clang 16, Release `-O3`, CMake/CTest 4.2.3. Dependency revisions and
instrumentation boundaries remain those in [V1-B](low-resource-lr2-vm-allocation.md).
The pinned private Fizzy `Module` layout was inspected, not inferred from a
system library or input bytecode size.

Generated builds, test data, logs and temporary files remain under the owned
`/Volumes/My Passport/teleno-lr2-EEMOFW/` run with explicit `TMPDIR`,
`TELENO_TEST_ARTIFACT_ROOT`, `TELENO_TEST_BUILD_ROOT` and
`PYTHONDONTWRITEBYTECODE=1`. Volume absence stops data-producing work. Earlier
failures/environment mistakes remain in their prior reports, not erased.

## Accounting and policy

Previously the backend retained at most 32 entries, regardless of their parsed
capacity. The new optional unsigned setting
`chain.vm-module-cache-max-capacity-bytes` defaults to zero (byte-unlimited);
the existing count ceiling remains 32. Invalid/negative/overflowing configuration
fails loading. The actual native controller passes this budget to its owned
Fizzy backend; no mainnet resource limit or validity rule changes.

The immutable guard caches a no-allocation, checked inventory of the pinned
parsed module: object size, capacities of all 14 section/imported-type vectors,
nested function inputs/outputs, import/export strings, element initialization,
decoded instructions and data strings including terminators. Indexed charges
also include the guard, map/list value objects and their actual key capacities.
Small-string capacities and repeated shared modules under different IDs are
conservatively charged. Overflow makes the optional retention charge unavailable
and bypasses caching; it never turns valid execution into rejection.

This is **attributed retained capacity, not physical heap/RSS**. Shared-control
blocks, allocator rounding/retention and tree/list node overhead are excluded;
the entry-count ceiling bounds the number of those cached objects, not their
physical platform bytes. Cache containers themselves, transient parse/staging,
caller-held/evicted modules, waiting callers and mutable instances are additional.
Changing the pinned dependency requires a fresh layout/accounting review.

Staged map/list nodes and strings are fully prepared before eviction/publication.
Allocation failure leaves membership, attributed capacity and recency unchanged;
optional retention failure still returns the caller's valid owned module. Warm
hits only look up/splice under the cache lock and do not allocate. Replacement
reuses its unique LRU member and adjusts capacity before evicting other owners.
Oversized new IDs do not flush useful unrelated entries. Oversized replacement
retires the old same-ID entry to prevent stale reuse. Eviction/clear does not
invalidate owners held by execution callers.

Cold-load locking/coalescing remains V1-A behavior; locks never span execution
or host callbacks. Active mutable instances remain fresh, not pooled or shared.
An entry that cannot be retained is reparsed on later calls, which can cost CPU.
No qualified Linux/default profile is chosen from these Mac diagnostics.

Native C++ snapshots and periodic `[metrics]` logs expose availability, current
entries/capacity/maximum and saturating lookup-hit/miss, eviction and bypass
counters. Cold-lock rechecks count as lookups; counters are not execution/parse
counts. `clear()` resets current entries/capacity, not cumulative counters.
No new HTTP/CLI endpoint or RPC schema is introduced.

## Focused regressions

`koinos_module_capacity_test` checks all pinned container/nested-capacity deltas,
checked addition/multiplication, exact and below-exact boundaries, zero count,
zero/unlimited and maximum byte settings, larger/smaller/oversized replacements,
eviction/clear with held owners and 10,000 allocation-free warm hits. Four staged
ordinary-new faults preserve capacity/membership/recency; optional oversized
loads return their valid owner on every call. Eight threads perform 8,000 bounded
retention/lookups with repeated clear. This is functional concurrency coverage,
not a supported TSan result. Constructed private-layout capacity fixtures are
not claimed as valid protocol execution inputs.

`koinos_fizzy_lifetime_test` preserves the prior context/parser/cache/allocation
tests and 800 concurrent executions. Three cache policies (default, one-byte
bypass and retaining limit) execute identical bytecode/metering. Sixty nested
calls across those policies preserve fresh-instance memory, exception handling,
missing-entrypoint rejection and final ordinary-new cleanup. Config tests cover
strict types and uint64 endpoints; native controller/factory snapshots prove
budget wiring. Current focused tests: **4/4**, 11.89 seconds, exit 0.

## Five comparable processes per policy

Preserve the pre-change VM executable; one excluded process warm-up per group,
then five measured alternating before/default/512-KiB groups. No concurrent
build or test during measurement. Every process loads 16 IDs with 128-KiB data,
then cycles 80 executions across those IDs, with identical 288 meter ticks and
zero final tracked allocations. The retained snapshot follows the first 16
loads. Cache counters/capacity are sampled after all executions.

| Metric | Before, median [min–max] | After default, median [min–max] | After 512 KiB, median [min–max] |
| --- | --- | --- | --- |
| Retained ordinary-new requested bytes | 2,108,016 [same] | 2,108,400 [same] | 395,325 [same] |
| Peak ordinary-new requested bytes | 2,239,556 [same] | 2,239,940 [same] | 658,021 [same] |
| Total ordinary-new requested bytes | 18,937,328 [same] | 18,937,712 [same] | 50,475,552 [same] |
| Fixture peak RSS bytes | 11,714,560 [11,698,176–11,812,864] | 11,632,640 [11,616,256–11,665,408] | 9,895,936 [9,846,784–9,895,936] |
| Timed fixture microseconds | 612 [598–706] | 605 [591–632] | 1,026 [1,014–1,051] |

New default accounting costs 384 additional retained requested bytes in this
fixture; it is not an unconditional default memory/speed optimization. Its time
ranges overlap the baseline, so no default speedup is claimed. The 512-KiB
policy keeps three entries/395,316 charged bytes rather than sixteen/2,108,352.
Retained requested bytes fall about 81%, but cyclic access misses on all 80
subsequent calls, with 192 lookup misses (two per cold load) and 93 evictions.
Total allocation requests rise and the timed fixture gets slower. The 512-KiB
setting is an experimental policy, **not a recommended VPS default**.

The probe is a bounded 65,536-slot thread-local ordinary-new requested-size
tracker, not allocator usable size, direct/aligned allocations, retained heap or
whole-machine RSS. RSS includes the fixture/tracker/runtime; mutable execution
buffers backed by direct malloc are not attributed as ordinary-new requests.
Timing includes parsing/execution/cleanup and probe overhead. These roughly millisecond
samples do not establish mainnet throughput, tail latency or Linux CPU costs.
Population dispersion is in `v1c-comparison.json`; no disk/I/O savings measured.

## Commands and broader gates

Every invocation uses the external environment; exact roots/versions/commands
and exit statuses are retained in the checkpoint evidence. CMake source caches
must name `/Users/pgarcgo/code/teleno`, never another workspace.

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/build'
export PYTHONDONTWRITEBYTECODE=1
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 2
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
python3 "$TELENO_TEST_ARTIFACT_ROOT/v1c_measurements.py"
```

The required dependency workflow uses the owned external dependency alias,
`KOINOS_BUILD_TESTS=ON` and `JOBS=2`, then a serialized all-target rebuild.
Current cached/all-target, separate dependency workflow/all-target and UBSan
builds exited 0. Final actual results:

| Gate | Actual result | Artifact under `evidence/` |
| --- | --- | --- |
| Required dependency workflow | exit 0 | `v1c-dependency-workflow.log` |
| Cached / separate all-target builds | both exit 0 | `v1c-all-build.log`, `v1c-dependency-all-build.log` |
| Final focused config/cache/VM/capacity | 4/4, 11.89 s, exit 0 | `v1c-final-focused-tests.log` |
| Cached full CTest | 49/49, 179.73 s, exit 0 | `v1c-full-ctest.log` |
| Separate dependency full CTest | 49/49, 172.94 s, exit 0 | `v1c-dependency-full-ctest.log` |
| Separate UBSan selected integrated components | 16/16, 55.91 s, exit 0 | `v1c-ubsan-tests.log` |
| Version/help for both native binaries | four exit 0 | `v1c-cli-results.json` |
| Koinos One strict MkDocs, external output | exit 0, 0.38 s | `v1c-koinos-one-mkdocs.log` |
| Docker prerequisite | exit 1, daemon socket absent; smoke unrun | `v1c-docker-info.log` |

UBSan uses `halt_on_error=1:print_stacktrace=1`, with `vptr` explicitly excluded;
15 native tests plus the pinned-dependency recipe execute. Native components
and owned Fizzy are instrumented; other prebuilt dependencies are not. This is
not ASan/TSan, whole-machine exhaustion or historical/Linux qualification. Both
full suites ran serially without concurrent builds or each other. The previous
R1-B timing-sensitive saved backup-status failure remains in its report; passing
unchanged current repeats does not establish its original exact runtime cause.

Both native binaries identify as `1.3.0-dev.0+cfd2eb83b808-dirty`, Mach-O arm64.
Cached SHA256 `349bfa4689c43ee97358e6725b017c0e7a32ec19db70fa2a0c7f5827957fcc32`;
separate dependency SHA256 `ecf91bf10522f8cf227567605c57eac238ee8b644b3743850671263b97ab319c`.
The preserved baseline VM fixture SHA256 is
`e5b5bfba49ce7572f0bf7081bbe73562a39728502fe908bcc172b4c24d50eb2a`;
current measured fixture SHA256
`2db0f46c90d66ec0bc2a9951c7bf9be3d398a91e100bfe8263639ab5e93b6ab8`.
The complete manifest retains flags, exact source roots, dependency-layout
hashes and fixture sample commands. Binary dirtiness is not a committed pin.

The initial helper compilation (`v1c-first-build.log`, exit 1) failed because pinned `fizzy::bytes` is a
`basic_string<uint8_t>`, not a vector; accounting was corrected to include its
capacity/terminator, then rebuilt/tested. A command named a nonexistent focused
target and failed immediately (`v1c-corrected-build.log`, exit 1); the actual `koinos_config_test` target was used
in successful repeats. Logs retain these attempts. No upstream dependency,
backup timing test or deadline was edited to obtain a pass.

## Remaining prerequisites and exit criteria

V1-C's cache axis does not close V1/LR2. Instance/queue/caller-held memory,
complete admission/other-consumer policies, supported integrated ASan/TSan,
representative Linux whole-machine workload and historical/differential proof
remain required. No Linux environment was created/started or VPS operated.
The eligible authenticated/pinned full-history corpus, successor/checkpoint
roots and reference execution range remain unavailable; no current differential
run from before 30,504,202 through beyond 32,789,378 was performed. Deterministic
fixtures are not full historical parity. Docker smoke is not claimed without a
functioning daemon. See the [completion matrix](low-resource-lr2-completion-matrix.md).

LR3 disk reserve, LR4 lossless archive, LR5 server/long-soak and LR6 publication
remain separately scoped. No pruning, omitted receipts, `teleno_light` fallback,
weakened validation/persistence or producer activation. **Every applicable LR2
Definition of Done item proven: NO. Persistent goal remains open.**

## Changed files and reproducibility artifacts

Checkpoint files (complete list, not all accumulated SC/LR work):

- Guides/release: `AGENTS.md`, `CLAUDE.md`, `CHANGELOG.md`.
- Native operator/index docs: `docs/README.md`, `docs/configuration.md`,
  `docs/logs-and-diagnostics.md`, `docs/release-builds.md`,
  `docs/running-observer-node.md`.
- Plan/audit/memory: `docs/implementation/low-resource-mainnet-observer-plan.md`,
  `docs/implementation/teleno-audit-remediation-plan.md`,
  `docs/performance/teleno-software-audit.md`, `docs/memoria.md`,
  `docs/validation/low-resource-lr2-completion-matrix.md`,
  `docs/validation/low-resource-lr2-memory.md` and this new report.
- Build/runtime: `src/CMakeLists.txt`, `src/core/config.cpp`,
  `src/core/config.hpp`, `src/main.cpp`, `src/koinos/chain/controller.cpp`,
  `src/koinos/chain/controller.hpp`.
- VM: `src/koinos/vm_manager/vm_backend.cpp`, `vm_backend.hpp`, new `vm_memory.hpp`;
  `src/koinos/vm_manager/fizzy/fizzy_vm_backend.cpp`, `fizzy_vm_backend.hpp`,
  `module_cache.cpp`, `module_cache.hpp`, new `module_memory.cpp`/`.hpp`.
- Tests: `tests/core/config_test.cpp`, `tests/vm/fizzy_lifetime_test.cpp`,
  new `tests/vm/module_capacity_test.cpp`.

Documentation-only handoff extends the two already-dirty Koinos One documents
`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
`docs/manual/koinos-one/backup-and-restore.md`. Removing only this checkpoint's
added paragraphs must reproduce their starting hashes. No GUI/config generator,
app changelog/version, submodule, binary, packaging or deployment changes.

Evidence prefix `/Volumes/My Passport/teleno-lr2-EEMOFW/evidence/v1c-*`:
starting worktree/GUI hashes, build manifest, command/CLI results, paired
`v1c-comparison.json` plus executable measurement recipe `v1c_measurements.py`,
source preservation and documentation/link checks. Native tracked patch and
untracked source archives exclude `.git`, ignored data/dependencies and other
repositories. Earlier R1-B archives and fixture preserve the before-source/
binary correspondence. Large evidence remains ignored on My Passport; sources/
committed documentation remain in the repositories. No native commit exists
for a new submodule pin; dirty binary identity is not an authorized integration
target. Rollback is a reviewed revert of this cache-only delta or default-zero
policy, not reset of the dirty worktree or deletion of state/history.

Preservation checks prove 33 checkpoint files (28 extended existing files,
five newly created), with 151 other starting dirty files unchanged; no missing
file, staged change or HEAD movement. Accumulated LR2 file inventory is 126.
README/high-throughput plan retain their starting hashes; AGENTS/CLAUDE are
byte-identical. `v1c-doc-checks.json` resolves 262 relative file links without
missing targets (not an external-link/publication or anchor-validity check),
passes scoped whitespace checks and proves both GUI docs retain pre-existing
content when only the new paragraphs are removed.

Native tracked patch SHA256
`8eabf4e8b3c6123299dd250bb990c3043f028095f9553eb2b7dfd2e9624bf262`
(349,551 bytes); native untracked source archive SHA256
`8bc110fb867770c2bf44dfb844df59275bb73f352279fe1e4afdc051c995b673`
(159,964 bytes). These source archives exclude documentation/data/dependencies;
final worktree hashes and complete checkpoint/cumulative file lists remain in
`v1c-source-preservation.json` and the final documentation check artifact.
