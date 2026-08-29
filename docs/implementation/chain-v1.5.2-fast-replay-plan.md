# Chain v1.5.2 Fast Replay Correctness Implementation Plan

Status: Repository implementation and WP1-WP6 complete; 1.2.0 release preparation authorized, observer canary pending

Scope: Teleno native chain indexing and state-delta replay

Protocol reference: `koinos-chain v1.5.2` and `koinos-state-db-cpp v1.2.1`

## Objective

Make Teleno's `verify-blocks: false` indexing path reproduce and verify
canonical Koinos history with the same safety properties as `koinos-chain
v1.5.2`:

1. preserve every serialized deletion tombstone during receipt replay;
2. verify a replayed delta while its state node is still writable;
3. use the next block header as the signed expectation for the pending block;
4. re-execute one block when its stored receipt cannot reproduce consensus;
5. accept the one documented mainnet consensus scar only for its exact known
   block/root combination; and
6. halt on every unexplained mismatch.

This is a local replay and availability correction. It is not a hard fork, a
database-format migration, or a change to normal contract-execution semantics.

## Reference Baseline

Implementation must be checked against these immutable upstream releases:

- [`koinos-chain v1.5.2`](https://github.com/koinos/koinos-chain/releases/tag/v1.5.2),
  source commit `0ae99eced8b585c4145424e9c2a28f667796cc66`;
- [`koinos-chain` PR #861](https://github.com/koinos/koinos-chain/pull/861),
  which adds checked replay, one-block lookahead, fallback re-execution, and
  the exact historical exception;
- [`koinos-state-db-cpp v1.2.1`](https://github.com/koinos/koinos-state-db-cpp/releases/tag/v1.2.1),
  source commit `3a1c904e61afbff59e167f50175519e68046e090`;
- [`koinos-state-db-cpp` PR #36](https://github.com/koinos/koinos-state-db-cpp/pull/36),
  which separates normal removal from replay removal and adds an uncached
  pending Merkle root; and
- [the operator and technical explanation](https://medium.com/koinosnetwork/koinos-chain-v1-5-2-making-fast-node-sync-trustworthy-again-24376f834da4).

The port should preserve upstream behavior and constants. Teleno-specific
adaptation should be limited to its monolithic `IRpcClient`, shared RocksDB
layout, event bus, startup lifecycle, tests, and operator documentation.

## Current Teleno State

Teleno already contains important parts of the fix. They must be retained and
extended rather than reimplemented under a second API.

| Requirement | Current implementation | Status |
| --- | --- | --- |
| Dedicated replay removal | `abstract_state_node::remove_object_preserve_tombstone()` and `state_delta::erase(..., preserve_tombstone)` | Present |
| Receipt replay uses preserved removals | `controller_impl::apply_block_delta()` | Present |
| Parent delta-root comparison | `controller_impl::apply_block()` and `apply_block_delta()` | Present, but cannot pass the documented scar |
| Receipt root comparison when populated | `apply_block_delta()` calls `pending_merkle_root()` | Present, but the pending root currently uses the cached root implementation |
| Uncached root for writable nodes | `pending_merkle_root()` delegates to cached `merkle_root()` | Incomplete |
| One-block lookahead | Indexer applies each item immediately | Missing |
| Replay mismatch fallback | No checked replay API or one-time re-execution path | Missing |
| Historical block 30,504,202 | Incomplete receipt is finalized before the next header exposes it | Missing |
| Historical block 32,789,377 | No exact `(block ID, honest root, signed root)` exception | Missing |
| Audit evidence | `tools/state_delta_replay_audit.cpp` inventories receipt roots and legacy entry drops | Present, requires production-path acceptance checks |
| Focused tombstone tests | `tests/chain/controller_delta_test.cpp` | Present, requires lookahead/fallback/scar coverage |

The present implementation can therefore detect some bad replay results, but
it cannot safely repair an incomplete receipt before finalization and it will
reject the documented consensus scar in both fast replay and full execution.

## Protocol Invariants

The implementation and review must preserve all of these invariants:

- Normal `remove_object()` behavior remains unchanged: deleting an absent key
  during contract execution is a no-op.
- Only replay of a serialized historical receipt uses
  `remove_object_preserve_tombstone()`.
- A writable node is never finalized before all available expectations for its
  delta root have been checked.
- A replay mismatch permits at most one fallback through the normal full block
  application path.
- Full re-execution must reproduce the signed expectation or throw. There is no
  retry loop and no generic ignore-mismatch configuration.
- The historical exception is an exact triple, never a height range, a
  network-wide bypass, or a general root allowlist.
- The honest 12-entry state of block 32,789,377 is retained. Teleno must not
  deliberately reproduce the old tombstone-dropping behavior.
- An unexplained mismatch stops indexing at its causal block and leaves the
  last validated state intact.
- Block production remains disabled during restore recovery and until the
  observer reaches and follows network head.

## Work Packages

### WP1: Make Pending Merkle Roots Truly Uncached

Affected files:

- `src/koinos/state_db/state_delta.hpp`
- `src/koinos/state_db/state_delta.cpp`
- `src/koinos/state_db/state_db.cpp`
- a focused state DB test under `tests/storage/`

Implementation:

1. Extract the current root calculation into
   `state_delta::calculate_merkle_root() const`.
2. Keep `state_delta::merkle_root()` as the finalized/cached API: populate
   `_merkle_root` only there.
3. Add an internal `state_node_impl::pending_merkle_root()` that calls
   `calculate_merkle_root()` directly.
4. Route `abstract_state_node::pending_merkle_root()` through that fresh
   implementation.
5. Do not make mutations clear or reuse the finalized-root cache as a substitute
   for the separate API. Matching upstream's two explicit semantics is easier
   to audit.

Tests:

- `pending_merkle_root()` works while a node is writable;
- `merkle_root()` still rejects a writable node;
- a pending root changes after a subsequent `put_object()`;
- a pending root changes after a subsequent preserved tombstone;
- the final cached root equals the last pending root after finalization; and
- normal and preserved removal still agree when the key exists in the parent.

Exit criterion: repeated pending-root inspection cannot return a stale value,
and normal execution semantics are byte-for-byte unchanged.

### WP2: Add the Exact Historical Root Exception

Affected files:

- `src/koinos/chain/rectify.hpp`
- `src/koinos/chain/rectify.cpp`
- `src/koinos/chain/controller.cpp`
- `tests/chain/controller_delta_test.cpp`

Implementation:

1. Port `acceptable_rectified_previous_root(parent_block_id,
   computed_parent_root, claimed_previous_root)` from `koinos-chain v1.5.2`.
2. Record the upstream mainnet constants exactly:
   - parent block ID for height 32,789,377;
   - honest 12-entry computed root; and
   - legacy 11-entry root signed by block 32,789,378.
3. Use the helper in the parent-root check of both full block execution and
   receipt replay.
4. Reuse the same helper when checked replay compares block H with the signed
   expectation in H+1.

Tests:

- the exact triple is accepted;
- the real 12-entry receipt produces the honest root;
- changing the block ID rejects the exception;
- changing the computed root rejects the exception;
- changing the signed root rejects the exception; and
- arbitrary mismatches continue to throw `state_merkle_mismatch_exception`.

Exit criterion: Teleno passes the historical boundary without weakening any
other Merkle comparison.

### WP3: Add Checked Delta Replay and One-Time Re-execution

Affected files:

- `src/koinos/chain/controller.hpp`
- `src/koinos/chain/controller.cpp`
- `tests/chain/controller_delta_test.cpp`

Implementation:

1. Extend the internal delta-application function with an optional
   `expected_root` supplied by the caller.
2. Add the public `apply_block_delta_checked(block, receipt, expected_root,
   index_to)` entry point. Return whether fallback re-execution occurred so the
   indexer can expose a count.
3. Preserve the existing unchecked `apply_block_delta()` entry point for the
   final target block, whose successor header is not yet available.
4. In checked replay:
   - verify the parent root, including only the exact historical exception;
   - create the writable state node;
   - replay all receipt entries with preserved tombstones;
   - validate `receipt.state_merkle_root` when it is populated;
   - compute a fresh pending root;
   - compare it with `expected_root`, allowing only the exact historical
     exception;
   - if it matches, continue to normal finalization; and
   - if it does not match, clear the execution context, discard only the
     writable node, release its DB lock, and mark the block for re-execution.
5. Re-execute through the existing full `apply_block()` path after leaving the
   replay try/catch scope.
6. Fetch the finalized re-executed node and compare its root with
   `expected_root`. Propagate a mismatch immediately.
7. Emit a stable warning containing `delta_replay_fallback`, height, and block
   ID. Do not log receipt values or state contents.

The fallback is expected at block 30,504,202 because its old stored receipt
omits the two Koinos Fund rectification writes. Full execution applies the
historical rectification and reconstructs the canonical root.

Tests:

- clean checked replay returns `false` and finalizes the receipt result;
- an intentionally incomplete/tampered receipt triggers exactly one fallback;
- the fallback produces the same head ID and root as direct full execution;
- an impossible expected root throws after re-execution rather than retrying;
- the last validated parent remains usable after a failed fallback;
- a populated incorrect receipt root fails before finalization; and
- the exact consensus scar does not trigger fallback.

Exit criterion: every checked replay result is either validated before
finalization, repaired once through full execution, or rejected at the causal
block.

### WP4: Add One-Block Lookahead to the Indexer

Affected files:

- `src/koinos/chain/indexer.hpp`
- `src/koinos/chain/indexer.cpp`
- a new focused indexer test under `tests/chain/`
- `src/CMakeLists.txt`

Implementation:

1. Add an optional pending `block_store::block_item` and a fallback counter to
   the indexer.
2. In `verify-blocks: false` mode, do not immediately apply the item just
   pulled from the queue:
   - keep H pending;
   - when H+1 arrives, pass H, its receipt, and
     `H+1.header.previous_state_merkle_root` to
     `apply_block_delta_checked()`; and
   - then retain H+1 as the new pending item.
3. When request processing finishes, apply the target head through the
   unchecked delta path. Its root will be checked by the first subsequent live
   block, matching the protocol's one-block-delayed commitment.
4. Leave the `verify-blocks: true` path sequential and unchanged except for the
   historical root exception in WP2.
5. Log the number of replay fallbacks at successful fast-index completion.
6. Do not finalize a pending item during signal/error shutdown. A normal restart
   must replay it from the block store.

Tests should use a fake or in-memory `IRpcClient`/block store and cover:

- zero missing blocks;
- one missing block;
- multiple request batches while preserving canonical order;
- H is not finalized until H+1 supplies its expectation;
- fallback at an intermediate block;
- an unrecoverable mismatch stops before later blocks;
- the final target item is applied once; and
- cancellation leaves the last validated head intact.

Exit criterion: no historical block with an available successor is finalized
without checking the successor's signed expectation.

### WP5: Align Restore and Recovery Behavior

Affected files:

- `src/main.cpp`
- `docs/backup-restore-cli.md`
- `docs/troubleshooting.md`
- `docs/configuration.md`
- `docs/logs-and-diagnostics.md`

Implementation and rollout decision:

1. Preserve `.backup-just-restored` observer-first behavior and automatic
   block-producer disablement.
2. During the first correctness release, retain current shipped
   `verify-blocks: true` profile values. Changing the operational default is a
   separate performance/operations decision, not part of the protocol fix.
3. After the fast path passes WP6, remove the restore marker's unconditional
   `force_verify` override so a profile that explicitly selects
   `verify-blocks: false` receives checked fast replay. Explicit
   `verify-blocks: true` remains the maximum-validation mode.
4. Replace the current "merkle correction" wording. Full verification during
   catch-up cannot repair an already finalized bad state node inside a restored
   database.
5. Document two distinct operator cases:
   - a healthy node upgrades in place without deleting data or resyncing; and
   - a node already halted on old bad finalized state must preserve its data,
     start the corrected binary as an observer, and, if still halted, rebuild
     chain state from its block store or restore a known-good backup using the
     documented recovery workflow.
6. Keep destructive state movement outside automatic startup. Existing data is
   moved or deleted only after explicit operator approval and failed
   validation-based recovery.

Exit criterion: restore can use the corrected fast path when explicitly
configured, remains observer-first, and never claims to repair an already bad
finalized database in place.

### WP6: Historical, Differential, and Failure Validation

The unit suite is necessary but not sufficient because the defect is a
full-history replay problem.

#### Focused historical fixtures

Add reproducible, secret-free fixtures derived from canonical public data:

- block 30,504,202, its stored incomplete receipt, and block 30,504,203's signed
  expectation;
- block 32,789,377, its honest 12-entry receipt, and block 32,789,378's legacy
  signed expectation; and
- neighbouring normal blocks to prove the exception does not broaden.

Store the source block IDs and fixture hashes with the tests. Prefer the
smallest serialized public fixture that exercises the production code path.

#### Extend the existing audit tool

Update `tools/state_delta_replay_audit.cpp` so its result separates:

- ordinary matching receipt roots;
- known replayable tombstone cases;
- the known incomplete-receipt fallback block;
- the exact consensus scar; and
- unexplained mismatches.

The tool must exit non-zero for an unexpected additional exception or a changed
known constant. It remains read-only with respect to the source database and
writes only to an explicit scratch directory.

#### End-to-end indexing

Run both modes from an immutable block-store copy or verified backup that
starts before block 30,504,202 and extends beyond block 32,789,378:

1. `verify-blocks: false` using checked replay;
2. `verify-blocks: true` using full execution; and
3. the official `koinos-chain v1.5.2` reference at matching checkpoints.

Compare at minimum:

- chain ID;
- final head height and block ID;
- head state-delta Merkle root;
- selected contract reads around both historical exceptions;
- fallback count and fallback height;
- clean restart from checkpoints before, at, and after each exception; and
- continued live catch-up after the indexed target head.

The checked fast path must reach current network head with exactly the known
fallback/exception behavior and no unexplained mismatch. Record the source
backup identity, block range, Teleno commit, reference commit, commands,
duration, and result hashes in a committed validation report.

#### Performance guardrail

Measure blocks per second, CPU time, peak RSS, and storage writes before and
after the change on the same block range. One-block lookahead must remain O(1)
in retained block items. Any material regression outside the isolated fallback
must be investigated before release.

Exit criterion: unit tests, full CTest, historical fixtures, full-range replay,
restart checks, and differential reference checks all pass from a clean
test-enabled build.

### WP7: Release and Consumer Handoff

Affected files after implementation is validated:

- `CHANGELOG.md`
- `VERSION` only if required by the chosen release milestone
- `docs/release-builds.md` if the release checklist changes
- Teleno operator pages listed in WP5

Release sequence:

1. Merge the state DB and controller/indexer changes together; do not release a
   build containing only a subset of the correctness path.
2. Build with `KOINOS_BUILD_TESTS=ON`, run all CTest tests, and run
   `teleno_node --version` and `teleno_node --help`.
3. Build and smoke-test the Linux container.
4. Deploy an observer canary from a pre-exception backup and let it reach and
   follow current head.
5. Restart the canary and repeat head/root/peer checks.
6. Publish the native release only after source, version, changelog, binary
   identity, container label, and validation report agree.
7. Hand Koinos One the exact Teleno commit and native build identity for a
   deliberate submodule update. Do not modify the consumer repository from
   this node-only work.
8. Block-producing deployments upgrade only after observer canaries pass. They
   restart with production disabled and re-enable it after database health,
   head progress, peers, producer address, VHP, and signer checks pass.

## Proposed Pull Request Sequence

Keep review units small while ensuring no incomplete unit is released:

1. **State DB parity:** fresh pending Merkle root plus state DB tests.
2. **Chain parity:** exact historical exception, checked replay, fallback, and
   controller tests.
3. **Indexer integration:** one-block lookahead, indexer tests, and fallback
   telemetry.
4. **Recovery integration:** restore-marker semantics and operator docs.
5. **Historical proof:** fixtures, audit-tool classification, full-history and
   differential validation report.
6. **Release closure:** changelog, version decision, binary/container checks,
   observer canary, and Koinos One handoff data.

PRs 1-3 form one correctness release gate. They may be reviewed separately but
must not be shipped independently.

## Risk Register

| Risk | Control |
| --- | --- |
| Stale pending root after inspection | Separate uncached calculation and mutate-after-read tests |
| Finalizing before the signed expectation is known | One-block lookahead and indexer sequencing tests |
| Fallback discards canonical state | Discard only the still-writable child; assert parent/head after failure |
| Infinite recovery loop | Single fallback, then strict final-root assertion |
| Overbroad historical bypass | Exact block ID plus exact honest and signed roots; negative tests for every field |
| Normal contract behavior changes | Keep ordinary removal API unchanged and test absent-key no-op semantics |
| Restore is assumed to repair old finalized corruption | Correct docs and retain recovery-first/manual state preservation |
| Monolith integration diverges from microservice reference | Differential checkpoint/root tests against tagged upstream releases |
| Producer outage during upgrade | Observer-first canary and explicit production reactivation gate |
| Fast replay performance regresses | Same-host benchmark and O(1) pending-item requirement |

## Definition of Done

The work is complete only when all of the following are true:

- pending roots are fresh and never populate the finalized-root cache;
- normal deletion semantics are unchanged;
- receipt replay preserves absent-key tombstones;
- every replayed block with an available successor is checked before
  finalization;
- block 30,504,202 triggers one controlled full re-execution and continues;
- block 32,789,377 retains its honest 12-entry state while its exact historical
  signed root is accepted;
- every altered version of the historical triple is rejected;
- an impossible replay/full-execution mismatch halts at the causal block;
- both verification modes pass the two historical boundaries;
- a full-range observer reaches and follows current head with no unexplained
  Merkle mismatch;
- clean build, focused tests, full CTest, CLI smoke checks, container smoke,
  restart tests, and differential checks are recorded;
- recovery documentation distinguishes healthy upgrade from already-corrupt
  finalized state; and
- no private operational data, keys, addresses, tokens, or host inventory is
  added to source, fixtures, logs, or reports.

## Out of Scope

- changing Koinos consensus rules or introducing a hard fork;
- changing balances or rewriting canonical receipts;
- making `verify-blocks: false` the default in every shipped profile before
  validation;
- automatically deleting or replacing an operator's state database;
- enabling a producer after restore or upgrade;
- modifying Koinos One GUI, packaging, or manuals; and
- general state DB, RocksDB, indexer, or throughput redesign unrelated to this
  replay-correctness issue.

## Implementation Progress Log

Results in this section are evidence already produced by the current worktree.
The committed validation report contains the reproducible commands and exact
artifacts. A work package is not treated as release-ready merely because its
code is present.

### WP1: Fresh pending roots

- Files changed: `src/koinos/state_db/state_delta.hpp`,
  `src/koinos/state_db/state_delta.cpp`, `src/koinos/state_db/state_db.hpp`,
  `src/koinos/state_db/state_db.cpp`,
  `tests/storage/state_db_pending_root_test.cpp`, and `src/CMakeLists.txt`.
- Invariants: pending roots are recalculated without populating the finalized
  cache; normal absent-key removal remains a no-op; receipt-only preserved
  removal retains an absent-key tombstone.
- Tests: `koinos_state_db_pending_root_test` passes with map and RocksDB
  coverage, explicit flush, commit, close, and reopen persistence.
- Remaining risk: none specific to WP1; full CTest remains part of WP6.
- Exit criterion: satisfied by focused tests.

### WP2: Exact historical exception

- Files changed: `src/koinos/chain/rectify.hpp`,
  `src/koinos/chain/rectify.cpp`, `src/koinos/chain/controller.cpp`,
  `tests/chain/controller_delta_test.cpp`, and the deterministic fixtures under
  `tests/fixtures/chain-v1.5.2-fast-replay/`.
- Invariants: the exception is restricted to the exact block 32,789,377 ID,
  honest 12-entry root, and root signed by block 32,789,378. Full execution,
  parent replay validation, and checked lookahead share the same helper.
- Tests: controller negative mutations and the real 12-entry public receipt
  pass. Canonical serialized block fixtures for 30,504,202, 30,504,203,
  32,789,377, and 32,789,378 are parsed, hashed, linked, and have their block
  IDs recomputed from their headers.
- Remaining risk: none repository-scoped; both modes and the tagged reference
  crossed the boundary in WP6.
- Exit criterion: satisfied by fixture/unit and end-to-end historical proof.

### WP3: Checked replay and one-time fallback

- Files changed: `src/koinos/chain/controller.hpp`,
  `src/koinos/chain/controller.cpp`, and
  `tests/chain/controller_delta_test.cpp`.
- Invariants: a mismatched writable replay node is discarded; full execution
  runs at most once; the expected root is checked before fallback finalization;
  failed fallback leaves the prior validated head; fallback emits no block
  store writes or broadcasts.
- Tests: normal replay, successful fallback, impossible fallback root,
  populated receipt-root rejection, and exact-scar acceptance pass.
- Historical result: checked replay from reference checkpoint 30,504,141 to
  30,504,203 produced exactly one fallback at 30,504,202 and the canonical
  block/root, with zero source writes and zero broadcasts.
- Remaining risk: none repository-scoped; the long-range scar and archive-head
  runs completed in WP6.
- Exit criterion: satisfied for controller behavior and the first historical
  boundary.

### WP4: One-block lookahead

- Files changed: `src/koinos/chain/indexer.hpp`,
  `src/koinos/chain/indexer.cpp`,
  `src/main.cpp`, `tests/chain/indexer_lookahead_test.cpp`, and
  `src/CMakeLists.txt`.
- Invariants: H is checked with H+1 before finalization, only the target is
  unchecked, only one item is retained, and block application is serialized
  with signal/error shutdown so no block starts after cancellation wins. All
  handlers run on the explicitly supplied chain `io_context`; empty queues use
  a bounded asynchronous retry instead of blocking or hot polling. At most one
  block-store response is staged and transferred nonblockingly into the
  bounded queue, so one worker can process responses larger than the queue
  without deadlock. The node now actually runs the configured `chain_jobs`
  worker count while indexing and refuses to enter ready state when indexing
  is cancelled or fails before reaching the block-store head.
- Tests: zero, one, and multiple batches; ordering; one-time target
  application; controlled fallback; unrecoverable halt; no duplicate external
  side effects; deterministic external cancellation with a pending item; and
  single-threaded producer/consumer handler ordering and a 275-block
  single-worker range with oversized batches all pass. The focused executable
  also passed 100 consecutive runs after the final backpressure fix.
- Remaining risk: none repository-scoped; historical checkpoint and
  archive-head durable restarts completed in WP6.
- Exit criterion: satisfied by focused tests.

### WP5: Restore and recovery safety

- Files changed: `src/core/restore_startup.hpp`,
  `src/core/restore_startup.cpp`, `src/main.cpp`, `tests/core/config_test.cpp`,
  `docs/backup-restore-cli.md`, `docs/configuration.md`,
  `docs/logs-and-diagnostics.md`, `docs/running-producer-node.md`, and
  `docs/troubleshooting.md`.
- Invariants: restore preserves the configured verification mode, creates a
  durable observer-recovery hold before consuming the one-shot marker, keeps
  production disabled across restarts, rejects same-start activation, and
  requires a later explicit `--enable block_producer` restart to release the
  hold.
- Tests: marker absence, both verification modes, hold persistence, same-start
  bypass prevention, and later explicit release pass in `koinos_config_test`.
- Remaining risk: the repository container smoke is complete; the operator
  canary remains an external authorization-gated WP7 action.
- Exit criterion: satisfied for repository behavior and documentation.

### WP6: Historical and differential validation

- Files changed: `tools/state_delta_replay_audit.cpp`,
  `tools/historical_indexing_validator.cpp`, historical fixtures and tests,
  `tests/storage/rocksdb_manager_test.cpp`,
  `docs/validation/chain-v1.5.2-fast-replay-validation.md`, and
  `src/CMakeLists.txt`.
- Invariants: the audit is source-read-only, classifies the known fallback and
  exact scar separately, and exits non-zero for changed constants, additional
  entry-drop exceptions, or unexplained mismatches. The production-path runner
  refuses block-store writes and broadcasts and mutates only its explicit
  scratch state. It can emit canonical protobuf JSON for a requested block
  directly from the read-only archive so fixtures do not depend on manual
  binary-field conversion.
- Tests completed: five focused CTest targets pass with Release assertions
  explicitly enabled; the actual checked-fast and
  full controller/indexer paths both reach 30,504,203 from the same reference
  checkpoint with the same canonical block/root. Both exact tagged-reference
  modes agree on that block, root, chain ID, and selected contract reads.
- Historical scar completed: checked-fast reached 32,789,379 with no fallback
  beyond 30,504,202. Teleno checked-fast/full and exact tagged-reference
  checked-fast/full agree at the 32,789,377 scar on final block/root, chain ID,
  and all six selected reads. Restarts at 32,789,377 and 32,789,378 converge
  without mismatch or side effect.
- Performance completed: on the same ordinary 50,000-block range, final
  checked replay achieved 222.26 blocks/second versus 220.96 for the baseline,
  used 28.62 versus 29.89 seconds of CPU, slightly less peak RSS, and the same
  storage allocation and output-operation count. The guardrail first exposed
  and then eliminated a hot empty-queue repost loop.
- Auxiliary full-suite result: 20/20 CTest targets pass. Assertion activation
  exposed and corrected one stale test expectation for a newly created unified
  RocksDB layout; production storage behavior was not changed.
- Completed: the final full archive audit checked 37,019,561 blocks and
  287,709,633 receipt entries. It reported the exact known incomplete-receipt
  fallback and scar, three known replayable tombstone blocks, and 70,730
  duplicate-key receipts from 34,686,822 through 35,526,515. Those receipts
  collapse during replay and therefore require the existing one-time full
  re-execution; they are not consensus exceptions. The corrected audit and a
  state-DB regression test prove the cause, with zero unexplained mismatches,
  entry drops, or changed constants.
- Clean build completed: the exact repository-local dependency command and
  full build exited zero; complete CTest passed 20/20, focused replay/storage
  CTest passed 5/5, and both CLI smoke commands passed. The pinned reference
  validator was also rebuilt from clean exact-version checkouts. After the
  duplicate-key audit correction and regression test, the final clean rerun
  again passed 20/20 in 22.78 seconds and the focused set passed 5/5 in 0.54
  seconds.
- Container validation completed: a Linux/arm64 image built successfully with
  nested dependency parallelism bounded by `JOBS`; native version and OCI
  revision agree, CLI and guarded-producer smoke checks pass, and an isolated
  offline observer reached ready state and stopped cleanly. The exact required
  native build and complete CTest were rerun after those build-identity changes
  and passed 20/20. The final Linux/arm64 image and both native/container
  observer smokes also exercised `chain_jobs=1`; the last clean CTest run
  passed 20/20 after the ready-state failure gate.
- Full-execution historical run completed: the continuous 2,285,236-block run
  from 30,504,143 reached 32,789,379 with the exact checked/reference
  block, root, chain ID, and reads, and zero external side effects. Reopening
  its durable LIB with the final binary reproduced the same result.
- Completed: the continuous production-path checked replay applied 6,515,418
  blocks from durable state 30,504,143 through archive head 37,019,561. It
  emitted exactly 70,731 classified fallbacks, ended at the complete audit's
  exact block/root with zero source write or broadcast, and logged no failed
  re-execution or unexplained mismatch. The same scratch reopened from durable
  LIB 37,019,501 with the clean final binary and reproduced the final block,
  root, chain ID, and six selected reads with zero fallback or side effect.
- Remaining risk: live observer-follow and canary validation are external WP7
  gates requiring explicit authorization; they are not repository-scoped
  historical proof.
- Exit criterion: satisfied for repository-scoped implementation and
  historical/differential validation.

### WP7: Release and consumer handoff

- Files changed: `VERSION`, `CHANGELOG.md`, `docs/release-builds.md`, and
  `docs/release-chain-v1.5.2-fast-replay-checklist.md` contain the
  repository-side behavior, validation gates, exact authorization-gated
  commands, and consumer handoff. `VERSION` identifies the prepared native
  release as `1.2.0`, and the matching changelog section is dated 2026-08-29.
- Invariants: the 1.2.0 native release preparation and publication are
  authorized. That authorization does not extend to operating a live canary,
  producer activation, or a consumer-repository change.
- Tests: all repository build, CTest, focused, CLI, container, historical,
  differential, archive-head, and reopen gates are complete.
- Remaining actions: the observer canary still requires an approved isolated
  basedir and config. Release publication remains gated on that result, and
  the Koinos One submodule handoff requires separate authorization.
- Exit criterion: repository checklist preparation is satisfied; the release
  remains blocked at the external observer-canary gate.
