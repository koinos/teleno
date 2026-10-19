# Finite Fogata producer repair, rebuild and mainnet commissioning

Updated: 2026-10-18. Status: **scoped repair and the complete finite producer
observation pass; the qualified producer remains active as ordinary operation.**
The original producer attempt remains failed. Zero blocks are observed in the
new fixed window, so canonical production remains unproved and issue 9 stays open.

A subsequent operator request separately extends observation until an own block
is independently confirmed in the irreversible mainnet chain. That observation
**completes with own block 40,323,375**, produced at 21:35:48 UTC and independently
verified irreversible. Its periodic follow-up is paused; the same producer stays
active. The closed repair and one-hour commissioning result retain their original
deadlines and counters; see
[continued production observation](#continued-production-observation).

The amended run uses one Linux candidate build and one producer start. Its
immutable observation ends at **17:57:11.701 UTC** after **3,600.00018 seconds**.
The controller and deadline timer are inactive; no clock or counter is reset.
See the
[current repair evidence](../validation/native-producer-handoff.md). Refresh the
private current-actor evidence before operating a deployment; historical runtime
statements do not override the new run.

## Explicit operator amendment

On 2026-10-18 the operator explicitly authorizes updating this objective to
**fix the observed producer problem, recompile Teleno and repeat the finite
mainnet commissioning test**. This amendment supersedes the earlier prohibition
on automatic repair/rebuilding or another observation. It authorizes the scoped
source change, builds, deployment and deliberate producer activation described
below without another permission request for those steps. Existing activation
and identity checks remain mandatory.

The original attempt and all its deadlines, one producer start, raw logs and
measurements remain historical evidence. A separate amended run has its own
explicitly authorized limits; it never resets or extends the original attempt.
The old goal text is preserved in private evidence. See the
[first-attempt report](../performance/mainnet-producer-commissioning-2026-10-18.md).

## Objective

Diagnose and correct the recurring native producer failure
`cannot retrieve pending transactions from an unknown block`. Reproduce the
failure path in a deterministic regression, preserve Koinos consensus and wire
behavior, and verify that stale-head handling neither silently substitutes an
empty mempool nor suppresses unrelated failures. Compare legacy behavior where
it informs the repair; do not claim an unverified cause or legacy result.

Build and test an exactly identified Linux candidate, then deploy that candidate
to the **same operator-owned reference VPS and existing Fogata mainnet pool**.
Reuse the preserved full-history database, existing pool settings and registered
hot key. Validate the replacement binary as an observer before deliberately
activating the producer and completing the finite commissioning observation.

Pool creation, the original 1,000-KOIN burn/Fogata stake, 900-KOIN pool reserve
and 100-KOIN owner remainder are already verified milestones. Reuse the existing
kcli owner vault and pool. Do not create another account, pool or key, resend or
restore the deleted recovery email, change wallet defaults, register a new hot
key, transfer funds, burn more KOIN or alter the approved reserve. The owner
custody key remains off the VPS. This repair authorizes block production with
the existing registered identity, not additional financial transactions.

## Fixed limits for the amended run

The amendment is observed at **2026-10-18 15:57:49 UTC**. Allow a maximum of
**four hours total**, ending at **19:57:49 UTC**:

1. **Repair/build:** at most two hours, ending at **17:57:49 UTC**, with at most
   **two Linux candidate builds**. Inspect the actual source, add focused
   concurrency/failure regressions, and run appropriate broader native checks.
   Local diagnostics and tests share this development deadline.
2. **One new VPS commissioning run:** at most **120 minutes**, always inside
   the overall deadline: at most **15 minutes preparation**, **45 minutes
   observer readiness**, and **60 minutes producer observation**. An unused
   phase allowance does not extend another phase or the overall deadline.
3. Allow at most **two producer starts in this amended run**, including any
   corrective restart. Persist the new phase deadlines and counters before
   activation. Keep the original attempt's counters separately and unchanged.

Do not reset these limits across turns, wait longer for a produced block, open
another commissioning run, start an indefinite optimization loop, or repeat
funding/restores. Stop on exhausted limits or an unresolved correctness/resource
gate, preserve evidence and return to the saved observer configuration. Report
unmet requirements; elapsed time never changes the pass criteria.

## Runtime, storage and compatibility requirements

Preserve Ubuntu/x86_64/ext4, **two vCPUs**, approximately **four decimal GB RAM**,
no swap and the strict **40,000,000,000-byte whole-server disk budget**, including
OS, software, databases, WAL, logs, temporary files and
`max(8 GiB, database bytes / 5)` of free recovery reserve. Preserve every existing
block/receipt and complete canonical history; do not prune or replace chain
state. Keep steady native RSS **<=2 GiB**, peak RSS **<=2.5 GiB** and concurrent
host MemAvailable **>=768 MiB**, with no OOM or reliance on swap.

Keep development on `codex/lr2-memory-hardening`. Use a fresh owned My Passport
run and isolated directories on the authorized Linux build machine for generated
data, compilation and tests. Preserve unrelated work, dependencies, services,
operational databases, backups and secrets. Never use the retired VPS. Keep
account/access bindings and key material out of public source and reports.

## Deployment and activation gates

- Record exact source/input manifests, dependency identities, candidate binary
  hashes and focused/broader test results. Verify native version and CLI help.
- Recheck the reference host, executable, basedir, config, owner/service state,
  mainnet RPC/chain identity, intended pool and on-chain hot-key bytes/VHP.
- Stage the new binary separately. Keep the original verified binary and observer
  launch available for rollback. Never replace an executable under a live process
  or run two native processes against the same database.
- Start the candidate with production explicitly disabled. Verify database health,
  advancing head/LIB, independent canonical agreement, peers, resource margins
  and exact pool/key bindings before the deliberate producer restart.
- Keep admin endpoints local/protected and preserve the existing hot-key startup
  guard. Review the private configuration/startup/rollback diff before activation.
  Follow [the producer guide](../running-producer-node.md).

## Success and deliverables

The repair must pass its focused failure/concurrency regressions and appropriate
broader native tests. The exact deployed candidate must pass observer readiness
and the full **60-minute producer observation** without persistent production
errors, unsafe stale-head handling or resource-limit breaches. Runtime evidence
must demonstrate candidate preparation and PoB evaluation; a running process or
auxiliary refresh alone is insufficient. Record expected recoverable handoffs
separately from unrelated failures and stop on an unresolved persistent fault.

Independently verify canonical inclusion/irreversibility of every observed
produced block. If no block is produced in the fixed window, state clearly that
canonical production remains unproved, and do not extend the observation.
Keep finite configuration commissioning distinct from issue-9 whole-node and
operational produced-block qualification; this goal alone does not close issue 9.

Deliver the source change, exact build/configuration identities, test results,
RAM/disk/CPU/I/O/latency measurements, pool/transaction provenance, actual final
service state and observer rollback procedure. Preserve prior failed evidence
and publish no private operational bindings. Keep affected Teleno and Koinos One
operator documentation consistent, without changing GUI/package/submodule code.

A producer that passes this bounded commissioning may remain running as ordinary
node operation. On failure, leave the verified saved observer running and report
the precise unmet condition. Mark success only after the amended acceptance
criteria are supported by the actual final implementation and runtime evidence.

## Owned evidence

Amended run:
`/Volumes/My Passport/teleno-producer-repair-20261018-flnbdgu4/`.
Its `evidence/authorized-repair-control.json` records authorization and immutable
limits. The preceding run remains under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/mainnet-producer-commissioning-20261018-8vhde0kn/`.

## Continued production observation

On 2026-10-18, after finite commissioning is closed, the operator explicitly
requests extending the time until an own produced block is detected. This
authorizes a separate observation of the same qualified producer in ordinary
operation, including observation after the former commissioning deadlines.
The earlier prohibition on waiting longer applied to that closed commissioning
window; it does not prevent this newly requested observation. Do not alter the
earlier result, measurement window, deadlines, build count or producer-start count.

Continue the existing mainnet process, binary, configuration, pool and registered
hot key. No new build, producer start, commissioning run, funding transaction,
manual burn, reserve change, wallet or key is needed. Retain the resource,
full-history, secret-handling and observer-recovery requirements above.

Use read-only checks approximately every ten minutes. Check producer activity,
head/LIB progress, peers, resource limits and the native produced-block log.
For each observed block, retain its height and ID, compare its decoded ID with
the independent mainnet canonical block ID at that height, verify its selected-pool
header and wait for the independent LIB to reach or exceed its height. A local
production message or a matching synchronized head alone is insufficient.

Observation completes when at least one own block is independently canonical
and irreversible. It has no predetermined block-wait deadline. An orphaned
candidate does not satisfy this condition. Persistent correctness or resource
failures require evidence preservation and operator reporting or the previously
authorized verified observer rollback, without automatic producer restarts.
Finish the periodic follow-up when its success condition is met or a persistent
failure needs intervention. Issue 9 remains open until its other criteria pass.

Keep the separate control, samples and block proofs under the existing owned
run's `evidence/continued-production-*` directory. The private pointer
`evidence/continued-production-current.json` identifies the active observation.

### Continued observation result

The same qualified producer emits block **40,323,375** at
**2026-10-18 21:35:48.815248 UTC**, with zero transactions. At the complete
read-only proof at **21:40:57 UTC**, the local canonical ID and selected-pool
signer match the independent mainnet block; independent LIB **40,323,411** makes
it irreversible. The observation is complete and the ten-minute heartbeat is
verified **paused**. The producer remains active with its original PID and no
restart, new build, configuration change or financial action.

The initial own-block check exposes an address-format error in the private
monitor: the signer is returned as a Base58 address but was decoded as Base64.
Its error/original script are preserved. Direct local/independent header reads
verify the selected pool; correcting only this diagnostic decoder and repeating
the complete read-only proof succeeds. No native repair or verification criterion
is changed. See [the continued-observation report](../validation/native-producer-handoff.md#subsequent-own-block-confirmation).

The former fixed window still contains zero produced blocks. Its deadlines,
counters and measurements remain unchanged, and issue 9 stays open for its
remaining acceptance criteria.
