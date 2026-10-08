# Contract ABI Index Recovery: Problem and Proposed Solution

Date: 2026-10-08.
Status: **PROPOSED - no recovery tool or live repair implemented by this work.**

## Summary

A Mainnet RPC can serve contract reads while its optional ABI index has no
usable entry for that contract. This prevents clients that discover interfaces
through `contract_meta_store.get_contract_meta` from operating normally.
Enabling the service does not backfill metadata from already processed blocks.

The proposed solution is a bounded, resumable repair of the `contract_meta`
index from verified historical data, without resetting chain state, replaying
contracts in the VM, pruning history, or requiring transaction signing.

## Observed Problem and Evidence Limits

The reported kcli failure was:

```text
kcli proposals
Error: The fund contract has no ABI in chain metadata.
```

On 2026-10-08, a read-only check of the configured private RPC verified the
Mainnet chain ID. The repeat at `2026-10-08T06:30:44.602Z` queried the public KFS
contract, `1A5BmMqV5jN5zBrdkhQumAfDZBzXLPBeN9`, using
`contract_meta_store.get_contract_meta` and obtained an empty result:

```json
{}
```

Neither `meta` nor `meta.abi` was present. No service-unavailable error was
returned. A separate same-day read-only check of `https://api.koinos.io`
returned a compatible KFS ABI, including the read methods and `update_vote`.
These are dated RPC observations, not proof of the private server's loaded
configuration, deployment revision, database health, or historical completeness.
Private connection details and wallet material are intentionally omitted.

The local kcli workaround uses a pinned bundled Mainnet interface for KFS reads
when metadata is absent. Voting still requires compatible RPC ABI metadata.
That workaround does not repair Teleno's index or establish voting readiness.

## Findings in the Local Teleno Source

Inspection baseline: branch `codex/lr2-memory-hardening`, HEAD
`cfd2eb83b8080ca49911555108ce05f0a9068f01`, with pre-existing uncommitted changes.
The findings describe that working tree, not a verified running deployment.

| Finding | Source |
| --- | --- |
| The ABI index is a RocksDB column family named `contract_meta`, keyed by contract ID. The block handler extracts `upload_contract.abi` and replaces the stored item. | [ContractMetaStore](../../src/contract_meta_store/contract_meta_store.cpp), `handle_block_accepted` |
| The handler is connected to block-accepted events only when `contract_meta_store` is enabled. | [Startup wiring](../../src/main.cpp) |
| The startup indexer starts after the stored chain head and stops at the block-store target. It does not independently scan older blocks for missing ABI entries. | [Chain indexer](../../src/koinos/chain/indexer.cpp), `prepare_index` and `send_requests` |
| Fast state-delta replay does not emit the block-accepted events used by this metadata handler; its controlled re-execution fallback also suppresses external side effects. | [Controller](../../src/koinos/chain/controller.cpp), `apply_block_delta` |
| The reviewed Mainnet public-bootstrap and public-backup observer templates disable `contract_meta_store`. This is a possible configuration history, not evidence about the user's active configuration. | [Bootstrap profile](../../config/mainnet-public-bootstrap-observer.yml), [backup profile](../../config/mainnet-public-backup-observer.yml) |
| No dedicated ABI reindex command was found in the reviewed native CLI, tools, or documentation. Recoverable optional indexing is already proposed under WP4. | [Command reference](../command-reference.md), [WP4 plan](teleno-audit-remediation-plan.md#wp4--make-optional-indexing-consistent-and-recoverable) |

In the reviewed source, a disabled RPC service throws
`contract_meta_store service not available`. However, `get_contract_meta` also
returns an empty response for some database-read and protobuf-decoding failures.
Consequently, `{}` is not sufficient to distinguish a genuinely absent record
from an unhealthy or undecodable index entry. Diagnostics must make those
states explicit before any repair is applied.

Setting the following flag permits indexing of relevant future accepted blocks;
it does not rewind or reconstruct the historical index:

```yaml
features:
  contract_meta_store: true
```

## Proposed Recovery Design

1. **Inventory without mutation.** Verify the actual binary, network, basedir,
   loaded configuration, storage ownership and index health. Pin a canonical
   irreversible target block by ID and height. Inventory available blocks,
   receipts and existing metadata; report gaps, corrupt entries and conflicts.
2. **Extract into a separate staging area.** Read the pinned branch in bounded
   batches and deterministic block/transaction/operation order. Recover ABI
   candidates from successful uploads with matching transaction receipts.
   Preserve provenance: chain, block ID/height, transaction ID, operation index,
   ABI digest and relevant deployment identity. Never turn an absent ABI, missing
   receipt or read failure into fabricated metadata or silent success.
3. **Resolve compatibility before selecting entries.** Exclude reverted uploads
   and noncanonical forks for this proposed canonical repair view. First confirm
   the required API semantics against the legacy reference: the current handler
   does not inspect head/reverted flags, so canonical-only selection must not be
   presented as an already agreed drop-in behavior. Check deployment history and
   protocol rectifications rather than assuming the last ABI matches current code.
4. **Produce a reviewable dry-run manifest.** Report proposed inserts/updates,
   unchanged records, conflicts and unresolved contracts. Record hashes and
   durable progress so interrupted scans can resume only against the same pinned
   inputs. An incomplete scan must never claim a complete index.
5. **Qualify and apply separately.** Test first on isolated fixtures and a
   verified copy. Live application requires separate operator authorization,
   sufficient capacity, stopped database users and exclusive ownership. Recheck
   source/target identities and stored preimages. Use a reviewed durable
   journal/batch protocol with idempotent resume and rollback, modifying only
   authorized `contract_meta` records, not chain state or block/receipt records.

These are proposed behaviors, not existing CLI options. Start with a scoped KFS
repair if its provenance can be established; a full historical scan may still
be needed if the deployment location is unknown. Fetching a public RPC's ABI
alone is comparison evidence, not a verified historical repair source. Any
external ABI import needs a separately reviewed provenance and trust policy.

## KFS and Missing-ABI Caveats

The Mainnet KFS code is explicitly updated by a protocol rectification in
[rectify.cpp](../../src/koinos/chain/rectify.cpp), during the timestamp window
between `2025-10-31T11:58:00Z` and `2025-10-31T12:00:00Z`. That code updates
bytecode and consensus contract metadata, not this ABI index. A recovered
historical ABI must therefore be checked against the effective deployment;
mere historical publication is not sufficient compatibility evidence.

ABI publication is optional. If the required interface was never present in
the available upload history, scanning cannot invent it or reliably derive it
from WASM. Report the gap and identify a separately verified interface source.
See the [official deployment guide](https://docs.koinos.io/developers/deploy-contract/).

## Required Validation Before Operational Use

- Fixtures: multiple uploads, intentionally absent/empty ABI, reverted
  transactions, forks, repeated delivery and protocol code changes.
- Failure handling: corrupt entries, missing blocks/receipts, read/write/parse
  errors, capacity exhaustion and interruption at each publication step.
- Resume/rollback: pinned-input checks, idempotence, preserved preimages and
  rejection of concurrent mutation, wrong chain or changed source data.
- Scope: unchanged chain head/state roots and all non-target records; successful
  ABI RPC reads after restart on an isolated copy.
- Client checks: read-only kcli discovery and proposals; voting readiness needs
  separate review and must not be inferred from ABI presence alone.
- Resources: bounded memory, batch size and HTTP connections when using RPC;
  limited retries/timeouts, resumable progress and measured total scan I/O.

Implementation, Linux/isolated-copy qualification, actual index diagnosis,
historical KFS provenance and live maintenance remain pending. No native build
or recovery test was run for this documentation-only work. No service was
restarted, no database or configuration was changed, and no transaction was
signed or submitted. This proposal does not close WP4, LR2 or release gates.
