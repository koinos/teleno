# LR1: source eligibility, persistence and native backup correction

Date: 2026-10-03. **Local implementation verified; LR1's mainnet
source-eligibility exit remains blocked.** No full-history Linux/40-GB guarantee.
Implements the authorized subset of the
[low-resource plan](../implementation/low-resource-mainnet-observer-plan.md),
including the user's additional request to correct future native backups.

## Baseline and scope

HEAD `cfd2eb83b8080ca49911555108ce05f0a9068f01`, branch
`codex/reward-reinvestment-macos`, native `1.3.0-dev.0+cfd2eb83b808-dirty`.
Work started with 65 dirty/untracked file fingerprints, including prior SC/LR0
work. Changes extend that work; no reset, commit, push, tag or release occurs.
The README and high-throughput plan are preserved. macOS 26.5 / Darwin 25.5.0,
10 CPUs, 17,179,869,184 RAM bytes; native arm64, system Python x86_64.
Release `-O3 -DNDEBUG`, tests ON, Apple Clang 16, CMake 4.2.3, RocksDB 8.8.1,
Boost 1.83.0; repository-local `.deps/` and `build/`, no foreign CMake home.
This is not a clean-from-empty dependency qualification. The final manifest is
`build-single-copy-evidence/lr1-verified-q4nu5t2y/build-manifest.json`.
Native binary SHA-256:
`7c9b03741948930be58fd922e481d97419dc73636222d696c7b6b5722e6012e3`.
Fixture helper SHA-256:
`96cac09a735e3edc430ddf015e1fb553052000550af19f0f42c0e9db17b8c85d`.

Before fixes, SC0's truncated HTTP harness reproduced `SIGBUS` / exit -10 in
`build-single-copy-evidence/sc0-run-094k8d3q/`. The earlier captured macOS stack
at `sc0-20260930-UuABQJ/fixture-crash-stack.json` locates Boost `error_code::what()`
during throwing HTTP read exception construction. Do not infer that all possible
Boost ABI/error-formatting paths are fixed. The first deterministic persistence
probe failed **0/5** groups before storage fixes; subsequent groups include
initialization, cache clear and process-interruption coverage.

No VPS/live node, existing chain/state DB, real producer key or network transaction
was used. Tests use isolated synthetic inputs only.
Only read-only public metadata was requested externally. No public payload was
downloaded. Local fixtures and publication faults use fresh ignored directories.
Canonical full history, all backed-up block/receipt records, normal deletion,
replay tombstones, fresh roots, exact exception/lookahead/fallback and observer
holds are preserved. Reserve policy, protocol limits and production remain unchanged.

## Source diagnosis and exact blocker

Selected ID: `20261002T114351Z-ms-1790941431745-files-487`, at the existing
Foundation mainnet read-only source. Fresh native HTTPS metadata-only preflight
and replay of previously captured metadata both return **exit 1** with
`failure_code: source_chain_identity_missing`, `ok: false`,
`ready_to_start_observer: false`, `transport_code: 0`. Neither creates a target
database or downloads objects. This explains the earlier generic
`metadata_authentication` refusal; it is not a TLS-bypass workaround.

Evidence: `build-single-copy-evidence/lr1-source-o4q1jkjv/commands.json`,
`https.stdout`, `cached-metadata.stdout` and their stderr. Cached manifest/inventory
digests remain those in the LR0 source review; fresh failed preflight does not
export newly fetched digests, so byte-identical live metadata is not claimed.
No independent mainnet verification signer/digest or complete historical corpus
has been supplied. The earlier signature-path 404 is not proof that no signature
exists elsewhere. Genesis inventory agreement alone does not validate the DB.

Root cause in the native writer: `manifest_json()` omitted checkpoint chain
identity; native promotion read the missing field with an empty-string fallback
and propagated it. Correcting future creation cannot authenticate or repair the
current backup. No legacy adapter is introduced because its required trusted
identity/content prerequisites are missing.

## Implemented invariants

### Backup creation and publication

- Read identity from the **read-only checkpoint**, not the live controller or
  configured network label. Check unified layout, actual chain-ID object against
  genesis, persisted state ID/root/revision/header and its block/receipt for a
  non-genesis checkpoint. Check archive topology is not behind durable state.
- Record `identity_version: 1`, `identity_status: checkpoint_verified`, chain
  multihash in native `0x` hex, state revision/ID/root and highest stored topology.
  `head_height_kind: durable_state_revision` is not live canonical head;
  `lib_height_known: false` is not measured LIB zero. These checks are structural
  identity checks, not independent authentication or historical execution.
- Empty diagnostic DBs may produce an explicitly `uninitialized` snapshot with
  no invented identity. Missing identity in a populated/inconsistent checkpoint
  refuses creation. Native public promotion rejects missing/unverified/malformed
  identity and contradictory state/network declarations before transport.
- Check every checkpoint inventory traversal/stat result. Rehash-check and sync
  reusable objects; corrupt reuse refuses publication without silent repair.
- Checked writes, file sync, close, rename and directory sync publish objects,
  inventory, manifest, COMPLETE, final snapshot directory, then latest. Unique
  metadata temporary files belong only to this operation. Failure is reported,
  even when rename already exposed a complete new generation. No durability
  weakening, format migration, pruning or hidden second restore payload.
- POSIX implementation qualified only by local tests. Windows durable publication
  fails closed; no Windows/power-loss/Linux support proof is claimed. Remote SFTP
  server fsync and the separate Koinos One promotion script are not changed.

### Persistence and reopen

- Non-NotFound state Get failures throw instead of caching absence; both object
  and metadata flush results are checked. Missing objects still return absence.
- Initial metadata and archive block/receipt/topology use checked atomic batches.
  Existing highest-height and equal-height fork behavior/durability are preserved.
  Corrupt/partial existing metadata refuses initialization, not genesis reset.
- Failed state commit retains previous-root backend ownership. Its failed session
  becomes unusable and cannot be silently retried with mutated cache/metadata.
  Successful root ownership changes follow completed synchronous persistence.
- Explicit backend/database close reports failure and releases handles; destructors
  do not throw. Cache clear resets accounting so close/reopen does not retain a
  phantom occupancy. Oversized-cache admission and other WP2/WP3 work remain open.
- Returned Write failures are ambiguous: tests permit coherent pre-state or
  complete post-state, never a partial successful retry. No automatic repair of
  existing chain/archive data is added.

### Transfer and diagnostics

- Direct metadata uses the existing async transfer engine, finite retry/deadline,
  cancellation and 8-MiB upper bound. Object transfer retains exact-size checks.
- Safe typed source codes identify identity, signature, metadata, TLS trust,
  HTTP, partial response and transport errors without exposing raw exception text.
  Numeric transport codes are optional; zero means unavailable. Cancellation
  remains exit 130 with `cancelled: true`, no target/services/producer startup.
- Legacy truncated body reads use the nonthrowing error-code overload and a
  standard typed exception, avoiding the reproduced Boost exception crash.
  Other legacy synchronous transfer routes and blocked kernel/filesystem I/O
  are not claimed wall-clock bounded.
- SC0 fixture v2 uses synthetic default/contract-meta/account-history rows,
  rather than pretending arbitrary bytes are chain/block records. Old v1 evidence
  remains archived; the current helper requires freshly generated v2 inputs.

## Reproducible verification

Final source rerun logs: `build-single-copy-evidence/lr1-verified-q4nu5t2y/commands.json`,
per-command stdout/stderr. Earlier attempts/failures:
`build-single-copy-evidence/lr1-verification-tkjkhfv3/` and
`lr1-final-58kps5xo/` (successful native-architecture dependency workflow).

```bash
# Native ARM64 shell avoids system-Python/Rosetta dependency toolchain drift.
arch -arm64 /bin/bash -lc \
  'KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh'
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/teleno_node --version
./build/teleno_node --help
ctest --test-dir build --output-on-failure \
  -R 'persistence_failure|durable_publication|state_db|backup_|restore_baseline_harness|restore_preflight|controller_delta|indexer_lookahead'
cmake --build build-sc3-ubsan-20261002 --target \
  koinos_persistence_failure_test koinos_durable_publication_test \
  koinos_backup_direct_activation_test teleno_node --parallel 4
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-sc3-ubsan-20261002 --output-on-failure \
  -R 'persistence_failure|durable_publication|direct_activation'
```

The initially plain workflow launched from x86_64 Python selected Hunter
toolchain `4e9d306` instead of native `11f66d7` and exceeded the owned 600-second
bound while building Intel Boost. Its owned process group was terminated; no
successful dependency validation is claimed for it. Explicit ARM64 retry uses
the repository-local native cache. Earlier intermediate test failures exposed
an initialization guard checking emptiness after RocksDB had created diagnostic
files, a publication fixture missing its enabled flag, and typed source failures
not retaining the existing activation-pending journal state. All are corrected;
final reruns, not earlier passes, determine the exit.

The final source rerun passes **33/33 CTest** (49.73 seconds), focused suites
**19/19** (24.25 seconds) and UBSan **3/3** (11.48 seconds), with build/version/help
exit 0. The publication-temporary ownership recheck is included under `final-*`
command names: final native rebuild 9.96 seconds, UBSan rebuild 12.36 seconds. The
ARM64 dependency workflow exits 0 (65.77 seconds); the Intel attempt remains a
recorded timeout, not silently discarded evidence. UBSan is a separate
repository-local configuration with undefined sanitizer, `-fno-sanitize=vptr`
(RocksDB dependency has RTTI disabled), and frame pointers. Dependencies are
not fully instrumented. ASan/TSan and physical fault coverage are not inferred.

Focused coverage includes 8 persistence groups, pre/post atomic write failures,
fresh-process interrupted archive writes/reopen, 5 publication interruptions,
one actual kernel EFBIG short-write failure, 3 publication fault callbacks,
positive checkpoint-derived metadata and public-schema validation, wrong genesis,
receipt/root mismatch, corrupt reusable object and unchanged latest on refusal.
CLI tests cover truncated metadata, oversized input, signatures, timeout,
SIGTERM cancellation, scratch cleanup and absence of target/services. Normal
replay/index/lookahead suites remain required.

Container smoke is blocked by the unavailable Docker daemon/socket. No daemon
was started. Koinos One's `python3 -m mkdocs build --strict` passed (0.36 seconds);
both worktree diff checks and AGENTS/CLAUDE byte equality pass. Preservation
manifest confirms **43/65 initial file fingerprints unchanged**, with the other
22 deliberately extended by LR1; README/high-throughput plan hashes are unchanged.
No unrelated initial dirty content was discarded. Process interruption and injected Status/hook failures
do not prove physical power-loss, actual ENOSPC, full historical parity or
sustained capacity.

## Changed files in this milestone

Native/storage: `src/CMakeLists.txt`; `src/backup/checkpoint_identity.hpp/.cpp`
(new), `checkpoint_manager.cpp`, `durable_publication.hpp/.cpp` (new),
`snapshot_repository.cpp`, `sftp_uploader.cpp`, `public_restore.hpp/.cpp`,
`restore_plan.hpp/.cpp`, `direct_restore_cli.cpp`;
`src/block_store/block_store.cpp`; `src/koinos/state_db/backends/backend.hpp`,
`backends/rocksdb/rocksdb_backend.hpp/.cpp`, `backends/rocksdb/object_cache.cpp`,
`state_delta.cpp`, `state_db.cpp`.

Tests/tools: `tests/storage/fault_db.hpp/.cpp` and `persistence_failure_test.cpp`
(new), `tests/backup/durable_publication_test.cpp` (new),
`tests/backup/direct_activation_test.cpp`, `backup_sftp_test.cpp`,
`restore_plan_test.cpp`, `tests/cli/direct_restore_preflight_test.py`,
`tools/backup/restore_baseline_fixture.cpp`, `record_sc0_baseline.py`.

Documentation: this report, `AGENTS.md`/`CLAUDE.md`, `CHANGELOG.md`,
`docs/README.md`, `docs/backup-restore-cli.md`, `docs/troubleshooting.md`,
`docs/release-builds.md`, `docs/memoria.md`,
`docs/implementation/low-resource-mainnet-observer-plan.md`,
`single-copy-backup-restore-plan.md`, `teleno-audit-remediation-plan.md`,
`docs/performance/teleno-software-audit.md`.
Koinos One docs only: `docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md`,
`docs/manual/koinos-one/backup-and-restore.md`. No app code, packaging, pin,
version or app changelog change. Integration needs an authorized committed native
pin, rebuilt binary and package checks; no new committed pin exists.

## Progress and exit

1. Reproduced SC0 crash and five persistence failures; retained baseline evidence.
2. Implemented typed diagnosis/bounded metadata; identified missing actual source
   identity without bypass; corrected native writer at the user's request.
3. Implemented persistence/publication guards and fault/process tests; corrected
   intermediate test regressions and architecture-specific dependency invocation.
4. Local verification and documentation recorded; preserve full audit TODO and
   required source/historical/Linux/capacity blockers.

**Not every LR1 exit criterion or full-plan Definition of Done is proven.**
Exact remaining prerequisite: a new concrete public backup created by the
corrected writer, with independently selected mainnet identity/trust evidence,
whose actual full block/receipt history and coherent DB can be validated in an
isolated environment. Historical reference ranges/corpus and Linux resource
qualification remain unavailable. Physical power loss, Windows publication,
complete legacy transfer deadlines and representative disk pressure remain open.

External next actions require authorization: deploy the reviewed writer on its
backup-producing host; create a new backup without touching existing chain data;
review/pin metadata and signing policy; publish it through the approved workflow;
then separately authorize isolated validation/deployment and Koinos One pin/package
integration. Do not hand-edit old metadata, activate production, delete data or
reduce reserve to make these gates appear satisfied. LR2–LR6 are not implemented.
