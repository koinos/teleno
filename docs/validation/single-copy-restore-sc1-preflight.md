# Single-copy restore: SC1 metadata planning

Date: 2026-10-01. Scope: SC1 of the
[single-copy restore plan](../implementation/single-copy-backup-restore-plan.md).

SC1's metadata-only planning contract is implemented and its focused exit gates
pass. Direct payload transfer, resume, offline database validation and activation
are **not implemented**. Neither safe end-to-end restore nor 40 GB operation is
qualified. The broader CTest suite still exposes the recorded SC0 fixture crash.

## Checkout, environment and preservation

- Checkout: `/Users/pgarcgo/code/teleno`, branch
  `codex/reward-reinvestment-macos`, commit
  `cfd2eb83b8080ca49911555108ce05f0a9068f01` plus pre-existing and SC1 changes.
  No commit, push, tag, release or deployment was performed.
- Native identity: `teleno_node 1.3.0-dev.0+cfd2eb83b808-dirty`.
- macOS 26.5 (25F71), Mac16,10, 10 logical CPUs, 17,179,869,184 bytes RAM;
  internal APFS volume, not capacity limited.
- AppleClang 16.0.0.16000026, arm64, C++20, Release; CMake 4.2.3, Ninja 1.13.2,
  Python 3.9.10. The Python driver runs under Rosetta; both checked native
  binaries are arm64. Dependencies remain the repository-local SC0 set,
  including RocksDB 8.8.1 and Boost 1.83.0.
- Reused `build-sc0-baseline`, the fresh repository-local configuration
  established by SC0. Its source cache points to this checkout, tests and
  cpp-libp2p are enabled. No foreign build tree or dependency revision was used.
- Captured initial worktree hashes and tracked patch before editing. Eighteen
  non-overlapping pre-existing files remain byte-for-byte unchanged, including
  README.md, the high-throughput plan, core/config/preflight code and SC0 tools.
  Shared main/CMake/changelog/command documentation was integrated additively;
  plan edits update SC1 status and retain SC0 evidence. A temporary Git index
  verified the comparison without changing the actual index. AGENTS.md and
  CLAUDE.md remain unchanged and identical.

Evidence root, abbreviated `E` below (ignored, machine-local):

```text
build-single-copy-evidence/sc1-20261001-XctRQa/
```

`environment-before.json`, `worktree-before.patch` and `preservation-check.json`
record environment, cache/dependency paths, hashes and preservation checks.
Existing databases and live nodes were not opened, moved, removed or operated.
Tests use owned scratch directories and synthetic fixtures only.

## Implemented contract

The default remains `repository`; its acquisition, cache publication, staging,
activation, service and admin behavior is unchanged. The new direct branch runs
before target creation and before legacy restore/startup routing:

```bash
./build-sc0-baseline/teleno_node \
  --basedir <FRESH_TARGET> --config <EXISTING_CONFIG> \
  --backup-restore-mode direct --backup-restore-preflight \
  --backup-metadata-dir <SEPARATE_SCRATCH> \
  --backup-public-url <TRUSTED_SOURCE> \
  --backup-expected-chain-id <EXPECTED_CHAIN_ID> --backup-json
```

The optional expected chain ID is independent caller input. Without it, the
report says `expected_chain_checked: false`; a signature binds metadata to its
declared chain but does not independently validate genesis or database contents.
Configured network, signature requirements, trusted keys and HTTPS policy are
preserved. Signed inputs require agreement between signed chain identity and
manifest identity. Unsigned metadata remains possible only when the existing
configuration permits it, with authentication explicitly reported as absent.

`latest` is resolved once per successful acquisition. A validated specification
holds immutable data: selected ID, source, raw manifest/inventory SHA-256,
network/chain/layout, input version, source node version, provenance status,
validated file entries and derived totals. Durable attempt persistence and
resume pinning belong to SC2, not this in-memory SC1 specification.

Metadata-only acquisition reuses signature verification and transport but does
not populate an object repository. It checks formats/version, unified layout,
identity, contradictory totals, inventory classification and required CURRENT
and genesis entries. Those entries alone do not prove coherent RocksDB/Koinos
state. Both existing download-byte conventions (unique content and per-path
payload) are accepted; space is derived from inventory rather than that field.

Limits are enforced during metadata acquisition/validation:

- 8 MiB per metadata response/file, including HTTP body-limit enforcement;
  at most six metadata files (48 MiB logical payload), plus filesystem overhead.
- JSON nesting depth 32, at most 100,000 inventory entries, 1 TiB per object and
  1 PiB total payload. These are validation limits, not claims of support for
  operating a node that large.
- At most ten configured retries, timeout setting in 1–300 seconds. An actual
  end-to-end synchronous transport deadline is **not proven** by this check.
- Safe portable relative destinations, lowercase SHA-256, consistent repeated
  object sizes, no traversal, duplicate/case collisions or file/directory aliases.
- Disjoint source/target/scratch paths for local input; no symlinked planning
  ancestors or direct URLs containing credentials, queries or fragments.

Scratch writes are disclosed. Acquisition exclusively creates its private
workspace and removes only that workspace on success/failure; the caller's
scratch parent remains. A fresh target is not created or opened. An existing
config is permitted without overwriting it; existing genesis/descriptors must
match the selected inventory. Existing DBs (even empty directories), legacy
chain stores, holds, unowned attempts and other managed data are rejected.
Eligibility is observational: activation-time locking and rechecking are SC3.

Direct stage, activation, public full restore, mixed actions and unsupported
flags fail before target creation. Runtime admin exposure is unchanged.

### Budget and output

`S` rounds every destination file separately to at least a 64 KiB allocation
quantum, or a larger reported filesystem allocation unit. Repeated hashes at
different destinations still count separately. Repository-mode estimation adds
the unique object cache on its actual designated filesystem; direct mode adds
no object cache. Local source data on the target filesystem is already in
observed existing usage `E = capacity - available` and is not treated as free.

`J` is at least 128 MiB and accounts for directory and metadata/journal records;
`T = 0` because a future partial file becomes its final destination;
`V = 128 MiB` is an explicitly **unproven validation allowance**, not a recovery
write upper bound. `R = max(10 GiB, database_bytes / 5)` remains conservative.
Report incremental minimum `S + J + T + V`, recommended minimum plus `R`, and
total capacity demand `E + S + J + T + V + R`. This is a planning model, not a
measured transfer/validation peak or 40 GB qualification.

JSON format `teleno-direct-restore-preflight`, version 1, includes identity/trust,
target eligibility/rejection, source/scratch filesystem relationships, capacities,
budget components, recommendation and explicit unverified items. Text uses the
same fields in labeled pretty JSON. Exit codes:

- `0`: eligible target and declared minimum budget fits, even if below reserve
  recommendation. This does not mean ready to restore or operate.
- `2`: target or available-space rejection, with a structured report.
- `1`: invalid arguments/metadata, failed authentication or inspection/I/O error.

Reports always state `payload_downloaded`, `target_database_opened`,
`attempt_initialized`, `ready_to_restore` and `capacity_40gb_qualified` as false;
`start_as_observer_first` is true. Unsupported work is never reported as done.

## Actual verification results

Commands were run from the repository. Parallelism was bounded at four for
native builds. Logs below are under E unless a separate scratch path is shown.

| Check / command | Actual result | Artifact |
| --- | --- | --- |
| Pre-edit `ctest --test-dir build-sc0-baseline --output-on-failure -R '^koinos_backup_'` | 7/7 passed, 3.54 s, exit 0 | `backup-baseline.log` |
| Full `cmake --build build-sc0-baseline --parallel 4`, followed by final incremental rebuild | Exit 0 | `build-final.log`, `build-latest.log` |
| `./build-sc0-baseline/koinos_backup_restore_plan_test` | 68 always-on checks passed, exit 0 (also active under NDEBUG) | `unit-final.log` |
| `ctest --test-dir build-sc0-baseline --output-on-failure -R '^(koinos_backup_|teleno_direct_restore_preflight_cli_test)'` | 9/9 passed, 5.41 s, exit 0 | `ctest-focused-final.log` |
| `ctest --test-dir build-sc0-baseline --output-on-failure` | 23/24 passed, 18.93 s, exit 8; only SC0 diagnostic harness failed | `ctest-all-final.log` |
| Native `--version` / `--help` | Both exit 0; identity above and new flags present | `version-final.log`, `help-final.log` |
| Separate arm64 UBSan build and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./build-sc1-sanitize/koinos_backup_restore_plan_test` | Build exit 0; 68 checks passed, exit 0; no UBSan report | `ubsan-configure.log`, `ubsan-build.log`, `ubsan-test.log` |
| ASan+UBSan experiment | Initial configuration selected x86_64 under Rosetta; link failed against arm64 dependencies. Explicit arm64 reconfiguration/build passed, but runtime aborted before tests | `sanitize-configure.log`, `sanitize-build.log`, `sanitize-arm64-configure.log`, `sanitize-arm64-build.log`, `sanitize-test.log` |
| `docker info --format '{{.OSType}}'` | Exit 1; Docker daemon socket unavailable, so container smoke not executed | `docker-availability.log` |
| `git diff --check`; preservation and guide comparison | Exit 0; preservation checks passed | `preservation-check.json` |

The sanitizer configuration is freshly generated from reviewed local dependency
settings, not a copied foreign CMakeCache. UBSan instruments the repository
libraries linked into the focused unit test, not the full node or third-party
dependencies. ASan abort text:

```text
AddressSanitizer: CHECK failed: sanitizer_malloc_mac.inc:189 "((!asan_init_is_running)) != (0)"
<empty stack>
```

This is an unavailable ASan result, not a passed memory-safety check or an
identified application defect. Leak checking and ThreadSanitizer were not run.

The canonical dependency script was not rerun for unchanged dependencies.
Its SC0 failure caused by Make splitting the selected Xcode path at a space
remains recorded in the [SC0 report](single-copy-restore-sc0-baseline.md).
The alternative native build is not a claim that the canonical workflow passed.

### Focused fixture evidence

The final full-suite CLI test retained evidence under:

```text
build-single-copy-evidence/sc1-cli-z8woize1/
```

The test command is reproducible independently:

```bash
python3 tests/cli/direct_restore_preflight_test.py \
  --node build-sc0-baseline/teleno_node \
  --fixture build-sc0-baseline/koinos_restore_baseline_fixture --repository .
```

`summary.json` records 21 commands, loopback request paths, unchanged source
hashes and no node startup. Each command JSON contains argv, output and exit
code. Valid HTTP preflight requested latest once and only six metadata files;
there were no object requests. Both file:// and loopback HTTP passed with a
synthetic signed nine-file RocksDB fixture, not a valid Koinos chain. A config
with production enabled did not start services or create a target DB.

Final selected fixture ID: `20261001T024642Z-ms-1790822802122-files-7`.
Manifest SHA-256:
`e996b808df02f39290af98192dc1a0eb88920f170415c85face82f9367e48211`.
Inventory SHA-256:
`e3231e0c4fe64f125d6f15ae9fe02c6040e48af6fac0c2ac2f22eec3d8adb99d`.
`file-latest.json` records logical payload 6,479,960 bytes, estimated allocated
payload 6,881,280 bytes, declared incremental minimum 275,316,736 bytes,
recommended 11,012,734,976 bytes, and zero object-cache allocation. These are
estimates on an uncapped host, not measured direct-restore savings.

Negative CLI cases cover tampered/missing/invalid signatures, excessive metadata,
wrong chain, HTTPS policy, existing DB retention, unsupported actions and mixed,
unknown or orphan flags. Unit tests additionally cover bad formats/layouts,
unsafe/colliding paths, contradictory totals and sizes, depth/entry/object/total
limits, arithmetic, reserve boundaries, filesystem/source overlap, symlinks,
legacy stores and recovery holds. Historical replay regression tests passed in
the broader suite; no required full historical range or reference differential
validation was executed, and no historical parity claim follows.

## Remaining blockers and SC1 progress log

The final SC0 harness still fails on a truncated response with fixture exit
`-10` (SIGBUS), evidence `build-single-copy-evidence/sc0-run-i6v6tl9o/`.
SC0 separately established a normal exit-1 rejection from the actual node CLI;
that earlier observation does not resolve the diagnostic linkage/error-formatting
discrepancy. The regression remains visible and must not be suppressed.

Existing storage/publication audit findings remain unremediated. Before SC3/SC5
qualification, resolve or explicitly gate flush/error propagation, write atomicity,
durability and bounded synchronous transport behavior. Missing prerequisites also
include representative authenticated chain fixtures and successor blocks,
capacity-limited Mac and isolated Linux/ext4 environments, offline validation
write bounds and container availability. SC2's transfer/journal is the next
implementation milestone; none of its exit criteria are claimed here.

Files changed for SC1 (not the entire dirty worktree):

- `src/backup/restore_plan.hpp`, `src/backup/restore_plan.cpp` (new).
- `src/backup/public_restore.hpp`, `src/backup/public_restore.cpp`.
- `src/main.cpp`, `src/CMakeLists.txt`.
- `tests/backup/restore_plan_test.cpp`,
  `tests/cli/direct_restore_preflight_test.py` (new).
- `docs/backup-restore-cli.md`, `docs/command-reference.md`.
- `docs/implementation/single-copy-backup-restore-plan.md`.
- `docs/validation/single-copy-restore-sc1-preflight.md` (this report, new).
- `CHANGELOG.md` (unreleased behavior, version unchanged).

Invariants: default legacy mode preserved; shared signature checks retained;
bounded metadata-only acquisition; immutable selected specification; conservative
mode-aware accounting; fresh-target fail-closed routing; no target DB opening,
object download, activation, source modification or producer startup.

**SC1 exit status:** satisfied for metadata planning and legacy focused tests,
with broader baseline/tooling blockers explicitly recorded. **Feature Definition
of Done:** not satisfied. **40 GB claim gate:** not satisfied. No production
readiness, durable activation, complete memory-safety or historical parity claim
is made.
