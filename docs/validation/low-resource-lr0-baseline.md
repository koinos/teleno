# LR0: full-history observer resource baseline

Date: 2026-10-03. **Local LR0 exit satisfied; complete-plan qualification blocked.**
This implements the first milestone of the
[low-resource mainnet observer plan](../implementation/low-resource-mainnet-observer-plan.md).
It establishes measurements, not a new runtime profile or a 40-GB minimum.
LR1–LR6, source eligibility and full-history Linux/mainnet qualification remain
pending. Teleno must preserve complete canonical history from genesis and every
backed-up block/receipt record. `teleno_light` is separate and deferred.

## Implementation and invariants

The new Python 3.9+ collector reuses SC5's signed fixture and native commands.
It pins the resource contract, build and host manifest, metadata digests,
disabled fixture services, seven-day qualification horizon and unavailable
metrics. Its optional sampler records owned-child `/proc` RSS/I/O, Linux host
memory and cgroup-v2 counters where exposed, plus SST/WAL/metadata/log/object
file sizes and allocation. Per-command `wait4` records RSS high-water and user/
system CPU time. No production DB-property collector is added: pending
compaction, write-stall time and advancing head remain unavailable.

The shared experiment now supports this optional sampler and bounded baseline
commands. On timeout/error it terminates only the newly owned command process
group and retains aborted logs/samples. Normal SC5 CLI/default behavior remains.
Fourteen collector tests cover counter units, missing versus zero, shared and
unlimited cgroups, path boundaries, allocation/inodes, metadata limits,
unverified trust, dependency identity, warm-up exclusion, failed baseline checks,
actual child accounting and child/descendant timeout cleanup. CTest runs them.

No node C++ runtime, pruning, history encoding, memory configuration, durability,
trust requirement, reserve policy or producer setting was changed. Normal
remove/tombstone/root/lookahead/fallback semantics are untouched. All node
processes launched by this experiment use new owned synthetic basedirs and
observer holds, with networking, production and optional writers disabled.
Existing chain/state data and the independent fixture source are preserved.
No VPS, live node, external payload, producer key or transaction was used.

The infrastructure-review guidance was used to keep startup/synthetic evidence,
steady synchronization and whole-server capacity as distinct claims; missing
resource observations are explicitly unavailable rather than inferred passes.

## Environment and reproducible inputs

Final retained run: `build-single-copy-evidence/sc5-run-lzi7fo5v/`.
Environment recorded at `2026-10-03T07:35:59.961697+00:00`.

| Item | Actual baseline |
| --- | --- |
| Source HEAD / branch | `cfd2eb83b8080ca49911555108ce05f0a9068f01` / `codex/reward-reinvestment-macos`, pre-existing dirty worktree |
| Native identity | `teleno_node 1.3.0-dev.0+cfd2eb83b808-dirty` |
| Node SHA-256 | `8dd7097b90dddc661f26ae5790548ebc8a070d5374ca604bf664ef671cd3d32e` |
| Fixture helper SHA-256 | `29596e548f23b73c0fc2717d121a17cb2358b7b4118288d7cc213f491e0dee13` |
| OS / CPU / memory | macOS 26.5 (25F71), Darwin 25.5.0, 10 CPUs, 17,179,869,184 physical bytes |
| Architecture | Native node is Mach-O arm64; Python/command inspection reports x86_64. Not the intended Linux x86_64 build |
| Build | Repository-local `build/`, tests ON, Release, `-O3 -DNDEBUG`; no foreign CMake home |
| Tooling | CMake 4.2.3, Apple Clang 16.0.0, Python 3.9.10; sanitizer flags not enabled |
| Dependencies | RocksDB 8.8.1, Zstd 1.5.7, libssh 0.12.0, Boost 1.83.0; local cached installs |
| cpp-libp2p | `c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`, with build-script patches; dirty status recorded |

`lr0-environment.json` records CMake cache/compiler flags, binary/header digests,
source HEAD and dirty/untracked file hashes, template hash and host filesystem
observations. A dependency extracted from a tarball has no independently
verified Git revision: the collector explicitly refuses to inherit Teleno's
parent HEAD as its revision. Installed header digests identify the observed
versions but are not a complete dependency-source checksum. This is a reused
local configuration, not proof of a clean-from-empty dependency bootstrap.

Fixture: signed Harbinger genesis, nine-CF native RocksDB, plus 8,192 rows of
4,096 deterministic xorshift bytes in the optional contract-metadata CF
(seed `0x5c0005`, compression disabled for padding). No successor blocks or
representative transaction/compaction workload. Requested disabled features:
`block_producer`, `p2p`, `jsonrpc`, `grpc`, `mempool`, `account_history`,
`contract_meta_store`, `transaction_store`.

The versioned contract records 2 CPUs, 4 decimal GB RAM, 40 decimal GB whole-server
disk, experimental RSS targets of 2 GiB steady / 2.5 GiB peak, at least 768 MiB
host available memory and a seven-day qualification horizon. These are **targets**,
not measured mainnet guarantees. Default reserve stays `max(10 GiB, DB / 5)`.

## Commands and actual results

```bash
KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/teleno_node --version
./build/teleno_node --help
python3 tests/scripts/low_resource_baseline_test.py
python3 tests/scripts/validate_single_copy_restore_test.py
python3 tools/backup/measure_low_resource_baseline.py --build-dir build \
  --repetitions 5 --baseline-checks \
  --source-metadata-dir build-single-copy-evidence/mainnet-source-20261003-9qQ0iV
ctest --test-dir build --output-on-failure \
  -R '^teleno_(low_resource_baseline|single_copy_measurement|single_copy_experiment)_test$'
```

- Standard dependency/native workflow: exit 0, with cached repository-local
  dependencies reused. The earlier dependency-bootstrap limitation did not
  reproduce in this invocation; no SDK/build-path fix or clean rebuild is claimed.
- Native incremental build: exit 0. Collector uses `--parallel 4` and records
  its command/output; separate standard `--parallel` invocation also passed.
- Before edits: full CTest 29/30, exit 8. Final collector baseline: **30/31**,
  exit 8, 45.32 seconds. Sole failure remains
  `teleno_restore_baseline_harness_test`: truncated local HTTP fixture exits
  `-10` / `SIGBUS`, recorded at
  `build-single-copy-evidence/sc0-run-rebytqzo/truncated.log`.
  It predates LR0 and has not been fixed or waived. LR1 must investigate/fix it.
- Version/help: exit 0, matching identity above. Collector unit tests: **14/14**;
  existing SC5 measurement unit tests: **7/7**. Focused three-test CTest passed.
- Collector: exit 0, `LOCAL_BASELINE_RECORDED`; `all_baseline_checks_passed` is
  **false** because full CTest fails. Exit 0 means recording succeeded, not all
  tests passed or mainnet eligibility. The test suite verifies this distinction.
- Container smoke: not run. Docker CLI exists but `docker info` cannot connect
  to its Unix socket because no daemon is running. No daemon was started.
- No new native sanitizer, sustained Linux, mainnet history audit or reference
  differential validation was run. Existing historical controller fixtures in
  CTest are not a full historical range or archive coverage proof.

An initial optional `--dependency-build` collector attempt used the inherited
120-second command bound and timed out while configuring/building Hunter Boost.
Its evidence is retained at `build-single-copy-evidence/sc5-run-i8heb1kd/`;
no restore measurement was completed there. The collector was corrected to
use 600-second baseline bounds, record aborted commands and stop owned process
groups, with a real descendant cleanup test. The optional dependency flag was
not rerun after that correction; the separate canonical script's successful
execution above is the actual dependency-build evidence. Earlier successful
collector runs are retained, not deleted or substituted for the final run.

## Five-pair synthetic measurements

One excluded warm-up pair, then five alternating direct/repository pairs from
the same signed snapshot. Exact DB hashes, read-only DB/genesis validation,
same-inode activation, equal verification results and unchanged source hashes
passed. Final pair: two clean observer restarts in each mode; direct mode also
passed an owned observer SIGKILL/reopen check. No physical power-loss claim.

| Restore/inspection metric | Direct | Repository |
| --- | --- | --- |
| Elapsed median / MAD / range, seconds | 0.280913 / 0.018173 / 0.261348–0.340895 | 0.222375 / 0.008880 / 0.210762–0.268146 |
| Child peak RSS median / MAD / range, bytes | 25,378,816 / 163,840 / 25,133,056–25,542,656 | 21,004,288 / 32,768 / 20,955,136–22,069,248 |
| Sampled target allocation peak median, bytes | 34,336,768 | 68,562,944 |
| Peak allocation MAD, bytes | 0 | 0 |

Paired allocation difference: median 34,226,176 bytes, MAD 0, five samples.
Direct mode avoids the fixture's second object payload; it was not faster and
did not use less RSS in this run. This baseline demonstrates measurement and
existing single-copy behavior, not a newly achieved runtime optimization.
The independent source allocated 68,710,400 bytes and remains outside the
destination; accumulated prior cases/builds/evidence are not hidden within a
whole-server qualification. Python-driver peak RSS: 38,084,608 bytes.

Observer phases are separate from the five restore statistics: direct peak
RSS 26,607,616 bytes / sampled allocation 34,934,784; repository 26,099,712 /
68,915,200. Neither is mainnet steady-state memory.

LR0 sampled category maxima across measured fixture/observer commands:

| Allocated category, bytes | Direct (89 samples) | Repository (65 samples) |
| --- | --- | --- |
| SST | 34,193,408 | 34,185,216 |
| WAL | 4,096 | 4,096 |
| DB metadata | 151,552 | 151,552 |
| Logs | 524,288 | 270,336 |
| Object payload | 0 | 34,250,752 |
| Other, including temporary transfers | 25,493,504 | 86,016 |

These are independent sampled maxima, **not additive simultaneous peaks**.
Allocation uses `st_blocks * 512`, inode deduplication within categories; it is
not exclusive APFS extent usage. Sampling adds traversal overhead and a 20-ms
sleep, so short peaks can be missed. Exact child RSS uses `wait4`, not polling.
No cold-cache purge, host isolation or benchmark-driver saturation study was
performed. CPU times are raw per-command observations, not throughput claims.

## Mainnet/target evidence and exact blockers

The collector only reread the previously acquired metadata in
`build-single-copy-evidence/mainnet-source-20261003-9qQ0iV/`; no public source
was refreshed and no payload downloaded. Pinned backup:
`20261002T114351Z-ms-1790941431745-files-487`, 490 files, 29,396,360,941 declared
logical bytes (DB: 29,395,709,834). Manifest SHA-256
`65e79a407d0969ec6813d5d93c62a956e04a4be1d0b76a2e029e692f9235d96c`;
inventory `58bf0c02b3c4f2aeec150bb4f81e1e42e07219859aa08d2e68ac8579d2abee50`.
Logical totals/digests are not source authentication or physical usage.

| Missing proof | Exact prerequisite / next gate |
| --- | --- |
| Eligible mainnet input | Empty declared chain identities, earlier native `metadata_authentication` failure with unspecified underlying cause, no supplied independent signer/digest. Resolve in LR1; do not edit publisher bytes or disable trust checks |
| Actual archive identity/history | Complete payload unavailable to this run; actual durable root/head, all block/receipt records and canonical continuity from first block unverified. Requires authenticated source and full-corpus checks |
| Linux accounting/resource profile | No matching isolated Linux x86_64 feature build/ext4 environment. `/proc`, cgroup and host `MemAvailable` parsing is unit-tested, not measured on the selected running node |
| Whole-server runtime footprint | Earlier host sample only, no deployed matching runtime. Installed services/files and their behavior are unmeasured; no VPS modification in this task |
| Combined peaks / growth / reserve | No sustained representative WAL rotation, compaction, recovery, backlog or archive-growth workload. DB properties/stalls/head marked unavailable; cannot qualify LR3 or reduce reserve |
| Historical/differential parity | No complete source or executed reference ranges; blocks 30,504,202 / 32,789,377 / 32,789,378 historical range claims remain unproven |
| All-green software baseline | SC0 truncated-transfer SIGBUS persists; fix and rerun in LR1, not an LR0 diagnostic change |
| Container delivery | Local Docker daemon unavailable; disposable container smoke still pending |

Prior read-only target sample at 06:28:29 UTC on this date is retained at
`build-single-copy-evidence/linux-preflight-20261003-5IHv7C/preflight.md`:
39,956,590,592 root capacity bytes / 36,146,835,456 available bytes, two CPUs,
4,000,043,008 RAM bytes, no swap. Under the unchanged reserve, the metadata-only
estimator recommends 40,410,218,496 free bytes: **4,263,383,040 bytes short**.
These prior observations were not reverified on the host in LR0. The actual
restore/run budget could be larger; unavailable peaks must not be estimated as
zero. Do not infer that a 40-GB full archive can safely run from synthetic results.

## Artifacts, progress and exit

Final run contains `lr0-baseline.json`, `lr0-environment.json`,
`lr0-artifact-sha256.json`, `commands.json`, `runs.json`, `summary.json`,
`source-before-sha256.json`, `database-input-sha256.json`, signed fixture metadata
and command stdout/stderr.
The artifact index excludes SSTs; exact DB/source checks are recorded separately.
Large artifacts are repository-local and ignored, not committed. Public reports
contain no credentials, private host identifiers or operational config contents.

Files changed for LR0 (not the larger pre-existing SC worktree):

- `tools/backup/measure_low_resource_baseline.py` (new),
  `tests/scripts/low_resource_baseline_test.py` (new), `src/CMakeLists.txt`.
- `tools/backup/validate_single_copy_restore.py` (additive sampler/accounting/
  owned-command cleanup), `tools/backup/README.md`.
- This new report, `docs/implementation/low-resource-mainnet-observer-plan.md`,
  `docs/implementation/single-copy-backup-restore-plan.md`, `docs/README.md`,
  `docs/memoria.md`, `CHANGELOG.md`.
- Koinos One documentation only:
  `docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
  `docs/manual/koinos-one/backup-and-restore.md`.

Checkpoint: contract/invariants and focused tests implemented; standard cached
build/CLI and final five-pair fixture baseline executed; full-suite failure and
resource/source blockers retained. Koinos One strict MkDocs, both repositories'
whitespace checks and matching agent guides passed. A local Markdown check
verified 80 relative file references and whitespace/EOF formatting. A turn-start
hash comparison preserves 54 unrelated pre-existing dirty/untracked files,
including README, the high-throughput plan and all native C++ runtime files.
No native commit/submodule integration pin exists
for this uncommitted work; build identity is the dirty baseline above.

**LR0 exit: satisfied locally** as defined by the plan: tested measurement driver,
current baseline recorded and exact unavailable-input/measurement blockers.
**Entire plan Definition of Done: not proven.** No full-history parity, source
eligibility, smaller reserve, memory optimization or whole-server 40-GB pass.
Next milestone is LR1, subject to a new implementation request. Publication,
deployment, packaging/pin updates and producer activation remain unauthorized.
Rollback is limited to reviewing/removing LR0's additive tooling/tests/docs;
do not revert pre-existing work or delete retained chain/source/evidence data.
