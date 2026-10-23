# Issue 9: optimize Teleno RAM and disk use on the operator's own VPS

Created: 2026-10-15. Purpose clarified: 2026-10-17.
Status: **work open; LR2 not qualified**.

**M1 completion, 2026-10-23:** exact R15 passes lifetime-owner/admission review,
104 normal OVH checks, both CLI checks, 63 focused ASan/UBSan and 61 default-
TSan cases with working controls. Five equivalent pairs per measured workload,
100-block/12,800-test-transaction creation/reopen, and the exact full-history
observer's one-hour mixed reads, clean restart and interruption/recovery pass.
Measured RAM and strict disk/reserve margins satisfy this M1 profile; component
improvements are kept separate from whole-node savings. See the
[M1 decision and exact identities](../validation/low-resource-lr2-m1-r15.md).
The operational Hetzner producer is untouched. M1 does not close the remaining
issue-9 requirements or replace separately identified producer/restore evidence.

**M1 execution-time amendment, 2026-10-21:** the operator removes overall
execution-time limits. Continue memory optimization, builds and qualification
for as long as necessary to demonstrate the acceptance criteria. Do not stop
because an eight-hour allowance or fixed work window elapsed. Preserve all
closed-run results/deadlines, source/candidate provenance, non-time resource
limits and the exclusive OVH test location. The
[current M1 execution controls](issue-9-m1-aggregate-goal.md#execution-controls-and-completion--no-overall-time-limit)
supersede earlier future-work time limits; the Hetzner producer remains outside
scope.

**M1 purpose clarification, 2026-10-21:** optimize actual application memory
use so that the complete Teleno node is economical to operate on the small-VPS
baseline. Profile and reduce unnecessary allocations, copies, temporary work
and retention; use lifetime-owned aggregate admission to keep concurrent
mempool/transaction work bounded. Prove physical memory behavior separately
from accounting counters. The
[current M1 objective](issue-9-m1-aggregate-goal.md#current-objective--clarified-by-the-operator-2026-10-21)
preserves all correctness, history, resource and evidence acceptance gates.
Every executable M1 test remains exclusive to isolated OVH; the operational
Hetzner producer is outside scope. This clarification extends no expired run.

**M1 test-location amendment, 2026-10-20:** execute all future M1 tests only
on the new isolated OVH VPS, including development reproductions, complete
native suites, sanitizers/controls, controlled-chain fixtures, paired
measurements and native CLI checks. My Passport and the authorized builder
remain available for artifact storage/preparation and compilation without test
execution. This overrides earlier M1 test-location permissions in this
document; preserve their historical evidence. Keep the operational Hetzner
producer untouched and retain all deadlines, counters, resource limits and
recovery reserve. The [M1 memory-optimization goal](issue-9-m1-aggregate-goal.md)
records the current scope; the [C11/C12 repair](issue-9-m1-receipt-output-repair.md)
is a closed, unqualified attempt with its evidence preserved.

The operator authorized necessary further M1 attempts on 2026-10-20. The
separately bounded C5/C6 and C7/C8 attempts are both closed unqualified;
the unchanged C1–C4 outcomes are recorded in the
[M1 goal](issue-9-m1-aggregate-goal.md). The live VPS producer remains
outside these implementation and qualification runs.

A separate C9/C10 attempt reproduces and corrects two shorter-predecessor
nonce/count read overflows. Frozen C9 passes 74/74 normal Linux checks, 22/22
focused ASan/UBSan and 22/22 focused TSan checks, separate cleanup checks, and
a controlled 100-block production/reopen profile. Five paired C8/C9 samples
and all ten reopens pass without an attributable performance saving or loss.
The finite attempt closed unqualified for M1; whole-node qualification remains
open. See the
[C9 report](../validation/low-resource-lr2-m1-nonce-read.md). No live VPS
producer operation is included.

The separately authorized [finite M1 aggregate-memory run](issue-9-m1-aggregate-goal.md)
starts at 2026-10-19 08:30:38 UTC and stops by 16:30:38 UTC with at most two
frozen candidates. It changes source and isolated tests only. It does not
authorize a producer restart/deployment or reopen closed commissioning counters.
See [M1 ownership and evidence](../validation/low-resource-lr2-aggregate-memory.md).

The first M1 attempt is closed and unqualified. A separate operator-authorized
repair allowance had the fixed window **2026-10-19 14:16:04–22:16:04 UTC**,
with at most two additional frozen candidates, C3/C4, and no VPS producer
operations. It stopped within that window with both slots consumed and M1 still
open. Its
[amended goal](issue-9-m1-aggregate-goal.md#authorized-finite-repair-allowance--2026-10-19)
also requires a [legacy applicability analysis](legacy-applicability-of-teleno-findings.md),
distinguishing reproduced failures from source-supported hypotheses. This does
not change the first attempt's limits or make M1 complete.

## Completed operational producer milestone (2026-10-18)

The repaired candidate passes the finite mainnet producer observation and
subsequently produces block **40,323,375**, independently verified canonical and
irreversible. Mainnet activation and real eligible PoB block acceptance are no
longer unproved. The block contains zero transactions; loaded producer, broader
recovery, aggregate-memory, diagnostics and historical gates remain open.
See [the producer evidence](../validation/native-producer-handoff.md#subsequent-own-block-confirmation).

The accumulated implementation is being published as the independent
**1.3.0-alpha.1** prerelease. Alpha publication does not mark issue 9 complete;
see [the detailed release scope](../releases/1.3.0-alpha.1.md).

## Current reserve requirement (operator amendment, 2026-10-18)

The operator explicitly changes **original issue 9** to require an **8-GiB
recovery-reserve floor (8,589,934,592 bytes)**, replacing the previous 10-GiB
acceptance requirement. Keep the greater 20%-of-database term:
`reserve = max(8 GiB, database bytes / 5)`. Hold that reserve free inside the
unchanged 40,000,000,000-byte whole-server budget throughout qualification.
The CPU, physical RAM, history, correctness and other acceptance gates remain.

The completed [8-GiB VPS measurements](../validation/vps-8gib-recovery-reserve.md)
now count toward original issue 9. For the final 312-input candidate and selected
backup, full restore peaks at 37,915,475,968 whole-server bytes; retained-reader
compaction peaks at 39,352,967,168 bytes, both including the full reserve.
The selected-input restore/disk criterion passes, with a minimum measured
647,032,832-byte margin. Do not require another 1.50 GB of savings merely to
emulate the superseded 10-GiB criterion. Broader aggregate memory, diagnostics,
historical execution and representative producer qualification remain open.

This amends acceptance, not runtime code: the standard CLI default remains
`max(10 GiB, database bytes / 5)` and the tested candidate explicitly selects
`--backup-restore-reserve-profile experimental-8gib`. Earlier checkpoints below
retain their dated evidence; their references to preserving the former reserve
do not override this current requirement. The five-hour experiment is complete;
this amendment does not close the remaining issue-9 gates.

Latest source checkpoint, 2026-10-17: B7 adds private arena-string copy safety
to the pinned Protobuf runtime. Same-object Linux controls reproduce UBSan exit
92 with the original runtime and pass with only `arenastring.cc.o` changed.
The integrated candidate passes all 63 normal Mac entries, complete build and
CLI checks. Its 311-input manifest is
`a21a8958e1757a8f2f661264b5b8665158c6db33c9c31a2fed8991d39e962a20`.
Fresh Linux normal/instrumented namespaces are prepared, not yet qualified.
The preceding diagnostic manifest completes all 63 normal Linux entries and
all 86 targets, then finds the arena failure after 17 instrumented passes;
all original failures remain preserved. See the
[current causal evidence](../validation/protobuf-unknown-field-ownership.md#arena-string-construction-failure).
Older checkpoints below retain their exact earlier identities.

Current diagnostic-source update, 2026-10-17: SC0 can now collect exact listed
source provenance from an explicitly selected verified manifest when an export
has no Git metadata. The earlier 41-pass Linux run and its metadata failure
are preserved. Exactly two diagnostic Python files and their README change;
all native code, commands, dependencies and executable hashes remain unchanged.
Nine focused tests and a fresh full normal Mac run pass; both exhaustive Linux
suites are restarted in fresh namespaces, with every entry required again.
The 311-input manifest is
`6dde3b266993faf8898f277ae8584f0c86eb659056fc604067dbbe84d2612688`.
See [source-export evidence](../validation/source-export-provenance.md).

Current source checkpoint, 2026-10-17: B6 retains partial cleanup decodes and
corrects publication ownership in the same pinned Koinos protobuf runtime.
Two independent controls reproduce the old leaks; twelve corrected controls
and the identical expanded native cleanup object pass. The full native Mac
build, CLI and all 63 tests pass. The normal Linux node build/CLI and both new
tests pass; the instrumented runtime and both new tests pass too. Its first
CLI build hits the private compiler scope's 2.5-GiB ceiling. A reviewed GC
profile changes one compiler command without raising any limit or disabling
diagnostics; the incremental build and CLI then pass without another OOM.
All original failures remain preserved. The normal staged suite also preserves
a coordinator failure caused by absent CTest paths for unbuilt executables;
its reviewed continuation verifies all generated commands before each test.
Full Linux suites and the fourth historical pair remain open. Current 311-input manifest:
`c8bad5024f0750d566145ba251d3fce72f52a7ee766414eadc57f447e466b1f2`.
See the [ownership report](../validation/protobuf-unknown-field-ownership.md).
The following B3/B4/B5 checkpoints retain their exact earlier identities;
neither component diagnostics nor an older VPS run qualify the whole B6 node.

Latest checkpoint, 2026-10-17 local time: optional native WAL flush policy
passes all 61 tests and all-target/CLI checks on Mac/Linux; the exact Linux
binary also passes read-only full observer-profile preflight on the reference
VPS. The denser native backup passes every object hash/checksum and comparison
of all 46,458,363 raw records, saving 688,676,036 DB bytes against the previous
native backup. The old generated public qualification duplicate is fully
preserved, rehashed and retired; native promotion, metadata and all 383 HTTPS
object sizes pass for the new isolated qualification source. Stable public
latest and installed publisher remain unchanged.
Native checked replay now completes 2,285,238 historical blocks on the actual
VPS and matches the independently anchored terminal ID/root. Four current
native fixtures also pass there, including controlled producer logic and WAL
recovery. The dense backup's complete restore, full validation and held
activation now pass at a measured 39,760,650,240-byte whole-server peak including
the unchanged reserve. The first current observer completes one hour, advances
34,955 blocks, matches a stable legacy head/root and stays below 635 MB RSS
with at least 2.83 GB host available and no swap/OOM change. A clean 90-second
restart and the actual-VPS 1,000-block federated producer/reopen fixture pass.
The first 24-hour observer is now running with unchanged B3 code and limits;
a conservative immediate-restart port-probe refusal is preserved and resolved
by a fresh successful preflight, without bypassing checks. A Linux full-VM
boundary also matches every generated receipt, head/root and LIB against the
tagged reference; its first 50,000-block pair also passes every trace row and
an independent canonical endpoint check. The second pair through 30,604,141
also passes the native/reference comparison and all 60 cross-chunk overlap
rows, including LIB; its independent endpoint and cold-state metadata checks
now also pass. Full historical
differential/VM qualification, abrupt recovery, sustained operation, actual PoB
and remaining producer gates stay open. See the current
[WAL](../validation/wal-retention-budget.md),
[backup](../validation/larger-block-dictionary.md) and
[historical](../validation/historical-checkpoint-eligibility.md) reports.

The later integrated sanitizer run finds a protobuf allocation-failure leak in
mempool admission. Its [arena-copy correction](../validation/mempool-admission-copy-ownership.md)
passes the Linux native fault/roundtrip test and all 61 normal Mac tests with
all-target/CLI checks. Normal Linux now also passes all 61 entries by combining
original passes with verified retries on adequately provisioned owned ext4
test roots; all original failures remain preserved. A mixed-RTTI test-wrapper
issue is independently reproduced and corrected only in the diagnostic
configuration; both affected tests pass. The activation fixture passes in
215.608 seconds under the reviewed 300-second instrumented profile, with
unchanged binaries, detectors, checks and resource limits. The suite reaches
36 passes before a negative Python test exposes overlapping temporary/artifact
roots in the runner. Restoring their original separation lets those tests pass.
The run then reaches 52 passes and stops in allocation-fault fixture setup;
that failed run remains preserved. Moving caller-input construction outside
the fault window exposes a real cleanup-decoding leak. A later B5 ownership
correction passes the identical preserved test object, but an expanded
unknown-field fixture still reports 640 leaked bytes in 20 allocations while
all cleanup-state invariants pass. Both results and the exact failed workspace
are preserved. B5's 306-input source manifest is
`7e8108f2a723aaba569009d5e51f0b2f6e5a3209df67fcd69a949de8beb45500`.
The B6 publication correction above resolves that focused report. New full
regressions and additional copy/merge failure qualification remain required;
B4 results do not qualify B5/B6. The full diagnostic suite is still unqualified.
Five paired admission samples per signature profile record fewer ordinary
allocation calls, slightly higher requested bytes and CPU/latency tradeoffs.
A [real-mempool producer fixture](../validation/controlled-native-producer.md)
also preserves 12,800 signed local transactions across 100 blocks and exact
clean-reopen tail execution on the builder. This candidate is distinct from
the frozen B3 VPS run; no final qualification follows.

## Objective and authority

**Reduce the RAM and disk requirements of Teleno, the user-maintained open-source
C++ Koinos cryptocurrency node, so it costs less to operate on the user's own
designated VPS.** This is application performance and storage-capacity work in
the project's source code and the operator's development environments. Measure
allocation and cache costs, improve data lifetimes and lossless storage, and
reduce backup/restore workspace. Measure CPU, I/O and latency tradeoffs while
preserving protocol behavior, complete history and reliable recovery.
The required outcome includes restoring the complete selected backup within
the 40-GB whole-server disk envelope and running the producer workload on the
user-designated 2-vCPU / 4-GB Ubuntu server. These are acceptance targets, not
claims that the current implementation already fits.

The user reaffirmed on 2026-10-17 that they maintain this open-source software
and own the designated reference server. The workload is their Koinos node and
its producer. Investigate concrete resource costs, implement focused improvements
and compare equivalent before/after measurements. Automated regression tests,
memory diagnostics and recovery checks establish that the optimized application
remains correct. This clarification preserves every existing acceptance gate,
checkpoint and authorization boundary; it is not a new performance result.

Implement and qualify [issue #9](https://github.com/koinos/teleno/issues/9)
through repeated profiling, scoped code changes, tests, measurement and review.
The user selected My Passport and their existing Linux build-and-test machine
for development storage and compilation. This authorizes remaining LR2
implementation and isolated development builds/tests there. Existing nodes, producer
services, operational data, keys, machine-wide settings, purchases and
deployment retain their separately defined authorization requirements.

### Reusable loop objective

> Optimize the source code of Teleno (https://github.com/koinos/teleno), the
> operator-maintained open-source C++ Koinos node, to reduce the cost of running
> its complete full-history node and block-producer workload on the operator-
> owned reference VPS: Ubuntu/x86_64/ext4, 2 vCPUs, 4,000,000,000 bytes of
> physical RAM and a 40,000,000,000-byte whole-server disk budget.
>
> Profile and improve application allocations, cache/data lifetimes, database
> compression and backup/restore workspace. Preserve Koinos consensus and wire
> behavior, complete canonical history from genesis, current state, and every
> block and receipt present in the selected independently verified backup. Use
> lossless storage improvements; do not prune history or omit records to meet
> the budget.
>
> Demonstrate that the complete backup can be restored, validated and reopened
> while total server storage—including the operating system, software,
> databases, WAL, compaction, logs, temporary files and the required recovery
> reserve—remains within the disk budget. Hold at least 8 GiB (8,589,934,592
> bytes) free, retaining `max(8 GiB, database bytes / 5)`, and preserve the
> reference server's specifications.
>
> On that VPS, demonstrate steady node RSS at most 2 GiB, peak RSS at most 2.5
> GiB, concurrent host MemAvailable of at least 768 MiB, and no OOM or reliance
> on swap. Verify catch-up, sustained synchronization, mixed workloads,
> restart/recovery and producer workloads. Keep observer, isolated producer-test
> and operational producer evidence distinct.
>
> Follow a continuous measure → implement → test → compare → review loop.
> Identify exact source, dependencies, binary and inputs. Compare equivalent
> workloads and report RAM/disk savings alongside CPU, I/O and latency
> tradeoffs.
>
> Use My Passport and isolated directories on the operator-owned Linux build-
> and-test machine for generated development data, compilation and tests.
> Continue from existing verified work and evidence. Preserve unrelated changes,
> existing services and operational data. Keep development on codex/lr2-memory-
> hardening.
>
> The scope is source optimization and authorized application qualification.
> Access administration and mainnet signing or block-production activation
> remain separately scoped operations governed by their existing explicit
> authorization requirements.
>
> Complete the goal only when every existing issue-9 acceptance criterion passes
> for the selected final implementation. Keep unmet requirements explicit and
> continue improving until they pass. Component tests and results from older
> binaries do not qualify the final whole node.

On 2026-10-18 (Europe/Berlin), the user supplied this precise replacement
objective. The previous registration was absent when checked; a new active
goal is registered with that exact text. Existing work, evidence and all issue-9
acceptance criteria continue unchanged. This registration is not a completion
claim or a reset of the implementation. The build machine is identified by its
compilation/test role, and operational authorization boundaries remain in force.


Follow-up on 2026-10-15: the user selected a specific VPS as the mandatory
baseline and explicitly requires the **producer** to work there. The user then
replaced that first server with a second one and explicitly retired the first
reference. The only active baseline is now the replacement:
**2 vCPUs / 4 decimal GB RAM / 40-GB-class disk, Ubuntu/x86_64/ext4, no swap**,
confirmed through live read-only inventory. The first server's 1-vCPU/2-GB
shape is retired context, not an alternate target or a server to contact again.
The user also explicitly authorized configuring persistent SSH access and any
necessary console/email password recovery. That access setup is complete and
verified; it does not activate block production or change mainnet authorities.

The durable goal is attached to this task. This file is its resumption contract;
the [completion matrix](../validation/low-resource-lr2-completion-matrix.md)
remains the detailed acceptance ledger. Read the live issue, project guidance
and [full-history plan](low-resource-mainnet-observer-plan.md) on resumption.
Keep `codex/lr2-memory-hardening` separate from `main`. Preserve the existing
mixed changes and review their dependencies before any separately authorized
publication or integration. LR2 includes prerequisite correctness work. The
subsequently approved full cost objective also requires isolated lossless
storage work and complete 40-GB restore qualification. LR3, LR4, LR5 and LR6
retain their separate acceptance status; a component experiment does not close
them or authorize production activation.

The [isolated publisher maintenance plan](backup-publisher-identity-update-plan.md)
records the completed isolated observer update and prepared rollback. Its
22/22 Linux regressions, container/installed-artifact checks, advancing head/LIB
and new receipt agreement with legacy pass. The corrected public backup is
published and matches the independently authenticated writer's inventory.
The mixed LR2 development binary was not installed on that publisher.

Previous client checkpoint: [P2-C](../validation/low-resource-lr2-p2p-client.md) adds
owned outgoing RPC phases, complete-response deadlines, stop/late-callback safety,
local receive-capacity classification and adaptive read-only block batches.
Final Mac/Linux all targets/version/help and 55/55 CTest pass (144.49/130.72 s).
The final source sheds ignored capacity for small decoded results. Separate
scoped ASan/UBSan/TSan, nine normal-ASLR VPS component commands and isolated
runtime-container CLI/hash checks pass. Earlier
[P2-B](../validation/low-resource-lr2-p2p-reads.md) Mac/Linux and scoped diagnostic
passes remain historical component evidence. Aggregate transport/SDK/decoded
memory, DNS/full lifecycle, whole-node/producer/history/40 GB and full B1/D1 gates
remain open. Development binaries are separate from the installed publisher.

Current continuation: [P2-D](../validation/low-resource-lr2-peer-bookkeeping.md)
corrects network-address score keys, idle decay, saturation and query-rounding
behavior. Positive finite record/key-capacity limits preserve nonzero scores and
refuse new unknown bookkeeping locally. Optional candidates trim independently
of discovery; protected seeds/connected metadata remain owned. Reproduced
publication/discovery allocation faults have scoped regression coverage.
Mac all targets/version/help and 56/56 CTest pass (186.07 s), plus actual-clock
decay and 16 Go/native arithmetic vectors. Linux all targets/CLI/actual-clock,
56/56 exhaustive native batches and five-pair component measurements pass;
eight fixtures under each separate ASan/UBSan/TSan and 11 normal-ASLR VPS
component commands pass. The matching runtime container passes CLI/hash checks.
Direct child CPU/I/O measurements preserve the component tradeoffs. P2-C passes
above identify older inputs. No aggregate/whole-node,
40-GB or producer gate is newly closed.

Current [P2-E](../validation/low-resource-lr2-p2p-dials.md) adds explicit native
dial reservations/retired-owner credits, failure-atomic setup and generation
publication. Reproductions cover stale admission, foreign-registry cancellation
and hidden seed-route omission. A controlled TSan stack-mutex lifetime diagnosis
corrects the fixture without detector suppression. Final Mac/Linux 57/57, all
targets/CLI, nine fixtures under each separate sanitizer, 12 normal-ASLR VPS
commands and matching runtime-container CLI/hash pass. Five paired common-harness
measurements record memory, elapsed and Linux child CPU/I/O tradeoffs.
Source manifest: `947958d848be5906923b8f968d6d19e01a601f8eb3e7b70bce99a6ed16d31275`.
Internal 32-job/1-MiB peer capacity excludes implicit GossipSub/RPC/DHT/inbound
work, SDK/DNS/known-peer/control aggregates and whole-node memory. Current
artifacts remain separate from the installed maintenance publisher. Full P2,
D1/B1/Q1/H1, aggregate consumers and mandatory producer/40-GB restore stay open.

Current [P2-F](../validation/low-resource-lr2-peer-snapshots.md) bounds optional
peer advice and lifetime-owned metadata projections, removes external SDK reads
and corrects the reproduced query/stop host-pointer race. Final R1 Mac/Linux
59/59 native checks, all targets/CLI, eleven fixtures per separate ASan/UBSan/TSan,
14 normal-ASLR VPS components and matching runtime-container CLI/hash pass.
Temporary Boost.DI scope mutex lifetime annotations retain real-cycle/destroy-held
detection; the R0 report and controls remain preserved. Five common-harness pairs
per 256/2,048-input workload record allocation, time and Linux child CPU/I/O.
Source manifest: `e100ef823bc7843f26f48a893a36893f4c7768119fab7991c11263769cc818d2`. Active/SDK metadata,
other queues/caller aggregates and full P2/D1/B1/Q1/H1 remain open. Mandatory
producer/40-GB restore and the prior disk reserve deficit are not waived.

P2-G's scoped implementation and checks pass: a native SDK-compatible address repository reduces
duplicate address storage, adds shared retention accounting and collects
expired metadata. The original SDK allocation/retry behavior is reproduced on
Mac and Linux. Incremental planning removes the reproduced full-cache scan and
temporary victim-vector cost. Current Mac/Linux all targets/CLI and 60/60 native
checks pass (187.58 / 143.23 seconds); five paired common-harness samples per
256/1,024/4,096-input cohort record allocation, CPU/I/O/RSS and timing tradeoffs.
The frozen 304-input manifest is
`07342ad3c065c0fe6d8bd632994c7a160b9500304c91468b42cde0b69588cb37`.
Twelve fixtures under each separate ASan/UBSan/TSan, fifteen normal-ASLR baseline
component commands and offline runtime-container CLI/hash checks pass. The
launcher RSS control prevents interpreting inherited process peaks as node RAM.
Native/Koinos One documentation and scoped preservation are checked separately.
P2-F results above identify the preceding source checkpoint. Whole-node and
complete backup/producer gates remain open.

The subsequent [lossless storage experiment](../validation/lossless-archive-storage-experiment.md)
verifies the full pinned input and reduces its database from 29,720,960,254 to
24,840,258,810 bytes (16.422%). All nine families compare byte for byte,
including 45,076,063 blocks/receipts. Every header ID and the complete
40,236,770-height canonical parent/receipt chain are checked; all 4,839,293 other
selected records remain. Public-reference ancestry and successor-header root
agreement match the pinned state. Historical execution/signatures and actual
40-GB restore/whole-node/producer operation are still unqualified.

Both full-data coordinators are terminal. Preserve and reuse the verified
My Passport source, compact candidate, topology audit and Linux measurements;
do not restart the download/rewrite without a new reason. Native storage now
has an opt-in
[persistent native compression policy](../validation/native-block-compression-policy.md),
with unchanged defaults. All targets/CLI and all 60 native tests pass on Mac
and Linux; nine actual-VPS CLI/component invocations pass. Initial reserve
refusal and an overly strict VPS harness assertion are preserved and resolved
with unchanged source/binaries. A supported export/restore path remains required.
Then perform the complete baseline restore/recovery and sustained workload
gates. After verified preservation/retirement of owned development duplicates,
the fresh VPS model has about 311 MB beyond recommendation, including the new
isolated native executable. This is sizing evidence, not a complete restore or
sustained capacity pass. Do not reduce the recovery reserve.

The [native rewrite continuation](../validation/native-archive-rewrite.md)
adds explicit offline bottommost rewriting and omits host identifiers from
new native SSTs. A fresh isolated copy starts from the original public backup;
the actual native backup completes with 385 files and 24,802,854,237 payload
bytes, with checkpoint identity matching the original. Its DB is 16.550%
smaller. All 385 object hashes, both complete RocksDB checksum scans and the
full nine-family raw comparison pass. All 45,076,063 blocks/receipts remain
byte-identical; only two expected operational metadata values change.
The earlier root-filesystem sizing model is about 305 MB beyond recommendation;
corrected physical whole-device accounting below supersedes that estimate.
Mac/Linux all-target builds and all 60 unique native tests pass across the
recorded attempts; two initial
Mac timeouts and their unchanged retries remain visible. Twelve current native
component commands pass on the mandatory VPS. Existing publisher/runtime data
remain preserved. Native promotion and HTTPS verification now pass for the
isolated qualification source, with stable public latest unchanged. The actual
VPS passes native preflight, controlled stage interruption and same-input
resume; all 385 payload files are received and checked, reusing 18 completed
files. Offline validation/held activation and all three full database scans
now pass, preserving the same database directory inode. Activation takes
3,608.699 seconds and peaks at 49,008,640 RSS bytes. Two short observer runs
and a clean restart pass; the second catches recent head and its captured
height/root agrees with the independently queried legacy reference. See
[reference VPS measurements](../validation/reference-vps-observer.md).

**Strict 40-GB acceptance still fails.** The actual provider disk is
40,961,572,864 bytes. Including physical overhead, boot partitions and the
unchanged 10-GiB reserve requires up to 40,628,682,752 bytes during these
observations: a 628,682,752-byte shortfall. Root-only margins do not qualify
the whole device. Preserve all history/receipts and obtain further lossless
savings. Sustained operation, abrupt recovery, producer and historical gates
remain open.

The next [direct-read change](../validation/direct-restore-read-buffer.md)
reserves 64 KiB inside the existing transport bound. Five paired Linux-builder
samples show 47.67% lower median CPU consumption and 96.87% fewer kernel write
calls for an identical verified HTTPS object; elapsed-time improvement is not
established. Native input manifest:
`af826d523daa1d196808b7a1e69effaadfe6e119bfa4c9be34a15af1b24d3553`.
Mac/Linux all-target builds, focused checks, all 60 native tests and CLI checks
pass for that source. Five additional reference-VPS pairs pass with the same
verified object: median paired CPU decreases 73.02%, elapsed time 71.56%, and
kernel writes 96.87%. These are scoped transport results. The completed full
restore retains its preceding frozen binary identity.

The preceding RSS native manifest is
`c073ab12836d01ddd761e8146ed829792fee0f92b77c622059cf798951208c3b`.
The subsequent [Linux RSS correction](../validation/linux-rss-counter.md)
reads resident rather than virtual pages; it changes reporting, not actual
RAM use. Mac all targets/CLI and 61/61 CTests pass. Linux all targets/CLI and
all 61 unique CTests pass across preserved attempts; the synthetic comparison's
initial space refusal and a retry prelaunch refusal remain explicit. No native
reserve is reduced. Independent kernel peaks underpin the earlier VPS results.

Current native manifest is
`ca0a6b4e890474528e385c3149f76250d38787d2cb41093059858a9d97290e7b`.
The [128-KiB dictionary option](../validation/larger-block-dictionary.md)
retains zero defaults and training/buffer bounds. Five repeated lossless
triplets show 2.572% smaller diagnostic files, with 8.850 times paired
rewrite/verification CPU and approximately unchanged median warm lookup CPU.
Three more native SSTs pass complete comparisons. Mac/Linux all targets,
CLI and all 61 tests pass. A full native rewrite is running on an isolated
My Passport COW copy; terminal equality, native backup and strict-budget
restore remain required. COW is development placement only.

The preceding RSS executable also completes a one-hour observer restart:
kernel peak 260,673,536 bytes, minimum host `MemAvailable` 3,218,644,992 bytes,
clean stop and stable legacy height/root agreement at 40,262,636. All 60 RSS
logs agree with the kernel within 816 KiB. Strict disk still fails by
629,100,544 bytes. This does not close 24-hour/seven-day, producer or history gates.

## Minimum target

The mandatory baseline is the user-selected VPS, with:

| Dimension | Required target |
| --- | --- |
| Platform | Ubuntu, x86_64, ext4 |
| CPU | 2 vCPUs, as verified on the replacement baseline |
| Physical RAM | 4 decimal GB, enforced against actual available host memory; account for OS and all consumers |
| Whole-server disk envelope | 40,000,000,000 bytes, including OS/runtime, full archive, WAL, compaction, logs, restore and recovery headroom |
| Required recovery reserve | At least 8 GiB (8,589,934,592 bytes) held free; retain `max(8 GiB, database bytes / 5)` inside the whole-server budget |
| Required steady node RSS | At most 2 GiB, additionally constrained by host headroom |
| Required peak node RSS | At most 2.5 GiB, additionally constrained by host headroom |
| Host memory headroom | `MemAvailable` at least 768 MiB; no OOM or swap concealment |
| Synchronization and production | Catch-up faster than block arrival, independently verified head, then mixed-load head following, producer-path work and restart/recovery on this host |

Earlier catalog reference, retained as context: provider sources checked on 2026-10-15 show the
[cost-optimized catalog](https://www.hetzner.com/cloud/cost-optimized/)
lists CX23 with 2 vCPUs, 4 GB RAM, 40 GB NVMe and 20 TB EU traffic, currently
unavailable. The
[June price notice](https://docs.hetzner.com/general/infrastructure-and-availability/price-adjustment/)
lists EUR 5.49/month excluding VAT and IPv4 for CX23 in Germany/Finland. This
is a published tariff reference, not a current purchasable or account-specific
quote. Refresh availability/pricing before any future purchase decision. This
catalog shape is not a reason to replace or enlarge the user-designated baseline.

The provisional node RSS ceilings are the original 4-GB engineering gates,
not achieved measurements or selected runtime defaults. Peak/headroom checks
must hold concurrently. The interim 2-GB host and its proposed stricter ceilings
are retired with that reference. Include production candidate construction, PoB calculations, mempool
selection, VM/state work, RPC/P2P and durable recovery in the actual host budget.
First validate producer behavior using isolated controlled-chain fixtures.
Mainnet activation still requires a reviewable plan and explicit confirmation of
the network, producer target and key/registration checks under project guidance.

Do not silently raise the CPU/RAM/disk target, prune history, omit receipts,
reduce the amended 8-GiB reserve floor, weaken source authentication or substitute a
state-only/remote-only archive. The 40-GB disk is a measured whole-server
qualification envelope, not an indefinite guarantee for growing history.
An unresolved disk blocker remains explicit; LR2 does not close LR3/LR4.

## Whole-server restore acceptance (2026-10-15 clarification)

The user reaffirmed that restoring the complete backup within the **40 GB
whole-server disk envelope** is a required improvement. Keep this acceptance
gate explicit alongside LR2 memory work; naming LR3/LR4 separately does not
defer or waive the requested restore outcome.

Use the opt-in single-copy restore path with an independently authenticated,
pinned full-history backup. Include Ubuntu, installed runtime, metadata,
temporary transfer files, validation/WAL/compaction, logs and recovery reserve
in measured peak disk use. Do not retain both a complete local object repository
and a complete restored payload. Preserve every selected block and receipt,
the required `max(8 GiB, database bytes / 5)` reserve and observer-first activation policy.

Acceptance requires an actual full-size restore, validation, reopen and verified
observer head progress on the designated replacement VPS, plus interruption/
resume and recovery evidence. Record actual filesystem capacity and available
bytes throughout. Tiny signed fixtures, a 40 GB directory on a larger machine,
metadata estimates and successful reserve refusal do not prove this gate.
If the authenticated input cannot fit with the required reserve, keep the
failure explicit and pursue lossless storage improvements; do not enlarge the
server, prune data or reduce the reserve to manufacture a pass.

The [fresh source and capacity assessment](../validation/restore-40gb-current-baseline.md)
records corrected public checkpoint identity and the 15 October native VPS space
model with a 4,140,425,216-byte deficit. That metadata preflight passes with a
below-recommendation warning; stage refuses before payload/DB creation.
No successful full-size restore is claimed.

### Publisher update clarification (2026-10-15)

The user identified the installed Teleno on their miner as the public-backup
writer and directed updating it as needed to correct the backups published to
the configured seed repository. This newly scopes the observer publisher
update; earlier prohibitions on changing unrelated existing nodes remain.
The publisher was an observer running `1.2.0+b8dab4c08f99`. Its isolated
maintenance successor is now installed and verified, retaining explicit
production disable, original configuration/data, protected administration and
prepared rollback. A newly created public backup contains checkpoint-verified
identity; old input is preserved without rewriting its metadata. Source/archive,
executable and image hashes are recorded in the maintenance report. The mixed
LR2 binary remains separate. Signature policy is unchanged and absent signatures
remain reported as absent. Full-history/40 GB qualification remains required.

## Cost-optimization loop and completion condition

1. Recheck branch, existing changes, issue and matrix. Hash the actual dirty
   source, dependency pins, fixture inputs and binaries; a HEAD alone does not
   identify this implementation.
2. Profile one remaining resource cost or select a missing acceptance check.
   Record the measured baseline, expected RAM/CPU/disk/I/O benefit, proposed
   change, invariants and required checks before broad implementation. A
   prerequisite correctness correction needs its own evidence rather than an
   assumed resource-saving claim.
3. Implement the smallest coherent improvement. Preserve transaction/block
   validity, bytes, execution/metering, receipts/events, roots, nonce/RC,
   forks/head/LIB, durable publication, authenticated restore and local admin
   protection. Local overload must remain local, without peer blame, partial
   mutation, duplicate effects, false success or automatic retry after commit.
4. Run focused checks; run broader native CTest when shared behavior changes.
   Use real ASan/UBSan/TSan controls before interpreting integrated diagnostics.
   Record instrumentation exclusions and failures rather than waiving gates.
5. Measure the affected RAM/disk axis and its CPU/I/O/latency costs. Use comparable
   inputs and at least five paired samples for claimed improvements. Requested
   allocations, retained payload and a process RSS are different measurements.
6. Review the final change and evidence. Update the matrix, checkpoint log,
   relevant native docs and affected Koinos One documentation. Repeat while
   required implementation or evidence remains. Keep AGENTS/CLAUDE identical.

Completion requires all issue #9 rows: M1 aggregate/fork/workspace admission,
P2 real transport/bookkeeping, R1 simultaneous request/service/output capacity,
V1 complete VM working set, S1 all-consumer state lifetime/capacity, D1 working
diagnostics, Q1 representative Linux whole node, H1 historical compatibility,
B1 actual-current-source regressions and O1 documentation. A passing component
or an old release result does not close the goal.

H1 must include the required range from before 30,504,202 to beyond 32,789,378,
with pinned reference versions and comparable checkpoints. Eligible history
must be independently authenticated and pinned; a local journal hash only
establishes local byte identity. Preserve the complete canonical history from
genesis and every block/receipt supplied by the selected backup. Never use
existing producer/node data as a test fixture.

When a prerequisite is unavailable, document it, continue independent work,
and keep the row open. Goal completion requires actual proof, not a rewritten
acceptance condition. No main merge, push, release, production operation,
source-trust change, mainnet signing/submission, GUI/packaging or submodule
change follows from this goal.

## Development storage and Linux execution

Current owned run:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/`.

Source its `environment.sh` before each local command that generates data.
The guard checks the mounted volume identity and owns separate `tmp/`,
`artifacts/`, `build/`, `evidence/` and `dependencies/` directories. It exports
`TMPDIR`, `TELENO_TEST_ARTIFACT_ROOT`, `TELENO_TEST_BUILD_ROOT`,
`PYTHONDONTWRITEBYTECODE=1` and external native/dependency roots. Build parallelism
starts at two. Fail closed if the volume is unavailable. Mac CMake trees must
name `/Users/pgarcgo/code/teleno`; preserve old dependencies/evidence as inputs.

Before using the miner, verify host identity, actual available resources,
filesystem, toolchain, active node ownership and existing workloads using
bounded read-only checks. Use a fresh owned directory, a hash-verified source
export, bounded build jobs and isolated ports/data. Do not change services,
read/copy keys, reuse operational databases, alter the host's global memory/CPU
or storage settings, or enable production. Record private access details only
in private external evidence, never in committed documentation.

A container/process memory cap on a larger machine is a development diagnostic;
it is not whole-machine 4-GB RAM/40-GB disk qualification. Record the effective
limits and host headroom separately. Resource restrictions must affect only
owned test children, never existing node or mining processes.

## Checkpoint log

### Purpose clarification: optimize the operator's own application (2026-10-17)

The user reaffirmed that the work concerns their open-source node software and
their own reference VPS. The plan and reusable loop now explicitly describe
application RAM profiling, memory-management improvements, lossless database
storage and backup/restore capacity, with measured CPU/I/O/latency tradeoffs.
The Koinos cryptocurrency workload remains explicit. All full-history, receipt,
40-GB whole-server, producer, correctness and authorization requirements remain.
This changes the description of the work, not the technical acceptance gates.

### Purpose clarification: reduce node operating costs (2026-10-16)

The user requested wording that clearly describes code optimization for an
inexpensive Koinos node. The issue and this resumption contract now lead with
that outcome. The resource envelope, full-history/receipt requirements,
40-GB restore, producer checks, existing evidence and authorization boundaries
remain unchanged. This documentation update does not establish a new
performance result or close an implementation/qualification gate.

### P2-D: score/candidate bookkeeping (2026-10-16)

Five old defects and one intermediate review regression are reproduced and fixed;
see [P2-D](../validation/low-resource-lr2-peer-bookkeeping.md). Mac 56/56 and Linux
56/56 exhaustive 54/1/1 batches pass. Native all targets/CLI/real-clock, separate
eight-fixture ASan/UBSan/TSan with controls, 11 actual-VPS normal-ASLR components
and isolated runtime-container CLI/hash pass. Five paired samples per cohort,
plus supplementary direct child CPU/I/O, record both reduced retained allocation
requests and the unique-address elapsed cost. Sixteen Go/native single-query
vectors match on both Mac and Linux; the native query-anchor protection is an
explicit policy difference. Raw fixtures/binaries/evidence are externally
archived and verified before any completed duplicate retirement.

Current manifest is `10b59877afabe323399ca5a8803a33256ea5f6208803a87640de267f9878027a`;
Linux native SHA-256 is `cfb5a0d1bcb407d52f8f6cbf618bf0586a80e7237fb8bfb965760cb0e2d8f700`.
P2 aggregate native/SDK/snapshot/dial ownership and full D1/B1/Q1/H1/40-GB gates
remain open. Source review identifies pre-publication connecting-marker rollback
and eager known-peer snapshots as next P2 candidates, not corrected/qualified
behavior. Strict Koinos One manual/rendered-identity and scoped preservation checks pass.
Existing nodes and the installed maintenance publisher remain on their existing
images/configuration; no mixed LR2 deployment occurred. The live publisher's
active service, maintenance image and configuration hash are rechecked.

### P2-A: complete Peer RPC writes and functioning Linux diagnostics

The original single-`writeSome` behavior reproduced a truncated frame reported
as successful. Real request and response paths now use an owned complete-frame
operation, with unchanged wire bytes, sequential callbacks, short-write
continuations, checked counts and exception/local-allocation handling. It
does not replay an RPC. Mac all targets/version/help pass and **52/52 CTest
pass in 202.83 seconds**. Binary SHA256
`187f6b39322c3edc3c3b8139c89890ce98d0d40c7f3f17afb7722a0da63d26d3`,
identity `1.3.0-dev.0+d3cfb94455da-dirty`.

Separate Linux ASan/UBSan/TSan builds/runs of the actual helper/codec fixture
pass. Standalone controls now work for all three sanitizers; five paired TSan
experiments require ASLR disabled only in the diagnostic child. All six
clean/known-fault controls also pass on the designated replacement VPS, using
privately bundled runtimes without installing or changing host libraries.
No global ASLR or service setting changed. Integrated D1 remains open; the
serialized Writer fixture does not prove concurrent transport/shutdown safety
and GCC's unsupported atomic-fence instrumentation warning is retained.

Two B1 prerequisite portability fixes preserve Linux failures and correct
the SDK's missing transitive C++ gRPC static-link dependency and a typed JSON
digest comparison in a synthetic restore tool. The corrected static-link
target, all-target Linux build and **52/52 CTest (122.88 seconds)** now pass.
Linux binary SHA256 is
`aa2f9c11d829b797ade8424f7173e92bae5f00efa994bb184bff6790733fb36e`.
Mac signed synthetic restore/activation/reopen digest verification passes.
Koinos One networking documentation and external strict MkDocs checks pass;
no packaged inclusion, submodule or application code changed.

See [P2-A evidence and exclusions](../validation/low-resource-lr2-p2p-writes.md).
All package logs/manifests and private Linux routes are under the current
external run's `evidence/p2a/`. This native execution has completed successfully;
`linux-native-summary.json` is the terminal checkpoint. No build from this
checkpoint needs further polling. Nine isolated native
CLI/component/restore commands also pass on the actual replacement baseline,
including the producer with synthetic chain/mempool and the real preflight CLI
regression script. The original L0 attempts below remain historical. Next:
continue real transport lifetime/capacity and fragmented-read work with the
functioning diagnostic controls; do not rerun a completed build without cause.
No full-history producer, whole-machine resource, historical or aggregate
admission gate is closed by this checkpoint.

### R2: replacement baseline and persistent access

The replacement baseline is verified and its dedicated restricted SSH key works
in a fresh noninteractive connection. The server-enforced temporary password
change was completed without retaining a password. The active private route
points only to the replacement. The first server is marked retired; no further
access, cleanup or deletion is requested for it. Address/access information and
full inventory remain solely in private external evidence.

### B1: current-source fresh Mac native baseline

A new external CMake/Ninja tree configured successfully and completed all native
targets with two build jobs. Existing dependency installs were read-only inputs;
the hash-pinned corrected Fizzy library was built inside this new tree. The
complete native CTest suite passed **51/51** in **209.86 seconds**, including the
host-memory, producer, persistence, RPC and CLI fixtures. Version/help succeeded;
binary identity is `1.3.0-dev.0+d3cfb94455da-dirty`, SHA256
`29d3053bb4093278fe13ea0bed8d614433cfb644d6c78afafb2bc1f40c8c86bb`.

Evidence: `evidence/b1-build-results.json`, `b1-native-summary.json`,
`b1-ctest.log`, `b1-ctest.xml`, version/help and command/source manifests under
the owned external run. This is a fresh native build using existing dependencies,
not a fresh complete dependency workflow, integrated sanitizer pass, Linux
qualification or producer readiness on the designated VPS. Next, build a
hash-verified Linux source export using existing pinned miner dependency inputs
in a fresh owned directory with bounded resource monitoring.

### L0: isolated Linux native baseline in progress

A hash-verified export contains 282 current native source/build/test/config/tool
files, with the original shallow Git base and actual dirty-source identity.
Source archive SHA256:
`8a85c7e22cf306703a2b2cd4967a7facfc2a36458ee6b156c83440e19ed8f771`.
The export and manifests are retained on My Passport. Existing Linux dependency
installs were located and verified as inputs, including cpp-libp2p
`c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`; they are not modified.

The Linux build uses a fresh owned source/build/fixture directory, one low-
priority build job and guards that stop only its own process group if free disk
drops below 10 GiB or host available RAM drops below 2 GiB. Existing nodes and
their databases are not fixtures or deployment targets. Initial configuration
failed because the zstd install was not selected; its raw failure is preserved.
Selecting the existing verified zstd input corrected configuration. The
subsequent all-target build was observed running with a live process. No Linux
build, CTest or target-VPS readiness pass is claimed yet.

Continuation reads the private `evidence/linux-build-route-private.json` and
`linux-live-session.json` in the current run, then polls the actual execution
handle/process before deciding whether to wait or repair a terminal failure.
Do not restart a live build because a progress file is old or a poll times out.

### G0: goal and preservation baseline

- Live issue #9 was open, last updated 2026-10-07; its full body was reviewed.
- Active branch: `codex/lr2-memory-hardening`; base HEAD
  `d3cfb94455da4a472f651eb4239abbc289e74225`, dirty implementation preserved.
- My Passport mount identity was verified before writes; 232,000,102,400 bytes
  were available at run creation. This is development storage, not target disk.
- `evidence/worktree-before.json` fingerprints 193 pre-existing dirty/untracked
  paths. Git status, tracked diff and staged diff are retained alongside it.
- This setup does not claim a new runtime fix, test pass, sanitizer pass or
  Linux capacity result. First execution checkpoint: inspect miner feasibility
  and restore a reproducible current-source B1 baseline before changing policy.

### D0: isolated Linux diagnostic controls

The authorized read-only inventory confirmed an accessible Ubuntu/x86_64/ext4
development host. Resource headroom does not justify a full dependency build on
that host at this checkpoint, so compilation remains on My Passport. Small
standalone controls used one owned remote directory and low-priority sequential
compiler/test children; no existing service, configuration or database changed.

GCC 13.3.0 compiled all selected controls. Clean ASan and UBSan programs entered
and completed `main` with exit zero. Their intentional heap-use-after-free and
signed-overflow controls respectively exited nonzero with the expected
diagnostic: **four standalone checks passed**. The clean TSan executable exited
with signal 11 before its entry marker, without a report. Its fault control was
therefore skipped. Immediate TSan startup failure is reproduced outside Teleno;
the underlying cause remains unattributed. No host-wide runtime/ASLR setting
was modified to make a control pass.

Private command/source/log/result evidence is retained under the current run's
`evidence/`: `run-miner-sanitizer-controls.py`,
`miner-sanitizer-controls-remote.py`, `miner-sanitizer-controls-stdout.json`
and `miner-sanitizer-controls-stderr.log`. The result records source/log hashes,
compiler, kernel, exact commands, exits and entry markers; owned remote routes
remain private. These are standalone tooling controls, with no Teleno dependency,
not integrated D1 completion or target whole-machine qualification. D1 remains
open; B1 current-source native regressions and independent implementation can
continue while the TSan environment is investigated.
