# Isolated compaction headroom test goal

Created: 2026-10-18. Status: **component experiment complete; no reduced
reserve or whole-node qualification**.

The operator explicitly requested a goal for the compaction experiment. This
component goal follows the [reserve analysis](../analysis/recovery-reserve-budget.md)
and leaves the existing [issue 9 acceptance criteria](issue-9-goal.md) unchanged.

## Registered objective

> Run a reproducible, isolated Linux/ext4 compaction experiment for Teleno, the operator-maintained open-source C++ Koinos node, to measure temporary disk headroom and assess whether a reserve smaller than 10 GiB can be justified for the reference profile: 2 vCPUs, 4,000,000,000 bytes of RAM and a 40,000,000,000-byte whole-server disk budget.
>
> Continue on codex/lr2-memory-hardening. Pin the exact source, RocksDB dependency, executable, settings and input digests. Use the native storage implementation and the reference configuration, including leveled compaction, nine column families, 64-MiB SST targets, the selected Zstd policy, one configured background job and the 64-MiB WAL flush trigger. Treat compaction and WAL thresholds as soft policies, not hard capacity bounds.
>
> Use fresh owned directories on My Passport and the operator-owned Linux build-and-test machine. Preserve existing source changes, backups, services, operational databases and evidence. Respect all existing storage and resource safety floors; inventory capacity before generating data. Linux experiments must use ext4. Mac measurements remain development diagnostics.
>
> Exercise append-oriented archive data and a separately labeled state-update workload. Distinguish representative verified archive samples from synthetic scale fixtures. Observe real automatic leveled compactions at increasing scales, including jobs producing at least 256 MiB and 1 GiB of new SST output. Attempt larger jobs around or above the current nominal compaction threshold only when the owned workspace safely permits them. Repeat the largest completed case at least three times. Record any unexecuted scale explicitly.
>
> Measure input/output bytes, actual compaction events, physical allocated-byte and filesystem-space peaks, simultaneous old/new SST retention, preallocation, WAL, flushes, logs and open/deleted files. Report CPU, RSS, I/O, duration, stalls and background-work overlap. Check every expected logical key/value or sampled archive record, digests and checksums after compaction and clean reopen. Exercise controlled interruption/recovery and capacity refusal on disposable test data without exhausting shared host storage.
>
> Separate temporary workspace from the emergency free-space floor and growth allowance. Evaluate 8-GiB and 6-GiB reserve candidates through explicit diagnostic budgets and measured peaks. Reducing an allowance saves no physical bytes. Keep the current 10-GiB application default, VPS configuration and existing issue-9 acceptance criteria unchanged during this experiment. Do not restart or compact the VPS database or activate block production.
>
> Complete this component goal only after the required scales, repetitions, integrity and recovery checks have executed and an English report records reproducible commands, exact identities, preserved failures, measured peaks and a justified conditional reserve recommendation. A negative or inconclusive reserve conclusion must remain explicit. Component evidence does not qualify the final whole node or complete issue 9.

## Execution evidence

The registration receipt and new experiment evidence belong to:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/compaction-headroom-goal-20261018-7b585e`.

Inventory storage before generating fixtures. Exact source and compiler/runtime
identities, planned allocation peaks, safety floors and test ownership must be
recorded before native execution. No compaction measurements exist at goal
registration; the earlier 52-MB observations retain their B3 identity.

## Initial inventory

The read-only Linux inventory verifies all 311 B7 source inputs against manifest
`a21a8958e1757a8f2f661264b5b8665158c6db33c9c31a2fed8991d39e962a20`.
The owned build workspace is on ext4 and no owned native executable is running.
Host available memory is 6,112,636,928 bytes at this snapshot.

Free ext4 space above the unchanged 10-GiB host floor is 1,861,754,880 bytes;
My Passport has 1,383,583,744 bytes above its unchanged 70-GiB floor. Those
figures require capacity planning before a 1-GiB output job which may retain
at least another GiB of input. No fixtures or compactions have been generated.
Reclaim only closed owned duplicates after verified preservation, or use an
already authorized suitable workspace. Do not lower either floor or touch
operational data. Exact inventory and progress are in `linux-inventory.json`
and `initial-progress.json` under the evidence directory above.

The subsequent [execution report](../validation/compaction-headroom-experiment.md)
records verified preservation and space reclamation, the diagnostic build
failures, passing 256-MiB/1-GiB automatic jobs, completed repetitions/recovery
and the conditional unexecuted larger scale. This initial inventory remains
a historical snapshot.

## Completed component outcome

- Actual automatic outputs above 256 MiB and 1 GiB pass for archive/state.
- Three fresh largest archive trials each produce 1,111,250,915 output bytes;
  maximum observed DB allocation is 2,291,363,840 bytes.
- Every generated record, all nine CF digests/checksums and ordinary-native
  reopen pass. The separate real backup fragment preserves all 121,814 records.
- Controlled process interruption at compaction begin recovers all 18,432
  baseline records. A diagnostic SST quota refuses more work and preserves
  all 1,024 acknowledged records. Neither is a physical power-loss/ENOSPC test.
- A scale around the 1.5625-GiB nominal input target is declined before data
  generation: its 3,556,769,792-byte old/new plan exceeds safe owned workspace.
  Existing floors remain unchanged; this conditional scale is unexecuted.
- The measured additional workspace estimate is 1,154,625,536 bytes. Explicit
  8-/6-GiB scenarios are reported with separate growth assumptions. The minimum
  safe reserve is unestablished; retain the default and qualify 8 GiB first.

`component-acceptance.json`, `measured-workspace-analysis.json` and
`final-native-inventory.json` record the result and its limits. All owned native
processes are closed. Native source/dependencies remain unchanged. The VPS is
not operated, issue 9 criteria remain intact, and this component does not close
the broader full-history node/producer goal.
