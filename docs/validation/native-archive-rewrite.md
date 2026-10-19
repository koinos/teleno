# Native archive rewrite and backup qualification

Date: 2026-10-16. Status: **native rewrite, complete preservation, public backup
and full reference-server restore pass; strict 40-GB qualification remains**.
Issue #9 and LR2 remain active.

## Change and preservation

The earlier [archive experiment](lossless-archive-storage-experiment.md) showed
useful lossless savings, but its diagnostic writer was not the native export
path. Ordinary RocksDB compaction can skip bottommost tables even after the
configured compression policy changes. The experimental tables also retain
the development host identifier in a physical table property.

Native `--compact-db --compact-rewrite` now explicitly rewrites existing
bottommost tables using the configured policy. Exactly one selection is
required: `--all` or valid `--compact-cf` names. Other maintenance actions cannot
be mixed with it. Invalid combinations and a missing existing shared DB are
refused with exit 2 before lease/basedir initialization. Ordinary compaction
retains its previous behavior. The manager uses RocksDB's `kForceOptimized` to
avoid rewriting newly generated tables a second time in the same pass.

New native tables omit `db_host_id`. Existing tables remain readable; there is
no automatic rewrite. Record encoding, comparators, validation, WAL, durability,
history retention and the 10-GiB restore reserve are unchanged. This is offline
maintenance on an isolated copy, not an in-place operation on the small VPS.

The original pinned public backup remains preserved: 497 files,
29,721,611,361 bytes, selected ID
`20261015T191906Z-ms-1792091946641-files-494`.
Its manifest/inventory hashes and full structural history evidence remain in
the preceding report. A fresh native working copy starts from that original,
not the experimental compact database. Immutable SSTs are initially linked on
the same external volume; mutable control files are copied. All node services,
backup scheduling, network listeners and production are disabled for export.

## Exact development inputs

Branch: `codex/lr2-memory-hardening`; base commit
`d3cfb94455da4a472f651eb4239abbc289e74225`; native identity
`1.3.0-dev.0+d3cfb94455da-dirty`. This checkpoint changes five native paths
from the preceding policy checkpoint: main CLI, storage manager/header,
storage tests and CLI tests. The frozen manifest covers 304 native inputs:
`c3b1cc4e2dad577874d5e0bb0afd8d303a89a5afe0b1d57ba11d016cd99fdb10`.

Executable SHA-256:

- Mac: `0b2c657a8e4f43c812815fd628b8a28247c4daa36bc4c702812045d34e50858c`.
- Linux builder/reference VPS:
  `a18c6e17038907d7e1d8a7fa0eb7ec815c3f2c71a37483c8d0d4342e4ba48f5f`.

There is no new integration commit, release or Koinos One submodule pin.

## Scoped validation

- The storage fixture ingests 4,096 binary records into a bottommost table
  carrying a synthetic host marker and the old compression policy. Ordinary
  compaction leaves that table unchanged. Explicit rewriting replaces it with
  Zstd level 9 / 65,536-byte dictionary tables without the host property.
  An ordinary default-policy reader reopens and compares every key/value.
- The existing 12,000-record compression fixture also checks empty host
  properties. Actual CLI controls reject missing mode/selection, conflicting
  selectors, invalid families, mixed maintenance and nonexistent DBs without
  filesystem changes. Preflight refuses the new maintenance option.
- Mac all-target build passes. The first full suite passes 58/60 while the
  large external rewrite is active; persistence/publication tests time out.
  Their failed logs remain. Temporarily stopping only the owned rewrite process
  removes disk contention; both unchanged test executables pass on retry in
  2.65 seconds. The same rewrite process is then resumed. All 60 unique tests
  pass across these retained attempts; the initial suite is not described as
  an uninterrupted 60/60 pass.
- Larger Linux builder all targets, version/help and all 60 tests pass in
  exhaustive 58/1/1 batches. Full fixture archives are externally verified
  before retiring only the owned remote duplicates. The installed observer
  publisher retains its config/image and fourteen running containers.
- The actual reference VPS passes 12 native command invocations, including
  storage/reopen fixtures, actual preflight/invalid CLI controls, explicit
  single-family/all-family rewrites and ordinary compaction. Component peak
  RSS is 52,346,880 bytes; concurrent sampled host `MemAvailable` is at least
  3,418,382,336 bytes. Swap remains zero and the OOM counter does not change.
  These component results do not establish whole-node memory or production.

## Complete native rewrite

The actual native command completes successfully against the isolated original
copy, with 64-KiB block tables, Zstd level 9 / 64-KiB dictionaries, one background
job and finite development cache/memtable settings. Physical inspection finds
378 SSTs, including 372 block SSTs and 45,076,063 block entries. All new table
host fields are empty and all block tables report the selected policy.
The working database, including its current logs/control files, occupies
24,803,500,585 bytes. This is not yet the final backup inventory size.

Observed Mac wall time is 2,282.28 seconds, including a 630.14-second scheduling
pause for the unchanged test retry. Subtracting that pause gives 1,652.14
seconds of active elapsed time. Sampled owned-process peak RSS is 86,753,280
bytes. Disk contention and macOS make these development diagnostics, not Linux
rewrite/whole-node capacity measurements. Original input size, mtime, mode and
inode remain unchanged throughout.

The real native `--backup-create-local` completes in 2,064.17 seconds on Mac,
with sampled owned-process peak RSS 38,404,096 bytes. Its normal checkpoint/
genesis checks, copied object hashes and durable COMPLETE/latest publication
succeed. The concrete native backup is
`20261016T115052Z-ms-1792151452613-files-382`: 385 files/objects and
24,802,854,237 payload bytes, including 24,802,203,337 DB bytes. This is
4,918,756,917 bytes (**16.550%**) smaller than the original DB. Its durable
revision, state ID/root, mainnet chain ID and highest stored height 40,236,830
match the pinned source. Historical replay remains explicitly false.

A fresh server sample has 36,117,983,232 available bytes. Applying the unchanged
64-KiB allocation quantum, 256-MiB metadata/validation allowances and 10-GiB
reserve to this native inventory gives a tentative recommendation of
35,812,999,168 bytes, with 304,984,064 bytes beyond that recommendation.
This is the earlier root-filesystem sizing calculation, not a strict physical
whole-device budget. Actual native publication/preflight/restore subsequently
pass below, while the corrected whole-device accounting still fails.

A read-only verification view does
link only those actual backup SST objects and copy the small control/runtime
files, avoiding another full 25-GB external allocation. The comparator has
independent Python digest fixtures and rejects wrong/missing blocks, wrong
operational metadata, extra families and corrupt SST checksums.

The complete raw comparison passes every key/value/count across all nine
families: 46,458,363 visible records, including all 45,076,063 block/receipt
records and 917,316 state objects. Exactly two operational values differ:
the expected native `layout.created_by` and `layout.basedir`. All other raw
keys/values, counts and family digests agree with the original. The block digest
is `c5953863ebfa9909adda21b26ace7e88bc1d3c63766259d7990a42205a54856b`.
It binds the prior full header/canonical continuity proof to this native backup:
all 40,236,770 consecutive canonical heights from genesis and all 4,839,293
other selected records are preserved. This proves preservation and structural
continuity, separately from signatures/historical execution.

The terminal verifier exits 0 after 3,612.64 seconds including object hashing;
checksum/pointwise comparison takes 2,956.73 seconds. The child's time report
records 118.42 user / 28.79 system CPU seconds. Sampled owned-process peak RSS
is 36,405,248 bytes. These are Mac component diagnostics, not whole-node/Linux
capacity. Original input and native object size/mtime/mode/inode remain
unchanged after the complete read.

The first verification coordinator assumes one object shard instead of the
native first-two/next-two/full-hash layout and fails before creating its view
or comparing data. That helper failure, source and handle are preserved. The
corrected coordinator verifies SHA-256 and size for all 385 objects of the
same completed native backup; it does not restart the rewrite/backup or change
any native object. Local manifest SHA-256 is
`f6bbcc9799b67968715ff48aee968aa39beeb76ed6df08771cc91f1ad5593b9d`;
inventory SHA-256 is
`edb3579be0bc01561aa1cd084e626d586f66a27d1e842daf3d9668a6489a841c`.
These are local snapshot digests, not prospective public-promotion digests.
Post-backup inspection finds 379 SSTs, with empty host properties throughout
and 45,076,063 physical block entries. This describes physical properties,
separately from the required complete logical comparison.
Both original and native-backup RocksDB checksum scans and the complete raw
comparison pass against the read-only view assembled from actual backup
objects. That terminal equality proof releases only the next publication/
baseline-restore gates, not the complete issue #9 goal.

## Evidence and next gates

Owned generated work is under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/native-export-k3ky3m61/`.
It retains source/binary identities, scheduling pause/resume, original metadata,
native logs, tests, exact failures/retries, table properties, verified remote
archives and the backup/verification coordinator handles. The 70-GiB external
free-space floor is unchanged. No operational data or original source is
retired to obtain the savings.

After full backup equality passes, use the existing native public-promotion
machinery with a reviewed observer config in an isolated HTTPS qualification
namespace. Preserve the stable public latest, host-key checking, source
identity and signature policy. A local snapshot alone is not an eligible
public source; do not manufacture publication metadata.

The native promotion API creates sanitized config and public snapshot metadata;
it expects a pre-provisioned object tree and does not upload every payload
object itself. For this isolated qualification source, transfer the 384 verified
database/public-runtime objects first, excluding the native maintenance config.
Use pinned-key SFTP, fsync, per-object temporary names/rename and remote size
checks. Then invoke native promotion to create the public config, manifest,
inventory, COMPLETE and latest. The existing publisher's private/shared object
layout and stable latest are untouched. No private snapshot is exposed to make
the qualification path work.

An actual small SFTP fsync/rename/stat transfer and HTTPS retrieval through the
content-addressed object route pass. The seed capacity check reserves room for
the complete candidate, another original-size publisher backup and 10 GiB.
The proof-gated coordinator waits for successful terminal raw comparison before
transferring any backup payload. After native promotion it must verify HTTPS
metadata, all public object sizes and the exact sanitized config, then provide
the concrete source/digests to the baseline restore coordinator. Target-side
native stage still has to hash every received payload body.

The actual native `--backup-list-remote` also succeeds with the same compiled
libssh backend, existing credential and strict host pins. It creates no DB/SSTs
and makes no seed mutation. Its byte-capacity observation agrees with OpenSSH
SFTP's KiB report, independently checking the transfer planner's units.

The baseline coordinator is connected behind successful terminal publication
and HTTPS verification. It uses detached, durable remote phase supervisors,
requires the unchanged native recommended-space check, and performs one
controlled stage interruption followed by same-source resume and explicit
observer-held activation. Final swap/OOM/memory/disk checks and timed owned-child
peaks supplement live native HWM samples. Failed or ambiguous observations do
not relaunch a remote operation. The controller stops before node startup;
synchronization, recovery and producer measurements remain separate gates.

The isolated qualification publication is complete. All 384 non-config objects
are transferred before normal native promotion creates the reviewed public
config and metadata. HTTPS metadata, all 385 object sizes and the exact public
config body pass verification; stable public latest remains byte-identical.
The published backup is `20261016T115052Z-ms-1792151452613-files-382`, with
24,802,854,821 payload bytes. Manifest SHA-256 is
`afa3030eca62b5bc913195354cf81312bb3bc2bbe813cf3c456d829444a10e26`;
inventory SHA-256 is
`0993cfa6581d038b43349d1cf1d18cbd6df77bb20c5a682bf54aaa750ace0282`.

The actual replacement VPS passes native preflight with the unchanged 10-GiB
reserve. A controlled SIGINT stops stage after 1,076,281,824 payload bytes,
retaining 18 complete files. Resume succeeds in 920.96 seconds, reuses those
18 files with unchanged inodes/sizes/mtimes, and checks all received object
bodies. The sole staged payload occupies 24,805,289,984 allocated bytes.
The ext4 root capacity is 39,956,590,592 bytes; available space after resume is
11,312,304,128 bytes. These root-wide values include root OS/runtime files but
exclude physical device overhead and boot partitions. They do not prove the
strict whole-server budget.

Offline validation and observer-held activation complete successfully in
3,608.699 seconds. Three full read-only database checks pass, including
post-rename reopen, with the expected identity, 46,458,363 records and nine
families. The same DB directory inode is retained. Activation peak RSS is
49,008,640 bytes, minimum concurrent `MemAvailable` is 3,418,607,616 bytes and
the unchanged native reserve passes. The complete
[reference VPS report](reference-vps-observer.md) records the restore phases,
substantial validation read amplification and provisional observer results.

The provider's actual device is 40,961,572,864 bytes. Conservative whole-device
accounting, including boot/formatting/reserved space and the unchanged 10-GiB
reserve, exceeds the strict 40,000,000,000-byte budget by 488,845,312 bytes
after restore and by 628,682,752 bytes at the later observer peak. Strict
40-GB acceptance remains failed. Further lossless savings are required.

The reviewed candidate observer config retains mainnet
verification, the public peers and original optional services, uses one worker
per chain/P2P/RPC component, a 128-MiB shared cache, 64-MiB aggregate memtable
flush threshold and the selected block compression policy. These are candidate
settings for measurement, not RSS ceilings. Runtime RPC is loopback-only and
production remains disabled. Native/Koinos One documentation passes strict
manual build, rendered-text checks and affected local-link/diff checks.

Two guarded 900-second observer runs and a clean restart pass. The second
catches recent head; two captured heights/roots match the existing legacy
reference. These are short runtime observations. Strict whole-server disk,
24-hour/seven-day operation, abrupt recovery, mixed workloads and controlled
producer measurements remain required. Historical execution/signatures and
integrated lifecycle qualification remain open. No producer is activated.

The subsequent [direct-read experiment](direct-restore-read-buffer.md) reduces
small transport writes and CPU in five paired Linux-builder and five paired
reference-VPS samples. Its new
native source manifest is separate; this full restore keeps the preceding
frozen executable and source identity throughout.
