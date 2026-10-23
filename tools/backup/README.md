# SC0 restore baseline tools

These diagnostic tools create a synthetic, signed native snapshot and exercise
the **existing** fetch, staging, activation, and reopen APIs. They never start a
node, connect to public peers, use operator keys, or implement direct restore.

Configure a repository-local build with `KOINOS_BUILD_TESTS=ON`, following
`docs/install-or-build.md` and the build script. Before using an existing build,
verify its `CMAKE_HOME_DIRECTORY`, compiler, dependency paths and test setting.
Then build and run:

```bash
cmake --build build-sc0-baseline --target koinos_restore_baseline_fixture --parallel 4
python3 tools/backup/record_sc0_baseline.py --build-dir build-sc0-baseline --repetitions 5
ctest --test-dir build-sc0-baseline --output-on-failure -R '^teleno_restore_baseline_harness_test$'
```

`--build-dir` can select another verified repository build. The measurement
driver creates a new unique directory beneath ignored
`build-single-copy-evidence/`. `--output-parent` can select a child of that
directory. Existing output is never replaced. Failed fixtures and successful
evidence remain available for inspection; the tools do not delete them.

When the matching source tree is exported without Git metadata, explicitly set
`TELENO_TEST_SOURCE_MANIFEST` to an absolute `teleno-native-input-manifest`
version-1 JSON file containing its `native_files` size/SHA-256 inventory. SC0
verifies every listed regular, unlinked source file before running the restore
fixtures. It records the manifest digest and declared base separately, with
`git_available: false` and no tracked-diff claim. A missing or mismatched
manifest refuses the run; no empty Git repository or artificial commit is
created. CMake source/build correspondence and all restore checks still apply.

The C++ fixture has two commands:

```text
koinos_restore_baseline_fixture prepare <new-output-directory>
koinos_restore_baseline_fixture restore <source-fixture-directory> <new-output-directory> <fixture-url>
```

Outputs must be inside the repository's `build-single-copy-evidence/`. The
driver runs one warm-up and the requested measured repetitions for each of
`file://` and loopback HTTP. The loopback server also returns a tampered signed
manifest and a truncated object; each must fail before activation. Response
sizes, fixture sizes, retries, and command duration are bounded by the harness.
The executable also checks Boost error formatting before creating output. The
native node CLI is used to cross-check any signal in the smaller diagnostic
executable; their static-library linkages differ. A signal is not an accepted
rejection: the driver writes a `BLOCKED` summary and exits 2, and CTest fails.
See the [SC0 baseline report](../../docs/validation/single-copy-restore-sc0-baseline.md)
for the initial formatting crash and the separate, graceful CLI outcome.

The payload has nine RocksDB column families and deterministic records in three
of them: 2,048 rows each, 1,024 value bytes per row, xorshift32 seed `0x5c0001`,
no compression, a 16 MiB block cache, and a checked synchronous write and flush.
These settings define the diagnostic workload, not recommended production
tuning. A publicly known synthetic Ed25519 seed signs fixture metadata; only
the public key is written. Neither key represents a producer or backup operator.

`fixture.json` records the logical digest, backup ID, file/manifest hashes,
settings and RocksDB version. Generated RocksDB metadata, timestamps, paths and
backup IDs can differ between runs; the logical dataset digest must stay the
same. One prepared snapshot is reused for every measured restore within a run.
The source is a valid RocksDB database, but its records and genesis placeholder
are **not a valid Koinos chain**. This cannot establish replay or mainnet parity.

`restore.json` records file hashes, separate repository/stage inodes, allocations
at each phase, same-inode activation, full logical-data verification after reopen,
elapsed time and process peak RSS. `commands.json` includes actual arguments,
exit codes and filesystem samples; `environment.json` includes compiler/build
identity and worktree hashes. `summary.json` separates transports, excludes warm-
ups and reports sample count, median, minimum and maximum. `artifact-sha256.json`
hashes the retained evidence. Source/checkpoint files are outside the measured
destination; their storage is retained and disclosed, not silently excluded
from the host disk requirement.

Allocated bytes use `st_blocks * 512`, with inode deduplication. This does not
prove exclusive APFS extent allocation. Filesystem free-space samples can also
reflect unrelated host activity. Sampling provides an observed peak, not an
absolute bound on every transient write. Native peak RSS is reported separately
from the Python driver, which may run under Rosetta. Use the recorded CMake
compiler architecture and `file` output for native architecture claims.

The tools preserve ordinary durability and snapshot contents. They establish
baseline behavior and local regressions only; 40 GB capacity, recovery from
physical power loss, and Linux/ext4 qualification belong to later milestones.

## SC2 direct staging diagnostics

The separate `koinos_direct_restore_fixture` exercises the opt-in direct-staging
library against SC0's synthetic signed backup. It permits only repository-local
ignored evidence paths, its own file source or loopback HTTP, and the known
synthetic chain identity. It never starts a node, opens/activates the target DB,
uses operator keys or cleans an existing unowned attempt. It is **not** an
operational command for the intended low-resource server.

```bash
cmake --build build-sc0-baseline --target koinos_direct_restore_fixture koinos_backup_direct_restore_test --parallel 4
ctest --test-dir build-sc0-baseline --output-on-failure -R '^(koinos_backup_direct_restore_test|teleno_direct_restore_harness_test)$'
python3 tests/backup/direct_restore_harness_test.py \
  --fixture build-sc0-baseline/koinos_direct_restore_fixture \
  --legacy build-sc0-baseline/koinos_restore_baseline_fixture --repository .
```

Raw fixture interface (only use synthetic sources prepared by the SC0 tool):

```text
koinos_direct_restore_fixture SOURCE TARGET ATTEMPT URL new|resume FAULT
```

`FAULT` is `none`, `kill-after-descriptor`, `kill-mid-file`,
`kill-after-rename`, `cancel`, `cancel-wait` or `no-space`. The process-exit faults
use exit 99; incomplete transfers report JSON and exit 2; invalid/unowned inputs
exit 1. Successful stage-only download exits 0 and still reports
`ready_to_activate: false`. Attempts and reports are retained for inspection.

The Python harness performs one warm-up plus five small measured direct runs,
rehash-only resume, repair of a changed file, process-exit/resume checks, finite
failure cases and exact-byte comparison with the legacy stage. It records each
command/output/exit, loopback request paths, process RSS, allocation observations
and unchanged source hashes. The native unit test also streams a 64 MiB object
to two independent destinations without holding a payload-sized buffer. Results
are fixture evidence, not a complete-node memory bound, sustained performance
benchmark, physical power-loss proof or 40 GB server qualification.

## SC3 offline validation/activation regression

`koinos_backup_direct_activation_test` creates only fresh repository-local
fixtures. It uses the checked-in Harbinger genesis and a real nine-CF database,
plus an explicitly synthetic nonzero checkpoint and WAL fixture. It tests
offline validation, every instrumented journal/sync/marker/rename boundary with
exceptions and abrupt process exits, exact-inode recovery, startup exclusion,
observer holds, invalid data and an isolated native genesis observer smoke with
network sockets and optional writers disabled. It never uses producer keys or
activates production. The synthetic checkpoint is not historical replay proof.

```bash
cmake --build build-sc3-final-20261002 --parallel 4
ctest --test-dir build-sc3-final-20261002 --output-on-failure \
  -R '^koinos_backup_direct_activation_test$'
```

The test requires the matching `teleno_node` executable as its sole argument
(CTest supplies it). Artifacts, source hashes, fault boundaries and logs remain
under ignored `build-single-copy-evidence/sc3-unit-*/`. No operational CLI is
provided by this diagnostic. See the [SC3 report](../../docs/validation/single-copy-restore-sc3-activation.md).

## SC4 standalone CLI regression

`teleno_direct_restore_workflow_cli_test` calls the real native CLI on a fresh,
signed Harbinger-genesis RocksDB fixture. It covers stage/resume, explicit
activation, interrupted publication recovery, cancellation, identity/argument
refusal, default repository restore and a socket-free observer smoke. The test
binary's `--sc4-prepare` / `--sc4-interrupt-activation` helpers are confined to
owned ignored fixtures, not operator restoration tools.

```bash
cmake --build build --parallel 4
ctest --test-dir build -V -R '^teleno_direct_restore_workflow_cli_test$'
```

See [operator commands](../../docs/backup-restore-cli.md#direct-stage-resume-and-explicit-activation-sc4)
and [actual SC4 evidence](../../docs/validation/single-copy-restore-sc4-cli.md).
No full historical, Linux/container or 40 GB claim follows from these fixtures.

## SC5 local capacity experiment

This local-only Python 3.9+ driver selects a verified repository-local,
test-enabled CMake tree and the matching native node/test helper. It creates a
new owned ignored directory, never accepts a live basedir or external snapshot,
never contacts public peers, and retains its source and diagnostic records.
By default it also retains every generated destination case.

```bash
python3 tools/backup/validate_single_copy_restore.py --build-dir build --repetitions 5
ctest --test-dir build -V -R '^teleno_single_copy_(measurement|experiment)_test$'
```

The fixture is Harbinger genesis with 32 MiB of deterministic xorshift bytes in
the optional contract-metadata CF (8,192 rows x 4,096 bytes, seed 0x5c0005,
no compression). Publicly known fixture signatures and an independently pinned
genesis identity are used. These bytes are not representative chain activity.
The independent source stays outside the destination volume; its allocation is
disclosed and its hashes must remain unchanged.

One excluded warm-up pair plus five to ten measured pairs alternate order and
use the same snapshot. The driver checks exact DB hashes before opening, same
DB inode on activation, checked read-only CF/genesis consistency and equal
verification reports. The final pair adds two clean observer restarts and
direct SIGKILL/recovery, with networking, production and optional writers disabled.
It processes no successor blocks and is not differential historical validation.

`--retire-completed-pairs` bounds disposable destination retention to the current
pair. Only after both modes pass their original checks, the driver records each
case's file hashes, allocation and owned directory identity, then retires those
newly generated synthetic trees. Source, command logs, measurements and immutable
retirement manifests remain. Failed or unverified pairs are kept. The final
pair still performs all restart/recovery checks before retirement. CTest uses
this option; it does not reduce the warm-up or five measured pairs. Retirement
CPU, I/O and wall time are reported separately from native phase measurements.
The Python runtime must support fd-based symlink-safe `shutil.rmtree`.

Records contain argv, exit, stdout/stderr, wall time, exact child peak RSS via
wait4 (Darwin bytes/Linux KiB normalized), Python-driver peak RSS, per-case
allocation and filesystem free-space observations. File/directory allocation
uses st_blocks x 512 with inode deduplication; it is not an APFS exclusive-extent
measurement. Samples occur after traversal plus 20 ms sleep and at phase ends;
short transient peaks can be missed. Restore/inspection statistics exclude
warm-ups and later observer restart phases, which have separate resource fields.
Report count/median/min/max/MAD. No physical cache purge or host isolation occurs.

Optional macOS image runs use a newly created APFS sparse image beneath the
owned scratch root, explicit 512-byte sectors, measured mounted geometry,
non-forced detach and retained backing files. No existing device is formatted.
Image capacity must be 1 GiB–40 decimal GB, in whole sectors. The host must be
able to back the full requested image plus 10 GiB reserve; monitoring retains
that reserve. This conservative safety gate can block a sparse image even when
the tiny fixture itself would fit.

```bash
python3 tools/backup/validate_single_copy_restore.py --build-dir build \
  --apfs-capacity-bytes 40000000000
python3 tools/backup/validate_single_copy_restore.py --build-dir build \
  --apfs-capacity-bytes 1073741824
```

Exit 0 means SYNTHETIC_PASS only. Exit 2 means BLOCKED or CAPACITY_REJECTED; read
summary.json and retained command logs. A minimum-space preflight can pass while
the recommended reserve cannot: stage must then refuse without a payload.
Capacity and whole-server qualification flags remain false in all modes.
Without the explicit retirement option, no generated case cleanup occurs.
No source download, 24-hour workload or Linux VM provisioning is performed.
See [SC5 evidence](../../docs/validation/single-copy-restore-sc5-capacity.md)
for actual executed checks and exact missing prerequisites.

## LR0 full-history resource baseline

`measure_low_resource_baseline.py` extends the same isolated SC5 experiment.
It records a versioned **full-history observer** resource contract, build/host
manifest, source metadata digests, child CPU/RSS and sampled file allocation
by SST, WAL, DB metadata, logs and content-addressed objects. On Linux it also
samples the owned child's `/proc` counters, host `MemAvailable` and cgroup-v2
memory when available. Shared cgroup measurements are not attributed solely
to the child. Missing counters are `null` with a reason, never observed zero.

```bash
python3 tests/scripts/low_resource_baseline_test.py
python3 tools/backup/measure_low_resource_baseline.py --build-dir build \
  --repetitions 5 --baseline-checks
ctest --test-dir build --output-on-failure \
  -R '^teleno_(low_resource_baseline|single_copy_measurement)_test$'
```

`--baseline-checks` records the incremental native build, full CTest, version
and help before measurement. `--dependency-build` additionally runs the standard
dependency script with tests enabled and the selected repository-local build
directory. Each baseline command has a 600-second bound; measured fixture
commands retain the 120-second bound. An error terminates only the newly owned
command process group and retains its logs, aborted state and available samples.
This is not a permission to stop existing node processes.

Previously acquired metadata can optionally be inspected read-only:

```bash
python3 tools/backup/measure_low_resource_baseline.py --build-dir build \
  --repetitions 5 --baseline-checks \
  --source-metadata-dir build-single-copy-evidence/<PINNED_METADATA_DIRECTORY>
```

The metadata directory must be inside the ignored repository evidence tree.
Only bounded regular `manifest.json` and `files.json` inputs are read. This
does not fetch payload, refresh the publisher, authenticate metadata, establish
chain identity or replace native preflight. No live basedir, external payload,
target server, producer key or existing filesystem mount is accepted.

Results are retained in a new `build-single-copy-evidence/sc5-run-*/`:
`lr0-baseline.json`, `lr0-environment.json`, `lr0-artifact-sha256.json`,
`commands.json`, SC5 `runs.json` / `summary.json`, source hashes and command logs.
The artifact index excludes SST files; exact fixture/source DB hashes are
recorded separately by SC5. Restore statistics exclude warm-up and observer
restart phases. LR0 category maxima include measured observer phases and are
independent maxima, not a simultaneous sum or a write-amplification metric.

Exit 0 / `LOCAL_BASELINE_RECORDED` means the local fixture baseline was collected,
**not** that every software test passed. Inspect `baseline_checks` and
`all_baseline_checks_passed`; a failing baseline is preserved. Full-history,
Linux, whole-server 40-GB and reserve-calibration flags remain false. No runtime
profile, pruning, smaller reserve or durability change is implemented by LR0.
See the [LR0 report](../../docs/validation/low-resource-lr0-baseline.md) for
executed commands and remaining qualification blockers.
