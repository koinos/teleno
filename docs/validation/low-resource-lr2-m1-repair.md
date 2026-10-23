# M1 finite repair allowance

Status: **finite C3/C4 and C5/C6 attempts closed unqualified; M1 remains open**
(2026-10-20).
The [amended goal](../implementation/issue-9-m1-aggregate-goal.md) permitted work
from 14:16:04 to 22:16:04 UTC and at most two additional frozen implementations,
C3/C4. Both slots are consumed. C3 was frozen at 16:51:19 UTC and failed normal
qualification; C4 was frozen at 18:37:05 UTC. C4 passes normal and scoped
ASan/UBSan checks, but its default TSan run reports a lock-order inversion and
no representative whole-node memory profile was obtained. The attempt stopped
at 19:43:02 UTC, within its deadline, without changing the frozen source. The
[first attempt](low-resource-lr2-aggregate-memory.md) remains closed and
unqualified, with its original C1/C2 identities, counters and failures intact.
No live VPS or producer operation was performed.

## Reproduction and development results

The unchanged baseline query-fault executable
`3ce6a4dec3ff1cdc187ecdf637a7394506ad0a0921153c39a7bba8044a414dcc`
again reports the inherited **1,008-byte leak in 28 allocations**. A separate
baseline generated-oneof fixture fails its next allocation while switching an
existing upload operation to a call operation; it publishes the new case before
allocating its object and ASan detects an invalid access during destruction.
Its executable is
`1024521714d9bc2a45b8e26c5d4bb47d2ec708d070226b589057e854ed1c6d9b`.
These are isolated development fixtures, not live-node failures.

A later integrated Linux ASan development sweep exposed a separate
**heap-use-after-free** when Protobuf 3.17.3 copied a nested unknown group and
the deep copy threw. `UnknownFieldSet` had already published the source pointer
in the destination. The preserved fault trace is
`evidence/aggregate-development-v4-uaf.stderr` in this run. The build-owned
dependency patch now prepares a deep copy before publication and cleans a
partially built group. This was an intermediate development finding; the frozen
C4 fault sweep and ASan/UBSan suite pass below.

The first integrated build also exposed a test-boundary error: a prior run's
transient peak did not equal the next run's exact admission demand. The test now
fills the actual live budget before refusal, checks unchanged accepted state,
releases the held reservation and retries. The following development run exposed
the generated-oneof failure again because transitive installed headers preceded
the checked build-owned headers in the compile command. CMake now puts both
build-owned generated and protobuf headers first; a fresh Linux configure
confirmed that order. The later frozen C4 normal and ASan/UBSan results below
include both corrections.

The subsequent **unfrozen** Linux ASan/UBSan aggregate sweep returned zero
with leak detection enabled, executable SHA-256
`1321d9b6d820c30c2ad7e5688f5c49ce09d8d352468ca1e21a3b426a522c3046`.
Its result and exact stdout/stderr hashes are preserved in
`evidence/aggregate-development-v10-result.json`. This executable still had a
temporary owner-probe CLI mode, now removed from source; it cannot qualify the
final source. The earlier 159-byte LeakSanitizer report was a false positive:
the tracked allocation header held the live budget owner but was poisoned and
therefore invisible to leak scanning. Four short owner-lifetime stages passed
after leaving the owner header visible while retaining an immediately adjacent
poisoned underflow guard. Frozen C4 instead uses a malloc-backed side table;
its final allocation-owner and leak checks pass below.

The same development source, after removal of the diagnostic CLI mode, passed
the full aggregate mempool ASan/UBSan sweep with leak detection (binary
`e012aa56290d374fbde1223108631f48059a5dfb7d70a7b4d2a227e76e364408`).
Owned-allocation, owned-budget and controller tests passed in the integrated
run. The real-VM host test first exposed an unaligned write in its new fixture;
after replacing that write with a byte copy and placing the capacity saturation
inside the callback, it passed (`e7ab8a35d7b34decc4737132b475c777f0899eeb7ae57baf40ce85739bea051d`).
The simulated producer and unknown-field tests passed under the same
instrumentation. These are successive development runs, not a single frozen
candidate suite; their preserved logs identify the precise binary for each.

A dependency-repair development stage then passes both Linux ASan/UBSan entries:

| Entry | Result | Executable SHA-256 |
| --- | --- | --- |
| Protobuf unknown-field ownership | Pass; 0.217 seconds | `b2da4c0a53c6a84e89a9c96de53d58e81fd261a7e48aaf3555a217f801e205fb` |
| Aggregate mempool memory, including the formerly leaking decoded-query fault sweep | Pass; 288.093 seconds | `801c3656355d35870b1e5936afbf5632b103b5f4f30a3b1e1e505ea24c82e472` |

The dependency-stage source manifest SHA-256 is
`34b279a0cf9ae2b6a51cfc48e6f0b886753418885032e78c0279797bc9649db2`.
This stage includes checked native record parsing, completed generated copy
targets and repeated-field exception cleanup. It precedes the newer generated
oneof repair and complete ordinary-allocation integration; it cannot qualify
that later implementation. Leak detection is enabled; UBSan excludes vptr,
matching the pinned dependency diagnostic policy. Native code, protobuf,
generated Koinos messages and Fizzy are instrumented; other prebuilt SDK/crypto/
storage dependencies remain outside this instrumentation. Libp2p is disabled
in this focused development configuration.

Two standalone Linux ASan/UBSan development tests also pass:

- Owned ordinary allocation forms, escaped/cross-thread final-owner release,
  container field copies, typed capacity refusal, protected work, retry and
  concurrent reservations: executable
  `7b22f0fccc5645bab20945635f8c0be504cdc9e50500f3c056088c43fa7ecd4d`.
- Owned budget/allocation-failure and lifetime behavior: executable
  `5b207af5fba4c82d80d2fa776edd1b9152c75ab79827a54d1264095f728cdb03`.

These are historical component development results. The frozen C4 source,
normal suite, scoped diagnostics, exact CLI binary and paired Linux measurements
are identified separately below; representative whole-node qualification remains
unmet.

## Implementation in frozen C4

The build-owned dependency repair preserves original generated layouts and wire
bytes, constructs message copy targets before copying their fields, publishes
repeated-field owners incrementally, and allocates oneof message objects before
clearing/publishing the old case. Exact input/output hashes and patch-state checks
reject unknown or mixed inputs. Installed SDK files and protoc remain unchanged.

Native lifetime accounting extends to ordinary C++ allocations made
within M1 operations, nonce/RC/read workspace, protobuf/RPC/serialization output
and native execution/receipt ownership. Allocation credits accompany the physical
allocation until its deletion, including movement and deletion on another thread.
Owned container requests are charged once; additional workspace remains explicitly
supplementary. Ordinary read admission can refuse locally; canonical block/LIB
and recovery processing remains observable and quota-exempt.

The C3 allocation primitive added a prefix and alignment space to C++
allocations, which proved incompatible with a pinned RocksDB size query. C4
returns allocator-base pointers and keeps enabled allocation leases in a
separate malloc-backed registry. Its scoped RSS/CPU/latency cost is measured
below, but not qualified for the complete node. Direct dependency malloc/realloc, allocator arenas, stacks and the VM
allocation domain are excluded; neither attributed requests nor supplementary
reservations are a physical heap/RSS ceiling. Fizzy's noexcept C API must not
receive M1 quota exceptions; native callbacks restore M1 capture and propagate
local failures through the existing native exception path.

## Remaining gates

- Resolve or conclusively qualify the default TSan lock-order warning without
  hiding any feasible cross-path deadlock. The race-only pass cannot replace
  the default 14/15 result.
- Establish a representative Linux M1 policy/profile and physical memory
  margin. The scoped five-pair fixtures below show higher RSS, not an M1 RAM
  saving, and direct dependency malloc/allocator arenas remain outside the
  attribution ceiling.
- Qualify complete node/runtime integration and whole-server resource targets
  separately; the VPS producer was not touched in this allowance.
- A new source candidate or deployment requires a separate authorization. The
  C3/C4 slots and original 22:16:04 UTC deadline are not reset.

The operator additionally requests the
[legacy applicability register](../implementation/legacy-applicability-of-teleno-findings.md).
It records inspected public revisions, reproduction plans and uncertainty;
no legacy service-binary reproduction or deployment is claimed.

## C5/C6 follow-up scope (authorized 2026-10-20)

The operator authorized necessary further M1 attempts on 2026-10-20. The
first new attempt has its own owned run, a non-resettable eight-hour interval
from 00:35:55.857720 UTC and at most two frozen source candidates C5/C6.
Keep C1–C4 and their evidence closed. Do not
touch or restart the live VPS producer, deploy a binary or submit a transaction.
The exact starting point is the C4 frozen archive identified below, plus the
separately identified later documentation edits; do not substitute an
unmanifested worktree for the compiled candidate.

First reproduce C4's default TSan M0/M1 warning with its clean and known-race
controls. Prepare one deterministic order for acquiring all relevant fork
mutexes in admission, removal and prune while retaining the existing
failure-atomic multi-fork staging, head counts and block/LIB behavior. Test
opposite fork enumeration orders, concurrent readers, add/prune/remove, block
and LIB transitions, allocation refusal and successful retry. Rerun default
TSan with deadlock detection enabled; the race-only result is supplementary.

Measure the C4 malloc-backed lease table directly: live entry count and bytes,
bucket occupancy, allocator active heap and process RSS across admission,
expiry, retained ancestors, escaped query owners and final release. Keep the
measurement path outside the admission quota and verify it cannot change
publication or failure classification. Optimize the table or its scope only
if accounting remains lifetime-owned and all final-owner tests pass. Use at
least five paired equivalent baseline/C4/new-candidate samples and report
allocation requests, accounted and retained bytes, RSS, CPU, I/O and latency
separately; do not call lower request totals a RAM saving when RSS rises.

Finish with the full Linux normal suite, exact binary version/help, focused
ASan/UBSan and default TSan with controls, and a measured representative Linux
M1 workload before selecting any low-resource policy. If the eligible input or
isolated resources are unavailable within the new fixed interval, stop and
leave M1 open. Component results cannot establish the reference VPS whole-node
RAM or disk gates. This scope changes no completed result.

## C5 development observations (before frozen qualification)

The unchanged frozen C4 TSan aggregate executable was rerun with default
deadlock detection and `second_deadlock_stack=1`. Its full mixed-fork check
reproduces an M0/M1 lock-order warning between admission and removal; the
short concurrency-only check did not reproduce it. The complete failure log
is preserved in the new run as `evidence/c4-tsan-default-full.log` (SHA-256
`c6f6ccb2d690b6c3e54b5da167a0b57ea8df4096c8acc21645f15fe95f447330`).
This is additional reproduction of C4's failed default detector gate, not a
change to its result.

The development C5 change locks selected fork mutexes in one multihash-key
order across admission, removal and expiry. It keeps the original overlay and
publication order. The first focused normal Linux build, owned-allocation
test and mixed-fork concurrency check pass. At this development checkpoint,
the exact frozen-source default TSan, ASan/UBSan and full suite had not yet
run; their results follow.

A development default-TSan full aggregate run subsequently passed in 910.0
seconds with clean and known-race controls; it reported no lock-order warning.
Before a test correction, a separate repeated short concurrency mode failed
2/20 runs with **zero accepted and 96 correctly classified local refusals**.
All eight workers can hold admission staging reservations at once under that
fixture's 512-KiB limit, leaving no room for any fork preparation until their
requests unwind. The earlier assertion incorrectly required an acceptance in
the burst. Its replacement preserves the no-loss, bounded-credit and fork
checks, permits all-local refusal, and requires a successful later retry.
The corrected short mode passed 20/20 normal and 20/20 default-TSan repeats.
These are development results; a frozen-source full suite remains required.

The new read-only lease-table diagnostic counts live malloc-backed table
entries, requested node bytes and bucket occupancy without reserving or
allocating. In one isolated 64-transaction Linux fixture it reports 1,606
entries (77,088 requested node bytes) with a caller-held query copy, 672
entries while that copy survives pool destruction, and 32 entries after its
release. Prewarming an untracked pool before the same fixture changes the
final result to **zero entries** and the budget's 240-byte fixed bootstrap
reservation. This is consistent with first-use process-global library or log
metadata being charged to the first enabled pool; it does not identify each
global owner or prove that all other lifetimes are leak-free. The per-phase
requested bytes, glibc active heap, current RSS and accounted bytes are in
`evidence/development-table-measurement-summary.json` in the new owned run.
RSS remains near 15 MB immediately after final owner release in this process,
illustrating why table credits are not a physical RSS measurement.

## C5 frozen qualification

C5 freezes 449 files from the C4 base at 2026-10-20 with source archive SHA-256
`6eeae7357d4164ca2af315996c3e96c688a629a63eac12d5f86a05ba9f62bfbb`.
The source manifest SHA-256 is
`568cf52880fb95414e9ef1adae27f2aca69bd30890e6ad7a0389c44c7ff1b027`.
Its exact normal Linux node is
`bb763b3e7281d5dd9f7a234896453c42285c0db9fba9b0bb699168e2055e4f44`
and reports `teleno_node 1.3.0-dev.1+d424264b24595786fdba86d9eddbcbf1635284b6`;
`--help` also passes. The linked-input/toolchain manifest hashes 172 inputs
(`1b9fbea2721f1fa619e8636e11f57482a5c373a752238ef98967558cef36b084`);
the separate compiled-input manifest hashes 5,083 unique inputs from 322
dependency files (`0671ccbbc66f0f537a364237611ba31c85935089920282dcd75e9af366de8439`).
These identify the isolated builder, not the running VPS producer.

The first complete normal CTest invocation passed 66/68; two Python harnesses
could not locate executables at the external build root. With build-root aliases
to the **same** binaries and no source or binary change, those exact two tests
passed 2/2. The corrected complete invocation then passed **68/68** from
02:24:31 to 02:28:42 UTC, with the same frozen source and binary hashes before
and after. Both initial failures, the corrected run, JUnit files and manifests
remain preserved; the initial 66/68 is not relabeled as a pass. The 16 focused
ASan/UBSan entries pass, including the formerly leaking aggregate fault sweep
and separate cleanup, with working clean, known-UAF and known-UB controls.
Prebuilt SDK libraries and libp2p are outside this scoped instrumentation.
The exact ASan build was archived on My Passport and verified member-by-member
before its owned builder copy was removed to recover disk space; archive SHA-256
`380d3acac98f033ff1f9c93cc351e3bdcc12eb930ee1a242d110d15723aad06a`.

The first frozen-source default-TSan run passed 14/15 entries and the separate
cleanup (189.69 seconds). The aggregate entry reached its source CTest timeout
of 600 seconds; the full development aggregate had needed 910 seconds. That
timeout remains a failed result. A second frozen-binary default-TSan suite
changed **only the generated CTest timeout** from 600 to 1,800 seconds. It
passes **15/15**, including the 878.03-second aggregate fault/concurrency
battery, with no reported race or lock-order warning. The original separate
cleanup pass and working clean/known-race controls remain part of this scoped
result. The frozen source and all 16 instrumented binary hashes match before
and after; the timeout override has its own before/after generated-file hashes
in `evidence/c5-tsan-timeout-override.json`. This is a corrected test-harness
qualification, not a source change or a whole-node concurrency proof.

Five alternating baseline/C4/C5 triplets for each identical 64-transaction
native fixture record the following medians. `new` requested bytes exclude
dependency `malloc`, glibc heap excludes retained free arenas, and the RSS is a
small component process peak. All three variants have zero reported physical
input/output blocks in these in-memory fixtures.

| Workload | Variant | Peak RSS bytes | `new` requested bytes | Accounted bytes | Elapsed ns |
| --- | --- | ---: | ---: | ---: | ---: |
| Ordinary | Baseline | 12,984,320 | 12,529,779 | 0 | 10,014,792 |
| Ordinary | C4 | 14,970,880 | 10,852,887 | 156,599 | 17,652,096 |
| Ordinary | C5 | 14,180,352 | 10,854,695 | 156,599 | 17,748,930 |
| Held query | Baseline | 15,024,128 | 22,536,867 | 0 | 25,541,354 |
| Held query | C4 | 16,171,008 | 15,693,383 | 1,514,513 | 23,380,799 |
| Held query | C5 | 16,150,528 | 15,695,191 | 1,514,513 | 23,441,623 |

The same per-sample reports also retain allocation counts, allocator-requested
bytes, glibc active heap, CPU time and I/O. C5's measured RSS remains above
baseline in both small workloads; no whole-node RAM saving follows.

A controlled federated chain profile then exercised the exact C5 normal
libraries with the real mempool, 32 local signing payers and 128 signed
no-operation transactions per block. At a 256-MiB aggregate policy, admission
refused locally after 43 complete blocks and 65 further accepted transactions;
the child exited on that refusal, with kernel peak RSS 306,335,744 bytes and
zero reported swap. This is a policy refusal, not a demonstrated OOM. The first
attempt cannot prove complete 100-block production or reopen.

A fresh owned fixture raised only that policy to 768 MiB and printed progress
every ten blocks. It reached block 62 before its fixed 900-second process limit.
At block 60 the counter showed 496,950,954 current and 564,639,994 peak
accounted bytes, with zero refusals. Systemd records a timeout, approximately
426.7 MB cgroup memory peak and zero cgroup swap peak. The wrapper was killed
before writing its process RSS report; that missing value is not inferred from
the systemd figure. Neither trial completed its 100-block/reopen checks, so C5
is unqualified for the representative profile. The failed run output, time
limit journal, input/binary hashes and separate assessments are retained in
`evidence/c5-representative-*` of the owned C5/C6 run.

Three read-only GDB stack samples during the second run all found the active
thread in `owned_allocation::deallocate`, including when destroying canonical
receipt and state-delta temporaries. Its 1,024-bucket side table linearly
searches a bucket on every delete, including pointers that carry no M1 lease.
This is a concrete CPU-cost diagnosis, not a claim that every cycle was spent
there. C5's frozen source, binaries, test outcomes and two profile failures
remain unchanged.

## C6 table development and freeze

The next source change increases the process-wide side table from 1,024 to
65,536 buckets, retaining malloc-base pointers and lease lifetime. It adds
about 1 MiB of static table storage on 64-bit Linux. A focused, alternating
five-pair Linux benchmark held 100,000 charged allocations while deleting
200,000 uncharged allocations per sample. Its identical benchmark source has
SHA-256 `5b7bc279a0fb6e5d870eab89c7413141a53e769c26ef310ad397e1d961c0c2fe`.

| Variant | Median deletion elapsed | Median bucket depth maximum | Median peak RSS |
| --- | ---: | ---: | ---: |
| C5 | 2.255608 s | 98 | 20,688,896 bytes |
| C6 development | 0.014376 s | 2 | 21,737,472 bytes |

Each sample ran under a 2-vCPU, 2.5-GB, no-swap child limit. The full ten
non-warmup sample records, CPU, I/O, allocation counters, executable hashes and
source hashes are in `evidence/c6-table-benchmark-result.json`. This isolates
the diagnosed lookup path; it does not establish a whole-node throughput or RSS
improvement. C6 freezes 449 files with only `src/core/owned_allocation.cpp` as
a runtime change from C5; its source archive SHA-256 is
`47481eab86268e0f1d5b4f90b9cbde2f348d6d7513854842b23ed114e4064a22` and
manifest SHA-256 is
`f01cabb2e007538dfb520aaf3343871c041f1d0c49b24d9a54ac1b0a3fa55837`.
The exact C6 normal Linux node has SHA-256
`075730832dd57fb7cf5d9d87761a3048b1f35bbc0fdbd585321a3cf8d29a39c4`,
reports `teleno_node 1.3.0-dev.1+d424264b24595786fdba86d9eddbcbf1635284b6`
and passes `--help`. Its single complete native invocation passes **68/68**,
with source and binary hashes unchanged before and after. The linked-input
manifest identifies 172 objects/libraries, SHA-256
`e51e2c32c815391865f943065529c01f7f160b6779965d739b5631166ec1ce3f`;
the compiled-input manifest identifies 5,083 inputs from 322 dependency files,
SHA-256 `6735aa16c269566aaf1c271f47fb800c3731794cef6abd2e524360e9765a220b`.
The sparse ext4 test fixture is functional capacity only, not a physical
whole-server disk measurement.

Two fresh controlled-chain runs then passed 100 blocks × 128 signed no-operation
transactions using the real native mempool, producer, state and block stores.
Each independently reopens at LIB 40, re-executes the 60 reversible blocks and
matches its own head ID, state root and complete block/receipt archive. No
network adapter or operational key is involved. The second uses the **exact
same fixture source and 768-MiB quota** as C5's timed-out trial, with SHA-256
`00a6b4bf262c599923d3cd81cb1b9ad65e7750e26a426c8f8e36305553ba3378`.

| C6 quota | Create wall / CPU | Create peak RSS | Peak attributed bytes | Reopen wall / RSS |
| --- | ---: | ---: | ---: | ---: |
| 2 GiB | 243.07 / 374.11 s | 818,077,696 | 564,639,994 | 12.41 s / 477,249,536 bytes |
| 768 MiB | 192.15 / 314.54 s | 808,235,008 | 564,639,994 | 5.83 s / 477,204,480 bytes |

Both runs report zero aggregate refusals and zero child swap. The 768-MiB
fixture has 240,666,374 bytes of unused attributed policy capacity at its peak.
Its raw block-family digest is
`0x7bb1b87da8406fad6a0697318406833037c6bfaa64e1921c9aa5c99613ad175f`
before and after reopen. The 2-GiB run has a separate chain/digest; the
different wall times are not a controlled quota-performance comparison.
Per-phase CPU, I/O, RSS, p50/p95/p99 producer latency, source/binary hashes and
raw output are retained in `evidence/c6-representative*-result.json`.

The builder has 16 GiB of physical RAM and host swap; each child is restricted
to 2 vCPUs, 2.5 GB and no swap. Federated no-operation transactions omit
mainnet PoB, VM contracts, RPC/P2P traffic and mixed real history. This selects
768 MiB only for this controlled profile. It does not establish a safe default
for the actual 4-decimal-GB VPS, whole-node RSS/`MemAvailable`, or issue-9
completion. Frozen C6's scoped Linux default-TSan run passes **15/15** main
entries plus the separately bounded cleanup in 96.41 seconds. The clean and
known-race controls behave as expected, deadlock detection remains enabled,
and source and all 16 test-executable hashes are recorded unchanged after the
run. The aggregate entry retained the previously recorded generated CTest
1,800-second timeout; no new source or binary change was made to extend it.
This is focused native instrumentation, not full P2P/VM/node lifecycle TSan.
The exact C6 focused Linux ASan/UBSan main run passes **15/15** in 373.03
seconds, including the inherited aggregate fault sweep with leak detection.
The separately bounded cleanup passes in 264.42 seconds. Clean, known-UAF and
known-UB controls behave as expected. Source and all 16 test-executable hashes
remain unchanged after the run. The ASan instrumentation still excludes
prebuilt Boost, RocksDB, crypto and SDK objects, disables libp2p in this scoped
build and disables vptr checking for the documented mixed-RTTI dependency.
These focused checks cannot prove a physical heap ceiling or complete runtime
lifecycle. The exact ASan build is archived on My Passport at
`artifacts/c6-asan-build.tar.gz`, SHA-256
`86b475e04e3aec16b0468db0da34e3aa0f0b34e7d98332e22e45a49fc21e84f6`.
All 4,489 regular archive members match their remote SHA-256 manifests, all 16
test executables match the recorded hashes, and 5,430 archive entries were
read. The remote owned ASan build was removed only after verification.

The C5/C6 control closed at **03:50:42 UTC**, before its unchanged 08:35:55
deadline, with both frozen candidates consumed. C6 passes its selected scoped
correctness and controlled workload gates, but **M1 remains unqualified**:
ordinary nonce/RC/count read workspace and escaped RPC/execution output still
lack complete lifetime/admission evidence; this workload omits mixed RPC/P2P/VM,
mainnet PoB and historical input; allocator/dependency memory and the physical
reference-VPS margins remain unproved. The 768-MiB setting is a controlled
fixture policy, not an approved operational default. Any further source
candidate needs a distinct finite attempt; neither C5/C6 counters nor their
deadline can be reset. No live VPS producer operation occurred.

## C3 failure and C4 development

The C3 source archive in the owned run contains 449 checked source files.
Its SHA-256 is
`660df6737cbb8bad07c147f68b65a7e5d62514e654364b2ecc2330587e0e071c`;
the accompanying per-file manifest SHA-256 is
`72e16cd3c374372b5c3ec53a922fd92800662a999d6cff88820ec8b8d9e9f6d6`.
The isolated Linux source copy passed per-file hash verification. Its complete
normal build passed under a 2-vCPU, 2,500-MB no-swap cgroup. The exact node
binary SHA-256 is
`b83a2573e5d1765545f0a3b93d281baea5c8ff22103996597953804bb2b55748`;
`--version` reports
`teleno_node 1.3.0-dev.1+d424264b24595786fdba86d9eddbcbf1635284b6`
and `--help` returns successfully. CTest lists 68 entries. An initial
qualification launcher counted only 59 because it assumed one fixed spacing
pattern in CTest's listing; that launcher failure is preserved separately.
With its parser corrected and C3 source unchanged, the complete normal suite
passed **44/68** and failed **24/68**. This is a failed C3 candidate, not a
partial qualification pass. The full log and JUnit record are in this run's
`evidence/c3-normal-*` files.

One confirmed cause is the global C++ `new` replacement: C3 returned a pointer
past its accounting prefix. The pinned RocksDB library calls
`malloc_usable_size` on such a pointer. An isolated GDB trace reaches glibc
`musable` from `rocksdb::BlockBasedTable::PutDataBlockToCache` and terminates
with SIGSEGV (`evidence/c3-rocksdb-gdb.log`). Other failures included an
uncaught local-capacity refusal during first Protobuf reflection initialization
and an aggregate-credit retention assertion. C4 development moves leases to a
side table while returning allocator-base pointers and exempts immutable
process-global reflection initialization from the per-work M1 quota. This
repair is frozen and passes normal/ASan checks but remains unqualified. No live VPS producer operation is part
of this work.

Five focused C4 development tests pass under Linux ASan/UBSan with leak
detection: transaction-work reservations, owned C++ allocation, aggregate
mempool memory, RocksDB manager and prepared atomic writes. The aggregate
fault sweep took 351.51 seconds; its additional runtime is a tradeoff to
measure, not a saving claim. An earlier development run failed before the
process-global Protobuf reflection initialization was isolated; both outcomes
are retained in `evidence/c4-development-tests*.log`.

The C4 development normal build succeeds. A functional-suite run on an owned
sparse ext4 test filesystem passes 66/68; the two remaining harness entries
pass separately after the external build root and verified source manifest
are supplied. The initial missing-artifact-root run and the later
insufficient-host-free-space/build-root runs remain preserved as environment
failures. The sparse filesystem provides logical capacity for small unit
fixtures only: it is **not** physical disk-budget, full-backup-restore or
whole-node evidence. C4 was frozen at 18:37:05 UTC with a 449-file source
archive SHA-256
`e996c9efacb1e3e1c4863e5d8160c0d09c9882aebcb9ce8c65ee8384c1c43e0e`
and manifest SHA-256
`885eb2afa0930d825223f4c4722a0915fa75eae88944484e271580390350c3a7`.
The isolated Linux source copy matches all 449 frozen hashes. Its normal
binary SHA-256 is
`de567276d0a55a546be01d5143be4185228cefa737cdbcc2efc8d8ac2655122c`;
`--version` reports
`teleno_node 1.3.0-dev.1+d424264b24595786fdba86d9eddbcbf1635284b6`
and `--help` passes. The complete normal Linux CTest suite passed **68/68** in
214.59 seconds. The launcher verified all 449 source hashes and the binary hash
both before and after the suite; see remote `evidence/c4-normal-result.json`,
`c4-normal-junit.xml` and `c4-normal-tests.log`. This is functional component
qualification on an isolated sparse ext4 test filesystem, not physical disk or
whole-node qualification.
Project documentation was updated after the C4 freeze. The frozen source
archive, not the later local documentation worktree, identifies the compiled
candidate; its runtime source was not changed after freeze. No commit, push,
release or deployment is part of this allowance.

The exact Linux link/toolchain manifest hashes 172 resolved link inputs,
including build-owned generated Protobuf and pinned libraries; its SHA-256 is
`76e949e9987d16f10cb3112da0dc0add4dd7d190413455a9961965cee4592fdb`.
A separate manifest hashes 5,083 unique compiled inputs from 322 dependency
files, SHA-256
`86ed8e772f7ff3231cd62c19cf3f64c2fe637b990be6d93ec7ef744d64b9ab70`.
Both manifests are retained in the owned run's `evidence/` directory. They
identify this isolated build; they do not turn component tests into whole-node
or reference-VPS measurements.

The same frozen C4 source passes a focused Linux ASan/UBSan build and **15/15**
main instrumented tests. The formerly leaking aggregate allocation-failure
sweep passes with leak detection enabled in 344.27 seconds. A separately run
cleanup/fork fault test also passes in 175.23 seconds, giving **16/16** scoped
entries. Clean, intentional use-after-free and intentional undefined-behavior
controls produce their expected results. The launcher verifies all 449 frozen
source hashes after testing and records the 16 test-executable hashes in
`evidence/c4-asan-result.json`; the JUnit and individual logs remain in the
owned run. The first launcher stopped *before controls* because it had not
created its output directory; that harness error and successful corrected run
are preserved separately. Vptr instrumentation is disabled, libp2p is excluded
from this focused build, and prebuilt SDK/Boost/RocksDB/crypto objects are not
instrumented. This evidence does not prove machine-wide OOM safety or an RSS
ceiling.

C4's first default TSan run passes its clean/race controls, compiles all 16
focused executables and passes 14/15 main entries plus separate cleanup.
The aggregate mixed-fork entry fails on a **lock-order-inversion (potential
deadlock)** report, not a data-race report. The two reported per-fork lock
acquisitions are in admission and prune, each after `_db.get_unique_lock()`
acquires the same exclusive state-database node mutex. This outer gate makes
their simultaneous acquisition impossible in the reported one-pool test, but
the default detector warning is retained as a failed gate. A separate run with
only deadlock detection disabled passes **15/15** main entries in 571.41
seconds, including the 532.71-second aggregate sweep, plus separate cleanup in
149.73 seconds. Its clean and known-race controls both behave as expected, and
all 449 source hashes and 16 executable hashes remain unchanged. This supports
the absence of detected data races in these scoped tests; it cannot erase the
default warning or prove a global absence of deadlocks. Both results and their
distinct detector settings remain in `evidence/c4-tsan-*`.

### Post-closure static review of the TSan warning

The preserved default report identifies an M0/M1 fork-mutex order cycle between
`add_pending_transaction` at `mempool.cpp:837` and `prune` at `:956`, within
one `mixed_fork_cleanup_concurrency` pool. The reviewed local files match the
frozen C4 manifest byte-for-byte: `mempool.cpp`
`8ef7e22e6ff5db99ecd6ad96539e37cea48ec0618c6f794da914deb9b817f5b0`,
`state_db.cpp`
`243ca83ac673a8cb4860b3331eb5376cd99a25d804d8df05499d94d77065a879`,
and the test
`c2d0b84f47f54939747660de513c231ca1204b187e56c98522b1eecf3b0e7413`.
Both reported writers acquire this pool's `_db.get_unique_lock()` before any
fork mutex. That call takes the same `_node_mutex` exclusively; the local read
paths first take its shared lock and hold no more than one fork mutex at a time.
For these reported paths in this one-pool fixture, two threads cannot occupy
opposing M0/M1 acquisitions concurrently. TSan's pairwise lock-order graph
does not by itself establish a realizable deadlock under that outer gate.

This is a **static explanation**, not a passing default TSan result or a proof
for every callback, future lock path or whole-node lifecycle. Keep 14/15 as the
qualification outcome. A future authorized candidate should impose one stable
fork-lock acquisition order or otherwise give the detector a clean checked
path, then rerun the default detector with working controls and integrated
concurrency checks. The inspected legacy mempool source at
`koinos/koinos-mempool@2aa415fc75fd40038509f06e66688ab505cf8df8`
holds only one fork mutex at a time in admission, removal and prune. Therefore
this exact multi-lock inversion is not established for that legacy revision;
see the separate [legacy applicability register](../implementation/legacy-applicability-of-teleno-findings.md).

## Paired Linux component measurements

Five alternating baseline/C4 pairs were run for each of two identical native
64-transaction workloads. The measurement-fixture body matches byte-for-byte
with SHA-256
`ea09836abdb0ac135995ff2b661e79a8658f3f12e993bb510134e7350142ecc9`.
Baseline executable SHA-256:
`0b5bb0886c1e9e889824e85dfd7ad8bfc8059a09a9493733ed28dfb063ab90bf`;
C4 executable SHA-256:
`a66237543ae553eb7694bce990512f14af8898f27ea738210c717d70995ad709`.
Each child had a 2-vCPU quota, 2,500-MB memory limit and swap disabled; the
Linux build host has a different physical profile from the reference VPS.
Medians below are baseline → C4; raw paired samples and per-process time data
are in `evidence/c4-paired-ordinary/` and `evidence/c4-paired-query/`.

| Metric | Ordinary admission/expiry/LIB | Two caller-held query copies |
| --- | ---: | ---: |
| Elapsed time | 10.88 → 19.13 ms (+75.8%) | 25.82 → 22.85 ms (−11.5%) |
| C++ `new` requested bytes | 12.53 → 10.85 MB (−13.4%) | 22.54 → 15.69 MB (−30.4%) |
| C++ `new` calls | 142,117 → 142,199 (+0.1%) | 198,795 → 197,993 (−0.4%) |
| Peak process RSS | 13.05 → 14.12 MB (+8.2%) | 15.07 → 16.15 MB (+7.2%) |
| glibc active heap | 1.07 → 1.22 MB (+14.1%) | 2.20 → 2.44 MB (+10.7%) |
| User CPU | 10.42 → 18.20 ms (+74.7%) | 24.62 → 22.60 ms (−8.2%) |
| Counted process I/O blocks | 0 → 0 | 0 → 0 |

C4 additionally reports 156,599 accounted bytes in the ordinary fixture and
1,514,513 with held query owners; the baseline has no equivalent aggregate
counter, so a zero baseline counter is **not** a memory-saving comparison.
All five pairs show higher C4 RSS in both scenarios. Lower cumulative `new`
requests do not offset the observed retained heap and RSS overhead, and the
ordinary workload has a material CPU/latency cost. The query improvement does
not establish an operational tail-latency or whole-node gain. This synthetic
fixture is too small to select a representative low-resource VPS profile.

Generated inputs, manifests and detailed evidence stay under the owned mounted
volume run `/Volumes/My Passport/teleno-m1-repair-20261019-n2bhladg/`.
