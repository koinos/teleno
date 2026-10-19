# LR2 R1-A: completed HTTP body ownership

Date: 2026-10-04 (Europe/Berlin).
Status: **local checkpoint and repository build/regression gates verified;
R1/LR2 NOT qualified**.

## Scope and baseline

Primary target remains Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB physical RAM.
This is a portable lifetime correction diagnosed on Mac, not a qualified VPS
profile. Complete history/receipts, validation, durability, source trust,
observer-first behavior and all defaults are unchanged. No live server/VM,
production, deployment, LR3/LR4, git publication or GUI/package integration.

Source: `/Users/pgarcgo/code/teleno`, branch `codex/reward-reinvestment-macos`,
HEAD `cfd2eb83b8080ca49911555108ce05f0a9068f01` plus preserved dirty work,
native version `1.3.0-dev.0`. Mac 26.5 / arm64 / Apple Clang 16 / 16 GiB /
10 CPUs, CMake 4.2.3, Release `-O3`; dependency identities remain those in
[V1-B](low-resource-lr2-vm-allocation.md). Starting 172-file hashes are
`/Volumes/My Passport/teleno-lr2-EEMOFW/evidence/r1a-worktree-start.json`.
README and the high-throughput plan are protected. Generated builds, temporary
data, loopback fixture artifacts and evidence use only the owned My Passport run
with explicit environment on every producing command. Volume absence stops work.

The preceding V1-B checkpoint finishes both full suites 47/47 (133.06 / 168.39
seconds), with exact source/build/preservation manifests. Those are historical
V1-B results, not proof of these new RPC changes. Its stopped/misplaced-fixture
attempt remains recorded; R1-A does not erase or recategorize that exception.

## Reproduced problem and correction

An owned keepalive session retained its complete previous response until the
next request or idle deadline, even after a successful write. Its completed
request parser also survived service dispatch through the write. These are
unnecessary lifetimes, not an unbounded leak or evidence of a dangling pointer.

Attribution-only instrumentation was added to the current pre-correction server;
the large-response idle-release regression then failed, exit **1**, at its
bounded wait (`r1a-before-lifetime-failure.log`). The fixture verifies a complete
fake-service 8-MiB result encoded as an 11,184,860-byte JSON-RPC body, not a real
contract execution or representative mainnet workload. The original
instrumented executable is preserved as `r1a-before-http-lifetime-test`.

The request parser is now destroyed after synchronous service/body construction,
before output writing. The response is an optional owner destroyed only in the
completed `async_write` callback. Cancellation retains it while that write still
borrows it; session destruction unwinds accounting after pending handlers drain.
Use destruction rather than `clear()` or empty-string assignment, which is not
a portable capacity-release guarantee on the Linux target. The input flat
buffer remains intact, preserving unread pipelined requests.

No output is truncated, refused after an accepted mutation, or automatically
retried. JSON-RPC schemas, IDs, batch order, base64, CORS, health, status/error
codes, access policy, keepalive, body/header limits and I/O deadlines remain.
Stop still waits for admitted service execution; disconnect does not mean rollback.

### Accounting is attribution, not admission or total memory

`JSONRPCServer::http_body_memory()` is a synchronized native C++ diagnostic,
not a new HTTP/CLI endpoint or configuration setting. It reports aggregate
completed-request string capacity, materialized-response string capacity and
the sampled-at-transition combined high water. Pending handler owners remain
charged until their body/destructor release, not merely registry removal.
Current capacities become zero after write/stop; high water remains cumulative
for the server object's lifetime, including restarts.

This excludes incomplete reads, headers, flat/transport buffers, protobuf and
JSON trees, temporary serialization, allocator overhead/retention and service
results. String capacity includes inline storage, excludes the terminating byte
and allocator overhead, and is not heap/RSS. It is
not a byte limit, reservation, instantaneous allocation sampler or replacement
for R1 aggregate service/output admission. Transition updates acquire a short
mutex; short-call contention and Linux throughput cost remain unmeasured.

## Functional evidence

The extended real-loopback lifecycle test verifies:

- A complete 8-MiB fake result, identical base64 bytes/ID and healthy keepalive;
  attributed response capacity becomes zero after write, not just connection close.
- A 512-KiB string ID held through gated dispatch and stop; stop cannot free or
  cancel already-admitted work early. Returning from dispatch drains both owners.
- Slow reader with large request/response: request is released before writing,
  response remains live while writing; two simultaneous pending bodies aggregate
  correctly, and their high water includes the just-completed request before
  release. Cancellation/stop drains both owners to zero.
- A large response followed by a pipelined health request; input buffered bytes
  survive parser destruction. OPTIONS 204, method refusal 405, malformed JSON,
  a mixed success/local-capacity batch, CORS, health reuse and `Connection: close`
  remain correct.
- Existing 20-cycle lifecycle, idle/partial deadline, overload/recovery,
  destruction and native local-refusal tests remain.

Initial focused CTest passes **1/1**, 7.92 seconds, exit 0 (`r1a-focused-tests.log`).
An initial test include named a nonexistent header; its build exit 1 is preserved
in `r1a-baseline-build.log`. The corrected build and baseline failure are separate;
no test expectation/deadline or system protection was weakened.

## Before/after measurements

One warm-up process followed by five measured processes per version, serialized
with no concurrent builds. Both run the same complete-response/keepalive/health
fixture; only the expected retention branch differs. The prior instrumented
baseline is not an uninstrumented production binary. Earlier preliminary samples,
including a first-startup outlier, are retained but excluded from this comparison.
Raw argv, binary hashes, outcomes, durations, capacities and per-child peak RSS
are `r1a-final-comparison.json`; logs use
`r1a-final-{before,after}-{warm-up,repeat-N}.log`. The initial comparison remains
`r1a-comparison.json`; it preceded the pre-release overlap sample, two-pending-
response and explicit-close regressions. Its high water omitted the tiny paired
request overlap. Final telemetry observes both owners before request destruction;
the final comparison supersedes those earlier values.

| Metric | Before, median [min, max], n=5 | After, median [min, max], n=5 |
| --- | --- | --- |
| Idle attributed response capacity, bytes | 18,087,935 [same, same] | 0 [same, same] |
| Combined HTTP-body capacity high water, bytes | 18,088,006 [same, same] | 18,088,006 [same, same] |
| Whole fixture-process peak RSS, bytes | 103,120,896 [103,120,896, 103,170,048] | 91,979,776 [91,947,008, 92,045,312] |
| Whole fixture elapsed, milliseconds | 160.34 [158.91, 164.42] | 160.97 [159.39, 161.38] |

The idle ownership improvement is exact for this fixture; body capacity high
water is unchanged. The RSS measurement includes the client,
base64/JSON equality checks and temporary expected result in the same process.
Earlier freeing removes overlap with those client allocations. It does **not**
establish a standalone node steady/peak RSS reduction, Linux capacity, natural
RPC latency or machine OOM recovery. Elapsed differences are within observed
spread: no speed improvement is concluded. Contract-size reachability under
real chain/resource limits and representative mixed output remain unqualified.

## Reproduction and current build gates

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/build'
export PYTHONDONTWRITEBYTECODE=1
cmake --build "$TELENO_TEST_BUILD_ROOT" --target koinos_jsonrpc_session_lifecycle_test --parallel 2
"$TELENO_TEST_ARTIFACT_ROOT/r1a-before-http-lifetime-test" --body-lifetime-only # recorded exit 1
"$TELENO_TEST_ARTIFACT_ROOT/r1a-before-http-lifetime-test" --expect-retained
"$TELENO_TEST_BUILD_ROOT/koinos_jsonrpc_session_lifecycle_test" --body-lifetime-only
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" -R '^koinos_jsonrpc_session_lifecycle_test$' --output-on-failure
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 2
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
```

The required dependency script uses the exact isolated dependency/environment/
native-shell invocation in V1-B, with current logs prefixed `r1a-`. Initial and
final workflow invocations exit 0, including the final overlap telemetry change.
The separate dependency and UBSan trees remain siblings under the external run.
All-target builds exit 0. One final separate all-target build overlapped the
still-running dependency workflow in the same external tree; both exit 0, but
that overlap is not used as clean final build proof. A separate serialized
all-target repeat after both completed exits 0
(`r1a-serialized-dependency-all-build.log`). No shared-tree parallel build is
part of the final measurement/test procedure, and no user/node data was touched.

The initial broader suites before strengthened overlap/two-owner tests pass
47/47 (130.73 / 159.48 seconds). Final strengthened focused CTest passes **1/1**,
8.00 seconds; final separate UBSan passes **13/13**, 43.05 seconds (12 native
tests plus one Python recipe). Native undefined instrumentation excludes vptr;
corrected Fizzy is instrumented, other prebuilt dependencies are not. Neither
integrated ASan nor TSan is passed. Final current full suites pass **47/47**:
cached 126.23 seconds, separate dependency 160.15 seconds, both exit 0
(`r1a-final-cached-ctest.log`, `r1a-final-dependency-ctest.log`). Both binaries'
version/help checks exit 0 (`r1a-final-cli-results.json`); this is current execution,
not inferred from earlier runs. Strict Koinos One docs exit 0 (1.26 seconds), docs only.
Fresh Docker info exits 1 (daemon socket absent); no applicable smoke executed.

## Changed files and reproducibility artifacts

Eighteen Teleno checkpoint files: 17 extended starting dirty files and one new
report. Other 155 files of the 172-file start remain unchanged; none removed.
The cumulative goal inventory is 115 paths, not 115 new files. Source hashes
and protected README/high-throughput hashes are `r1a-source-preservation.json`;
native/test/cache hashes and flags are `r1a-build-manifest.json`.

Both Mach-O arm64 binaries report `1.3.0-dev.0+cfd2eb83b808-dirty`:

| Build | Native SHA256 |
| --- | --- |
| Cached | `9d0196d2b11e4005361917836fbd1e69f637cad7a34e4cae67cdb2bc70b2180d` |
| Separate dependencies | `f5383459d9af26a03e9aacf487f188c2595e36be1c80daa2bb3b6c0b030341f6` |

All three native CMake caches name this source and contain no other workspace or
`teleno-extract` reference. Original installed Fizzy SHA256 remains unchanged.
No staged changes or new commit/push/tag/release exists.

- Runtime/tests: `src/jsonrpc/jsonrpc_server.cpp`, `src/jsonrpc/jsonrpc_server.hpp`,
  `tests/jsonrpc/session_lifecycle_test.cpp`.
- Guidance/changelog: `AGENTS.md`, `CLAUDE.md`, `CHANGELOG.md`.
- Documentation: `docs/README.md`, `docs/configuration.md`, `docs/rpc-endpoints.md`,
  `docs/logs-and-diagnostics.md`, `docs/release-builds.md`, `docs/memoria.md`,
  `docs/implementation/low-resource-mainnet-observer-plan.md`,
  `docs/implementation/teleno-audit-remediation-plan.md`,
  `docs/performance/teleno-software-audit.md`,
  `docs/validation/low-resource-lr2-completion-matrix.md`,
  `docs/validation/low-resource-lr2-memory.md`, this new report.

Koinos One changes only its two already-dirty documentation files:
`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
`docs/manual/koinos-one/backup-and-restore.md`. Its instructions and status were
read before additive editing. Native dirty development is not packaged GUI
capability; no new committed submodule/integration pin exists.

The ignored evidence retains a complete tracked native-input patch against the
recorded HEAD (`r1a-final-native-tracked.patch`) and untracked native-source archive
(`r1a-final-native-untracked.tar.gz`, 62 entries; NUL file list alongside it).
Earlier snapshots are retained separately. They include prior
native changes required to reconstruct this dirty build, not just this checkpoint.
They omit Git metadata, installed dependencies, ignored operational files,
chain data and other repositories. They are local diagnostic artifacts, not
publication or permission to overwrite a checkout. Input versions, exact build
environment and source correspondence remain required for reproduction.

Final documentation checks (`r1a-doc-checks.json`): 212 relative file references
resolve, no missing targets; both repositories' whitespace checks exit 0 and
AGENTS/CLAUDE remain byte-identical. Protected hashes, exact two-doc GUI handoff
hashes and the 173-path current dirty inventory are recorded. This is a local
file check, not proof that uncommitted GitHub links are published. Gate logs and
outcomes are indexed by `r1a-command-results.json`; failed/earlier attempts remain.

## Remaining gates and rollback

R1 still needs compatible aggregate accounting/admission for service results,
JSON/protobuf/serialization and pending output, including overload and already-
admitted mutation semantics. No Mac-derived VPS policy is selected. No new
whole-node memory cap, historical parity or full-audit closure is claimed.

Functioning integrated ASan/TSan, eligible authenticated/pinned full-history
source plus immutable successors/reference checkpoints and an authorized isolated
Ubuntu/x86_64/ext4 whole-machine environment are missing prerequisites. Docker's
local daemon socket is absent; applicable container smoke has not executed.
The historical range before 30,504,202 through beyond 32,789,378 has not executed
in this checkpoint. LR5's long full-server soak remains separate.

Rollback is a reviewed source/rebuild change, not data migration or deletion.
Retain the write-borrow ownership invariant; never reset a response while an
outstanding async write still uses it. Preserve prior user changes and re-run
equivalent lifecycle/compatibility gates. The parent LR2 goal remains active.

**Every applicable R1/LR2 Definition of Done item proven: NO.**
