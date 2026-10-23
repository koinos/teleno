# Legacy applicability of Teleno findings

The final Teleno-only [R15 qualification](../validation/low-resource-lr2-m1-r15.md)
completes its M1 gate on 2026-10-23: normal/focused sanitizer checks, paired
measurements, controlled-chain creation/reopen and exact full-history observer
restart/recovery pass. This result does not reproduce or qualify a legacy
defect. The legacy source comparisons and missing binary evidence below
retain their stated scope.

Earlier source-comparison/checkpoint record, updated 2026-10-22.
**No failure in this document has
been reproduced against a legacy service binary.** Teleno's finite C3/C4 repair
attempt is closed unqualified; the separate C5 follow-up passes scoped default
TSan after imposing one fork-lock order but fails its complete controlled load.
C6 passes its normal suite, two controlled loads and focused ASan/UBSan/TSan
checks. Frozen C7 passes 73/73 normal Linux tests and 21/21 focused
ASan/UBSan entries plus cleanup, but its focused TSan run fails 2/21 entries
on JSON-RPC session lifetime. Frozen C8 passes 73/73 normal Linux, 21/21
focused ASan/UBSan and 21/21 focused TSan entries plus both cleanup checks,
and a controlled 100-block/reopen profile. Its five-pair comparison records
unresolved CPU/latency slowdown; the attempt closed unqualified. Broader M1
qualification remains pending. Frozen C9 corrects two reproduced Teleno
shorter-predecessor nonce/count read overflows and passes 74/74 normal Linux,
22/22 focused ASan/UBSan and 22/22 focused TSan checks, both cleanup checks,
and a controlled production/reopen profile. Five paired C8/C9 controlled
samples and all ten reopens pass without an attributable performance change;
the attempt closed unqualified for M1. No legacy service binary has been
tested for this read pattern.
This register remains open for evidence updates.
A suggested reproduction is not a completed test.

Later R10 Teleno qualification passes its exact Linux native suite, focused
sanitizers and a one-hour isolated mainnet observer/reopen; it does not close
M1. The 2026-10-22 R11 development move of a decoded pending transaction into
its caller-owned vector passes Teleno's focused normal allocation-failure and
wire-parity regression. Five paired Teleno OVH component samples show fewer
allocation requests but a higher peak RSS in every pair, so no physical-memory
saving is claimed. The legacy source's corresponding copy is already recorded
as M1-L1; neither the R11 result nor its source pattern proves a legacy defect
or benefit. An exact legacy dependency/binary test remains necessary.

The later exact R13 Teleno native suite and focused sanitizers pass on OVH,
and its controlled 100-block/12,800-transaction workload reopens with the
same state and archive hashes. R13's block-event copy removal is specific to
Teleno's in-process dispatch; the legacy service boundary still requires wire
transport. No legacy binary has been tested, and R13 does not establish a
legacy memory defect or saving.

The later SC5 normal-suite storage failure is specific to Teleno's synthetic
diagnostic driver retaining completed restore cases. Its proposed opt-in
fixture retirement is not a legacy node defect or an operational database
cleanup policy. No legacy executable or service has been tested for it.

This register tracks problems being improved in Teleno that might also affect
the microservices node. It separates shared source/dependencies from behavior
introduced by Teleno's single-process architecture. The
[M1 goal](issue-9-m1-aggregate-goal.md#additional-deliverable-legacy-applicability-analysis)
defines scope; the closed first-attempt
[M1 report](../validation/low-resource-lr2-aggregate-memory.md) retains its failures,
and the separate [C3/C4 repair report](../validation/low-resource-lr2-m1-repair.md)
records the frozen candidate identities and qualification status.

## Evidence and comparison boundary

- Teleno source checkpoint: `d424264b24595786fdba86d9eddbcbf1635284b6`, with
  uncommitted M1 work and separate finite allowances. Frozen C4 passes 68/68
  normal Linux tests plus 16/16 scoped ASan/UBSan entries, but its default TSan
  reports a fork-lock warning. Frozen C5 imposes one key order for those locks
  and passes 68/68 normal, 16/16 scoped ASan/UBSan and 15/15 scoped default
  TSan after a recorded generated-CTest timeout adjustment. C5's complete
  controlled load is unqualified because of local admission refusal and a
  separate time limit. Frozen C6 changes only the Teleno owner-table bucket
  count, passes 68/68 normal checks and two controlled nonempty/reopen profiles;
  its final focused sanitizers pass, while target-VPS physical margins remain open.
  Neither is a released M1 candidate.
- Inspected public legacy mempool checkout:
  `koinos/koinos-mempool@2aa415fc75fd40038509f06e66688ab505cf8df8`.
  Its `CMakeLists.txt` declares version 1.5.0 and requires Protobuf and
  `koinos_proto`; installed image/library identities are unknown.
- Inspected legacy Hunter checkout:
  `cpp-pm/hunter@cfe363388b042066dac06f7aff54fdff4e67bf9c`.
  `cmake/configs/default.cmake` selects Protobuf **3.19.4-p0**, whereas the
  inspected Teleno SDK integration uses **3.17.3**. A Hunter default is not proof
  of the package used by an actual legacy build.
- Inspected public legacy chain checkout:
  `koinos/koinos-chain@dfb544df23e8e90241256cb7e1195e6829890d11`.
  Its source is a comparison input, not an identified production binary.
- Teleno's matching generated Koinos message input is
  `koinos/koinos-proto-cpp@a24bb5ff91b2f245863bddb02496cc4831ad96c2`.
  The legacy generated archive, compiler, allocator, build flags and executable
  have not been identified. Do not assume their equivalence.
- No legacy service has been changed or operated for this comparison. No
  operational database or live producer is used by these proposed tests.

## Findings register

| Finding | Teleno evidence and current repair | Legacy applicability and missing evidence |
| --- | --- | --- |
| M1-L1: decoded query copy failure leaks allocations | The unchanged starting implementation reproduces a 1,008-byte leak in 28 allocations during a bounded allocation-failure sweep. Frozen C4 repairs generated copy/repeated-pointer ownership and passes the formerly leaking ASan/UBSan sweep with leak detection. C4 remains unqualified because of the separate default-TSan warning and missing representative profile. | **Source-supported hypothesis.** Legacy `mempool.cpp:316` and `:360` copies `pending_transaction()` from a decoded temporary into a result vector, matching the relevant ownership pattern. Its generated constructors and protobuf runtime must be inspected and tested; a different dependency version may change the result. |
| M1-L2: generated oneof mutation publishes a case before allocation succeeds | A baseline Teleno fixture switches an existing `upload_contract` operation to `call_contract`, fails its next allocation, and reports an ASan crash on destruction. Frozen C4 allocates the new message before clearing/publishing the previous case; its oneof failure/retry regression passes under ASan/UBSan. | **Dependency hypothesis.** Applicability requires the legacy generated `protocol.pb.h` to contain the same publish-before-allocation setter. The inspected mempool source alone does not establish this. No legacy crash is claimed. |
| M1-L3: eager result-vector workspace exceeds useful query work | Legacy source inspection confirms `reserve(limit)` and `reserve(ids.size())`; Teleno is replacing unconditional result reservation with growth for actual results and lifetime-owned admission. | **Shared source pattern confirmed; resource impact unmeasured in legacy.** Effective RPC limits, retained caller counts, allocator capacity and workload determine whether this becomes a problem. There is no demonstrated legacy memory ceiling or savings figure. |
| M1-L4: nonce/RC/read workspace and returned protobuf/serialized owners outlive narrow admission counters | Teleno's C2 baseline reproduces an oversized synthetic nonce-read and retained-response attribution gap. With the later allocation-owner path, frozen C8 attributes 5,248,992 peak bytes during that nonce read and retains credit while 256 native responses remain held; credit returns to baseline after their release. These narrow C8 probes do not cover every RC/count read, serialization or caller path. | **Architecture-sensitive hypothesis.** Legacy has corresponding nonce/RC queries and protobuf responses, but AMQP serialization and separate process ownership differ. Teleno's shared admission-domain implementation cannot be copied into legacy without a separate design. Measure mempool, broker and caller memory independently. |
| M1-L5: head/mempool snapshot disagreement | The previously closed producer commissioning evidence records `cannot retrieve pending transactions from an unknown block`. Teleno's producer recovery is a separate repair from the current leak. | **Shared refusal, unproved shared defect.** Legacy `mempool.cpp:299` and `:342` contains the same defensive error. The string alone cannot prove a bug: service ordering, retries and block/head sampling must be compared. A legacy producer may already tolerate the interleaving. |
| M1-L6: aggregate transaction-work pressure across component boundaries | Teleno joins execution, mempool, native RPC and returned values in one process and is qualifying one lifetime accounting domain. Canonical processing remains quota-exempt and observable. | **Teleno-specific integration until demonstrated otherwise.** Legacy separates these processes but can still accumulate broker queues and in-flight copies. This is a measurement question, not proof that either architecture meets a whole-node RSS target. |
| M1-L7: unknown-field deep-copy failure aliases source ownership | A Linux ASan development fault sweep of a generated transaction with a nested unknown group reproduced a use-after-free: Protobuf 3.17.3 published a shallow field copy before its deep copy completed. Frozen C4's build-owned repair passes its dedicated unknown-field regression and aggregate sweep under ASan/UBSan; broader qualification remains pending. | **Dependency-version hypothesis.** The inspected legacy Hunter default selects Protobuf 3.19.4-p0, and the actual service binary is unidentified. Inspect its `UnknownFieldSet` copy path and generated messages before attempting the same isolated fault sweep; do not infer exposure from Teleno's 3.17.3 failure. |
| V1-D-L8: unaligned WASM result-count address reaches typed native stores | Teleno's [V1-D report](../validation/low-resource-lr2-host-memory.md) reproduces UBSan misaligned stores in its original host writers and records the source correction, focused tests and broader qualification limits. This finding predates the current M1 repair. | **Matching source pattern, legacy execution untested.** At the inspected `koinos-chain` revision, Fizzy resolves the in-bounds result address as `uint32_t*` in both callbacks (`fizzy_vm_backend.cpp:231,281`); `thunk_dispatcher.hpp:306,333` and `host_api.cpp:64,69,83,102,163,168` dereference it. The actual legacy binary, compiler flags and sanitizer result are unknown. Do not call this a reproduced legacy defect until an isolated legacy build reaches the typed store with an unaligned address. |
| M1-L9: global `new` prefix conflicts with RocksDB's size query | C3's normal RocksDB test crashed in glibc `malloc_usable_size` after RocksDB received a pointer displaced by Teleno's accounting prefix. The GDB trace is preserved in the M1 repair run. Frozen C4 returns allocator-base pointers and keeps M1 leases in a side table; its normal and scoped ASan/UBSan suites pass, but its overall M1 qualification is incomplete. | **No established legacy applicability.** This failure depends on Teleno's new process-wide allocation replacement, not on RocksDB alone. No equivalent replacement has been established in the inspected legacy chain or its exact installed binary. Check linked allocation symbols and the RocksDB build before proposing a legacy test or patch. |
| M1-L10: partial multi-fork admission after a later commit fails | Teleno's M1-C work prepares all fork writes before `commit_all_writes`; the frozen C4 test covers refusal, retained accepted work and retry. C4's simultaneous fork-lock acquisition triggers a default TSan warning. Frozen C5 acquires those locks in one key order; its scoped default TSan suite passes after a recorded harness timeout adjustment. | **Source-supported legacy hypothesis, not a reproduced failure.** At the inspected legacy `mempool.cpp:736-740`, admission commits each fork under one fork mutex and releases that mutex before moving to the next fork. A failure in a later commit could leave earlier forks changed; whether the exact legacy state-db dependency can fail there and what it publishes must be tested. The Teleno-only multi-lock TSan inversion is not established for this legacy revision because admission, removal and prune hold only one fork mutex at a time. |
| M1-L11: large mempool HTTP reply outlives narrow service accounting | The C6 localhost baseline holds a 9,650,176-byte JSON-RPC response while its M1 domain attributes 977,851 bytes. Frozen C7 retains a result/body contribution through asynchronous write but its focused TSan run finds a JSON-RPC session lifetime race. Frozen C8 uses a concrete strand executor and passes 73/73 normal Linux, 21/21 focused TSan and 21/21 focused ASan/UBSan entries, each sanitizer with separate cleanup; its controlled production/reopen profile passes. Five paired C6/C8 samples show higher CPU and latency with nearly unchanged RSS; cause remains unresolved. See the [output report](../validation/low-resource-lr2-m1-output-ownership.md). | **No established legacy applicability.** Teleno's shared M1 budget and Beast session are monolith-specific. The exact legacy JSON-RPC source and deployed binary have not been inspected for this finding. A legacy service may retain large HTTP output or broker copies, but it cannot be inferred from Teleno's counter gap or TSan trace. Identify the exact service source, runtime, response ownership and limits before proposing an isolated slow-reader test. |
| M1-L12: nonce lookup reads past a shorter predecessor key | Frozen C8 has two three-iterator `std::mismatch(account.begin(), account.end(), key.begin())` calls without first checking the predecessor key length. In an isolated Linux ASan fixture, an admitted transaction leaves a shorter nonce-index key; a longer account query matching its prefix and terminating NUL causes heap-buffer-overflow at Teleno `mempool.cpp:609` and `:692`. The ordinary-account control passes. Full traces and both initial non-triggering fixture results are preserved under `/Volumes/My Passport/teleno-m1-nonce-read-20261020-l2q4z85b/evidence/`. Frozen C9 checks key length first and passes 74/74 normal, 22/22 focused ASan/UBSan, 22/22 focused TSan, a controlled production/reopen profile and five paired samples; broader M1 qualification remains open. | **Matching legacy source pattern, legacy execution untested.** Inspected legacy `koinos-mempool` source commit `2aa415fc75fd40038509f06e66688ab505cf8df8`, `mempool.cpp` SHA-256 `7483a26a125adcb61566574acc92448296397da4a40630ba523cbe3390a26b1c`, has the same calls at lines 535 and 592. The deployed legacy binary and its exact state-db dependency are unknown. Do not infer a live legacy failure without an isolated exact-version ASan reproduction. |
| M1-L13: reversible-fork memory during sustained transaction work | In R12's isolated 100-block/12,800-transaction controlled workload, Teleno's M1 attribution rose to about 497 MB by block 60 and stayed near that value through block 100 while head 100/LIB 40 retained 60 reversible blocks. Peak attributed bytes were 564,747,832 and peak process RSS was 734,810,112 B, both within the configured test limits. This plateau is a profile input, not proof of a leak or a physical saving. | **Workload hypothesis only.** The legacy mempool also handles block/fork transitions, but its process boundaries, state-db version, transaction distribution and allocator differ. Reproduce the same controlled head/LIB and transaction mix on an identified isolated legacy build, measuring live fork owners, heap and RSS over more than one reversible window and after restart. No legacy binary has been tested for this profile. |

The C5 CPU regression in Teleno's new process-wide allocation-owner table is
**not transferred to legacy by this comparison**. Three C5 stack samples reach
its linear delete lookup, and five paired component samples show C6's larger
table shortens that path. The inspected legacy mempool has no established
equivalent of this Teleno-specific side table. A legacy service may have other
allocation costs, but this result does not identify one.

The 2026-10-20 [C9 raw-request diagnostic](../validation/low-resource-lr2-m1-isolated-vps.md#new-reproduced-measurement-defect-raw-request-counter)
also concerns Teleno's own accounting layer: its custom allocator reports an
obsolete raw-wrapper allowance as requested bytes. A header-only Linux
control observes 69/100 actual/reported bytes for an ordinary owner and
512/783 for an over-aligned owner; credits release in both cases. There is
**no established legacy applicability**. No equivalent accounting layer has
been identified in the inspected legacy source, and no legacy binary was
tested. This measurement defect must not be described as a legacy memory leak.

A separate unchanged-C9 synthetic `IChain` diagnostic reproduces a
transaction-receipt HTTP attribution gap: 4.5-MB single/mixed bodies remain
held after the native M1 count returns to its 240-byte bootstrap. The mempool
HTTP ownership control passes. This is a Teleno JSON-RPC/domain handoff finding,
not a demonstrated legacy service defect; no controller, ledger, mainnet
submission or legacy binary was used. See the
[diagnostic and exact linked identities](../validation/low-resource-lr2-m1-isolated-vps.md#new-reproduced-ownership-gap-transaction-receipt-http-output).
Legacy applicability requires its own RPC/broker/serialization owner inventory
and isolated exact-version control rather than copying Teleno's shared-budget
result across process boundaries.

The [C11 JSON cleanup failure](issue-9-m1-receipt-output-repair.md#terminal-result)
is also **not a reproduced legacy defect**. It occurs in Teleno's native
`nlohmann::json` HTTP request/response destruction when an allocation is failed
after the synthetic chain adapter accepts a call. The current development
cleanup helper changes Teleno-owned trees only. No matching JSON-RPC language,
library/version, generated runtime or service binary has been identified for
the legacy gateway in this investigation. Inspect those exact inputs before
proposing an analogous isolated test; do not infer shared applicability from
the Koinos RPC method name or from the presence of C++ chain services.

## Bounded isolated reproduction plans

All plans first pin source, generated headers/runtime, compiler, allocator, input
and binary hashes. Use an owned scratch directory, finite process timeout and
memory limit, no swap for the test process, and no operational database, chain
submission or network-facing service. Run working sanitizer clean and known-fault
controls and disclose uninstrumented dependencies. Preserve every failing result.

1. **M1-L1, copy cleanup:** build a standalone fixture against the exact legacy
   message/runtime inputs. Create a representative transaction, pending record,
   repeated fields and unknown fields. Fail successive C++ allocation ordinals
   during the legacy decode-and-copy query sequence, up to a declared finite
   bound. Verify the original input remains unchanged, destruction is safe,
   leak checks pass and a later uninjected retry returns identical wire bytes.
   The clean control uses the same input without injected failure. A Teleno-only
   fixture does not establish the legacy result.
2. **M1-L2, oneof cleanup:** inspect the exact generated setter first. Retain an
   `upload_contract` payload, fail the next allocation when requesting
   `mutable_call_contract()`, then destroy the message. Check for invalid access,
   leaks and valid object state. A corrected setter should preserve the original
   case on allocation refusal and allow a later correct switch. Keep the fault
   injection in the fixture; never inject failure into a running service.
3. **M1-L3, result workspace:** use a small synthetic database with zero, one and
   several records. Compare legal configured query limits against actual results
   and concurrent callers. Record vector capacity, allocator requests, retained
   output, RSS and latency, including refused requests. Do not assume the
   service accepts an unbounded client limit without inspecting its entrypoint.
4. **M1-L4/L6, complete request lifetime:** retain identical responses before and
   after serialization and after the originating request ends. Measure the
   mempool process, transport/broker, execution process and caller separately;
   release the final owners and check reclamation. Use at least five equivalent
   paired samples for an improvement claim. Distinguish malloc/allocator arenas,
   physical RSS, counters, payload sizes and process-global caches.
5. **M1-L5, head ordering:** in a deterministic service-level harness, pause
   mempool block application while the producer observes a newer head. Inspect
   its bounded retry and stale-candidate rejection. Advance the mempool and
   verify transactions remain present and a fresh candidate succeeds. Compare
   the legacy behavior before proposing a patch; do not infer failure from the
   expected unknown-block response.
6. **M1-L7, unknown-field copy:** after identifying the exact legacy protobuf
   source and generated message archive, copy a message containing a nested
   unknown group while failing successive allocation ordinals. Check that the
   original wire bytes remain unchanged, both objects destroy safely, leak and
   use-after-free checks pass, and a later copy succeeds. Record a negative
   result for the tested dependency version if its implementation differs.
7. **V1-D-L8, host result-count alignment:** compile the exact inspected legacy
   chain writers and Fizzy callback with a working UBSan clean and known-fault
   control. Use a one-page WASM fixture with an in-bounds unaligned four-byte
   result address, an aligned control and an out-of-bounds rejection control.
   Record whether the real legacy callback reaches a typed misaligned store,
   preserves result bytes and metering, and allows a subsequent valid call.
   Do not infer deployed exposure from the matching source pattern alone.
8. **M1-L9, allocator interoperability:** first identify the exact legacy
   executable and whether it replaces global C++ allocation functions. If it
   does, use a standalone clean control that calls `malloc_usable_size` on a
   normal `new` pointer and a bounded RocksDB cache fixture under ASan; compare
   the actual pointer base and deletion path. If it uses an ordinary
   malloc-compatible allocator, record this Teleno-specific finding as not
   reproduced for that binary. Never infer a legacy problem merely because
   RocksDB is shared.
9. **M1-L10, multi-fork commit failure:** pin the exact legacy state-db source
   and executable first. In an isolated two-fork fixture, use a deterministic
   failpoint or bounded allocation fault at the second fork's commit after the
   first fork has committed. Compare both forks' transaction IDs, nonce/RC
   reservations and a later retry with an unfaulted clean control. Record
   whether the failure is reachable and whether state remains consistent; do
   not infer a production legacy failure from source ordering alone. Separately
   run default TSan with clean and known-fault controls for that exact binary if
   lock-order behavior is being compared.
10. **M1-L11, HTTP output lifetime:** identify the actual legacy JSON-RPC
    source, binary, HTTP framework and any configured output limit. With a
    clean small-reply control and a bounded large mempool query, pause one
    localhost client before reading its response. Measure each legacy service
    process, broker queue and final HTTP body independently until completion
    and cancellation; require exact response bytes and later service recovery.
    A large retained body alone is expected output ownership, not a defect.
    Call it a failure only if a stated admission or release invariant is
    demonstrably violated in the exact legacy implementation.
11. **M1-L12, shorter predecessor key:** first reproduce or disprove the
    suspected range violation in an isolated Teleno mempool with a populated
    nonce index, a longer lexicographically later account query, and ASan plus
    a clean matching-length control. For legacy, pin the exact binary and
    state-db dependency, then use the same bounded local query while recording
    the actual returned predecessor key length. Require a sanitizer report or
    an observable incorrect result before labeling a legacy defect; retain
    unchanged accepted transactions and verify an ordinary retry.

## Adoption and updates

The 2026-10-21 JSON-cleanup continuation also separates two new observations.
Teleno's normal cold-Protobuf fixture refuses lazy global field metadata under
its optional request quota. The proposed correction keeps actual message
allocations admitted while initializing immutable generated descriptors
separately. This depends on Teleno's shared allocation policy; no equivalent
legacy quota layer or legacy failure is established. Separately, a standalone
GCC TSan control cannot retry a once initializer after an exception. That is
an identified diagnostic-runtime limitation, not a reproduced native or legacy
node deadlock. Any legacy comparison must first identify its runtime, exact
Protobuf build and allocation policy. Neither observation establishes a legacy
memory leak. See the [scoped continuation report](../validation/low-resource-lr2-m1-json-cleanup.md#tsan-cold-reflection-investigation).

The continuation also reproduces termination while a stock nlohmann JSON
3.11.2 copy unwinds an allocation refusal: its destructor allocates again.
External Teleno cleanup guards cannot protect a constructor that never returns.
The proposed build-owned destructor backport has passing direct dependency
controls, with SDK/native integration still pending. A legacy comparison must
first identify whether the particular executable actually links this C++ JSON
implementation, its version and its allocation behavior. A Go JSON-RPC service
or a different dependency build does not inherit this finding merely because
it belongs to the same node stack. Reproduce an isolated partial-copy failure
with clean and functioning allocation-fault controls before calling it a
legacy defect. No legacy executable has been tested for this condition.

The actual Teleno HTTP fixture also reproduces a resource exception being
mistaken for an alternate address encoding: the unchanged C++ normalizer sends
different native payer bytes after injected failures. R6/R7 preserve the
exception and their scoped controls verify exact bytes and retry. A legacy
frontend does not inherit this C++ catch behavior automatically; identify its
own implementation, formats and failure model before proposing a reproduction.
No legacy executable has been tested for this condition.

A separate prebuilt-gRPC TSan warning reproduces in a minimal SDK-only service
without Teleno. Rebuilding the exact pinned gRPC source with TSan makes the
clean control pass, detects its deliberately injected callback race, and passes
the identified native gRPC regression. Preserve the earlier warnings as failed
diagnostics with incomplete instrumentation. They establish neither a deployed
legacy race nor a native memory leak. A legacy comparison must identify and
instrument its own linked runtime and retain working clean/fault controls.

For every executed legacy reproduction append its exact identities, finite
limits, clean/fault control results, expected/observed behavior and evidence
location. Promote a hypothesis to reproduced only after a matching legacy test
fails. Record a negative result for the exact tested inputs without generalizing
to all releases. Keep repair development, final candidate qualification, upstream
adoption and deployment separate. This document does not authorize legacy source
changes, publication or any live-node operation.

The later C13 status-path checks reproduce Teleno-specific quota-accounting
gaps and allocation failures at a C++ gRPC error boundary. The pinned SDK can
percent-encode each error byte into three wire bytes; that protocol expansion
alone is not a legacy defect. A legacy comparison must identify its actual
error-string preparation, exception containment, workspace policy and linked
SDK, then inject an isolated copy failure and verify cleanup and retry. No
legacy runtime has been tested, and no deployed legacy crash or memory leak is
established by these Teleno component results.


The C14 HTTP overload diagnostic demonstrates allocations outside Teleno's M1
owner after quota refusal, including copies of a large JSON-RPC ID. This is a
Teleno accounting defect. A legacy frontend may have its own response cloning,
error-workspace and concurrent-output costs, but this must be inspected and
measured in its actual implementation; it does not inherit Teleno's C++ quota
policy. No legacy executable or operational node has been tested for this
hypothesis. The separate SC5 retention change affects Teleno's diagnostic driver,
not the legacy node or a deployed backup database.

The R11/R12 Teleno profiles identify avoidable decoded-transaction and
nonce-key copies. Five paired OVH component processes for each revision show
fewer allocation requests, but no corresponding peak-RSS saving; the measured
query RSS is higher. A legacy mempool or RPC service might make analogous
copies, especially when it decodes records or constructs account/nonce keys,
but its language runtime, ownership and call boundaries differ. Inspect and
profile an exact legacy build with equivalent inputs before proposing a
legacy fix or claiming shared memory savings. No legacy executable was run for
these copy patterns.

R13 removes serialization and reparsing of `block_accepted` between Teleno's
controller and its in-process event bus. The legacy microservices use a real
service boundary for block broadcasts, so their wire serialization cannot be
removed by the same technique. This R13 finding does not establish a legacy
defect. A separate legacy application-memory review could still profile
unnecessary copies on either side of that boundary using identified binaries
and equivalent block inputs.

The later protected-refusal experiment is specific to Teleno's M1 admission
policy. It demonstrates uncapped concurrent refusal attribution with correct
final release, and does not establish a legacy leak. A new Teleno fallback
also exposes a pinned gRPC 1.31.0 synchronous initialization order: cancelling
from the interceptor factory runs before the call is bound. The unchanged
native component passes its refusal case after moving that SDK binding before
the callback. A legacy service would require the same SDK and the same
factory-time cancellation precondition for this finding to apply. No legacy
binary or running service has been tested, and its usual service-boundary
operation is not evidence of this condition.
