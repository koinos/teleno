# Native 1.2.1 compatibility release candidate

Prepared 6 October 2026. Base: native 1.2.0 commit
`b8dab4c08f99ff1ba951e4bd229a154572b8e4ee`.
Candidate version: `1.2.1-rc.1`; branch: `codex/release-1.2.1-compatibility`.
This candidate is not a published final release or a live-node update.

## Production scope and review

Only `src/main.cpp` and `src/block_store/block_store.cpp` change production
behavior. The external `chain.invoke_system_call` buffer increases from
64,000 (the actual base-source value) to 131,072 bytes. This accommodates
retrieval of the selected 109,002-byte bridge contract plus protobuf framing.
The capacity change does not alter VM execution, compute limits or RPC exposure.
Actual large-contract retrieval using this candidate remains unqualified.

The store archives an early receipt before finalization sets its state root.
On the later validated accepted notification, the existing-record path can
complete only a missing root. It checks the same serialized block, receipt IDs,
height, every other serialized execution field and a 34-byte SHA-256 multihash
with prefix `1220`. Existing roots cannot be replaced; malformed/conflicting
completion raises an error. An absent entire receipt remains absent.

The existing exclusive store lock covers lookup, validation and write. The
single-record write uses the existing `put_record_bytes` and asynchronous WAL
policy. Skip-list pointers, highest-block topology and other records are
preserved. Encoding validation relies on the controller's validated receipt;
it does not independently prove the digest against chain state. Interruptions
before the accepted notification can still leave an omission. No historical
repair, database rewrite, pruning, new defaults or unfinished LR2 work is included.

The canonical `koinos_block_receipt_completion_test` links the production store.
Its test-only RocksDB wrapper injects a failed Put without owning the real DB.
It requires an existing, explicitly selected `TELENO_TEST_ARTIFACT_ROOT` and
retains fresh fixture directories. Temporary overlay builds and edited generated
CTest files are not part of the candidate source.

## Verified prepared-input evidence

Before assembling this candidate, all 251 production/component input files
matched their recorded sizes and SHA-256 values. Manifest SHA-256:
`81450faac970a5534487333a516ea7eb622cfb42d24cb2fde48f3f21ec12b798`.
Input archive SHA-256:
`fde1210b87e76e4e2b5813c4ed1d98fb2e26159d1872c3759f53f9215b1806d0`.
Minimal corrected block store SHA-256:
`53deada5889918566bae8311e72f1ddc1f5158f9563422e8715a2adc614817d6`.

The earlier native Linux/x86_64 build finished successfully without OOM.
The original missing-root defect failed at the expected preservation assertion;
the corrected component passed completion after topology advance, cold reopen,
late/duplicate preservation, failed-write retry, fourteen conflict refusals and
absent-receipt preservation. Eleven focused CTests passed with zero executed
failures. Ten scenario tests were disabled before execution and did not pass.
The earlier modified binary still reported its base 1.2.0 identity and must
never be relabeled or distributed as this candidate. CLI and library checks
of that earlier binary were performed with networking disabled and no chain or
key mounts; they do not qualify the candidate artifact.

## Repeatable build and bounded qualification

Use a clean checkout of the candidate commit, Ubuntu 24.04/x86_64, pinned native
dependencies, fresh owned build/test directories and copied dependency caches.
Keep original caches and unrelated services intact. Limit the build to two CPUs,
5 GiB RAM and no container swap; no chain data or signing secrets are inputs.

A clean dependency build uses the repository recipe:

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
```

A fresh CMake configure can reuse the exact pinned installed dependencies;
retain its command, source manifest, cache and compiler identity as evidence.
For a Git source export, set `TELENO_GIT_COMMIT_OVERRIDE` to the verified full
commit. Require `CMAKE_HOME_DIRECTORY` to name that exact source tree.

Within the currently authorized component-only scope, execute this exact subset:

```bash
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure \
  -R '^koinos_(block_receipt_completion|config|service_registry|rocksdb_manager|state_db_durability|state_db_pending_root|backup_plan|backup_checkpoint|backup_snapshot|backup_sftp|gorpc_codec)_test$'
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
```

No generated CTest edits are needed. Record the excluded controller delta,
indexer lookahead, historical replay fixture, public restore, backup service,
backup admin, peer sync, producer, mempool and gRPC scenario tests as **not run**.
Do not start additional simulated chains, forks or laboratory node deployments.
The exact current build/test/container outcome belongs in the completion
checkpoint and artifact metadata; this source report does not predeclare a pass.

## Publication and operator gate

A final release requires version/commit-matched artifacts, focused and broader
native test evidence, CLI identity, container verification, checksums, operator
notes and a secret-free source/artifact review. The restricted subset is not
complete CTest qualification. Whole-node accepted-receipt readback after
finalization and observer restart, and actual large-contract readback, remain
unqualified. Keep this concrete candidate rather than publish a falsely
qualified final release while those gates are unresolved. GitHub PR CI runs
scenario tests outside this task's scope; do not use it to bypass that boundary.

For a separately authorized maintenance action, verify exact source commit,
artifact SHA-256, version, libraries, network, observer role, selected basedir,
configuration and ownership before staging. Preserve the old binary and data.
After approved installation, check fresh accepted receipts against an independent
reference and exact retrieved bytecode, then perform an approved observer
restart. Preserve existing state and diagnose any mismatch; historical omissions
require separate repair authorization. A rollback plan must retain the original
binary/configuration and account for complete receipts already written; do not
reset or prune the DB. Producer activation remains a separate decision.

Koinos One documentation may describe this native candidate, but its submodule,
binary, package and GUI do not gain the fixes until a deliberately authorized
integration pins this exact native commit and passes its own package checks.
