# Block receipt root completion — development checkpoint

6 October 2026. This source change is not installed on a live node or bundled
into a Koinos One release. Linux whole-node and broader native CTest remain open.

## Reproduced defect

At source revision `b8dab4c08f99ff1ba951e4bd229a154572b8e4ee`, the controller
archives a receipt before finalization sets `state_merkle_root`. The completed
accepted-block notification reaches the store later, but the existing-block
branch returns without updating the incomplete receipt. A read-only comparison
with the reference RPC exposed this omission on an identical irreversible block;
it did not establish divergent chain state.

## Scoped correction

The existing-record path accepts completion of a missing root only when block
bytes, receipt identity/height and every other receipt field match. The new root
must have SHA-256 multihash encoding. A conflicting block, execution receipt or
existing root is refused. Late incomplete repeats cannot erase a complete root.
The exclusive store lock covers the single-record write; skip-list pointers,
highest topology and the existing asynchronous archive WAL policy are preserved.

This relies on the validated controller's accepted receipt. Encoding checks do
not independently prove a supplied digest against chain state. The patch does
not change consensus execution, expose a new endpoint or permit archive repair.
Missing entire receipts are left unchanged. Old omissions, or interruption
between the early write and accepted notification, require separate diagnosis;
no historical archive rewrite, deletion or resync is part of this correction.

Unrelated worktree topology validation and atomic cross-column-family writes are
preserved. The pinned successor carries only this receipt correction, so those
other changes are not silently promoted into a deployment candidate.

## Actual component evidence

A standalone macOS regression uses the actual store, accepted notification,
RocksDB records and cold reopen. The pinned original reproduces the missing-root
failure. Both the minimal pinned successor and the current native worktree pass:

- Completion after the highest topology has advanced, preserving complete block,
  nonempty synthetic execution receipt and topology.
- Cold reopen and late partial/identical repeats preserving the complete record.
- Failed database write with unchanged data, followed by successful retry.
- Fourteen malformed/conflicting completion refusals before and after completion.
- Preservation of a record without an entire receipt.

The first test revision incorrectly expected an incomplete receipt to contain an
expected digest. Its failure is retained. The corrected test checks invalid
encoding before completion and a conflicting valid digest after completion.
The original fixture still fails at the exact missing-root assertion.

This is component evidence with public synthetic inputs, not independent state
root proof, Linux durability, full-node synchronization or producer qualification.
Logs, binaries, temporary databases and receipts are retained on the selected
external development volume. No production process or database was changed.

## Remaining acceptance

Build and execute `koinos_block_receipt_completion_test` on Linux with an explicit
owned `TELENO_TEST_ARTIFACT_ROOT`, then run the relevant broader native CTest.
Qualify whole-node receipt readback after finalization and observer restart.
Check the large-code return correction and enabled metadata independently.
Prepare reviewed maintenance/rollback instructions and obtain approval before
any actual producer binary/configuration change or restart. Verify fresh complete
receipts and exact bytecode after an approved update; historical repair remains
separate.
