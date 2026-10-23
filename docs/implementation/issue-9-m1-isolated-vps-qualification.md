# M1 isolated VPS qualification of frozen C9

Status: **closed unqualified at 2026-10-20 18:32:58.009720 UTC. Staging,
offline validation/held activation and observer readiness pass; mixed
observation stopped early and no clean reopen ran. M1 remains open.**

The additional 1-GiB disk-margin guard stopped the owned observer cleanly at
18:32:57 UTC, about 21.5 minutes after readiness. The final strict-budget
margin, including the required 8-GiB free recovery reserve, was still positive
at 998,899,712 bytes. This is an early-stop harness result, not a demonstrated
breach of the 40,000,000,000-byte acceptance budget. One observer start, one
stage and one activation were consumed; no producer started. The remote
terminal coordinator/observer reports are authoritative over earlier local
running snapshots. See the [terminal measurements](../validation/low-resource-lr2-m1-isolated-vps.md#terminal-observer-result).

**Operator amendment, 2026-10-20:** this new isolated OVH VPS is the exclusive
execution host for all future M1 tests, including development reproductions,
native/sanitizer suites and controls, fixtures, paired measurements and CLI
checks. Other authorized machines may prepare artifacts and compile without
running tests. My Passport retains generated evidence. This changes no active
deadline, counter, frozen executable or resource/reserve requirement. Keep
later test phases separate from this running sequence and leave the Hetzner
producer untouched.

The operator supplied a newly purchased, otherwise unused VPS on 2026-10-20
for the already authorized M1 tests. This separate run uses frozen C9 and
does not reopen the closed C9/C10 implementation attempt. The designated
operational producer remains outside scope.

## Fixed limits and identity

- Actual start: **2026-10-20 15:19:53.510265 UTC**.
- Deadline: **2026-10-20 23:19:53.510265 UTC**. Never extend it across turns.
- No new source candidates or candidate builds in this qualification run.
- At most two observer starts, including the deliberate recovery restart.
- No producer activation, signing keys, funds or mainnet transaction submission.
- Owned local evidence, artifacts and temporary data:
  `/Volumes/My Passport/teleno-m1-ovh-qualification-20261020-sgjo6v1a/`.
- Durable counters and stop decision: `control.json` in that private run.
- Isolated remote data: one owned directory on the new test VPS. Access
  bindings and credentials are kept outside this repository.

C9 source archive SHA-256:
`40c28493e7ff0c2985d85f2ce1a278c47e209e5e23d16af1bbe2641070eab9d5`.
The exact Linux executable SHA-256 is
`bea7856f367d008e6ab43a1b9ecb016cab1623197e60dd928b936150347d0aa8`.
It passes version/help checks on the new host without additional packages.
See the [C9 native report](../validation/low-resource-lr2-m1-nonce-read.md)
for the completed normal/sanitizer tests and dependency identities.

The host reports Ubuntu 26.04 LTS, x86_64, two CPUs, 4,000,022,528 physical
RAM bytes, ext4 and zero swap. The physical disk is 42,949,672,960 bytes;
the larger physical device does not increase the acceptance budget. Continue
the existing conservative whole-server calculation:

```text
required = physical device bytes - root f_bavail bytes
           + max(8 GiB, database bytes / 5)
margin = 40,000,000,000 - required
```

This counts OS/software, unavailable filesystem/device space, other partitions,
databases, WAL, logs and temporary work while retaining the free recovery
reserve. Do not credit spare physical capacity or reserved filesystem blocks.
Keep the original reference-VPS evidence distinct from this additional host.

## Selected immutable input

Use the complete backup
`20261016T201050Z-ms-1792181450480-files-380`: 383 payload files,
24,114,178,822 bytes, including 380 database files. Its previously verified
genesis-continuity lineage is an immutable input, not proof of C9 runtime
qualification. Refetching all four published metadata files currently matches
their pinned hashes. The manifest SHA-256 is
`228a95506dceeff4a35b42cbd9af1709eb66906cde6346d615ed87984318ab5e`;
the inventory SHA-256 is
`7a4c755fe30610281fe8059707b5e9b8d114f944faa52c9c3aa56e169b5c55c5`.
Use the independently selected mainnet chain identity already bound to that
lineage. Preserve the existing HTTPS/source-signature policy and disclose its
limits. Do not select a different `latest` while resuming.

## Finite sequence

1. Run startup preflight and direct-restore metadata preflight with the exact
   observer configuration. Require known binary, input and configuration hashes,
   available ports, no other database owner and the strict whole-server budget.
2. Permit one native single-copy stage (up to 90 minutes), with at most one
   explicit transfer resume if needed within the same deadline. Verify every
   downloaded object. Never create a second complete payload repository.
3. Permit one native offline validation/activation episode (up to 120 minutes)
   with the same pinned input, direct mode and `experimental-8gib` reserve
   profile. Preserve a failed attempt and recovery markers.
4. Start C9 deliberately as an observer with a provisional 768-MiB M1 policy,
   the measured 128-MiB shared block cache, 8-MiB per-family write buffers,
   64-MiB aggregate memtable threshold and one background job. Keep block
   execution verification enabled and RPC loopback-only. Allow up to 120
   minutes for catch-up/readiness, then at most 60 minutes of bounded mixed
   read-only RPC/head-following observation. This provisional policy is not a
   production default or a complete M1 admission claim.
5. If ready, stop cleanly and use the second observer start for one bounded
   reopen/recovery check (up to 30 minutes). Stop the test observer at the end;
   this VPS is a qualification environment, not an operational producer.

Sample host memory, independent process RSS/peak, CPU, I/O, disk, cgroup OOM
and swap counters throughout. Require steady RSS <=2 GiB, peak <=2.5 GiB,
concurrent host MemAvailable >=768 MiB, zero OOM/swap, and nonnegative strict
disk margin including the reserve. Stop only the owned test process on a
confirmed limit/correctness failure or deadline, retaining the database and
evidence. Do not automatically restart a failure.

Verify advancing head/LIB and independent mainnet agreement. Record RPC
success/failure and latency with the exact workload. Preserve all selected
block/receipt records and validate the reopened database. A short observer
run cannot establish the separate historical-execution, producer, long-duration
or complete M1 ownership/admission requirements. Keep every unproved issue-9
criterion open and update the completion matrix with the actual result.

## Resumed execution — 2026-10-20

The operator explicitly requested continuation on the additional test VPS while
leaving the operational producer untouched. The original start, deadline and
counters are unchanged. At the C9 preflight, all **287** checked runtime,
build, script and test files matched the frozen C9 source manifest. Subsequent
C11/C12 development is separate and is not deployed into this C9 sequence.

Startup preflight and direct metadata-only preflight both exit zero. The first
metadata command incorrectly supplied a staging-only `--backup-output` path;
its argument refusal is preserved. Removing that harness argument produces a
successful preflight without changing source, configuration, binary, policy or
selected backup. The inventory reports 383 files and 24,114,178,822 payload
bytes. Its manifest/inventory hashes match the independent pins. The source
uses required HTTPS with the existing unsigned-source policy; HTTPS/hash
agreement is not a source signature or historical execution proof.

Native staging began at **15:56:41 UTC** and completed at **16:22:44 UTC**
with all 383 selected files / 24,114,178,822 payload bytes verified. Its
minimum sampled strict disk margin was 2,382,008,320 bytes including the
recovery reserve. Offline validation/held activation began at **16:22:46 UTC**.
It completed at **16:46:18 UTC**, checking 46,458,363 records across nine
column families and 917,316 state objects. The restored state is height
40,236,770; the highest stored block is 40,236,830. Structural validation is
not full historical execution. The minimum sampled strict margin through
activation was 2,380,722,176 bytes, including the unchanged recovery reserve.
The first observer started at **16:46:22 UTC** and its exact native process
was verified live at **17:00:31 UTC**. Catch-up/readiness, full observation and
clean reopen remain unproved.
The reviewed finite coordinator
continues only after a successful guarded phase and matching input hashes.
It does not retry failed phases or restart a failed observer. The second
observer start is solely the planned clean reopen after a successful complete
observation. Durable remote phase counters complement the private local
`control.json`; the closed implementation attempts remain unchanged.

Every native phase has a two-vCPU cgroup, 3-GiB cgroup memory ceiling and zero
swap allowance. The 3-GiB cgroup includes reclaimable page cache and is distinct
from the **2.5-GiB native peak-RSS acceptance ceiling**. One-second samples and
kernel RSS high-water/rusage measurements record native RSS, host availability,
CPU, I/O, OOM/swap and the conservative disk calculation. A further **1-GiB
disk margin** triggers an early stop before spending the required reserve.
Sampled host/disk extrema are not continuous maxima. These guards do not prove
that application-owned admission covers every allocation.

The independent public mainnet RPC is reachable from the test VPS and returns
the expected chain ID. A local-Mac HTTP 403 is retained as a reference-access
limitation, not a native correctness failure. Runtime comparison will check
canonical block IDs at a shared irreversible height, receipt roots when both
are available, advancing head/LIB and a near-head readiness condition.

The mixed observer workload calls only `chain.get_head_info`,
`chain.get_resource_limits`, `node.get_status`,
`mempool.get_pending_transactions` and
`block_store.get_blocks_by_height`. It records bounded response identities,
counts and latency rather than retaining every complete response. It does
not exercise signed transaction admission or large execution receipts.
The recorded per-cycle RPC-group latency excludes that cycle's initial
head read; it is not end-to-end cycle latency. A separate bounded local
collector copies test evidence to My Passport and checks terminal file
hashes. It performs no server mutations or producer operations.

### Optional existing controlled-chain fixture on the physical host

The original M1 scope includes isolated native transaction/producer tests. If
the full observer sequence succeeds, all its test observers are stopped and
at least 32 minutes remain before the unchanged deadline, one existing C9
controlled-chain executable may run a single create/reopen pair in a new
owned fixture directory. Each process has at most 15 minutes, two vCPUs,
2.5-GiB cgroup memory and zero swap. It uses the same physical-host/disk
guards and an isolated network namespace. Do not launch it after a failed
observer sequence or while any test node owns the restored database.

This is the existing 100-block/12,800-no-operation-transaction federated
fixture, source SHA-256
`00a6b4bf262c599923d3cd81cb1b9ad65e7750e26a426c8f8e36305553ba3378`,
ELF SHA-256
`8c15159060c20a16aa0c5acb35cc24e4a0b1c7c437cdf7f4271cea52353e86ef`.
Its source rejects the mainnet chain identity, uses embedded synthetic test
keys without key files, creates its own genesis and has no network adapter.
No wallet, pool, mainnet transaction or operational producer is involved.

This adds no source candidate/build and no node-observer start; it does not
change the two-start observer ceiling or reopen any failed phase. Persist
separate fixture process counters before launch. A passing physical-host
component sample remains distinct from the full-history observer and does
not establish mainnet PoB, large VM execution or mixed simultaneous workloads.

## Conditions still required for M1

1. Complete the allocation/lifetime inventory and demonstrate coverage of all
   applicable nonce/RC/count reads, staging/encoding and escaping output paths.
   C8/C9 controls address earlier specific read/output gaps; they do not prove
   every native read, large receipt/log, mixed-output or gRPC transport shape.
2. Demonstrate coherent failure/refusal and correct retry across retained
   forks, block/LIB transitions and persistent publication. Existing focused
   tests cover specific boundaries and injected failures; canonical/recovery
   allocations remain counted but quota-exempt. The configured attributed
   limit is not an unconditional aggregate heap/RSS ceiling.
3. Select and justify the policy using representative Linux workloads,
   including concurrent transaction execution and retained outputs. The
   provisional 768-MiB policy and a read-only observer run are insufficient
   evidence for those workloads.
4. Complete the exact-candidate whole-node catch-up, mixed-load and clean
   reopen measurements on the physical 2-vCPU/4-decimal-GB host. Require steady
   RSS <=2 GiB, peak RSS <=2.5 GiB, concurrent MemAvailable >=768 MiB, no
   OOM/swap, and the strict 40-decimal-GB accounting with the recovery reserve.
5. Retain the normal and focused sanitizer passes with their instrumented
   boundaries; finish any applicable integrated ownership/concurrency checks
   not established by them. Do not substitute older binaries or component
   counters for final-node qualification. Paired measurements support only
   their identified workloads and do not currently demonstrate an attributable
   whole-node memory saving.

The new observation can add evidence to these conditions; successful completion
of its finite sequence alone does not close M1, Q1, other issue-9 rows or
qualification on the designated operational producer.
