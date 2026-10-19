# Experimental 8 GiB reserve on the reference VPS

Date: 2026-10-18. Status: **finite experimental qualification passed on the
reference VPS for the selected backup and recorded workloads, including the
operator-requested five hours, clean reopen and complete final archive
preservation. These measurements meet the subsequently amended issue-9 8-GiB
reserve/disk criterion for the selected input and workloads; remaining issue-9
gates are open and a universally safe minimum reserve is not established.**

The operator explicitly requested this [separate direct-VPS goal](../implementation/vps-8gib-reserve-test-goal.md).
At completion this was a separate experiment. The operator then explicitly
amends original issue 9 on 2026-10-18 to require an **8-GiB free recovery-reserve
floor (8,589,934,592 bytes)** with `max(8 GiB, database bytes / 5)`, replacing
the former 10-GiB acceptance requirement. The completed measurements now count
toward issue 9. The standard runtime default remains 10 GiB; the measured node
explicitly selects the 8-GiB CLI profile. A smaller allowance saves no physical
bytes. The earlier
[compaction experiment](compaction-headroom-experiment.md) remains component
evidence, not qualification of this final node or a minimum safe reserve.

## Final result

All 14 application cases, the final complete archive scan, the current-source
Linux build/63 original test reports, and actual VPS CLI/policy cases pass
their specified outcomes. The final source remains the same frozen 312-input
candidate. No history is pruned and no selected record is omitted.

| Final measured requirement | Result |
| --- | ---: |
| Whole-server peak, including OS, runtime, unavailable space and the full 8-GiB reserve | 39,352,967,168 bytes |
| Smallest remaining margin within 40 decimal GB | 647,032,832 bytes |
| Maximum native RSS across application cases | 745,668,608 bytes |
| Maximum measured steady observer RSS | 635,420,672 bytes |
| Minimum concurrent host MemAvailable | 2,760,302,592 bytes |
| OOM / swap | 0 / 0 |
| Main observation / clean reopen | 18,007.577 / 300.707 seconds |
| Synchronized reads during the main observation | 9,360 successful / 0 failed |
| Original archived records preserved byte-for-byte | 45,076,063 |
| Final archive records / newly added records | 45,153,063 / 77,000 |

RSS uses the larger observed native VmHWM or timed lifetime maximum. The worst
whole-server peak occurs in the real 1.39-GB compaction/retained-reader case.
The final archive scan completes in **1,765.567 seconds**, reads all current
records and preserves the exact **45,075,988-record** frozen prefix and all
**75 original tail records**; no tail root completion or other byte change is
needed. All 21 closed archive-control members are preserved and verified.
The final native head at **40,312,594** and its complete state root match the
stable irreversible legacy reference. Source, runtime libraries, configuration,
2-vCPU/4-decimal-GB/ext4 host geometry and whole-server accounting are freshly
rechecked after all application/probe owners close. The test node is left closed.

Main-observation native CPU is **3,252.18 seconds**, including catch-up and
shutdown. Mixed historical/current block RPC p95 and p99 histogram upper bounds
are **20 ms**; maximum observed latency is **60.178 ms**. Complete CPU and raw
I/O counters remain in the per-case ledger and latency summaries. The existing
equivalent-input restore comparison below records the small timing differences;
these are not causal RAM/CPU improvements from a reserve flag.

The profile reduces the accounting allowance by **2,147,483,648 bytes** and
saves **zero physical data bytes**. Its default remains 10 GiB with the retained
20%-of-database term. This result covers the selected full-history backup,
five-hour observer episode, measured compaction and bounded isolated federated
producer fixture. It does not qualify larger/arbitrary concurrent compactions,
future growth, historical VM replay, power loss, longer operation or an activated
mainnet producer. Original issue-9 and broader LR2 requirements remain open.

Final evidence includes `final-selected-archive-retention-summary.json`,
`final-vps-host-and-runtime-identity.json`, `completed-application-resource-ledger.json`,
`final-head-legacy-raw-review.json` and the requirement-by-requirement
`final-experimental-qualification-audit.json`. The dated checkpoints below retain
their original pending states and failures as historical evidence; they do not
override this final result or the acceptance table.

## Operator duration amendment

At the operator's explicit request on 2026-10-18, this experiment's observation
is reduced from 24 hours to **five hours (18,000 seconds)**. The same native
process continues; its accumulated resource, synchronization and mixed-read
measurements are preserved. Source, executable, configuration, disk reserve,
resource limits and data-retention requirements are unchanged.

The amendment is installed and independently observed active on the actual
VPS at **10:23 UTC**. The native process is still the original launch. A bounded
controller will send an identity-checked graceful interrupt after at least
18,000 native-process seconds, retain the original harness's raw results,
independently check the amended duration and every resource/workload condition,
and then execute the original 300-second reopen workload after its TCP cooldown.
The new reopen diagnostic changes only its prerequisite to the independently
verified five-hour result; the native executable and its configuration do not
change. A live mapped-library/configuration/process proof is captured during
that reopen. Complete original-archive retention and final canonical/source/
resource review remain required.

The running original harness loaded its 86,400-second duration at startup.
It is not patched in memory and its files/results are not rewritten. Its early
duration refusal will therefore remain visible after the requested graceful
stop. Only an independently audited five-hour result may qualify this amended
experiment; a resource failure or another refusal cannot be relabeled as a
pass. The original 24-hour run remains incomplete. Earlier references below
to a required 24-hour run describe the superseded experimental plan, not the
current duration or a completed result. Original issue-9/LR2 requirements remain
open and are not shortened by this separate experiment.

Owned evidence includes `five-hour-plan.json`,
`five-hour-amendment-install-receipt.json`, and `five-hour-controller.py`.
Its synthetic audit accepts the valid scoped resource fixture and rejects nine
negative controls; those controls are not completed native-run evidence.

Before the requested deadline, actual closed-service inspection exposes a
private controller issue: systemd may collect a successful transient service
and return an empty invocation identifier. Revision 2 accepts that observation
only for a closed collected unit, while still rejecting a different nonempty
identifier. Only the private controller is replaced; the original native
PID/start identity is verified unchanged across the replacement. Both revisions
are preserved. The actual five-hour observation remains active at **10:35 UTC**.
Final helpers now require the explicit amended evidence, retain the original
24-hour refusal and all raw measurements, and check the independent reference
containers both before and after canonical RPCs. A live invocation correctly
refuses final qualification before the required terminal result exists. None
of those diagnostic checks credits completed duration, archive retention or
final whole-node qualification in advance.

## Completed five-hour observation; reopen and final retention pending

The original native process completes **18,007.577 seconds** and exits **0**
after the identity-checked programmed interrupt. Independent inspection of its
raw measurements verifies every resource/read/head condition, **9,360 steady
reads with zero failures**, peak native RSS **635,420,672 bytes**, minimum host
`MemAvailable` **2,769,223,680 bytes**, minimum strict whole-server margin
**1,833,799,680 bytes**, and no OOM or swap. Catch-up occurs after
**2,237.276 seconds**, from the already advanced database. Maximum observed DB
and WAL allocations are **24,288,677,888** and **82,173,952 bytes**; the WAL
setting is a soft flush trigger, not a 64-MiB ceiling. Native CPU is
**3,252.18 seconds** (3,162.55 user + 89.63 system), including catch-up and
shutdown. GNU time filesystem input/output counters are **1,808,024 /
1,652,720**, retained in their emitted units.

The original 86,400-second harness retains its sole duration refusal; its raw
result is not rewritten as a 24-hour pass. The separately authorized five-hour
audit passes, and both original service process groups are independently
confirmed closed. Evidence: `five-hour-day-closed-preliminary-review.json`,
the original result hash
`e84fa9c89782b058eead2e38e7ba95e745f37f559db8ac62a1bf3d4f42dae9a0`,
and the durable 27-check waiter log/summary. At **12:17 UTC**, the corrected
controller is executing the original 300-second reopen on the actual VPS;
its fresh native process and complete live mapped-library proof are observed.
That phase and the final all-original-record scan/reference/resource review are
not yet credited as complete at that checkpoint.

The subsequent reopen completes **300.707 seconds**, exits **0**, catches up
after **10.647 seconds** and completes **168 steady reads without failures**.
Its complete counters retain two expected startup connection failures. Peak
RSS is **115,625,984 bytes** including the timed lifetime maximum; minimum
host MemAvailable is **3,315,453,952 bytes**, strict margin **1,867,513,856
bytes**, and no OOM/swap occurs. All ten closed reopen artifacts and all 21
original/amended/superseded-controller artifacts are archived with every member
verified. See `vps8-five-hour-summary.json` and `five-hour-closed-proof.json`.

The final captured native height **40,312,594**, canonical ID and complete state
root independently match the preserved legacy reference. Its canonical
successor at **40,312,595** contains the identical previous-state root; reference
LIB **40,312,601** makes that comparison irreversible. Actual decoded responses,
mainnet chain identity, timestamps, parent linkage and all three reference
container images before/after calls are independently inspected in
`final-head-legacy-raw-review.json`. The complete final selected-record scan
starts only after the observer processes close. At **12:29 UTC** its exact
existing process is verified live after reading **7,340,032 archive records**;
that scan and whole-goal review remain pending.

## Final candidate and policy

The frozen candidate includes 312 native inputs on
`codex/lr2-memory-hardening`, based on commit
`d3cfb94455da4a472f651eb4239abbc289e74225` plus preserved uncommitted work.
Source manifest SHA-256:
`6456879f2162baf57d6b932054bf92e19569fb8e47c89d109fbd35e11a964ec5`.
Linux native executable SHA-256:
`f086e2d71c04cc0922ccd497a2cb1c63d5265bc1d3190b2e17e1156b5a7d10f4`.
The actual VPS reports
`teleno_node 1.3.0-dev.0+d3cfb94455da4a472f651eb4239abbc289e74225`.
The export uses the declared base-commit override; its exact source manifest
and executable hash, rather than the version string alone, identify the changes.
This is not a native release, commit, integration or packaged app update.

The pre-closure identity audit rehashes all **312 inputs** in both the current
local tree and the isolated Linux build export, the source archive and exact
native executable. It also records the compiler, CMake cache/compile/link inputs,
dependency-source archives and **168 resolved link archives**. The compiler is
Ubuntu GCC **13.3.0** on the separate build machine; execution qualification
remains on the reference VPS. See
`preclosure-source-build-dependency-audit.json`. This read-only inspection
performs no build or native-test rerun and does not credit the remaining
whole-node gates.

Standalone direct actions accept
`--backup-restore-reserve-profile experimental-8gib`. The common reserve
calculation is `max(8 GiB, database bytes / 5)` for that explicit experiment and
`max(10 GiB, database bytes / 5)` for the default `standard` profile. The selected
profile flows through planning, staging/resume, offline validation and activation.
Experimental attempt descriptors pin it and reject a profile change; old
standard descriptors retain their previous format. Ordinary/repository/admin
defaults and consensus/wire behavior are not changed by the setting.

## Reference host and input

Read-only identity and runtime checks confirm the designated replacement VPS:
Ubuntu 26.04.1 LTS/x86_64/ext4, 2 vCPUs, 4,000,034,816 bytes RAM, no swap, a
39,956,590,592-byte root filesystem and a 40,961,572,864-byte physical device.
The permitted whole-server budget remains **40,000,000,000 bytes**. Do not
credit the extra physical capacity or filesystem-reserved blocks.

For each observed peak, account:

```text
required = raw device bytes - root f_bavail bytes + held-free reserve
margin = 40,000,000,000 - required
experimental held-free reserve = 8,589,934,592 bytes
```

This includes OS/software, all databases, WAL, logs, temporary work and
unavailable device/filesystem space. Additional workspace already allocated
is counted in occupied space; the complete reserve must still remain free.
Runtime guards also require node peak RSS <=2.5 GiB, steady RSS <=2 GiB,
concurrent host `MemAvailable` >=768 MiB, and no OOM or swap use.

The selected published input is
`20261016T201050Z-ms-1792181450480-files-380`: **383 payload files**,
**24,114,178,822 logical bytes**, including **24,113,527,301 database bytes**.
Public manifest SHA-256:
`228a95506dceeff4a35b42cbd9af1709eb66906cde6346d615ed87984318ab5e`;
inventory SHA-256:
`7a4c755fe30610281fe8059707b5e9b8d114f944faa52c9c3aa56e169b5c55c5`.
Its independently verified published metadata and database comparison are
retained. Native/public metadata differ only because native promotion replaces
`config.yml` with the reviewed public observer configuration: all **380 DB
entries are identical**. The exact published metadata, not the unpromoted local
digests, is pinned. The initial mismatch is correctly refused before deployment.
HTTPS and expected-chain checks remain active. Source signatures remain absent;
independent operator provenance and matching hashes do not create a signature.

The complete-genesis input proof is independently rebound to this exact
selected backup in `selected-backup-genesis-continuity-lineage.json`. The
existing structural audit follows **40,236,770 consecutive canonical blocks
and receipts** to the zero SHA-256 genesis parent and verifies header IDs for
all **45,076,063 archive records**. Both subsequent complete raw comparisons
retain the identical block-family count and length-delimited digest
`c5953863ebfa9909adda21b26ace7e88bc1d3c63766259d7990a42205a54856b`.
All 380 database path/size/SHA-256 entries are identical between the verified
native backup, promoted publication and actual restore's pinned metadata.
Checkpoint chain/state identities and the actual final-native restore result
also agree. This transfers immutable input-content evidence, not qualification
of an older executable or a historical execution/signature audit.

The native offline validator checks every record but its ancestry walk stops
at the persisted state revision; its count alone does not prove genesis
continuity. The earlier independent audit supplies that missing coverage.
The current VPS compaction/retention probes encode digest lengths in big-endian
order, whereas the source comparisons use little-endian order; their differing
digests are not directly compared. The exact payload lineage and sealed
pre-observer baseline bind those proofs. Complete preservation of the selected
records through the final observer shutdown still requires the final archive
scan and remains pending.

## Preservation and completed checks

The previous B3 observer is already stopped after its recorded budget failure.
Before replacement, all 434 entries of its latest node tree are inventoried;
374 existing off-host CAS members totaling **24,062,289,819 bytes** are freshly
hashed. A complete 60-entry delta preserves 160,077,880 logical bytes in a
97,764,998-byte verified archive. Exact prior executable/config/source-selection
inputs receive their own verified four-member archive. The unchanged source
metadata and absence of live source owners are checked.

Replacement claims the original storage gate, verifies no configured node
service, rehashes every source entry against that reconstruction set, checks
ownership again, and preserves original file metadata before retiring only this
owned stopped test node. It recovers **24,223,096,832 available bytes**, leaving
**35,813,335,040 bytes available**. The gate remains. Unrelated services and
data, original backups, build inputs and producer authorities are preserved.

The four focused tests pass on both the builder and the actual VPS:

- Default/experimental estimation, retained 20% term and profile diagnostics.
- Streaming stage/resume, refusal below the selected budget and descriptor pinning.
- Offline validation/activation, policy mismatch/refusal and observer holds.
- Signed-fixture CLI workflow, explicit profile on every action and invalid input.

Actual VPS version/help and pinned-public-source metadata preflight also pass
their expected outcomes. That first preflight exits **2** because the previous
database still occupies the server: no payload is downloaded or database opened.
After preservation/replacement the full-restore preflight exits **0**. Capacity
refusal is retained as evidence, not recategorized as a completed restore.

The exact final-source full native build and all **63 distinct current-source
CTest entries** pass on the builder. Fast component cgroup measurements are not
whole-node RSS qualification. VPS component runs have zero swap, no new OOM and
minimum sampled host `MemAvailable` of **3,328,954,368 bytes**. Their complete
fixtures and controls are archived and every member verified before retiring
closed duplicate fixtures. No component result substitutes for the whole node.

## Actual full-restore checkpoint

| Phase | Exit | Seconds | Timed native peak RSS bytes | Whole-server peak including 8 GiB bytes |
| --- | ---: | ---: | ---: | ---: |
| Native preflight | 0 | 0.418 | 30,838,784 | 13,738,299,392 |
| Controlled stage interruption | 130 | 13.222 | 32,550,912 | 14,862,680,064 |
| Same-source/profile resume | 0 | 394.161 | 28,913,664 | 37,854,965,760 |
| Complete validation/held activation/reopen | 0 | 2,485.981 | 54,222,848 | 37,915,475,968 |

All **383 files** are present; explicit resume reuses the 19 complete prefix
files. Read-only offline validation checks **46,458,363 records** across all
nine column families, including **917,316 state objects**, state revision
**40,236,770**, and highest stored height **40,236,830**. Held activation and
reopen pass. The largest observed restore allocation plus the complete reserve
leaves **2,084,524,032 bytes** within the strict whole-server budget. Minimum
sampled host `MemAvailable` during validation is **3,391,688,704 bytes**;
no OOM or swap occurs. This is structural/checksum/state/ancestry validation
of the trusted backup, not historical execution of every block.

The final native node completes its initial **900-second observer** smoke/catch-up
and planned clean stop, then starts the clean-restart episode. Its executable, owner/start ticks, cgroup,
runtime configuration, absent producer key and observer hold are verified. All
five actually mapped dynamic libraries match the recorded runtime inputs.
An early stable canonical sample at height **40,237,506** matches the independent
legacy block ID and complete state root. This is an early catch-up sample, not
proof that the node is near tip or has completed sustained qualification.
A second in-progress sample at **40,295,164**, 58,334 heights above the selected
backup's maximum, also matches the independently irreversible canonical ID and
successor-header state root. At 07:39 UTC the reference head/LIB are
**40,307,104 / 40,307,044**. All three reference containers retain their expected
images and are running. Raw block responses and decoded roots are independently
reviewed; this sample still does not replace the required final post-day check.
Clean restart and the complete controlled interruption/recovery case pass.
Sustained operation remains open. The running 24-hour
observation must complete its actual duration and planned shutdown, demonstrate
head progress and successful mixed reads, and keep every resource guard intact.
Bounds on larger/deeper compactions, retained readers and longer growth horizons
are not inferred from an uneventful observation.

The supervisor records half-second whole-device/kernel/hold/key guards,
five-second SST/WAL/allocation and owned open-deleted-file traces, minute samples
and ten-second bounded mixed reads of chain/head/resources, recent/historical
blocks and receipts, and pending transactions. Response digests/counts keep
telemetry bounded. CPU, I/O and every RPC latency remain part of the evidence.
Mainnet signing/production activation is outside this experiment.

## Equivalent-backup restore comparison

The original standard-profile restore and this final candidate use the exact
same published manifest/inventory, all 383 payload files, runtime configuration,
hardware profile and complete validation oracle. The original and final source
and executable identities remain separate. Their observed restore workloads
are equivalent; initial whole-host software/test artifacts and native versions
differ, so these single observations do not establish a causal performance gain
from selecting a smaller reserve.

| Measurement | Original standard profile | Final experimental profile |
| --- | ---: | ---: |
| Resume elapsed seconds | 383.268 | 394.161 |
| Resume native CPU seconds, user + system | 358.23 | 368.69 |
| Resume timed peak RSS bytes | 29,204,480 | 28,913,664 |
| Validation/activation elapsed seconds | 2,478.100 | 2,485.981 |
| Validation/activation native CPU seconds, user + system | 2,325.66 | 2,331.24 |
| Validation/activation timed peak RSS bytes | 53,493,760 | 54,222,848 |
| Validation/activation GNU time filesystem input counter | 350,212,080 | 348,871,920 |
| Validation/activation GNU time filesystem output counter | 240 | 240 |
| Whole-server restore peak with selected reserve bytes | 39,760,650,240 | 37,915,475,968 |

Validation/activation takes 0.32% more elapsed time and 0.24% more native CPU
in this observation; resume takes 2.84% more elapsed time and 2.92% more CPU.
GNU time filesystem counters are reported as emitted, without an undocumented
conversion into physical bytes. Both observations have zero swaps and identical
data/state/ancestry validation results. Runtime CPU/I/O and mixed-read latencies
are reported separately in the completed observation and per-case ledger.

The allowance is **2,147,483,648 bytes smaller**; this saves **zero physical
data bytes**. The observed accounting reduction is only 1,845,174,272 bytes
because occupied/unavailable whole-server space is 302,309,376 bytes higher in
the new run. This includes differing host/test footprints and must not be
presented as a database compression result. Neither the small RSS difference
nor these timing samples is a qualified RAM improvement from the profile.

## Acceptance checkpoint

| Requirement | Current authoritative evidence | Status |
| --- | --- | --- |
| Exact source, binary and dependencies | 312-input manifest, frozen build/links, three deployed executable hashes and all five resolved dynamic-library hashes | Recorded |
| Default/experimental consistency and pinning | Four focused tests on builder and actual VPS; all 63 distinct original Linux JUnit reports independently inspected | Pass for the scoped implementation |
| Preserve existing data/services | Complete latest B3 reconstruction set and verified controlled retirement; isolated owned test paths | Pass for completed actions |
| Complete backup restore, interruption/resume, validation/reopen | Exact published pins and all four successful final-native VPS phases | Pass |
| Larger real compaction with retained reader/checkpoint | Native job outputs 1,390,521,101 bytes; all 46,458,363 current/checkpoint/reopened records and SST checksums pass | Pass for this actual case |
| Isolated producer create/reopen | 100 custom-chain blocks, 12,800 signed no-operation transactions; replay/head/root/raw archive preserved | Pass for this bounded isolated case |
| Observer catch-up, mixed reads, clean and abrupt recovery | Short cases pass; 18,007.577-second day catches up in 2,237.276 seconds and completes 9,360 steady reads without failures; initial near-tip canonical ID/root matches legacy | Pass for the amended five-hour observation |
| Operator-requested five hours and post-observation reopen | Five-hour raw measurements and clean native exit independently pass; original 24-hour refusal retained; 300.707-second reopen and live mapped-library identity pass | Pass for the amended duration and reopen |
| CPU/I/O/latency and whole-node resource review | All 14 application cases and final archive scan meet the limits; exact counters/latency histograms and equivalent-input restore comparison reviewed | Pass for the recorded finite workloads |
| Independent stable canonical sample and final review | Final height 40,312,594 canonical ID/full root matches the irreversible legacy reference; source/host/library identities rechecked | Pass |
| Preserve every original archived block/receipt through the observer run | Complete final 45,153,063-record scan preserves the exact frozen prefix and all 75 original tail records; 77,000 higher records added | Pass; all 45,076,063 selected records unchanged |

The exact original JUnit files, not merely a summary flag, cover each registered
test name. Recovering the four original focused XML reports repeats no tests.
Earlier monitor/import failures retain their original evidence and are not
counted as passes. This checkpoint cannot complete the goal.

## Real-data compaction on the actual VPS

A separate private diagnostic links the same frozen native storage libraries.
Its executable SHA-256 is
`bb8ad80bd081677233dbefa93132d01e2a1fcab3fee6eec56e33f140e850b6c9`;
the exact source, compile/link commands, library hashes and pilot evidence are
preserved. The pilot passes data/checkpoint/reopen checks but is small and
synthetic. It does not qualify the real case.

The actual VPS read-only plan selects **21 contiguous level-6 blocks SSTs**,
**1,389,104,671 input bytes**, for a manual level-6 job using the native
compression policy. The planned additional allowance is **1,801,143,333 bytes**;
the contemporaneous strict budget margin above the held-free reserve is
**2,082,353,152 bytes**. The native compaction job returns `OK` after
**1,245.788 seconds**, with **1,390,521,101 output bytes**, exceeding 1 GiB.
All **2,603,865 input records** remain in the output. This is 1,416,430 more
compressed file bytes than the selected input; no compression saving is
attributed to this same-policy rewrite. The complete current-data comparison
passes: **46,458,363 records** in all nine families have exactly the same counts
and raw key/value digests before and after compaction. This includes all
**45,076,063 archived block/receipt records** and **917,316 state objects**.
SST checksums, retained-checkpoint comparisons and ordinary read-only reopen
also pass for all nine families. The whole case completes in **4,118.362 s**,
including all integrity scans. Native timed peak RSS is **143,949,824 bytes**;
minimum host `MemAvailable` is **3,264,700,416 bytes**, without OOM or swap.
Maximum additional unique DB/checkpoint allocation is **1,424,293,888 bytes**.
The maximum whole-server requirement including the full 8-GiB reserve is
**39,352,967,168 bytes**, leaving **647,032,832 bytes** within the strict budget.
The retained checkpoint is retired only after the all-record/reopen oracles pass.
This is one measured real-data compaction case, not a universal workspace bound
or whole-node memory result. Complete controls are archived with all 24 members
verified before the next workflow phase.

The completed job's conservative native-process accounting bounds are:

| Timed compaction region | Lower bound | Upper bound |
| --- | ---: | ---: |
| Kernel-accounted CPU seconds, user + system | 1,226.01 | 1,250.57 |
| Kernel `read_bytes` | 1,326,161,920 | 1,568,563,200 |
| Kernel `write_bytes` | 1,376,485,376 | 1,391,046,656 |

The bounds use bracketing five-second observations and verified process/clock
alignment, rather than treating a sampled counter as an exact boundary. They
include any background work in the same native process. The 1,245.788-second
job therefore consumes approximately one vCPU; output throughput is about
1.12 decimal MB/s with the selected native compression policy. Whole-probe
CPU/I/O totals additionally include the integrity scans. The independent
archive-retention reader starts after the timed compaction job; its concurrent
load affects the subsequent integrity scans and whole-server measurements.

The diagnostic computes streaming checksums/counts of every key/value in all
nine families, holds an iterator and a verified same-filesystem hard-link
checkpoint across compaction, compares every current/checkpoint record, checks
SST checksums, and closes/reopens the database before retiring the checkpoint.
Native open refreshes ordinary layout metadata before the baseline digest;
this does not change blocks, receipts or consensus state. Any failure preserves
the database/checkpoint. Whole-server storage remains the primary disk measure;
additional five-second file allocation samples deduplicate hard-linked inodes
and include owned open-deleted files.

The job runs in a bounded transient Linux service, with no automatic restart,
2-CPU quota, 2.5-GiB cgroup memory maximum, zero cgroup swap, a six-hour workload
deadline and quarter-second host/RSS/disk/reserve/OOM guards. Its processes
inherit that service's cgroup; they are not moved into sibling scopes. This is
an isolated qualification job, not a change to an existing node service.

The independently bounded VPS workflow has completed that exact compaction
invocation, both isolated producer phases, the 900-second observer smoke, clean
restart and the complete controlled interruption/recovery case. It is now
running the full 86,400-second mixed-read observation; post-day reopen follows.
Each native stage has its own finite service/supervisor deadline and resource
limits; the workflow has no automatic retries. Recorded TCP cooldowns preserve
the unchanged bind preflight. The 24-hour stage starts at approximately
07:13 UTC on 2026-10-18; its completed duration is not credited in advance.

The long observation additionally requires catch-up within its declared
two-hour experimental window, near-tip state at completion and bounded
synchronization/RPC staleness after catch-up. A lost guard, early exit, missing
oracle or failed phase stops the sequence and preserves all output for review.
Final review and independent canonical-reference evidence remain required;
dispatching a service or writing a success manifest does not complete the goal.

## Whole-run archive retention oracle

An additional current-library diagnostic reads a hard-link clone of
the compaction case's retained checkpoint. Its initial full raw archive digest
must exactly equal the already sealed pre-compaction `blocks` family digest.
The clone adds only bounded copied metadata and directory entries; it does not
copy the 24-GB database. Its separate finite service uses at most one CPU,
1 GiB of cgroup memory and no swap, with half-second whole-host guards. This
read runs concurrently with the compaction integrity scans and is
included in whole-server accounting; it is not an isolated timing benchmark.

The verifier fingerprints every archived record through the backup's committed
state revision **40,236,770**. It separately records every original tail key up
to highest stored height **40,236,830**. After the observer is stopped, a full
scan must preserve the historical raw record count/digest and every original
tail block/receipt field. Only filling an originally missing tail receipt state
root is allowed; an existing root may not change. New higher blocks are allowed.
This checks all original archived records rather than inferring retention from
sampled historical RPCs. The final scan additionally holds the actual native
storage gate and verifies that no native writer is running.

The complete baseline passes in **1,815.390 s**, matching the sealed raw archive
digest for all **45,076,063 records**. It records **45,075,988 frozen records**
and **75 original tail records**. Timed peak reader RSS is **38,785,024 bytes**.
Its owned hard-link clone is retired after validation and verification of the
other owned copies; copied metadata and all 13 closed control members are
archived and every member verified. No extra checkpoint remains pinned.

The private executable SHA-256 is
`263bb8ca59b2be8cdeb088e8ab7bc5b0aa6080bd11eabb67fa4f2453bc717185`.
Its controlled positive case accepts new higher blocks and completion of an
empty root. Five negative controls reject missing/changed historical bytes,
changed tail fields, rewriting an existing root and a missing original tail.
Source, link commands/libraries and every fixture member are preserved. Earlier
private-checker typed-JSON compilation and relative-link-path failures are
preserved; they do not alter or invalidate the frozen native product binary.
The final actual-data comparison remains required; the passing baseline and
self-tests alone do not establish retention through the observer run.

## Isolated producer on the actual VPS

The same final native libraries produce **100 blocks with 128 signed
no-operation transactions each**, **12,800 total**, using 32 synthetic payer
accounts, the real controller/mempool/producer/storage and a custom federated
genesis. Network adapters and key files are both zero. This is distinct from
operational mainnet production, PoB construction and contract-heavy workloads.

| Measurement | Create/produce | Ordinary reopen/replay |
| --- | ---: | ---: |
| Elapsed seconds | 165.933 | 12.725 |
| Native CPU seconds, user + system | 309.41 | 23.57 |
| Timed native peak RSS bytes | 745,459,712 | 605,413,376 |
| Minimum host `MemAvailable` bytes | 2,760,302,592 | 2,881,675,264 |
| Whole-server peak including 8-GiB reserve bytes | 38,055,694,336 | 38,029,172,736 |
| GNU time filesystem input counter | 16 | 0 |
| GNU time filesystem output counter | 1,503,448 | 1,816 |

Maximum additional fixture allocation is **86,794,240 bytes**. Native production
latency per 128-transaction block is p50 **315.760 ms**, p95 **584.047 ms** and
p99 **664.008 ms**; these timings exclude the preceding transaction-admission
work. Reopen starts at committed height 40, replays 60 blocks and restores height
100 with the same head ID, state root and raw archive digest. Every block,
receipt and transaction receipt remains present. Both phases exit zero with
all guards intact and no OOM/swap. All 48 fixture/control members are archived
and verified. These component measurements do not establish integrated
operational-producer readiness.

## Initial whole-observer episode

The final `teleno_node` completes **900.605 seconds** and its planned clean
shutdown, with the original complete history present and block production
disabled. Head advances **40,236,951 -> 40,252,374**. Independent kernel peak
RSS is **347,844,608 bytes**, minimum host `MemAvailable` **3,118,170,112 bytes**,
and the whole-server peak including the complete reserve is
**38,061,752,320 bytes**. There is no OOM or swap. All 15 logged RSS observations
agree with the kernel within 946,176 bytes; none fails the declared tolerance.

The episode records **534 successful** and **two failed** read calls. Historical/
recent block-and-receipt, irreversible block-and-receipt, resource-limit and
mempool reads each have 89 successful calls and zero failures. The two failures
are in chain-ID/head calls. The measured p95 latency bucket upper limits are
20 ms for mixed historical/recent blocks/receipts and at most 10 ms for the
other methods; the mixed-read maximum is 40.522 ms. These are histogram bounds,
not exact percentile samples. Every call, including drained futures, is counted.

Native CPU is **880.96 seconds** across user/system time; GNU time filesystem
input/output counters are **697,992 / 405,144**. Sampled DB allocation peaks at
**24,202,256,384 bytes**, WAL at **75,141,120 bytes**, and owned open-deleted
allocation remains zero. The WAL trigger is soft and is not misreported as a
64-MiB hard limit. All ten closed episode artifacts are archived and verified.

After that initial episode the node was still catching up; no near-tip or
sustained synchronization result or completed day was credited. The completed day, post-day reopen, final
original-archive verification and final canonical-reference review remain
required.

## Clean restart and controlled interruption/recovery

All three completed episodes use the same final executable and restored database.
Their native processes and service units are verified closed. Every member of
the ten-file restart archive and both eleven-file interruption/recovery archives
is verified.

| Measurement | Clean restart | Controlled abrupt stop | Recovery/reopen |
| --- | ---: | ---: | ---: |
| Completed observation seconds | 301.293 | 300.791 | 301.397 |
| Expected and observed native exit | 0 | 137 | 0 |
| First head height | 40,252,427 | 40,257,650 | 40,262,990 |
| Last head height | 40,257,401 | 40,262,758 | 40,268,199 |
| Kernel native peak RSS bytes | 299,884,544 | 304,177,152 | 302,538,752 |
| Minimum host `MemAvailable` bytes | 3,194,441,728 | 3,177,730,048 | 3,172,044,800 |
| Whole-server peak including 8-GiB reserve bytes | 38,053,023,744 | 38,042,226,688 | 38,059,630,592 |
| Native CPU seconds, user + system | 294.14 | 294.00 | 298.41 |

All resource guards, observer holds and producer-key absence pass, without OOM
or swap. The abrupt episode records irreversible height **40,262,698** seven
seconds before its planned process interruption. The reopened node returns
the identical block ID and canonical complete block/receipt record digest for
that height. The captured record is independently compared with the archived
pre-interruption evidence. The recovery episode completes its full duration
and clean stop, and the closed archive contains exactly the same recovery proof
as the initial live readback. This is one finite interruption/recovery case,
distinct from the pending completed day and final whole-archive comparison.

## Current 24-hour observation

The actual final native process is running in its dedicated bounded VPS service
with an explicit **86,400-second** duration. An independent live check matches
the executable SHA, all five actually mapped dynamic-library hashes and their
mapping device/inode identities, the runtime configuration and source manifest.
It also verifies the unchanged 2-vCPU/4-decimal-GB/ext4 host, zero host swap,
2.5-GiB service memory limit, zero service swap, observer hold and absent
producer key. This is liveness/provenance evidence, not a completed observation.

At 07:29 UTC the same live native PID/start identity has completed the first
**941.333 seconds** and advanced **40,268,418 -> 40,284,403**. It remains in
catch-up. The observed kernel peak RSS is **444,514,304 bytes**, the first
post-900-second sampled RSS maximum is **443,752,448 bytes**, minimum host
`MemAvailable` is **2,994,348,032 bytes**, and the whole-server observed peak
including the held reserve is **38,166,200,320 bytes**. Host and service OOM
counters, swap capacity and native swap remain zero. These are partial maxima
and minima; they do not establish the final bounds.

At that checkpoint the last persisted native counters are **952.81 CPU seconds** in user/system
time, **335,515,648 read bytes** and **300,597,248 write bytes**. Mixed reads
record 552 successes and two failures; the synchronized steady-read window has
not started. These are cumulative partial observations, not a completed timing
comparison or sustained synchronization result.

The day episode subsequently reaches a recent head after **2,237.276 seconds**
(37 minutes 17 seconds), within its declared two-hour window. It starts from
the same database already advanced by the earlier smoke/restart episodes;
this timing is for the day episode, not a cold complete-backup catch-up
benchmark. A captured near-tip head at **40,307,373** is **13.865 seconds** old.
After the reference declares that height and its successor irreversible,
its canonical ID and complete state root match exactly. Reference head/LIB
are **40,307,435 / 40,307,375**. Raw block/root responses are independently
reviewed, and all three reference containers are rechecked after the RPCs.

The initial post-catch-up window records **180 successful steady read calls
and zero failures**. One **151.360-second** natural synchronized interval uses
**6.33 native CPU seconds** (average **0.04182 CPU cores**), **1,077,248 kernel
read bytes** and **581,632 write bytes**. Observed native kernel peak RSS is
**463,765,504 bytes** and minimum host `MemAvailable` **2,964,951,040 bytes**;
whole-server accounting remains within the budget and OOM/swap remain zero.
These are partial observations under a different workload from catch-up,
not causal code-saving measurements or completed sustained qualification.

The **08:14 UTC first-hour checkpoint** covers **3,671.706 seconds** of this
day episode, including **1,434.430 synchronized seconds**. The head is
**12.194 seconds** old at readback and its telemetry is **6.874 seconds** old;
the actual native PID/start identity and service invocation remain live.
There are **846 successful synchronized read calls and zero failures**.
Observed native peak RSS is **516,374,528 bytes**, minimum host `MemAvailable`
**2,913,595,392 bytes**, and whole-server peak including reserve
**38,166,200,320 bytes**. OOM and swap remain zero. All figures are partial;
the checkpoint does not qualify the complete day.

A bounded read of 28 existing native metric lines records 32 retained parsed
modules, with attributed capacity **4,471,894 -> 4,543,721 bytes**, while logged
RSS varies **447,643,648 -> 499,679,232 bytes**. The sampled pending owners and
transaction-work jobs are zero at both endpoints. These scoped counters do
not explain total RSS: other database/cache/work/allocator memory is excluded.
No leak or whole-memory bound is inferred from them. The qualification uses
the independent kernel RSS/host-memory guards throughout the existing run.

The selected runtime configuration explicitly sets a **128-MiB shared block
cache**, **8-MiB per-family write buffer**, **64-MiB aggregate memtable flush
threshold**, two write buffers and one background job. The live command has
no additional overrides and its configuration SHA remains the pinned value.
Existing startup telemetry confirms the 128-MiB cache and one background job.
Source inspection shows one shared cache passed to the column families; the
configuration and flush thresholds are not a total process RSS ceiling.
No runtime setting is changed during this observation.

At **09:14 UTC**, the partial two-hour checkpoint covers **7,219.365 seconds**
of this day episode, including **4,982.089 synchronized seconds**. Thirty-five
bounded read-only checks confirm the same live native PID/start identity and
service invocation. Synchronized read calls total **2,952 successes / zero
failures**. Observed peak RSS is **593,633,280 bytes**, minimum host
`MemAvailable` **2,832,052,224 bytes**, and whole-server peak including reserve
**38,166,200,320 bytes**. Head/telemetry age at readback are **38.557 / 30.107
seconds**. OOM and swap remain zero. The native test is still running; two
hours is not the required completed 24-hour observation.

At **10:14 UTC**, the partial three-hour checkpoint covers **10,827.535
seconds**. Fifty-eight fresh service/process checks confirm the unchanged
native launch identity. The head is **40,310,108**, age **34.933 seconds**, and
telemetry age **29.153 seconds**. Synchronized read calls total **5,094 successes
/ zero failures**. Native peak RSS is **602,464,256 bytes**, minimum host
`MemAvailable` **2,827,907,072 bytes**, strict whole-server peak including reserve
**38,166,200,320 bytes**, and OOM/swap remain zero. The read-only observation
watcher finishes; the actual 24-hour native test remains active. The complete
packet and observation history are retained on My Passport. This is partial
evidence and does not pass the completed-day gate.

At this earlier checkpoint, the operator-amended completed five-hour duration,
fresh head at shutdown, steady mixed-read checks, post-day reopen, complete
original-archive comparison and final independent canonical sample remain
required. No early elapsed time is reported as a passing sustained result.

At **11:14 UTC**, 45 additional fresh process/service observations confirm the
same native launch and corrected private controller. Native elapsed is
**14,436.008 seconds**, with **3,563.992 seconds** remaining to five hours.
The head is **40,311,275**, age **8.813 seconds**, telemetry age **3.435 seconds**.
Synchronized reads total **7,236 successes / zero failures**. Native peak RSS
is **619,962,368 bytes**, minimum host `MemAvailable` **2,801,008,640 bytes**,
strict whole-server margin beyond the held reserve **1,833,799,680 bytes**,
and OOM/swap remain zero. The read-only observation watcher finishes; the
actual five-hour test remains active. This partial checkpoint is not final
qualification.

## Evidence and limitations

Owned local evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/vps-8gib-goal-20261018-72a46f/`.
Access routes and usable host inventory are private and excluded from public docs.
Initial local Python API, metadata-selection, no-matching-unit exit-code and
archive-validator import refusals are preserved. Corrections affect diagnostic
orchestration; they do not bypass source trust, ownership, capacity or native
test failures. The successful first broader test is retained after its archive
is verified, without rerunning or counting it twice.

The general 10-GiB default, external 70-GiB and builder 10-GiB floors remain.
A closed generated fixture archive is losslessly re-encoded, with every member
and raw tar byte verified, to obtain development workspace without losing evidence.
The minimum generally safe reserve remains **unestablished**. Issue 9 remains open.
