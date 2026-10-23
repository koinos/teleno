# Finite M1 receipt-output repair: C11/C12

Status: **closed unqualified at the original 2026-10-21 00:26:31.428768 UTC
deadline; C11 passed 6/8 component invocations, C12 expired unused; M1 remains
open**. Closure was reconciled on 2026-10-21 at 02:49:36.876118 UTC from the
preserved test results; no later test or build was launched in this attempt.

The current [memory-optimization objective](issue-9-m1-aggregate-goal.md#current-objective--clarified-by-the-operator-2026-10-21)
states the purpose and unchanged acceptance. This closed attempt does not
restart when that wording changes.

The operator's later 2026-10-21 instruction removes overall execution-time
limits from future M1 work; see the
[current execution controls](issue-9-m1-aggregate-goal.md#execution-controls-and-completion--no-overall-time-limit).
The fixed windows below remain historical evidence for this closed attempt.

## Terminal result

C11 froze at **2026-10-20 22:09:33.956325 UTC**. Its 455-file source archive
SHA-256 is `95b2914962e9127b18bbc88b4fe535cd69dcc96c3465d46cbca7554204e36bd3`.
The exact component binaries were compiled on the authorized build-only host
and executed on OVH. No whole-node C11 candidate was built.

| OVH component invocation | Result |
| --- | --- |
| HTTP receipt capacity/refusal/retry | Pass |
| HTTP error output ownership | Pass |
| HTTP error capacity/refusal | Pass |
| HTTP successful receipt output ownership | Pass |
| HTTP concurrent successful output | Fail: timed out waiting for two bodies |
| HTTP enabled/disabled wire roundtrip | Pass |
| HTTP post-acceptance allocation failure | Fail: SIGABRT, exit 134 |
| gRPC output ownership and existing component regressions | Pass |

The concurrent fixture uses 256 MiB of policy credit while two conservative
conversion-workspace reservations can overlap. Its two-success assumption
requires a separate success/refusal analysis; do not remove capacity-refusal
coverage or interpret the fixture's reserved bytes as physical RSS.

A diagnostic-only test binary preserves the C11 product object and adds a
stack capture at the injected allocation failure. Its single OVH execution
reproduces exit 134: `std::vector<nlohmann::json>::reserve` is reached from
`json_value::destroy` during `process_single_request` after the fake adapter's
acceptance effect. The JSON destructor is `noexcept`, so the session's outer
catch cannot recover. This is an unresolved correctness failure, not evidence
of reduced memory use or of a physical leak. The diagnostic-only source and
trace are retained on My Passport; its instrumentation is removed from the
active test source after closure.

Exact component ELF SHA-256 values:

- JSON: `0612dda6ecc73b3bab853b5023b574b6f34f675db5a63ca57f5809e2c3a5defe`.
- gRPC: `7ee35f7278a8c6f3b47bee31de37a862cf4b8a23ebdb072da86b1605b7149adb`.
- Diagnostic-only JSON: `0e6495676e5a59fd66896017fc37021016cb83def4a10f8ccbae11d249af2584`.

Evidence is in `evidence/ovh-c11-components-control.json`,
`evidence/ovh-c11-components/`, `evidence/ovh-c11-post-acceptance-trace/`
and the immutable source/build manifests in the owned run below. The last
diagnostic execution ended at **2026-10-20 22:19:54.244462 UTC**. At closure,
a read-only OVH check finds no native test observer or running attempt unit.
The complete normal suite, sanitizer controls/suites, final native CLI,
paired measurements and whole-node qualification were not completed for C11.
The historical development snapshots below retain their original context;
pending/running wording there is superseded by this terminal result.

## Exclusive test host — operator amendment, 2026-10-20

From this amendment onward, **all executable M1 tests run only on the new
isolated OVH VPS**. This includes development reproductions, unit/regression
tests, failure injection, ASan/UBSan/TSan and their controls, controlled-chain
fixtures, paired measurements, native version/help checks and whole-node
qualification. Do not execute these tests on the Mac, miner or another host.

My Passport and the authorized Linux builder may store inputs, prepare
artifacts and compile binaries without running the resulting tests there.
Static source/document checks may remain local. Keep generated evidence on
mounted My Passport and transfer only the required artifacts into owned OVH
directories. Preserve earlier builder/Mac results as historical evidence;
they do not satisfy this new host restriction or qualify the final candidate.

The running frozen-C9 OVH sequence keeps its exact executable, original
deadline and counters. Schedule later test execution in separate controlled
phases so it does not compete with that sequence or invalidate its resource
measurements. The Hetzner producer remains outside scope. No deadline,
candidate allowance, resource limit or recovery reserve changes.

The operator's standing 2026-10-20 authorization covers necessary additional
finite M1 repairs without touching the operational producer. This separate
attempt follows two reproduced unchanged-C9 findings: raw-request metric
overcount and execution-receipt HTTP bodies that outlive their M1 attribution.
See the [C9 diagnostic report](../validation/low-resource-lr2-m1-isolated-vps.md).

## Fixed controls and preservation

- Actual start: **2026-10-20 16:26:31.428768 UTC**.
- Hard deadline: **2026-10-21 00:26:31.428768 UTC**; do not extend it.
- At most **two frozen implementation candidates, C11 and C12**.
- The unused C10 slot in the closed C9/C10 attempt remains unused; this new
  allowance cannot reset any prior time, failure, evidence or counter.
- Owned local artifacts, inputs and evidence:
  `/Volumes/My Passport/teleno-m1-receipt-output-repair-20261020-zxyqrmy1/`.
- Durable start/deadline/candidate record: `control.json` in that root.
- Compilation may use a distinct owned directory on the already authorized
  builder; executable tests use only the new isolated OVH VPS. Existing
  operational data and services are untouched.

Frozen C9 remains the baseline, source archive SHA-256
`40c28493e7ff0c2985d85f2ce1a278c47e209e5e23d16af1bbe2641070eab9d5`.
The simultaneous [C9 physical-VPS sequence](issue-9-m1-isolated-vps-qualification.md)
keeps its original **15:19:53.510265–23:19:53.510265 UTC** interval, executable,
configuration and at-most-two-observer-start ceiling. This repair changes
neither that running binary nor its controls. C9 results cannot qualify C11/C12.

No operational deployment, producer action, mainnet submission, signing, funds,
pool/key changes, commit, push or release is included. A later exact-candidate
test-host phase needs its own recorded finite controls under the existing
isolated-VPS authorization; do not alter the running C9 test to obtain it.

## Required implementation and proof

The complete [M1 acceptance](issue-9-m1-aggregate-goal.md#acceptance) remains in
force. The new findings are concrete entry points, not a smaller replacement
objective. Finish all applicable ownership/admission requirements or report
precisely what remains unproved.

1. Correct raw-request attribution without changing conservative total bytes
   reserved for admission. Distinguish actual allocator requests, supplementary
   policy, retained ownership, allocator heap and physical RSS. Preserve the
   original C9 metrics with their original definition.
2. Retain M1 ownership for native transaction receipts through JSON/protobuf
   conversion, individual/mixed HTTP bodies, asynchronous writes, cancellation,
   concurrent callers and final-owner release. Inventory the analogous gRPC
   boundary and all applicable M1 output paths before claiming coverage.
3. Reserve before allocation/publication. A response-capacity failure must not
   lose, discard or falsely invalidate already accepted transactions. Admission
   refusal must remain local/retryable and preserve correct later retry. Test
   before/after-effect failures explicitly; do not hide missing admission behind
   unbounded quota exemption or a discarded/empty response.
4. Preserve fork/block/LIB/expiry/persistent behavior, complete canonical
   history, every selected block/receipt, consensus, wire bytes and metering.
5. Use the unchanged-C9 clean and known-gap controls first. Add meaningful
   regressions for large receipts, single/mixed/concurrent responses, retained
   caller ownership, capacity boundaries, allocation failures, stop/cancellation
   and successful retry. Preserve every initial harness/build failure.
6. Freeze each selected candidate before qualification. Run the complete
   normal Linux suite on OVH retaining all 74 C9 entries and new regressions, exact
   native version/help, focused ASan/UBSan and supported default TSan with
   functioning clean/known-fault controls. Disclose uninstrumented dependencies
   and all remaining integrated-diagnostics gaps.
7. Use at least five paired equivalent samples for any claimed improvement;
   separate allocation counters, accounted/retained bytes, RSS, CPU, I/O and
   latency. A reporting correction is not a physical-memory saving.

Keep the two-vCPU, four-decimal-GB RAM/no-swap, strict 40-decimal-GB disk and
`max(8 GiB, DB bytes / 5)` recovery-reserve target unchanged. Whole-node steady
RSS <=2 GiB, peak <=2.5 GiB and concurrent MemAvailable >=768 MiB require
actual final-candidate physical-host evidence, including representative mixed
transaction/output work and recovery. The read-only C9 observation does not
prove those outcomes for a new implementation.

## Development evidence so far

The raw-request correction moves the obsolete wrapper/alignment allowance to
supplementary reservation while keeping its previous threshold arithmetic and
total accounted admission bytes. A new direct-allocation regression compiled
against unchanged C9 fails with the expected overcount diagnostic. The same
Linux test compiled against the development correction passes its request,
alignment, boundary, allocation-failure, final-owner, transfer and concurrency
checks. These builder development tests predate the exclusive-OVH amendment;
they are historical evidence, not frozen-candidate qualification. Repeat
applicable checks for the final candidate on OVH only.
An additional Linux comparison covers 13 ordinary/over-aligned allocation
shapes, including the 64-KiB policy transition. Every old/new total accounted
byte delta is identical and every final release returns its credits. Actual
request deltas change from 100 to 69 bytes and 783 to 512 bytes for the two
reported examples, without relaxing admission. Exact commands, binaries and
results are in `evidence/dev-metric-linux-result.json` and
`evidence/dev-metric-capacity-linux-result.json` under the owned run.

The chain-receipt HTTP finding remains reproduced and **unqualified**. Its
synthetic interface fixture has no controller, ledger, keys or external
network; it links 46 SHA-256-verified inputs from unchanged C9. Single and
mixed replies retain 4,538,368 and 4,546,560 HTTP-capacity bytes while their
M1 counters have already returned to 240 bytes. The mempool-output clean
control retains appropriate credit. Do not reinterpret this as a native
transaction-validity failure or a demonstrated physical heap leak.

Development now adds a synchronous native preparation callback after receipt
creation and before accepted-work publication. The optional HTTP path builds
the exact success envelope/body and reserves any mixed-body contribution before
admission. The final string or contribution lease survives until asynchronous
write completion. Unsupported adapters fail before executing a mutating call.
This preserves the default path when the aggregate policy is disabled.

New regressions exercise single/mixed/concurrent receipt ownership, cancelled
writes, pre-preparation capacity refusal, post-preparation admission refusal,
allocation failure, later retry, byte-encoding preservation and a following
canonical RPC under a full optional quota. The signed native-controller test
also checks the actual callback/admission ordering and receipt/root parity.
The implementation is **not yet executed or qualified**. The HTTP component
build now passes on the authorized builder without test execution. Both the
unchanged-C9 control and development libraries link; initial relocated-include
harness failures are preserved. The restored pinned C9 generated/dependency
include paths correct the harness without changing implementation source.
A later post-acceptance allocation-failure regression also requires compilation
and execution in the final-candidate suite. No tests run on the builder.

An additional diagnostic against the unchanged C9 gRPC library/SDK also builds
without execution. It will measure serialization's native `malloc` requests
and M1 attribution at the real gateway handoff, with an explicit counter
observability control. It does not yet prove a gRPC gap or a repair. Source,
header, linked-input and binary identities are retained privately. All probe
execution waits for the separate C9 OVH sequence to release its resource
measurements. The gRPC boundary and all other applicable M1 conditions still
require proof.

## Finite OVH component execution

An owned probe queue waits for a terminal C9 coordinator result, its actual
service to stop and both known observer PIDs to disappear. A state file alone
does not prove closure. It changes no C9 executable/configuration/counter,
database or service. Its fixed deadline is this run's original
**2026-10-21 00:26:31.428768 UTC** deadline.

The queue permits at most six component executions: the unchanged-C9 HTTP
gap/clean control, development HTTP ownership, wire roundtrip, capacity/retry,
concurrent output and the unchanged-C9 gRPC serialization diagnostic. It
executes no tests on the builder. Every OVH probe uses a private network
namespace, two-vCPU CPU quota, 512-MiB cgroup limit, zero swap and at most
45 seconds of native execution. Host MemAvailable must remain >=768 MiB with
no OOM/swap, and the strict whole-server budget still includes the 8-GiB
recovery reserve. One hash-verified duplicate ELF is staged at a time.

These fixtures never open a chain database and write only bounded diagnostic
output. Their additional early disk margin is 64 MiB after accounting for the
staged ELF; this does not change C9's separate 1-GiB early-stop guard or the
40,000,000,000-byte acceptance budget. Preserve failures without retrying
them automatically. Independent functional cases may continue after a
functional failure; an environment/resource failure stops the queue.
Collect every result on mounted My Passport before retiring the now-closed
remote ELF duplicate. The queue control is
`evidence/ovh-component-probes-control.json` in this run's owned root.

These are development diagnostics. They do not qualify the final source,
new post-acceptance fault fixture, complete native/sanitizer suite or whole
node, and do not consume or reset a frozen implementation-candidate slot.

## Additional error-boundary reproduction

Static review also identifies a possible escaped error-body/status owner.
Native contract errors can carry caller-controlled text; success-response
coverage alone cannot establish the complete output boundary. A separate
synthetic diagnostic compiles once and links against unchanged-C9 and the
existing development HTTP object. All three compiler/link invocations pass;
no test executes on the builder and no new implementation pass is consumed.
Its source SHA-256 is
`5c7caeff09ba7936f2d801f112fbd705131b8a80f0819deb3e5cbfb99be5f86d`.
The control/development ELFs are
`2bac88b56019c04120c25603360bd779a8a432b6923f1438c0b01ebb978acd2b` /
`8bdc4352d6189a4d039eb1446b80999f814c7f0867990c06cfc86dcbd176ca3b`.

At most five error-boundary executions may run on OVH after both C9 and the
original six-component queue close: old/development single/mixed HTTP and
unchanged-C9 gRPC error status. They retain the same original repair deadline,
resource guards and no-retry behavior. Their separate control is
`evidence/ovh-error-probes-control.json`; the old queue is unchanged. This is
reproduction work, not a frozen implementation or qualification result.
Inspect exact error preservation, held attribution and the explicit counter
control before asserting a defect. Preserve any functional/resource failure.

Stop at success or exhausted limits. Mark M1 complete only after auditing every
applicable condition for the selected final implementation; otherwise leave
M1, issue 9 and the independently incomplete D1/P2/R1/V1/S1/Q1/H1 rows open.
