# LR2 completion matrix and execution log

## Native 1.3.0 publication — 2026-10-23

The separately authorized clean release integrates M1 and the committed Fogata
corrections at `2c11229267cff2f9ee39ed8e56ab70af391d85ce`. Its exact Linux
ELF `9fcb043727b23ab05d9660701a61f369f9413a82ff646f94f2dbf830fb4de53f`
passes 107 normal checks, four tool tests, both CLI checks, focused Fogata
detectors/controls, one hour of mainnet mixed reads and clean same-database
reopening. Both test observers finish stopped. Native artifacts are published;
container publication is tracked separately in the
[release record](native-1.3.0-publication.md). Earlier R15 and failed-run records
below retain their identities and limits; issue 9 remains open beyond M1.

## Current M1 R15 Linux qualification — 2026-10-23

**M1 complete for exact R15.** Source archive
`3be9aa55f128f649d851ec87ca9dfa3574736eec0bdec9e4e9ebaecbae47995f`
and Linux node `e825f1951a583590e9173c57be79626635dbb110f750d1258eb34efffa31eef4`
pass 104/104 normal checks, both CLI checks, 63/63 focused ASan/UBSan and
61/61 default-TSan unique cases with working controls. Eight owner groups,
including bounded refusal controls, pass final review. Five equivalent pairs
per measured workload and exact-source 100-block/12,800-test-transaction
create/reopen pass. The exact full-history OVH observer completes 3,600
error-free mixed-read cycles, 31 independent comparisons, clean restart and
owned-process interruption/recovery. Observer peak RSS is 419,278,848 B;
controlled peak is 705,712,128 B. Host memory, no-swap/OOM and strict disk
plus full-reserve gates pass. See the [R15 report](low-resource-lr2-m1-r15.md).
Issue 9, LR2 and separate R1/P2/V1/S1/D1/Q1/H1 gates remain open. Older failures,
closed counters and unrelated P2P work are preserved. The Hetzner producer is
untouched; no commit, push, release or app integration is performed.

## R13 Linux qualification — historical checkpoint, 2026-10-22

Frozen R13 source archive SHA-256
`839e77930e20600cb6fdf143a4bf259448ad076c426f5ff4e14b1f0ab368c684`
and exact Linux node SHA-256
`c3205142024a1911244c69bfd2d3a9708f67b2fd53b5f96d3bd6f3c8bf5f5710`
pass **101/101** normal OVH checks, both CLI checks, **61/61** focused
ASan/UBSan and **59/59** default-TSan unique cases with working clean and
known-fault controls. The original TSan cleanup CTest timeout and separate
unchanged-binary completion remain visible. Five R12/R13 paired block-event
samples show that R13 removes 1,536 allocation requests per 256 events; the
small RSS difference is inconclusive. The isolated exact-source native
workload passes 100 blocks, 12,800 locally signed test transactions, and
same-state reopen with matching block ID, state root and archive digest.
Its highest sampled process HWM is 720,809,984 B; minimum strict disk margin
after the 8-GiB reserve is 498,278,400 B. The exact R13 full-history OVH
observer passes its one-hour mixed-read window: 3,599 successful cycles,
31 independent comparisons and zero errors. Its clean same-database reopen
and owned-process interruption/recovery pass with four observer starts, zero
producer starts and no node left running. Maximum observer kernel/sampled HWM
is 369,373,184 B; minimum host `MemAvailable` is 2,960,805,888 B and minimum
strict margin after the reserve is 548,851,712 B. Runtime evidence is
externally verified and reconciled in private
`evidence/r13-runtime-reconciliation-v2.json`; the first sample-only margin
report is preserved and superseded. Final aggregate-owner review,
representative policy and whole-node physical attribution remain pending.
**M1 remains open.** See the
[R13 validation record](low-resource-lr2-m1-json-cleanup.md#r13-avoid-redundant-native-block-notification-copies).

The continuing unfrozen refusal follow-up reproduces protected-control growth
with exact final release, then bounds it with separate lifetime-owned credits.
Six small normal, the same six scoped ASan/UBSan cases with three controls,
and eight small default-TSan cases/controls pass on OVH. A new real-server
exhaustion test first fails in pinned gRPC's unbound factory-time call; a
single-member SDK call-binding correction subsequently passes four normal,
four scoped ASan/UBSan server cases and three controls. Thirteen SDK source/archive/configuration
recipe checks pass, and the repository module builds and links its Release
component after preserving header-inventory and Debug-selection failures.
Expanded fallback-fault checks pass eleven normal/ASan/control invocations,
and six real-gRPC default-TSan cases pass with the instrumented SDK and working
clean/known-race callback controls. A new frozen whole-source build and its
full normal/sanitizer, paired-policy and runtime gates remain open. R13's node
results do not qualify this new implementation. See the
[refusal follow-up](../implementation/issue-9-m1-grpc-control-ownership.md#aggregate-refusal-follow-up--2026-10-22).

Frozen R15's complete normal Linux build passes: source archive
`3be9aa55f128f649d851ec87ca9dfa3574736eec0bdec9e4e9ebaecbae47995f`,
71 ELFs, 168 identified linked libraries and exact node SHA-256
`e825f1951a583590e9173c57be79626635dbb110f750d1258eb34efffa31eef4`.
At that build checkpoint its native, sanitizer and runtime qualification was
pending; the current R15 section above supersedes that status. R14's source-assembly CMake
failure is retained; unrelated workspace P2P changes remain untouched.

## R10/R12 Linux observer qualification — historical checkpoints

The later frozen R12 source passes **101/101 registered normal tests** and
both native version/help checks on OVH with the exact node SHA-256
`31dac5e65fada62b800506cef76d4f9d21cbbd6aed6d87b67e8576e20cd50c5a`.
Its full normal ELF set is externally hash verified. Focused final-source
ASan/UBSan passes 61/61 OVH invocations with working controls. Its default
TSan matrix reconciles 59/59 unique passes, preserving an original 120-second
CTest timeout and separately verified 600-second unchanged-test completion.
A separate 100-block/12,800-transaction controlled-chain workload passes
creation and same-database reopen on OVH with local deterministic test
signatures, no network adapter and a 734,810,112-B creation peak RSS. The
exact-node full-history mainnet observer has caught up and begun its
one-hour mixed-read window. Clean restart, interrupted-test recovery,
whole-node qualification and complete aggregate ownership remain pending;
**M1 remains open**. See the
[R12 validation record](low-resource-lr2-m1-json-cleanup.md).

R10 preserves R9's source archive SHA-256
`8f7f9e37190be31cc6b540c3f3ed775e8bb01b612cd8446ade56c152d4418e02`
and links the verified Zstd-enabled RocksDB closure. Node SHA-256 is
`00c6f3c4a87d7ff01ae58eda41a3ed4713c80f1bdb19b7aa8ad52617bd0f9f6f`.
OVH passes **101/101 unique normal tests** and native `--version`/`--help`.
The complete build tree and every ELF are externally hash verified. Preserve
the earlier recipe-stage and CLI-expectation failures as separate results.
The least strict whole-server disk margin during a passing case was
154,886,144 bytes, with the recovery reserve included. Same-source R9 focused
ASan/UBSan passes 61/61; TSan has 59 unique passes in 60 invocations, with its
original timeout and separate successful diagnostic retained. These sanitizer
results use an earlier RocksDB closure and do not qualify exact R10.

R10 caught up to the public mainnet head as an isolated observer and matched
the common irreversible block ID. Its first mixed-read window stopped after
three public-reference timeouts, preserving 1,261 correct local cycles and 30
successful independent comparisons. A separate one-hour continuation passed
3,573 local cycles, with zero local errors and 15 independent canonical
agreements; 15 reference calls were unavailable. The first run stopped cleanly
with peak sampled RSS 627,982,336 B and a 495,771,648 B minimum strict disk
margin including reserve. The same database reopened and advanced 92 blocks
from the first reopened proof, passed three further independent checks and
stopped cleanly. Peak sampled reopen RSS was 109,805,568 B. Exact R10 focused
ASan/UBSan passes **61/61** OVH invocations with working clean/known-fault
controls and hash-verified exact-closure binaries. Minimum strict disk margin
is 1,131,679,744 B and minimum `MemAvailable` is 2,872,823,808 B across its
sampled cases. The corrected exact-source, Zstd-RocksDB, instrumented-gRPC
TSan closure passes **59/59** focused OVH invocations with working controls and
the unchanged cleanup test under its recorded 600-second allowance. Its
minimum sampled strict margin is 1,139,589,120 B after reserve and minimum
`MemAvailable` is 3,189,100,544 B. The original 120-second timeout and
uninstrumented-gRPC race report remain preserved. Both observer runs and all
test units are now stopped. Complete M1 ownership/admission, paired physical
memory savings and representative whole-node qualification remain pending;
**M1 remains open**. See the
[observer work record](../implementation/issue-9-m1-mainnet-observer.md) and
[validation report](low-resource-lr2-m1-json-cleanup.md).

## M1 JSON-cleanup continuation — started 2026-10-21

The [new development continuation](low-resource-lr2-m1-json-cleanup.md) starts
at **03:33:59.573386 UTC** with **no overall deadline**, owned evidence at
`/Volumes/My Passport/teleno-m1-json-cleanup-20261021-lygqon1e/` and separately
tracked C13/C14 candidate labels; neither is frozen. It addresses C11's
post-acceptance allocation failure in `noexcept` JSON cleanup using a fixed
parent stack, in-place unary collapse and allocation-free deep fallback.
All **14/14 scoped normal OVH development invocations** meet their expected
result, including a clean/default-destructor fault control, saturated-budget
cleanup, exact final credit release, post-acceptance HTTP failure recovery,
concurrent output, complete success/error encoding and refusal/retry. The
post-acceptance single/mixed calls remain accepted and later calls succeed.
A relocated-header build refusal and harness-only correction are preserved.
The components use identified C11 headers and retained C9 linked libraries;
they do not qualify a complete C13 node. Initial static sanitizer binding/link
failures are preserved. Corrective dynamic-runtime ASan/UBSan passes 15/15
scoped invocations. TSan has 13 uninterrupted passes and one interrupted
cold-reflection capacity test; its raw wrapper-zero result is explicitly
superseded by a failure reconciliation. A standalone control reproduces the
TSan once-exception limitation, while a normal regression separately proves
request-quota capture of lazy global metadata. The proposed metadata correction
and deep-cleanup CPU correction are compiling, not qualified. The first 30
paired cleanup processes confirm zero helper allocation requests but expose a
17.4x median CPU cost for deep branches. Preserve this result and all failed
build/control invocations. Execution remains OVH-only and the producer is
untouched. The later R3 revision passes 16/16 normal, 17/17 ASan/UBSan and
16/16 default TSan component invocations with explicit child-completion markers.
A second 30-process paired comparison removes the original deep-comb CPU
penalty while retaining zero helper cleanup allocations. Wide-fixture process
peak-RSS median falls by 348,160 bytes; this is not whole-node evidence.
A 510-setting guarded parse diagnostic releases its credits correctly, but
an unguarded response-dump control terminates at full quota. Its guarded
counterpart recovers. R4 then passes 16/16 normal, 17/17 ASan/UBSan and 16/16
default TSan component invocations. A separate stock-dependency partial-copy
sweep still terminates at quota 10,793 despite an outer guard. The proposed
build-owned JSON destructor backport passes four direct copy/dump controls,
nine normal/diagnostics/sanitizer destructor invocations and seven staging
recipe tests on OVH. Its SDK/native integration is compiling; these scoped
results do not qualify it. A real HTTP fixture separately measures 1,019,389
additional attributed bytes at service entry for a 262,144-byte argument;
earlier release of redundant encoded parameter trees is not yet qualified.
The private collector cleanup-scope incident and uncertain prior file state
are preserved separately from native results. No C13/C14 candidate is frozen.
M1 remains open.

Integrated R5 completes 18/19 normal invocations; its only failure is a gRPC
large-status fixture using the client's default metadata limit. A diagnostic
confirms status 8 and the peer metadata limit; a test-only enlarged client now
has a separate default-limit rejection control. The real HTTP parameter fixture
passes with a 281,499-byte service-entry delta against the earlier 1,019,389;
this is not paired RSS evidence. A separate 14-fault baseline reproduces four
wrong native payer inputs and two misclassified local refusals in base58
normalization. R6 rethrows resource failures, preserves valid base64 fallback
and passes 14/14 normal RPC invocations with zero mismatches/misclassification
and exact final credits. Scoped ASan/UBSan and default TSan are underway.
The [current source ownership review](low-resource-lr2-m1-json-cleanup.md#current-ownership-review-before-candidate-freeze)
also identifies unaccounted gRPC output-state/interceptor/registry control
allocations; these require assessment before completing M1. No C13/C14 source
is frozen, and final whole-node gates remain unmet. R6 ASan/UBSan subsequently
passes 17/17. Default TSan passes 15/16 with a prebuilt-core gRPC metadata/read
signal. A pure-SDK control reproduces it without the Teleno server/allocator;
the signal remains unresolved while a pinned instrumented SDK is built.
Five paired input/large-receipt processes per side show 737,890 fewer live
attributed bytes but a 462,848-byte higher median peak RSS, with overlapping
CPU/wall ranges and zero filesystem blocks. This is a negative physical-memory
result. R7 source avoids creating the extra parameter tree and byte-field
string copy. Its later revision passes 13/13 normal, 16/16 scoped ASan/UBSan
and 15/15 default TSan HTTP checks. Five new paired measurements show 548,864 B
lower median process peak RSS, lower in every pair, with equal CPU/wall
medians. This scoped result does not establish whole-node savings.

Pinned instrumented gRPC passes clean/intended-race/unchanged-native controls
(3/3). R8 startup/per-call control attribution passes 2/2 normal, 4/4 default
TSan and 5/5 ABI-compatible ASan/UBSan invocations. Preserve the first 4/5
ASan sequence: its public context layout mismatches the prebuilt runtime.
The upstream GRPC_ASAN_SUPPRESSED compatibility mode restores that ABI while
compiler instrumentation remains active. Earlier R6 gRPC ASan success is not
valid gRPC qualification. Expanded simultaneous cancellation/stop and actual
callback clean/fault controls pass 3/3 normal, 9/9 scoped ASan/UBSan and
5/5 default TSan invocations. Final source, complete native suites and whole-node
resource/restart evidence remain unmet. C13 source freezes at 2026-10-21T09:51:26.273826+00:00 after the expanded scoped checks.
C14 remains unused. The complete final-source native build and OVH suite,
sanitizers, profiles and whole-node/restart gates are pending.

C13's complete normal build and actual version/help pass. Its normal suite
reaches **41/90 functional passes**, then stops because retained generated test
data/co-staged executables breach the strict disk budget by **11,403,264 B**.
Verified external preservation and retirement restore the recorded disk margin;
the full-history database is unchanged. The 21 focused ASan/UBSan binaries
compile and are externally hash-verified, without sanitizer execution yet.
Exact C13 status regressions independently fail full-quota refusal ownership,
percent-encoding workspace, copy-failure handling and pre-boundary exception-text
materialization. An unfrozen first repair passes three requirements and still
fails the argument-copy case. A revised repair passes 7 normal, 13 scoped
ASan/UBSan and 9 scoped default-TSan checks with working clean/fault controls.
Its first ASan launch cannot find the runtime library; a separate phase with
the same binaries and verified runtime passes. Final-source native/sanitizer
and whole-node gates remain; C14 is not frozen.
See the [C13 checkpoint and evidence](low-resource-lr2-m1-json-cleanup.md#c13-remains-unqualified-resource-guard-and-status-path-defects).
Preserve every failed result. M1 remains open and the producer is untouched.

The corrected **C14** snapshot freezes at **2026-10-21 12:14:23.846369 UTC**,
archive `8400d631109f458c61c82ad12ae24aab2ce35f670c160450d595b0e2bb1b8869`,
with 471 files. Its complete normal build is running without tests on the
miner. The configured 94-entry suite retains every C13 entry; the first four
registered source-recipe tests pass on OVH. Per-case binary staging, generated
data preservation and disk/resource sampling replace C13's accumulating
execution layout. No whole-history observer starts or producer operations
occur. See the [current C14 checkpoint](low-resource-lr2-m1-json-cleanup.md#c14-frozen-for-full-qualification).

C14's complete normal build subsequently succeeds with 71 externally verified
ELF outputs and 167 linked-library identities. Exact node SHA-256:
`71bc0b0773cbd108d4783b39eccf9d16289b9f1f5a7cf4e077d10fdf168d568d`.
The interrupted caller's incomplete summary is reconciled against recorded
exit zero, the successful unit journal and frozen-source verification, without
rebuilding. A 21-case core phase uses 19 ELF files identical to the final
outputs. The complete 94-entry normal suite/CLI remains pending, and the
separate final-source ASan/UBSan build is running. No whole-node completion is
claimed.

C14 subsequently passes **49/94 normal entries and both native CLI checks**,
then the SC5 synthetic restore experiment crosses the strict disk budget by
26,140,672 B. Its interrupted result and all 647 generated entries are externally
preserved before exact owned-copy retirement; the history database is unchanged.
A separate continuation covers the 44 unexecuted cases without erasing that
failure. The 22 final-source ASan/UBSan outputs are externally verified; final
sanitizer execution and TSan compilation remain pending. **Complete normal
qualification and M1 remain open.** See the [C14 storage failure](low-resource-lr2-m1-json-cleanup.md#c14-normal-suite-storage-failure-remains-open).

The distinct continuation completes all 44 scheduled cases. Exact JUnit/source
reconciliation proves **93/94 normal passes and two CLI passes**; SC5 remains
a failed resource gate. Among passing cases, minimum strict margin is
228,212,736 B and minimum MemAvailable is 3,262,717,952 B; the complete evidence
also retains the negative SC5 margin. Both 22-ELF sanitizer builds are verified;
ASan/UBSan execution has started with working clean/fault controls. Unfrozen
SC5 retirement revision 2 passes 15 helper unit tests; the full experiment and
Python-compatibility revision remain pending. No final-node completion follows.

## M1 execution-time amendment — 2026-10-21

The operator removes overall execution-time limits for continuing M1 memory
optimization, compilation and qualification. Continue until the applicable
acceptance criteria are proved or a concrete blocker requires intervention.
Do not use an eight-hour cutoff or inherited expired-run deadline to stop
future work. The [current objective and execution controls](../implementation/issue-9-m1-aggregate-goal.md#execution-controls-and-completion--no-overall-time-limit)
govern continuation. Source/candidate identities, meaningful diagnostics,
non-time resource guards, exclusive OVH execution and the exclusion of the
operational Hetzner producer remain. Closed runs below retain their original
deadlines, failures and counters; their expiry is not a future work limit.

## Current M1 purpose and terminal checkpoint — 2026-10-21

The [clarified objective](../implementation/issue-9-m1-aggregate-goal.md#current-objective--clarified-by-the-operator-2026-10-21)
is to optimize actual Teleno application memory: reduce unnecessary
allocations, copies, workspace and retention, and qualify lifetime-owned
aggregate mempool/transaction admission. Accounting counters alone are not
physical savings. All executable M1 work remains OVH-only and excludes the
operational Hetzner producer. This wording starts no new attempt.

C11/C12 expired unqualified at its original **2026-10-21 00:26:31.428768 UTC**
deadline. C11 froze at **2026-10-20 22:09:33.956325 UTC**, source archive
`95b2914962e9127b18bbc88b4fe535cd69dcc96c3465d46cbca7554204e36bd3`.
Its exact OVH component sequence passes **6/8** invocations. Concurrent
successful HTTP output times out with the fixture's 256-MiB policy; an injected
post-acceptance allocation failure aborts in `nlohmann::json`'s `noexcept`
destruction. A single diagnostic-only OVH trace confirms that stack. C12
expired unused; no whole-node C11 binary, complete normal/sanitizer suite,
native CLI, paired comparison or final whole-node result is available.
See the [terminal repair evidence](../implementation/issue-9-m1-receipt-output-repair.md#terminal-result).

The separate C9 observer sequence stopped unqualified at **2026-10-20
18:32:58.009720 UTC** after its additional 1-GiB early-stop margin was crossed.
The final strict 40-decimal-GB margin, including the full 8-GiB recovery
reserve, remained **998,899,712 bytes**. Readiness passed, but only about
21.5 minutes of the requested mixed observation completed and no reopen ran.
Kernel peak RSS was **421,789,696 bytes**; sampled minimum host MemAvailable
was **2,960,740,352 bytes**, with no recorded OOM/swap. This is incomplete C9
evidence, not qualification of C11. A read-only OVH check at reconciliation
finds no native test observer or running attempt unit. M1 remains open.

## M1 exclusive test host — operator amendment, 2026-10-20

All future executable M1 tests run only on the new isolated OVH VPS:
development regressions, failure injection, complete native/sanitizer suites
and controls, controlled-chain fixtures, paired measurements and CLI checks.
My Passport and the authorized builder may prepare/store artifacts and compile
without running tests. Preserve prior builder/Mac results as historical
evidence, not qualification for the new final candidate. Keep current C9 and
C11/C12 deadlines/counters, physical limits and recovery reserve unchanged.
Later OVH tests must not compete with the active frozen-C9 sequence; the
operational Hetzner producer remains untouched. See the
[active repair goal](../implementation/issue-9-m1-receipt-output-repair.md#exclusive-test-host--operator-amendment-2026-10-20).

## Additional isolated VPS: frozen C9 qualification — 2026-10-20

Status: **single-copy staging, offline validation/held activation and observer
readiness pass; mixed observation stopped early, reopen not run; closed
unqualified for M1**.
The unchanged finite window is **15:19:53.510265–23:19:53.510265 UTC**,
with no new source candidates/builds and at most two observer starts. The
operational producer remains outside scope. Generated artifacts, controls
and evidence are under
`/Volumes/My Passport/teleno-m1-ovh-qualification-20261020-sgjo6v1a/`.

The additional host has two x86_64 CPUs, 4,000,022,528 physical RAM bytes,
Ubuntu 26.04/ext4 and no swap. Its 40-GiB physical device does not enlarge the
40,000,000,000-byte acceptance budget. Source verification matches all 287
checked C9 runtime/build/script/test files; the executable remains SHA-256
`bea7856f367d008e6ab43a1b9ecb016cab1623197e60dd928b936150347d0aa8`.
Version/help and startup/metadata preflight pass. A harness-only preflight
argument refusal is preserved separately. The complete selected input is
383 files / 24,114,178,822 bytes, with unchanged manifest/inventory pins.

Guarded native staging started at **15:56:41 UTC** and completed at
**16:22:44 UTC**: 383 files / 24,114,178,822 transferred payload bytes,
64,057,344-byte kernel peak RSS and 2,382,008,320-byte minimum sampled strict
disk margin, including the recovery reserve. Offline validation/held activation
started at **16:22:46 UTC** and passed at **16:46:18 UTC**, checking 46,458,363
records, nine column families and 917,316 state objects. Its kernel peak RSS
is 64,163,840 bytes; minimum sampled strict disk margin is 2,380,722,176 bytes
including reserve. Structural validation does not prove historical execution.
Observer start one began at **16:46:22 UTC**. Readiness passes at **18:11:27
UTC** with independent canonical head ID/root agreement at height 40,375,538,
LIB 40,375,478 and zero height lag. The irreversible block ID agrees; the
reference omits its receipt state root. The full one-hour mixed read-only
observation began at that time and stopped at 18:32:57 UTC under the extra
1-GiB disk guard; the full hour and reopen were not completed.
A reviewed finite
coordinator gates offline validation, held activation, observer catch-up,
one-hour read-only mixed observation and a single clean reopen on successful
prior results. It cannot restart failed production or touch the existing
producer. Physical/runtime gates and complete M1 ownership/admission are
pending; download-process memory is not node-runtime evidence. See the
[qualification plan and explicit remaining conditions](../implementation/issue-9-m1-isolated-vps-qualification.md).

A separate, one-build header-only Linux diagnostic against unchanged C9 headers
reproduces raw-request metric overcount: 69 actual bytes are reported as 100,
and a 512-byte over-aligned request as 783. The disabled control and final
credit release pass. It consumes no node candidate build and makes no native
source change. See the [coverage audit and diagnostic identities](low-resource-lr2-m1-isolated-vps.md).

The same unchanged-C9 linked libraries also reproduce a chain-receipt output
gap: single/mixed HTTP bodies retain 4,538,368 / 4,546,560 capacity bytes while
their M1 counter is back to 240 bytes. The owned mempool-output control passes.
This synthetic interface test has no controller, ledger, keys or external
network; it is missing attribution rather than an observed leak.

## C11/C12 receipt-output repair — started 2026-10-20

The separate finite [repair run](../implementation/issue-9-m1-receipt-output-repair.md)
starts at **16:26:31.428768 UTC**, ends no later than
**2026-10-21 00:26:31.428768 UTC**, and permits at most two frozen candidates,
C11/C12. C11 later froze and failed component qualification as recorded above;
C12 expired unused. The old unused C10 remains unused; no closed
attempt or simultaneous C9-VPS limit is reset. Generated data and durable
control are under
`/Volumes/My Passport/teleno-m1-receipt-output-repair-20261020-zxyqrmy1/`.

Development moves obsolete raw-wrapper/alignment policy out of the actual
request counter and into supplementary reservation, preserving total admission
bytes. The new Linux regression fails against unchanged C9 and passes against
the development correction, including final-owner and concurrency checks.
Full candidate qualification and the receipt-output repair remain pending.
No operational producer, deployment, funds or signing action occurs.

Development now adds an explicit synchronous native receipt-preparation
callback before accepted-work publication. The opt-in HTTP path prepares its
complete response before acceptance and retains single/mixed output credits
through write completion. New regressions cover receipt lifetimes, cancellation,
capacity/refusal/retry, byte-encoding preservation and canonical RPC isolation.
At this historical development snapshot the code was not yet executed or
qualified. The build-only HTTP component
now passes after preserving and correcting relocated-include harness failures;
both objects use the pinned C9 generated/dependency paths. Control/development
ELF SHA-256 values are respectively
`ae5d5391da75edb0fe615ffe7e4856243762b650a6775f382883989601d74bf7` /
`8e5fd7857a1c54b820a16b02bd25722adafd81f5c18502f3e119620c7436a214`.
A later post-acceptance allocation-failure fixture is not part of that snapshot.
The unchanged-C9 gRPC serialization diagnostic also builds (ELF SHA-256
`5df30ba8539b7a2c61a7a5b4459bdcd927f866e9ce7e1078e836b92d3ffbcb4f`).
No tests run on the builder. All future execution remains OVH-only and separate
from C9's resource observation. Later exact C11 component results are recorded
above; final qualification and full M1 evidence remain unproved.

A separate bounded error-boundary diagnostic compiles once and links against
both the unchanged-C9 and development HTTP implementations, without executing
on the builder. It will check single/mixed HTTP error bodies and native gRPC
error status using synthetic data. At most five OVH executions follow closure
of the original six-component queue and C9 observation. Its controls retain
the original C11/C12 deadline and all earlier build/candidate counters. No
error-output defect or repair is claimed before inspecting those results.

## M1 C9/C10 shorter-predecessor read follow-up — 2026-10-20

Status: **one frozen candidate C9 passes its scoped native qualification;
the finite attempt closed unqualified for M1 at 11:11:04 UTC**. The separate attempt starts at
**08:00:37.080251 UTC**, has an unchanged **16:00:37.080251 UTC** deadline and
allows at most C9/C10. Its evidence and control are under
`/Volumes/My Passport/teleno-m1-nonce-read-20261020-l2q4z85b/`.
Frozen C8 reproduces two Linux ASan heap-buffer-overflow reads in nonce/count
lookup when a shorter predecessor key precedes a longer queried account. C9
checks the key length before prefix comparison and adds a regression for both
reads, normal results, accepted transactions and credit release. Its source
archive SHA-256 is
`40c28493e7ff0c2985d85f2ce1a278c47e209e5e23d16af1bbe2641070eab9d5`;
the normal node binary SHA-256 is
`bea7856f367d008e6ab43a1b9ecb016cab1623197e60dd928b936150347d0aa8`.
The full Linux suite passes 74/74; focused ASan/UBSan and TSan each pass 22/22,
their separate cleanup checks and working detector controls. A controlled
768-MiB-policy 100-block/12,800-transaction production/reopen passes with
zero refusals, matching block ID/root/archive and 816,521,216-byte peak RSS.
The first two-job **build-only** OOM is preserved; the same frozen source
builds with one job under a 4-GiB build cgroup. Five paired C8/C9 controlled
samples and all ten reopens pass. Median elapsed is 333.25/315.29 seconds,
CPU 447.62/446.03 seconds, peak RSS 826,904,576/825,638,912 bytes and M1
peak accounting identical at 564,639,994 bytes. Paired elapsed differences
range from +119.98 to -85.48 seconds; no code-caused saving or slowdown is
claimed. C10 was unused. Sanitizer exclusions, exact identities and remaining M1 gates are
in the [C9 report](low-resource-lr2-m1-nonce-read.md). No reference VPS or
producer operation is part of this attempt.

## M1 C7/C8 follow-up — 2026-10-20

Status: **C7/C8 finite attempt closed unqualified at 07:58:20 UTC with both
slots consumed. C7 fails focused TSan. C8 passes 73/73 normal Linux, 21/21
focused ASan/UBSan and 21/21 focused TSan entries plus both cleanup checks,
but five paired controlled samples show unresolved CPU/latency slowdown.
M1 remains open**.
The separately persisted run starts at **03:52:11.516774 UTC**, has a fixed
**11:52:11.516774 UTC** deadline and at most two frozen candidates. Owned
generated data and control are under
`/Volumes/My Passport/teleno-m1-output-gap-20261020-rF9mPYUo/`. The exact C6
source archive is the starting input. The first diagnostic reproduced a mempool
JSON-RPC response whose HTTP body outlived its M1 attribution. C7 froze at
04:17:59 UTC with source archive SHA-256
`808ae567ad42864ac0f85b46cb4ff7bb93f212537e649d90070541f2706bbb7b`.
Its corrected complete normal run passes 73/73 with node binary SHA-256
`ee933b8897f7cf6661256d36322c0aa61f54191b0e6ac1e1b2e683dde0ffa8c0`.
The initial parallel build OOM and 71/73 harness-path run remain preserved;
neither consumed C8. See the [C7 output report](low-resource-lr2-m1-output-ownership.md).
The ASan/UBSan clean/known-fault controls and separate cleanup check pass; its
exact build archive is verified on My Passport before remote cleanup. C7's
TSan controls work, but session-lifecycle and mempool-output roundtrip tests
report a session executor lifetime race. The TSan result and exact build archive
are preserved. C8 changes only the JSON-RPC session source relative to C7;
its frozen archive SHA-256 is
`16e1d33d1ef197b19ceb636e3283baaadfa1b659585f21916eb691a5b52c42ea`.
Two pre-freeze full lifecycle TSan runs pass. Frozen C8 also passes its
normal/sanitizer suites and an isolated 768-MiB-policy profile with 12,800
signed local transactions, zero aggregate refusals and matching root/archive
after reopening. The profile's one-sample 362.66-second creation time versus
C6's 192.15 seconds is not an attributed regression by itself. The completed
five-pair comparison has C6/C8 median creation times of 188.59/298.31 seconds,
CPU times of 309.87/445.53 seconds and producer p95 of 1.171/1.714 seconds.
Median peak RSS is nearly unchanged at about 820 million bytes (782 MiB),
and M1 peak accounting is identical. The slowdown is observed but not causally
assigned because the
builder had variable cohost load and each run generated different block IDs.
Broader M1 ownership, policy and VPS evidence remain open. See the
[output ownership report](low-resource-lr2-m1-output-ownership.md).
No reference VPS or producer operation is part of this attempt. Preserve the
closed C5/C6 evidence below.

## M1 C5/C6 follow-up — 2026-10-20

Status: **C5/C6 closed unqualified; M1 remains open**. The operator authorized necessary further
M1 attempts. This distinct eight-hour run started at **00:35:55.857720 UTC**
and ends at **08:35:55.857720 UTC**, with at most two frozen candidates C5/C6.
Control, source-start manifest and generated data are under
`/Volumes/My Passport/teleno-m1-followup-a5711dmt/`. The 449-file C4 source
manifest matches the current worktree for all runtime/build/test files; eight
documentation files changed later. Preserve C1–C4 evidence and deadlines.
Frozen C5 passes 68/68 normal, 16/16 scoped ASan/UBSan and 15/15 scoped
default TSan after a recorded generated-test timeout adjustment. It is
unqualified: its 256-MiB controlled workload refuses admission after 43 blocks,
and the same 768-MiB workload stops at block 62 on its 900-second time limit.
Three stack samples reach the linear allocation-owner delete lookup. Five
alternating C5/C6 development pairs of that path show median 2.256 → 0.014
seconds for 200,000 uncharged deletes while 100,000 charged owners remain,
with about 1 MiB higher RSS for C6's larger static table. This is a component
benchmark, not a whole-node saving claim.

C6 freezes only that owner-table runtime change from C5 and passes 68/68 normal
Linux entries with unchanged source/binary hashes. Two isolated custom-chain
100-block/12,800-transaction runs, including one at a 768-MiB aggregate limit,
pass nonempty mempool admission, block/receipt continuity and exact clean
reopen/replay; peak RSS is 818,077,696 and 808,235,008 bytes respectively,
with zero child swap and zero aggregate refusals. These are on the separate
16-GiB Linux builder under a 2-vCPU/2.5-GB/no-swap child limit, not the actual
4-decimal-GB reference VPS. C6 scoped default TSan passes 15/15 main entries
plus separately bounded cleanup with working controls. Focused ASan/UBSan
passes 15/15 main entries and separate cleanup with working controls. The
finite run closed at 03:50:42 UTC with both source slots consumed; its exact
C6 ASan archive was verified before removing the owned remote build. Complete
M1 attribution, target-host physical margins and integrated
whole-node behavior remain unproved. See the
[C5/C6 report](low-resource-lr2-m1-repair.md). No live VPS producer operation
is authorized here.

## Authorized M1 repair allowance — 2026-10-19

Status: **finite C3/C4 attempt closed unqualified; M1 remains unmet**. The operator
authorized a separate finite allowance from **14:16:04 to 22:16:04 UTC**, with
at most two additional frozen candidates, C3/C4. Both slots are consumed and
work stopped within the unchanged deadline. The first attempt below stays closed and
unqualified. Its counters, evidence and deadlines are unchanged.

The new attempt reproduced the inherited 1,008-byte/28-allocation query-copy
leak and a generated oneof allocation-failure destruction crash. C3 failed its
normal suite (44/68); a pinned RocksDB `malloc_usable_size` crash traced to
the C3 accounting prefix. C4 repairs that interoperability fault and isolates
process-global protobuf reflection initialization from local admission quotas.
Its frozen 449-file source archive is identified in the
[repair report](low-resource-lr2-m1-repair.md), and the exact C4 normal Linux
suite passes **68/68** with version/help and unchanged source/binary hashes.
Frozen C4 also passes 15/15 focused Linux ASan/UBSan entries, a separately
bounded cleanup entry and working clean/UAF/UB detector controls. Its inherited
decoded-query fault sweep no longer leaks under this instrumentation. Default
TSan passes 14/15 plus cleanup, with a lock-order-inversion warning in the
aggregate mixed-fork test; a separate race-only detector run passes 15/15 plus
cleanup with working clean/known-race controls. The default warning is retained.
Five baseline/C4 pairs per scenario show lower cumulative `new` requests but
approximately 1.1 MB higher RSS in both small synthetic workloads, with slower
ordinary admission. A representative Linux M1 profile and complete-node
qualification remain missing. No VPS operation was performed.

Owned generated data:
`/Volumes/My Passport/teleno-m1-repair-20261019-n2bhladg/`.
See the [amended goal](../implementation/issue-9-m1-aggregate-goal.md) and the
requested [legacy applicability register](../implementation/legacy-applicability-of-teleno-findings.md).
Legacy service failures remain unproved; source similarity is not a reproduced
legacy defect.

The formerly leaking aggregate ASan/UBSan entry passes in the separately
identified dependency-repair development stage (288.093 seconds). This precedes
the later allocation/oneof integration and is not final qualification. See the
[repair-stage identities and remaining gates](low-resource-lr2-m1-repair.md).

## Finite M1 aggregate-memory implementation — 2026-10-19

Status: **finite attempt unqualified; M1 and LR2 NOT complete**. Fixed UTC interval:
08:30:38–16:30:38, at most two frozen candidates. The first Linux reproduction
shows retained tombstones after record-owner credits reach zero. Owned budget,
block/LIB preparation and shared native transaction-work changes are implemented.
C2, the second and final candidate, is frozen at 11:33:04 UTC with runtime/build/
config manifest `144e579e5d703ace10e4db403d9c56a34ee0d51a3b3046b0ff16920fb73f79d8`.
Its eight development entries pass; the frozen Linux build, version/help and
all 66 normal entries subsequently pass. C2's instrumented aggregate entry
fails LeakSanitizer: 1,008 bytes in 28 allocations during decoded-query failure
injection. Eleven of twelve focused entries pass, including the separately
bounded 145.883-second cleanup. Clean/UAF/UB controls work. Scoped TSan passes
12/12 with working clean/race controls; it does not clear the ASan leak failure.
Two synthetic workloads each have five pairs against a freshly compiled exact
starting checkpoint. They measure accounting overhead, not RAM savings.
Development component passes do not qualify the final implementation. Other
nonce/RC/count read workspace, escaped RPC/execution output ownership and representative Linux profile selection
remain explicit gaps. See the [finite goal](../implementation/issue-9-m1-aggregate-goal.md)
and [ownership/evidence report](low-resource-lr2-aggregate-memory.md). No live VPS
operation is authorized by this run; D1/P2/R1/V1/S1/Q1/H1 remain open where their
required final evidence is missing.

A normal C2 adapter probe retains 256 protobuf responses (2,123,520 serialized
bytes) after the native vector releases its reservation. Current reservations
remain 50,618 bytes. This directly proves the escaped-output gap; it is neither
a heap/RSS total nor a successful M1 lifetime bound.

The same query-fault leak is independently reproduced against the freshly built
starting checkpoint, with a clean-query control. It is inherited protobuf
exception-safety behavior; it still fails C2 qualification. No third runtime
candidate is allowed in this finite run.

The final planned native check finishes at 13:06:47 UTC, within the original
deadline. Both candidate slots are consumed. Source implementation stops with
the listed M1 failures and omissions; this is not successful goal/M1 completion.

## Native alpha publication checkpoint — 2026-10-19

Native [1.3.0-alpha.1](https://github.com/koinos/teleno/releases/tag/teleno-node-v1.3.0-alpha.1) is published from `2380fcb545ed056a548adbcd5e1cffcefe61ecc0`. All 64 normal Linux
entries, native version/help and the exact-binary container checks pass. The
compatibility regression restores the published 131,072-byte external syscall
capacity. This updates the scoped current normal-build evidence; integrated
diagnostics, aggregate policies and final whole-node/loaded producer gates
remain open. Predecessor VPS observations keep their original identities. See
[the publication record](alpha-1.3.0-publication.md).

Updated: 2026-10-18. Status: **active implementation; LR2 NOT qualified**.

**Subsequent own-block observation completed:** under the operator's separate
authorization to wait for an own block, the same qualified candidate produces
**40,323,375** at **21:35:48 UTC**. Local/independent canonical IDs and selected-pool
headers match; the full proof at **21:40:57 UTC** has independent LIB **40,323,411**.
The block is irreversible and contains zero transactions. The producer remains
active without a new start/restart; the ten-minute heartbeat is verified paused.
This proves eligible PoB submission and real acceptance for one empty block.
The closed one-hour result remains unchanged and issue 9 stays open for its
other criteria. A private monitor signer-format decoding error is preserved and
corrected independently of native source/binary/configuration; see the
[separate confirmation](native-producer-handoff.md#subsequent-own-block-confirmation).

**Separately authorized native producer repair:** the operator authorizes source
repair/rebuild and one new finite mainnet commissioning. Candidate manifest
`775bb4cbae8462cea397c4fd1e9248279bc64c0fa97a82931e2d4c16bf31385b`,
executable `0bd4c3666a93a3b43d7c4c7e72776d9d935eec1a33cdeec22572ac71022d9447`,
passes the deterministic real-mempool handoff regression, scoped concurrency/
failure checks, CLI and all 63 distinct native entries after documented
environment corrections. Observer readiness and the deliberately authorized
**3,600.00018-second** producer observation pass, using one new start without a
restart/extension. All 31 logged handoffs recover in one retry, at most 36.628 ms,
with no producer error. Peak native RSS is **223,084,544 bytes**, minimum host
MemAvailable **3,230,367,744 bytes**, and maximum whole-server accounting including
the 8-GiB reserve **38,186,901,504 bytes**. No produced block is observed in that
fixed window, so it does not itself prove canonical production/real eligible-block
acceptance. The subsequent independent confirmation above is separate. The repaired
producer remains active as ordinary operation; its finite controller/timer are
inactive and automatic restart/boot activation is not enabled. This does not
close issue 9 or replace the earlier failed attempt's evidence. The fixed-window
final native head 40,319,079 and full state root match the independent irreversible
canonical successor, separately from own-production evidence.
See [the repair report](native-producer-handoff.md). Owned generated evidence:
`/Volumes/My Passport/teleno-producer-repair-20261018-flnbdgu4/`.

**Current issue-9 amendment, 2026-10-18:** the operator changes original issue 9
to an **8-GiB recovery-reserve floor (8,589,934,592 bytes)**, retaining
`max(8 GiB, database bytes / 5)`. This replaces the former 10-GiB acceptance
requirement. The completed current-source 8-GiB VPS measurements below now count
toward issue 9: the selected-input restore and measured-workload disk criterion
pass. The standard CLI default remains 10 GiB unless the explicit profile is
selected. Remaining aggregate-memory, diagnostic, historical and producer gates
stay open; older 10-GiB measurements remain dated evidence, not a current blocker
requiring another 1.50 GB merely to satisfy the superseded reserve.

**Separately authorized mainnet producer attempt, 2026-10-18:** the existing
candidate passes observer readiness, independent canonical LIB comparison,
matching key/registration and positive-VHP checks after the operator's explicit
Fogata funding/burn requests. One producer start then reports **12** recurring
`cannot retrieve pending transactions from an unknown block` exceptions. The
attempt stops at approximately **21 minutes**, before completing its one-hour
window, and the original observer is restored and verified. The partial producer
window has kernel RSS high-water **159,924,224 bytes**, minimum host MemAvailable
**3,301,134,336 bytes**, no OOM/swap change and positive disk margin, but zero
observed produced blocks. This does **not** qualify operational production or
close issue 9. The exact head/mempool interleaving remains unproven; no source
repair/rebuild or deadline reset is performed under that finite commissioning
scope. See the [failed-attempt report](../performance/mainnet-producer-commissioning-2026-10-18.md).
Owned generated evidence and the lossless failed-attempt archive are under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/mainnet-producer-commissioning-20261018-8vhde0kn/`.
That failed attempt finishes with a verified observer and its producer/controller/
watchdog inactive. The separately authorized repair above determines the current
runtime; do not infer it from this historical result.

Separate authorized 8-GiB experiment: final 312-input source manifest
`6456879f2162baf57d6b932054bf92e19569fb8e47c89d109fbd35e11a964ec5`
and native ELF `f086e2d71c04cc0922ccd497a2cb1c63d5265bc1d3190b2e17e1156b5a7d10f4`
pass the complete Linux build/all 63 distinct CTest entries and actual VPS
full restore, interruption/resume, read-only validation, held activation and
reopen. Restore peaks at 37,915,475,968 whole-server bytes including the held
8-GiB reserve. Real-data compaction passes all 46,458,363 current/checkpoint/
reopened records, SST checksums and a 1,390,521,101-byte job, peaking at
39,352,967,168 whole-server bytes including the held reserve. Its original-archive
retention baseline and isolated producer create/reopen also pass. The final
whole observer passes its 900-second smoke/catch-up, mixed reads and planned
clean stop, clean restart and complete controlled interruption/recovery. The
original irreversible block/receipt digest matches after reopening. The actual
observation completes the operator-requested five hours from the same process's
original start: **18,007.577 seconds**, clean native exit, **9,360 steady reads /
zero failures**, peak RSS **635,420,672 bytes**, minimum host MemAvailable
**2,769,223,680 bytes**, no OOM/swap and strict disk margin **1,833,799,680 bytes**.
Its day episode catches up after
2,237.276 seconds from an already advanced database; an initial near-tip
canonical ID/root independently matches legacy. The 300.707-second
post-observation reopen passes, and its final height 40,312,594/canonical ID/full
root matches the irreversible legacy reference. Final archive comparison is
complete: the final 45,153,063-record scan preserves all 45,076,063 original
records byte-for-byte and adds 77,000 higher records. Final host/source/library
and finite-goal review pass. The separate experimental 8-GiB qualification is
complete for this selected backup and measured workloads; largest native RSS is
745,668,608 bytes and minimum host MemAvailable is 2,760,302,592 bytes.
This finite five-hour experiment does not qualify the original 24-hour/seven-day
or wider issue-9/LR2 sustained-operation requirements.
See [the separate report](vps-8gib-recovery-reserve.md). Owned generated inputs,
build evidence and tests are under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/vps-8gib-goal-20261018-72a46f/`.
The operator's subsequent amendment adopts the tested 8-GiB reserve for original
issue 9; the standard 10-GiB CLI default and broader LR2 qualification remain distinct.

Latest original-10-GiB designated-VPS result: the B3 24-hour observer attempt stops after
76,023.649 seconds when the unchanged strict disk guard records a minimum
margin of **-11,091,968 bytes**. Peak whole-server accounting including the
unchanged 10-GiB reserve is **40,011,091,968 bytes**. Physical root free space
stays above the reserve; the native process exits cleanly and is freshly
verified closed. Peak RSS is 407,150,592 bytes, minimum host available memory
3,033,333,760 bytes, with no OOM or swap. This is a failed sustained-disk
qualification, not a completed day; earlier full-restore and one-hour passes
retain their separate scope. See [the final observer report](reference-vps-observer.md).

Latest B7 checkpoint: the pinned runtime's arena string copy now constructs a
valid empty object before the throwing payload copy. Same-object Linux controls
change only `arenastring.cc.o`: minimal and real mempool cases fail with the
original runtime (UBSan 92) and pass with the correction. The integrated
311-input candidate `a21a8958e1757a8f2f661264b5b8665158c6db33c9c31a2fed8991d39e962a20`
passes all 63 normal Mac entries, complete build and version/help; native ELF
`e50d79cda78ea16ff31f415bcb717bcf5f9d4975f8d073f54c7ea496331ee98d`.
The first Mac regression assumed a standard-library allocation size; its failed
fixture/source are preserved. Ordinal fault sweeping now verifies actual
payload refusal on both library layouts. Fresh Linux suites are prepared and
remain required. See [the causal report](protobuf-unknown-field-ownership.md#arena-string-construction-failure).

For the preceding diagnostic manifest, all 63 normal Linux entries and all
86 targets complete with individually audited records. Its instrumented suite
passes 17 entries before exposing the arena-construction failure; it is not a
full sanitizer pass. The paired cleanup launcher refuses that failed gate and
records no comparison measurements. Frozen B3 is the separate stopped VPS
observer; its final sustained-disk failure is recorded above.
The [precise objective](../implementation/issue-9-goal.md#reusable-loop-objective)
uses "operator-owned Linux build-and-test machine" and preserves all acceptance
criteria and operational authorization boundaries.

Current diagnostic-source update: the SC0 source-export provenance correction
has manifest `6dde3b266993faf8898f277ae8584f0c86eb659056fc604067dbbe84d2612688`
(311 inputs). Exactly two diagnostic Python files and their README differ from
the B6 ownership checkpoint below. C++/headers/config/CMake/dependencies,
both Linux compile-command files and the native ELF hashes remain identical.
Nine focused checks and a fresh full normal Mac run pass. The prior normal
Linux run's 41 passes and Git-metadata failure are preserved; fresh exhaustive
normal/instrumented Linux suites and paired cleanup measurements remain open.
See [the source-export report](source-export-provenance.md).

Latest B6 checkpoint: the pinned protobuf publication-ownership correction
passes controlled Linux leak regressions, the full native Mac build/CLI and
all 63 normal Mac entries. The normal Linux node build/CLI and both new tests
pass; the instrumented runtime and both new tests pass. Its first CLI build
hits the private compiler scope's unchanged 2.5-GiB ceiling. The global OOM
guard stops the next normal action and fourth historical pair. Exact failures
are preserved; a one-command GC adjustment lets the incremental build and CLI
pass with no new OOM. A reviewed normal-suite continuation verifies canonical
CMake commands after correcting the coordinator's handling of unbuilt test
executables. Full Linux suites remain open.
Source manifest `c8bad5024f0750d566145ba251d3fce72f52a7ee766414eadc57f447e466b1f2`
has 311 inputs. Earlier B3/B4/B5 rows retain their own evidence identities;
see [the current ownership report](protobuf-unknown-field-ownership.md).

Purpose clarified on 2026-10-16: **optimize Teleno's code to lower the cost of
running a complete Koinos node on a low-cost VPS**. RAM/CPU/disk/I/O efficiency,
complete backup restoration within 40 GB and the producer workload on the
designated 2-vCPU / 4-GB Linux server are the outcome. Correctness and recovery
checks establish that the savings preserve node behavior. The
[cost-optimization loop](../implementation/issue-9-goal.md) retains all existing
acceptance gates and records component evidence separately from whole-node
qualification.

This matrix tracks LR2 and the subsequent full cost-optimization objective.
Earlier LR2 authorization alone did not authorize LR3/LR4. The approved objective
now requires isolated lossless storage work and complete 40-GB restore
qualification; those milestones retain their separate acceptance status.
It does not authorize source-trust changes, repository publication, Koinos One
integration or mainnet production activation. Earlier local fixes are preserved,
not reclassified as whole-node evidence. Full genesis history and receipts
remain required.

## Persistent issue 9 goal (2026-10-15)

### Subsequent mandatory baseline and producer clarification

**Replacement clarification (later on 2026-10-15):** the user retired the first
server and selected a different VPS as the only reference. The replacement is
verified as **2 vCPUs, 4 decimal GB RAM, 40-GB-class disk, Ubuntu/x86_64/ext4 and
no swap**. Its independent restricted-key SSH login is verified; the active
private route points only there. The first host is no longer contacted and is
not a fallback. The producer requirement remains. Use the original 4-GB
steady/peak RSS targets (2 GiB / 2.5 GiB) with concurrent host headroom at least
768 MiB. The following first-server paragraph is retired historical context.

The user subsequently selected a particular VPS and requires the producer to
work on that server. Read-only verification found a 1-vCPU, 2-GB-class,
40-GB-class Ubuntu/x86_64/ext4 host without swap. This stricter baseline replaces
the earlier 2-vCPU/4-GB observer reference for active qualification; retain older
results as historical comparisons. Provisional steady/peak node RSS ceilings
are now 768 MiB / 1 GiB, additionally constrained by concurrent host
`MemAvailable >=768 MiB`. These are unverified engineering gates, not defaults
or proof that the target fits. Producer candidate construction, PoB calculations,
mempool selection, VM/state, mixed networking and recovery all belong in the
budget. Whole-node, history and disk/source-trust requirements are preserved.

The user explicitly authorized persistent SSH access setup and necessary
console/email password recovery. A dedicated local key was installed with
forwarding/PTY restrictions, the server-enforced temporary password change was
completed, and an independent noninteractive key login succeeded. Credentials
and usable access routes remain outside repository documents; no password was
retained. Private access verification and the real host inventory are in the
owned external run. No existing node service, producer registration/key or
mainnet signing/submission was changed. Mainnet activation still follows the
network/target/observer-first gate. See the updated [goal contract](../implementation/issue-9-goal.md).

The following initial setup paragraphs retain their original context; the
subsequent clarification above controls the active target.

Earlier P2-A/B1 execution: the reproduced Peer RPC short-write truncation is
corrected, with **52/52 Mac CTest (202.83 s)**, all-target/version/help checks,
signed synthetic restore roundtrip and strict Koinos One documentation build.
Separate Linux ASan/UBSan/TSan checks of the actual write helper/codec fixture
pass. Six standalone clean/known-fault diagnostic controls also pass on the
replacement VPS. TSan requires ASLR disabled only for its diagnostic child;
global host settings remain unchanged. Linux all targets/version/help and
**52/52 CTest (122.88 s)** pass after two prerequisite portability corrections;
failed attempts remain preserved. Nine isolated native CLI/component/restore
commands also pass on the actual replacement VPS, including producer mocks.
See [P2-A evidence](low-resource-lr2-p2p-writes.md). Whole-node,
producer, integrated diagnostics and historical gates remain open.

The user requested a persistent implementation goal for issue #9, using
My Passport or their miner for development storage and compilation while
retaining the cheapest Hetzner VPS resource target. The requested loop's
[resumption contract](../implementation/issue-9-goal.md) defines the loop and
all remaining proof gates. This request additionally authorizes isolated
development builds/tests on the selected miner after identity/resource/ownership
checks. Earlier server restrictions still apply to live-node services,
operational data, producer changes, global host settings and deployment.

The minimum reference is Ubuntu/x86_64/ext4, 2 shared vCPUs, 4 decimal GB RAM
and 40 decimal GB whole-server disk, including OS/runtime, full history,
WAL/compaction/logs/restore/recovery headroom. The catalog still lists that CX23
shape but currently marks it unavailable. Do not raise the resource envelope,
weaken source/reserve rules or prune history to make a test pass. The provisional
LR2 RSS/headroom criteria in Q1 remain unchanged; disk qualification and
LR3/LR4/LR5/LR6 remain separately identified.

Fresh owned run: `/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/`.
Its `environment.sh` verifies the volume identity before exporting external
temporary, fixture, native build and dependency roots plus disabled Python
bytecode writes. `evidence/worktree-before.json` fingerprints 193 pre-existing
dirty/untracked paths; Git status, tracked/staged diffs and the live issue body
are retained. Actual branch/HEAD are `codex/lr2-memory-hardening` /
`d3cfb94455da4a472f651eb4239abbc289e74225`; source is still dirty and mixed.

A bounded read-only miner inventory completed successfully and is retained
privately in the owned run. Local compilation remains selected; isolated Linux
diagnostic feasibility is the next step. Neither access nor process/container
limits on a larger host constitute whole-machine target qualification. No
runtime code, native regression, sanitizer or Linux capacity gate is newly
claimed by this goal setup. Existing matrix rows remain open as recorded.

Current [P2-B evidence](low-resource-lr2-p2p-reads.md): 54/54 native CTest on
Mac/Linux (127.37/139.29 s), all-target/version/help, codec differential and
separate scoped ASan/UBSan/TSan actual-transport checks pass. A reproduced leak
from established connections surviving listener shutdown is corrected. These
larger-host and Mac fixtures close no whole-machine baseline, source or 40 GB
gate. The separately authorized [publisher update](../implementation/backup-publisher-identity-update-plan.md)
is installed from its isolated maintenance worktree after 22/22 Linux regressions
and container checks. Observer/head/LIB/new receipt verification and public
publication pass. The mixed LR2 source is not deployed on the writer. Current
native VPS preflight also verifies the new public identity, but its subsequent
stage refuses at the unchanged reserve before creating payload/DB state.

Current [P2-C evidence](low-resource-lr2-p2p-client.md) supersedes P2-B as the
active client implementation: a real slow-fragment response reproduces the old
renewed deadline; outgoing calls now own cancellation/late buffers and keep
local receive capacity out of peer scoring, with smaller later read-only block
batches. Final review also sheds large ignored capacity for small decoded
results. Mac/Linux all targets/version/help and 55/55 CTest (144.49/130.72 s),
separate scoped ASan/UBSan/TSan, nine normal-ASLR VPS component commands and
isolated runtime-container CLI/hash checks pass. Interrupted/intermediate
results remain separate. No whole-node or 40 GB completion follows.

### P2-D continuation (2026-10-16)

[Peer-bookkeeping evidence](low-resource-lr2-peer-bookkeeping.md) records five
old defects and an intermediate query-rounding regression, their corrections,
finite record/key-capacity policy and optional candidate ownership. Reviewed
focused tests pass 3/3. Mac full CTest passes 56/56 (186.07 s), all targets/CLI,
actual-clock decay and 16 Go/native single-query vectors. Five-pair Mac component
measurements show lower retained allocation requests with a unique-address elapsed
cost; they do not prove node RSS savings. Linux all targets/CLI/actual-clock and
56/56 native tests pass (131.90 s summed), plus 16 reference vectors and five-pair
measurements; eight current fixtures under each separate ASan/UBSan/TSan pass
with controls. Eleven actual-VPS normal-ASLR component commands and matching
runtime-container CLI/hash checks pass. Linux tests use exhaustive
54/1/1 batches with verified external fixture archival before owned retirement, keeping
the unchanged builder reserve. Strict Koinos One manual/rendered identity and
final scoped preservation checks pass. Verified external archives precede
retirement of 36 completed duplicate builder outputs; original builds/source/
evidence and operational data remain. Whole P2/D1/B1/Q1/H1 and 40 GB remain open.

### P2-E continuation (2026-10-16)

[Explicit native dial evidence](low-resource-lr2-p2p-dials.md) records reproduced
stale admission, foreign-owner cancellation and seed-route omission, lifetime
credits, stop publication and weak callback contexts. Final R2 Mac/Linux 57/57
all-target/native/CLI checks pass (153.42 / 141.60 s); nine fixtures
under each separate ASan/UBSan/TSan pass with controls. The intermediate TSan
report is retained with a reduced stack/heap/live-lock control and fixture-only
correction; no detector suppression or SDK patch was added. Twelve normal-ASLR
VPS components and the matching isolated runtime-container CLI/hash pass.
Five paired common-harness measurements preserve ordinary-allocation, elapsed
and Linux child CPU/I/O tradeoffs. Native Linux 55/1/1 batches use verified
external fixture archives before owned retirement, with unchanged builder floors.
Strict manual/rendered identity, relative links and scoped preservation pass;
the installed publisher and 14 existing containers are freshly verified unchanged.
Forty completed duplicate builder copies are retired only after external
member/hash verification. The guarded TSan compiler interruption and recovery
remain recorded; no reserve or detector setting was weakened.
Source manifest: `947958d848be5906923b8f968d6d19e01a601f8eb3e7b70bce99a6ed16d31275`.
The native 32-job/1-MiB policy excludes GossipSub/RPC/DHT/inbound, SDK/native
known peers, parsed/control/caller aggregates and complete DNS/SDK cancellation.
Full P2, other consumers, D1/B1/Q1/H1 and mandatory producer/40-GB restore remain
open; no mixed LR2 binary is installed on the maintenance publisher.

### P2-F continuation (2026-10-16)

[Peer advice and owned snapshots](low-resource-lr2-peer-snapshots.md) bounds
optional advice and native caller-held projections, removes external SDK reads
and corrects a reproduced query/stop host-pointer race. Final R1 Mac/Linux
59/59 native checks pass (168.53 / 133.42 s), with all targets/CLI, eleven
fixtures per separate ASan/UBSan/TSan, 14 normal-ASLR VPS components and isolated
runtime-container CLI/hash. The intermediate R0 TSan temporary scope-mutex
report is preserved; explicit lifetime controls retain real-cycle/destroy-held
detection. Actual native compiler/dependency provenance verifies the hash-pinned
TSan-only overlay. Five paired samples per equivalent/churn workload preserve
allocation, timing and Linux CPU/I/O tradeoffs. Native 57/1/1 batch fixtures are
externally member/hash-verified before retirement, with unchanged builder floors.
Source manifest: `e100ef823bc7843f26f48a893a36893f4c7768119fab7991c11263769cc818d2`.
Active/SDK peer metadata, implicit work, other queues/caller aggregates, whole
node and full D1/B1/Q1/H1 remain open. No mixed LR2 deployment or qualified
producer/40-GB restore follows from these component passes.

### P2-G continuation (2026-10-16)

[SDK address retention](low-resource-lr2-sdk-addresses.md) replaces duplicate
address storage with one owned representation and shared peer/address/slot/
encoding limits. Retirement preserves real-owner credits. Owned periodic
collection runs on the serialized SDK runner and stops with its generation.
An incremental borrowed-input plan removes the reproduced full-cache scan and
temporary-vector cost. Actual DHT bootstrap, continuing RPC, timer restart,
retained slots, 96 new-peer/eviction fault cases, optional observers and
cross-repository concurrency have scoped regression coverage.

Current Mac/Linux all targets/CLI and 60/60 native tests pass
(187.58 / 143.23 seconds). Five paired common-harness samples per
256/1,024/4,096-input cohort preserve original SDK, intermediate prototype and
current source identities, retention differences and CPU/I/O/RSS/time tradeoffs.
Mac and Linux have different duplicate-hit allocation behavior. The frozen
304-input manifest is
`07342ad3c065c0fe6d8bd632994c7a160b9500304c91468b42cde0b69588cb37`.
Linux native fixture outputs are streamed, member/hash-verified on My Passport
and retired only from their duplicate owned builder directories.

Twelve fixtures under each separate ASan/UBSan/TSan, fifteen normal-ASLR baseline
component commands and offline runtime-container CLI/hash checks pass. The
launcher control and new thin-launcher pairs keep instrumented component RSS
separate from the unqualified whole node. Native operator and Koinos One
developer documentation are updated; strict MkDocs passes. Full SDK metadata/queues/DNS, other consumers, D1/B1/Q1/H1 and actual
producer/full-size 40-GB restore remain open. P2-F results above retain their
preceding source identity. Evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/p2g/resume-20261016-rrm0ukg_/`.

### Lossless storage investigation (2026-10-16)

[The isolated archive experiment](lossless-archive-storage-experiment.md) verifies
all 497 files / 29,721,611,361 bytes of the pinned public input. Forty-two
complete-table comparisons, 41 larger-builder commands and 52 actual-VPS
component commands preserve exact records and measure CPU/read tradeoffs.
64-KiB blocks with Zstandard level 9 / 64-KiB dictionaries avoid the much higher
lookup cost of 1-MiB blocks. Complete-copy physical database size is
24,840,258,810 bytes, 16.422% smaller; fresh read-only reopen, full checksum and
pointwise validation pass across all nine families, including 45,076,063 blocks
and receipts. Source metadata is unchanged. Native queries match all 68
blocks/receipts in six selected ranges. The complete audit verifies all header
IDs, 40,236,770 consecutive canonical heights/receipts and the zero genesis
parent; all 4,839,293 other selected records remain. Public RPC successor-header
agreement anchors the selected ID/root. Signatures and historical execution
replay remain open. This is an external diagnostic
prototype; native defaults/export, whole-node operation and the actual 40-GB
restore remain unqualified.

Owned input, candidate, programs, measurements and terminal coordinators:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/disk-465t9_bg/`.
Remote duplicate fixture SSTs were retired only after complete external
member/hash verification. No original backup, runtime source, operational data,
publisher or producer was changed.

### Persistent block compression policy (2026-10-16)

The subsequent [native policy checkpoint](native-block-compression-policy.md)
adds optional block Zstd level/dictionary parameters at normal and bottommost
levels. Both default to zero; no existing table is automatically rewritten.
Dictionary training uses finite per-builder settings rather than an unlimited
sampling buffer. Config, native storage/reopen and filesystem-preserving CLI
tests cover the change. All targets/version/help and all 60 Mac/Linux native
tests pass. The actual VPS passes nine native command invocations, including
two configured offline opens and normal-startup rejection. The initial Linux
space refusal and VPS harness over-assertion are retained; unchanged binaries
pass after the corresponding environment/harness corrections. Component peak
RSS is 40,828,928 bytes; minimum sampled host `MemAvailable` is 3,404,636,160
bytes with no swap/OOM. These are component measurements. The frozen 304-input source
manifest is
`5f89837ed3d002ece2355865d6bde342f50441581e15b8719603c89fe09b247a`.
This retains future writer policy, not a supported exporter or whole-node RSS
bound. Whole-server restore, historical execution, sustained operation and
producer gates remain open.

### Native full-archive rewrite (2026-10-16)

The [native rewrite checkpoint](native-archive-rewrite.md) adds explicit offline
bottommost rewriting and empty host properties for newly written tables.
Source manifest:
`c3b1cc4e2dad577874d5e0bb0afd8d303a89a5afe0b1d57ba11d016cd99fdb10`.
Mac/Linux all-target builds and all 60 unique tests pass across retained
attempts; the first Mac suite has two timeouts during concurrent external
rewrite I/O. Both unchanged executables pass with that owned process briefly
stopped, then the same rewrite resumes. Linux passes all three complete batches.
The mandatory VPS passes 12 native component commands, including explicit
rewrites and invalid-existing-DB controls. Component peak RSS is 52,346,880
bytes and minimum sampled host `MemAvailable` is 3,418,382,336 bytes, with no
swap or OOM-counter increase.

The actual native full rewrite completes at 24,803,500,585 working-DB bytes,
with 45,076,063 physical block entries and empty host fields in every new SST.
The original 497-file input metadata is unchanged. The real native backup
completes with matching checkpoint identity, 385 files and 24,802,854,237
payload bytes. Its database is 24,802,203,337 bytes, 16.550% smaller than the
original. All 385 native object SHA-256/size checks, both complete RocksDB
checksum scans and full pointwise comparison across all nine families pass.
All 46,458,363 visible records are preserved, including 45,076,063 block/receipt
records; only the two exact expected operational metadata values differ.
The matching raw block digest binds the complete previous canonical proof.
The first helper's object-shard-path error is preserved and corrected without
changing/recreating that backup. Native public promotion and HTTPS metadata,
object-size and public-config verification now pass for the isolated source. The
stable source/publisher, recovery reserve and original records remain protected.
Full-server restore, whole-node memory, history execution, catch-up/recovery and
producer gates stay open.

Owned generated work and durable coordinator handles:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/native-export-k3ky3m61/`.
Koinos One receives documentation coordination only; no package/native pin
integration or producer activation is implied.

The qualification object route now passes actual SFTP fsync/rename/stat and
HTTPS-byte checks. Native read-only SFTP listing passes with the same backend,
and its byte-capacity metric agrees with OpenSSH's KiB units. The gated
publication coordinator transfers verified non-config objects before native
metadata promotion; the native promotion API itself assumes an existing object
tree. Actual VPS native preflight, controlled stage interruption and resume
pass, including unchanged reuse of 18 completed files and native body checks.
Available root space after resume is 11,312,304,128 bytes within the
39,956,590,592-byte ext4 filesystem, above the unchanged reserve. Offline
validation/held activation now completes with all three full DB checks and the
same directory inode preserved. Two short observer runs and clean restart pass,
with recent-head and captured legacy root/height agreement. Corrected whole-device
accounting exceeds the strict 40-GB budget by up to 628,682,752 bytes; the actual
40,961,572,864-byte device retains its unchanged 10-GiB reserve. Strict disk,
sustained/recovery/producer/history gates remain open; see the
[reference VPS report](reference-vps-observer.md). Verification session `60476`
is terminal success; do not restart its full read. Original source, stable
publication, operational data, memory ceilings and disk reserve remain protected.

### Direct restore read capacity (2026-10-16)

The [bounded read-buffer change](direct-restore-read-buffer.md) reserves
64 KiB within the existing 128-KiB wire maximum. Five paired samples on the
Linux builder verify the same 66,533,451-byte HTTPS object with checked
hash/write/fsync. Median paired CPU decreases 47.67%, and kernel write calls
decrease from 129,950 to 4,062 per sample. Network wall time varies; no download
speedup is established. The new 304-input manifest is
`af826d523daa1d196808b7a1e69effaadfe6e119bfa4c9be34a15af1b24d3553`.
Mac/Linux all-target builds, focused checks, all 60 native tests and version/help
pass. Five paired reference-VPS measurements also pass: median paired CPU
decreases 73.02%, wall time 71.56%, and write calls 96.87% for this object.
Strict Koinos One manual/rendered checks pass. The complete restore keeps the preceding frozen executable.
No Q1/B1/H1/producer or whole-server restore gate closes from these measurements.

### Resident-memory reporting (2026-10-16)

The [Linux RSS correction](linux-rss-counter.md) reads resident pages instead
of virtual pages. Kernel HWM/timed peaks already supply independent evidence
for the reference observations. That 306-input manifest is
`c073ab12836d01ddd761e8146ed829792fee0f92b77c622059cf798951208c3b`.
Mac all targets/CLI and 61/61 CTests pass. Linux all targets/CLI and all 61
unique tests pass across retained attempts. The synthetic comparison initially
refuses staging because retained generated pairs leave less than its native
recommendation; a retry prelaunch also refuses. Externally verified duplicate
retirement restores headroom and the unchanged single test passes. No reserve
or capacity requirement changes. The reporting correction claims no RAM saving.

### Optional larger block dictionary (2026-10-16)

The [new bounded option](larger-block-dictionary.md) accepts 128 KiB with
unchanged defaults and training/buffer ceilings. Five lossless triplets quantify
smaller files against higher rewrite CPU; three additional native SSTs also
pass complete comparisons. Current manifest
`ca0a6b4e890474528e385c3149f76250d38787d2cb41093059858a9d97290e7b`
passes Mac/Linux all-target/CLI and all 61 native tests. Full native rewrite,
the real native backup API, all 383 object hashes/checksums and pointwise
equality of all 46,458,363 records now pass under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/dictionary-policy-20261016/`.
Independent COW input placement avoids an initial development copy; it is
not ext4 savings. The new 24,113,527,301-byte DB saves 688,676,036 bytes against
the prior actual native backup. Five VPS component pairs quantify rewrite and
warm-read CPU/latency penalties. Native promotion and all 383 HTTPS object-size
checks now pass in the new qualification namespace; stable public latest remains
unchanged. A new strict complete restore remains required.

Only the newly generated, fully compared old prototype/checkpoint and linked
working duplicate are retired. Protected original/native backups, reports,
full inventories, unique controls and exact representative tables remain.
Completed Mac fixture trees are archived and independently verified before
retirement. These development-space savings do not count toward VPS capacity.

The previous RSS binary completes a clean one-hour observer run below 249 MiB
RSS, with host `MemAvailable` at least 3,218,644,992 bytes. All 60 RSS logs
agree with kernel samples within 816 KiB. Captured final height/root
40,262,636 matches legacy after reference LIB passes it. The strict whole-device
shortfall remains 629,100,544 bytes; no sustained, producer or history gate closes.

## Required 40 GB restore outcome (2026-10-15 clarification)

Restoring the complete backup within the designated VPS's **40 GB whole-server
disk budget** remains required. Under the operator's 2026-10-18 amendment, hold
`max(8 GiB, database bytes / 5)` free inside that budget. The final 312-input
candidate and selected full-history backup pass actual interruption/resume,
complete restore, validation, held activation/reopen and advancing observer
head. Restore peaks at **37,915,475,968 bytes**; the measured retained-reader
compaction case peaks at **39,352,967,168 bytes**, both including the full reserve.
All 45,076,063 selected original block/receipt records remain byte-identical.
This closes the selected-input restore/disk gate for that candidate and workload;
it does not close all Q1, producer, future-growth or arbitrary-compaction gates.
See the [explicit acceptance contract](../implementation/issue-9-goal.md#whole-server-restore-acceptance-2026-10-15-clarification)
and [current completed VPS evidence](vps-8gib-recovery-reserve.md). The earlier
[source/capacity assessment](restore-40gb-current-baseline.md) retains the
4,140,425,216-byte deficit and pre-payload refusal for its older inputs.
Source signatures are absent and remain explicitly distinguished from pinned
HTTPS/operator provenance; full historical execution and broader qualification
remain open. Earlier refusals are preserved and are not relabeled as passes.

## Current development branch (2026-10-06)

The user requested a dedicated branch for the memory improvements. Local
`codex/lr2-memory-hardening` is now active, created from
`cfd2eb83b8080ca49911555108ce05f0a9068f01` without a commit, push or merge.
`main` stays at `8a13a5084b6874d9f61475ea6a20860183e4b2cb`; the previous
`codex/reward-reinvestment-macos` branch reference is unchanged.

Before/after snapshots verify identical contents for all 191 pre-existing
dirty/untracked files, unchanged HEAD/index and unchanged branch references.
Evidence and reproduction script:
`/Volumes/My Passport/teleno-lr2-EEMOFW/branch-isolation-zdd3KT/`
(`before.json`, `after-switch.json`, `snapshot.py`). The final snapshot
`after-docs.json` permits only the five branch-policy documentation updates:
AGENTS/CLAUDE, this matrix, the active plan and project memory.

The worktree contains earlier backup/restore and other pre-existing changes,
not only LR2. Switching branches does not permanently associate uncommitted
files with that branch. Review scope/dependencies before any authorized commit;
do not describe this as a committed, memory-only changeset. This administrative
change requires no runtime build and closes no LR2 correctness or Linux gate.

## Starting evidence

### Quality-first acceptance clarification (2026-10-04)

The user explicitly requires the final node to be more robust and operationally
secure, not only lower-memory. Apply this jointly to every matrix row: preserve
protocol validity, durable data, authenticated restore and protected admin
surfaces; verify relevant failure/overload/disconnect/shutdown/restart behavior
and no partial publication, duplicate effects or false success. Record resource
tradeoffs and distinguish verified corrections from expected benefits.
Memory savings cannot compensate for a failed required integrity, lifetime,
recovery or operational-protection gate. Existing D1/Q1/H1 blockers are not
waived. This is an acceptance clarification, not authorization for external
probing, live nodes, other packages or publication.

Branch `codex/reward-reinvestment-macos`, HEAD
`cfd2eb83b8080ca49911555108ce05f0a9068f01`, dirty
development `1.3.0-dev.0`. Evidence root:
`build-single-copy-evidence/lr2-completion-qKjj7h/` (ignored, local).
`worktree-before.sha256` snapshots every pre-existing dirty/untracked file;
README and the high-throughput plan must remain unchanged.

The local CMake cache names this repository, Release `-O3`, tests and cpp-libp2p
ON. Apple Clang 16, macOS 26.5 (25F71), arm64, 10 CPUs, 16 GiB RAM. cpp-libp2p
`c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`, cached Boost 1.83/RocksDB 8.8.1.
Only about 15 GiB free was observed; avoid multiplying full diagnostic builds.
Focused baseline: **5/5** cache, state DB, pending-root, P2P sync and mempool
tests pass, 16.01 seconds (`baseline-focused.log`). This is not a full rebuild.

Docker info exits 1; no local daemon. Read-only `colima list` shows only a
stopped, unrelated Vortex VM; `limactl list` shows no instance. Neither was
started, installed or changed. A functioning isolated Linux diagnostic/whole-
machine environment and eligible pinned historical input remain prerequisites.

### External development storage (explicit follow-up authorization)

The user subsequently requested My Passport for development data. The mounted
volume is `/Volumes/My Passport`, APFS, writable, with about 381 GiB available
at inspection. New build trees, fixture basedirs, temporary files and evidence
belong to the task-owned `/Volumes/My Passport/teleno-lr2-EEMOFW/` directory.
The current turn's evidence was moved into its `evidence/` subdirectory; the
ignored repository evidence path above is a symlink to it. Source/docs remain
in this repository. Existing dependency installs and earlier evidence/chain
data are not relocated or deleted. Cached dependencies may be read as inputs;
new diagnostics must use the external build and `TMPDIR` there. This explicit
authorization changes the earlier repository-local scratch constraint, not
source ownership, live-node scope or qualification requirements. APFS/USB
results are not Linux/ext4 resource evidence.

The user's follow-up explicitly prioritizes the Linux VPS over the Mac. The
primary optimization/qualification target is Ubuntu/x86_64/ext4, 2 vCPUs and
4 decimal GB physical RAM. Mac diagnostics may support correctness investigation
but cannot satisfy Linux whole-node, catch-up, persistence or recovery gates.
No live-server or VM-creation authorization follows from this preference.

## Requirements and proof gates

| ID / owner | Current accounting / effective policy | Remaining implementation or experiment | Invariants and regression gate | Measurement / artifact / status |
| --- | --- | --- | --- | --- |
| M1 / native mempool + transaction work | R15 shares lifetime-owned ordinary admission across records, retained forks/indexes/tombstones, nonce/RC/count reads, staging, native work/candidate and selected output owners. Proposed tested ordinary policy: 768 MiB; HTTP fallback: separate 64 MiB; gRPC refusal controls: separate 1 MiB. Instrumented canonical/recovery, fixed startup and internal inspection work remain counted with explicit exemption. | None for the scoped R15 M1 gate. Keep R1 ingress/other services, P2 SDK queues, V1 VM, S1 non-mempool state and wider D1/Q1/H1 work separate; these policies are not a universal heap/RSS ceiling. | 104/104 normal, 2 CLI, 63 focused ASan/UBSan and 61 default-TSan cases pass with controls. Eight owner groups have hash-verified regression references. Exact-source 100-block/12,800-transaction creation/reopen and 8,852 concurrent queries pass; peak RSS 705,712,128 B. The exact full-history observer passes 3,600 error-free mixed-read cycles, 31 independent comparisons, clean restart and interruption/recovery; peak 419,278,848 B, minimum target-adjusted host MemAvailable 2,921,375,744 B, strict disk margin after reserve 538,800,128 B. | Five equivalent pairs per measured workload preserve neutral/worse results; HTTP fixture median peak RSS falls by 7,917,568 B, without whole-node/single-edit attribution. [R15 report](low-resource-lr2-m1-r15.md), [final inventory](../implementation/issue-9-m1-owned-memory-inventory.md), [objective](../implementation/issue-9-m1-aggregate-goal.md); **M1 complete for exact R15** |
| P1 / P2PNode sync workers | Now owns connected and retired generations; count limit 64 includes draining workers/current peers; bytes are not capped | Implemented join ownership, safe-boundary cancellation, bounded overlap, startup rollback and drained stop; real transport cancellation remains P2 | Baseline failure reproduced; local gated RPC/batch, 200-cycle churn, capacity/reconnect/zero/start-failure/stop tests pass; already-admitted application may finish | Five after-only process samples, high water 2/final 0; [report](low-resource-lr2-workers.md); **implemented and functionally verified; supported race/lifetime qualification pending** |
| P2 / transport + peer bookkeeping | Accepted IDs; owned RPC/deadlines/local refusal; finite error/optional advice; explicit dial and projection lifetime credits; one-owned SDK addresses with shared limits and owned collection | Other SDK/protocol/key/DNS metadata, implicit work, aggregate decoded/output/control/caller ownership and complete lifecycle/watchdogs | Failure-atomic address/advice/index publication, protected roots, retained-slot/old-generation accounting, real RPC under bootstrap saturation, collector/restart/concurrent statistics and detector controls | [P2-G](low-resource-lr2-sdk-addresses.md), earlier checkpoints; current Mac/Linux 60/60, paired mechanisms, twelve fixtures per separate sanitizer, fifteen baseline commands and runtime CLI/hash pass; **full P2 open** |
| R1 / JSON-RPC | Owned sessions/deadlines, <=64 slots; body 1 MiB/header 8 KiB; completed body-capacity attribution/release; per-item batch serialization and moved results | Complete simultaneous service/JSON/protobuf/serialization/transport accounting and compatible output admission without truncation or false post-commit refusal | Full replies, pipeline/CORS/batch reuse, exact bytes/order and later dispatch after strict UTF-8 failure, slow readers/stop; supported races remain | [R1-A](low-resource-lr2-rpc-bodies.md), [R1-B](low-resource-lr2-rpc-batch.md); scoped retention/working-set reductions, not final-output cap or Linux node RSS; **local corrections verified, R1 open** |
| V1 / Fizzy backend | 32-entry cache plus opt-in attributed parsed capacity; pinned-layout inventory/metrics; failure-atomic optional publication; strong ownership/coalesced cold loads/fresh instances; corrected pinned contexts/local allocation errors. Five tracked and five probe-disabled Linux-builder triplets plus five actual-VPS triplets quantify a distinct-code working set and reparsing cost | Caller/queue/active-instance and physical allocator accounting/policy; representative operational policy selection | Checked layout/limits/replacements/staged faults/held-owner/zero-allocation hits, execution/nested metering/cleanup; integrated ASan/TSan/history remain | [V1-A](low-resource-lr2-vm-lifetime.md), [V1-B](low-resource-lr2-vm-allocation.md), [V1-C](low-resource-lr2-vm-capacity.md), [Linux cache policy](linux-vm-cache-policy.md); smaller cache raises CPU/latency in the cyclic fixture; **cache axis verified, V1/LR2 open** |
| S1 / state backend | Reads carry immutable owners; map clones share immutable payloads/replacement-on-write; retained object-cache payload <=64 MiB, oversized values bypass retention | Physical fork/delta/iterator/caller-held accounting; supported race/lifetime and historical qualification | Read-caused deletion reproduced without dereferencing freed storage; owned direct/iterator reads, eviction/replacement/erase/close, clone isolation and roots/tombstones pass locally | [S1 report](low-resource-lr2-state-ownership.md), [two external key layouts](state-cache-key-ownership-prototypes.md); standalone diagnostics, paired builder series and 45 interleaved target-VPS measurements pass, with host/workload-dependent CPU tradeoffs; no native layout change or physical capacity bound. Caller-held/metadata bytes are additional; **local ownership correction verified, full S1 qualification open** |
| D1 / diagnostics | Current P2-G clean/known-fault controls and twelve fixtures per separate ASan/UBSan/TSan, with controlled DI mutex lifetime overlay | Complete integrated lifecycle/dependency/thread coverage. B3 protobuf fault leak is corrected in B4 and its native fault test passes. Mixed-RTTI wrapper cause is independently reproduced; exactly two diagnostic commands align, all other commands/native CLI bytes unchanged. Fifty-two B4 instrumented entries pass, including activation at 215.608 seconds under the documented 300-second diagnostic property and the corrected separate TMPDIR/artifact-root layout; the run then fails in cleanup fixture setup. The corrected fixture exposes a cleanup decode leak. B5 exposes 640 bytes / 20 leaks on added unknown-field inputs. B6 corrects pinned protobuf publication ownership; the identical expanded native object and minimal known-fault controls pass. The rebuilt normal/instrumented Linux runtimes and both new tests pass; current CLI builds and full integrated regressions are still required | All failures/fixtures preserved, all original checks/detectors and RAM/disk/OOM limits retained; ordinary activation timeout remains 180 seconds. Child-only 2.5-GiB/zero-swap scopes, distinct owned ext4 roots, per-case verified offload | [Copy ownership](mempool-admission-copy-ownership.md); full ASan/UBSan, TSan, dependency and lifecycle qualification **open** |
| Q1 / representative whole node and producer | Complete native backup/publication, interrupted/resumed full restore, DB validations and held activation pass. B3 one-hour observer advances 34,955 blocks and matches stable legacy head/root; peak 634,728,448 bytes, host available >=2,837,860,352. Clean 90-second restart and actual-VPS 1,000-block empty federated producer/60-block replay pass. Later B4 builder fixture also passes 100 blocks x 128 signed no-operation transactions with the real mempool and exact 60-block clean-reopen replay; kernel peak 738,041,856 bytes. This is separate from the actual-VPS nonempty/PoB gate | Pinned full-history input, explicit absent signatures; mandatory 2-vCPU/4-decimal-GB Linux/ext4 baseline. Older C073 strict whole-device peak exceeds budget by 629,100,544 bytes. B3 full restore/validation/held activation passes at 39,760,650,240 bytes including reserve; one-hour runtime peak 39,877,636,096. Subsequent 24-hour attempt stops cleanly after 21h07 when a strict-disk peak reaches 40,011,091,968 bytes; native process verified closed, original failure retained | Further lossless disk savings; fresh 24-hour/seven-day mixed RPC/P2P/VM/state and producer candidate/PoB work, flush/compaction/abrupt recovery/overload cleanup | Steady RSS <=2 GiB, peak <=2.5 GiB, host MemAvailable >=768 MiB, no OOM/swap; [VPS report](reference-vps-observer.md), [controlled producer](controlled-native-producer.md); **full qualification open** |
| H1 / compatibility | Starting revision 30,504,141 independently anchored; fresh complete 2,285,238-block receipt audit through 32,789,379 has no unexplained mismatch; native/tagged-reference full-VM results match in both 62-block boundary windows and three Linux 50,000-new-block ranges through 30,654,141, independently anchored with cold-state metadata proofs; all 60 overlap rows match all five fields including LIB. Fourth pair toward 30,704,141 stops at the global OOM guard after the separate compiler-scope event; its partial state/trace is fully preserved | Full native/reference differential and full-range VM qualification. First two VPS attempts stop at unchanged disk guard; shared-backend WAL64 checked replay completes all 2,285,238 blocks, matching the independent terminal ID/root and all nine tagged-reference common checked-range fields, with zero native source writes/broadcasts and no swap/OOM change | No unexplained root/receipt/resource/head/LIB/index differences; no weakened validation or history removal | [Fresh historical evidence](historical-checkpoint-eligibility.md); complete historical execution and differential H1 remain **open** |
| B1 / current native build | B4 arena source `93a2cc29...`: all targets, CLI and all 61 normal entries pass on Mac/Linux; Linux combines 55 original passes, two R1 and four R3 ext4 passes with unchanged source/ELF/commands and all failures retained. Earlier WAL source `b3f83775...` has its own complete Mac/Linux checks. Actual-VPS full observer-profile preflight, four current native fixtures and seven isolated existing-image runtime cases pass. Five WAL mechanism pairs complete on builder/VPS; preceding scan-reuse pairs retain their separate source | Complete supported-platform dependency/release workflow, repository Dockerfile build and integrated regressions; latest-source full VPS restore/runtime checks remain separate | Frozen 306-input manifests, exact binaries, roster/logs, verified archives before owned fixture retirement; preserved failures and unchanged reserve; instrumented activation uses its explicitly recorded diagnostic timeout profile | [WAL report](wal-retention-budget.md), [container runtime](current-container-runtime.md), [scan-reuse report](direct-restore-scan-reuse.md); earlier full restore/one-hour observation retain separate identities; **B4 regressions and B6 all-63 normal Mac checks pass; full B6 Linux/integrated checks and B1 remain open** |
| O1 / documentation | P2-G report, native operator/config/diagnostic/changelog/goal/matrix and Koinos One networking handoff updated | Maintain remaining package evidence and final documentation; no GUI/package/submodule integration | English, identical AGENTS/CLAUDE, prior mixed changes preserved; exact source/native identities and separate installed publisher | Strict external MkDocs/rendered identities and scoped preservation pass; **whole-goal O1 remains open** |

Statuses describe requirements, not goal-tool status. A blocked input does not
close LR2. Strict entry/byte bounds, soft thresholds, temporary working sets,
caller-held memory and unavailable metrics must be reported separately. LR5's
24-hour/seven-day full-server soak remains a separate milestone; no completion
is claimed here. Whole-audit TODO remains open.

## Checkpoint log

### V1-D / D1 investigation contract: host result-count alignment

Before changing runtime behavior, exercise actual thunk/host writers with an
unaligned, in-bounds four-byte result-count address and reproduce any undefined
typed store with working UBSan. Characterize unchanged native bytes, return
codes, write ordering, overlapping arguments/results and metering. A correction
must write at the original sites without rejecting an accepted byte address or
deferring writes through an aligned temporary. Keep the host signature, bounds,
VM limits, execution policy and consensus accounting unchanged; no allocation
or memory-saving claim follows from an alignment correction.

D1 separately tests sanitizer startup with a standalone bounded program that
has no Teleno dependencies or allocator replacement. Existing Apple Clang 16
and Clang 14 (explicit installed SDK) both compile, but clean ASan/TSan programs
terminate before `main`; positive controls cannot establish working diagnostics.
The runtime/OS cause remains unattributed. Integrated ASan/TSan is blocked,
not passed or waived; independent UBSan and native investigations continue.

Starting 184 hashes match finalized V1-C. New evidence and temporary data use
the fresh owned `/Volumes/My Passport/teleno-lr2-EEMOFW/d1-diagnostics-u6pnEJ/`
subrun; existing exact-source external builds are reused with explicit external
environment variables. Linux/whole-node/history qualification remains mandatory.

Current V1-D focused native and UBSan suites both pass 3/3 (1.26 / 1.36 s).
Complete deterministic output SHA256 matches the preserved pre-correction
fixture (`c3dbc08cc72002ea76b4b16c77f9e902965309c13b0f04a2721f3b5db51a4b3e`).
Required dependency workflow exits 0 (102.89 s). Current broader all-target/full
CTest, selected integrated UBSan, CLI and strict documentation gates remain
pending; earlier V1-C results are not proof of this new source. The investigation
and correction are progress, not V1/D1/LR2 closure.

### V1-C implementation contract: retained parsed-module capacity

Replace entry-count-only retention with an additional opt-in byte-capacity
policy, preserving the 32-entry/default unlimited-byte behavior. Inventory the
exact pinned immutable Fizzy module containers, including nested vectors and
names; checked accounting overflow must bypass disposable retention, not change
execution validity. Charge parsed capacity, guard and indexed key/value storage
conservatively; disclose allocator/control-block/tree-node overhead and caller-held
owners separately. Do not claim a physical heap/RSS ceiling from this charge.

Retain failure-atomic insertion/replacement, unique LRU membership and allocation-
free warm hits. An oversized module remains owned/executable without flushing
unrelated entries; no mutable-instance reuse, metering/limits or consensus change.
Wire strict configuration through the actual native controller/backend and expose
cache-only diagnostics. Test exact/oversize/zero/unlimited boundaries, replacement,
eviction, allocation failure, concurrent readers and execution/metering/cleanup.
Measure five paired unchanged/limited-workload processes, including CPU costs of
additional parsing; no Linux defaults selected from Mac samples. Queue/active-
instance/caller/whole-node and supported Linux/history/race gates remain required.

Starting 175 dirty/untracked hashes in `v1c-worktree-start.json` match finalized
R1-B. Generated data remains on the owned My Passport run; exact source caches
and preserved baseline VM executable are verified. [V1-C report](low-resource-lr2-vm-capacity.md):
strict config reaches native backend; checked all-container charge, exact/oversize/
larger/smaller replacement, staged allocation faults, held-owner/clear/concurrent
capacity and nested metering regressions pass. Five comparable groups show
default retained requests +384 bytes; at 512 KiB retention falls to 395,325
requested bytes but total requests and reparsing time rise. No Linux default
selected. Focused 4/4, cached/separate 49/49 (179.73 / 172.94 s), UBSan 16/16
(55.91 s, vptr/prebuilt exclusions), dependency/all-target/four CLI and strict
Koinos One MkDocs pass. Docker daemon absent; Linux/history/ASan/TSan and full
instance/queue/caller bounds remain. **Cache checkpoint locally verified;
V1/LR2 remains open.**

### R1-B implementation contract: batch result serialization

Before changing dispatch, characterize aggregate batch JSON ownership using real
loopback HTTP and a bounded fake service. Do not accumulate all result trees
plus the final serialized body. Serialize and release one result at a time into
the complete HTTP body, preserving exact compact bytes, item order, IDs, errors,
headers/keepalive and one invocation per item. Prepare the success envelope before
service invocation and move the result into its pre-existing slot, not copy it.

Preserve the old batch behavior for deterministic invalid-UTF8 serialization:
all later items still execute before serialization failure closes the connection;
no partial HTTP output, automatic retry or rollback claim. Test mixed mutating/
read-only batches, empty batches, escaped/non-ASCII/numeric IDs, service errors,
byte equality and final ownership cleanup. Measure five before/after processes,
with separate warm-up and server-thread allocation/RSS exclusions disclosed.

This is the serialization working-set prerequisite, not a complete output byte
reservation or network streaming design: the final wire body remains retained
through write completion. Aggregate service/DOM/input/transport/output admission
and Linux qualification remain required. Initial 173 hashes are
`r1b-worktree-start.json`, matching final R1-A; generated data stays under the
owned My Passport run. [R1-B report](low-resource-lr2-rpc-batch.md) and evidence
now verify the scoped correction: 19 checkpoint files, 156 other starting files
unchanged, zero removed; full lists/hashes in the source manifest. Five paired
processes reduce peak tracked requested bytes 21,960,048 -> 14,265,952; timing
ranges overlap and the final string remains unbounded. Full exact/failure/cleanup
regressions pass. Required workflow/all-target builds/four CLI checks exit 0;
cached/separate dependency CTest 48/48 (122.44 / 161.64 s), UBSan 14/14 (53.67 s),
strict Koinos One docs exit 0 (0.34 s). Initial cached 47/48 and unchanged backup
test repeat remain recorded with a qualified stale-status explanation. Docker
daemon absent; ASan/TSan/Linux/history not qualified. **Local checkpoint verified;
R1/LR2 exit NOT satisfied**. No live node/VM/VPS, publication or package change.

### R1-A implementation contract: completed HTTP body lifetimes

Inspect and reproduce request/response body retention after completed dispatch/
write on healthy keepalive. Add synchronized attribution of completed-request
and materialized-response string capacities, including pending handler owners
until destruction; this is not incomplete ingress, JSON/protobuf working-set,
headers, transport buffers, allocator heap/RSS or a byte ceiling. Release request
ownership after service completion and response ownership only after write
completion. Use destruction, not implementation-dependent empty-string assignment,
to release capacity portably on Linux and Mac. Preserve pipelined buffered bytes,
complete responses, CORS/keepalive/deadlines and admitted service completion.

Proof gates: large complete response, held request during gated dispatch,
success/error/batch/health reuse, slow-reader and cancellation/stop cleanup,
five equivalent before/after processes with capacity/RSS limitations disclosed,
focused/broader tests and supported sanitizer checks. No response-size cap,
post-commit overload error or protocol change. Real aggregate service/serialization/
pending-write admission remains R1, not replaced by this checkpoint. Starting
172 dirty hashes are `r1a-worktree-start.json`; all generated data stays on the
owned My Passport run. Linux VPS remains primary. Status: **local checkpoint
verified, R1/LR2 remains open**. Baseline idle-release regression exits 1; final
five-before/five-after comparison removes 18,087,935 idle response-capacity bytes
with unchanged body-capacity high water and no proven speed gain. Whole fixture
RSS includes client/equality allocations, not isolated node/Linux evidence.
Strengthened tests cover coexisting request/output, two pending responses,
pipeline/error/batch/close and cancellation/admitted dispatch. Focused 1/1,
UBSan 13/13, final cached/separate full suites 47/47 (126.23 / 160.15 seconds),
required workflow/builds/four CLI checks and strict Koinos One docs pass. Initial
include error, overlap-telemetry refinement and same-tree build overlap followed
by a serialized repeat are preserved. Eighteen Teleno files/two docs-only GUI
handoff paths changed; other 155 starting dirty files remain unchanged. Exact
commands/hashes/artifacts are in the report; no Linux/history/ASan/TSan or
container qualification is claimed. Complete aggregate R1 policy still required.

### V1-B implementation contract: local VM allocation failure

Linux Ubuntu/x86_64/ext4 on 2 vCPUs and 4 decimal GB physical RAM remains the
primary target. Mac runs are diagnostics, not Linux capacity evidence. All new
generated sources, dependency builds, temporary files and evidence use the owned
My Passport run. Initial hashes are `v1b-worktree-start.json`; they match V1-A.

Correct the reproduced pinned Fizzy `noexcept` context-construction allocation
termination in a Teleno-owned, hash-pinned build recipe shared by all three CMake
entrypoints. Keep existing installed dependencies and other repositories intact.
Preserve upstream compiler/platform checks in a separate child build, ABI,
module ownership, execution results, depth and metering. No mutable instances
are shared. Verify source/patch identity and fail closed on unexpected inputs.

Native parse/instance/context allocation failure must propagate as local
`std::bad_alloc`, never as a consensus reversion or a successful receipt. Keep
deterministic invalid-module/instantiation/trap behavior unchanged. Exercise
both context constructors, normal metering/depth, failure cleanup/retry and
native execution allocation paths; then focused and broader regression gates.
These are scoped allocator faults, not machine-wide OOM recovery or historical
parity. Additional byte/instance/queue policies and Linux qualification remain
open. Status: **local implementation/focused correction verified; complete V1/LR2
exit criteria not proven**. [V1-B report](low-resource-lr2-vm-allocation.md): 35
real-VM allocation boundaries, six controlled dispatch cases, local P2P refusal/
redelivery without peer penalty, both context constructors and pinned recipe.
Focused 5/5 and UBSan 13/13 (12 native plus recipe; Fizzy instrumented, other
prebuilt dependencies/vptr excluded) pass. Current cached full CTest 47/47,
133.06 seconds; separate-dependency full suite 47/47, 168.39 seconds. Required workflow,
all-target builds/four CLI checks and strict Koinos One docs pass. The report
retains an interrupted env-placement error (143) and verified relocation of
17 new synthetic directories to My Passport; it is not erased or counted as a
pass. Linux/history/ASan/TSan/container qualification remains unexecuted/blocked.

### V1-A implementation contract: parser ownership and concurrency

Before runtime edits, verify the exact pinned Koinos Fizzy fork and reproduce
raw parsed-module ownership leakage on shared-owner allocation failure. The
installed public C header's ownership comment is stale for the pinned fork;
actual instances borrow immutable modules and runners must retain their owner.
Do not clone or share mutable instances to resolve a documentation discrepancy.

Implement strong ownership from parse return through guard allocation; cache
publication must be optional and failure-atomic. Coalesce same-ID cold lookups
through a serialized miss/parse boundary, release that boundary before instance
creation/execution, preserve allocation-free warm hits and nested host calls.
Quantify diverse cold/warm/no-cache and concurrent parser/instance working sets
with actual metering/results, failures and lifetime cleanup. A source/wire count
must not be mislabeled parsed heap; cache-byte/global instance policies remain
V1 requirements until real attribution and compatible admission are established.

Evidence prefix `v1a-*` under the existing owned My Passport run. Initial 159-file
manifest `v1a-worktree-start.json` matches the final M1-G hashes. Read-only pinned
Fizzy source is a fresh owned `v1a-source-I09vWF/` child, archive SHA1
`d4e682dca504a831b30274fbec0b154b8b7adfd1` (matches Hunter), commit
`b9bf7feaa8009a3d4f4bdd49245a5cd55d122055`; neither dependencies nor another
repository are edited. Linux remains primary, all generated data stays external.
Status: **local correction verified; full V1/LR2 remains open**.

V1-A evidence: empty-module shared-owner allocation failure leaks one 344-byte
ordinary C++ request before correction and zero after. Four cache-publication
faults preserve existing owners; optional retention failure still returns the
loaded module. Eight gated cold callers share one owner while a warm caller
progresses; 800 concurrent executions and 20 nested calls preserve metering/
fresh instance memory. Five serialized single-thread samples show unchanged
tracked heap and no RSS improvement; a controlled lookup-strategy burst reduces
8 same-ID parses/owners to 1. This is a mechanism test, not natural/Linux latency.
Distinct cold IDs serialize; queues/instance/parsed-byte policies remain open.
Pinned Fizzy's uncaught allocation inside noexcept context creation is separately
reproduced with isolated terminate-handler exit 77 and remains unfixed. The
[V1-A report](low-resource-lr2-vm-lifetime.md) records commands, exclusions,
current build gates and exact blockers. Linux primary/data-placement unchanged.

V1-A final local gates: serialized cached full CTest **46/46**, 120.20 seconds;
separate dependency **46/46**, 162.24 seconds; separate UBSan **12/12**, 45.39
seconds (vptr/dependencies excluded). Required workflow/all-target builds/four
CLI checks exit 0. First cached full attempt is 45/46 after a preflight subprocess
15-second startup timeout; unchanged isolated recheck is 1/1, 1.23 seconds.
No attributed Mac startup cause, relaxed deadline or erased failure. Strict
Koinos One docs exit 0, 3.19 seconds. Docker socket absent, smoke unexecuted.
Exact current source/binary hashes, 20-file checkpoint and two-doc handoff are
in the V1-A report/external artifacts. Full goal exit remains **not satisfied**.

### M1-G implementation contract: pending-record ownership

Before implementation, select an incremental lifetime-owned budget for immutable
serialized pending records. Admission serializes once and shares the same owner
across eligible forks; clones, squash and in-memory LIB publication must retain
that owner and its lease. Reserve before encoding; all failed preparation frees
credits. Expiry of head records must not free credits retained by ancestors.
This does not prune ancestors, block canonical validation, evict accepted work,
scan every fork on each admission, or select Mac-derived VPS defaults.

Opt-in independent owner-count/string-capacity limits default to zero/unlimited.
They bound these record owners, not map keys/indexes/nonces/resources/tombstones,
all temporary protobuf/serialization buffers or total heap/RSS. Those additional
budgets remain M1 requirements; this is not a substitute for aggregate whole-node
proof. Local refusal must retain JSON-RPC/gRPC/P2P retryable classification with
the correct reason, rather than claim protocol invalidity or penalize a peer.

Planned proof: independent accounts, exact boundaries, fork sharing, ancestor/
LIB/caller lifetimes, capacity refusal and unfailed retry, nonce conflicts,
concurrent counters, allocation-failure cleanup and original receipt/root/
reopen regressions. Five identical before/after fixture processes must record
owner inventory, allocation and RSS limitations/tradeoffs. Evidence prefix
`m1g-*` under the existing owned My Passport run; initial 154-file hashes are
`m1g-worktree-start.json`. Status: **implemented and locally verified;
complete M1/LR2 qualification open**.

- Baseline: required plans/reports and relevant native operator documentation
  read; existing worktree captured; focused tests 5/5. No production code changed
  yet, no resource improvement measured. Full LR2 exit **not satisfied**.
- Chosen first implementation checkpoint: P1 worker ownership. This is a
  correctness/lifetime prerequisite independent of historical source
  availability. Global mempool M1 remains required, not replaced by this work.
- P1 and explicit external-data checkpoint: 39 Teleno files changed, plus two
  Koinos One documentation files. Reproduced missing disconnected-worker stop
  ownership, implemented owned/cancelled/joined generations and count backpressure.
  New build/test/evidence data is on My Passport. Full CTest 38/38, focused 4/4,
  supported UBSan 3/3 and five after-only churn processes pass; missing ASan/TSan,
  transport byte/lifetime, historical/source and real whole-node gates remain.
  See the worker report for exact commands, source preservation and remaining
  dependency workflow. No overall RAM/CPU improvement or LR2 completion claimed.
- M1 characterization follow-up: an additive mempool adapter test retains 2,001
  synthetic records across independent fixed-width account names, checks the
  2,000 response limit versus exact ID membership, refuses an oversized response
  request and verifies expiry/RC/nonce logical cleanup. Native focused test 1/1
  passes in 0.49 seconds (`m1-characterization-test.log`). Inputs bypass chain
  execution: they are not evidence of mainnet-valid signed transaction throughput.
  Empty responses do not establish physical delta/fork/allocator reclamation.
  No aggregate admission policy has been installed yet. Current source changes
  total 40 Teleno files: the 39-file P1 checkpoint plus this existing test file.
  Full suite after the addition passes 38/38 in 151.25 seconds and UBSan 3/3
  in 19.64 seconds (`m1-final-ctest.log`, `m1-final-ubsan-tests.log`). The fresh
  external dependency workflow is running separately, not counted as a pass.

**Every applicable LR2 Definition of Done item proven: NO.**

### M1 boundary and Linux-first follow-up

The native accepted-work boundary now propagates ordinary insertion refusal
before notifying observers. Post-insertion observer errors are separately
counted without acknowledging failure for already-retained work. Visible-head
pending diagnostics no longer incorrectly use the zero-record request API.
See [the checkpoint report](low-resource-lr2-admission.md) for the failed baseline,
changed files, five scoped count comparisons and limitations.

Current native suite: 38/38, 206.74 seconds; UBSan subset: 3/3, 20.94 seconds;
version/help exit 0. No global capacity policy or all-fork/OOM atomicity claim.
The fresh dependency build's third attempt failed in OpenSSL recursive Make;
the fourth uses verified command-local MAKE with Ninja and remains in progress.
The Linux VPS remains the primary optimization/qualification target; these Mac
tests do not satisfy it. Full LR2 exit remains **not satisfied**.

### S1 ownership and successful dependency checkpoint

- Files: backend/iterator/delta/state read ownership APIs and immutable map
  storage, cache bypass policy, new lifetime test and existing cache/root/
  durability tests; exact inventory in [S1 report](low-resource-lr2-state-ownership.md).
- Invariants: held bytes survive read eviction/replacement/iterator destruction/
  close; maps remain independent; default retained payload <=64 MiB; serialized
  bytes/order/roots/tombstones/durability/history are unchanged.
- Evidence: actual baseline held buffer deleted by another read, CTest exit 8;
  current cached/fresh-dependency full suites 39/39 (163.17 / 204.15 seconds),
  UBSan subset 5/5 (11.97 seconds), both CLI checks exit 0. The fourth required
  dependency-script attempt completes successfully; earlier failures remain.
- Measurements: five processes per implementation/input show fewer copies/peak
  for large cloned values, but higher short-value clone time/peak. No Linux or
  total-node gain is inferred; active-owner/metadata/fork bytes remain additional.
- Risks/gates: supported ASan/TSan, all-consumer retention/admission policies,
  current historical differential and representative Linux evidence remain open.
  Whole-audit TODO and full S1/LR2 exits remain **not satisfied**.

### M1-C failure-atomic admission checkpoint

- Files: mempool publication; map iterator/transfer; state delta/anonymous batch
  API; two new native regression binaries and CMake; affected docs/guidance and
  two Koinos One documentation files. Exact inventory and cumulative preservation
  manifest are linked in [M1-C report](low-resource-lr2-admission-atomicity.md).
- Invariants: preparation fails before any parent mutation; no allocating
  publication/reset; original per-fork nonce/resource and anonymous root/tombstone
  semantics preserved. General persistent commit and normal remove are unchanged.
- Baseline: 455 cases include 65 rejected calls with changed state and 56
  terminate paths. Fixed: 363 injected failures, 356 unchanged rejections with
  successful readmission, seven successful post-publication logging failures,
  plus the noninjected success; no invalid result or termination.
- Measurements: five paired Mac processes show 527,456 fewer requested C++
  allocation bytes and 91 fewer allocation attempts for one synthetic large
  candidate. Not retained bytes, Linux/RSS benefit or mainnet throughput proof.
- Risks/exit: aggregate/fork/transient capacity, nonadmission lifecycle OOM,
  integrated sanitizers, representative Linux and historical evidence remain
  open. **M1 and LR2 exit not satisfied**; whole-audit TODO remains open.
- Final verification: cached/fresh-dependency serialized full suites 41/41
  (112.52 / 114.42 seconds), UBSan subset 7/7 (68.80 seconds), external dependency
  workflow/CLI exit 0; final strict Koinos One docs exit 0 (0.42 seconds).
  Initial incorrect-oracle and concurrent preflight/SC5 transfer-deadline failures
  remain recorded. Named-parent validation also rejects nested anonymous inputs;
  no timeout, trust or test expectation was weakened for final verification.

### M1-D retained-fork inspection checkpoint

- Files: additive indexed-delta/map/mempool inspection, public inventory type,
  internal accumulator, new regression/measurement test and CMake; affected
  docs/guidance and two Koinos One docs. [Complete inventory](low-resource-lr2-retained-memory.md).
- Invariants: exclusive database scan, no payload/protobuf copies or mutation;
  owner/backend identity deduplication; tombstones and unmeasured non-map backends
  explicit; no expiry/LIB/nonce/RC/root/history/durability changes.
- Evidence: five synthetic processes retain 8,404,704 logical payload bytes
  despite an empty head after expiry; LIB removes indexed ancestors, but leaves
  256 tombstones. 1,050 injected scan allocation failures leave state unchanged.
  Final focused 8/8; final build/regression gates in the checkpoint report.
- Cost/risk: warmed scan median 56 microseconds and 50,432 C++ requested workspace
  bytes in this fixture, not heap/RSS or Linux costs. O(entries) scan blocks
  private mempool operations. No hot-path calls or new runtime capacity policy.
- External paths remain the owned My Passport run above. Linux VPS is still the
  primary target; integrated sanitizers, historical inputs and whole-node proof
  remain missing. M1/LR2/full-audit exits remain **not satisfied**.
- Final verification: cached/separate dependency full suites 42/42 (156.73 /
  170.91 seconds), UBSan subset 8/8 (25.99 seconds), required dependency workflow,
  all-target builds and both CLI checks exit 0; strict Koinos One docs exit 0.
  Source preservation verifies 122 other files unchanged, 19 extended/three added
  and no removed files. Docker socket unavailable, no smoke or new VM/server run.

### M1-E standalone transaction-work checkpoint

- Files: native work-budget ownership/interface, controller entry, opt-in strict
  config/main metrics, JSON-RPC/gRPC/P2P local-refusal mapping and focused tests;
  affected docs/guidance and two Koinos One documentation pages. [Inventory and
  commands](low-resource-lr2-transaction-work.md).
- Invariants: atomic nonblocking job/input-wire reservation before controller
  copies/execution, held through admission/notification and unwound on failure;
  no peer penalty/cache/retry for local overload, no canonical replay gate or
  protocol/resource/history/durability changes. Defaults zero/unlimited remain.
- Actual first checkpoint: corrected focused build exits 0, tests 6/6 in
  27.52 seconds. Initial duplicate test-local name failed compilation and was
  corrected; the failure log is retained. Broader current-source results follow
  in the report, not inferred from previous full suites.
- Risks/exit: serialized input bytes/count are not retained fork/metadata/VM/
  ingress/output heap or RSS. Linux VPS policy values and all-consumer/lifetime/
  historical/whole-node gates remain required. My Passport generated paths are
  unchanged. **M1 and LR2 exit not satisfied**; no live-server or VM operation.
- Final verification: serialized cached/separate dependency full suites 43/43
  (169.04 / 167.23 seconds); UBSan subset 9/9 (38.51 seconds); required dependency
  workflow, all-target builds and four CLI commands exit 0. Five functional
  processes finish at zero credits; tiny test count/wire ceilings are not VPS
  profile values. Strict Koinos One docs exits 0 (0.35 seconds). Docker socket
  missing, no smoke; current ASan/TSan/Linux/historical gates remain unexecuted.

### M1-F failure-atomic cleanup checkpoint

- Files: staged mempool cleanup/checked decoding, map-delta prepared tombstone
  publication and anonymous wrapper, new failure/measurement test and extended
  state regression, CMake and affected docs/guidance. [Full 20-file inventory](low-resource-lr2-cleanup-atomicity.md)
  and two Koinos One documentation pages; all generated evidence remains on the
  owned My Passport run. Linux VPS remains the optimization/qualification target.
- Invariants: failure before publication preserves every fork; no allocating
  map publication/reset; direct decoding cannot silently supply partial records.
  Normal remove/squash/durability, ancestry, nonce/RC, replay and history unchanged.
- Evidence: original 206/2,115 partial rejected removal/expiry cases plus 24/36
  crashes; final 327/3,592 unchanged injected rejections with successful retry
  plus both unfailed end cases. State publication observes zero new calls;
  12 preparation faults preserve inputs and retry, roots match ordinary squash.
- Current verification: cached/separate dependency suites **44/44** (158.39 /
  185.29 seconds), UBSan **10/10** (37.49 seconds), required dependency workflow/
  all-target builds and four CLI checks exit 0. Strict Koinos One docs exits 0
  (0.47 seconds); no GUI/package/pin integration. Initial failures are preserved.
- Measurements: five serialized processes per implementation/mode reduce total
  requested C++ bytes, but expiry allocation count increases. Extra bucket/
  overlay workspace requires emergency headroom; not retained heap, Linux or
  whole-node proof. Aggregate limits and block/LIB failure paths remain open.
- Risks/exit: supported ASan/TSan, eligible pinned historical inputs, isolated
  whole-machine Linux environment, transport/RPC/VM/state policies and complete
  resource proof remain required. Docker socket missing, smoke not run. **Scoped
  local correction verified; M1/LR2/full-audit exit NOT satisfied**. No live node,
  VPS/VM, producer, commit/push/release, trust/reserve or LR3/LR4 change.

### M1-G lifetime-owned pending-record checkpoint

- Files: shared immutable record insertion/clone/squash/LIB propagation,
  opt-in owner/capacity budget/config/metrics/local-refusal reason, native
  regression and affected docs/guidance; [complete inventory](low-resource-lr2-pending-records.md).
  Two Koinos One operator docs only; generated data remains on My Passport.
- Invariants: one encoded owner per admission, retained ancestor/caller credits,
  checked overflow/pre-publication reservation, unchanged refused state/retry,
  no accepted eviction/notification or canonical replay gate. Bytes/roots,
  nonce/RC, tombstones, persistence/durability and complete history unchanged.
- Evidence: 335 unchanged injected admission rejections with successful retry,
  three factory faults, eight-thread native admission four accepts/76 refusals;
  ancestor/LIB/caller/reopen, byte boundary, actual signed receipt, saturated
  fallback and both local-refusal RPC/P2P reasons pass. One fixture-parent error
  and initial wrong build-target failure are retained, not waived.
- Measurements: five serialized processes per version reduce unique logical
  indexed payload from 16,809,400 to 4,205,044 bytes with unchanged map entries;
  current head expiry still retains ancestors. This is not heap/RSS/Linux or
  mainnet throughput proof. Enabled lease/mutex overhead and remaining metadata/
  encoding/cleanup workspace require measurement/budgets; defaults unlimited.
- Verification: cached/separate dependency full suites **45/45** (179.20 /
  181.78 seconds), separate UBSan **11/11** (64.63 seconds, vptr/dependencies
  excluded), required dependency workflow/all-target builds/four CLI checks
  exit 0. Strict Koinos One docs exit 0 (0.78 seconds), no package integration.
- Risks/exit: all remaining consumer/metadata/workspace policies, block/LIB
  allocation paths, supported ASan/TSan, eligible full historical/reference
  inputs and isolated Ubuntu/x86_64/ext4 whole-node proof remain open. Docker
  daemon socket is absent; smoke not run and no VM/VPS started. **Scoped local
  checkpoint verified; every applicable M1/LR2 DoD item proven: NO**.


## Development-data relocation (2026-10-12)

Pablo explicitly authorized copying only necessary Teleno development data to
My Passport, verifying it, and retiring only the verified corresponding originals.
This extends the earlier data-placement instruction for this selected artifact;
it does not authorize general cleanup, operational databases, snapshot deletion,
live nodes, restore activation, production, Git publication or other projects.

Preserved artifact: the complete historical state-delta audit journal, needed to
retain and reproduce the unique earlier full-archive audit and support subsequent
historical investigations. It contains public block IDs, headers and receipts,
not configurations, wallets, producer keys, credentials or private correspondence.
The source metadata remains unchanged; preservation does not change source trust
or qualify the current LR2 historical/whole-node/Linux gates.

| Item | Verified outcome |
| --- | --- |
| Owned destination | `/Volumes/My Passport/teleno-data-migration-20261012-gv2jtizt/state-delta-audit-journal/` |
| Continuation alias | `build-chain-v152/validation/audit-scratch.delta-journal` now points to that destination |
| Inventory | 371 binary buckets plus `metadata.txt`; 56,750,601,233 logical bytes (52.853 GiB) |
| Historical corpus | Metadata declares 37,019,561 unique heights and 41,148,159 stored records; no journal records were omitted |
| Copy | Native `ditto --rsrc --extattr --acl`, into a fresh owned destination; no prior data overwritten |
| Integrity | Every file SHA-256, size, mode, mtime and flags matches before/after; full source SHA-256 recheck also matches; source/destination xattrs absent |
| Manifest SHA-256 | `3f3f72dfac1d58e3ef9acb6aaeeac00a48c8a11d9aeea55b10c62c8e65acbecb` for each identical source/destination/recheck inventory |
| Reader checks | Original and external results agree at 30,504,201–203, 32,789,376–379 and 37,019,559–561; six matching original/external invocations plus one successful post-retirement continuation-alias check; no unexplained mismatches |
| Restorability sample | Metadata and final bucket copied into a separate owned directory and rehashed; both match the complete inventory |
| Original retirement | Only the 372 manifest-listed journal files removed, after verification and renewed open-file checks; original path replaced by a continuation symlink |
| Internal available before/after | 21,697,912,832 / 21,511,426,048 bytes (20.208 / 20.034 GiB), observed at the recorded checks |
| Internal immediately before/after retirement | 21,496,430,592 / 21,511,426,048 bytes; observed change 14.301 MiB, far below the logical offload |
| External available before/after | 289,271,926,784 / 230,456,872,960 bytes |
| Filesystem | Verified My Passport APFS volume UUID `BFF5690D-C6A9-411A-91B7-F73758D4E757`, writable, unencrypted; only public journal content transferred |

Logical transfer size and `du` allocations are not exclusive physical recovery.
There were 24 local Data-volume Time Machine snapshots before retirement; none
were deleted or modified by this task. Snapshot/shared-extent retention and other
background activity can affect observed free space. The recorded free-space
difference is not a claim that all transferred bytes became immediately free.

Evidence, per-file hashes, scripts, before/after space readings and the retained
candidate inventory are under `/Volumes/My Passport/teleno-data-migration-20261012-gv2jtizt/`. The inventory has 748 nested rows; parent
and child sizes overlap and must not be summed indiscriminately.

Retained candidates awaiting separate review and specific deletion approval:

- `build-single-copy-evidence/`: 50.902 GiB reported by `du`; includes unique
  referenced logs/manifests and many repeated synthetic fixture databases.
  Regenerable payloads need separate selection while unique evidence is retained.
- `build/canary-fix-local-smoke-basedir/`: 25.485 GiB; archived isolated observer
  preparation, distinct state and logs. No full byte-level deduplication performed.
- `build/canary-minador-1.2.0-30504141/`: 25.476 GiB; separate historical checkpoint
  preparation. Existing external public archive is not a deletion qualification.
- `build/release-1.2.0-source/build/` and `.deps/`: 0.437 and 5.715 GiB; generated
  build/dependency candidates only. Their parent is a registered Git worktree;
  preserve its source, `.git`, registrations and any unique release evidence.
- Other historical state/checkpoint directories remain in place. No blanket
  cleanup of `build-chain-v152/validation/` is approved.

No CMake tree/cache was relocated. Source, `.git`, existing dependencies and all
pre-existing changes were hash-checked before original retirement. Only this
matrix, project memory and the historical validation report receive appended
storage-location notes. No commit, push, deployment or operational action occurs.

For subsequent diagnostic writes, successfully source the guarded environment
before executing the tool (the `&&` is intentional):

```bash
source '/Volumes/My Passport/teleno-data-migration-20261012-gv2jtizt/continuation.sh' &&
./build-chain-v152/src/koinos_state_delta_replay_audit \
  --scratch-state-dir "$TELENO_TEST_ARTIFACT_ROOT/new-owned-audit" \
  --journal-dir '/Volumes/My Passport/teleno-data-migration-20261012-gv2jtizt/state-delta-audit-journal' \
  --journal-only --from-height 37019559 --to-height 37019561 \
  --no-log-file --progress-every 0 --json
```

The guard verifies the mounted volume identity and owned writable roots, exports
external TMPDIR/test/build/dependency paths, and disables Python bytecode writes.
The source remains this exact repository. No new compilation or Linux test was
performed for this data-only relocation. Existing LR2 external runs and their
aliases remain intact. LR2 and whole-audit qualification remain open.


## Authorized regenerable-data cleanup (2026-10-12)

Pablo explicitly followed up with a request to free the Teleno data on the Mac.
This authorizes the reviewed regenerable payload/cache cleanup below. It does not
explicitly override the original prohibition on deleting snapshots or backups.
Separate confirmation for local Time Machine snapshot deletion was requested;
no snapshot deletion has been performed.

Removed 429,560 regular files, 58,741,273,531 logical
bytes (54.707 GiB), exclusively from unreferenced
repeated synthetic-run payloads and inactive release-1.2.0 build/dependency caches.
Every deleted path, inode, size, mtime and reason is recorded under
`/Volumes/My Passport/teleno-data-cleanup-20261012-smb0hjfh/` in `delete-plan.json` and `deleted-paths.jsonl`.

Preserved all documentation-referenced fixture roots, logs, result/manifests,
source hashes and provenance, embedded dependency Git checkouts, registered
release-source worktree/source and its existing untracked logs. Both primary
source/worktree and release-source file hashes remain unchanged. The existing
467,130 retained regular files were checked for unchanged size/mtime.
Historical canaries and distinct state checkpoints remain intact because their
redundancy has not been qualified. Operational databases are untouched.

Observed internal free space immediately before/after cleanup:
21,320,286,208 / 21,041,758,208 bytes
(19.856 / 19.597 GiB).
Logical removal did not yield equivalent immediate physical space. Twenty local
Time Machine snapshots were found at the follow-up inspection. No local/global
backup, system snapshot, external copy, application profile or other project
was deleted. `tmutil deletelocalsnapshots /System/Volumes/Data` is the reviewable
volume-scoped next action only after explicit snapshot-deletion authorization;
its consequence is loss of those local recovery versions, not external backups.

Existing primary native `--version` and `--help` pass; identity remains
`1.3.0-dev.0+cfd2eb83b808-dirty`. AGENTS/CLAUDE are identical and git diff checks
pass. No native code, version, producer/configuration, source-trust, commit,
push, release, deployment or restore activation change. Existing cleanup
records are point-in-time evidence, not new LR2/Linux qualification.


### C14 scoped sanitizer and further refusal checkpoint

C14 has 54 unique successful scoped ASan/UBSan checks across 55 invocations,
retaining the original cleanup timeout and separate unchanged-ELF diagnostic
pass. Default TSan is running with working generic/SDK clean/race controls.
SC5 revision 3 has 16 helper-unit passes, not a full native experiment.
A large-ID HTTP overload diagnostic reproduces 524,420 untracked requested bytes
in C14; its unfrozen bounded, borrowed-ID correction is not yet qualified.
C14, M1 and final whole-node gates remain open. See the
[continuation report](low-resource-lr2-m1-json-cleanup.md#reproduced-http-refusal-ownership-gap-and-unfrozen-repair).


The later C14 checkpoint reconciles **52/52 default TSan invocations** with
working generic and actual SDK controls; this does not close M1. The unfrozen
HTTP refusal revision 1 passes individual ownership and fails mixed-batch
handling with abort 134. A preserved terminate trace locates temporary JSON
allocation inside the library's `noexcept` version comparison. Revision 2 borrows
the existing version string and adds same-service retry after each fault;
compilation is in progress and qualification remains open. Preserve SC5's
failure and the consumed C13/C14 slots. See the [detailed report](low-resource-lr2-m1-json-cleanup.md).

### R15 checkpoint: 2026-10-23

R15 passes 104/104 registered normal checks, both native CLI checks,
63/63 focused ASan/UBSan and 61/61 default-TSan cases on OVH. Paired
measurements, controlled-chain create/reopen and exact-node full-history
mixed-read/restart/interruption-recovery pass. Final owner review completes
M1 for exact R15. The
[R15 report](low-resource-lr2-m1-r15.md) records
exact identities, the preserved normal restore early guard and TSan
coordinator artifact-prefix failure. No Hetzner operation occurs.
