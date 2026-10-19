# Recovery reserve sizing for the reference VPS

Date: 2026-10-18. Status: **analysis complete; 8 GiB and then 6 GiB are
qualification candidates. The existing 10-GiB policy remains unchanged.**

The available evidence supports testing a smaller reserve on the designated
2-vCPU, 4-decimal-GB, ext4 server. It does not establish a minimum safe reserve.
Start with 8 GiB, then evaluate 6 GiB after larger compactions and recovery pass.
Reducing a reserve changes the capacity allowance; it saves no physical bytes.
Continue lossless code and storage optimization independently.

The operator subsequently authorized a [separate goal directly on the VPS](../implementation/vps-8gib-reserve-test-goal.md).
An opt-in direct-CLI profile is now in development, with fresh build, restore
and whole-node measurements pending. The default and original issue-9 contract
remain unchanged. The earlier component experiment does not qualify this profile.

## Existing policy and its use during observation

The repository restore estimator already recommends the greater of 10 GiB or
20% of restored database bytes. The direct estimator repeats this formula,
and direct staging and activation enforce its recommended space. Relevant code:

- [Repository estimate](../../src/backup/snapshot_repository.cpp):
  `recommended_min_margin_bytes` and `estimate_restore_space()`.
- [Direct estimate](../../src/backup/restore_plan.cpp):
  `estimate_mode_restore_space()`.
- [Direct enforcement](../../src/backup/direct_restore.cpp):
  staging/resume and offline validation/activation checks.

This is an application capacity policy, independent of Koinos consensus.
The selected backup's database has 24,113,527,301 bytes; its 20% term is
4,822,705,460 bytes, about 4.49 GiB. The 10-GiB minimum therefore dominates.

The observation supervisor also requires the whole 10-GiB reserve to remain
available throughout every temporary peak. This is stricter than treating
some of that headroom as workspace which compaction may temporarily use.
Its peak accounting includes the temporary allocation already present and
then adds the full reserve. A future policy should distinguish temporary
workspace from the emergency space that must remain free while that workspace
is in use. These are different guarantees; changing between them requires a
new documented test contract.

## Measurements from the reference VPS

The frozen observer source is
`b3f83775d32e815d94adcbee967d2d66333e20eaed33d764daa390bcf9f80364`;
native SHA-256 is
`b38318065a982d9a83de30f4dafd0dd85a00a2709f32aa9bec23e9c28788332c`.
It is separate from the later B7 candidate. Complete restore/validation/held
activation peaks at 39,760,650,240 bytes including the original reserve.

The subsequent observer runs for 76,023.649 seconds. Its worst whole-device
occupied/unavailable sample is 29,273,673,728 bytes. Adding 10,737,418,240
reserved bytes produces 40,011,091,968 bytes, triggering the strict 40-billion-
byte guard. Physically available root space remains 11,687,899,136 bytes.
The provider's actual device is 40,961,572,864 bytes; that extra capacity is
not added to the permitted 40,000,000,000-byte budget. Filesystem reserved and
unavailable space remain charged. See the [observer report](../validation/reference-vps-observer.md).

Read-only inspection of the stopped node preserves six OPTIONS/log/metadata
files and checks identical database file metadata before and after collection.
The node is not started and the database is not opened.

| Observation | Measured value and scope |
| --- | --- |
| RocksDB version and compaction style | 8.8.1, leveled compaction, nine column families |
| Configured background jobs and subcompactions | 1 and 1 |
| SST target size | 64 MiB; largest existing SST is 67,303,882 bytes |
| Nominal compaction input threshold | 1,677,721,600 bytes, or 1.5625 GiB; not a hard maximum |
| WAL flush trigger | 64 MiB; latest stopped-node WAL allocation is 66,748,416 bytes, not a whole-run bound |
| Logged compactions | Six successful; a seventh is interrupted by the controlled shutdown |
| Largest successful compaction | 52,701,273 input bytes and 52,471,122 output bytes |
| Extra total host allocation at the observed peak versus start | 219,660,288 bytes, including growth and temporary work |
| Net host allocation increase after shutdown | 184,258,560 bytes over 21h07 |
| Owned node log and telemetry | 32,794,717 and 3,518,257 bytes, already included in host use |

The failed guard sample is at 22:45:30.586684 UTC. The final blocks compaction
starts at 22:45:30.306018 and reports shutdown interruption at 22:45:32.347472.
It has 20,093,108 input bytes. The timing supports compaction as a contributor
to the peak, but there is no simultaneous per-file allocated-byte trace that
attributes the entire peak to it. Its partial output is not a successful
compaction result. Shutdown returns 35,401,728 available bytes across the host.

The observed net host increase has a daily equivalent of about 209 MB if
repeated. It includes WAL, diagnostic logs and other host activity; it is not
an isolated chain-growth measurement or a reliable long-term forecast.
Full-history growth still needs a maintenance horizon and capacity alerts.

## Why the small observed peaks do not establish a small safe reserve

In the pinned RocksDB source, `max_compaction_bytes` is a target which may be
exceeded. The saved options also enable `ignore_max_compaction_bytes_for_input`.
Consequently, the 1.5625-GiB value cannot prove that a 2-GiB reserve is enough.
[RocksDB 8.8.1 compaction options](https://github.com/facebook/rocksdb/blob/v8.8.1/include/rocksdb/advanced_options.h).

The WAL setting initiates flushes; it does not bound all concurrent storage.
Flushes, compaction outputs, old tables held by readers/checkpoints and recovery
can overlap. [RocksDB 8.8.1 database options](https://github.com/facebook/rocksdb/blob/v8.8.1/include/rocksdb/options.h).
An offline full rewrite or a locally retained backup can require substantially
more space than ordinary observation. They need separate capacity checks;
neither 10 GiB nor a smaller reserve certifies those operations automatically.

RocksDB offers compaction space admission through `SstFileManager`, but its
SST/blob accounting does not cover WAL or the whole server. It can also defer
compaction or cause background space errors, which affect availability.
It is a possible future implementation component, not a complete solution or
an existing Teleno whole-server limit.
[RocksDB disk space management](https://github.com/facebook/rocksdb/wiki/Managing-Disk-Space-Utilization),
[pinned interface](https://github.com/facebook/rocksdb/blob/v8.8.1/include/rocksdb/sst_file_manager.h).

## Capacity with smaller candidate reserves

These calculations reuse the same observed B3 peak. No reduced-reserve
experiment has run, and no earlier failed result is reclassified.

| Reserve | Peak accounting including reserve | Additional allowance within 40 GB |
| --- | ---: | ---: |
| 10 GiB, current rule | 40,011,091,968 bytes | -11,091,968 bytes |
| 8 GiB, first candidate | 37,863,608,320 bytes | 2,136,391,680 bytes |
| 6 GiB, second candidate | 35,716,124,672 bytes | 4,283,875,328 bytes |
| 4 GiB, exploratory only | 33,568,641,024 bytes | 6,431,358,976 bytes |

Eight GiB retains 80% of the existing minimum while testing a smaller policy.
Six GiB offers a larger capacity allowance and remains above this backup's
20% term. These are engineering candidates, not unique minima derived from
the six successful compactions. Four GiB needs stronger evidence and would
also be below the existing 20% recommendation for this input.

## Qualification before changing the policy

1. Separate the reserve into additional temporary workspace, a persistent
   emergency floor and growth until operator response. Account existing SST,
   WAL and logs once. Work performed in mutually exclusive restore/validation/
   runtime phases should not be summed as if it all overlaps; concurrent work
   must be included. Define the supported workloads and local-backup retention.
2. On isolated Linux/ext4 fixtures, exercise larger L0 and deeper merges,
   concurrent flushes, retained readers/checkpoints, low-space refusal and
   interruption/recovery. Record input/output and physical allocated-byte
   peaks, including preallocation and open/deleted files. Check every retained
   record, checksums and reopen. Measure CPU, I/O and stalls.
3. Qualify the final source on the designated server: complete direct restore,
   validation/reopen, catch-up, mixed workloads, producer tests and sustained
   operation at 8 GiB before 6 GiB. Record reserve and emergency-floor minima,
   compaction backlog, history equality and recovery outcomes. Keep earlier
   B3 evidence separate from final-source acceptance.

If implemented later, the selected profile should feed both native estimators,
all direct-stage/activation checks and observation tooling through one explicit
policy. Retain the existing general recommendation until the smaller profile
passes. Low-space refusal should precede an operation that cannot fit; disabling
durability, deleting WAL/SST files, pruning history or assuming a compaction
target is a hard bound would not establish a smaller safe reserve.

The existing goal still requires the original reserve. This analysis proposes
a reviewable change; it does not modify that criterion, source settings, runtime
configuration or the stopped VPS.

The operator subsequently requests and registers a separate
[compaction test goal](../implementation/compaction-headroom-test-goal.md).
Its [completed component experiment](../validation/compaction-headroom-experiment.md)
retains the current application/VPS reserve and broader issue 9 requirements.
Three fresh archive trials produce 1,111,250,915-byte automatic jobs, with a
maximum observed owned DB allocation of 2,291,363,840 bytes. Their additional
workspace estimate above final SST content is 1,154,625,536 bytes. The real
backup fragment preserves all 121,814 records. Separate interruption-at-begin
and diagnostic SST-quota refusal cases preserve their acknowledged records.

Adding that measured workspace to the older B3 peak and an illustrative
512-MiB growth allowance leaves 444,895,232 bytes with an 8-GiB held-free
reserve, or 2,592,378,880 bytes with 6 GiB. Growth is not a measured forecast;
these are additive scenarios, not final-source VPS results or upper bounds.
Jobs around the 1.5625-GiB nominal input target cannot fit the unchanged owned
workspace plan and are explicitly unexecuted. Retained-reader/checkpoint,
partial-output/power-loss, long-growth and whole-node qualification remain
open. The minimum safe reserve remains unestablished. Qualify 8 GiB first;
the component result does not justify changing either default or acceptance
criteria automatically.

## Evidence

Owned external directory:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/recovery-reserve-analysis-20261018-bd2975/`.

`reserve-assessment.json` contains the reviewed arithmetic, parsed OPTIONS and
compaction status. `collection-proof.json` verifies the read-only collection;
the six-member archive is 217,799 bytes, SHA-256
`19985085219e0f9d1bc093cf201aa8422cc6b26ba95b11473495d04e3f78fccc`.
Raw logs and access details remain outside repository documents. The earlier
21-hour outcome, full-restore measurements and source identities remain intact.
