# Direct VPS qualification of the experimental 8 GiB reserve

Date: 2026-10-18. Status: **completed finite experimental qualification on the
actual reference VPS: complete restore, retained-reader real compaction,
bounded isolated producer create/reopen, clean/abrupt recovery, operator-requested
five hours, clean reopen, irreversible legacy ID/root comparison and every
selected archived record preserved. The operator subsequently adopts the 8-GiB
reserve for original issue 9; its remaining broader requirements stay open.** See the
[current evidence](../validation/vps-8gib-recovery-reserve.md).

The operator explicitly requested a goal and qualification directly on the
designated reference VPS. The prior compaction component result is an input,
not a pass for this goal. Development stays on `codex/lr2-memory-hardening`.

On 2026-10-18 the operator explicitly reduced this experiment's ongoing
observation from 24 hours to **five hours (18,000 seconds)**, counted from the
same native process's original start. Existing measurements are retained.
This amendment changes the finite experiment's duration only; the original
issue-9 and wider LR2 sustained-operation requirements remain separate.

## Objective and scope

Implement an explicit opt-in `experimental-8gib` recovery-reserve profile and
qualify the final whole node on the existing Ubuntu/x86_64/ext4 reference VPS:
2 vCPUs, 4 decimal GB physical RAM, no swap, and a strict 40,000,000,000-byte
whole-server disk budget. Preserve an 8-GiB free reserve throughout every
measured temporary peak. The default remains `max(10 GiB, database bytes / 5)`;
the experiment uses `max(8 GiB, database bytes / 5)`.

This began as a separate experiment. On 2026-10-18, after its completion, the
operator explicitly changes original issue 9 to the same 8-GiB reserve floor,
retaining the greater 20%-of-database term. These exact final-candidate/input
measurements now count toward issue 9 and satisfy its amended selected-input
restore/measured-workload disk requirement. They do not retrospectively pass B3
or close the remaining issue-9 gates. The standard runtime default remains
10 GiB. Changing the allowance saves no physical storage bytes.

Preserve complete canonical history from genesis, current state, every block
and receipt in the independently verified selected backup, and Koinos consensus
and wire behavior. Keep exact source, dependency, executable and input hashes.
Use My Passport and isolated build-machine directories for development data and
compilation; execute application qualification on the actual VPS.

## Execution and acceptance

1. Verify the VPS identity/specifications, live processes/services, ownership,
   basedirs, backup provenance, binary and disk headroom. Preserve existing work
   and operational data. A stopped test database must have a verified recovery
   copy before replacement; an older snapshot is not a copy of its latest state.
2. Keep estimation and enforcement consistent. Bind the selected profile to
   the attempt descriptor; require it again on resume and activation. Check
   default/experimental estimates, the retained 20% term, low-space refusal,
   mismatched-policy refusal, recovery and unchanged observer holds.
3. Build and run focused and broader native regressions on the exact final
   source. Independently verify the deployed binary and its CLI surface.
4. On the VPS, repeat full single-copy restore, interruption/resume, validation,
   held activation and reopen. Retain every selected payload record. Preserve
   the original source-trust requirements and all failed observations; absent
   source signatures must remain explicitly distinguished from provenance.
5. Run observer catch-up, mixed read workloads, clean restart and controlled
   interruption/recovery. A completed five-hour observation must include all
   startup/shutdown peaks; a guard refusal is a failure, not a completed soak.
   Measure compaction/WAL activity and temporary file allocation. Record larger
   compaction/retained-reader cases as separate evidence; absence of such a job
   during observation is not qualification of that case.
6. Measure actual whole-server unavailable/occupied device space plus the
   held-free reserve, including the OS, runtime, databases, WAL, logs, temporary
   work and filesystem unavailable/reserved space. Do not credit capacity beyond
   40 decimal GB. Require steady node RSS <=2 GiB, peak <=2.5 GiB, concurrent
   host `MemAvailable` >=768 MiB, no OOM and no use of swap. Report CPU, I/O,
   read latency, stalls and growth alongside disk and RAM.
7. Execute bounded isolated native producer workloads where practical. Keep
   observer, isolated producer and operational producer evidence distinct.
   This goal does not authorize mainnet signing or production activation.
8. Review the final implementation, completed measurements and limitations.
   Complete only after this experimental final-node qualification passes;
   keep every unmet gate explicit and continue the measure/implement/test/
   compare/review loop. No pruning, record omission, VPS enlargement or silent
   reduction of the experimental reserve is allowed.

The initial read-only inventory verifies the selected replacement server,
2 vCPUs, 4,000,034,816 bytes RAM, no swap, no running native node, and an intact
observer hold with no producer key in the test basedir. The prior stopped B3
test database is fully preserved off-host before controlled replacement. The
new complete backup passes actual VPS restore, interruption/resume,
read-only validation and held activation/reopen. The combined final result also
passes 18,007.577 seconds of observer operation and the 300.707-second reopen,
with all 45,076,063 selected archive records unchanged and 77,000 new records.
Worst whole-server allocation including the held reserve is 39,352,967,168 bytes;
maximum native RSS is 745,668,608 bytes and minimum host MemAvailable is
2,760,302,592 bytes. No OOM/swap occurs. The standard reserve and hardware stay
unchanged. This finite selected-backup/workload result does not establish a
general minimum safe reserve or operational mainnet producer readiness.

Owned evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/vps-8gib-goal-20261018-72a46f/`.
Private access routes and host inventory remain outside public source/docs.
