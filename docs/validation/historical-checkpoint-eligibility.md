# Historical checkpoint eligibility

Updated: 2026-10-17, Europe/Berlin. Status: **complete receipt audit, native
VPS checked replay and boundary comparisons pass; full differential H1
qualification remains open**.

The current H1 gate requires a fresh state-delta audit and differential replay
from before height 30,504,202 through beyond 32,789,378. Older reports and
directory labels do not prove that a retained database still has an eligible
starting state.

A stopped retained unified database is opened read-only with checksum checks,
a quiet logger and absolute-consistency WAL recovery. Its file metadata is
identical before and after the probe. The probe also passes a positive control
against the completely verified native backup at revision 40,236,770.

The retained candidate reports mainnet chain ID
`0x1220592bf18654fd07fdf5d500cde3e8402ecf7f81fa5dde8f14527b08bba8805f48`,
revision **30,504,141**, block ID
`0x1220631e4951636e58753899595f661a9b9904c9aca4855e1fa167e540bf9c8a8447`
and state root
`0x12207a65e9afa67a7c0e1c6c16fbe889164c940d6a50b4f606ae0b3b3cde2701bf41`.
The checkpoint block/header match the state metadata. Its historical receipt
has no state-root field; this absence is retained explicitly. WAL input is
150 bytes, replayed only in memory by the read-only open.

The independent, already running legacy node confirms that canonical block ID
and the successor's previous-state root. Its observed head/LIB are
40,263,541 / 40,263,481, beyond the compared height. The comparison verifies
the same pinned running legacy image identities used in the VPS report and
uses only the three whitelisted read RPCs. No service or chain mutation occurs.

A separate standalone checkpoint whose directory name contains `30504141`
actually reports revision **32,789,319**. It is preserved and excluded as the
starting checkpoint for this full-range run. Its label is not accepted as
evidence of its current state.

## Preserved diagnostic failures

The first probe fails compilation before any database open because its flags
disable RTTI while a borrowed crypto header requires it. A smaller diagnostic
removes that dependency and compiles. Its first positive control then refuses
a revision/header mismatch because the diagnostic decodes an integer using
the wrong byte order. The correction uses the pinned Koinos big-endian
serialization, and both positive control and candidate pass. Native code and
stored data are unchanged.

The first independent-reference launcher fails SSH command quoting before the
Python helper can make an RPC call. Its corrected, quoted invocation obtains
the match above. All failure logs and scripts remain alongside the successful
records.

## Isolated state copy and current native boundary checks

An isolated standalone state fixture contains all **852,450** source state
objects and all **five** metadata records. Both column families are compared
key/value byte for byte after synchronous writes and flushes. The framed raw
SHA-256 digests are
`4da83e986765bdab6244adf0cc532df13464b953af00a6731a0673abbb5618ec`
and
`cafc958f4a60c7f7d5acfa403ff501ca45c0e5c624769f98760e4aafee4a3cbc`.
Its 48,200,505 file bytes encode 151,834,674 raw key/value bytes. All source
file metadata remains unchanged. The fixture is not a restorable full node;
the complete source database and history remain preserved.

Fresh current native controller/indexer runs start at 30,504,141 and reach
30,504,203 with one worker. Checked replay uses the expected single fallback
at 30,504,202; full execution uses none. Both produce block
`0x12208f0ab558a8d90c0abfd1075bc602b96658df29d088f075a396e7fcf21735cc5a`
and root
`0x1220f73169eb40a9977cefbf0e64e95a07f12a1a4050bed0e765ed84e63d9606f088`,
the same chain ID and six selected contract-read hashes. Source write and
broadcast attempts are zero. The current 306-input native source manifest is
`ca0a6b4e890474528e385c3149f76250d38787d2cb41093059858a9d97290e7b`.
These are Mac correctness diagnostics, not Linux resource qualification.

The fresh direct receipt audit completes all **2,285,238** blocks from
30,504,142 through **32,789,379**: 19,658,669 delta entries, comprising
19,061,953 puts and 596,716 removes. It classifies 2,285,234 ordinary blocks,
one known replayable tombstone, one incomplete fallback and one consensus scar.
The remaining terminal tip is independently anchored below; the reverse audit
does not classify it against a successor outside the selected range.
Duplicate keys, constant mismatches and unexplained cases are zero. Historical
receipt state-root fields are empty throughout this range; that absence is
retained. This is a complete receipt audit, not complete historical execution.

An independent legacy probe anchors that endpoint to
block
`0x1220170b1299234fba884a41a307a316d79501ebae8e75a9c696b537b82f2de05ded`
and root
`0x12209ad9032cad3ee78fd78cdf11a6e67df5dcea18d2996f36547789a9330206c64a`.
Reference LIB 40,263,645 is beyond it. The 23 selected journal buckets and
metadata are rehashed against their fully preserved migration inventory before
and after the new audit. A fresh chain v1.5.2 / state DB v1.2.1 reference build
uses commits `0ae99eced8b585c4145424e9c2a28f667796cc66` and
`3a1c904e61afbff59e167f50175519e68046e090`. Its checked and full-execution
62-block boundary results match the current native chain ID, height, block ID,
state root, six contract-read hashes and fallback count. The reference helper
does not emit source-write/broadcast counters; no numeric zero is inferred.
The tagged Mac reference checked replay now completes all 2,285,238 blocks,
reporting 19,920.7 seconds including documented pauses and concurrent disk work.
Its final height, block ID, root, chain ID, six contract-read hashes and fallback
count exactly match the complete Linux native replay below. Nine common fields
also cover mode and starting/target heights. This is a complete checked-range
comparison, not full-range VM execution. The separate Mac native diagnostic
stops at its original six-hour deadline, exit -15; its partial result is not a
pass. The Linux replay supplies the native completed-range result. Different
platforms/storage prohibit a performance ranking from these elapsed times.

The tagged reference's durable checkpoint at **32,789,319** is independently
anchored before two fresh isolated copies fully execute the next **62 blocks**,
including the second consensus scar at **32,789,378**. Native and tagged
reference both reach **32,789,381**, matching all nine common result fields,
including six contract reads and zero fallbacks. The independently checked
terminal root is
`0x1220ec9d0af4922d335d3e3cfe64136a1dfade621d65a548d501289b9cafa7bfff71`.
Native source-write and broadcast attempts are zero; the reference does not
emit these counters. Mac wall times are 3.548 / 2.020 seconds and do not qualify
Linux resource use or establish a performance comparison.

Both terminal checked-replay states and their logs are preserved with every
archive member verified: 24 reference members / 50,872,858 archive bytes, and
22 partial-native members / 50,660,893 bytes. The second full-VM boundary's
complete 49-member namespace is separately verified in a 101,696,198-byte
archive. Source metadata remains unchanged, and none of these local states is
retired. Full VM execution now covers both known boundary windows; execution
of every block between them remains required.

## Actual VPS replay and storage-profile correction

The initial VPS helper passes both boundary modes but its complete checked
replay stops at the unchanged 10-GiB root free-space guard after 1,253.970
seconds, exit -15. Sampled HWM is 328,130,560 bytes and concurrent host
`MemAvailable` remains at least 3,173,392,384 bytes, with no swap/OOM change.
The complete read-only source retains identical file metadata. This is a
failed range run. Its 138-member namespace is preserved and member-verified
in `baseline-historical-r0.tar.gz` before its owned remote copies are retired.
The stale overall `running` label is superseded by the phase exit/abort record.

The standalone state backend uses uncompressed 64-MiB write buffers without
an aggregate write-buffer cap. The actual node profile uses Zstd, 8-MiB write
buffers and a 64-MiB aggregate cap. A separate diagnostic overlay therefore
uses the real shared `RocksDBManager` and borrowed state backend, with that
profile, a 128-MiB cache and one background job. No repository source or
history-retention policy changes for this overlay.

The new unified fixture raw-compares all 852,450 state objects and five metadata
records against the first exact fixture. All source historical blocks/receipts
stay present in the stopped full-node database. The isolated state fixture is
not described as a complete node, backup or capacity solution.

Both overlay boundary modes pass on Mac and the designated VPS, matching all
eight native comparison fields. The new VPS full checked range also stops at
the unchanged 10-GiB guard, after 680.841 seconds, exit -15. Sampled HWM is
210,767,872 bytes; concurrent available memory stays at least 3,267,563,520
bytes, without swap/OOM change. Complete source metadata remains unchanged.
Its 187-member namespace is fully preserved in a verified 196,704,663-byte
archive before retirement. No full-range or whole-node pass is claimed.

A read-only physical sample finds 299,394,207 WAL bytes in 45 files versus
46,657,290 SST bytes. The actual saved options show
`max_total_wal_size=0`, independently of the 64-MiB aggregate memtable setting.
The pinned RocksDB 8.8.1 header defines zero as an automatic WAL flush threshold
of four times the sum of each family's write-buffer size times buffer count:
576 MiB for these nine 8-MiB/two-buffer families. This is a soft flush trigger,
not a hard disk ceiling. WAL retention must be measured separately from
memtables. Five mechanism pairs on each Linux host now validate an explicit
64-MiB trigger, with every raw record/checksum and recovery result preserved;
see the [WAL report](wal-retention-budget.md). No WAL is manually removed.

The corrected historical experiment applies that trigger to the same native
shared backend. Both boundary modes pass, and the checked VPS replay completes
all **2,285,238 blocks**, reaching **32,789,379** in **7,735.086 seconds**. Its
final block ID and state root match the independent legacy endpoint above.
Source write and broadcast attempts are zero; complete source metadata remains
unchanged. Kernel peak child RSS is **488,472,576 bytes**; sampled process-group
HWM is **490,307,584 bytes**. Concurrent host available memory stays at least
**3,004,477,440 bytes**, root available space at least **10,956,881,920 bytes**,
with zero configured swap and unchanged OOM/swap counters.

Process CPU is **4,342.260 seconds**. GNU time reports **40,385,802,240 physical
input bytes** and **17,391,194,112 physical output bytes** (512-byte block
units). These costs include the historical scratch-state workload and WAL
reclamation; this is not a before/after I/O improvement claim or a complete
node workload. The checked range uses receipt deltas with one fallback; full
VM execution is established for the two 62-block boundary windows above.
The complete checked range matches the tagged reference; full-range VM and
complete receipt/resource differential qualification remain open.

The complete 172-member namespace is preserved and every member verified in
the **96,107,946-byte** `baseline-r2-complete.tar.gz`. Fresh process-group,
descriptor, mapping and working-directory checks establish that its source
users have closed before subsequent tests or cleanup. The first monitor's
Python variable collision and the local archive verifier's older-Python API
failure remain recorded; the finished archive is reverified with Python 3.12
before it authorizes any scoped cleanup.

This experimental helper uses the frozen `b3469afb...` libraries and an explicit
linked-library option; the later YAML wiring has separate build/tests. Helper
source SHA-256 is
`5649983c12f3cb82c3998e568e1e4e51ef702dcd081e5da539b834e4a06e8c39`,
and executable SHA-256 is
`871876a2d34e1f65a7b9b012400c2400a51cecbdafccf644b9662cc277b691ed`.
Overlay source SHA-256 is
`73513dbbae8bc195214a05e6709911a3e674338c388aeff7f066e2c9bf5e7282`;
its Linux executable SHA-256 is
`979294ae83d835168c144e141c5d84e1a7bbbe76ba8137285afe9da1bffdf000`.

## Frozen Linux tagged reference

A fresh Linux build uses the same exact tagged chain/state git archives and
unchanged reference harness as the Mac reference. All tagged source-file hashes
match independently. Configure/build pass in 1.027 / 317.193 seconds on the
authorized miner, with sampled owned build peak RSS 1,527,263,232 bytes.
Existing native pinned dependencies are read-only inputs; no installed service
changes. Reference executable SHA-256:
`457f6afd2351c998cf3d42d3cc1cfe90e928aa83a23ccd74696a870bc79f7765`
(24,723,928 bytes).

The complete 387-member namespace is preserved in an 18,167,715-byte archive,
with every member verified. This build prepares an additional independent
correctness check; it does not itself complete a replay, qualify a producer
or establish whole-node/Linux resource bounds. The Mac checked-range result
and both full-VM boundary comparisons pass as described above; full-range VM
execution remains pending.

## Linux full-VM receipt comparison

The [read-only source bridge](historical-source-bridge.md) passes its fresh
protobuf/capacity/anchor controls and a paired Linux 62-block full-VM boundary.
Every complete generated receipt, computed head/root and LIB matches the
unchanged tagged reference. Its first 50,000-block comparison passes every trace
row and an independent canonical endpoint check. Matching durable checkpoint
metadata permits a second isolated pair with a verified 60-block overlap;
that next pair is running. The complete source remains on My Passport.
This direct-controller diagnostic supplements the native indexer evidence;
the full historical range remains incomplete.

## Scope and next checks

This proves an independent stable starting anchor, an exact isolated copy and
current native boundary behavior. It does not close full historical execution,
receipt/resource parity or the H1 range. Current full-range and tagged-reference
results remain required. Full-history node inputs remain intact; the state
fixture introduces no history-retention policy.

Owned evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/h1-eligibility-20261016/`.
