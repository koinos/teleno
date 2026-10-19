# Native 1.2.1 compatibility qualification

Prepared 6 October 2026. Base: native 1.2.0 commit
`b8dab4c08f99ff1ba951e4bd229a154572b8e4ee`.
Final source version: `1.2.1`; branch: `codex/release-1.2.1-compatibility`.
Publication requires the final version, exact source commit, rebuilt artifacts,
checksums and container gates to agree. Preparing source does not publish it.
The [initial RC checkpoint](compatibility-1.2.1-release-checkpoint.md) preserves
its distinct frozen build and unpublished assets.

## Production scope

Only `src/main.cpp` and `src/block_store/block_store.cpp` change runtime behavior.
The external `chain.invoke_system_call` buffer increases from the actual base
value of 64,000 to 131,072 bytes. This accommodates the selected 109,002-byte
bridge contract plus protobuf framing. VM execution, compute limits, RPC exposure,
production defaults and unfinished LR2 changes are outside this patch.

The store archives an early receipt before finalization sets its state root.
On the later validated accepted notification, the existing-record path can
complete only a missing root. It checks the same serialized block, receipt IDs,
height, every other serialized execution field and a 34-byte SHA-256 multihash
with prefix `1220`. Existing roots cannot be replaced; malformed/conflicting
completion raises an error. An absent entire receipt remains absent.

The existing exclusive store lock covers lookup, validation and write. The
single-record write uses the existing `put_record_bytes` and asynchronous WAL
policy. Skip-list pointers, highest-block topology and other records are
preserved. The validated controller receipt supplies the root; the store does
not independently recompute it. An interruption before the accepted notification
can still leave an omission. Historical repair, database rewrites and pruning
remain separate work.

## Canonical regressions

`koinos_block_receipt_completion_test` links the production store. It checks
accepted notification after topology advance, cold reopen, late/duplicate
preservation, failed Put/retry, fourteen conflicting completion refusals and
absent-receipt preservation. Its RocksDB wrapper injects a failed Put without
owning the real DB.

`koinos_compatibility_node_test` runs the actual native executable with fresh
owned data, loopback-only RPC and disabled P2P and production. Its helper reuses
the repository's deterministic test-only genesis/signing fixture and generates
reference finalized roots with the native shared RocksDB layout. No operator
keys are read or written. The test verifies:

- byte-for-byte retrieval of a 109,002-byte payload through the external syscall;
- an exact 131,072-byte serialized result, oversize refusal and a successful small
  read after refusal, without changing head/state;
- real controller submission, early archive and accepted-notification completion
  for two test blocks, matching independently generated finalized roots and the
  complete execution receipt returned by submission;
- a clean observer shutdown and cold restart, unchanged head, chain identity,
  blocks and receipts, followed by a third block preserving earlier records.

The default CTest payload is deterministic test data. Set
`TELENO_TEST_CONTRACT_BYTECODE` to the selected public WASM for exact bridge
qualification. The runner requires 109,002 bytes and SHA-256
`d66facf63456ff6b2690d7e6756142008b11d985886d8ce10411864eb6992a49`.
The WASM is an external qualification input, not bundled into Teleno source.
This proves retrieval and receipt persistence in an isolated native observer;
it does not execute the bridge contract or qualify a Vortex deployment.

The backup missing-object regression now selects `db/CURRENT` from the latest
snapshot manifest. Its previous directory-order selection could remove an
unused historical object and produce a false CI failure. This is a test-only
correction. CI selects owned temporary/test roots explicitly. The GMP download
URL now uses the kernel.org GNU HTTPS mirror; version 6.3.0 and pinned SHA-256
`a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898`
are unchanged. The GNU redirect and GMP project endpoints both timed out in
official image builds. The mirror's downloaded bytes match the same pin.
Download waits are bounded, and Docker builds GMP before its long dependency
build so a source-download failure is visible early. Failed image builds are
retained as failures, not qualification.

## Repeatable qualification

Use a clean checkout or hash-verified Git export of the exact source commit,
Ubuntu 24.04/x86_64, pinned dependencies and owned build/test storage. Keep source
manifests, compiler/configure commands, logs and binary identity. For an export,
set `TELENO_GIT_COMMIT_OVERRIDE` to the verified full commit. A copied build cache
is only an optimization: its source paths must resolve to this exact export and
all executed targets must be built from it.

For bounded local qualification, use two CPUs and 5 GiB RAM without container
swap, copied dependency inputs, no network and no live chain/key mounts. Mac
results cannot close Linux operational gates. Generated local data belongs on
the user-selected external volume; an absent volume is a stop condition.

```bash
export PYTHONDONTWRITEBYTECODE=1
export TMPDIR=/absolute/owned/tmp
export TELENO_TEST_ARTIFACT_ROOT=/absolute/owned/test-artifacts
export TELENO_TEST_BUILD_ROOT=/absolute/owned/build
export KOINOS_NODE_BUILD_DIR="$TELENO_TEST_BUILD_ROOT"
export KOINOS_DEPS_ROOT=/absolute/owned/deps
export HUNTER_ROOT=/absolute/owned/hunter
export JOBS=2 CMAKE_BUILD_PARALLEL_LEVEL=2 HUNTER_JOBS_NUMBER=2
export KOINOS_BUILD_TESTS=ON
mkdir -p "$TMPDIR" "$TELENO_TEST_ARTIFACT_ROOT"
./scripts/build-cpp-libp2p-koinos.sh
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 2
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
python3 -B -m unittest -v tests/scripts/benchmark_transaction_tps_test.py
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
```

The user subsequently authorized the full native suite and isolated readback/
restart fixture. The initial eleven-test restriction does not apply to that
follow-up. Record every failure and repair before the final passing run. Test
reader corrections included Koinos hex/URL-safe base64 handling, persisted
reference genesis and two RPC session slots for adjacent connection cleanup.
No generated CTest overlays or edits are part of the source.

## Release and operator boundaries

Require the full native suite, benchmark unit suite, exact selected bytecode
fixture, version/help, runtime libraries, producer guard, official Dockerfile
build/smoke and secret-free source/artifact review. Record final commit, binary
hashes, source manifest and the actual container provenance in artifact metadata.
Rebuild after setting the final version; never relabel the earlier provisional
or RC binaries. Publish only when the applicable gates pass.

This scoped compatibility release does not qualify whole-node memory bounds,
4-GB VPS catch-up, sanitizer/race coverage, historical repair or production
activation. Those remain separate gates for the LR2 work excluded here.

For separately authorized installation, verify source commit, artifact SHA-256,
version, libraries, network, observer role, basedir, configuration and ownership.
Preserve old binary, configuration and data. Check fresh receipts against an
independent reference and exact bytecode, then perform an approved observer
restart. Preserve and diagnose state on a mismatch. Rollback must account for
complete receipts already written; do not reset or prune the database.

Koinos One documentation can describe the native fixes, but its submodule,
binary, GUI and packages acquire them only through a separately authorized
integration pinning the exact native release commit and passing app package
checks. No live node, producer, public chain, Vortex deployment, app packaging
or submodule mutation is part of this qualification.

## Pre-release test evidence

On 6 October 2026, all 22 canonical CTests passed in 15.33 seconds, with
zero failures, on source `b52ff4b94cd4f001ffbe1fe2dfe91596f5c36b70`.
The four benchmark unit tests also passed. The integration CTest used the exact
selected bridge WASM and the two runtime patches are unchanged for the final
version rebuild. The container exited 0 without OOM; the complete source
manifest matched before and after building. All fourteen pre-existing service
IDs, PIDs and start times remained unchanged.

This evidence precedes the final version/commit rebuild. The final artifact
metadata and completion checkpoint must record its own identities and results;
a passing RC-era binary cannot be relabeled as a final release. An official
image builds failed on GNU redirect and GMP project connection timeouts.
The clean hosted native build of `4b52a44d8007883cb52a3a6e85c2f5ae15280580`
passed all 22 CTests and all four benchmark unit tests. The subsequent build
recipe correction keeps the two runtime patches unchanged and requires its own
final artifact identities and official image qualification. Runs superseded by the
corrected fixture/final-source work were cancelled and do not count as passes.
