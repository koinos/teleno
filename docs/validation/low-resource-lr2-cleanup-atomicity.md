# LR2 M1-F: failure-atomic fork-head cleanup

Date: 2026-10-03. Status: **local correction; M1/LR2 NOT complete**.

The primary target remains Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB physical
RAM. Mac/ARM64/APFS diagnostics below are correctness and scoped allocation
evidence only. They select no VPS limits and cannot close Linux, mainnet,
whole-node memory, disk, catch-up or recovery gates. Full history and receipts
remain required; no pruning, backup/trust/reserve or live-node changes occurred.

## Reproduced failure and prerequisite

Before enforcing retained-fork capacity, cleanup must not partially release RC,
erase a record, or leave its transaction/nonce indexes when allocation fails.
Previously `remove_pending_transactions()` and `prune()` changed each live fork
head directly, one operation/fork at a time. An ordinary C++ allocation failure
could reject the call after changing some state. Cleanup also used stream-based
protobuf conversion whose `ParseFromIstream()` result was ignored; a stream
allocation failure could produce an incomplete record instead of propagating it.

The fixture has four mutable states: root pending state, block 1 pending state,
and two competing height-2 heads. Three synthetic pending transactions share a
payer, request 100 RC each out of 1,000, and carry fixed heap-backed IDs/signatures.
Removing IDs 1/2 (with duplicate and absent IDs), or expiring those two while
retaining transaction 3, should change only the current heads. Snapshots compare
pending protobuf bytes, per-checkpoint membership/nonces/counts/RC availability,
global reservation and the complete M1-D inventory, including tombstone buckets.
These inputs bypass signed chain validation; they are not mainnet transactions.

Every calling-thread ordinary `new/new[]` ordinal is tested in a separate
baseline process until the first noninjected success. The preserved original
runtime yields:

| Mode | Cases (includes noninjected end) | Unchanged injected rejection | Rejected with changed state, exit 91 | Crash, exit 139 | Noninjected success, exit 92 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Explicit removal | 316 | 85 | 206 | 24 | 1 |
| Expiry | 2,577 | 425 | 2,115 | 36 | 1 |

Examples: removal ordinal 91 rejects with changed state, ordinal 61 crashes;
expiry ordinal 431 rejects with changed state, ordinal 401 crashes. Every invalid
noncrashing case is a rejection, not merely an incorrect test expectation.
`m1f-baseline-cases.tsv` records all actual exit codes; corresponding per-case
logs and `m1f-comparison.json` preserve classification. The initial in-process
audit terminated with exit 139; it is not a completed sweep.

The first staged-publication implementation still crashed at removal ordinal
70. Working UBSan identifies reference binding through a null resource-record
owner in `cleanup_account_resources_on_node()` after incomplete decoding.
`m1f-ubsan-case-remove-70.log` contains the actual stack, not inferred Linux or
ASan/TSan attribution. The cached utility header explicitly ignores the parse
return. Its dependency is read only; no other repository/dependency was edited.

## Correction and invariants

Cleanup decodes pending/resource records directly with checked `ParseFromArray()`
and a checked integer-size conversion, propagating allocation failure and
rejecting ordinary parse failure. Missing required records throw rather than
dereference a null owner with Release assertions disabled. Valid protobuf bytes,
RC arithmetic, expiry order, duplicate/absent-ID behavior and return counts are
unchanged. Other stream conversions elsewhere are outside this checkpoint.

Both public cleanup operations now stage changes in anonymous overlays while
holding the private DB exclusive lock and every acquired fork-head mutex.
No parent changes until every fork is staged, validated and prepared. Empty
cleanup overlays are not published; retained ancestors are not cleaned early.

Additive `anonymous_state_node::commit_all_changes()` accepts distinct writable,
non-root named map parents only. Reset children and separate tombstone bucket
workspace are prepared before publication. Internal `prepare_all_changes()`
holds source/parent lifetime owners and rejects null, root/finalized, duplicate,
aliased/nested or unsupported inputs before any earlier parent is changed.

Publication transfers existing tombstone nodes into the reserved bucket set,
erases removed parent map entries, transfers write map nodes/immutable owners,
and swaps the prepared marker set. Writes take precedence over same-key source
markers exactly as in ordinary anonymous squash. Fixed default map/set
allocators, string hash/comparison and ownership moves require no allocation
in that phase. It does not clone the entire retained map or copy old key/value
payloads. All overlays reset to fresh children of their original parents after
publication; prepared `apply()` is single-use/idempotent.

This is **not** a generic backend/durable transaction or arbitrary allocator/hash
guarantee. Normal `remove_object()`, tombstone-preserving replay, ordinary
`commit()`/`squash()`, persistent flush/commit, fresh pending roots, historical
exception/lookahead/final-target/fallback, canonical head and source history are
unchanged. Block handling reuses the checked cleanup helper, but its own
finalize/create/mutex and LIB allocation safety remain unqualified.

All parent tombstone bucket work is prepared separately: a rejected operation
also preserves bucket counts. Successful publication may choose a different
bucket count. Workspace is O(existing plus changed tombstones); publication
rehashes/transfers existing markers. Overlay iteration adds allocations and
temporary working memory. This is a correctness prerequisite, **not a hard
memory limit, guaranteed cleanup under machine OOM, or unconditional resource
improvement**. A future capacity policy must reserve cleanup headroom and account
for this workspace; it must not make cleanup impossible at its own ceiling.

## Current focused proof

- Fixed removal: **328 cases**, 327 injected unchanged rejections with successful
  retry, then one unfailed success; no invalid result or crash.
- Fixed expiry: **3,593 cases**, 3,592 injected unchanged rejections with successful
  retry, then one unfailed success; no invalid result or crash.
- Every successful case retains transaction 3, frees the removed head nonces/RC,
  and permits same-nonce replacement on both heads while rejecting it on the
  conflicting retained ancestor. Counts refer to the canonical head only.
- State batch: **12** preparation allocation failures preserve source/parent
  entries and retry successfully; actual publication observes zero C++ allocation
  calls. Two parents match ordinary squash roots/serialized entries; inherited/
  local removal, absent normal versus preserved removal, same-key remove/write,
  old markers, immutable owners, lifetime, unsupported batches, reset/reuse and
  empty batches are covered. The existing write-only admission tests remain.
- Corrected two-test checkpoint: **2/2, 5.20 seconds**. Broader results are recorded
  below only after completion, not inferred from previous reports.

Limitations: one-shot ordinary calling-thread C++ allocations, not dependency
`malloc`, aligned new, simultaneous failures, hardware OOM or race proof.
Integrated ASan/TSan, actual Linux and historical differential proof remain open.

## Five-process scoped comparison

Same synthetic four-mutable-state input per process: 32 prior expired independent
accounts establish existing markers, then 64 records with 65,536 signature bytes
each are removed/expired. Setup/prior cleanup precedes the timed operation.
All builds finish first. Each implementation/mode has five separate processes,
fixed nonce/time/inputs, same compiler/dependencies/Release configuration.
No mainnet throughput, node RSS or retained-heap reduction is inferred.

| Mode / quantity | Before median (min–max; MAD) | After median (min–max; MAD) |
| --- | --- | --- |
| Removal C++ allocation attempts | 8,348 (constant) | 6,610 (constant) |
| Removal requested C++ bytes | 36,990,644 (constant) | 9,750,276 (constant) |
| Removal elapsed µs | 44,141 (43,908–44,820; 77) | 956 (656–1,179; 223) |
| Removal process peak RSS bytes | 35,209,216 (35,192,832–35,274,752; 16,384) | 35,110,912 (35,078,144–35,160,064; 16,384) |
| Expiry C++ allocation attempts | 57,350 (constant) | 119,998 (constant) |
| Expiry requested C++ bytes | 40,566,900 (constant) | 14,391,028 (constant) |
| Expiry elapsed µs | 45,347 (44,696–46,044; 287) | 3,527 (3,513–3,799; 14) |
| Expiry process peak RSS bytes | 35,192,832 (35,160,064–35,323,904; 32,768) | 35,143,680 (35,094,528–35,160,064; 16,384) |

Direct decoding removes stream copies, while staged expiry's deeper iterator
path **increases allocation count**. Total requested bytes are lifetime sums,
not simultaneously retained memory. RSS covers the entire fixture process,
including unchanged setup; the small peak differences are not whole-node gains.
The large local timing difference does not establish Linux/libstdc++ performance
or attribution to one subchange. These short synthetic numbers select no policy.
An initial after series overlapped focused tests; it is retained as
`m1f-overlapped-after-*` and excluded from this serialized comparison.

## Environment, commands and artifacts

Repository `/Users/pgarcgo/code/teleno`, branch `codex/reward-reinvestment-macos`,
HEAD `cfd2eb83b8080ca49911555108ce05f0a9068f01`, dirty native
`1.3.0-dev.0+cfd2eb83b808-dirty`. Apple Clang 16.0.0 (clang-1600.0.26.3),
CMake/CTest 4.2.3, macOS 26.5/ARM64, 16 GiB; Release `-O3`, tests/libp2p ON.
Boost 1.83, RocksDB 8.8.1; pinned cpp-libp2p
`c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`. Caches name this exact repository.

All new generated artifacts use the existing owned
`/Volumes/My Passport/teleno-lr2-EEMOFW/{build,ubsan,tmp,evidence}` and separate
`dependency-workflow-alias-build/`; APFS/USB had about 344 GiB free. Existing
dependency installs are inputs. Existing chain/user data is untouched.

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/build'
export PYTHONDONTWRITEBYTECODE=1
cmake --build "$TELENO_TEST_BUILD_ROOT" --parallel 3
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure
"$TELENO_TEST_BUILD_ROOT/teleno_node" --version
"$TELENO_TEST_BUILD_ROOT/teleno_node" --help
"$TELENO_TEST_BUILD_ROOT/koinos_mempool_cleanup_atomicity_test"
"$TELENO_TEST_BUILD_ROOT/koinos_mempool_cleanup_atomicity_test" --case remove 70
for task_mode in remove prune; do
  for task_sample in 1 2 3 4 5; do
    "$TELENO_TEST_BUILD_ROOT/koinos_mempool_cleanup_atomicity_test" \
      --measure "$task_mode" \
      >"$TELENO_TEST_ARTIFACT_ROOT/m1f-after-${task_mode}-${task_sample}.json" 2>&1 \
      || exit "$?"
  done
done
```

`--case` returns 0 for an injected valid case, 91 for invalid state, 92 for a
noninjected valid end case. The bounded baseline harness runs ordinals until 92,
records every exit and retains crash cases. Final CTest increases the ordinal
coverage bound from 3,000 to 12,000 to reach the deeper path's noninjected case;
it does not relax its 120-second timeout or any state assertions.

Required dependency workflow uses the documented M1-E command-local native
architecture/Ninja/MAKE, external dependency/build/Hunter roots and libssh source
archive, with `KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh`; see
[exact environment](low-resource-lr2-transaction-work.md). This rerun reuses owned
dependencies, not a clean-from-empty download/build. Build every target afterward.
Separate UBSan uses `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, the existing
vptr exclusion and uninstrumented dependencies. It is not ASan/TSan qualification.

Ignored evidence alias `build-single-copy-evidence/lr2-completion-qKjj7h/` points
to the external evidence. Key artifacts: `m1f-baseline-cases.tsv`, per-case logs,
preserved binaries, `m1f-{before,after}-*.json`, `m1f-comparison.json`, focused/
build/regression/dependency logs and source-preservation manifest. Retained failed
attempts: first mixed-auto test declaration, unqualified internal exception name,
first staged cleanup crash, exact UBSan diagnostic and incomplete 3,000-ordinal
sweep. A local LLDB launch made no progress and was terminated after about a
minute; no protection was disabled or debugger success claimed.

Preserved original test binary SHA-256:
`05146039f2415a21301616c9cb2ae78856014441efb6002ebfd30e338a41e4ef`.
The isolated-case version adds only diagnostic test entrypoints to that same
original runtime: `f97ffb2b9f568d147086177c64165ce92ffa5d9a1db61447d71b94abc1b4ec25`.

## Complete checkpoint files

Native source/tests:

- `src/CMakeLists.txt`
- `src/koinos/mempool/mempool.cpp`
- `src/koinos/state_db/state_delta.hpp`
- `src/koinos/state_db/state_delta.cpp`
- `src/koinos/state_db/state_db.hpp`
- `src/koinos/state_db/state_db.cpp`
- `tests/mempool/cleanup_atomicity_test.cpp` (new)
- `tests/storage/atomic_write_batch_test.cpp`

Documentation/guidance:

- `AGENTS.md`, `CLAUDE.md`, `CHANGELOG.md`
- `docs/README.md`, `docs/logs-and-diagnostics.md`, `docs/release-builds.md`
- `docs/implementation/low-resource-mainnet-observer-plan.md`
- `docs/validation/low-resource-lr2-completion-matrix.md`
- `docs/validation/low-resource-lr2-cleanup-atomicity.md` (new)
- `docs/implementation/teleno-audit-remediation-plan.md`
- `docs/performance/teleno-software-audit.md`
- `docs/memoria.md`

Koinos One documentation only: current native backup implementation and manual
backup/restore pages. Its instructions/worktree were inspected; existing edits
are preserved. No GUI/config generation/binary/pin/package/app-version changes.

## Final verification and remaining gates

Actual final results, executed serially after all builds completed:

| Check | Actual result / artifact |
| --- | --- |
| Required test-enabled dependency workflow | Exit 0; `m1f-required-dependency-workflow.log` |
| Cached and separate dependency all-target builds | Both exit 0; `m1f-all-build.log`, `m1f-dependency-all-build.log` |
| Focused mempool/state/work suite | 10/10, 25.73 seconds; `m1f-focused-tests.log` |
| Controller receipt/fallback regressions | 1/1, 3.02 seconds; `m1f-controller-focused.log` |
| Cached full CTest | **44/44, 158.39 seconds**; `m1f-full-ctest.log` |
| Separate UBSan subset | **10/10, 37.49 seconds**; `m1f-ubsan-tests.log` |
| Separate dependency full CTest | **44/44, 185.29 seconds**; `m1f-dependency-ctest.log` |
| Both binaries' version/help | Four exits 0; `m1f-cli-exits.tsv` and per-command logs |
| Koinos One strict MkDocs, external output | Exit 0, 0.47 seconds; `m1f-koinos-one-mkdocs.log` |
| Guidance/diffs/local documentation checks | Exit 0; identical AGENTS/CLAUDE, no staged files, 14 documents/157 existing local file links, no whitespace findings; `m1f-final-checks.json` |
| Docker precondition | Exit 1, local daemon socket missing; `m1f-container-precondition.log`; smoke not run |

The first ten-test selector misspelled the controller name; the separately
executed correct selector and both full suites include it. The first after
measurement series overlapped focused tests; only its repeated serialized series
is used above. No regression expectations, deadlines, retry/trust settings or
resource/durability policy were relaxed to pass.

Current SHA-256 values (not release/integration pins):

- Cached native: `b99f484d078220fa474740de94ce363a42769a021982882e9169419d666f6a35`.
- Separate dependency native: `8231cfc44fe2ecbff22a263ea27ce32515fec744cb734b0ae6a0a1f31cbe025d`.
- Cleanup test: `34389837648610024547adff2a090836a92208a21ef984e0a5c093d87108a8e4`.

The checkpoint contains 20 Teleno files and two Koinos One documentation files.
Final preservation compares all 152 pre-existing files: 134 other files unchanged,
18 extended through scoped edits, two added, none removed or unexplained. README
and the high-throughput plan retain their starting hashes. The cumulative LR2
goal inventory has 93 Teleno files, recorded with the full dirty worktree hashes
in ignored `m1f-source-preservation.json`; this report's list is the checkpoint,
not a claim that all current dirty files were introduced by this work.
Final guidance, diff/link/whitespace checks are recorded in that evidence tree.
The external check helper's initial field-name typo was corrected; its retained
error is `m1f-doccheck-initial-error.txt`, not a software test failure or waived
documentation check. Link checks do not validate anchors, remote publication or
packaged capabilities.

Remaining: aggregate retained/caller/metadata/staging capacity and emergency
cleanup headroom; block/LIB mutation-failure proof; RPC outputs, VM instances,
transport/peer bytes; supported integrated ASan/TSan; eligible independently
authenticated/pinned checkpoint and full successor/reference corpus from before
30,504,202 through beyond 32,789,378; isolated Ubuntu/x86_64/ext4 2-vCPU/4-decimal-
GB whole-machine environment. No current historical/differential or Linux memory/
catch-up/durability validation was executed. A missing local Docker daemon also
prevents container smoke. No VM/live node/VPS, commit/push/tag/release/publication,
producer, trust change, LR3/LR4 or GUI/package integration was authorized here.

M1-F's local correction and coverage do not close aggregate M1, S1, whole-audit
or LR2 exits. LR5's long-server/soak remains separate.
**Every applicable LR2 Definition of Done item proven: NO.**
