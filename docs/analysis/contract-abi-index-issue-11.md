# Issue #11: Contract ABI Index Causes and Legacy Comparison

Date: 2026-10-08. Scope: read-only investigation and isolated characterization.
No recovery implementation or operational repair is included.

## Conclusion

The missing-ABI mechanism is **not specific to KFS**. Teleno's optional ABI
index is populated from observed `upload_contract` operations, independently
of executable contract state. An operational contract and synchronized chain
can coexist with missing, empty, outdated or unreadable ABI metadata.

KFS adds a deployment-history complication: its bytecode was rectified by
protocol in October 2025, without writing the optional ABI index. That does
not explain the affected server's empty response by itself. The exact cause
on that server remains unproven because its deployed binary, loaded
configuration, index contents and historical indexing path were not inspected.

The premise that legacy cannot have this problem needs qualification. Its
metadata service also depends on delivered uploads and has no independent
historical backfill in the reviewed source. A legacy installation with an
already populated, preserved metadata database can work while a restored or
late-enabled Teleno index does not. The Armana frontend additionally supplies
its own ABI, so successful frontend operation is not an ABI-index health test.

## Evidence and Source Identities

- [Teleno issue #11](https://github.com/koinos/teleno/issues/11) was open, with
  no comments, when fetched. Its private-RPC observations are treated as dated
  evidence reported by the issue, not repeated deployment diagnostics.
- Local inspection: `codex/lr2-memory-hardening`, HEAD
  `d3cfb94455da4a472f651eb4239abbc289e74225`, with substantial pre-existing
  mixed modifications. The current `contract_meta_store.cpp` is byte-for-byte
  identical to that file in HEAD: SHA-256
  `c8c0b36e58e7f94b9ab3bba2cd5a2de57cf86eb65d859fbe8e6cf376b6d6adbc`.
- [Official Go metadata service](https://github.com/koinos/koinos-contract-meta-store/tree/75b7a0d128ce39a59b2195ae57db83c2af4b5a32),
  revision `75b7a0d128ce39a59b2195ae57db83c2af4b5a32`, matched the local
  reference checkout's HEAD and the remote master revision at inspection.
- [Official chain reference](https://github.com/koinos/koinos-chain/tree/0ae99eced8b585c4145424e9c2a28f667796cc66),
  revision `0ae99eced8b585c4145424e9c2a28f667796cc66`, was fetched independently
  from GitHub. The other local chain checkout was older and was not treated as
  the current upstream reference.
- [KFS contract source](https://github.com/koinos/koinos-contracts-as/blob/4fc33bbe0520a77a89619da1e9e6efe98e7c423c/contracts/fund/assembly/Fund.ts)
  and its published ABI are in `koinos/koinos-contracts-as`, revision
  `4fc33bbe0520a77a89619da1e9e6efe98e7c423c`. `Fund.ts` identifies Julian
  Gonzalez; its inspected commit history lists `joticajulian`.
- [Armana frontend](https://github.com/Armana-group/kps/tree/74cb27da62414d07ca8d2b46539fbfe885ddfe96),
  revision `74cb27da62414d07ca8d2b46539fbfe885ddfe96`, includes
  `lib/abiKoinosFund.ts` and injects it in `lib/utils.ts`. This distinguishes
  the frontend repository from the official contract repository.

These source identities do not identify the running private server or prove
that the latest contract source is the deployed WASM.

## What Was Verified Against Public Mainnet

Between `2026-10-08T07:08:18Z` and `07:08:23Z`, bounded read-only requests to
`https://api.koinos.io` established:

| Observation | Result |
| --- | --- |
| `chain.get_chain_id` | Mainnet: `EiBZK_GGVP0H_fXVAM3j6EAuz3-B-l3ejxRSewi7qIBfSA==` |
| Head before and after the main sample | Height `40026297`, ID `0x1220050ef3dc5ef58f74b3828fabaa49371ae75bb97c0af53c26392e04ff7228fca8` |
| Last irreversible height in those responses | `40026237` |
| KFS `get_contract_meta` | Nonempty ABI, 7,271 UTF-8 bytes, nine methods including `update_vote` |
| KFS `chain.read_contract`, `get_global_vars` selector `776169940` | Returned protobuf bytes successfully |

The returned ABI's exact string SHA-256 was
`26e3d53b6defa489c60aa918e87764ff2ba7aac2e19dadefdef1a3ce8fb5feb4`.
Its nine method definitions matched the pinned official ABI and the Armana
frontend ABI. The complete ABI documents were not identical: protobuf options,
extension declarations and encoded descriptors differed. Matching selectors
does not certify every client-side address conversion or transaction path.

These calls were not explicitly pinned to an irreversible block. Matching
surrounding head responses do not make the independently maintained metadata
index a consensus-authenticated view. The endpoint's implementation and
database history were not inspected, so this is public-RPC success evidence,
not proof of a particular legacy deployment's internal state.

A later attempt to obtain controls for the verified Mainnet KOIN address
received HTTP 403 and was not retried or counted as a successful comparison.
Two earlier exploratory requests used unrelated/unverified addresses and are
excluded from KOIN/VHP evidence. No affected-private-RPC control matrix was
obtained. The generality conclusion instead rests on source inspection and
the generic-contract native characterization below.

## The Three Distinct Data Layers

```text
Historical upload_contract operation
  |-- bytecode and authorization/hash metadata --> consensus state --> VM execution
  |-- ABI string --> accepted-block notification --> optional ABI index --> client discovery

Historical blocks and receipts --> state replay
                             \--> ABI history only if separately indexed
```

1. Executable code and consensus contract metadata live in state objects.
   [The upload thunk](../../src/koinos/chain/system_calls.cpp) stores bytecode,
   its hash and authorization flags. It does not persist the ABI into those
   state objects.
2. Original upload operations retain the ABI in historical block data. This
   permits recovery only when the needed historical uploads are available and
   their provenance and selection policy are established.
3. [ContractMetaStore](../../src/contract_meta_store/contract_meta_store.cpp)
   stores a separate protobuf item, keyed by contract ID in `contract_meta`.
   It observes uploads through block-accepted notifications and replaces the
   existing value. It does not read the ABI from current WASM or contract state.

Consequently, full canonical block/receipt retention is necessary for a
history-based recovery but does not itself mean the ABI index is populated.
State Merkle verification and a ready RPC server do not verify this index.

## Teleno Cause Analysis

### Missing historical delivery and no independent cursor

The [startup wiring](../../src/main.cpp) subscribes the metadata handler only
when `contract_meta_store` is enabled. Blocks processed while it is disabled
cannot populate it. Enabling the feature later permits future indexing; it
does not revisit old uploads. Ordinary calls to KFS do not republish its ABI.

The [startup chain indexer](../../src/koinos/chain/indexer.cpp) begins after the
stored chain head and processes only the backlog up to the block-store target.
It has no independent ABI-index checkpoint, completeness marker or repair
scan. A fully advanced state database therefore gives it no reason to replay
the historical upload that an empty metadata index needs.

This makes late enablement, an initially incomplete snapshot, a legacy-state
conversion without its metadata database, or loss of optional-index entries
plausible explanations. None was established as the affected server's actual
history. A snapshot containing a complete index would preserve those records;
restoring a snapshot is not inherently a cause of their loss.

### State delta replay does not rebuild the index

[The controller](../../src/koinos/chain/controller.cpp) applies receipt deltas
without emitting `koinos.block.accept` on the normal fast path. Its controlled
full-execution fallback suppresses external side effects. The ABI handler
therefore receives no upload from this replay path, even when enabled.

Full verification through `submit_block` has an accepted-block notification
path, but it helps only for blocks actually replayed while the service is
enabled. Switching verification modes on an already synchronized database
does not rewind its head or repair the missing ABI.

The inspected public-bootstrap and public-backup profiles explicitly set
`contract_meta_store: false`, while setting `verify-blocks: true`. They are
evidence of a supported configuration that omits optional indexing, not
evidence of the affected deployment's configuration. Fast replay is a separate
possible history and should not be asserted merely from use of these profiles.

### Empty uploads replace useful metadata

The handler writes the ABI string of every observed upload, including an empty
one. An upload without an ABI can therefore replace a previously useful entry.
With protobuf's default serialization, an item containing only an empty ABI
serializes to empty bytes. Teleno's getter skips an empty value and returns
no `meta`, making this case indistinguishable from a missing entry at the API.

Whether KFS had such a subsequent upload was not established. This is a
reproduced generic mechanism, not the diagnosed KFS history.

### Read, decoding and write failures conceal completeness

The getter returns an empty response both for no stored value and for a failed
RocksDB read or failed protobuf parsing. The writer logs a failed Put and
returns normally, without a durable failed-index checkpoint or recovery retry.
Thus `{}` does not identify absence, healthy storage, or how a gap arose.

In the reviewed JSON-RPC routing, a disabled service produces a
`contract_meta_store service not available` error instead. If the deployed
binary and routing match this code, `{}` favors an available handler with no
usable record over a service disabled at query time. It does not prove that
the handler was enabled when the original upload was processed.

### Fork and reverted-upload handling are additional consistency issues

The handler does not inspect `head` or transaction receipt `reverted` flags.
Direct delivery of those uploads can replace the ABI. There is no stored
canonical provenance or rollback mapping. This matters when selecting a
historical recovery candidate, but no evidence links such an upload to the
reported private-server result.

The observed behavior is not proof that the controller emits notifications
for rejected blocks. The characterization invokes the handler directly and
does not establish an end-to-end fork or transaction-rejection sequence.

## Why a Legacy Installation May Work

The pinned [Go service handler](https://github.com/koinos/koinos-contract-meta-store/blob/75b7a0d128ce39a59b2195ae57db83c2af4b5a32/cmd/koinos-contract-meta-store/main.go)
subscribes to `koinos.block.accept`, extracts each upload's ABI, and stores it
in its own Badger database. It also ignores head/reverted flags and has no
historical scan in its startup path. A continuously maintained or restored
complete metadata database can explain successful discovery; this is an
installation-history explanation, not a special KFS contract capability.

The [reference chain controller](https://github.com/koinos/koinos-chain/blob/0ae99eced8b585c4145424e9c2a28f667796cc66/src/koinos/chain/controller.cpp)
also omits accepted-block broadcasts during ordinary delta replay. Its rare
full-execution fallback can broadcast, unlike Teleno's suppressed fallback,
but this is not a complete indexing strategy. An upload must happen in that
specific fallback block to benefit. This difference cannot explain the private
result without the relevant historical block and indexing evidence.

The [Go MetaStore API](https://github.com/koinos/koinos-contract-meta-store/blob/75b7a0d128ce39a59b2195ae57db83c2af4b5a32/internal/metastore/meta_store.go)
returns errors on backend/decode failures, unlike the C++ getter. However, its
reviewed RPC handler shadows `err` in an inner conditional and does not assign
that failure to the outer RPC error variable. Therefore the internal Go error
handling is not proof that every legacy RPC failure is reported correctly.
This was inspected in source, not exercised through a deployed broker stack.

Finally, [Armana's client construction](https://github.com/Armana-group/kps/blob/74cb27da62414d07ca8d2b46539fbfe885ddfe96/lib/utils.ts)
passes `abiKoinosFund` explicitly. Local kcli instead attempts RPC discovery
before its guarded read-only bundled-interface fallback. The two clients have
different dependencies on metadata discovery. A working KFS frontend and a
failing discovery client can therefore use the same executable contract.

## What Is Special About KFS

[The protocol rectification](../../src/koinos/chain/rectify.cpp) targets
`1A5BmMqV5jN5zBrdkhQumAfDZBzXLPBeN9` in the timestamp interval after
`2025-10-31T11:58:00Z` and before `12:00:00Z`. It replaces bytecode and consensus
hash/authorization metadata when the old hash differs from the designated
replacement. It adds those changes to the receipt's state deltas. It writes
neither an upload operation nor the optional ABI column family.

The replacement hash embedded in that source is
`0x12206d263c191b3a6d7cbd70e24fc5c62159f316f6ec115ba8796f269e2ce41d11aa`.
This is a source constant, not a freshly verified hash of the private server's
effective contract. The latest `Fund.ts` also contains changes after the 2025
rectification; repository HEAD cannot be substituted for deployed-code proof.

Rectification can leave executable code present with no ABI on an index that
already lacks the earlier upload. It does not delete a populated ABI entry,
and it occurs in the protocol reference as well. It is therefore a provenance
and interface-compatibility concern, not sufficient evidence of a
Teleno-exclusive KFS failure.

Historical KFS fixes to system return-buffer sizing and proposal constraints
concern contract execution. They cannot explain a client error raised during
ABI discovery, before the intended proposal read is submitted to the VM.

## Isolated Native Characterization

A fresh probe compiled the current metadata source and reused pinned dependency
inputs from a CMake tree naming this exact repository. All generated files,
logs and the synthetic RocksDB database were placed under:

```text
/Volumes/My Passport/teleno-issue11-analysis-xuRAtH/
```

Nine assertions completed successfully:

1. A missing ABI yields an empty result for an unrelated synthetic contract ID.
2. Ordinary accepted contract calls do not backfill that ABI.
3. Delivering an upload populates the generic contract's ABI.
4. An empty-ABI upload replaces the useful ABI and yields an empty query result.
5. Malformed stored protobuf metadata yields an empty result.
6. An injected database read error yields an empty result.
7. An injected write error returns normally, leaving the metadata absent.
8. A directly delivered non-head upload replaces metadata.
9. A directly delivered reverted upload replaces metadata.

`meta_probe.cpp`, `run-native-probe.py`, `native-commands.json`, source hashes,
compile/link logs and `native-results.log` preserve reproduction evidence.
These assertions characterize existing behavior; they are not recovery tests,
fix validation, representative Linux qualification or production diagnostics.
The ordinary node test suite was not rerun for this analysis-only task.

Other preserved evidence includes `sources.json`, pinned source files,
`rpc-summary.json`, raw successful public responses, ABI comparisons and the
failed-control status. No active database, configuration, runtime service,
producer or wallet was touched. No transaction or GitHub comment was sent.

## Recommended Next Steps

1. Establish the affected node's actual binary/revision, loaded feature flags,
   storage ownership and index health through separately scoped read-only
   deployment diagnostics. Compare KFS with several verified non-KFS contracts
   on that same endpoint, distinguishing service errors from empty records.
2. Identify the historical KFS upload and every later upload, the ABI carried by
   each, relevant receipts and branch provenance. Determine whether the index
   was disabled, replay bypassed it, an empty upload replaced it, or storage
   failed. Do not infer this history from the public ABI alone.
3. Correct index observability and provide inventory plus a deterministic,
   bounded, resumable metadata recovery path as proposed in
   [the recovery plan](../implementation/contract-abi-index-recovery-plan.md).
   Preserve full blocks, receipts and consensus state. A complete chain reset
   is neither necessary nor an appropriate first action for this optional index.
4. Establish the intended legacy-compatible selection behavior before imposing
   canonical-only or reverted-upload filtering. The reviewed legacy handler
   shares these semantics; changing them requires an explicit contract decision.
5. Keep bundled client ABI use distinguishable from index repair and verify
   effective deployment compatibility before voting or any other signing flow.

The issue's metadata-recovery direction is justified and applies beyond KFS.
Its implementation, exact deployed-server diagnosis, historical provenance,
isolated-copy qualification and any live maintenance remain pending.
