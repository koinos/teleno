# LR2 local memory hardening and outstanding whole-node gates

Date: 2026-10-03. Status: **local cache/RPC hardening implemented and tested;
LR2 whole-node exit NOT satisfied. No qualified small-server profile.**

Historical checkpoint below: its single-oversized-entry policy and raw-pointer
ownership gap are superseded by the later
[S1 ownership checkpoint](low-resource-lr2-state-ownership.md). Current reads carry
immutable owners; oversized entries safely bypass retention and retained cache
payload stays within capacity. Earlier measurements/build hashes remain earlier
evidence, not current-source proof. Whole-node/Linux/ASan/TSan gates remain open.

The later [V1-A checkpoint](low-resource-lr2-vm-lifetime.md) protects parsed
modules through shared-owner allocation failure, makes cache publication
optional/failure-atomic and coalesces retained same-ID cold loads. It supersedes
the concurrent-duplicate-parse description below; warm hits stay independent,
instances stay fresh, and metering remains unchanged. Distinct cold IDs serialize
too. Subsequent [V1-B](low-resource-lr2-vm-allocation.md) corrects the pinned
dependency's context-allocation termination and propagates allocation errors
locally, outside consensus reversion handling. Parsed-byte/queue/instance policy
and Linux whole-node qualification remain open.
Its measured controlled burst is not mainnet throughput or natural latency.

The [R1-A follow-up](low-resource-lr2-rpc-bodies.md) supersedes retention of
already-sent responses during idle keepalive. Completed request/response owners
now release after service/write completion; scoped body-capacity diagnostics
do not bound JSON/service/serialization/transport or whole-node memory.

The subsequent [R1-B checkpoint](low-resource-lr2-rpc-batch.md) removes retained
prior batch result trees and the success-envelope result copy. Five paired
synthetic Mac processes reduce attributed peak/total ordinary C++ requested
bytes with identical complete responses. The final wire body remains retained;
aggregate admission, supported races and representative Linux proof stay open.

The subsequent [V1-C checkpoint](low-resource-lr2-vm-capacity.md) supersedes
entry-count-only parsed-cache retention with additional optional attributed
capacity accounting/limits. Default bytes remain unlimited and the count remains
32. This is not physical heap/RSS: allocator overhead, evicted caller-held
modules, waiters and mutable instances remain additional. Representative Linux
policy and complete V1/LR2 gates are still open.

The [V1-D / D1 follow-up](low-resource-lr2-host-memory.md) reproduces unaligned
production result-count writes with functioning UBSan and replaces them with
byte copies at the original sites. Output/argument alias ordering is preserved;
no memory-saving or physical-bound claim follows. Standalone clean ASan/TSan
programs with both installed compilers fail before main, outside Teleno; the
underlying runtime cause remains unattributed and integrated gates stay open.

## Scope and baseline

Further explicit LR2 completion work is tracked in the
[completion matrix](low-resource-lr2-completion-matrix.md) and
[worker report](low-resource-lr2-workers.md). New development data is explicitly
on My Passport, with source retained in Teleno. The earlier results below retain
their source/workload correspondence; they are not whole-node qualification.

Follow-up on 2026-10-03: the scoped accepted-ID P2P retention package is recorded
separately in [its report](low-resource-lr2-p2p-retention.md). It adds bounded
FIFO/byte/expiry caches and exact pending-ID duplicate checks; the earlier
measurements in this report remain evidence for their earlier build only.
Whole-node LR2 qualification remains open, including other P2P consumers.

"Siguente" authorizes the next repository-local LR2 work. This implements a
scoped WP2/WP3 tranche while LR1's selected mainnet source gate remains blocked.
The goal is still backup-based **full-history** observer operation on 2 CPUs,
4 decimal GB physical RAM and a 40-decimal-GB whole-server disk. These Mac
fixtures do not prove that goal. No live server, producer configuration, public
publisher, chain data, source trust, reserve, protocol accounting or history
retention policy was changed. No commit, push, release or deployment occurred.

Checkout: branch `codex/reward-reinvestment-macos`, HEAD
`cfd2eb83b8080ca49911555108ce05f0a9068f01` plus preserved dirty SC/LR work. Native
identity is `1.3.0-dev.0+cfd2eb83b808-dirty`, not a committed integration pin.
The local CMake home is this repository, Release `-O3`, tests ON, Mach-O arm64;
no `teleno-extract` tree was used. Hardware reports 10 physical/logical CPUs and
16 GiB RAM, macOS 26.5, Apple Clang 16.0.0. System Python/compiler invocations
can report x86_64 under Rosetta; ARM64 binary identity is separately verified.
This is not the selected 2-CPU Linux host. Dependencies include locally cached
Boost 1.83, RocksDB 8.8.1 with Zstd and cpp-libp2p revision
`c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`.

Before edits, current identity/cache paths were inspected and existing
RocksDB-manager, persistence-failure and mempool-adapter tests passed 3/3.
The prior LR1 33/33 result is context,
not a rerun in this task. A SHA-256 manifest captured 87 pre-existing
dirty/untracked files in the ignored evidence directory. `README.md` and the
high-throughput implementation plan are preserved byte-for-byte.

Evidence root (local, ignored, not shipped by a clone):
`build-single-copy-evidence/lr2-t12vhw9o/`. Commands retain argv, output, exit,
elapsed time and owned-child resource usage in `commands.json`; initial,
post-review, sanitizer and final verification logs are separate. Early ad-hoc
compiler/test errors were observed in task tool output, not separately captured
as full raw files; later workflow and sanitizer attempts are retained. Raw
toolchain paths stay in ignored logs.

## Implemented behavior

### Object and module caches

- Object hits move the existing LRU node with `splice`; no key/list allocation
  or second map lookup is needed. Overflow-safe payload arithmetic uses
  subtraction before addition. Failed map insertion unwinds the new list node.
- Clear accounting remains the LR1 correction. Replacement, eviction, absent
  values and zero/oversized budgets terminate. A single oversized object is
  retained alone rather than returning an uncached value whose last owner dies
  before the backend consumes its raw pointer. The existing 64-MiB **soft
  payload** default is unchanged. A zero-byte diagnostic budget also permits
  this one-entry working set. It is not a hard heap/RSS ceiling.
- Cache payload/entry/capacity accessors explicitly require the existing external
  mutex. Caller-held shared values may survive eviction and are additional memory.
  Existing borrowed raw-pointer concurrency is not solved by this change;
  TQ-12 remains an investigation gate.
- Module replacement moves/reuses the one existing ID entry. `A,A,B` at capacity
  two retains A and B. Concurrent same-ID inserts cannot create duplicate list
  membership. Zero capacity disables module retention while callers still own
  their parsed module. Hits are allocation-free. Executions still create their
  own instances, with unchanged metering/call depth; concurrent duplicate parses
  outside the cache lock remain possible. The 32-entry default is unchanged and
  is not a parsed-byte budget.

### RPC lifecycle

Accepted sessions are registered shared owners with serialized socket strands.
Read/write work is asynchronous, with no detached per-session threads. Stop
closes acceptance, cancels/closes owned sockets and joins/drains callbacks before
the registry releases service dependencies; repeated start/stop is supported.
Startup thread-creation failure drains started work; failed listener rebind closes
the acceptor for a later attempt. Thread-allocation failure was source-reviewed,
not fault-injected.

`jsonrpc.io-timeout-seconds` defaults to 30, accepts 1–300, and covers each entire
read/write including idle keepalive, incomplete requests and stalled readers.
`jobs` still controls workers and maximum active sessions (default four, zero
maps to one), now capped at 64 with configuration/native range checks. Excess
connections receive best-effort 503 without another worker or retained rejection
session. HTTP body/header limits remain the previous Beast defaults, explicitly
1 MiB / 8 KiB. Health, CORS and JSON-RPC dispatch/conversions remain unchanged.

Already-admitted service calls run to completion, including mutation requests:
disconnect/timeout cannot imply rollback. Stop waits for dispatch before freeing
dependencies. I/O deadlines need worker progress and do not forcibly cancel an
underlying service; no universal execution/shutdown deadline is claimed.
Response/result byte bounds remain a separate open gate. The public/admin
exposure and access policy do not change.

### RocksDB accounting, not new tuning

The linked 8.8.1 `options.h` documents `db_write_buffer_size` across all CFs.
For one DB, a WriteBufferManager would give equivalent flush tracking unless
additional stall behavior were selected. Existing shared block cache and
aggregate memtable threshold already exist; no new manager, strict cache
admission, smaller cache/buffer, disabled sync or compression change was added.

`--storage-report` exposes shared-cache capacity/usage/pinned usage once,
configured aggregate flush threshold and supported aggregate all-memtable and
table-reader estimates. Missing properties are unavailable, not zero; pinned
bytes are a subset of usage. Configured thresholds are not hard process limits.
Checked native range conversion rejects overflow before creating/opening a DB.
The report opens the stopped DB and cannot measure runtime RSS/queues/VM or
whole-host/cgroup memory. Do not run it against another active owner.

## Regressions and reproducible measurements

`koinos_cache_invariants_test` covers clear/reuse, null/zero/oversized inserts,
replacement/recency/eviction, maximum-capacity arithmetic, held shared-value
lifetime across eviction, external-mutex concurrent object access, `A,A,B`,
concurrent module replacement and zero object/module hit allocations for
heap-backed keys. It uses real parsed empty WASM modules, not mutable reused
execution instances. The oversized policy is ownership-preserving, not evidence
that this case is reachable under current protocol limits.

`koinos_jsonrpc_session_lifecycle_test` uses only its own loopback listeners:
20 start/stop cycles, duplicate start/stop, one-slot overload/recovery, healthy
keepalive, idle/no-header and partial-body expiry, oversized-body refusal,
unchanged unknown-service response, stopped partial reads, blocked response
writes, a gated in-flight read-only service dispatch and destruction with an
idle socket. It asserts zero sessions after stop and fixture-defined deadlines.
No external endpoint or live service is tested. Stalled-write data is a bounded
8-MiB fake read-only response; no chain mutation is performed.

### Cache before/after, five processes each

Build and run the same committed measurement fixture:

```bash
cmake --build build --target koinos_cache_invariants_test --parallel 4
./build/koinos_cache_invariants_test --measure
```

Repeat five times per source version in fresh processes. Workload: 50,000
heap-backed keys, 64-byte values, 8-MiB cache, warm-up over every key followed by
100,000 hits. True logical key/value payload is 6,638,890 bytes. The first
before-driver output mistakenly hard-coded 6,688,890; the keys/value sizes and
allocation/RSS observations are unchanged, and the corrected driver computes
payload instead of copying that label. Before raw artifacts remain intact.

| Metric | Before median (min–max; MAD) | After median (min–max; MAD) |
| --- | --- | --- |
| Hit allocations / 100,000 hits | 200,000 (200,000–200,000; 0) | 0 (0–0; 0) |
| Hit-loop microseconds | 23,531 (22,081–26,292; 1,022) | 13,564 (10,456–25,603; 391) |
| Process peak RSS bytes, Darwin wait4 | 36,896,768 (36,896,768–36,945,920; 0) | 36,896,768 (36,864,000–36,913,152; 16,384) |

Artifacts: `cache-before*.stdout/stderr`, `cache-before.json`, `cache-after*`,
`cache-after.json`. Cache/RSS setup includes the key vector, linked runtime and
allocator, not just cache payload. **No process-RSS reduction is demonstrated.**
Host CPU was not isolated; after timing overlaps other diagnostic build work and
has an outlier. The allocation result is deterministic, but these short timings
do not prove mainnet throughput or an overall CPU percentage improvement. There
are no measured underlying DB reads/VM miss rates in this hit-only workload.

### Offline whole-binary smoke

```bash
python3 tools/diagnostics/measure_lr2_memory.py \
  --binary ./build/teleno_node \
  --output build-single-copy-evidence/<NEW-OWNED-OUTPUT> \
  --repetitions 5
```

This bounded tool rejects reused/non-repository evidence targets and nonlocal
binaries. It retains fresh basedirs/logs, copies the repository's Harbinger
genesis and disables P2P, gRPC and production. Only the newly owned process group
is signaled/cleaned up on failure. It samples that PID, checks real health RPC,
one-second idle expiry and clean shutdown with a partial request. It does not
use a backup, process historical blocks, follow peers or measure mainnet load.
Host MemAvailable/cgroup values stay null rather than invented zero.

The first five-run smoke (`observer-final/results.json`) passed all five with
exit 0. Ten RSS samples per run: pooled median 25,264,128 bytes, range
25,214,976–25,378,816, MAD 49,152. Peak median 26,279,936, range
26,148,864–26,296,320, MAD 16,384. Shutdown median 0.0533 s, range
0.0488–0.0542 s. This lightweight genesis smoke is **not** a representative
steady-state memory measurement or a 4-GB whole-host pass. Final source/build
correspondence is separately retained under `verified/` after review.

The final source-corresponding five-run smoke (`verified/observer/results.json`)
also passes all five: pooled RSS median 25,231,360 bytes, range
25,149,440–25,378,816, MAD 81,920; peak median 26,181,632, range
26,099,712–26,296,320, MAD 81,920. Shutdown median 0.0561 s, range
0.0522–0.0599 s. Its recorded native SHA-256 is
`71807aa069ee7aae046bb657a1c0dca2b09608dc44f74f2da0d8bfd6e86623bb`.
The post-review stopped-fixture `--storage-report` passes and reports 256-MiB
cache/aggregate threshold, 2,131 cache bytes (1,667 pinned subset), 18,432
all-memtable bytes and 7,107 reader-estimate bytes. These are reopened genesis
DB properties, not running-mainnet memory measurements.

## Build and diagnostics

Initial complete native verification actually executed:

| Command/check | Actual result | Evidence under root |
| --- | --- | --- |
| `KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh`, from ARM64 shell | PASS 62.40 s, cached repository-local dependencies | `dependencies-arm64.*` |
| `cmake --build build --parallel` | PASS 47.25 s | `build.*` |
| Focused CTest | PASS 7/7, 8.06 s | `focused.*` |
| Full CTest | PASS 35/35, 65.49 s CTest time | `ctest.*` |
| `./build/teleno_node --version`, `--help` | PASS, dirty identity above | `version.*`, `help.*` |
| Separate UBSan build/tests, halt-on-error, vptr excluded | PASS 3/3, 6.53 s CTest time | `sanitizers/ubsan-*` |
| Separate integrated ASan build / tests | Build PASS; both tests fail ASan initialization | `sanitizers/address-*` |
| Separate integrated TSan build / tests | Build PASS; both exit with segmentation fault and no race report | `sanitizers/thread-*` |
| Minimal cache ASan / TSan isolation | Compile PASS; same ASan initialization refusal / TSan early exit without report | `minimal-sanitizers/*` |
| `docker info` | FAIL exit 1: daemon socket unavailable; container smoke not executed | `docker.*` |

Corrected ARM64 workflow invocation (system Python otherwise inherits Rosetta):

```bash
arch -arm64 /bin/bash -lc \
  'KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh'
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/teleno_node --version
./build/teleno_node --help
```

UBSan tree `build-sc3-ubsan-20261002` points to this repository and has
`-fsanitize=undefined -fno-sanitize=vptr -fno-omit-frame-pointer`;
prebuilt dependencies are not fully instrumented. Fresh separate arm64 CMake
trees are `build-lr2-address-kht0nq62` and `build-lr2-thread-yi8m6tt1`, with flags
and configure/build/run argv in sanitizer manifests. ASan explicitly reports
`sanitizer_malloc_mac.inc:189 (!asan_init_is_running)` and an empty stack.
TSan's failure has no attribution/race report; do not call it a detected code
race or a passing test. A functioning integrated sanitizer environment remains
required. Minimal-probe failure does not establish a native node bug.

Intermediate test-driver failures were corrected, not waived: shared_ptr's
explicit bool conversion caused the first compile error; the unknown-service
fixture expected -32601 but unchanged dispatch returns -32603. DB metrics needed
the linked 8.8.1 `advanced_cache.h` definition. A temporary misplaced validation
hunk was removed before successful compilation. Initial minimal compilation
included an unused exception header requiring a Boost macro; removing that
unused production include allowed the isolated build. Retained successful/failing
diagnostic logs identify their source phase; final verification uses separate
`verified/` outputs. Cached workflow success is not a clean-from-empty build.

### Final post-review verification

Final source/build correspondence is retained in `verified/commands.json`.
After review and diagnostic admission-test registration:

| Command/check | Actual result | Evidence under `verified/` |
| --- | --- | --- |
| Full rebuild | PASS, 22.10 s | `post-doc-build.*` |
| Cache/RPC/storage/config/tool focused CTest | PASS 5/5, 6.71 s CTest time | `post-doc-focused.*` |
| Full CTest | **PASS 36/36**, 57.89 s CTest time | `post-doc-ctest.*` |
| Version / help | PASS, dirty native identity above | `post-doc-version.*`, `post-doc-help.*` |
| Post-review UBSan build / focused tests | PASS 3/3, 6.37 s CTest time; same vptr/dependency limitations | `ubsan-build.*`, `ubsan-tests.*` |
| Post-review ASan / TSan builds | PASS compilation; tests still fail 2/2 for each as described above, not qualified | `address-*`, `thread-*` |
| Full-binary observer smoke | PASS five owned offline runs | `observer.*`, `observer/results.json` |
| Stopped owned-genesis storage report | PASS | `storage-report.*` |

The final binary hash equals the measured smoke hash above. Native dependency
script, build, full CTest and CLI are actually executed, not inferred from LR1.
The new admission unit tests prove bad repetition counts, nonlocal binary/output
and reused output fail before spawning or writing node data. They do not replace
the actual five-run observer smoke. No container, mainnet payload, historical
audit or differential validation execution is claimed in this task.

## Changed files in this LR2 task

Native/runtime/tests/tooling:

- `src/CMakeLists.txt`
- `src/core/config.cpp`, `src/core/config.hpp`
- `src/jsonrpc/jsonrpc_server.cpp`, `src/jsonrpc/jsonrpc_server.hpp`
- `src/koinos/state_db/backends/rocksdb/object_cache.cpp`, `object_cache.hpp`
- `src/koinos/vm_manager/fizzy/module_cache.cpp`, `module_cache.hpp`
- `src/main.cpp`, `src/storage/rocksdb_manager.cpp`, `rocksdb_manager.hpp`
- `tests/core/config_test.cpp`, `tests/storage/rocksdb_manager_test.cpp`
- New `tests/jsonrpc/session_lifecycle_test.cpp`, `tests/storage/cache_invariants_test.cpp`
- New `tools/diagnostics/measure_lr2_memory.py`, `tests/scripts/lr2_memory_test.py`

Documentation/handoff:

- `AGENTS.md`, `CLAUDE.md`, `CHANGELOG.md`, `docs/README.md`
- `docs/configuration.md`, `docs/rpc-endpoints.md`, `docs/logs-and-diagnostics.md`
- `docs/release-builds.md`, `docs/memoria.md`
- `docs/implementation/low-resource-mainnet-observer-plan.md`
- `docs/implementation/teleno-audit-remediation-plan.md`
- `docs/performance/teleno-software-audit.md`
- New `docs/validation/low-resource-lr2-memory.md` (this report)
- Koinos One docs only:
  `docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md`,
  `docs/manual/koinos-one/backup-and-restore.md`.

Koinos One's own instructions/worktree were read and its existing dirty docs
extended, not replaced. Strict MkDocs passed; no application/packaging test is
claimed because no GUI/package/pin/binary change was authorized. App version and
app changelog are unchanged. Authorized future integration needs an actual
committed native revision, staged/rebuilt binary identity and package tests;
there is no exact new commit to pin yet.

## Coverage, progress and remaining gates

Final documentation checks: `git diff --check` passed in both repositories;
`cmp AGENTS.md CLAUDE.md` passed; 86 local Markdown file references in changed
native documents resolved with no missing files. Koinos One strict MkDocs passed
in 0.35 s. SHA-256 comparison retains 72 of 87 pre-existing dirty/untracked
native files unchanged; the other 15 were deliberately extended for LR2. The
complete task changes 31 Teleno files listed above and the two existing dirty
Koinos One documents. `README.md`, the high-throughput plan and unrelated native
backup/replay changes remain unchanged. Full source-preservation and final binary
correspondence are in `verified/source-preservation.json`; these are local checks,
not GitHub publication or release/package validation.

| Work package | Implemented invariant / evidence | Exit |
| --- | --- | --- |
| LR2 cache tranche | Terminating ownership-preserving oversize policy; unique LRUs; measured zero warm-hit allocations; concurrent shared-owner fixtures | Local functional tranche verified; borrowed-pointer lifetime and parsed-byte/miss-rate gates open |
| LR2 RPC tranche | Owned asynchronous sessions, session count/read/write bounds, stop joins/drains; overload/idle/partial/write/restart/dispatch/destruction tests | Local fixtures verified; integrated lifetime/race and universal service-work/result budgets open |
| LR2 DB accounting | Count one shared cache, separate estimates/pinned subset; no altered thresholds/fsync/compression/schema | Local API/CLI verified; representative memory/flush-stall workload open |
| Whole-node memory profile | Offline genesis tool, explicit absent host/cgroup metrics, unchanged defaults | NOT satisfied; no experimental profile selected |
| Protocol/history compatibility | Existing controller/lookahead/pending-root/historical fixture regression coverage | Local regression only; historical audit/full differential execution not performed |
| Docs handoff | Native docs/changelog/plan/TODO/memory plus docs-only Koinos One handoff | Local docs only; packaging/release/deployment pending separate authority |

Required follow-up before closing LR2:

1. Establish conservative byte/lifetime accounting and compatible policy for P2P
   seen-ID growth, global mempool across accounts, fork/delta retention, RPC
   response/results and VM parsed modules/instances. Existing per-account nonce
   limits, transaction expiry, session counts and module counts are not whole-node
   byte bounds. Do not evict historical records or change transaction validity to
   fit RAM. P2P dedup eviction must prove unchanged duplicate/fork behavior and
   no inappropriate peer scoring; it is not safe to merely truncate the sets.
2. Resolve/verify TQ-12 borrowed-pointer read concurrency with a functioning
   lifetime/race environment before changing uncached ownership semantics. The
   current singleton oversize policy avoids introducing that new failure.
3. Measure mixed cold/warm state/VM/RPC/P2P/admission overload, retained heap,
   flush/compaction, normal/stop/cancel cleanup and throughput/miss-rate tradeoffs.
   Source-backed candidate 128/256-MiB caches/memtables and 1/2 background workers
   are not yet selected or accepted profile values.
4. Supply an eligible authenticated/pinned mainnet backup and full historical
   corpus, durable roots and reference/successor range. LR1's inspected concrete
   backup has missing declared chain identity; LR2 did not re-authenticate mutable
   `latest`, rewrite metadata, publish or deploy the corrected writer.
5. Obtain an isolated Linux x86_64/ext4 feature build and whole-host/cgroup
   workload: prove <=2-GiB representative steady RSS, <=2.5-GiB peak, >=768-MiB
   minimum MemAvailable, no OOM/swap concealment and block-following/catch-up
   capacity. Full historical/differential and LR5 soak/capacity gates remain open.
6. Restore working container and integrated sanitizer tooling before claiming
   those checks pass. Do not start external services or alter host protections
   as an implicit workaround.

Rollback is source-only in a review/test checkout, with no DB format/data change.
Removing these local fixes needs no chain reset; preserve user work and the
independent backup. Older configs remain usable unless they requested over 64
JSON-RPC jobs; review that intentional range check and the new deadline before
deployment. No reduced-memory default is released by this task.

**Every repository-scoped LR2 Definition of Done item has NOT been proven.**
This is a verified local hardening tranche, not completion of LR2, the audit,
historical parity, a release or full-history operation on the selected server.
