# teleno_light: deferred bounded-history node concept

Date: 2026-10-03. Status: **deferred concept only; no implementation authorized.**

## Product boundary

The user requires ordinary Teleno to retain the complete history from genesis.
Its [active resource-adaptation plan](low-resource-mainnet-observer-plan.md)
must use lossless savings and must not substitute a pruned node when a full
archive cannot fit the selected disk.

`teleno_light` is a provisional name for a different future node/variant with
different historical-service requirements. Recording this idea creates no
repository, binary, package, release, CLI mode or submodule integration. A
future request must explicitly scope its design and implementation. Whether
to share source, use a separate target or maintain another repository remains
undecided; do not fork or duplicate the runtime merely from this note.

## Proposed meaning of “light”

This idea is a **full-state, bounded-history node**, not a header-only or SPV
client and not a promise of trustless snapshot bootstrap. It would retain:

- Genesis/network identity and complete current account, contract and system state.
- A verified durable-state anchor and every required fork/replay/recovery record.
- A recent canonical block/receipt window; 250,000 blocks is an initial research
  candidate, not an implemented policy or a fixed number of offline days.

Older historical bodies/receipts could be retired only after proving safety,
crash durability, coherent readers and physical reclamation. Historical RPC
and peer availability would explicitly disclose limited coverage. This differs
from ordinary Teleno's complete archive and must never be a hidden auto-fallback.

## Existing research and required future gates

The [bounded-history producer design](bounded-history-producer-design.md)
contains earlier retention, ancestry, checkpoint, export and recovery analysis.
It is research, not implemented functionality or permission to activate a
producer. The safest initial future scope would be observer-only.

Before implementation, separately establish:

1. Exact retention and full-state contracts, including source trust and durable-
   root/fork/reader pins. Stored root metadata alone cannot authenticate all state.
2. Checked persistence, coherent prune/export transitions, restart recovery and
   bounded physical compaction overhead under injected failures.
3. Retention-aware ancestry/skip lists, explicit RPC/history coverage and actual
   legacy-peer/checkpoint/missing-gap compatibility.
4. Canonical results/roots/receipts and replay invariants matching an archival
   reference for recorded input streams; no weakened consensus checks.
5. Actual memory/disk/long-running measurements and a growth/finality-stall
   response. Complete state and required fork tails can still exceed a quota.
6. Distinct operator documentation, source/build identity and deliberate GUI/
   packaging integration. Returning to an archive requires validated backfill
   or archival restore, not merely changing a flag.

No live data may be pruned to explore this idea. Tests would use new isolated
fixtures/export destinations and preserve the source. Existing databases,
producer settings and backup retention remain untouched.
