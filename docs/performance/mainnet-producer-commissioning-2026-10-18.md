# Finite mainnet producer attempt: recurring head/mempool lookup failures

Date: 2026-10-18. Result: **producer qualification failed; observer rollback
verified.** Pool deployment and the separately authorized initial burn/stake
are canonical and irreversible. This report does not close issue 9.

## Funding and runtime outcome

The operator requests an exact **1,000-KOIN burn** and confirms reducing their
previously approved pool reserve. One atomic transaction withdraws 1,000 KOIN
from their reserve, approves the matching PoB burn and VHP transfer, and stakes
through Fogata. Nonbroadcast simulation and the actual receipt consume
**3.19472266 Mana**. A single broadcast is verified in canonical irreversible
block **40,315,460**. Final read-only verification after the native attempt
confirms **1,000 VHP credited to the owner's stake, 900 KOIN reserved and
100 KOIN retained by the owner**. Actual account and transaction bindings
remain in private evidence.

The existing Linux native candidate passes observer readiness, independent
canonical LIB comparison, registration/key matching and positive-VHP checks.
Production is deliberately activated once. The configured hot key loads
correctly and the recovery hold is released through the documented explicit
restart. The owner custody key stays off the VPS.

The producer repeatedly reports:

```text
cannot retrieve pending transactions from an unknown block
```

There are **12 occurrences** before the clean stop. The last producer resource
sample is at **1,253.23 seconds**, approximately 21 minutes into the intended
one-hour window. No produced block is observed, including a final audit of
the stopped producer log. This is a failed partial attempt, not a completed
one-hour qualification or proof of canonical production.

The controller and producer are stopped; the saved observer configuration is
restarted and verified against mainnet identity, advancing head/LIB and three
peers. The observer has **76,464,128 bytes RSS** and the host has
**3,410,071,552 bytes MemAvailable** at rollback verification. Production is
disabled. The deadline watchdog is cancelled after this terminal failure;
the original deadlines and single producer-start count remain preserved.

## Measured resource evidence

Profile: Ubuntu/x86_64/ext4, two vCPUs, approximately four decimal GB RAM and
a strict **40,000,000,000-byte whole-server budget**, preserving
`max(8 GiB, database bytes / 5)` of recovery space.

| Measurement | Partial producer window |
| --- | ---: |
| Resource samples | 205 |
| Maximum kernel native RSS high-water mark | 159,924,224 bytes |
| Minimum concurrent host MemAvailable | 3,301,134,336 bytes |
| Minimum margin inside the whole-server budget including reserve | 1,869,701,120 bytes |
| Mean CPU consumption | 20.71% of one logical CPU |
| Kernel process read-byte counter increase | 9,297,920 bytes |
| Kernel process write-byte counter increase | 47,099,904 bytes |
| Successful local head RPC mean / p95 / maximum | 2.23 / 4.00 / 11.18 ms |

Host swap/OOM and owned-cgroup OOM invariants remain unchanged. The preceding
observer catch-up has a kernel RSS high-water mark of **302,268,416 bytes**.
These are different workloads and must not be treated as a direct memory
savings comparison. No produced-block receipt or canonical produced block is
observed in this partial window, so its resource measurements cannot qualify
successful block submission or the full issue-9 workload.

## Confirmed failure path and remaining uncertainty

1. `BlockProducer::next_block` reads the current chain head and saves it as the
   candidate's parent (`src/block_production/block_producer.cpp:279`).
2. `fill_block` requests pending transactions for that saved parent
   (`src/block_production/block_producer.cpp:297`).
3. The mempool resolves the requested fork and throws the observed exception
   when no matching node is available (`src/koinos/mempool/mempool.cpp:324`).
4. The outer producer exception handler logs the warning and backs off for
   five seconds (`src/main.cpp:1878`). Candidate preparation happens before
   PoB eligibility testing and block submission.

Block events reach the mempool through `MonolithRpcClient` and the event-bus
callback registered in `src/main.cpp:1944`. A race between published chain head
and mempool fork availability is a plausible cause. Unexpected fork retirement
is another possibility. Warning-time head IDs and the mempool fork inventory
were not recorded, so the exact interleaving is **not proven**. The report does
not claim that the LR2 changes introduced it or that legacy cannot exhibit it.

### Bounded follow-up diagnosis

A read-only probe on the same observer performs **300** explicit-parent mempool
queries over **30.0003 seconds** and **13 distinct chain heads**, with **zero**
mempool or diagnostic request errors. Head height advances from 40,316,672 to
40,316,684. The observer process identity is unchanged before/after, the producer
remains inactive, and no service operation, build or commissioning deadline reset
occurs. This short negative result does not exclude a publication race or qualify
the producer.

The source exposes a possible handoff window: chain finalization publishes the
new head and releases its exclusive database lock before broadcasting block
events. The irreversibility event precedes block acceptance; block-store and
enabled indexer subscribers also precede the mempool subscriber. The independent
producer thread can read a head before `mempool_impl::handle_block` creates that
fork's writable transaction view. The exact runtime interleaving behind the
recorded warnings remains unproven.

The producer source, the chain's head-publication/event ordering, and the mempool's
`relevant_node`, `handle_block` and `handle_irreversibility` definitions match
repository HEAD `d3cfb94455da4a472f651eb4239abbc289e74225`. This scoped comparison
neither proves legacy behavior nor excludes timing effects from other changes.

Saved-log correlation finds another auxiliary-data refresh **5.0119 to 5.0229
seconds** after each of the twelve warnings. This confirms retry activity rather
than a permanently stopped loop. An auxiliary refresh alone does not prove a
successfully assembled candidate or an eligible produced block.

The error originates in the native head/mempool query path. It does not undo
the independently confirmed Fogata burn/stake. Fogata's authorization during
an actually eligible produced block remains untested in this attempt.

## Concrete next scope

The operator subsequently authorizes the repair, recompilation and one new
finite run. The [amended goal](../implementation/mainnet-producer-commissioning-goal.md)
and [handoff repair report](../validation/native-producer-handoff.md) govern that
separate execution. The original attempt below remains failed and its counters,
deadlines and measurements are unchanged.

The original finite commissioning goal prohibited automatic rebuilding or a new
observation loop. That attempt therefore ended with the verified observer,
without replacing its candidate or restarting the producer again.

The subsequently authorized repair addresses the following scope:

1. Record the requested parent, chain head and relevant mempool fork IDs around
   this failure, and reproduce rapid head publication/fork handoff locally.
2. Compare legacy candidate assembly and classify stale-head handling without
   suppressing unrelated failures or substituting an empty mempool.
3. Implement a coherent candidate snapshot or bounded stale-head retry with
   explicit failure behavior. Preserve consensus validation, key bindings and
   complete history.
4. Run focused concurrent producer/mempool tests, then produce a newly identified
   Linux binary using My Passport or the authorized build machine.
5. Qualify that exact binary in a newly authorized finite observer/producer
   window on the same VPS. Reuse the existing pool and keys; further funding
   conversion is not part of this repair.

## Evidence and diagnostic corrections

Private evidence is under the existing owned
`mainnet-producer-commissioning-20261018-8vhde0kn` run. The primary records are
`initial-burn-mainnet-verified.json`,
`post-native-attempt-pool-balances-reverified.json`,
`native-failed-attempt-evidence/failed-attempt-summary.json`,
`observer-rollback-verified.json` and `native-evidence-archive-verified.json`.
Raw logs, resource samples, RPC latency samples and service records are copied
losslessly to My Passport; no private-key file is included in that archive.
Additional read-only evidence is `producer-warning-retry-correlation.json`,
`head-mempool-source-baseline-audit-v2.json` and
`read-only-head-mempool-probe-20261018T155204Z.json`. The v2 source audit corrects
an earlier comparison span that included unrelated class declarations; it compares
only the named out-of-class definitions.

The first stop-report parser encounters non-UTF-8 log bytes before taking any
stop action. Tolerant diagnostic decoding preserves the original raw log and
allows the same stop to proceed. The immediate post-stop preflight then finds
the RPC port temporarily unavailable. A later listener/connection inventory is
empty and the same preflight passes without changing the port/configuration.
Observer readiness is subsequently verified, not inferred from that probe.

Local archive extraction initially uses an unsupported system-Python argument.
The already completed archive is recovered with the pinned Python and
exclusive regular-file copies. No remote qualification, deadline, signing,
broadcast or producer start is repeated for these reporting corrections.

Binary SHA-256:
`f086e2d71c04cc0922ccd497a2cb1c63d5265bc1d3190b2e17e1156b5a7d10f4`.
Source manifest SHA-256:
`6456879f2162baf57d6b932054bf92e19569fb8e47c89d109fbd35e11a964ec5`.
The runtime reports `1.3.0-dev.0+d3cfb94455da4a472f651eb4239abbc289e74225`;
the manifest and binary hash identify the candidate beyond that version string.
