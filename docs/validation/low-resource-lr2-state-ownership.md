# LR2 state read ownership and retained-payload checkpoint

Date: 2026-10-03. Status: **local ownership correction verified; S1 and LR2 not qualified**.

## Target, scope and authoritative baseline

Linux VPS optimization/qualification remains primary: Ubuntu/x86_64/ext4,
2 vCPUs and 4 decimal GB physical RAM. The Mac is the development host, not a
substitute for those gates. No live server, producer or existing VM was operated.

Source: dirty `/Users/pgarcgo/code/teleno`, branch
`codex/reward-reinvestment-macos`, HEAD
`cfd2eb83b8080ca49911555108ce05f0a9068f01`, `1.3.0-dev.0`.
New build, fixture and evidence data remains in the task-owned
`/Volumes/My Passport/teleno-lr2-EEMOFW/` run, on mounted APFS/USB storage.
Evidence files below are in its `evidence/` directory, also reachable via ignored
`build-single-copy-evidence/lr2-completion-qKjj7h/`.
Mac ARM64/16-GiB/Apple Clang 16 Release measurements are supplementary only.

Previous goal turn is **progress**, not a no-progress/blocker turn: it implemented
the native admission boundary, accurate diagnostics, tests and documentation,
with current-source 38/38 evidence. This turn confirms/fixes an additional
ownership defect and completes the previously running dependency gate.

## S1-A: confirmed buffer release during read eviction

An actual `rocksdb_backend::get()` returned a borrowed string pointer after
releasing its cache lock/local shared owner. A second read of a 65-MiB fixture
value exceeded the existing 64-MiB soft cache and cleared the first entry.

The deterministic regression tracks deletion of the first string's live data
buffer through an allocation/deallocation probe. It checks deletion before any
further dereference, avoiding intentional use-after-free. No concurrent write
or close is involved: another **read** alone suffices. Outer shared database
locks do not exclude other readers or protect the cache payload lifetime.

Failed baseline: `s1-reproduction-test.log`, CTest exit 8, 4.26 seconds:
`held_buffer_deleted_after_read_eviction=1`.
Build log: `s1-reproduction-build.log`. This is a confirmed allocation-lifetime
failure at the actual API, not merely a suspicious source pattern. The probe
does not replace ASan/TSan and makes no claim about all possible races.

## S1-B: carry immutable ownership to consumption

Backend/delta/merged-state and public native state read APIs now return
`shared_ptr<const string>` ownership. Direct reads and next/previous iterator
results retain the selected value before releasing temporary iterators. There
is no implicit raw-pointer result, thread-local workaround or immediately
destroyed uncached owner. Callers must retain the returned owner through use.

RocksDB cache hits transfer existing ownership without payload copies; cold
reads move the fetched string into its immutable owner. State-map entries store
immutable owners. A cloned map has independent entries but shares payloads;
put replaces an owner rather than mutating existing bytes. Read/clone values
therefore survive overwrite, erase, cache eviction, iterator destruction and
database close. The last owner releases evicted bytes.

This changes native C++ source interfaces, not RPC/protobuf schemas or on-disk
formats. Full native/tool/test builds verify local callers compile. Existing
database/node locking remains required: immutable payload ownership does not
make concurrent map mutation, commit or close generally safe.

Key ordering, state bytes, tombstones, root algorithms, canonical history,
receipts and durability are unchanged. Current deterministic root/replay/
lookahead/fallback and persistence regressions pass; real historical differential
qualification remains required and was **not** executed.

## S1-C: bound retained object-cache payload

With ownership carried to consumers, an individually oversized value can safely
bypass cache retention. It no longer clears unrelated warm entries or requires
the old single-oversized-entry exception. A zero-capacity internal/test cache
retains no entries, including negative entries. `payload_bytes() <= capacity_bytes()`
holds, with the default still 64 MiB.

This is a bound on retained key/value payload accounting, **not** heap/RSS.
Map/list/key capacity, string/control blocks, allocator retention and caller-held
or evicted owners remain additional. Cache entry/metadata and active-owner budgets
remain LR2 requirements; do not count them as solved by this change.
A backend constructor budget supports isolated component tests; no new CLI/YAML
setting or low-resource default profile is selected.

Uncached oversized/zero-budget reads may increase disk/cache lookup and allocation
work. Caller-owned values can outlive cache entries and temporarily increase
working sets. Immutable map ownership adds control-block/reference-count cost,
especially for short inline strings. Linux workload measurement must govern
final resource choices rather than this Mac fixture alone.

## Functional verification

New `koinos_state_value_lifetime_test` covers:
- Concurrent read-caused eviction, first-reader survival and last-owner release.
- Immutable clone sharing and isolated original/clone replacement.
- Owner survival after erase, clear and backend/clone destruction.
- Direct/next/previous state results surviving iterator destruction, removal,
  replacement and close.
- Fresh pending roots, normal absent removal and preserved replay tombstones.
- Persistent public read results surviving read eviction and close.
- Oversized/zero-budget uncached ownership, missing reads and reopen persistence.

The final fixture uses a 2-KiB test-only cache and 1-KiB values for deterministic
eviction without a large working set. The original 64-MiB/65-MiB failed baseline
is retained separately. Backend default capacity is unchanged.

Cache invariant tests now prove oversized bypass preserves normal warm entries,
zero budget retains nothing, owned bypass results survive, warm hits allocate
zero, and concurrent owners remain valid. They no longer assert the superseded
single-oversized-entry retention policy.

| Current check | Actual result | Evidence |
| --- | --- | --- |
| Cached-dependency build all | Exit 0 | `s1-current-build.log` |
| Cached-dependency CTest | 39/39, exit 0, 163.17 seconds | `s1-current-ctest.log` |
| Required fresh external dependency script | Exit 0 | `external-dependency-make-workflow.log` |
| Fresh-dependency build all | Exit 0 | `s1-clean-deps-build-all.log` |
| Fresh-dependency CTest | 39/39, exit 0, 204.15 seconds | `s1-clean-deps-ctest.log` |
| Current UBSan subset | 5/5, exit 0, 11.97 seconds | `s1-current-ubsan-build.log`, `s1-current-ubsan-tests.log` |
| Version/help in both builds | Exit 0 | `s1-current-{version,help}.log`, `s1-clean-deps-{version,help}.log` |

Native identity in both builds: `1.3.0-dev.0+cfd2eb83b808-dirty`.
Cached-dependency native SHA-256:
`68ac8e89f63cb5e052ea67aadca61de0c131973d5dd93cc23c44d7d1dfaf3980`.
Fresh-dependency native SHA-256:
`70b4be64fd866f97702958aeeef1e5560cd6deb979a6e7a9f25020e0195b2e0b`.
Current lifetime-test SHA-256:
`e4ca3650ace6ebd6787313ddfe05676f50d7f3735a84abf05b867e752ad5d241`.
Build paths/dependencies differ, so binary hashes are recorded separately.

UBSan uses the separate external `ubsan/` tree with
`-fsanitize=undefined -fno-sanitize=vptr -fno-omit-frame-pointer` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
Vptr checks and complete dependency instrumentation are excluded. ASan/TSan and
Linux/container checks remain unavailable, not passes.

Earlier intermediate results (`s1-focused-tests.log`, `s1-ubsan-tests.log`,
`s1-final-ctest.log`) precede final strict retention/test changes or overlap a
subsequent rebuild. They are not used as final current-source proof.

## Scoped before/after clone measurements and tradeoffs

Five fresh processes per implementation/input; one discarded clone warm-up;
16 clones retained simultaneously. Large fixture: 128 entries of 64 KiB each.
Small fixture: 4,000 entries of 16 bytes each. Timings cover clone creation only;
peak RSS includes source construction, runtime and warm-up. Benchmark driver
overhead is not separately profiled and other dependency builds were concurrent.
This is synthetic map/backend evidence, not whole-node or Linux data.

| Fixture / implementation | Median clone time | Min–max time | Time MAD | Median peak RSS | RSS min–max |
| --- | --- | --- | --- | --- | --- |
| Large / copied payloads | 10,575 us | 10,470–10,842 us | 76 us | 189,186,048 bytes | 189,136,896–189,218,816 bytes |
| Large / immutable shared payloads | 64 us | 63–89 us | 1 us | 20,905,984 bytes | 20,856,832–20,938,752 bytes |
| Small / inline copied values | 2,513 us | 2,448–2,991 us | 65 us | 15,532,032 bytes | 15,450,112–15,548,416 bytes |
| Small / immutable shared payloads | 4,055 us | 2,714–4,292 us | 237 us | 15,794,176 bytes | 15,761,408–15,843,328 bytes |

The large fixture eliminates repeated payload copies. **The small fixture
regresses in both median time and observed peak RSS**; inline-copy savings are
too small to offset ownership/allocation costs there. Do not generalize either
fixture to mainnet, transaction throughput, total-node RAM or a percentage VPS
gain. Repeat representative Linux mixed-size/fork workloads before qualification.

Artifacts: `s1-map-baseline-{large,small}-{1..5}.json`,
`s1-map-owned-{large,small}-{1..5}.json`, `s1-measurement-summary.json`.
Modes: `koinos_state_value_lifetime_test --measure-map-clones` and
`--measure-map-clones-small`. Baseline and after use the same mode/input;
the after samples precede the unrelated strict cache policy but already use the
immutable map implementation. No cached/historical-node comparison is claimed.

## Reproducible commands

From this repository, for current local regression:

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/dependency-workflow-alias-build'
export PYTHONDONTWRITEBYTECODE=1
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 4
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
```

The required dependency script used `KOINOS_BUILD_TESTS=ON`, Ninja,
command-local `MAKE=/usr/bin/make`, `JOBS=4`, external `TMPDIR`,
`KOINOS_DEPS_ROOT`/`HUNTER_ROOT` through the space-free ignored external alias
and external `KOINOS_NODE_BUILD_DIR`, invoked with
`/usr/bin/arch -arm64 /bin/bash ./scripts/build-cpp-libp2p-koinos.sh`.
`dependency-workflow-manifest.json` records exact environment/argv, physical
paths, three failed attempts and the fourth successful attempt. Existing
internal dependencies/workspaces were not trusted as this fresh run's build.
Both CMake caches name this exact source and tests ON.

## Changed files and documentation handoff

S1 runtime/test files (earlier edits within these files are preserved):

- `src/CMakeLists.txt`
- `src/koinos/state_db/backends/backend.hpp`
- `src/koinos/state_db/backends/iterator.cpp`
- `src/koinos/state_db/backends/iterator.hpp`
- `src/koinos/state_db/backends/map/map_backend.cpp`
- `src/koinos/state_db/backends/map/map_backend.hpp`
- `src/koinos/state_db/backends/map/map_iterator.cpp`
- `src/koinos/state_db/backends/map/map_iterator.hpp`
- `src/koinos/state_db/backends/rocksdb/object_cache.cpp`
- `src/koinos/state_db/backends/rocksdb/object_cache.hpp`
- `src/koinos/state_db/backends/rocksdb/rocksdb_backend.cpp`
- `src/koinos/state_db/backends/rocksdb/rocksdb_backend.hpp`
- `src/koinos/state_db/backends/rocksdb/rocksdb_iterator.cpp`
- `src/koinos/state_db/backends/rocksdb/rocksdb_iterator.hpp`
- `src/koinos/state_db/backends/types.hpp`
- `src/koinos/state_db/merge_iterator.cpp`
- `src/koinos/state_db/merge_iterator.hpp`
- `src/koinos/state_db/state_db.cpp`
- `src/koinos/state_db/state_db.hpp`
- `src/koinos/state_db/state_db_types.hpp`
- `src/koinos/state_db/state_delta.cpp`
- `src/koinos/state_db/state_delta.hpp`
- `tests/storage/cache_invariants_test.cpp`
- `tests/storage/state_db_durability_test.cpp`
- `tests/storage/state_db_pending_root_test.cpp`
- `tests/storage/state_value_lifetime_test.cpp`

Documentation updates: identical AGENTS/CLAUDE, CHANGELOG, docs index,
configuration/diagnostics/release gates, active plan/completion matrix/project
memory, historical memory report annotation and scoped audit/remediation status.
This report is new. Koinos One documentation only updates its current native
backup implementation reference and manual backup/restore page. No app code,
package, GUI control, staged binary or submodule pin is changed; there is no
new native commit to pin. Exact dirty identities above are not a shipped release.

Koinos One strict MkDocs passes, exit 0, with generated output in external
`koinos-one-s1-manual/` and log `s1-koinos-one-mkdocs.log`. Its worktree still
changes only the same two documentation files; no package check is claimed.

README/high-throughput plan remain hash-identical. Initial dirty work is
preserved; no commit/push/release or chain/state-data deletion/reset occurred.
Owned successful test fixtures clean up only their validated fresh child;
failed reproduction evidence/fixture remains retained on My Passport.

## Exit audit and remaining prerequisites

Local S1 ownership/bypass correctness is proven by current regressions.
**Full S1 and LR2 exits are not proven.** Required remaining work includes:
- Physical map/fork/tombstone/iterator/caller-owned accounting and lifetime bounds.
- Global mempool/transient admission, RPC output and VM/P2P working-set policies.
- Supported integrated ASan/TSan with relevant dependencies and real lifecycles.
- Representative Linux/x86_64 whole-node memory/catch-up/recovery evidence.
- Authenticated pinned full-history checkpoint, scar successors and matching
  reference corpus from before 30,504,202 through beyond 32,789,378, followed by
  actual current state-delta audit/differential execution.

The missing authorized isolated Linux environment and eligible historical corpus
remain exact prerequisites. No VM approval, live operation, trust weakening,
historical run or unexplained mismatch acceptance is inferred. LR5 server/long
soak remains separate.

Rollback reviews this checkpoint's ownership/retention hunks in a stopped owned
checkout, preserving earlier dirty work and evidence. No DB format migration or
reset is required. Reverting restores the confirmed read-eviction lifetime fault.

**Every applicable LR2 Definition of Done item proven: NO. Goal remains active.**

## Cumulative goal inventory and preservation

The goal has changed 72 Teleno files relative to its captured dirty baseline,
not 72 newly created files. Preservation manifest `s1-source-preservation.json`
records 66 initial files unchanged, 45 extended, 19 previously clean tracked
files modified and eight new untracked files. No initial file is removed or
unexpectedly changed, and nothing is staged. Protected README/high-throughput
plan hashes still match. The 140 local Markdown file references checked exist;
anchors and remote link availability are not included in that check.

- `docs/implementation/low-resource-mainnet-observer-plan.md`
- `docs/memoria.md`
- `docs/validation/low-resource-lr2-completion-matrix.md`
- `docs/validation/low-resource-lr2-memory.md`
- `docs/validation/low-resource-lr2-p2p-retention.md`
- `tests/artifact_root.hpp`
- `tests/backup/direct_activation_test.cpp`
- `tests/backup/direct_restore_harness_test.py`
- `tests/backup/direct_restore_test.cpp`
- `tests/backup/durable_publication_test.cpp`
- `tests/backup/restore_plan_test.cpp`
- `tests/cli/direct_restore_preflight_test.py`
- `tests/cli/direct_restore_workflow_test.py`
- `tests/scripts/fixture_paths_test.py`
- `tests/scripts/lr2_memory_test.py`
- `tests/storage/persistence_failure_test.cpp`
- `tools/backup/direct_restore_fixture.cpp`
- `tools/backup/record_sc0_baseline.py`
- `tools/backup/restore_baseline_fixture.cpp`
- `tools/backup/validate_single_copy_restore.py`
- `tools/diagnostics/fixture_paths.py`
- `tools/diagnostics/measure_lr2_memory.py`
- `AGENTS.md`
- `CHANGELOG.md`
- `CLAUDE.md`
- `docs/README.md`
- `docs/configuration.md`
- `docs/implementation/teleno-audit-remediation-plan.md`
- `docs/logs-and-diagnostics.md`
- `docs/performance/teleno-software-audit.md`
- `docs/release-builds.md`
- `docs/running-observer-node.md`
- `src/CMakeLists.txt`
- `src/p2p/p2p_node.cpp`
- `src/p2p/p2p_node.hpp`
- `src/p2p/types.hpp`
- `tests/backup/backup_sftp_test.cpp`
- `tests/p2p/p2p_one_peer_sync_test.cpp`
- `docs/validation/low-resource-lr2-workers.md`
- `tests/mempool/mempool_adapter_test.cpp`
- `src/core/monolith_rpc_client.hpp`
- `src/koinos/mempool/mempool.cpp`
- `src/koinos/mempool/mempool.hpp`
- `src/main.cpp`
- `docs/rpc-endpoints.md`
- `docs/validation/low-resource-lr2-admission.md`
- `src/koinos/state_db/backends/backend.hpp`
- `src/koinos/state_db/backends/iterator.cpp`
- `src/koinos/state_db/backends/iterator.hpp`
- `src/koinos/state_db/backends/map/map_backend.cpp`
- `src/koinos/state_db/backends/map/map_backend.hpp`
- `src/koinos/state_db/backends/map/map_iterator.cpp`
- `src/koinos/state_db/backends/map/map_iterator.hpp`
- `src/koinos/state_db/backends/rocksdb/object_cache.cpp`
- `src/koinos/state_db/backends/rocksdb/object_cache.hpp`
- `src/koinos/state_db/backends/rocksdb/rocksdb_backend.cpp`
- `src/koinos/state_db/backends/rocksdb/rocksdb_backend.hpp`
- `src/koinos/state_db/backends/rocksdb/rocksdb_iterator.cpp`
- `src/koinos/state_db/backends/rocksdb/rocksdb_iterator.hpp`
- `src/koinos/state_db/backends/types.hpp`
- `src/koinos/state_db/merge_iterator.cpp`
- `src/koinos/state_db/merge_iterator.hpp`
- `src/koinos/state_db/state_db.cpp`
- `src/koinos/state_db/state_db.hpp`
- `src/koinos/state_db/state_db_types.hpp`
- `src/koinos/state_db/state_delta.cpp`
- `src/koinos/state_db/state_delta.hpp`
- `tests/storage/cache_invariants_test.cpp`
- `tests/storage/state_db_durability_test.cpp`
- `tests/storage/state_db_pending_root_test.cpp`
- `tests/storage/state_value_lifetime_test.cpp`
- `docs/validation/low-resource-lr2-state-ownership.md`

The separate Koinos One handoff changes only
`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
`docs/manual/koinos-one/backup-and-restore.md`. Both repositories' diff whitespace
checks pass and Teleno AGENTS/CLAUDE remain byte-identical.
