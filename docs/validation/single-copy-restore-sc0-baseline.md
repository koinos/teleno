# Single-copy restore: SC0 baseline

Date: 2026-09-30. Scope: SC0 of the
[single-copy restore plan](../implementation/single-copy-backup-restore-plan.md).
This is a baseline of the existing repository-based restore, not a direct-mode
implementation or a 40 GB qualification.

## Checkout and environment

- Repository: `/Users/pgarcgo/code/teleno`.
- Commit: `cfd2eb83b8080ca49911555108ce05f0a9068f01`.
- Branch: `codex/reward-reinvestment-macos`; no commit, push, tag or release
  was performed for SC0.
- The checkout already contained tracked and untracked preflight, documentation,
  and storage-design work. The native baseline includes those changes; it is
  not evidence for a clean release of the commit above. The initial tracked
  diff SHA-256 was
  `84d9e6ec72c61771e36c2229d49e77e093ee1164482bac6161e7f22ec255e0d4`.
- macOS 26.5, build 25F71; Mac16,10, 10 logical CPUs, 17,179,869,184 bytes RAM.
- Repository data volume: APFS on internal solid-state storage, 4,096-byte device
  blocks; `filesystem.json` records the selected data volume, without device IDs.
- Native compiler: AppleClang `16.0.0.16000026`, arm64, C++20, Release (`-O3`,
  `-DNDEBUG`); existing asserting test targets also use `-UNDEBUG`.
- CMake 4.2.3; Ninja 1.13.2; Python 3.9.10. The measurement driver runs under
  x86_64/Rosetta; the native compiler record and Mach-O binary are arm64.
  Python's `uname` result must not be used to describe the native binary.
- Native configuration: tests ON, cpp-libp2p ON, repository-local
  `build-sc0-baseline`, with verified existing local dependencies. This build
  directory was new; the large existing `build/` tree was preserved.
- Dependencies: RocksDB 8.8.1 with local Zstd support, Boost headers 1.83.0,
  Zstd 1.5.7, GMP 6.3.0, libssh 0.12.0. cpp-libp2p tag `v0.1.37`, commit
  `c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`; generated Hunter entrypoint's
  koinos-cmake dependency `12cc060400781497140ad549a094f2d0680503e0`.
- Disk availability was approximately 102 GB at initial inspection and 97 GB
  during the measured run. This was not a capped test volume. Existing build,
  dependency, source-fixture and evidence allocations are retained separately.
  Filesystem allocation was measured using `st_blocks * 512`; exclusive APFS
  physical extents and power-loss durability were not measured.

Full compiler flags, dependency paths, CMake cache hash, hardware commands and
worktree file hashes are in the ignored environment manifests. No user-owned
database was opened, restored, moved or removed. `AGENTS.md` and `CLAUDE.md`
remain unchanged and identical.

## Build and test baseline

Evidence root, abbreviated `E` below:

```text
build-single-copy-evidence/sc0-20260930-UuABQJ/
```

| Check | Actual result | Artifact beneath E |
| --- | --- | --- |
| Test-enabled dependency script with new native/Hunter build directories | Exit 1: generated Make subbuild splits the selected external Xcode path at a space (`make: /Volumes/My: No such file or directory`) | `dependency-build.log` |
| Per-process Command Line Tools retry | Deliberately stopped, exit 143; selected AppleClang 14 and started a different Hunter Boost rebuild; not a successful dependency build | `dependency-build-clt.log` |
| Fresh native Ninja configuration with original compiler and existing local dependencies | Exit 0; no foreign native source/build cache reused | `native-configure.log` |
| Full native build, parallel 4 | Exit 0 | `native-build.log` |
| Existing CTest baseline before SC0 registration | 21/21 passed, 25.12 seconds, exit 0 | `ctest-baseline.log` |
| Binary identity | Exit 0, `teleno_node 1.3.0-dev.0+cfd2eb83b808-dirty` | `version.log` |
| CLI help | Exit 0 | `help.log` |
| Initial SC0 fixture build | Exit 0 | `fixture-build.log` |
| Initial full suite with new diagnostic regression | 21/22 passed, 22.09 seconds, exit 8; only the fixture's truncated-response case crashed | `ctest-final.log` |
| Final full rebuild | Exit 0 | `build-checkpoint.log` |
| Final full suite | 21/22 passed, 21.51 seconds, exit 8; same diagnostic-only failure | `ctest-verified.log` |
| Final CLI identity/help | Both exit 0, unchanged native identity | `version-verified.log`, `help-verified.log` |
| Tool boundary checks | Five expected rejections passed; existing fixture unchanged | `tool-safety-checks.json` |
| Pre-existing worktree preservation | All captured user files unchanged; CMake addition stripped only for comparison | `preservation-check.json` |

The supported dependency workflow has an environment/build-generator blocker;
the successful fresh native build is an explicitly recorded alternative, not a
claim that the dependency script passed. No global Xcode setting was changed.
The cpp-libp2p source changes matched the repository's compatibility patch using
a temporary Git index; its actual index and source were not reset or modified.

## Fixture and duplication evidence

Recorded measurement run:

```text
E/sc0-run-7qkzh0bz/
```

The fixture uses the real RocksDB manager, checkpoint manager, snapshot
repository, signature verification, fetch, stage, activation and reopen APIs.
It writes 2,048 deterministic rows of 1,024 bytes to each of three of the nine
column families, xorshift32 seed `0x5c0001` (6,029,313), no compression and a
16 MiB block cache. The synchronous write and all-column-family flush are
checked. A publicly known synthetic Ed25519 seed signs the metadata; there is
no producer key or operator secret.

This is a valid RocksDB snapshot with synthetic records and a genesis
placeholder, **not a valid Koinos chain**. Its reopened logical records are
verified in full, but no consensus execution, chain catch-up or producer startup
is performed.

| Fixture identity | Recorded value |
| --- | --- |
| Backup ID | `20260930T151444Z-ms-1790781284014-files-7` |
| Logical dataset SHA-256 | `d42b756d4bab382c5e1c5fba4ecfe4590b39128a16feb29542b8c13addc77fa7` |
| Manifest SHA-256 | `c0eb7259f696cb4ad990ba1d076a9f4ddd51445924d1decd2fd8f8efaa16baa4` |
| Files inventory SHA-256 | `96e3fa7490afe408536f728dcafd3fc01d6c1bf5cd0e2441fff4c043b3711da3` |
| Snapshot payload | 6,479,980 bytes, nine inventory entries |

One source fixture was reused for one warm-up plus five measured restores per
transport. All 12 valid restores passed, including size/hash verification,
staging, activation, observer-hold marker presence and complete data verification
after reopening the activated database. Every staged inventory file matched
the repository hash and had a different inode. The activated `CURRENT` kept
its staged inode, proving rename for this same-filesystem activation; this does
not test the existing cross-filesystem copy fallback.

| Measurement | file://, five samples | Loopback HTTP, five samples |
| --- | --- | --- |
| End-to-end fixture elapsed median | 0.075778 s | 0.114979 s |
| Elapsed minimum–maximum | 0.073031–0.076681 s | 0.113997–0.118898 s |
| Native fixture peak RSS median | 25,165,824 bytes | 25,378,816 bytes |
| Downloaded repository allocated after staging | 6,524,928 bytes | 6,524,928 bytes |
| Target/stage allocated after staging | 6,516,736 bytes | 6,516,736 bytes |
| Combined destination allocation after staging | **13,041,664 bytes** | **13,041,664 bytes** |
| Identical payload retained in both trees | **6,479,980 bytes** | **6,479,980 bytes** |

These tiny, warm-host observations demonstrate duplication; they do not predict
large-snapshot throughput or a production memory bound. The Python driver and
source/checkpoint fixture are outside the native process RSS and destination
allocation totals. They remain on disk and are not excluded from whole-host
space requirements. Sampled free-space deltas can include unrelated host work;
phase observations are not a guaranteed upper bound on transient allocation.

`fixture.json`, each `restore.json`, `summary.json`, `commands.json`,
`http-requests.json`, per-command logs and `artifact-sha256.json` retain exact
arguments, phase data, hashes, outcomes and sample provenance.

## Failure checks and blocker classification

| ID | Evidence / status | Consequence / next gate |
| --- | --- | --- |
| SC0-B01 | Confirmed dependency-script Make failure with a space in the selected Xcode path. Native Ninja configuration/build succeeds without changing compiler. | The canonical dependency workflow is not proven on this Mac environment. Fix or qualify the workflow before release; do not silently switch compiler or trust another workspace's build. |
| SC0-B02 | The small fixture executable receives SIGBUS while formatting a Boost error after an advertised-length HTTP object is truncated. Reproduced in the initial measurement, CTest and a subsequent run. The **real node CLI** rejects the same input with exit 1 and `Fatal: partial message`, without DB activation. | This is an executable/linkage-dependent diagnostic failure, not evidence of a node CLI crash. Error-formatting compatibility must be checked in the harness; the dependency/linkage discrepancy needs classification before trusting failure-path instrumentation. |
| SC0-K01 / TQ-01 | Current source still discards state-backend Flush statuses and maps non-NotFound Get errors to absence. The audit records prior fault-injection reproductions; SC0 rechecked source, not those injected failures. | Existing storage integrity remediation remains open. A healthy synthetic reopen is not proof of correct behavior under I/O failure. |
| SC0-K02 / TQ-02 | Current block store still writes record and highest topology separately. Prior audit reproduction is context, not rerun evidence for this build. | Atomic persistence/retry and recovery discovery require their own fault-injection gates. |
| SC0-K03 / TQ-11 | Current downloader's HTTP body limit remains effectively unlimited, synchronous deadline enforcement is unproven, and marker publication lacks an explicit checked-write/file-and-directory synchronization contract. | Tampered/truncated inputs do not establish bounded stalled-transfer handling, ENOSPC behavior or power-loss-safe publication. SC2/SC3 require these checks/contracts. |
| SC0-K04 / TQ-10 | Cross-CF WAL lifetime and realistic compaction/recovery working space were not measured by the tiny flushed fixture. | Do not size a production target from SSTs or this fixture. Representative SC5 workload and Linux/ext4 environment remain prerequisites. |

The tampered signed manifest fails with exit 1 and the expected manifest hash
mismatch. Neither negative attempt publishes a database or observer activation
marker. The truncated CLI attempt retains a partial download in its owned
scratch repository, which is preserved for inspection.

The macOS crash report places the fixture fault in
`boost::system::error_code::what()`, then the `system_error` constructor and
`PublicBackupClient::read_response_to_file()`. The executable-dependent outcomes
support a static dependency/inline-symbol compatibility investigation, but do
not establish a general runtime root cause. A local LLDB attempt could not run
the target (`the platform is not currently connected`); no protection was
disabled to work around it. Initial failing evidence remains retained. Adding
an ordinary Boost error-formatting check at fixture startup did not resolve the
truncated-transfer crash; the final suite still exposes it. It was not suppressed
or accepted as a passing rejection.

The final CTest fixture's separately retained run is
`build-single-copy-evidence/sc0-run-pq89aiuj/`. The final five-repetition driver
returned exit 2 (`BLOCKED`), with valid measurement reports and explicit
`negative_outcomes` for the fixture and native CLI. The safety checks also prove
that existing output, output outside the evidence tree, remote fixture URLs,
foreign driver output and foreign build directories are rejected.

## Reproduction

Use a fresh destination and retain all prior evidence. Never point these tools
at an operator basedir. The ordinary supported dependency command is:

```bash
KOINOS_BUILD_TESTS=ON \
KOINOS_NODE_BUILD_DIR="$PWD/build-sc0-baseline" \
KOINOS_NODE_HUNTER_SOURCE="$PWD/.deps/sc0-teleno-node-hunter-src" \
KOINOS_NODE_HUNTER_BUILD="$PWD/.deps/sc0-teleno-node-hunter-build" \
JOBS=4 ./scripts/build-cpp-libp2p-koinos.sh
```

The successful alternative on this checkout used the already installed local
packages below. Review versions and paths before reuse; do not presume these
Hunter installation hashes exist on another machine. Select a new build name
instead of overwriting an existing tree.

```bash
sc0_repo="$PWD"
sc0_hunter="$sc0_repo/.deps/hunter/_Base/a20151e/11f66d7/d63702f/Install"
sc0_aux="$sc0_repo/.deps/hunter/_Base/15ca502/a9dc8f8/0022ad1/Install"
cmake -S . -B build-sc0-baseline -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DKOINOS_BUILD_TESTS=ON -DKOINOS_ENABLE_LIBP2P=ON \
  -DCMAKE_PROJECT_INCLUDE="$sc0_repo/cmake/cpp-libp2p-koinos-prelude.cmake" \
  -DOPENSSL_ROOT_DIR="$sc0_hunter" \
  -Dyaml-cpp_DIR="$sc0_hunter/lib/cmake/yaml-cpp" \
  -DCPP_LIBP2P_THIRDPARTY_INCLUDE_DIR="$sc0_repo/.deps/cpp-libp2p-thirdparty-include" \
  -DCMAKE_CXX_FLAGS="-Wno-error=deprecated-declarations -DBOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED -DBOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT -I$sc0_hunter/include -I$sc0_repo/.deps/cpp-libp2p-thirdparty-include" \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$sc0_repo/build-sc0-baseline" \
  -DCMAKE_PREFIX_PATH="$sc0_repo/cmake/shims;$sc0_repo/.deps/libssh-static-0.12.0;$sc0_repo/.deps/cpp-libp2p-koinos;$sc0_hunter;$sc0_aux" \
  -DLIBSSH_ROOT="$sc0_repo/.deps/libssh-static-0.12.0" \
  -DRocksDB_DIR="$sc0_repo/.deps/rocksdb-zstd-8.8.1/lib/cmake/rocksdb" \
  -Dzstd_INCLUDE_DIRS="$sc0_repo/.deps/zstd-static-1.5.7/include" \
  -Dzstd_LIBRARIES="$sc0_repo/.deps/zstd-static-1.5.7/lib/libzstd.a" \
  -DGMP_LIBRARY="$sc0_repo/.deps/gmp-static-6.3.0/lib/libgmp.a"
cmake --build build-sc0-baseline --parallel 4
ctest --test-dir build-sc0-baseline --output-on-failure
./build-sc0-baseline/teleno_node --version
./build-sc0-baseline/teleno_node --help
python3 tools/backup/record_sc0_baseline.py --build-dir build-sc0-baseline --repetitions 5
```

See [tool usage](../../tools/backup/README.md) for capture-only operation and
the focused CTest command. New run IDs, ports, timestamps and RocksDB file hashes
may differ; the logical dataset hash must match. Failed fixture checks produce
a nonzero result and remain visible; do not interpret a retained measurement
summary as a passing failure-path check.

## SC0 progress log and exit criteria

- Files: `src/CMakeLists.txt` (diagnostic target/CTest registration only),
  `tools/backup/restore_baseline_fixture.cpp`,
  `tools/backup/record_sc0_baseline.py`, `tools/backup/README.md`, this report and
  `docs/implementation/single-copy-backup-restore-plan.md`.
- Invariants: no runtime restore implementation or default changed; fresh owned
  scratch destinations only; signed/hash-checked input; unchanged source backup;
  observer marker required; reopen data verified; rejected transfers do not
  activate; no producer, live node, historical chain mutation or data deletion.
- Tests: successful existing 21-test baseline; both valid fixture transports;
  signed metadata rejection; separate fixture/real-CLI truncation outcomes;
  initial additional diagnostic failure explicitly recorded above.
- Risks: SC0-B01/B02 and existing storage/durability findings; representative
  trusted snapshot, successor blocks, Linux/ext4 and capped-capacity environment
  are unavailable in this milestone.
- Exit: environment/build/test results and fixture hashes recorded; current
  duplication reproduced; unresolved storage/durability issues classified.
  **SC0 baseline collection is complete with recorded blockers.** This does not
  make the direct-restore implementation, release workflow or 40 GB gate complete.
- Historical/differential validation: not executed by SC0. Passing historical
  unit fixtures in CTest is not a current-checkout full-range parity result.
- External actions: none executed. Commit/push/release/deployment and later
  milestone implementation remain outside this request.
