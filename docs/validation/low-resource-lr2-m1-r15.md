# M1 R15 final-candidate qualification

Status: **M1 complete for the exact R15 implementation, 2026-10-23.** All
104 normal checks, both CLI checks, 63 focused ASan/UBSan and 61 default-TSan
cases pass on isolated OVH, with working controls and explicit exclusions.
Five pairs per measured workload, exact-source 100-block/12,800-test-
transaction creation/reopen, and the exact full-history observer's one-hour
mixed reads, clean restart and interruption/recovery pass. The ownership and
physical-result review below completes M1. Wider issue-9 requirements remain
open; this is an unreleased native development snapshot.

This is application memory optimization of the operator-maintained Teleno
node. It does not authorize an operational producer change or a release.
Generated evidence stays on mounted My Passport. The miner compiles only;
all executable checks use the isolated OVH test host.

## Exact implementation

| Item | Identity |
| --- | --- |
| Source archive | `3be9aa55f128f649d851ec87ca9dfa3574736eec0bdec9e4e9ebaecbae47995f` |
| Snapshot | 478 files; based on the existing dirty LR2 branch |
| Linux node SHA-256 | `e825f1951a583590e9173c57be79626635dbb110f750d1258eb34efffa31eef4` |
| Normal build | 71 ELF outputs; 168 linked-library identities |
| Toolchain | GNU C++ 13.3.0; CMake 3.28.3; native Release uses `-O1 -g1 -DNDEBUG` |
| Native version | `1.3.0-dev.1+unknown`; archive lacks Git metadata |
| Evidence root | `/Volumes/My Passport/teleno-m1-grpc-refusal-20261023-7u4u7z17/` |

The [ownership inventory](../implementation/issue-9-m1-owned-memory-inventory.md)
maps allocation creation, final ownership, release and quota treatment. R15
adds a separate bounded gRPC refusal allowance, preserving final-owner
credits when ordinary application admission is full. It includes the pinned,
build-owned gRPC call-binding correction required by early cancellation.
Instrumented mempool block/LIB and recovery work remain counted but quota-exempt.
The counter is not an allocator-heap or process-RSS ceiling.

R14's source assembly included an unrelated P2P test registration without that
work's source and failed configuration. R15 excludes only that registration
from its qualification snapshot. The workspace's separate P2P changes remain
untouched. R13's older passing observer does not qualify R15.

## Normal and sanitizer results

- All **104/104 unique registered normal checks** and both actual node
  `--version`/`--help` checks pass on OVH. This retains all earlier entries.
- There are 105 normal invocations. The first restore invocation stops at the
  extra 128-MiB disk guard with 103,202,816 bytes of strict margin remaining.
  The 40-decimal-GB budget and full recovery reserve remain intact. That
  result is preserved. After externally verifying and retiring obsolete
  owned lab binaries, the unchanged restore and all remaining cases pass.
- The minimum sampled strict margin in a passing normal invocation is
  **271,998,976 bytes**. SC5 passes; its minimum is 546,648,064 bytes.
- Final-source focused **ASan/UBSan passes 63/63 invocations**: 52 registered
  native cases, five explicit cold fault/concurrency cases and six
  clean/known-fault detector controls. It finishes at
  **2026-10-23 04:07:10.809814 UTC**.
- The first default-TSan clean control passes. Artifact preservation then
  rejects the new case-name prefix in the coordinator's allowlist. The native
  result and original preparation failure are retained. A separate
  continuation preserves that control without repeating it. The subsequently
  corrected SDK-link closure qualifies all 61 unique cases as described below.

The sanitizer matrices instrument a focused native closure with libp2p off.
ASan disables vptr checks and uses `GRPC_ASAN_SUPPRESSED` to preserve the
normal prebuilt SDK's public ABI; the new call-binding object remains
instrumented. Default TSan uses all five identified instrumented gRPC
archives, keeps the repaired C++ archive and disables ASLR only in its child.
Deadlock detection remains enabled. Other prebuilt dependencies, including
RocksDB, Zstd, Boost, libssh and OpenSSL, are explicit instrumentation
exclusions. No process-wide race-freedom claim follows from these matrices.

Key private records are `evidence/r15-normal-reconciliation.json`,
`evidence/r15-cli/control.json`,
`evidence/r15-exact-sanitizer-v2-asan/control.json` and
`evidence/r15-exact-sanitizer-v4-tsan/control.json` under the evidence root.
Build manifests retain compiler/configuration identities, commands, dependency
hashes and ELF hashes.
The full normal control is
`evidence/r15-all-native-build/evidence/r15-full-build-control.json`; its
168 linked-library hashes identify the dependency artifacts. The frozen
input list is `inputs/r15-bounded-grpc-refusal-source.json`, with the matching
source archive beside it. Each sanitizer has a separate preserved build
inventory and actual link inputs. The selected backup/database, genesis,
configuration and comparison inputs remain in the staging/preflight/runtime
proofs. Results apply to these identified builds and profiles.

## Physical envelope and qualification boundary

Fresh device inspection confirms two x86_64 vCPUs, ext4 and no swap. Observed
physical RAM is 4,000,022,528 bytes. Final runtime samples subtract the
22,528-byte excess from `MemAvailable` before comparing with the
4,000,000,000-byte target. This does not enlarge the acceptance limit.

Whole-server usage is conservatively calculated from the complete
42,949,672,960-byte raw disk minus root available space, then adds
`max(8 GiB, database bytes / 5)` of recovery reserve before comparison with
40,000,000,000 bytes. This includes boot partitions and filesystem overhead;
the smaller formatted-root capacity must not replace the complete device in
that calculation. The selected full-history database remains unchanged by
artifact retirement.

R15's full-history mainnet observer passes independent canonical agreement,
one-hour mixed reads, clean restart and owned-process interruption/recovery.
The final native/sanitizer matrices, paired component measurements and
controlled-chain create/reopen pass; their scope and measurements are recorded
below. Deterministic local
test signatures are authorized for the controlled chain only. No mainnet
transaction or operational key is involved.

The results separate attributed bytes, allocator requests, retained owners,
physical RSS, host availability, CPU, I/O and latency. Neutral and worse
measurements remain visible. Component savings do not establish whole-node savings.
Other issue-9 requirements remain independently open.

## Preserved TSan SDK-link failure

The first default-TSan matrix reaches **26 passing invocations and one failed
gRPC capacity invocation**. The exhausted-control call faults in the old SDK;
the client then reaches its five-second deadline and its cancellation assertion
fails. Resource guards remain intact. The later measurement/controlled-chain
sequence starts no phase and preserves the failure.

Archive inspection identifies a qualification-build configuration error:
`-UCMAKE_PROJECT_INCLUDE<path>` did not install the new include. The inherited
R12 overlay therefore remapped the C++ SDK target back to its unrepaired
instrumented base, even though the correct replacement archive had been built.
The actual gRPC test link line contains the original C++ archive and omits
`teleno-grpc/libgrpc++.a`. This failed execution cannot qualify R15's repaired
SDK or be relabeled as passing. Normal and ASan/UBSan results retain their
separate verified identities.

A build-only correction uses `-DCMAKE_PROJECT_INCLUDE=<path>` and verifies the
resolved final C++ SDK link input before compilation. Its first check rejects
CMake's correct relative path spelling; that preparation failure is preserved.
The corrected path comparison resolves both absolute and relative inputs.
At that checkpoint the new sanitizer binaries still required external
preservation and OVH execution. The corrected final result below supersedes
that pending status.
Only exact unchanged ELF hashes may retain earlier passing evidence; changed
or previously unexecuted cases must run against the corrected closure.
Private diagnosis: `evidence/r15-tsan-unrepaired-link-diagnosis.json`.

## Corrected final TSan result

The corrected build verifies the resolved link input before compilation and
preserves all 22 ELF outputs externally. Its archive SHA-256 is
`f0d16d298d3df5a413d0a16d7ae1d250fa8e5e3f56c28167ffa9123ae49bc183`.
Twenty ELF hashes are unchanged; the real gRPC server and Protobuf metadata
fixtures change. The gRPC capacity failure passes against the repaired SDK.

The final default-TSan matrix qualifies **61/61 unique invocations**: 40 new
runs and 21 retained passes whose complete final ELF bytes match exactly.
Changed and previously unexecuted cases all run on OVH. The matrix includes
working clean and intended-race controls; deadlock detection remains enabled.
The separate cold expiry sweep checks 3,658 allocation-failure ordinals and
completes in 615.25 seconds. All earlier failed links, preparation errors and
test results remain preserved. This is focused native/SDK instrumentation,
not whole-process race-freedom or whole-node qualification.

Private reconciliation: `evidence/r15-sanitizer-final-reconciliation.json`.
The subsequent paired measurements, controlled-chain and final observer
restart/recovery results below pass.

## Five equivalent pairs per workload

All **40 normal processes** complete on OVH: five alternating baseline/final
pairs for each of four workloads, without concurrent sanitizer or observer
execution. Inputs, policies, source identities and ELF hashes are preserved
in `inputs/r15-paired-input-identities.json` and
`evidence/r15-final-paired-processes/`.

| Workload | Baseline requests | R15 requests | Cumulative requested-byte change |
| --- | ---: | ---: | ---: |
| 64 admissions, 8,192-byte signatures | 142,265 | 141,497 | -50,078 B |
| Same admissions plus two retained query copies | 198,059 | 196,011 | -1,151,738 B |
| 256 identical in-process block notifications | 1,536 | 0 | Not instrumented for this event fixture |

The first two baselines are R10; the event baseline is the identified R12
profile fixture. Retained map entries, unique record capacities, tombstones
and final record-owner counts agree. Ordinary peak attribution increases by
40 bytes with the larger shared counter; no smaller attribution ceiling is
claimed from the eliminated copies.

| Workload | Baseline median process peak RSS | R15 median process peak RSS | Process CPU / wall medians, baseline to R15 |
| --- | ---: | ---: | --- |
| Admission | 18,522,112 B | 18,460,672 B | 0.02 / 0.03 s to 0.03 / 0.03 s |
| Admission plus queries | 19,828,736 B | 19,652,608 B | 0.04 / 0.04 s to 0.04 / 0.05 s |
| Block notification | 8,384,512 B | 7,892,992 B | 0.08 / 0.09 s to below displayed timer precision |
| HTTP argument and receipt fixture | 27,705,344 B | 19,787,776 B | 0.19 / 0.19 s to 0.02 / 0.03 s |

Native admission/query RSS is variable across pairs; these small median
differences do not establish physical savings. Their inner operation wall
medians are 27.410 to 26.835 ms and 37.687 to 38.377 ms, respectively.
The coarse process timer and sample variation prevent an attributable CPU
claim for these paths. Block notification dispatch falls from 89,972 to
2,285 microseconds and removes the measured copies, but its process RSS
comparison is not a whole-node or isolated-edit memory claim.
These short component processes have no separate steady-state interval;
their RSS comparison uses kernel process peaks. Steady RSS is measured in
the full-history observer window below.

The HTTP baseline is the identified pre-R7 R6 component. It sends the same
warm request, 262,144-byte argument and deterministic 4-KiB receipt through
localhost HTTP. All five R15 process peaks are lower: pair differences range
from -7,983,104 to -7,200,768 bytes; the difference of process medians is
**-7,917,568 bytes**. At service entry the additional attributed bytes fall
from 1,019,389 to 281,276; both versions return exactly to their own initial
credit after shutdown. These are different measurements. Multiple identified
source/dependency changes separate these binaries, so the full process delta
is not attributed solely to one copy edit or generalized to the whole node.
Supplemental allocator-request measurements remain separate from the normal
process RSS/CPU comparison.

Observed native filesystem input blocks are zero in these warm staged
processes. Output counts are unchanged: 24 blocks for admission/query and
eight for event/HTTP, in 512-byte units. No I/O saving is claimed. Minimum
sampled target-adjusted host `MemAvailable` is 3,401,041,920 B; minimum strict
disk margin, after the complete recovery reserve, is 676,958,208 B.

## Controlled workload preparation

The first R15 controlled invocation exits before creating state because its
unchanged fixture requires the immediate parent name
`r12-controlled-mixed-profile`. The initial coordinator supplied another
owned parent. Its exit, empty output, guard state and exact evidence archive
remain preserved under `evidence/r15-controlled-final/`. This is a fixture
path refusal, not an accepted transaction or a new application-memory result.

The separate continuation uses a fresh `R15ControlledV2` directory containing
the required guard-only parent name. The source and Linux binary are
unchanged. It runs creation and reopen with deterministic test-only signatures
inside the isolated network; no operational key, mainnet transaction or
producer is involved. The completed result follows.

## Exact controlled-chain result

The separate corrected-path run passes creation of **100 blocks and 12,800
locally signed test transactions**, with 8,852 concurrent native pending-query
cycles. It uses 32 payer accounts, the federated controlled-chain algorithm
and no-op transactions; VM-heavy contract throughput and mainnet production
are not established by this fixture. No network adapter or operational key
is present. Reopening replays 60 blocks from LIB 40 to the identical height,
block ID, state root and 100-record archive digest.

| Measurement | Exact R15 result |
| --- | ---: |
| Aggregate admission policy | 805,306,368 B (768 MiB) |
| Peak attributed policy bytes | 564,456,504 B |
| Aggregate refusals | 0 |
| Kernel process peak RSS | 705,712,128 B |
| Minimum target-adjusted host MemAvailable | 2,730,473,472 B |
| Minimum strict disk margin including reserve | 607,547,392 B |
| Peak allocation-owner table entries | 520,731 |
| Final tracked-node requested bytes | 24,190,176 B |

The difference between the quota and attribution peak is 240,849,864 bytes
of **policy headroom**, not measured free heap. The 503,962 final table owners
include retained fork/index work; a zero pending count alone would not prove
release. Reopen reduces attribution to 535,260 bytes and retains the expected
persistent/replay owners. Final-owner and expiry/LIB regressions separately
verify exact release at the appropriate lifetime boundary.

The controlled workload's ten-block progress samples rise to 497,045,423
accounted bytes at height 60, then remain between **497,033,693 and
497,071,649 bytes** through height 100 while the reversible window stays at
60 blocks. Together with the 564,456,504-byte peak and successful capacity
boundary/refusal tests, this supports the proposed **768-MiB ordinary policy
for the tested workload**. It is not an infinite-duration retention proof or
an all-process RAM cap. The observer/recovery run checks that setting with
the separate SDK, VM, database and allocator consumers included in physical
RSS. Private derivation: `evidence/r15-controlled-policy-selection.json`.

Creation takes 1,028.86 seconds with 1,528.67 user and 188.29 system CPU
seconds. Producer-call latency is 2.222 seconds p50, 3.179 seconds p95 and
3.260 seconds p99. Reopen takes 9.65 seconds with 8.73 user and 0.89 system
CPU seconds. Observed filesystem input/output counts are 0/1,508,912 and
40/472 blocks, respectively, in 512-byte units. This is one final resource
qualification, not a paired whole-node saving or throughput claim.

Private proof: `evidence/r15-controlled-final-v2/summary.json`; exact generated
state and evidence are externally verified before owned-copy retirement.
Archive SHA-256:
`26051671a8d2fe46accffebcd66c5ed4e92851bca0a52dd3a62707613cf45b3d`.
The earlier path refusal remains failed. The separate exact-node observer
qualification below also passes its restart/interruption recovery.

## Supplemental HTTP allocation-request counts

A separate glibc-only measurement interposer is compiled without execution
on the builder, preserved by hash and run only on OVH. It counts
`malloc`/`calloc`/`realloc` requests without inspecting their contents. Its
clean control observes exactly one request through each API, two frees and
32,668 requested bytes. Aligned and internal allocation APIs remain explicit
exclusions. Instrumented timing/RSS is not substituted for the normal-process
comparison above.

Five further alternating HTTP pairs preserve the same input/receipt workload
and exact application ELF hashes. Median captured malloc/calloc/realloc
calls are **22,595 to 22,307**, with cumulative requested bytes
**94,374,789 to 11,612,240**. All five cases retain correct input/output and
return exactly to their initial M1 credit. These cumulative byte counts are
not live heap, retained policy bytes or a physical-memory ceiling.
Private source, compiler, interposer/control hashes, raw samples and results
are in `evidence/r15-supplemental-allocation-build/` and
`evidence/r15-http-allocation-request-pairs/`.

R15's read-only native preflight passes, and its first exact full-history
mainnet observer starts at **2026-10-23 05:57:41.924475 UTC**. It catches up
with producer disabled, then completes the separate mixed-read and
restart/recovery gates below.

## Exact full-history observer, restart and recovery

The same R15 ELF and configuration complete four separately counted observer
starts against the existing full-history database. Configuration SHA-256 is
`c987f917fe4ad9a3877adbab8cc1feb4d0af0bbb73ae80cd2a6f7e05965c33f3`.
The profile uses the 768-MiB ordinary aggregate policy, one chain job,
128-MiB RocksDB cache, 8-MiB write buffers, a 64-MiB total write-buffer
budget, one background job, Zstd level 9 and a 65,536-byte dictionary.
JSON-RPC is local; gRPC and the producer are disabled. This identifies the
tested profile without changing a production configuration.

The full **3,600.632-second** mixed-read interval completes **3,600 successful
cycles, zero failed cycles and 31 successful independent comparisons**, with
zero unavailable-reference samples. Each cycle reads head, node status,
resource limits, pending transactions and an irreversible archive receipt.
The workload comparisons show head lag between zero and two blocks; local
LIB advances from 40,445,198 to 40,446,448. Native logs show three usable
connected peers, and four after recovery. The independent block IDs and
available receipt state roots agree at the sampled irreversible heights.
All 3,600 pending-query results are empty in this mainnet interval; the
separate nonempty controlled workload supplies transaction-pressure evidence.

The second start reopens cleanly and follows the canonical chain for more
than five minutes. The third verifies advancing head/LIB and native writes
before an explicitly recorded SIGKILL of only its owned test unit. The fourth
reopens the same database, follows head/LIB for more than five minutes and
stops cleanly. Its last independent proof is head **40,446,759**, LIB
**40,446,699**, with zero head lag. There are **four observer starts and
zero operational producer starts**; no two nodes share the database.

| Physical measurement | R15 observer result |
| --- | ---: |
| Maximum kernel/sample HWM across all four starts | 419,278,848 B |
| Maximum steady RSS in the one-hour window | 410,882,048 B |
| Median steady RSS, 359 recorded samples | 387,026,944 B |
| Minimum target-adjusted host MemAvailable | 2,921,375,744 B |
| Minimum strict disk margin after full recovery reserve | 538,800,128 B |
| Worst whole-server footprint including reserve | 39,461,199,872 B |
| Mixed-cycle latency p50 / p95 | 13.082 / 20.009 ms |
| Swap use and OOM events | 0 |

Every resource gate passes: steady RSS <=2 GiB, peak <=2.5 GiB,
target-adjusted host `MemAvailable` >=768 MiB, no swap/OOM, and the strict
40-decimal-GB budget with the entire 8-GiB recovery reserve intact. These
are final-workload absolute measurements, not a baseline-to-final whole-node
memory-saving claim.

| Cleanly stopped phase | Elapsed | User / system CPU | Filesystem input / output, 512-B blocks |
| --- | ---: | ---: | ---: |
| Catch-up and one-hour reads | 4,002.70 s | 513.841 / 12.357 s | 453,880 / 299,784 |
| Clean reopen | 321.55 s | 20.055 / 0.884 s | 72,200 / 89,384 |
| Interrupted-database recovery | 321.53 s | 18.486 / 0.620 s | 4,776 / 3,656 |

These CPU/I/O figures use native child rusage for the three cleanly stopped
phases. The deliberately killed third phase retains separate sampled CPU/I/O
in its interruption record and is excluded from those totals. Host memory
and disk guards include concurrent collector and OS consumption.

Private reconciliation: `evidence/r15-runtime-reconciliation-v3.json`.
The externally verified observer archive contains 165 files, SHA-256
`bf74a9c136bced75746aed4c93918b6b268b75aa963ad84c0ad91f36a7f56813`.
The final read-only stopped-server check confirms zero native node processes
and zero active owned test units. The full-history basedir contains 429 files,
24,244,805,648 logical bytes and 24,245,702,656 allocated bytes. The final
whole-server footprint plus reserve is **39,272,824,832 B**, leaving
727,175,168 B of strict margin. The database and original rollback assets
remain preserved.

## M1 completion decision

The [final owner inventory](../implementation/issue-9-m1-owned-memory-inventory.md)
and private `evidence/r15-m1-ownership-review.json` link eight ownership groups
to 46 registered-case references, each checked against its preserved JUnit
hash. They cover retained records/forks/indexes/tombstones, nonce/RC/count
reads, input and transaction-work leases, candidate/proposal copies, returned
values and HTTP/gRPC output/refusal controls, failure-atomic block/LIB and
persistent publication, cleanup, allocation failure and successful retry.
Final native/sanitizer matrices, paired measurements and the exact runtime
results satisfy the applicable M1 gates for this implementation.

Ordinary admission is bounded by the shared policy. HTTP fallback and gRPC
refusal controls have separate finite allowances. Instrumented native
canonical/recovery work, necessary startup controls and internal diagnostic
inspection remain explicitly counted but quota-exempt. Static/process-global
metadata, allocator retention, transport ingress/SDK queues, other RPC
services, VM and non-mempool database consumers retain their declared
boundaries. Their physical presence in these node measurements does not
complete the separate R1/P2/V1/S1/D1/Q1/H1 requirements or establish a universal
heap/RSS ceiling. M1 completion does not close issue 9.

The qualification leaves the isolated OVH observers stopped. The operational
Hetzner producer is never contacted or modified. No mainnet transaction,
operational signing, funds movement, commit, push, release, app submodule or
packaged-binary update is performed. Earlier failed attempts, deadlines,
counters and negative measurements retain their original results.

## Workspace boundary

The live workspace remains on `codex/lr2-memory-hardening`. All M1 runtime
source matches the frozen R15 archive. Separate preserved P2P work differs in
the transport source/header, its CMake test registration and the dependency
build script's listener-lifetime patch. Those changes are outside R15; direct
building of the mixed workspace is not the identified R15 artifact. The
source reconciliation records every hash and leaves those changes untouched.
No integration commit, push, release or app-package update is performed.
