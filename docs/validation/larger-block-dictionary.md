# Optional 128-KiB block dictionary

Date: 2026-10-16. Status: **all 61 tests pass for the frozen dictionary source
on Mac/Linux; native rewrite completes; raw backup and whole-server
qualification remain open**.

## Reason and experimental scope

The [reference VPS](reference-vps-observer.md) still exceeds the strict
40-decimal-GB whole-device budget by about 629 MB with its unchanged recovery
reserve. Further lossless savings are required. A larger dictionary is an
optional encoding policy for complete block/receipt records; it changes no
logical keys, protobuf values, history retention or consensus checks.

The earlier eight single-SST screens compare block sizes and Zstd levels.
Ten further screens vary dictionary sizes. Several larger dictionaries increase
file size, including 512-KiB dictionaries in this sample. The smallest screened
variant uses 64-KiB blocks, Zstd level 15 and a 128-KiB dictionary. It is not
selected as a whole-node policy from screening alone.

## Five repeated Linux comparisons

Five ordered triplets compare that candidate, the current 64-KiB / level-9 /
64-KiB control and an already-supported 128-KiB / level-15 / 64-KiB alternative.
Order varies explicitly between triplets. All use the same immutable native
backup SST: 66,533,451 bytes, SHA-256
`752dd3a2a7034770889c90ee100573b7fbc5ff279ea462da7f69342ddfe5edbb`.
Each of the 15 rewrites verifies all 121,814 block/receipt records pointwise,
their raw digest, count and output checksum. The input remains byte-identical.
Diagnostic SSTs retain diagnostic writer metadata and are not native backups.

| Policy: block / level / dictionary | Output bytes, every trial | Median process CPU seconds, including verification | Maximum process RSS bytes |
| --- | --- | --- | --- |
| 64 KiB / 9 / 64 KiB | 66,685,864 | 6.00 | 45,621,248 |
| 64 KiB / 15 / 128 KiB | 64,970,574 | 43.01 | 70,983,680 |
| 128 KiB / 15 / 64 KiB | 65,082,101 | 37.69 | 79,376,384 |

The candidate is **2.572% smaller than the diagnostic control**, with a median
paired process CPU ratio of **8.850**. The existing alternative is 2.405%
smaller, with median paired CPU ratio 4.842. Ratios are calculated within each
triplet; they differ from ratios of the aggregate medians above. The substantial
rewrite cost and higher component RSS are explicit tradeoffs. These figures
do not predict complete-database savings or native node memory.

Two deterministic 2,048-query passes validate exact key/value results with the
64-MiB data cache both disabled and enabled. Each process requests a file-local
drop hint for its generated SST; this is not a guarantee of physically cold
media and does not flush shared host caches.

| Policy | Median first-pass physical read bytes, cache disabled | Median warm-pass CPU seconds, cache disabled | Median warm p95 microseconds, cache disabled |
| --- | --- | --- | --- |
| Control | 35,045,376 | 0.085715 | 49.192 |
| Candidate | 34,115,584 | 0.092853 | 50.230 |
| Existing alternative | 49,561,600 | 0.138358 | 75.541 |

Warm-pass physical reads are zero in these samples. The candidate's median
paired warm CPU ratio is 1.008 with data caching disabled and 1.010 enabled;
warm p95 ratios are 1.001 and 1.026. Results vary between trials; they establish
no read-CPU improvement over the control. The existing alternative's paired
warm CPU ratios are 1.614 and 1.654. Whole-node reads, negative queries, RPC
tails, background compaction and producer timing require separate measurements.

The complete screening archive retains the immutable input, smallest physical
sample and all measurements. Other temporary screened outputs are retired only
after complete equality/checksum/lookup validation and durable result recording.
All repeated-comparison outputs are likewise disposable derived copies; their
exact size/hash and validation results remain. Both completed experiment
archives are independently verified against every retained member hash.

## Native change and validation

Native config now also accepts exactly `131072` for
`rocksdb.blocks-zstd-dictionary-bytes`. Existing values and zero defaults remain
unchanged. Zstd is still required for nonzero Zstd options. Training remains
limited to 1 MiB and dictionary buffering to 8 MiB per table builder, with
other allocator/cache/active-reader memory additional. This is not an aggregate
RSS ceiling. Opening existing files does not rewrite them automatically.

Frozen native manifest, 306 inputs:
`ca0a6b4e890474528e385c3149f76250d38787d2cb41093059858a9d97290e7b`.
Four paths differ from preceding RSS manifest
`c073ab12836d01ddd761e8146ed829792fee0f92b77c622059cf798951208c3b`:
config validation, config fixtures, native preflight fixtures and storage
fixtures. No default, codec implementation, training-buffer limit or runtime
deployment changes.

The storage fixture writes 12,000 binary values with the old policy, opens the
same tables under the larger policy without an automatic rewrite, then
explicitly rewrites them. Native table properties and full checksums pass;
an ordinary default reader compares every key/value afterward. Config and
preflight fixtures accept the new bounded value, retain runtime codec checks,
and refuse `131073`/`262144` and existing invalid values without creating a DB.

All-target builds, the three focused checks, version/help and all 61 CTests
pass on Mac and the larger Linux builder. Mac's guarded full suite takes
201.211 seconds. Linux's exhaustive 59/1/1 batches take 132.305, 2.274 and
12.026 seconds. Every fixture archive is verified before owned duplicate
retirement. The final synthetic comparison checks retained-pair headroom;
its native reserve remains unchanged.

Frozen executable SHA-256 values are
`76c22e8b5fca74d52fde6e9449eb8e2fe37fba71bc26fc4b37f654902ffbedb4`
(Mac) and
`1c2981e42604b5a68db1b92a2916181313ebb8a830b3597780ad09b47925731e`
(Linux), both `1.3.0-dev.0+d3cfb94455da-dirty`. The completed one-hour observer
assessment uses the preceding RSS executable and its earlier dictionary policy;
it cannot validate this new policy.

## Additional corpus and full native experiment

Three native block SSTs undergo nine further complete pointwise/checksum
comparisons, with 335,466 source rows across the three inputs. Level 15 /
128-KiB dictionaries reduce diagnostic control sizes by 2.572%, 3.408% and
2.812%. Exploratory level-19 results save only another 0.10%–0.12% of the
level-15 output, with variable additional rewrite cost. Those are single
screens per input. The full experiment retains the repeatedly measured
level-15 candidate.

A fresh isolated copy starts from the actual fully verified native backup.
Its 384 non-config files have independent APFS COW inodes; an append-to-clone
pilot verifies unchanged original bytes/metadata. This is development placement
to avoid another initial 25-GB allocation, not Linux/ext4 savings. Every service,
production, listener, scheduler and remote backup remains disabled; one
background job and finite cache/memtable settings remain. Native preflight passes.

The native full rewrite completes with the frozen Mac executable,
64-KiB blocks, level 15 and a 128-KiB dictionary. Its monitor preserves the
70-GiB external free-space floor, bounds owned RSS/time and checks original
input metadata. A first helper fails before native startup because its copied
input inventory is missing; the failure/script remain. The corrected helper
uses the unchanged inventory and starts one native process. It exits zero
without a guard abort after 6,420.273 seconds, including a 766.481-second
documented verification pause and concurrent development work. Sampled owned
peak RSS is 161,234,944 bytes. These are Mac diagnostic observations, not Linux
rewrite throughput or whole-node memory qualification.

The resulting working DB contains 24,114,745,582 file bytes, 687,457,755 fewer
than the prior actual native backup DB, about 2.772%. Native table properties
report 376 SSTs, including 370 block tables with 45,076,063 entries. All block
tables report the selected level-15 / 128-KiB policy; all table writer-host
fields are empty. Physical properties and size alone do not prove raw equality.

The strict read-only comparison refuses the writable working directory's
zero-byte WAL before validating records. Its failure remains preserved; no WAL
is deleted and no corruption or successful comparison is inferred. The normal
native checkpoint/backup API completes successfully in 1,738.707 seconds,
including the quiet-test pause. Its real snapshot has 383 files/objects,
24,114,178,238 payload bytes, 24,113,527,301 database bytes and zero WAL bytes.
Checkpoint identity is verified. Independent verification completes for all
383 object hashes and physical checksums. The unchanged pointwise comparator
checks all 46,458,363 raw records across nine column families, including all
45,076,063 block/receipt records with identical framed digest
`c5953863ebfa9909adda21b26ace7e88bc1d3c63766259d7990a42205a54856b`.
Only one permitted operational storage-metadata value differs; no chain data
is excluded. Original input and backup-object metadata remain unchanged.
Verification takes 4,156.522 seconds, including a documented 324.975-second
Mac build/test pause; sampled verifier peak RSS is 37,437,440 bytes. The DB
saves 688,676,036 bytes against the preceding actual native backup. These are
lossless Mac backup results, not a Linux restore-capacity or runtime pass.
Prepopulated immutable objects
use independent APFS COW inodes and are rehashed by the native API. This avoids
another development allocation; it is not Linux restore-capacity evidence.

## Native public qualification

The old generated qualification namespace is fully inventoried, all 385 payload
bodies are rehashed over HTTPS against unchanged off-host copies, and metadata
and the two original transport probes are preserved. Only those 393 known files
and their empty directories are retired. This reclaims 24,806,154,240 seed
available bytes; stable public latest is byte-for-byte unchanged.

The normal native public-promotion API publishes the new completed backup into
the separate `qualification/dictionary-policy-20261016` namespace. Its helper
uses frozen B3 source; the backup's producing binary remains CA0. Public metadata,
all 383 HTTPS object sizes and the sanitized observer configuration hash pass.
Public payload size is 24,114,178,822 bytes. Manifest/inventory SHA-256 values:

- `228a95506dceeff4a35b42cbd9af1709eb66906cde6346d615ed87984318ab5e`.
- `7a4c755fe30610281fe8059707b5e9b8d114f944faa52c9c3aa56e169b5c55c5`.

Publication completes in 1,066.275 seconds. Seed available space afterward is
46,149,042,176 bytes. Throughout transfer, the remaining payload, another
complete original-size publisher backup and the unchanged 10-GiB seed reserve
stay budgeted. Stable latest and the installed publisher remain unchanged.
Source signatures remain explicitly absent under the unchanged policy. Native
staging now verifies the complete public payload on the target, with controlled
interruption and same-source resume. Full validation and held activation check
all 46,458,363 records and preserve the DB inode. Independent whole-server peak
accounting is 39,760,650,240 bytes including OS/runtime, temporary work and the
unchanged 10-GiB reserve. See the [actual restore report](reference-vps-observer.md)
for the exact identities and scope. The first observer is running; sustained
runtime growth, recovery, historical VM and producer qualification remain open.

## Five pairs on the reference VPS

The designated 2-vCPU / 4-decimal-GB Linux VPS completes five alternating
control/candidate pairs without an observer running. Every rewrite compares all
121,814 raw records and checksums; both cache modes repeat the same 2,048
positive lookups twice. The immutable input and the stopped node's `CURRENT`
identity remain unchanged. All generated output SSTs are retired only after
their full comparison results and hashes are durable.

Output sizes are identical to the Linux-builder results: the candidate saves
2.572%. Median paired rewrite process CPU is **5.722 times** the control.
First-pass lookup CPU ratios are 1.001 with the cache disabled and 1.008 with
it enabled. Warm-pass CPU ratios are **1.122 / 1.136**, and p95 latency ratios
are **1.111 / 1.199**. These are component tradeoffs, including a measured
warm-read penalty; they establish no sustained whole-node improvement.

The largest component RSS is 90,652,672 bytes. Concurrent host `MemAvailable`
never falls below 3,384,213,504 bytes; swap and OOM counters remain unchanged.
The stopped complete node remains present throughout. The strict 40-GB budget
still fails, and these temporary component files are included in that usage.
The complete 100-member namespace is preserved in an independently verified
85,946,794-byte external archive, `baseline-dictionary-mechanism.tar.gz`.

## Remaining work

Full raw equality, the native backup and isolated public promotion now pass.
Complete strict-budget restore/recovery must still pass
before this is an operator profile.
Do not extrapolate the single-SST ratio into a capacity pass or prune any
history/receipts. Sustained mixed workload, producer and historical gates stay
open.

Owned evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/strict-disk-profiles-20261016/`
and
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/dictionary-policy-20261016/`.
