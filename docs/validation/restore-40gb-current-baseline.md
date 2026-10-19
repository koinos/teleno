# Complete-backup restore within the 40 GB baseline

Date: 2026-10-15. Historical status: **original full-size source refused by
capacity preflight**. The later [reference VPS report](reference-vps-observer.md)
supersedes this capacity checkpoint: a denser complete backup restores and
validates within the unchanged 40-GB budget. Its subsequent 24-hour observer
attempt stops after 21h07 when a peak exceeds that budget by 11,091,968 bytes;
sustained qualification remains open.

The user reaffirmed that the complete backup must restore within the designated
replacement VPS's 40 GB whole-server disk envelope. Single-copy restore removes
the second full payload. The current public writer/identity prerequisite is now
corrected, while the unchanged disk budget still fails. Neither the remaining
capacity nor full-history/runtime evidence is waived.

## Current public source and provenance

The authorized [observer publisher update](../implementation/backup-publisher-identity-update-plan.md)
completed on the larger writer host with an isolated maintenance candidate,
not the LR2 development binary. Native build, 22/22 regressions, container smoke,
installed artifact/observer checks, advancing head/LIB and new receipt agreement
with the independent legacy implementation pass. The old local/public input was
preserved before configured retention removed its published generation.

The new concrete public ID is
`20261015T191906Z-ms-1792091946641-files-494`, with **497 files** and
29,721,611,361 logical bytes. Metadata was fetched over HTTPS from the
[Foundation bootstrap repository](https://seed.koinosfoundation.org/backups/prodnet/teleno-bootstrap/latest.json).
Manifest SHA-256 is
`0bd864df4ea7a526f6096dd5d0f952e42062e8eb4a0d84742565c013f483fdd5`;
inventory SHA-256 is
`e1b83cb557e800554d89bb0f347f4c87795120ea7a93a9c2b4ba50f83d8f82bb`.

The public inventory and checkpoint fields match the independently
SSH-authenticated writer. Only the reviewed public observer configuration is
substituted. Native metadata-only preflight on Mac and on the designated VPS
matches both digests and the independently expected mainnet chain ID. The
manifest's `checkpoint_verified` identity refers to durable revision 40,236,770,
not live head or proven historical replay. Source signatures remain absent:
`signature_required: false`, `signature_verified: false`. Independent operator
provenance and checksum equality do not create a native signature, historical
coverage proof or complete public-payload audit. HTTPS/signature policy is
unchanged; no trust or identity check was bypassed.

## Current actual-VPS capacity check

The replacement baseline is Linux/x86_64/ext4, 2 vCPUs, 4,000,034,816 bytes RAM,
no swap, with a 39,956,590,592-byte root filesystem. At 19:32:31 UTC no
`teleno_node` process was observed. A copy of the current Linux P2-B development
binary was then verified in an owned isolated directory and used only for the
direct metadata/space CLI. No node service or production was started.

The baseline CLI's executable SHA-256 is
`290cf112aed00a06b5e839c83268116036d76fa3564ccda30ea49a463df8a6e8`;
the observer config SHA-256 is
`5ab6d85df9276acbc9b1e25df11522d1dcdd8b33135561d2b7959425f8b9ff97`.
These are separate from the maintenance executable installed on the writer.

| Dimension | Bytes in the native VPS preflight sample |
| --- | ---: |
| Actual root filesystem capacity | 39,956,590,592 |
| Filesystem available space | 36,594,917,376 |
| Existing usage plus filesystem unavailable/reserved space | 3,361,673,216 |
| Declared complete payload, including runtime files | 29,721,611,361 |
| Declared database payload | 29,720,960,254 |
| Modeled allocated payload, unchanged 64 KiB quantum | 29,729,488,896 |
| Metadata and validation allowances combined | 268,435,456 |
| Unchanged recovery/operating reserve | 10,737,418,240 |
| Direct-restore recommended free space | 40,735,342,592 |
| Planned filesystem usage including reserve | 44,097,015,808 |
| Deficit against actual available space | **4,140,425,216** |

Preflight exits 0 because it passes source/target eligibility and the declared
minimum, while explicitly reporting `below_recommended: true`,
`ready_to_restore: false` and `capacity_40gb_qualified: false`. The subsequent
stage command exits 1 in staging before creating the target/attempt or
transferring payload. Source identity is no longer the refusal. The reserve
check correctly prevents transfer on this host; this is not a successful restore.

The model follows `estimate_mode_restore_space()` for direct mode: one payload,
no complete object cache, existing allocation quantum, metadata/validation
allowances and `max(10 GiB, database_bytes / 5)` reserve. OS/runtime, the isolated
binary and filesystem reservations already reduce observed available bytes.
The model/allowances are not measured upper bounds on restore/compaction peaks.
At this sample the modeled allocated payload ceiling is **25,589,063,680 bytes**,
requiring at least about **13.93%** lossless payload reduction before additional
workload variation. This threshold does not prove any compression setting can
achieve it. Do not consume root-reserved space, reduce the recovery reserve,
prune history or enlarge the server to turn the result into a pass.

## Earlier same-day observation, retained as history

The superseded public ID
`20261015T080853Z-ms-1792051733888-files-497` had 500 files / 29,711,902,447
bytes and empty chain identity, produced by `1.2.0+b8dab4c08f99`. Its native
preflight exited 1 with `source_chain_identity_missing`; inspection also found
an empty archived receipt root at durable revision 40,223,698. Its earlier
capacity model had a 4,086,128,640-byte deficit. Those preserved raw observations
remain valid for that old generation, not for the current public source.

## Required next proof

The [16 October lossless archive experiment](lossless-archive-storage-experiment.md)
subsequently verifies every pinned input file and builds a database 16.422%
smaller. Complete byte comparison and structural continuity through the selected
40,236,770-height head pass, with public-reference successor-header agreement.
Linux component tradeoffs
and selected native historical queries are recorded there. These do not replace
the dated VPS refusal above or establish a successful complete restore; fresh
whole-server available bytes and the unchanged reserve remain required.

The subsequent [native policy change](native-block-compression-policy.md)
retains explicit level/dictionary options for future block tables. It does not
rewrite the source or establish a compact-backup export/restore result. Its
component tests cannot replace the whole-server restore check.

1. Preserve and independently authenticate the pinned complete-history input,
   all selected block/receipt records and the existing trust policy. Signatures,
   full public-payload verification and historical coverage/replay remain explicit
   checks; metadata provenance alone does not close them.
2. Measure lossless full-archive storage improvements off-target in owned
   development storage. Compare the complete logical corpus and historical
   access/replay, plus CPU/RAM and migration-space costs. Sample compression
   percentages are insufficient.
3. Restore the resulting eligible backup through the single-copy path on the
   designated VPS. Record whole-server peak disk use through transfer,
   interruption/resume, validation, activation, reopen and observer head progress,
   with reserve intact. Producer activation remains separate.

Private raw observations and initial refusal remain in the owned run's
`evidence/restore40/`; corrected publication, metadata, current native VPS
preflight and reserve refusal are in `evidence/publisher/`, under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/`.
See the [goal contract](../implementation/issue-9-goal.md#whole-server-restore-acceptance-2026-10-15-clarification)
and [single-copy plan](../implementation/single-copy-backup-restore-plan.md).
