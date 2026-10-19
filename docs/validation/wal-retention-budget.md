# Retained WAL disk budget investigation

Date: 2026-10-16. Status: **five pairs complete on both Linux hosts;
optional native setting implemented; whole-node qualification remains open**.

Both complete historical replay attempts on the designated VPS stop at the
unchanged 10-GiB root free-space guard. The shared-native-backend retry remains
well below the memory ceiling. A read-only physical sample finds 299,394,207
WAL bytes in 45 files and 46,657,290 SST bytes. The complete source node stays
read-only with unchanged metadata; every block and receipt remains present.
See the [historical evidence](historical-checkpoint-eligibility.md).

The actual saved options have a 64-MiB aggregate memtable setting but
`max_total_wal_size=0`. In the linked RocksDB 8.8.1 header, zero selects an
automatic flush threshold equal to four times the sum of each column family's
write-buffer size times its buffer count. Nine families with two 8-MiB buffers
therefore select 576 MiB. Infrequently updated families can keep earlier WAL
files needed for recovery even after the active family flushes. A memtable
setting is not a retained-WAL disk bound.

`max_total_wal_size` is a **soft flush trigger**, not a hard upper limit. Normal
RocksDB flushing and WAL reclamation must preserve durability and every live
record. No manual WAL deletion, history pruning, weaker synchronization or
smaller recovery reserve is part of this investigation.

## Isolated mechanism

An external diagnostic uses the real native `RocksDBManager`, nine column
families, Zstd, 8-MiB write buffers, a 64-MiB aggregate memtable setting, a
64-MiB cache and one background job. It compares the existing automatic WAL
policy against an explicit 64-MiB trigger through the linked library's
`SetDBOptions` API. The measurements use a private control on frozen preceding
native libraries. The new YAML implementation is validated separately below.

The fixture puts one retained value in every family, then writes 256 MiB of
deterministic updates to 64 hot keys. Every batch synchronizes its WAL. The
writer checks every expected value, all SST checksums and a framed raw digest,
then exits without C++ destructors. A separate process recovers read-only with
absolute consistency, verifies all records/checksums and unchanged file
metadata, and reopens through the ordinary default native reader. This tests
synced-WAL process-exit recovery; it does not simulate power loss or a chain.

The first pilot's cases individually preserve all 79 raw records through
recovery. Their cross-case hashes differ. An exact read-only pointwise
comparison confirms that only `layout.basedir` and `layout.created_at` differ;
all other raw values and checksums match. That failed aggregate assertion and
both fixtures remain preserved. The repeated fixture uses the same owned
basedir sequentially and a fixed, explicitly synthetic creation timestamp.
Its cross-case digest includes every record, without raw-hash exclusions.

The initial pilot observes 270,199,855 peak WAL bytes for automatic policy and
68,672,011 for the explicit trigger. Write wall times are 8.418 and 8.574
seconds. They are retained separately from the repeated results below.

## Five pairs on each Linux host

Both hosts complete five pairs with alternating control/candidate order. All
79 raw records, digests, SST checksums and expected cold/hot values match
between policies and through both recovery opens. Read-only recovery preserves
file metadata. Every pair namespace is archived and member-verified before
its owned duplicate files are retired.

| Host | Peak-WAL ratio | Allocated-file peak ratio | Write-CPU ratio | Write physical-byte ratio | Recovery CPU ratio | Recovery wall ratio |
| --- | --- | --- | --- | --- | --- | --- |
| Larger Linux builder | 0.254 | 0.288 | 1.070 | 1.00030 | 0.066 | 0.147 |
| Designated 2-vCPU / 4-GB VPS | 0.250 | 0.288 | 1.007 | 1.00032 | 0.080 | 0.093 |

Values are median paired candidate/control ratios. On the VPS, the control
reaches 270,199,936 sampled WAL bytes and the candidate 67,616,632 in every
pair. Peak retained WAL falls about 75% and allocated-file peaks about 71%.
The 64-MiB trigger is 67,108,864 bytes: the observed peak exceeds it, directly
illustrating the soft semantics.

VPS median write CPU increases 0.66%, physical writes 0.032%, and write wall
ratio is 0.929. Recovery CPU and wall decrease about 92% and 91% in this
fixture. The builder measures a larger 7.02% write-CPU cost. Recovery RSS
ratios are 1.008 / 1.011; no memory saving is established. Largest measured
process RSS is 31,842,304 bytes on the VPS, with concurrent host available
memory at least 3,386,286,080 bytes. Actual VPS swap is disabled, sampled owned
`VmSwap` stays zero and OOM/swap counters remain unchanged.

The shared builder has a configured swap pool. An earlier attempt stops when
a global swap counter changes despite over 8 GB of available memory. Its
correct first pair and aborted next case remain preserved. The repeated
builder run reports global activity and requires sampled owned `VmSwap` to
stay zero; the VPS keeps the strict whole-host zero-swap check. No swap-free
builder-host claim is made.

These are bounded overwrite and recovery measurements. They do not establish
whole-node throughput, compaction tails, producer timing, arbitrary crash
recovery or a hard WAL ceiling.

## Native implementation

New source accepts optional `rocksdb.max-total-wal-size`, default `0`, and
passes it to `DBOptions` before opening storage. Invalid unsigned values fail
in ordinary and strict parsing. Preflight reports `storage.wal: not_checked`
with the configured policy. An explicit trigger appears in the startup log.
Defaults, compression, synchronization and record retention remain unchanged.

The existing storage test execs a fresh child, writes synchronized hot/cold
records across all nine families with a small explicit trigger and exits
without DB destructors. The ordinary default reader verifies checksums and
every expected record afterward. Parser/CLI tests cover zero, explicit values,
malformed values and preflight filesystem preservation. This covers process
exit, not power loss.

Frozen source manifest, 306 inputs:
`b3f83775d32e815d94adcbee967d2d66333e20eaed33d764daa390bcf9f80364`.
Seven paths differ from scan reuse: config/header, preflight, storage wiring
and the three existing config/storage/CLI fixtures. All-target builds,
version/help and all 61 tests pass on Mac/Linux. The Mac suite takes 214.469
seconds; Linux exhaustive 59/1/1 batches take 147.790, 1.404 and 7.213 seconds.
All source hashes are rechecked. Every Linux fixture archive is member-verified
before owned duplicate retirement; reserve and test timeouts remain unchanged.

Frozen executable SHA-256 values:

- Mac: `b854856b8afcc96c97b51ab3d0057171aca8a4882199564f18df8a00b6e21dd8`.
- Linux: `b38318065a982d9a83de30f4dafd0dd85a00a2709f32aa9bec23e9c28788332c`.

Both report `teleno_node 1.3.0-dev.0+d3cfb94455da-dirty`. The Mac all-target
build takes 657.260 seconds, including initial external-disk contention. Three
owned Mac data jobs pause for 324.975 seconds during the bounded build/test
window, then resume. Mac timings are development observations; they cannot
close Linux workload or recovery gates.

The frozen Linux executable also passes version/help and positive/negative
preflight on the designated VPS. The explicit trigger is reported as
`storage.wal: not_checked`; invalid configuration is refused. The absent
basedir stays absent, all complete held-node metadata is unchanged, and no
runtime executable is replaced or started. The initial coordinator has a
retained Python quoting failure before any remote statement; the corrected
coordinator and parsed remote source pass. These are CLI checks, not storage
startup, restore or memory qualification.

The VPS historical experiment with the same 64-MiB trigger passes both
checked/full boundary runs and is replaying the complete required range.
It uses frozen preceding protocol/storage libraries with the explicit native
library option, independently of the new YAML wiring. This remains component
evidence. Its first monitor fails on a Python variable-name collision and
cleans up its child; the corrected monitor retains resource/source guards.
See the [historical report](historical-checkpoint-eligibility.md).

## Remaining work

Native source inspection, the affected documentation handoff and latest-binary
actual-VPS CLI checks are complete. The One manual passes strict MkDocs; app
integration remains separate. Full historical replay and the complete strict-budget
restore still need separate proof, followed by sustained mixed and producer
workloads. A soft WAL trigger cannot establish whole-server capacity by itself.

Owned evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/wal-retention-20261016/`.
