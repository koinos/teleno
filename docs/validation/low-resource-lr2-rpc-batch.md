# LR2 R1-B: JSON-RPC batch serialization working set

Date: 2026-10-04 (Europe/Berlin).
Status: **local correction, measurements and repository repeat gates verified;
initial backup-test failure retained. R1/LR2 NOT qualified.**

## Scope and baseline

Primary qualification remains Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB
physical RAM. This portable correction is measured on the development Mac;
no VPS, Linux VM or live node was operated. Complete genesis history, receipts,
consensus execution, durability, source trust and observer-first operation are
unchanged. No LR3/LR4, production, commit/push/release or packaged integration.

Source `/Users/pgarcgo/code/teleno`, branch `codex/reward-reinvestment-macos`,
HEAD `cfd2eb83b8080ca49911555108ce05f0a9068f01` plus preserved dirty development,
version `1.3.0-dev.0`. Starting 173 dirty/untracked file hashes are
`/Volumes/My Passport/teleno-lr2-EEMOFW/evidence/r1b-worktree-start.json`;
they match the finalized preceding R1-A state. Protected README/high-throughput
plan and all unrelated files remain preserved. Mac 26.5/arm64/16 GiB/10 CPUs,
Apple Clang 16, Release `-O3`, CMake/CTest 4.2.3. Dependencies and build-owned
Fizzy correction remain those in [V1-B](low-resource-lr2-vm-allocation.md).
The JSON library is nlohmann 3.11.2 in both dependency sets.

Generated builds, logs, loopback test data and temporary files use the owned
`/Volumes/My Passport/teleno-lr2-EEMOFW/` run, with explicit `TMPDIR`,
`TELENO_TEST_ARTIFACT_ROOT`, `TELENO_TEST_BUILD_ROOT` and
`PYTHONDONTWRITEBYTECODE=1`. Volume absence stops data-producing work. Earlier
checkpoint failures and the V1-B interrupted environment mistake remain recorded
in their reports; these are not deleted or reclassified as passes.

## Reproduced cost and correction

Previously `process_body()` retained every batch response as a JSON array,
then allocated the complete serialized body while all those trees remained
live. `make_result()` also copied each returned JSON tree into a newly allocated
success envelope after dispatch. The fixture observes earlier large result
owners increasing from zero through eleven before the twelfth item.

The success envelope/ID and null result slot are now prepared before dispatch.
The result moves into that slot. Batch items execute in the same order; each
complete result is serialized through nlohmann's public compact ostream API into
the final body and then destroyed. A small string-appending stream buffer avoids
a separate per-item dumped string. Stream failures propagate instead of silently
returning a partial body. The entire HTTP body is still constructed before
network writing: no chunked/network streaming, response limit or new setting.

The final body, parsed input, current service/protobuf/JSON result, conversion
workspace and transport buffers remain additional. This is a working-set
correction, not complete aggregate output admission or machine-wide OOM proof.
Preparing the envelope does not prove every service/conversion allocation is
failure-atomic after mutation. No accepted result is refused by a new byte limit.

### Preserve deterministic serialization-failure dispatch

The previous implementation dispatched all batch items before strict UTF-8
serialization. Simply serializing incrementally and throwing immediately would
skip later calls, including accepted mutations. On nlohmann type error 316,
the new implementation saves the first error, releases the unusable partial
body, executes all remaining items once without serialization, then rethrows.
No HTTP bytes are written. A later dispatch failure still propagates as it did
before final serialization. This does not promise later execution after physical
allocation failure or every other exception: the previous JSON-array insertion
could also fail between calls. No automatic retry or rollback assertion.

## Focused functional evidence

`koinos_jsonrpc_batch_memory_test` uses a real loopback server/client and bounded
fake chain service, not public transactions or actual contract execution:

- Twelve 512-KiB raw read results produce the exact same 8,389,203-byte compact
  base64 response, IDs and array order, with one dispatch each.
- Escaped/newline/non-ASCII, integer/floating/null and currently accepted array
  IDs retain exact bytes. Empty batch and mixed success/internal-error/mutating
  batch behavior are unchanged. Array IDs characterize existing behavior rather
  than asserting JSON-RPC-spec validity.
- First/middle/last and multiple invalid-UTF8 result failures preserve later
  fake mutation counts and transmit no partial HTTP bytes.
- Warm keepalive, subsequent health work and final large-allocation cleanup;
  prior result retention stays at at most one large owner, the output string.
- Existing R1-A lifecycle tests cover full replies, pipelines/CORS, local
  refusal, slow readers/cancellation, admitted service completion and restart.

The original baseline fixture passed **1/1**, 2.39 seconds, before the
post-correction retention assertion was introduced. Its preserved executable
`r1b-before-batch-memory-test` is used for measurements. Initial corrected focused
tests passed **2/2**, 8.47 seconds; subsequent edge regressions are required in
the final full suites. Logs retain both stages rather than treating baseline
functional success as evidence of reduced memory.

## Five paired before/after processes

One excluded process warm-up per version; five measured alternating processes,
each also warms its service conversion before worker attribution. No simultaneous
build/tests during measurement. Same 12 items, payload, driver and full byte
comparison. Results and command/binary hashes: `r1b-comparison.json`.

| Metric | Before, median [min–max] | After, median [min–max] |
| --- | --- | --- |
| Peak tracked ordinary C++ requested bytes | 21,960,048 [same] | 14,265,952 [same] |
| Total tracked ordinary C++ requested bytes | 47,465,473 [same] | 39,080,833 [same] |
| Fixture-process peak RSS bytes | 56,672,256 [56,655,872–56,688,640] | 48,644,096 [48,578,560–48,660,480] |
| Timed fixture microseconds | 76,881 [75,675–78,892] | 77,002 [76,322–78,966] |

Peak requested bytes fall approximately 35%; total requested bytes fall
8,384,640 bytes (twelve 699,056-byte large copies). Allocation totals are not
physical retained heap. RSS includes client, expected array, verification and
driver buffers; it is not standalone node RSS. Timing includes client exchange,
byte verification, subsequent call and shutdown; ranges overlap, so no speedup
or latency improvement is claimed. No disk/I/O reduction is claimed.

Tracking uses a bounded, allocation-free 4,096-slot thread-local probe on one
worker. It measures ordinary `new` requested sizes, not aligned allocation,
direct malloc, allocator overhead/retention or global concurrency. Population
standard deviations in the artifact: zero for requested bytes; RSS
12,260.66/30,387.79 bytes; time 1,099.13/990.91 microseconds, before/after.
Five samples do not justify tail-latency percentiles or a whole-node ceiling.

Per-item serialization materializes the output earlier: before the last item,
tracked prior large bytes increase from 7,689,616 to 9,043,968 even though owners
decrease from eleven to one. String growth/spare capacity remains. The peak
benefit is removal of aggregate trees at final serialization and returned-tree
copies, not lower bytes at every instant or bounded final response size.

## Commands and final gates

Use the same explicit external environment for every command. The required
dependency workflow uses the owned external dependency alias and `JOBS=2`,
followed by an all-target build only after it exits. Never build the same tree
concurrently with its dependency workflow. Exact invocation is retained in the
command manifest; build caches must name this exact source repository.

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/build'
export PYTHONDONTWRITEBYTECODE=1
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 2
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
"$TELENO_TEST_BUILD_ROOT/koinos_jsonrpc_batch_memory_test" --measure
```

Final actual results (all exits 0 unless noted):

| Gate | Result | Artifact in the owned external evidence root |
| --- | --- | --- |
| Required test-enabled dependency workflow | Passed | `r1b-dependency-workflow.log` |
| Cached / separate dependency all-target builds | Passed / passed, serialized per tree | `r1b-cached-all-build.log`, `r1b-dependency-all-build.log` |
| Final focused RPC regressions | 2/2, 6.45 s | `r1b-final-focused-tests.log` |
| Cached full CTest repeat | 48/48, 122.44 s | `r1b-cached-ctest-repeat.log` |
| Separate dependency full CTest | 48/48, 161.64 s | `r1b-dependency-ctest.log` |
| Separate UBSan subset | 14/14, 53.67 s | `r1b-ubsan-tests.log` |
| Version/help in both native trees | Four commands pass | `r1b-cli-results.json` |
| Koinos One strict MkDocs, external output | Passed, 0.34 s | `r1b-koinos-one-docs.log` |
| Docker prerequisite | Exit 1, daemon unavailable; smoke not run | `r1b-docker-info.log` |
| Current integrated ASan/TSan, Linux and historical checks | Not qualified / not executed | [Completion matrix](low-resource-lr2-completion-matrix.md) |

UBSan uses `-fsanitize=undefined -fno-sanitize=vptr -fno-omit-frame-pointer`;
13 native tests plus the Python dependency-recipe test. Corrected Fizzy is
instrumented; other prebuilt dependencies and vptr are excluded. Earlier Mac
ASan initialization/TSan failures remain unresolved, not passes. No sanitizer
waiver or unsupported Linux/historical claim follows from this subset.

Native identity is `1.3.0-dev.0+cfd2eb83b808-dirty` in both trees, Mach-O arm64.
Cached binary SHA256:
`e6c9e6cff36f066469ee167075104e91e24488e88ba33b1721b9e687fe1a5429`;
separate dependency binary:
`65730b4325c2f2f528df502b0d8fde4ebd01bfdfa6451c538a6ffc565817620a`.
All three native caches name the exact Teleno source, not another workspace.
Original installed Fizzy is unchanged. There is no new committed pin to integrate.

Command/build/source manifests, binary/source artifact hashes and link checks
are `r1b-command-results.json`, `r1b-build-manifest.json`,
`r1b-source-preservation.json` and `r1b-doc-checks.json`. Native tracked changes
and untracked inputs are archived without `.git`, ignored data, dependencies or
other repositories. The preserved baseline executable SHA256 is
`cb1e67ec08da3bbfb736f3ae37bf9bbc27481bb80fd8501eeb5ee85e4acbea51`.
Its fixture source is reconstructed from the inspected original by removing
only later assertions/edge cases; it was not rebuilt. Pre-correction runtime
source is in the R1-A final tracked patch at the same HEAD. Both staged tests
and all original measurement logs remain available.

### Initial full-suite failure is retained

`r1b-cached-ctest.log` exits 8: **47/48 pass**, 180.21 seconds. The unchanged
`koinos_backup_service_test` aborts at line 228, asserting a saved asynchronous
operation status is `succeeded`. Its helper polls for only 100 x 10 ms, then the
test joins the worker but asserts the previously returned status rather than
querying the final state. The owned failed fixture had a published manifest and
COMPLETE marker, but that alone does not prove its final service status.
This failure is consistent with a stale pre-join observation; the original
failure did not record the actual final status, so its exact runtime cause
remains qualified rather than invented.

An immediate isolated repeat of the unchanged test passes **1/1**, 1.75 seconds,
exit 0 (`r1b-backup-original-repeat.log`). Neither backup production code nor
its test/expectations/deadlines are changed in this checkpoint. Its separate
target does not link the changed JSON-RPC library. The first full suite overlapped
independent UBSan work, not a before/after performance sample; final full suites
are repeated serially. A repeat pass does not erase this timing-sensitive test
gap. The failure/reproduction command remains in the evidence manifest.

## Changed files, risks and checkpoint exit

Runtime: `src/jsonrpc/jsonrpc_server.cpp/.hpp`; regression:
`tests/jsonrpc/batch_memory_test.cpp`; registration: `src/CMakeLists.txt`.
Documentation: AGENTS/CLAUDE, CHANGELOG, docs index/configuration/RPC/diagnostics/
release, active plan/matrix/memory, scoped audit/remediation, project memory and
this report. Koinos One: two additive operator/manual documentation pages only.
Final preservation verifies 19 checkpoint files (17 extended, two new),
156 other starting files unchanged, zero removed, 175 current dirty/untracked
paths. README/high-throughput-plan hashes are unchanged and AGENTS/CLAUDE remain
byte-identical. Complete checkpoint list:

```text
AGENTS.md
CLAUDE.md
CHANGELOG.md
src/CMakeLists.txt
src/jsonrpc/jsonrpc_server.cpp
src/jsonrpc/jsonrpc_server.hpp
tests/jsonrpc/batch_memory_test.cpp
docs/README.md
docs/configuration.md
docs/rpc-endpoints.md
docs/logs-and-diagnostics.md
docs/release-builds.md
docs/implementation/low-resource-mainnet-observer-plan.md
docs/implementation/teleno-audit-remediation-plan.md
docs/performance/teleno-software-audit.md
docs/validation/low-resource-lr2-completion-matrix.md
docs/validation/low-resource-lr2-memory.md
docs/validation/low-resource-lr2-rpc-batch.md
docs/memoria.md
```

Koinos One's only changed paths remain
`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
`docs/manual/koinos-one/backup-and-restore.md`, preserving their previous edits.
No app code, changelog/version, submodule, binary or package is integrated.

R1/LR2 still requires aggregate service/input/DOM/serialization/output/transport
policies, remaining mempool/P2P/VM/state accounting/lifetimes, functioning
integrated ASan/TSan, an authorized isolated Linux whole-machine environment and
eligible authenticated/pinned full-history inputs/successor roots/reference
versions for current historical/differential validation. Docker runtime is not
currently available; smoke is not a pass. LR5's long full-server qualification
remains separate. No existing backup identity was rewritten or trust bypassed.

**Every applicable LR2 Definition of Done item proven: NO. Goal remains active.**
