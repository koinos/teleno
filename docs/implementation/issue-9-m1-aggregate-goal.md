# Issue 9 M1: optimize Teleno application memory

Status: **M1 complete for exact R15, 2026-10-23.** The final candidate passes 104/104 normal
OVH checks, both CLI checks, 63/63 focused ASan/UBSan and 61/61 default-TSan
cases. Five equivalent pairs per measured workload and exact-source
100-block/12,800-test-transaction creation/reopen pass. Its exact full-history
observer completes 3,600 error-free mixed-read cycles, 31 independent
comparisons, clean restart and owned-process interruption/recovery. Peak RSS
is 419,278,848 B in the observer and 705,712,128 B in the controlled workload;
the target-adjusted host-memory and strict disk/reserve gates pass. Final
owner review and all six closeout-plan gates pass for the identified R15
implementation. Issue 9 and separate R1/P2/V1/S1/D1/Q1/H1 work remain open.
No producer change, commit, push, release or app integration is performed. The
[R15 report](../validation/low-resource-lr2-m1-r15.md) is the current record.

## Earlier qualification checkpoints

Frozen R12 passes 101/101 normal checks,
both native CLI checks, 61/61 focused ASan/UBSan and 59/59 focused
default-TSan invocations on isolated OVH. A separate controlled-chain run
passes 100 blocks and 12,800 locally signed test transactions, followed by a
clean database reopen, within the RAM and disk guards. Its exact full-history
mainnet observer passed a one-hour mixed-read window, clean same-database
reopen and a separate owned-process interruption/recovery test. Five paired
R6/R12 HTTP input/receipt processes show lower R12 RSS for that scoped
workload. R10's earlier observer qualification and all
failed R9/C14 results retain their distinct identities and outcomes.
Complete aggregate ownership, representative memory-policy selection and
whole-node physical-savings attribution remain open. The
[validation report](../validation/low-resource-lr2-m1-json-cleanup.md) records
exact versions and earlier attempts.

Frozen R13 isolates one additional in-process block-notification copy from
R12. Its normal Linux build, 101/101 registered OVH checks, both CLI checks,
focused regression and five paired component samples pass. Focused R13
ASan/UBSan passes 61/61 unique cases and default TSan passes 59/59 unique
cases after preserving the initial CTest timeout and separate harness
failures. The isolated R13 workload passes 100 blocks, 12,800 locally signed
test transactions and a hash-matching reopen. Its exact full-history OVH
observer passes a one-hour mixed-read window, clean same-database reopen and
owned-process interruption/recovery, with independent irreversible agreement
and all resource guards intact. Four observer starts and zero producer starts
are preserved. The exact R13 runtime gates pass; complete aggregate-owner
review, representative policy and physical-memory attribution remain open.
R12's results do not transfer to the new node binary.

The R13 final-source review identifies one concrete unresolved policy edge:
gRPC refusal states and their minimum status workspace are correctly counted
and released, but use protected quota-exempt admission so a full ordinary M1
quota can still report overload. The ordinary quota therefore does not bound
the combined memory of simultaneous refusal controls. One-call and two-call
cancellation tests do not measure that aggregate. Resolve its concurrency
bound or explicitly qualify it as an R1 transport exclusion, and finish the
per-owner size/release inventory before marking M1 complete. This source
finding does not invalidate R13's passing observer or controlled-chain runs.

The continuing refusal follow-up reproduces that growth on OVH and adds an
initially unfrozen bounded refusal allowance. Six small normal checks, the same six
ASan/UBSan cases with three controls, and eight default-TSan cases/controls
pass. The initial real-server exhausted-control path fails because pinned
gRPC binds its synchronous call after factory callbacks. A separately
identified call-binding correction passes four normal and four scoped
ASan/UBSan real-server cases plus three controls. The expanded fallback-fault
fixture also passes all eleven normal/ASan/control invocations: seven
cancellations, exact final credits and a healthy retry in each capacity case.
All thirteen source/archive/configuration recipe checks pass on OVH. The repository contains a pinned build-owned
single-member SDK repair; its Linux component configure/build/link passes
after preserving the initial header-inventory and Debug/Release-selection
failures. Six real-gRPC default-TSan checks now pass with the instrumented
pinned SDK, including clean and intentionally racy callback controls. No new
complete candidate was qualified by those component checks. Frozen R14 then
fails configuration because its assembled build file includes an unrelated
P2P test registration without that separate work's source. Preserve that
failure. R15 excludes only that registration from the qualification snapshot,
leaving the workspace P2P work untouched. R15 source archive is
`3be9aa55f128f649d851ec87ca9dfa3574736eec0bdec9e4e9ebaecbae47995f`;
its complete normal build succeeds with 71 ELFs, 168 identified linked
libraries and node SHA-256
`e825f1951a583590e9173c57be79626635dbb110f750d1258eb34efffa31eef4`.
All 104 registered normal checks and both CLI checks now pass on OVH.
Final-source focused ASan/UBSan passes 63/63 invocations, including working
controls. Corrected default TSan now passes 61/61 unique invocations with functioning
controls: 40 new runs and 21 exact unchanged-ELF passes. Its earlier
artifact-prefix and unrepaired-SDK link failures remain preserved. Five
equivalent pairs per measured workload, exact-source controlled-chain
creation/reopen and the later exact-node observer restart/recovery pass.
The [R15 final-candidate report](../validation/low-resource-lr2-m1-r15.md)
separates current evidence from older candidates. See the
[follow-up record](issue-9-m1-grpc-control-ownership.md#aggregate-refusal-follow-up--2026-10-22).

There is **no overall execution deadline**. Keep previous results and candidate
counters unchanged while continuing the authorized development work.

## M1 closeout implementation plan — 2026-10-22

The separate R10 build-and-observer goal is complete. Its source archive is
`8f7f9e37190be31cc6b540c3f3ed775e8bb01b612cd8446ade56c152d4418e02`
and its normal Linux node is
`00c6f3c4a87d7ff01ae58eda41a3ed4713c80f1bdb19b7aa8ad52617bd0f9f6f`.
On OVH it passed 101/101 normal tests, both CLI checks, 61/61 focused
ASan/UBSan and 59/59 corrected instrumented-gRPC TSan invocations. It also
passed a one-hour mainnet observer mixed-read window and clean same-database
reopen. The [observer record](issue-9-m1-mainnet-observer.md) and private
`evidence/observer-r10-final-reconciliation.json` retain exact inputs,
measurements, exclusions and earlier failures. R10 is a qualified observer
checkpoint, not a complete M1 admission or physical-memory result.

Work through these gates for one identified final implementation:

The [working ownership inventory](issue-9-m1-owned-memory-inventory.md)
tracks creation, final release, quota treatment and remaining proof for the
R10 checkpoint and subsequent development changes. It is not a completion
matrix or a physical-memory measurement.

1. **Reconcile and profile.** Compare the current dirty source with the
   verified R10 snapshot and preserve unrelated changes. Create a live-owner
   inventory for shared pending records, retained forks, map keys/indexes and
   tombstones, nonce/RC/count state, transaction-work staging, encoding,
   JSON/Protobuf and HTTP/gRPC output, cancellation, caller-held values and
   cleanup/recovery workspace. For each allocation, record its creating path,
   final owner, release point, measured size, quota treatment and any sharing.
   Identify protected quota-exempt canonical/recovery work and adjacent R1/P2
   consumers separately; neither an attribution counter nor a scoped sanitizer
   pass establishes a process RSS ceiling.
2. **Fix measured M1 gaps.** Reproduce any missing ownership, avoidable copy,
   retained buffer or admission failure against an identified baseline. Reduce
   unnecessary application allocations and retention first. Reserve
   conservative capacity before allocation or publication, transfer credits
   with the actual owner and release them only after the final owner finishes.
   Preserve accepted transactions and failure-atomic fork/head/expiry,
   block/LIB, persistent-index and response handoff paths. Keep local overload
   retryable and distinct from invalid transactions and peer scoring; canonical
   validation and recovery must remain correct when the optional local quota
   is full.
3. **Qualify the exact source.** Run boundary, concurrent, fault-injection and
   later-retry regressions for each changed owner. Include large IDs/receipts,
   mixed HTTP batches, native gRPC response/cancellation, retained ancestors
   and callers, nonce/RC/count reads and block/LIB transitions. Build the
   complete Linux node, run every registered normal test (at least the 101 R10
   entries), version/help and focused ASan/UBSan and supported default TSan
   with clean/known-fault controls on OVH. Use the pinned instrumented gRPC
   closure for its TSan scope and disclose prebuilt-library/libp2p exclusions.
   Preserve failures, timeouts and separately corrected dependency builds.
4. **Measure equivalent workloads.** Choose low-resource admission settings
   from representative OVH data. Run at least five paired baseline/final
   samples for each claimed improvement using identical inputs, policies and
   workload definitions. Report retained/accounted bytes, allocation count,
   steady/peak RSS, host `MemAvailable`, CPU, disk I/O and latency separately.
   Explain allocator overhead, measurement noise and any CPU/I/O cost. Do not
   infer physical savings from counters or a single observer sample.
5. **Exercise the complete node.** On OVH, run the exact final build in an
   isolated full-history mainnet observer and a separate controlled-chain
   mixed-transaction workload; do not submit mainnet transactions. Verify
   head/LIB progress, independent canonical agreement, sustained concurrent
   transaction/RPC/output activity, a clean restart and recovery after a
   deliberately interrupted owned test. Measure the entire server against
   two vCPUs, 4,000,000,000 physical RAM bytes, no swap and the strict
   40,000,000,000-byte disk budget with `max(8 GiB, database bytes / 5)` free
   reserve. Require steady node RSS <=2 GiB, peak RSS <=2.5 GiB, concurrent
   host `MemAvailable` >=768 MiB and no OOM. Identify differences between the
   OVH device geometry and the reference envelope rather than enlarging the
   budget. Guard the owned test process and preserve the full-history database.
6. **Review the M1 decision.** Reconcile source/dependency/configuration/input
   and binary hashes across every result; update this plan, the completion
   matrix, validation and operator documentation, and the legacy-applicability
   document. Mark only M1 complete when all its applicable ownership,
   admission, correctness, sanitizer, measured-memory and exact-final-node
   gates pass. Record remaining R1/P2/V1/S1/D1/Q1/H1 and wider issue-9 work
   independently. Preserve all closed attempt counters and negative evidence.

There is no overall deadline or fixed number of future attempts. Use new
auditable candidate identities and a diagnostic reason for each rebuild or
repeat. Every executable M1 check stays on the isolated OVH VPS; the authorized
miner only builds, My Passport holds generated evidence, and the Hetzner
producer remains outside this work. This plan authorizes no deployment,
mainnet signing, funds movement, commit, push or release.

For the isolated controlled-chain mixed workload, the operator separately
authorized deterministic local test signatures on OVH only. This uses no
operational key or mainnet transaction and does not expand the observer-only
mainnet scope.

## Working prompt — application memory optimization

Improve the memory efficiency of Teleno, our open-source C++ Koinos node, and
complete issue 9's M1 requirement. Reduce unnecessary allocations, copies,
temporary buffers and retained objects in the mempool, transaction processing
and associated RPC responses. Keep memory accounting consistent with each
allocation's lifetime, and verify that the node handles limited memory and
subsequent retries correctly.

Continue the existing work on `codex/lr2-memory-hardening`. Preserve Koinos
compatibility, accepted transactions, complete history, existing data and
unrelated changes. Keep earlier test results and use exact source, build and
input identities for each new result.

Compile using My Passport or the existing Linux builder. Run all executable
checks only on the isolated OVH test VPS, with generated evidence stored on
mounted My Passport. Leave the Hetzner producer unchanged.

Use focused regression tests, the full native suite and supported memory/thread
sanitizers with working controls. Compare at least five equivalent baseline and
updated runs before claiming an improvement, reporting actual RAM use alongside
CPU, disk I/O and latency.

Validate the final complete node on the existing 2-vCPU, 4-decimal-GB RAM,
no-swap baseline and strict 40-decimal-GB disk budget, including the existing
recovery reserve. Targets remain steady RSS <=2 GiB, peak RSS <=2.5 GiB and
host MemAvailable >=768 MiB during representative mixed workloads and
restart/recovery. Component results alone do not qualify the final node.

Continue without an overall time limit. Keep reports, the completion matrix
and the document about possible legacy-node applicability current. Finish M1
when its existing acceptance conditions pass for the final implementation;
otherwise state precisely which conditions remain. The detailed scope and
historical records below remain applicable.

The operator subsequently requested a complete build and mainnet observer run
on OVH. Follow the [observer work record](issue-9-m1-mainnet-observer.md);
this adds observer qualification on the test host and keeps Hetzner unchanged.

## Current objective — clarified by the operator, 2026-10-21

Optimize the memory use of Teleno, the operator-maintained open-source C++
Koinos node, so that its full-history workload can run economically on a small
Linux VPS. Complete issue 9's M1 requirement for the native mempool and
concurrent transaction work. Reduce unnecessary allocations, copies, temporary
workspace and retention, and implement bounded aggregate application-memory
accounting and admission that follow the actual lifetime of each allocation.

Continue in `/Users/pgarcgo/code/teleno` on `codex/lr2-memory-hardening`, from
checkpoint `d424264b24595786fdba86d9eddbcbf1635284b6` and the existing verified
work and evidence. Preserve unrelated changes. Read `AGENTS.md`,
`issue-9-goal.md`, the LR2 completion matrix, M1-A through M1-G and the later
M1 reports before changing shared ownership or admission behavior.

The engineering work is application profiling, C++ memory optimization,
correctness repair and isolated regression testing. Accounting is part of the
implementation: a smaller counter alone does not demonstrate lower memory use.
Measure actual retained allocations and process RSS, distinguish conservative
reservations from allocator overhead, and disclose any excluded consumers.
Claim physical savings only when equivalent Linux workloads demonstrate them.

### Implementation scope

1. Inventory all M1-owned memory: shared transaction records, retained forks,
   keys/indexes, nonce/RC metadata, tombstones, concurrent work, caller-held
   values, admission staging, serialization, RPC output and cleanup/recovery
   workspace. Identify sharing without double counting.
2. Reproduce missing lifetime accounting, unnecessary retention or failure
   behavior before changing it. Remove avoidable copies and allocations where
   measured, reserve conservative capacity before allocation/publication, and
   release credits only when the last owner frees the memory.
3. Make memory-pressure refusal local, explicit and retryable. Preserve
   failure-atomic admission, accepted transactions, expiry, fork transitions,
   nonce/RC semantics and block/LIB processing. Local policy must not become a
   consensus rule, falsely invalidate transactions or penalize peers.
4. Repair the reproduced JSON cleanup failure after transaction acceptance.
   Validate both successful concurrent output and deliberate capacity refusal.
   Qualify complete HTTP/gRPC response ownership, including large receipts and
   errors, conversion, asynchronous handoff, cancellation and final release.
   C11's reproduced abort remains a failed result until a later exact candidate
   passes its regression.
5. Preserve Koinos consensus, wire behavior, execution/metering, roots, receipts,
   complete canonical history from genesis and every selected block/receipt.
   Do not discard accepted transactions, prune history, omit records or weaken
   validation to reduce memory or disk use.

### Execution and qualification

Execute every native M1 test exclusively on the operator-owned isolated OVH
test VPS. This includes development reproductions, allocation-failure and
concurrency regressions, the full native suite, sanitizer suites and controls,
controlled-chain fixtures, paired measurements and native version/help checks.
The operational Hetzner producer is outside this work: do not contact, modify,
deploy to or restart it. Do not submit mainnet transactions or change funds,
wallets, keys, pool settings or access administration.

Use a fresh owned directory on mounted My Passport for generated artifacts,
inputs, logs and evidence. The Mac and authorized Linux builder may prepare
artifacts and compile without executing M1 tests. Preserve operational
databases, backups, services and secrets. Stop data-producing work if My
Passport is unavailable. Read-only mainnet comparison during an explicitly
recorded observer qualification is separate from isolated synthetic tests.

Test exact capacity boundaries, concurrent admissions, retained ancestor and
caller ownership, expiry, fork/block/LIB transitions, allocation failures,
cleanup/recovery and correct later retry. Run focused Linux ASan/UBSan and
supported default TSan with working clean and known-fault controls; record
instrumentation exclusions. Run the complete normal native suite, retaining
all 74 C9 entries (including the original 64 alpha entries) and meaningful
new regressions. Verify the exact final Linux binary's version/help on OVH.

Use at least five paired baseline/candidate samples for every claimed
improvement. Compare equivalent source inputs and workloads, and report
accounted/retained bytes, allocation requests, RSS, CPU, I/O and latency
separately. Select proposed low-resource settings from representative Linux
measurements. Component tests, older binaries and counters alone do not
qualify the final whole node.

The resource target remains Ubuntu/x86_64/ext4, 2 vCPUs,
4,000,000,000 physical RAM bytes, no swap and a strict 40,000,000,000-byte
whole-server disk budget including OS, software, databases, WAL, compaction,
logs, temporary files and `max(8 GiB, database bytes / 5)` free recovery
reserve. Demonstrate steady node RSS <=2 GiB, peak RSS <=2.5 GiB and concurrent
host MemAvailable >=768 MiB, without OOM or reliance on swap, for the exact
final implementation under representative mixed work and restart/recovery.
The test host's slightly larger physical geometry does not enlarge these
acceptance limits. Whole-node observations must identify any inability to
reproduce the exact reference geometry.

### Execution controls and completion — no overall time limit

The operator explicitly removes execution-time limits for continuing M1
optimization. There is no overall deadline, eight-hour cutoff, maximum build
duration or fixed qualification window imposed by this goal. Continue across
turns for as long as the work requires, until the acceptance conditions are
demonstrated or a concrete blocker requires user input or an external change.
Do not stop useful progress merely because an earlier time allowance elapsed.

Record actual start/progress times, owned paths, the diagnosis being tested,
source snapshots, frozen candidate identities and every result. Keep candidate
and build counters auditable. This time-only amendment does not change any
separately specified non-time candidate/resource limits or authorize repeated
unchanged tests without a diagnostic reason. The standing authorization covers
necessary further repairs and qualification; a new request is not needed for
each continuation.

Protocol deadlines and bounded liveness assertions may remain when they are
the behavior being tested. A tool polling timeout is an observation boundary,
not evidence that a live build/test failed: recheck the same handle and its
progress before acting. Resource guards remain in force. Do not impose an
overall work cutoff through a hidden supervisor or an inherited expired-run
deadline.

Preserve every closed run exactly as recorded, including failures, deadlines
and unused candidate slots. Continue under new evidence/candidate identities
instead of reopening C11/C12 or relabeling an older binary as the final one.
This amendment changes future execution controls, not historical outcomes or
the completion standard.

Deliver exact source/dependency/input/binary identities, implementation changes,
test reports, paired measurements and updated English documentation/completion
matrix. Keep the legacy-applicability document current: distinguish observed
Teleno defects from hypotheses requiring a separate legacy reproduction. Keep
affected operator documentation current under the repository's Koinos One
documentation handoff requirements.

Mark M1 complete only when every applicable M1 condition is demonstrated for
the selected final implementation. Otherwise report the exact remaining
conditions. Issue 9 and the separately incomplete D1/P2/R1/V1/S1/Q1/H1
requirements remain open. This clarification preserves the acceptance and
boundaries below and all historical evidence.

## Recorded attempts and location amendment

The dated windows and fixed execution deadlines below describe historical
runs. Their records remain unchanged; they do not impose a time limit on
work authorized after the 2026-10-21 execution-time amendment above.

**Operator test-location amendment, 2026-10-20:** all future executable M1
tests and qualification run exclusively on the new isolated OVH VPS,
including development regressions, fault injection, sanitizer controls/suites,
controlled-chain fixtures, paired measurements and native CLI checks. The
Mac/miner/authorized builder may prepare artifacts and compile without running
tests. My Passport remains the generated-evidence store. This supersedes
earlier M1 test-location permissions below; historical results remain intact
and cannot qualify a new candidate under this amendment. The Hetzner producer
is excluded. Preserve every existing deadline, counter and resource/reserve
requirement. See the [active repair goal](issue-9-m1-receipt-output-repair.md#exclusive-test-host--operator-amendment-2026-10-20).

The separate [C11/C12 receipt-output repair](issue-9-m1-receipt-output-repair.md)
started at **2026-10-20 16:26:31.428768 UTC**, with an unchanged deadline of
**2026-10-21 00:26:31.428768 UTC** and at most two new frozen candidates.
The unused C10 slot in the old closed attempt remains unused. C11 froze at
22:09:33.956325 UTC and passed 6/8 OVH component invocations; concurrency
timed out and a post-acceptance allocation fault aborted during JSON cleanup.
The attempt expired unqualified with C12 unused. The separate frozen-C9
observer sequence also stopped unqualified; neither result changes the
operational producer.

The operator supplied an additional isolated VPS for tests on 2026-10-20.
The separate [frozen-C9 qualification run](issue-9-m1-isolated-vps-qualification.md)
has the fixed interval **15:19:53.510265–23:19:53.510265 UTC**, no new source
candidates/builds and at most two observer starts. Its CPU/RAM/architecture
match the target; its larger physical disk does not increase the strict
40,000,000,000-byte whole-server budget. This does not authorize changes to
the existing producer or alter any closed attempt's counters.

The C9/C10 attempt began on **2026-10-20 at 08:00:37.080251 UTC** with an
unchanged **16:00:37.080251 UTC** deadline and at most two frozen source
candidates. Its owned generated-data and control root is
`/Volumes/My Passport/teleno-m1-nonce-read-20261020-l2q4z85b/`.
C8 source reproduces two shorter-predecessor nonce/count read overflows under
Linux ASan. C9 is the only frozen candidate; it corrects both reads and passes
74/74 normal Linux tests, 22/22 focused ASan/UBSan and 22/22 focused TSan
checks, separate cleanup checks, and a controlled 100-block/12,800-transaction
production/reopen profile. Five paired C8/C9 controlled samples and all ten
reopens pass, with nearly identical median RSS/CPU and wide run-to-run latency
variation; no code-caused saving or slowdown is claimed. The run closed at
**11:11:04 UTC** before its original deadline, with C10 unused. This does not
establish complete M1 ownership or whole-node margins.
See the [C9 evidence](../validation/low-resource-lr2-m1-nonce-read.md). The
live VPS producer remains untouched, and older attempt limits are unchanged.

The operator's standing 2026-10-20 authorization also covers a new, separate
C7/C8 attempt begun at **03:52:11.516774 UTC** with an unchanged deadline of
**11:52:11.516774 UTC** and at most two new frozen candidates. Its owned root is
`/Volumes/My Passport/teleno-m1-output-gap-20261020-rF9mPYUo/` and starts from
the exact verified C6 source archive. First reproduce and address M1-owned
nonce/RC/count and escaped output accounting, then run isolated Linux
qualification. The C5/C6 control, source and artifacts are immutable inputs;
this attempt cannot reset their counters or convert the controlled C6 profile
into reference-VPS evidence. The live producer remains untouched.

On 2026-10-20 the operator authorized all further attempts needed for M1.
This removes the need to request permission for each distinct, necessary repair
attempt; it does not turn the closed C1–C4 runs into successes, reset their
counters or authorize an unbounded run. Each follow-up must retain its own
fixed start, deadline, candidate ceiling, evidence and stop decision. The
closed C5/C6 interval began at **2026-10-20 00:35:55.857720 UTC**, had a deadline of
**08:35:55.857720 UTC**, and permits at most two new frozen candidates. Its
owned generated-data root is
`/Volumes/My Passport/teleno-m1-followup-a5711dmt/` with private durable
`control.json`. It starts from the exact C4 frozen archive; only eight local
documentation files had changed after that freeze. No live VPS producer change,
deployment or mainnet transaction is part of this follow-up. The
[C5/C6 scope](../validation/low-resource-lr2-m1-repair.md#c5c6-follow-up-scope-authorized-2026-10-20)
was drafted before authorization; the limits above govern this specific
C5/C6 attempt. Later attempts, if necessary, must have a new diagnosis
and separately persisted finite limits rather than silently extending this one.

C5 passes 68/68 normal Linux entries, 16/16 focused ASan/UBSan checks and
15/15 default-TSan entries after a recorded generated-test timeout adjustment.
It remains **unqualified**: a 256-MiB controlled workload refuses admission
after 43 complete blocks, and the identical 768-MiB workload reaches only 62
of 100 blocks before its fixed 900-second limit. Three stack samples identify
the C5 allocation-owner table's linear delete lookup as a CPU hotspot. C6, the
second and final frozen candidate, enlarges that table. It passes 68/68 normal
entries and two isolated 100-block/12,800-transaction production-and-reopen
profiles, including a 768-MiB aggregate policy. Focused default TSan passes
15/15 entries plus cleanup; focused ASan/UBSan passes 15/15 entries plus its
separate cleanup with working controls. The run closed at **03:50:42 UTC** with
both candidate slots consumed. C6 is scoped-qualified, but M1 is not: the
controlled builder profile does not establish complete ownership/admission,
mixed-runtime behavior or physical margins on the reference VPS. These tests
do not authorize touching its producer. Exact
identities, measurements and failures are in the
[C5/C6 report](../validation/low-resource-lr2-m1-repair.md).

The 2026-10-19 C3/C4 allowance below consumed both frozen-candidate slots and
stopped within its original deadline. C4 passes 68/68 normal Linux tests and
16/16 scoped ASan/UBSan entries; its default TSan run reports a potential
lock-order inversion, and five paired component samples per workload show
higher RSS. Representative Linux M1 policy and whole-node qualification remain
unproved. The [repair report](../validation/low-resource-lr2-m1-repair.md)
preserves the exact identities and results. The additional
[legacy applicability document](legacy-applicability-of-teleno-findings.md) is
created and remains a hypothesis register, not a reproduced legacy defect.

The first attempt below remains closed and unqualified. Its deadlines, candidate
identities, failures and evidence are immutable. The subsequent allowance is
separate and follows the operator's explicit 2026-10-19 authorization.

The final planned native check completes on 2026-10-19 at 13:06:47 UTC. Both
frozen implementation candidates are consumed. Normal 66/66 and scoped TSan
12/12 pass; ASan/UBSan 11/12 includes a decoded-query fault leak. Incomplete
read/RPC/execution output ownership and profile selection also prevent M1
completion. Preserve the unchanged limits below. Further runtime repair requires
a newly authorized finite implementation allowance; do not reset this run.

The operator authorizes source implementation and isolated qualification of
aggregate native mempool and concurrent transaction-work memory. This run starts
from `d424264b24595786fdba86d9eddbcbf1635284b6` on
`codex/lr2-memory-hardening`. The published alpha's 64 normal Linux tests are
historical evidence, not validation of this implementation.

## Fixed execution limits

- Actual start: **2026-10-19 08:30:38 UTC**.
- Hard deadline: **2026-10-19 16:30:38 UTC**.
- At most **two frozen implementation candidates**.
- Owned generated-data root:
  `/Volumes/My Passport/teleno-m1-aggregate-20261019-gaqjl_4n/`.
- Durable control: `control.json` in that root. Candidate source manifests and
  counters are append-only across turns; a failed candidate consumes its slot.

Development compilation and small reproductions precede freezing. Each complete
implementation selected for the final native suite, diagnostics and paired
comparison is frozen before qualification and consumes one candidate slot.
Changing frozen runtime source requires the remaining slot. Test corrections
must preserve failed results and distinguish harness changes from source changes.
Deadlines and counters are never reset or extended.

## Acceptance

Inventory shared records, fork/map/index ownership, nonce/RC metadata, tombstones,
caller-held values, staging, serialization, concurrent work and recovery
workspace. Account shared allocations once and release credits only after their
last owner frees them. Reserve before allocation/publication, retain accepted
transactions on refusal/failure, and permit a correct later retry. Local overload
must remain separate from invalid transactions and cannot govern canonical block
validity or peer penalties.

Prove capacity boundaries, concurrency, retained ancestors/callers, expiry,
fork/block/LIB transitions, allocation failures, cleanup and retry. Run working
Linux ASan/UBSan and supported TSan clean/known-fault controls; disclose exclusions.
Preserve all 64 existing normal tests, add meaningful regressions, and verify
the exact final Linux executable's version/help. Use at least five paired
equivalent samples for any improvement claim, separating accounted/retained
bytes, allocator requests, RSS, CPU, I/O and latency.

The design target remains Ubuntu/x86_64/ext4, 2 vCPUs, 4,000,000,000 physical RAM
bytes, no swap and 40,000,000,000 whole-server disk bytes including the existing
`max(8 GiB, database bytes / 5)` recovery reserve. Whole-node steady RSS <=2 GiB,
peak RSS <=2.5 GiB and host MemAvailable >=768 MiB remain separate requirements.
Component counter/reservation evidence cannot establish physical heap/RSS or
whole-server outcomes.

## Boundaries

Preserve consensus, wire behavior, execution/metering, roots, receipts, nonce/RC
semantics and complete canonical history. Do not prune or omit records, weaken
validation, or discard accepted transactions to satisfy a budget. Use mounted
My Passport for generated inputs/evidence and isolated builder directories for
compilation only. Execute all future M1 tests exclusively in owned directories
on the new isolated OVH VPS. Preserve operational data, backups, services,
secrets and unrelated changes.
Stop data-producing work if the volume is unavailable.

No live VPS producer changes/restarts, replacement deployment, mainnet
transactions, wallets, keys, funding or access administration are authorized.
Keep M1, issue 9 and other incomplete requirements open until their applicable
acceptance conditions are demonstrated for the final implementation. On success
or exhausted limits, deliver exact source/dependency/input/binary identities,
implementation, tests, measurements, English documentation and explicit unmet
conditions.

## Authorized finite repair allowance — 2026-10-19

The operator explicitly authorizes another finite attempt to fix the inherited
protobuf allocation-failure leak and complete the remaining M1 ownership,
admission and qualification requirements, **without touching the VPS producer**.
This supersedes only the prohibition on additional implementation candidates;
the original attempt remains closed, with both original candidates consumed.

- Actual resumed start: **2026-10-19 14:16:04 UTC**.
- New hard deadline: **2026-10-19 22:16:04 UTC** (eight hours).
- At most **two additional frozen implementation candidates, C3 and C4**.
- Preserve the original C1/C2 counters and results; never relabel or replace them.
- Owned generated-data root: `/Volumes/My Passport/teleno-m1-repair-20261019-n2bhladg/`.
- Its separate `control.json` persists this allowance and append-only candidate
  identities. Never reset or extend it across turns.

First reproduce and repair parse/copy/repeated-field exception cleanup in the
pinned protobuf integration, preserving wire format and accepted transactions.
Then reserve ordinary nonce/RC/count and generic result workspace before
allocation, retain reservations through RPC/protobuf/serialization/caller and
execution/receipt lifetimes, and validate final-owner release, refusal and retry.
Do not substitute serialized sizes or a fixed input envelope for complete
ownership. Select any proposed profile from representative Linux measurements.

For the final frozen candidate rerun the full normal native suite (retaining
all 66 existing entries), CLI identity checks, focused Linux ASan/UBSan and
supported TSan with clean and known-fault controls, and at least five paired
equivalent samples for claimed improvements. Preserve failing results and
explicit instrumentation/allocator exclusions. Component results do not close
whole-node resource, history or other issue-9 requirements.

All acceptance and boundaries above continue to apply. No deployment, VPS
operation, producer change/restart, chain mutation, funds, keys, access
administration, commit, push or release is authorized by this amendment.
Stop on verified M1 success or exhausted new limits and report exact unmet
conditions. Do not declare M1 complete when any applicable condition is unproved.

### Additional deliverable: legacy applicability analysis

The operator additionally requests a maintained English document identifying
problems being improved in Teleno that could also be reproduced in the legacy
Koinos microservices node. Maintain
[`legacy-applicability-of-teleno-findings.md`](legacy-applicability-of-teleno-findings.md)
as evidence changes during this attempt.

For each finding record the cause, affected allocation/lifetime or interleaving,
Teleno evidence and repair status, the exact inspected legacy source/dependency
identity, shared versus architecture-specific behavior, and a bounded isolated
reproduction procedure with a clean control, expected failure and pass condition.
Distinguish a reproduced legacy failure, a source-supported hypothesis and a
finding with no established legacy applicability. Never infer that a production
legacy node is affected from a shared package name or a Teleno-only test.

Read-only comparison of existing public source checkouts is included. Legacy
software edits, deployment, live-node fault injection, new remote access and
production operations are not authorized by this document deliverable. Keep
unknown installed versions, missing source and unexecuted reproductions explicit.
This addition does not extend the deadline or candidate allowance, change the M1
acceptance conditions, or authorize touching the VPS producer.

## Continuing authorization and current isolated attempt — 2026-10-20

The operator authorizes the additional attempts needed to complete M1, while
preserving the instruction not to touch the live VPS producer. This authorization
does not reopen or alter the closed C1–C6 attempts. Each new attempt must have
its own recorded start, finite deadline, candidate allowance, evidence and stop
condition. Failed results and consumed candidate slots remain part of the record;
neither time nor counters may be reset within an attempt.

The current C7/C8 attempt started at **2026-10-20 03:52:11.516774 UTC** and ends
at **2026-10-20 11:52:11.516774 UTC**. Its separate control record is
`/Volumes/My Passport/teleno-m1-output-gap-20261020-rF9mPYUo/control.json`.
It allows at most two frozen source candidates, C7 and C8. C7 was frozen at
**04:17:59.420828 UTC** and is unqualified after 19/21 focused TSan entries.
C8 was frozen at **06:18:23.502270 UTC** with source archive SHA-256
`16e1d33d1ef197b19ceb636e3283baaadfa1b659585f21916eb691a5b52c42ea`.
Both slots are consumed. The run closed at **07:58:20.432912 UTC**, before its
unchanged deadline. C8 passes 73/73 normal, 21/21 focused ASan/UBSan,
21/21 focused TSan, both separate cleanup checks and one controlled 100-block
production/reopen profile. Five paired equivalent C6/C8 samples show a
material C8 CPU/latency slowdown with an unresolved cause and nearly unchanged
RSS; the exact measurements are in the [output report](../validation/low-resource-lr2-m1-output-ownership.md).
Broader M1 ownership, representative policy and reference-VPS evidence remain
unmet. Do not claim M1 complete from a focused fixture or a prior binary.
Continue the legacy
applicability document as evidence develops, distinguishing Teleno-specific
HTTP behavior from any independently verified legacy finding.

The authorization covers source repair, isolated builds, tests and documentation.
It does not authorize deployment or any operation on the running VPS producer,
mainnet signing, funds, access administration, commits, pushes or releases.


Current C14 qualification checkpoint: 49/94 registered normal tests and both
native version/help checks pass. SC5 then exceeds the strict disk budget by
26,140,672 B and is interrupted; its evidence is preserved and owned duplicate
data is externally verified before retirement. A separate sequence covers the
44 previously unexecuted cases, retaining SC5 as an unmet condition. The
22 final-source ASan/UBSan ELF files are externally verified; sanitizer execution,
TSan compilation and whole-node qualification remain pending. No frozen counter,
resource limit, historical result or operational producer state is reset.


Current continuation checkpoint: C14's default TSan scope finishes **52/52**,
including working generic and actual SDK controls. Its normal SC5 failure and
HTTP refusal ownership gap remain unmet. The unfrozen HTTP repair passes the
individual ownership case but aborts in the mixed-batch case; the preserved
terminate trace identifies an allocating `noexcept` JSON version comparison.
Revision 2 replaces that comparison with a borrowed string check and strengthens
fault/retry coverage. Qualify it on OVH before freezing a later implementation;
retain the failed revision, all prior candidate counters and final whole-node
gates. See the [current validation report](../validation/low-resource-lr2-m1-json-cleanup.md).
