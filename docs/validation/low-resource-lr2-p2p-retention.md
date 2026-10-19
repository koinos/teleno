# LR2 disposable P2P accepted-ID retention

Date: 2026-10-03. Status: **scoped local package implemented; whole-node LR2
exit and full-history Linux/4-GB/40-GB qualification NOT satisfied.**

## Scope, baseline and build identity

The user's "Procede" request authorizes the next repository-local LR2 package,
not LR3, other audit packages, live-node/VPS operation, production, publication,
commit/push, release or deployment. Only disposable accepted-ID metadata is
evicted. All archived blocks, transactions, receipts and state remain unchanged.
No new persistent index, schema, source-trust, disk-reserve or durability policy.

Repository `/Users/pgarcgo/code/teleno`; branch `codex/reward-reinvestment-macos`,
HEAD `cfd2eb83b8080ca49911555108ce05f0a9068f01` plus preserved dirty SC/LR work.
Native identity `1.3.0-dev.0+cfd2eb83b808-dirty`, ARM64 Mach-O, not a committed
revision to pin in Koinos One. Local Release CMake home is this repository,
tests and cpp-libp2p ON; no `teleno-extract` reference in the inspected cache.
The dependency workflow reused repository-local cached dependencies, not a
clean-from-empty build. cpp-libp2p revision
`c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`, RocksDB 8.8.1, Boost 1.83.
Host: macOS 26.5 (25F71), Apple Clang 16.0.0, 10 CPUs, 16 GiB RAM; not the
selected 2-CPU/4-decimal-GB Linux host. Tests use synthetic callbacks, not a
public network or an existing basedir.

Before edits, codec/sync baseline passed 2/2 (12.10 seconds). SHA-256 snapshots
capture 103 pre-existing dirty/untracked files: 90 unchanged and 13 deliberately
extended by this package. Five previously clean tracked files and three new
files are also listed in `source-preservation.json` (21 Teleno files total).
No unexpected changes; README and the high-throughput plan are preserved
byte-for-byte. Evidence root is local and ignored, not shipped by a clone:
`build-single-copy-evidence/lr2-p2p-HALtjd/`.

Native binary SHA-256:
`19f0b796f0f008a51774e82373cda992d7d3df680b13963c1979e5d95b0b5ab7`.
Measurement binary SHA-256:
`aaccc37ecd15d6399b4c4346ba650e37f3f390b44e4a0e80693abff628412e89`.

## Implemented invariants and policy

- Replace lifetime-growing sets with a mutex-protected FIFO and ordered index
  borrowing one stable owned key per entry. Hits allocate no key/node and do
  not refresh FIFO position or acceptance time. No probabilistic membership or
  false-positive validity shortcut.
- Block cache defaults: 4,096 entries and 2 MiB owned string-capacity bytes.
  Transaction cache defaults: 65,536 entries and 8 MiB. Acceptance lifetime:
  600 seconds. These are internal `P2POptions` defaults, not new CLI/YAML/GUI
  settings or a selected low-resource profile. Expiry is lazy on cache access
  or stats/log snapshots, not a background deadline. Idle retention stays
  capacity bounded even when periodic logs are disabled.
- Account string `capacity()+1` (including inline capacity), not just length.
  Entry count bounds tree/list metadata; node/allocator overhead, live decoded
  messages and transport queues are additional. Counters are **not** heap/RSS
  totals or hard process limits. Subtractive arithmetic avoids byte overflow.
- Oversized/disabled-cache IDs bypass optional retention, not validation.
  Failed owned-key/list/index allocations do not turn successful application
  into a peer error. Incomplete insertions release ownership and index views.
- LIB purges cached block IDs, including late sync insertion at/below an
  already observed LIB. Current-LIB gossip is ignored before repopulating the
  fork watchdog. Live fork-bomb detection remains independent of cache eviction.
- Evicted live blocks still go to the controller's existing idempotent path.
  A block-store record alone is not trusted as proof of accepted state. No
  canonical/head change or receipt replay exception is introduced here.
- On a transaction-cache miss, native wiring reads exact current mempool ID
  membership before submission. On failure, recheck cache/mempool to handle a
  concurrent accepted duplicate without a peer penalty. Unknown/failed lookup
  is not positive membership; rejected unknown data is not remembered.
- After expiry/reorg, an evicted transaction absent from the current mempool
  undergoes ordinary validation again: a still-valid transaction can be
  readmitted. No permanent historical duplicate exemption is promised. A mined,
  expired or otherwise invalid unknown transaction follows ordinary rejection
  and scoring; no generic nonce/error suppression or transaction-history scan.
  With mempool disabled/unavailable, the exact pending-ID shortcut is absent.
- Stop clears IDs/bytes after stopping transport and disconnects owned EventBus
  subscriptions. Repeated starts do not accumulate those subscriptions. This
  does not qualify all P2P worker/callback lifetime paths.
- Periodic peer snapshots expose retained counts, owned ID-capacity bytes,
  capacity eviction and expiry counters. Native `seen_stats()` also exposes
  limits, hits, LIB removals and bypasses. Counters are cumulative per object;
  clear resets live entries/bytes, not the cumulative diagnostics.

## Tests and isolated measurements

`koinos_p2p_seen_cache_test` proves count/byte bounds, FIFO without hit renewal,
expiry without duplicate renewal, LIB and late-old-ID removal, clear/reuse,
zero/oversized/max-capacity handling, zero allocations for 10,000 warm hits,
owned-key/list/map allocation failure unwind and four-thread access/purge.

The fake-transport sync test now also covers cached versus evicted live block
duplicates, exact pending transaction duplicates, concurrent admission rejection,
lookup failure with normal scoring, valid readmission after removed membership,
10,000 additional distinct block/transaction pairs with advancing LIB, rejected
state-root blocks not cached, four live forks despite a two-entry cache,
stop cleanup, ignored stopped callbacks and repeated EventBus subscriptions.
Existing sync/checkpoint/fork/state-mismatch cases still run. The native
mempool-adapter test checks exact ID presence/absence before and after expiry;
these are local synthetic tests, not historical or differential mainnet proof.

Five separate-process paired growth measurements use the same 500,000 generated
34-byte IDs, no retained input vector, checkpointing at 100,000 and 500,000 IDs.
Legacy mode models the old transaction `std::set`; bounded mode uses the real
new cache at its default transaction limits. It is not the full P2P transport or
whole node. `final-{legacy,bounded}-1..5.jsonl` retain the final samples; earlier
unprefixed pilot samples are separate and excluded from this table.

| Metric | Legacy set | Bounded cache |
| --- | --- | --- |
| Entries at 100,000 / 500,000 inputs | 100,000 / 500,000 | 65,536 / 65,536 |
| Owned ID capacity at both checkpoints | Not instrumented | 2,621,440 bytes |
| Process peak RSS at 100,000, median [min,max] | 12,779,520 [12,746,752,12,779,520] B | 13,107,200 [13,090,816,13,107,200] B |
| Process peak RSS at 500,000, median [min,max] | 57,786,368 [57,737,216,57,786,368] B | 13,107,200 [13,090,816,13,123,584] B |
| Cumulative elapsed at 500,000, median [min,max] | 97,306 [97,170,108,907] us | 209,089 [198,266,214,718] us |
| Capacity evictions at 500,000 | None | 434,464 |

This fixture demonstrates a local cache plateau and lower long-run peak RSS
(about 55.1 to 12.5 MiB), **not** total-node RAM reduction. The new ownership
and eviction machinery has additional work: approximately 2.15 times the
cumulative elapsed time here, and slightly higher RSS at the first checkpoint.
Record this tradeoff; do not claim a CPU/mainnet throughput improvement. Timing
includes generated-key construction, cache/set work and checkpoint output,
starts cold, and is not a warmed execution/latency benchmark. The 100,000-input
checkpoint is the growth warm-up; no pure-cache CPU profile or driver-bottleneck
proof was collected. No sustained node-throughput acceptance follows from it.
Peak RSS uses Darwin `getrusage`, not retained heap, steady RSS, cgroup or host
MemAvailable. Five short pairs are not a soak or proof against allocator growth.

Reproduce from the repository:

```bash
ctest --test-dir build -R 'koinos_p2p_seen_cache_test|koinos_p2p_one_peer_sync_test|koinos_gorpc_codec_test|koinos_mempool_adapter_test' --output-on-failure
./build/koinos_p2p_seen_cache_test --measure-legacy
./build/koinos_p2p_seen_cache_test --measure-bounded
```

Run each measurement mode five times as separate processes. Output is JSONL,
one row per growth checkpoint; preserve mode, count and raw samples. Compare
equivalent generated-key inputs; do not infer archival disk savings from this
RAM-only fixture. No data-retention or storage format change occurred.

## Build and diagnostic evidence

Actual commands/results are recorded below. Logs are under the evidence root;
no pass is inferred from compilation alone.

| Command/check | Actual result | Artifact |
| --- | --- | --- |
| `arch -arm64 /bin/bash -lc 'KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh'` | Exit 0, cached repository-local ARM64 workflow | `dependencies.log` |
| `cmake --build build --parallel` | Exit 0 | `build.log`, `final-build.log`, `post-review-build.log` |
| Initial focused codec/sync/cache CTest | 3/3, 15.79 s | `focused.log` |
| Final focused codec/sync/cache/native mempool CTest | 4/4, 15.19 s | `final-focused.log` |
| Full CTest before final native mempool assertions | 37/37, 63.91 s | `ctest.log` |
| `ctest --test-dir build --output-on-failure`, final sources | 37/37, 56.59 s | `post-review-ctest.log` |
| `./build/teleno_node --version` / `--help` | Both exit 0 | `version.log`, `help.log` |
| Final cache/sync UBSan CTest | 2/2, 16.07 s | `final-ubsan-test.log` |
| Post-review cache/sync/native mempool UBSan CTest | 3/3, 15.83 s | `post-review-ubsan-test.log` |
| Standalone cache ASan compile / run | Compile exit 0; run exit 134, initialization CHECK failure | `asan-build.log`, `asan-test.log` |
| Standalone cache TSan compile / run | Compile exit 0; run exit 139, no diagnostic; cause unattributed | `tsan-build.log`, `tsan-test.log` |
| Docker daemon availability | Exit 1, local daemon socket unavailable; no container smoke | `docker-info.log` |
| Koinos One `mkdocs build --strict` | Exit 0 (final 0.55 s) | `koinos-one-docs.log`, `final-koinos-one-docs.log` |

UBSan uses the existing repository-local `build-sc3-ubsan-20261002` configuration,
`-fsanitize=undefined -fno-sanitize=vptr -fno-omit-frame-pointer`, with
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Changed P2P/tests compile
instrumented; cached third-party dependencies are not fully instrumented.
Standalone ASan/TSan probes compile `tests/p2p/seen_cache_test.cpp` directly with
Apple Clang, `-arch arm64 -std=c++20 -UNDEBUG -Isrc`, respectively
`-fsanitize=address` or `-fsanitize=thread`, and `-fno-omit-frame-pointer`.
ASan reports `sanitizer_malloc_mac.inc:189 (!asan_init_is_running)` with an
empty stack. TSan produces no race report. Neither check passes; no system
protection, sanitizer requirement or daemon configuration was bypassed.

## Changed files and documentation handoff

Documentation checks: both repositories' `git diff --check`, guidance-file
`cmp`, and a nine-document check of 85 relative file references passed. These
are local file checks, not proof that uncommitted GitHub links are published.

Teleno files changed in this package (other dirty work is pre-existing):

- `src/p2p/seen_cache.hpp` (new), `src/p2p/p2p_node.cpp`,
  `src/p2p/p2p_node.hpp`, `src/p2p/types.hpp`, `src/main.cpp`, `src/CMakeLists.txt`.
- `tests/p2p/seen_cache_test.cpp` (new), `tests/p2p/p2p_one_peer_sync_test.cpp`,
  `tests/mempool/mempool_adapter_test.cpp`.
- This new report, `docs/validation/low-resource-lr2-memory.md`,
  `docs/implementation/low-resource-mainnet-observer-plan.md`,
  `docs/implementation/teleno-audit-remediation-plan.md`,
  `docs/performance/teleno-software-audit.md`, `docs/running-observer-node.md`,
  `docs/release-builds.md`, `docs/README.md`, `docs/memoria.md`, `CHANGELOG.md`,
  and byte-identical `AGENTS.md` / `CLAUDE.md`.

Koinos One documentation only, preserving its dirty work:
`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
`docs/manual/koinos-one/backup-and-restore.md`. No app changelog/version, GUI,
submodule pin, native binary/package or publishing changes. The dirty native
identity above has no exact committed integration revision; a later authorized
integration must commit/pin/stage/check the intended native build deliberately.

## Progress log, remaining gates and rollback

Subsequent explicitly authorized completion work owns disconnected sync-worker
generations through join; see [its report](low-resource-lr2-workers.md) and the
[completion matrix](low-resource-lr2-completion-matrix.md). The detached-worker
observation below describes this package's original endpoint, not a claim that
the later code still detaches. Supported race/lifetime, transport and whole-node
qualification remain unproven.

Scoped LR2 P2P package: code, focused synthetic invariants and memory-growth
comparison implemented locally; native build/CLI and full suite checked.
No failed-validation attempt is accepted/cached as valid; exact pending IDs do
not trigger duplicate application penalties. Retention stays disposable and
full archival data remains untouched. Operator/plan/memory/release/audit and
GUI documentation are coordinated, without integrating or publishing a build.

Remaining LR2 gates: transport queues, fork watchdog, peer/error tracking and
peer-sync worker lifetimes; global mempool/response bounds; parsed VM bytes and
instances; borrowed state-cache pointer concurrency; working integrated ASan/
TSan; representative memory, miss-rate and block-following evidence. Existing
detached peer-sync lifetime behavior is not fixed or qualified by this package.
Lazy expiry/count bounds do not constrain caller-held messages or all consumers.

LR1's eligible authenticated full-history source/corpus remains a prerequisite
for actual mainnet qualification. This task did not refresh mutable public
`latest`, deploy the corrected backup writer or download historical payloads.
Required state-delta audit/differential historical ranges, isolated Linux/ext4
full-size restore/reopen/catch-up, combined resource peaks, container smoke and
24-hour/seven-day observer soaks were **not executed**. Prior reports do not
prove current full historical parity. No 40-GB or whole-node capacity claim.

Rollback: review/revert only this package's code/configuration changes on a
stopped owned test checkout while preserving earlier dirty SC/LR work. No DB
migration or data removal is required. Returning to old unbounded sets restores
the known lifetime-growth risk. Never use a whole-worktree reset or delete state.

**Scoped local package exit: satisfied. Every repository-scoped LR2 Definition
of Done item proven: NO. Full-history target qualification: blocked.**
