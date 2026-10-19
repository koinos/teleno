# Reference VPS restore and observer measurements

Updated: 2026-10-18. Status: **dense-source full restore, one-hour observer
and clean restart pass. The subsequent 24-hour attempt stops after 21 hours
7 minutes when the strict disk budget is exceeded by 11,091,968 bytes.
Complete issue #9 qualification remains open**.

## Target and preserved input

The designated replacement server reports two CPUs, 4,000,034,816 physical RAM
bytes, ext4 and zero swap. Its provider's 40-GB-class device actually contains
40,961,572,864 bytes. Qualification must still satisfy the stricter
40,000,000,000-byte whole-server requirement.

The earlier C3b native public backup contains 385 files and 24,802,854,821 payload
bytes, ID `20261016T115052Z-ms-1792151452613-files-382`. All object hashes,
complete nine-family raw equality and canonical continuity from genesis are
recorded in the [native archive report](native-archive-rewrite.md). Complete
history and all selected block/receipt records are preserved. Original source,
native backup, stable public latest and operational publisher remain intact.
Source signatures remain absent under the unchanged explicit policy;
structural consistency is separate from historical execution/signatures.

On 2026-10-17 local time, the old generated qualification namespace is retired
after all 385 public objects (24,802,854,821 bytes) are rehashed over HTTPS
against the unchanged off-host copies, all metadata is preserved and the full
remote inventory is checked. Its generated URL now returns 404. Original and
native backup copies, the stopped-node recovery set, stable public latest and
installed publisher remain preserved. This retires only an experiment's
remote duplicate; the measurements below keep their original identities.
The [denser native backup](larger-block-dictionary.md) has complete raw equality
and normal native promotion to a new isolated qualification namespace. The
latest binary's full observer profile passes read-only VPS preflight. After
the complete rollback set is reverified off-host, all 424 prior test-node files
are freshly hashed on the VPS under the compatible storage lease. Source
owners are closed; only that owned test duplicate and its preserved older
executable are retired, returning 24,879,448,064 available bytes. Original
backups, delta/runtime/config/executable recovery copies and unrelated data
remain preserved.

The new B3 native preflight passes in 0.418 seconds. Controlled interruption
passes in 12.793 seconds; resume completes in 383.268 seconds with every one
of the 19 completed file inodes, sizes and mtimes unchanged. The selected
383-file public payload is 24,114,178,822 bytes. Native staging verifies the
complete selected payload against its pinned metadata. Kernel child peaks for
these three phases are 30,638,080 / 31,916,032 / 29,204,480 bytes.

The strict raw-device accounting includes all OS/runtime, files, temporary
work, metadata and the unchanged 10-GiB reserve. Observed transfer peak requires
**39,759,351,808 bytes**, leaving **240,648,192 bytes** within 40 decimal GB.
Disk peaks/minima accumulate over every 0.2-second observation, including final
state; the retained series is sampled every second. Complete validation and
held activation now pass in **2,478.100 seconds** with **53,493,760-byte** kernel
child peak RSS. All **46,458,363 records** across nine families are checked,
including 917,316 state objects. Revision is 40,236,770, highest stored height
40,236,830 and WAL input zero; checkpoint chain/ID/root match. The same database
directory inode survives activation and the observer hold remains active.

Strict peak over all four phases is **39,760,650,240 bytes**, leaving
**239,349,760 bytes**. Minimum concurrent host available memory is
**3,429,847,040 bytes**, with zero configured swap and unchanged OOM/swap
counters. Activation uses 2,325.660 process CPU seconds and reports
179,308,584,960 physical input bytes / 122,880 output bytes. These are actual
pilot observations with a different compressed source and cache state, not a
five-pair full-restore speedup claim. The previously documented mechanism pairs
retain their separate source and scope.

The CLI deliberately keeps its generic `capacity_40gb_qualified` flag false;
it does not certify complete server/runtime/history qualification. Its
post-command allocation sample is not a peak. The independent raw-device,
available-space and reserve measurements above establish this narrower
full-restore result. They do not establish sustained growth or an indefinite
40-GB capacity guarantee.

All 439 selected diagnostic/control/runtime metadata archive members are
verified before the first one-hour B3 observer starts. Its complete DB is
already represented by the unchanged selected native backup; metadata archiving
does not create a second full payload. Startup reaches ready, verifies mainnet
identity and advances head under explicit disabled-production, observer-hold,
strict disk/memory and no-swap/OOM guards. The one-hour run now completes in
**3,601.088 seconds**, exit zero, without a guard failure. Head advances
**34,955 blocks**, from 40,236,956 to 40,271,911. Peak sampled/kernel HWM and
maximum sampled RSS after the 900-second warmup are **634,728,448 bytes**;
GNU time reports a 634,605,568-byte owned-child peak. Minimum concurrent host
available memory is **2,837,860,352 bytes**. Swap remains absent and OOM/swap
counters do not change.

The strict whole-server runtime peak including the unchanged reserve is
**39,877,636,096 bytes**, leaving **122,363,904 bytes**. Whole-run process CPU
is 1,924.190 seconds, physical input 706,904,064 bytes and physical output
540,585,984 bytes. The first head timestamp within 30 seconds of the host clock
appears at 1,834.511 seconds. The following 1,762.544-second sampled window
uses 92.230 process CPU seconds, about **2.62% of two-vCPU capacity**, with
14,045,184 physical input and 8,421,376 output bytes. RSS rises from
552,656,896 to 634,728,448 bytes during that window; a memory plateau is not
established by this short run. Sixty native RSS log samples agree with the
independent kernel observations within 6,512,640 bytes, with no mismatch.
The last native metrics report zero pending transactions/record owners and
32 parsed VM cache entries with 4,550,222 attributed bytes. This describes the
observed workload; nonempty mempool and larger mixed workloads remain separate
qualification work.

An independently checked near-tip head/root at **40,271,440** matches the
pinned running legacy reference once its LIB reaches 40,271,441. The completed
one-hour namespace has all nine members verified in a **323,174-byte** archive.
The stronger [controlled producer fixture](controlled-native-producer.md) also
passes on this VPS, with its complete history source held unchanged.

A separate **90-second** native restart/telemetry smoke passes, peak RSS
97,034,240 bytes, with mainnet identity, observer hold and absent producer key
preserved. All 18 read-RPC latencies are counted in bounded histograms; its
nine-member namespace is verified in a 13,560-byte archive.

The first day-long attempt stops at native preflight before starting a node:
the conservative JSON-RPC probe binds without address reuse immediately after
shutdown. A controlled Linux loopback test reproduces `EADDRINUSE` during
`TIME_WAIT` without reuse and successful bind with the runtime's reuse setting.
After the delay, the unchanged native preflight passes. The failed two-member
namespace is preserved and verified, with no database mutation or check bypass.
The newly labelled **`b3-day1-r1` 24-hour observer starts** on the same B3
binary and configuration. Resource/hold/key guards run every half-second,
read RPCs every ten seconds, and persisted samples every minute; all RPC
latencies, including an in-flight final pair, remain counted. Log/telemetry
budgets and every resource ceiling remain unchanged. Completed sustained,
abrupt-recovery, PoB and full-goal qualification are still open.

A PID/start-tick/executable check at the earlier six-hour checkpoint confirms
the observer is then live at **22,029.077 seconds**. Cumulative sampled steady/peak
RSS and kernel HWM are **338,604,032 bytes**; concurrent host available memory
stays at least **3,093,225,472 bytes**. Strict whole-server margin including the
unchanged reserve stays at least **108,359,680 bytes**. The latest captured
head is **40,279,252**, about 23 seconds old, with 4,372 successful read calls
and the same two startup failures. A preserved earlier sample independently
matches pinned legacy at **40,278,632**, using the successor's previous-state
root. These are intermediate observations of the frozen B3 process, not a
completed day or qualification of the later source candidate.

The final report at **2026-10-17 22:45:32 UTC** supersedes that live status.
After **76,023.649 seconds** (21 hours 7 minutes), a half-second resource sample
crosses the strict disk ceiling. Minimum margin is **-11,091,968 bytes**:
whole-device occupied/unavailable space plus the unchanged 10-GiB reserve
reaches **40,011,091,968 bytes**. Minimum physically available root space is
**11,687,899,136 bytes**, still above the **10,737,418,240-byte** reserve.
The strict margin additionally charges the device's space beyond the requested
40,000,000,000-byte budget; physical free space minus reserve is therefore not
the strict margin. The result fails the sustained disk requirement even though
the disk is not physically full. The peak's exact file/compaction attribution
is not established by this report.

The controller requests a clean stop and the native process exits zero;
the observation is correctly recorded as failed, not as a completed 24-hour
test. Shutdown returns some space, leaving a positive final margin of
24,309,760 bytes; a positive final sample does not erase the failed peak.
Peak kernel RSS is **407,150,592 bytes**, minimum concurrent host available
memory **3,033,333,760 bytes**, with zero configured swap and no OOM/swap-counter
increase. Head advances **24,651 blocks**, from 40,272,082 to 40,296,733.
Memory and head-progress observations retain this frozen B3 workload scope.
Observer hold and absent producer key remain intact.

A fresh read-only check at **23:59:48 UTC** confirms the owned native process
is closed and the same failure report is present. No automatic restart,
reserve reduction, database cleanup or source replacement is performed.
The failed namespace and exact outcome are preserved in the owned external
run under `evidence/dictionary-policy-20261016/actual-baseline-restore-20261017/long-observer-20261017/`.
Further lossless savings and fresh sustained measurements remain required.

The subsequent [reserve sizing analysis](../analysis/recovery-reserve-budget.md)
verifies the stopped node's OPTIONS and compaction logs. It proposes 8-GiB
and then 6-GiB qualification candidates, without changing the current reserve
or treating arithmetic on this B3 peak as a new test pass.

Before replacing that owned test restore, four current-source fixtures pass
on the actual reference VPS using frozen `b3f83775...` libraries:

| Fixture | Seconds | Kernel peak RSS bytes |
| --- | ---: | ---: |
| Controlled producer | 0.756 | 11,112,448 |
| Fizzy lifetime/allocation | 0.127 | 17,002,496 |
| Storage/configured-WAL recovery | 7.604 | 53,612,544 |
| Real-loopback RPC batch/retention | 0.451 | 50,548,736 |

All exits are zero, source metadata remains unchanged, configured/owned swap
is zero and OOM/swap counters are unchanged. Concurrent host available memory
stays at least 3,425,996,800 bytes and root space at least 10,898,079,744 bytes.
Producer work uses the existing fake-chain fixture and synthetic keys, without
mainnet signing or activation. The complete 32-member namespace is archived,
every member verified, then only its generated remote copies are retired.
These are current component regressions, not a new observer or producer soak.

The earlier restore executable uses frozen source manifest
`c3b1cc4e2dad577874d5e0bb0afd8d303a89a5afe0b1d57ba11d016cd99fdb10`,
SHA-256 `a18c6e17038907d7e1d8a7fa0eb7ec815c3f2c71a37483c8d0d4342e4ba48f5f`.
It reports `1.3.0-dev.0+d3cfb94455da-dirty`. No later source change is credited
with this completed restore.

## Earlier C3b complete direct restore

| Phase | Exit | Wall seconds | Timed owned-child peak RSS bytes |
| --- | --- | --- | --- |
| Metadata preflight | 0 | 0.424 | 23,322,624 |
| Controlled stage interruption | 130 | 33.392 | 25,124,864 |
| Same-source resume | 0 | 920.958 | 24,018,944 |
| Offline validation and held activation | 0 | 3,608.699 | 49,008,640 |

Preflight preserves the unchanged 10-GiB reserve. The interruption retains
18 complete files totaling 1,076,281,824 bytes. Resume reuses all 18 with the
same inode, size and mtime, receives the remaining 367 files and validates
every payload body. The staged payload occupies 24,805,289,984 allocated bytes.

Activation performs three full read-only consistency scans: validation,
prepublication validation and post-rename reopen. Each checks 46,458,363 records
across nine families, including 917,316 state objects, durable revision
40,236,770 and highest stored height 40,236,830. State identity/root agree with
the selected checkpoint. WAL input is zero. The same database directory inode
survives activation; the observer hold remains active.

Minimum concurrent host `MemAvailable` during activation is 3,418,607,616 bytes;
minimum root available space is 11,210,088,448 bytes. Swap and OOM counters do
not change. The last sampled activation process I/O reports 248,005,733,820
logical bytes read and 228,512,276,480 physical bytes read. Repeated complete
checks are a material I/O/time cost. No validation is removed by these results.

## Earlier C3b/C073 whole-server disk finding

Root filesystem capacity is 39,956,590,592 bytes. Root-only accounting excludes
parts of the physical device: boot partitions, partition gaps and filesystem
formatting overhead. Earlier positive root-filesystem margins therefore do
**not** prove the strict whole-server budget.

Use the conservative measured requirement:

```text
required = raw persistent device bytes - root f_bavail bytes + 10 GiB reserve
strict margin = 40,000,000,000 - required
```

This counts all other device space as unavailable, including reserved blocks;
it gives no credit for unused EFI space. `/boot` is on root; RAM-backed tmpfs
is not additional persistent storage. At the stopped post-restore audit,
required capacity is 40,488,845,312 bytes: **488,845,312 bytes over budget**.
The later observer peak requires 40,628,682,752 bytes: **628,682,752 bytes
over budget**. The actual server still retains its native 10-GiB reserve.

Earlier C3b/C073 strict 40-GB acceptance is **failed**, independently of successful restore on
the slightly larger actual device. Do not reduce the reserve, prune history,
remove receipts or count filesystem-reserved space as available to close it.
The measured owned non-database duplicates total only about 217 MB; cleanup
alone cannot close the observed gap. Further lossless storage savings and a
new complete capacity measurement remain necessary.

## Short observer and restart observations

Runtime keeps production disabled and the restore hold intact. Candidate
settings use one chain/P2P/RPC worker, loopback JSON-RPC with one session,
128-MiB block cache, 64-MiB aggregate memtable threshold and two 8-MiB write
buffers. Existing optional-service selections are preserved. This is not
evidence of complete optional legacy-service parity.

| Observation | Original restore binary | Read-buffer binary |
| --- | --- | --- |
| Duration, seconds | 900.611 | 901.434 |
| Captured first / last height | 40,236,950 / 40,251,766 | 40,251,908 / 40,260,861 |
| Kernel/timed peak RSS, bytes | 304,467,968 | 297,615,360 |
| Minimum host `MemAvailable`, bytes | 3,182,432,256 | 3,198,681,088 |
| User / system CPU, seconds | 854.28 / 7.17 | 537.89 / 5.29 |
| Native exit | 0 | 0 |

The second run uses read-buffer manifest `af826d523daa1d196808b7a1e69effaadfe6e119bfa4c9be34a15af1b24d3553`
and executable SHA-256
`b68994f0bdf4ec2b46c92606b1d8e162306edeb648efd1ed68809d9cd4510e0b`.
It restarts the same restored database, catches recent head after about
555 seconds and has about 346 seconds of sampled recent-head observation.
The different catch-up/head-following workloads prevent attributing the lower
second-run CPU or RSS to the transport change.

Both runs reach ready, verify chain identity and stop cleanly after planned
SIGINT. Neither changes swap or OOM counters or trips the resource guards.
Each records 178 successful RPC calls and two initial startup failures.
Connection events establish observed connections, not a continuous peer count.

Independent read-only reference checks use existing legacy versions chain
`v1.5.2`, block store `v1.1.0` and JSON-RPC `v1.2.0`, with image digests retained
in private external evidence. At two captured heights, native block IDs match
legacy canonical blocks; the successor's previous state root matches native
executed state. The final second-run match is at height 40,260,861. This is
current-height agreement, not complete historical differential replay. An
earlier public RPC returned HTTP 403; it supplied no comparison evidence.

## One-hour RSS-fix observation

The frozen RSS-fix executable `52a17d127dcdb0c5f25bb87227d5f41d085c4d3a13ea920e25e160845a0cf2e5`
subsequently runs for 3,600.852 seconds and stops cleanly. It uses source
`c073ab12836d01ddd761e8146ed829792fee0f92b77c622059cf798951208c3b`
and the same runtime config/compression policy. Kernel HWM is 260,673,536 bytes;
the owned-child time report gives 260,423,680 bytes. Retain the larger observed
value rather than treating these sampling/accounting differences as savings.
Minimum concurrent host `MemAvailable` is 3,218,644,992 bytes. Swap/OOM counters
remain unchanged; no resource guard trips.

Captured head progresses from 40,260,964 to 40,262,636, with 714 successful RPC
calls and two initial startup failures. All 60 native RSS log checks agree
with kernel samples within 835,584 bytes (816 KiB). The final block ID and
executed root agree with the independently queried legacy reference; reference
LIB is 40,262,898, beyond the compared height. This is another current-height
agreement, not historical execution proof.

The minimum strict disk margin is still negative 629,100,544 bytes. This
observation does not close 24-hour/seven-day, mixed-load, producer or abrupt
recovery requirements. It does not exercise the later 128-KiB dictionary policy.
Complete observer/control archives and all failed preparation records remain
under the owned evidence directory. An initial installation helper expects a
version string without the normal `teleno_node` prefix and fails its assertion;
the following prepared coordinator still starts after independent hash/config/
preflight checks. Corrected CLI verification confirms the expected prefix,
hash and help without reinstalling or relaunching. That helper failure remains.

## Failed tooling and remaining acceptance

The first observer monitor stops its newly owned startup almost immediately
because of an undefined Python variable. No ready/database-open observation
is credited to that attempt. The failure, original script and logs remain.
The corrected monitor runs the two observations above without repeating the
restore, clearing state or weakening guards. Independent kernel RSS and GNU
time supply memory evidence; the older native Linux RSS log field counted
virtual size and is addressed in the separate
[RSS correction](linux-rss-counter.md).

These short observations do not close 24-hour/seven-day operation, mixed
RPC/P2P/VM/state pressure, disk growth/compaction, abrupt recovery, controlled
producer workloads, full historical compatibility, aggregate memory or release
qualification. No producer is enabled. The persistent optimization goal stays
active.

Owned evidence is under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/native-export-k3ky3m61/`.
It includes the complete restore ledger, corrected whole-device budget,
externally verified restore/observer archives, per-second resource samples,
legacy comparison responses and every failed monitor attempt. Both 900-second
observer coordinators and the full restore coordinator are terminal; inspect
their records instead of relaunching them.
