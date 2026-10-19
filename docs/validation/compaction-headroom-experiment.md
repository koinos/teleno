# Isolated native compaction headroom experiment

Date: 2026-10-18. Status: **component experiment and review complete.
No reduced reserve is qualified. Issue 9 remains open.**

Automatic compactions producing more than 256 MiB and 1 GiB pass on
Linux/ext4. The largest archive scale passes three fresh trials,
with byte-identical largest-job output sizes and a maximum observed owned DB
allocation of **2,291,363,840 bytes**. Every expected record, checksum and
ordinary-native reopen digest passes. The verified backup sample and the
separate interruption/refusal checks also pass.

**Recommendation: retain the current policy; qualify 8 GiB first in a separate
whole-server experiment.** The measured workspace supports planning that test,
but neither 8 GiB nor 6 GiB is established as a safe minimum. Product source,
current 10-GiB default, VPS configuration and issue 9 criteria are unchanged.
This is the outcome of the [component goal](../implementation/compaction-headroom-test-goal.md),
not whole-node or producer qualification.

## Exact source, dependencies and executable roles

All 311 native inputs match manifest
`a21a8958e1757a8f2f661264b5b8665158c6db33c9c31a2fed8991d39e962a20`
on both Mac and Linux before execution and at final review. Branch is
`codex/lr2-memory-hardening`, HEAD
`d3cfb94455da4a472f651eb4239abbc289e74225`, plus the preserved worktree.
Native `VERSION` remains `1.3.0-dev.0`. AGENTS/CLAUDE remain identical.

The pinned RocksDB is 8.8.1; its 30,659,064-byte static library SHA-256 is
`8e49a0647f9600ffd2c6fbdaf90da78107fc1fa52b1d3ed5eb78a164805a0358`.
Compiler is `c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`. Full flags, native archive hashes,
link commands, maps and private-source hashes are in the evidence. All native
library inputs are byte-identical between r4 and r5.

A private copy of the current native `RocksDBManager` adds exactly two optional
hooks: the event listener, and an SST quota manager used only by the named
refusal case. Other options and native implementation bytes remain unchanged.
The writer links that private unit; the reader links the **unmodified native
storage archive**. Link maps confirm the distinction. The native storage unit
retains RTTI; the listener probe uses `-fno-rtti`, matching RocksDB. Product
source and installed dependencies are not patched for the diagnostic.

These are diagnostic executable SHA-256 identities, not a new node release:

```text
r4 writer: ca8d2b3811fc79b273d740cef96370423e45631caffc7d5e0e7bd4ce60b3cbdc
r4 reader: 994ef366e64d20432a9f7c0caf1d872b92dea8bb04238a04f4394c0b4046e831
r5 writer: 005b4d57ae5154a8cbce90fd264451ece3ac01206320c7890dcf4135c7da6df0
r5 reader: 7edd630a67aca17bd68a99ccd2f118a9585e4fbd0539d459f27c372a93fbb2aa
```

R4 runs the scale/recovery/refusal cases. R5 changes the diagnostic import order
only: unchanged selected archive records are deterministically permuted before
writing. Its ordinary reader and the native implementation remain independently
identified. Earlier r2/r3 identities remain in their frozen plans and archives.
No earlier binary is substituted for the reported final diagnostic revision.

## Configuration and measurement boundaries

The target profile remains Ubuntu/x86_64/ext4, two vCPUs, 4,000,000,000 physical
RAM bytes and a 40,000,000,000-byte whole-server budget. These component runs use
the authorized larger Linux build-and-test host. Each command passes a verified
pre-execution cgroup gate: two-CPU quota, 2.5-GiB memory maximum and zero permitted
swap. Root free space stays above the unchanged 10-GiB test-host floor and host
MemAvailable above 2 GiB; the existing shared build-storage limits remain.
My Passport retains its unchanged 70-GiB floor. The host has configured swap;
owned native swap stays zero and the global OOM counter remains at its starting
value of 1. This is not a physical 4-GB VPS qualification.

Each native open uses nine leveled column families, 64-MiB SST targets,
8-MiB family buffers, 64-MiB aggregate memtable/WAL thresholds, one configured
background job/subcompaction, and the block policy of Zstd level 15 / 128-KiB
dictionary. State retains its native compression policy. The nominal
1.5625-GiB compaction-input target and WAL setting are soft policies. Runtime
profile assertions and the preserved OPTIONS files identify the actual policy.
Final OPTIONS review confirms all nine buffer/file-size policies, aggregate
buffer/WAL values and the selected block compression options.

No manual compaction is called. Writes cause automatic leveled jobs; explicit
flushes drain or shape the disposable workload. Listener callbacks record actual
job statistics and per-file allocated bytes. Resource checks run about every
20 ms. Since r3, aggregate trace retention is about every 200 ms with full peak/
last file snapshots, all native callbacks retained and deleted descriptors
counted once per device/inode. GNU time supplies kernel peak RSS, CPU and I/O
block counters; raw `/proc` I/O counters remain available. Sampled/callback
allocation peaks are observations, not mathematical upper bounds. Allocation
sums regular DB files and owned open/deleted files; directory/inode/journal
overhead remains in global filesystem-free samples. Those samples also include
unrelated host activity. Across the 28 r4/r5 commands, minimum sampled root
available space is 10,918,191,104 bytes and minimum host MemAvailable is
4,459,712,512 bytes, reinforcing the larger-host boundary.

Native timing includes diagnostic callbacks. Python monitoring is additional:
22.352–27.676 CPU seconds and at most 24,186,880 RSS bytes for the archive
trials. Shared-host scheduling, frequency and page cache affect timings.
No shared cache is forcibly cleared. Reader timing does not prove cold-media
latency on the smaller VPS.

## Workloads and completed scales

The initial synthetic fixtures use deterministic hash-shaped 34-byte keys and
16-KiB mostly incompressible values. Archive appends; state subsequently updates
one eighth of values. Both 32-MiB pilots pass. The r2 288-MiB archive/state
cases check all 18,432 records, all nine CF digests/checksums and ordinary reopen.
Their largest successful jobs output **294,668,223 / 303,251,743 bytes** and
owned allocation peaks are **651,030,528 / 688,406,528 bytes**.

An ordinary r3 1,056-MiB state workload checks all 67,584 records but produces
only a 569,313,038-byte largest job. It **does not** pass the 1-GiB output gate.
RocksDB divides the work across levels 5 and 6. Input size is not job output.

To exercise a physically larger job without changing native options, r4 first
writes 32 MiB, then a pre-reserved 1-GiB atomic batch, then 24 MiB in small
flushes. Each case has 69,120 records / 1,080 MiB of values. This intentionally
overshoots soft buffer targets: **synthetic storage stress, not ordinary Koinos
transaction traffic**. The RSS peak includes the batch and memtable ingestion.
Every case checks all generated records, all nine CF digests/checksums, then
reopens through the unmodified native storage reader and compares the digest.

| 1,080-MiB workload | Largest successful job output, bytes | Observed DB allocation peak, bytes | Writer CPU, s | Writer wall, s | Native kernel RSS peak, bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| state-r1 | 1,111,975,604 | 2,290,585,600 | 17.59 | 25.213 | 2,168,426,496 |
| archive-r1 | 1,111,250,915 | 2,291,359,744 | 292.55 | 301.993 | 2,181,976,064 |
| archive-r2 | 1,111,250,915 | 2,291,359,744 | 246.88 | 253.054 | 2,181,566,464 |
| archive-r3 | 1,111,250,915 | 2,291,363,840 | 257.34 | 263.409 | 2,201,108,480 |

All four largest jobs output at level 6. Archive is selected for three trials
because it has the larger initial physical peak at the largest input scale.
Its actual largest-job duration is 117.163–135.763 seconds; state is 6.575
seconds. Archive physical output is 3,417,350,144–3,417,370,624 bytes and input
35,983,360–37,613,568 bytes. State physical output/input is
4,079,210,496 / 50,343,936 bytes. State includes extra updates and a different
compression policy: these are not paired speedup ratios or a producer-latency
qualification. Raw results preserve the variation between equivalent archive
trials.

### Real selected-backup sample

The representative content is `db/009413.sst` from selected backup
`20261016T115052Z-ms-1792151452613-files-382`: 66,533,451 bytes, SHA-256
`752dd3a2a7034770889c90ee100573b7fbc5ff279ea462da7f69342ddfe5edbb`.
The immutable source is recovered from the previously verified archive and
independently size/hash-checked again. It has **121,814 original block/receipt
records**; r5 imports every unchanged key/value once in a fixed permutation.
The permutation avoids treating sorted SST order as ordinary block arrival;
it is a diagnostic arrival order, not a canonical replay.

All 257,400,266 raw user key/value bytes compare exactly with the source after
native compaction and ordinary reopen. Source/output checksums and the nine-CF
digest pass. Four successful automatic jobs run; the largest outputs
58,909,656 bytes. Owned allocation peaks at 180,867,072 bytes, native kernel
RSS at 393,592,832 bytes, CPU is 136.57 s and writer wall time 96.686 s.
Physical output/input is 440,688,640 / 65,536 bytes. This case includes input
preloading, permutation and verification. Its content is real; its scale is
separate from the synthetic GiB jobs. A fragment is not full-history validation.

## Temporary space, overlap and stalls

The three archive trials retain 1,136,738,304 allocated bytes of final SST
content. The maximum observed allocation exceeds that by
**1,154,625,536 bytes**. This attributes additional measured workspace without
adding the generated dataset again to an existing node. It is not a bound for
a 24-GB database or every possible workload.

The largest archive callback observes roughly 1.111 GB of old input and
1.111 GB of new SST allocation simultaneously. Maximum observed WAL allocation
is **1,079,619,584 bytes**, despite the 64-MiB trigger. Allocated space in excess
of logical file lengths, including preallocation and block rounding, reaches
70,746,189 bytes for archive and 134,908,495 for state. WAL and old/new
SST maxima do not all coincide; summing unrelated phase maxima would double
count capacity. All large cases show one flush and one compaction active at
once, despite one configured background job. Sampled owned open/deleted
allocation is zero; that is an observed result, not a universal exclusion.

Native logs contain two write-stop warning events per archive trial and nine
in state. Flush callback flags alone would miss these. Initial zero-duration
log statistics are not terminal counters; **exact cumulative stall duration
is not instrumented**. The reported writer/job times include their effect.
Retained-reader/checkpoint and delayed deletion scenarios remain outside this
component's demonstrated envelope.

## Controlled interruption and capacity refusal

The interruption case verifies and fsyncs a complete 18,432-record baseline,
then performs synchronous idempotent overwrites. At automatic job 6 begin,
with 332,742,323 input bytes, the listener exits the process with expected
status 87. There is a begin event without a matching completion. The ordinary
native reader reopens and verifies every baseline record, checksum and the
identical nine-CF digest. This is interruption **at compaction begin**; it does
not test power loss, partially emitted output SSTs or interrupted new-value
updates.

The separate capacity case supplies a **12-MiB diagnostic SST quota** through
RocksDB's `SstFileManager`. It observes
`IO error: Space limit reached: Max allowed space was reached`. All 1,024
acknowledged records remain byte-identical; the failed batch has zero visible
records. After expected exit 88, an unmodified native reader without that
quota recovers and checks all 1,024 records and the same nine-CF digest.
The quota is not a total physical-DB limit, physical host ENOSPC, or a new
Teleno default. The shared filesystem is not exhausted. Both nonzero exits
are explicit test expectations, not hidden failed jobs relabelled as passes.

## Reserve assessment and unexecuted scale

Use separate allowances for existing data, temporary workspace, emergency
free space and growth until operator response. The current supervisor holds
the entire reserve free during temporary peaks. A future partition in which
compaction consumes some reserve is a different contract and needs its own
qualification. Reducing an allowance saves **zero physical bytes**.

The diagnostic model below adds the maximum observed workspace increment
(1,154,625,536 bytes) to the **older B3** VPS occupied peak (29,273,673,728), a
held-free reserve and illustrative growth. It excludes final generated SST
content from the addition. The B3 baseline already contains its smaller
transient work, so this extra-workspace scenario is deliberately additive;
it is neither a B7 measurement nor a worst-case upper bound. Growth values
are assumptions, not measured forecasts. Margins are against exactly
40,000,000,000 bytes; negative values exceed that budget.

| Held-free reserve | Margin, no additional growth, bytes | Margin, +512 MiB growth, bytes | Margin, +1 GiB growth, bytes |
| --- | ---: | ---: | ---: |
| 10 GiB | -1,165,717,504 | -1,702,588,416 | -2,239,459,328 |
| 8 GiB | 981,766,144 | 444,895,232 | -91,975,680 |
| 6 GiB | 3,129,249,792 | 2,592,378,880 | 2,055,507,968 |

Eight GiB is the first candidate to qualify; its illustrative margin is only
444,895,232 bytes after 512 MiB growth, and 1 GiB growth exceeds the budget.
Six GiB permits more data but reduces the emergency floor by another 2 GiB.
These tests do not establish that reduction as safe. Keep the 10-GiB policy
and [issue 9](../implementation/issue-9-goal.md) unchanged pending final-source
whole-server restore/reopen, catch-up, sustained/mixed workloads, recovery
and isolated producer qualification. Operational producer evidence remains
separate.

A fixture around the 1.5625-GiB nominal input target is **not executed**.
The two-copy plan plus existing allowances needs 3,556,769,792 bytes above
the unchanged host floor; final available workspace is only
2,337,210,368 bytes. The recorded earlier roughly 2.5-GB preparation
headroom is also insufficient. Applying the oversized-batch method at that
scale would additionally exceed its 2.5-GiB memory limit. Capacity is declined
before data generation; no floor is reduced. Input size would still not
guarantee a particular automatic job's output. Larger jobs, retained readers,
long growth horizons and whole-node/producer behavior remain unqualified.

## Preservation, reproducibility and review

Owned local evidence is
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/compaction-headroom-goal-20261018-7b585e/`.
Plans retain all private source, exact commands/flags, input identities and
executable/library hashes. `measured-workspace-analysis.json` derives peaks,
overlap, I/O and budgets from the completed traces. `final-native-inventory.json`
rechecks all native inputs/libraries, optional-only overlays, reader link roles,
process closure and unchanged resource floors.

The r0 link failure (RocksDB built without RTTI) and r1 native-unit compilation
failure (Boost.Log requires RTTI) are completely archived, with their failed
statuses intact. Neither runs a fixture. R2 solves the diagnostic flag mismatch
without changing product dependencies. Its full 189-member namespace retains
every raw tar byte in a verified 363,477,977-byte Zstd archive. Closed r2/r3
remote namespaces and private object/map duplicates are retired only after
fresh exact-member checks and verified external recovery copies. Restore those
copies before historical commands reference the retired paths.

Passing generated DB payload is disposable only after every-record/checksum/
reopen checks, final file hashes and externally verified source/metadata/traces.
The representative policy explicitly preserves its original selected source.
Recovery/refusal cases receive **complete** retained-data archives:
74 members / 356,187,267 bytes and
45 members / 40,108,538 bytes respectively.
R5 source, binaries, selected input and remaining evidence are fully preserved:
81 members / 93,546,770 bytes, archive SHA-256
`a279033b180d26862c727f62e9ec15198751dacd8a248698165030c30fc14958`.
Original backups, operational data, services and current native build/source
are preserved. No VPS database is opened, restarted or compacted by this goal.

To repeat the large archive case, use the recorded guarded coordinator from
the local evidence with a fresh owned label and sufficient preflight capacity.
Set `COMPACTION_EVIDENCE` to that directory and `COMPACTION_PYTHON` to the
recorded Python 3.12 interpreter. Do not rerun an installer or reuse a
completed/failed case label. For example:

```bash
"$COMPACTION_PYTHON" -B "$COMPACTION_EVIDENCE/r4/run-action.py" case fresh-owned-archive archive 69120 bulk-create
"$COMPACTION_PYTHON" -B "$COMPACTION_EVIDENCE/r4/audit-preserve-retire-case.py" fresh-owned-archive
```

The exact native payloads are `writer CASE bulk-create 69120 16384 archive 1`
and `reader CASE verify 69120 16384 archive 0`; the coordinator supplies the
ownership, source/dependency, resource and pre-execution gates. Recovery uses
`run-action.py recovery`, refusal uses `run-action.py capacity`; each requires
its corresponding `audit-special.py` and full preservation. Representative
input uses the frozen r5 `import-create` plan and pinned input. Evidence
scripts are development diagnostics, not installed Teleno CLI commands.

Review checks actual completed outputs against per-file sizes, three identical
archive inputs/binaries, every logical/checksum/reopen result, expected refusal
and interruption statuses, and a clean final process inventory. No source
optimization or reserve change is inferred from this component result. The
minimum safe reserve remains **unestablished**; that explicit negative conclusion
is compatible with completing the requested experiment.
