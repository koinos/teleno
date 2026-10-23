# Native 1.3.0 publication and qualification

Published on 2026-10-23T18:21:41Z as the stable, non-prerelease
[Teleno native 1.3.0](https://github.com/koinos/teleno/releases/tag/teleno-node-v1.3.0). This is an independent native-node release.

## Exact identities

| Item | Identity |
| --- | --- |
| Qualified source and release tag target | `2c11229267cff2f9ee39ed8e56ab70af391d85ce` / `teleno-node-v1.3.0` |
| Source archive SHA-256 | `d8f77832f83ea1a7c82c7f0487c81fd973a8dc855f3e0fa671d079bf35570608` |
| Linux x86_64 native ELF SHA-256 | `9fcb043727b23ab05d9660701a61f369f9413a82ff646f94f2dbf830fb4de53f` |
| Reported native version | `teleno_node 1.3.0+2c11229267cf` |
| Actual linked archive identities | 169, in the release build-info asset |
| Compiler | GNU C++ 13.3.0, Ubuntu 24.04 |
| Native release optimization | `-O2 -g0 -DNDEBUG --param=ggc-min-expand=20 --param=ggc-min-heapsize=32768` |
| Runtime ABI requirement | Linux x86_64, GLIBC 2.38 or later |

The clean release branch integrates M1 in `2b8805a`, merges the committed
Fogata alpha.2 corrections in `e33319c`, and corrects the pinned gRPC archive
location in `2c11229`. The archive SHA-256 is unchanged; custom/local source
overrides retain hash verification. Original mixed LR2 changes remain preserved.
The [release notes](../releases/1.3.0.md) describe the full change set and limits.

## Exact native release checks

- 107/107 registered normal checks pass on the isolated qualification VPS.
- Four benchmark-tool unit tests pass against private-network mock services.
- Both actual native version/help checks pass on that host.
- Six focused Fogata ASan/UBSan and five default-TSan invocations pass, including
  clean and known-fault controls. Final-source fixture/control ELFs are
  byte-identical to the tested supporting S2 build. Native P2P code and the
  pinned GossipSub/listener objects are instrumented; other prebuilt SDK/native
  archives remain excluded. ASan/UBSan omits vptr checking for SDK compatibility.
  `GRPC_ASAN_SUPPRESSED` preserves the normal SDK header layout and excludes its
  mutex/CV leak checker; compiler instrumentation remains enabled. TSan retains
  deadlock detection and uses child-only ASLR-off. This is not whole-process
  race freedom.
- The exact full-history native observer completes 3600.443208
  seconds of mixed reads, 3,583 successful cycles and zero
  failed cycles. All 31 independent common-LIB block
  comparisons agree; 13 available root pairs
  agree. Missing root fields are not treated as matched roots. The maximum
  observed absolute tip lag at known comparisons is 2 blocks.
- Clean shutdown and same-database reopening pass. The reopened node has
  9 independent canonical samples. Exactly two
  observer starts occur; both finish stopped with clean native exits. No
  mainnet producer starts or operational producer deployment occur.

## Resources and tradeoffs

The runtime qualification preserves Ubuntu/x86_64/ext4, two vCPUs, the
4,000,000,000-byte target physical-RAM envelope, no swap and the strict
40,000,000,000-byte whole-server disk budget. Storage includes the raw device,
OS, software, databases, WAL, logs, temporary files and
`max(8 GiB, database bytes / 5)` free recovery reserve. The 8-GiB floor dominates
for this database. The larger raw device does not enlarge the budget.
The small host-RAM reporting excess is subtracted from `MemAvailable`.

| Measured release result | Bytes |
| --- | ---: |
| Conservative native peak RSS | 414,470,144 |
| Observer-1 maximum retained steady RSS after 300 s | 414,470,144 |
| Reopened observer maximum retained steady RSS | 99,536,896 |
| Minimum adjusted host `MemAvailable` | 2,952,484,864 |
| Minimum strict disk margin after the full reserve | 428,666,880 |

No OOM, swap configuration or swap activity occurs during these phases.
Conservative peak RSS takes the larger of `wait4` kernel peak and sampled
`/proc` RSS; both raw measures remain in the qualification record.

| Phase | User CPU seconds | System CPU seconds | Physical input, 512-B blocks | Physical output, 512-B blocks |
| --- | ---: | ---: | ---: | ---: |
| observer1 | 625.389605 | 15.350877 | 568192 | 582144 |
| observer2 | 20.039721 | 0.771345 | 58360 | 104408 |

Mixed-read cycle latency p50 is 0.012545 seconds;
p95 is 0.021080 seconds. A cycle contains multiple
local RPC reads. Observer 1 includes catch-up before the measured hour. These
figures are measurements of the identified inputs/workload, not paired evidence
of a release-wide speed or RSS improvement.

## Container source validation and publication

The exact-source [Container dry-run](https://github.com/koinos/teleno/actions/runs/39978257157)
passes fresh source compilation, version/help and producer-consent refusal.
The [native-tag publication workflow](https://github.com/koinos/teleno/actions/runs/40001203766) now passes
those package checks and publishes the image from source `2c11229267cff2f9ee39ed8e56ab70af391d85ce`.

| Image identity | Value |
| --- | --- |
| Registry | `ghcr.io/koinos/teleno` |
| Immutable manifest digest | `sha256:40d109f526d4e5922e09137fe6244890e3956e7acd6f9cfb92dc2211b1297b5d` |
| Linux amd64 image ELF SHA-256 | `7b0f0f8fb97e2f470651894f0332ce1b593e826c026ded3feab71aa671e7d409` |
| Image ELF bytes | 45177240 |
| Native downloadable ELF byte identity | `false` |
| Verified version | `teleno_node 1.3.0+2c11229267cf` |

Tags `1.3.0`, `teleno-node-v1.3.0` and `latest` resolve to that manifest.
The `beta` and `1.3.0-alpha.2` manifests remain unchanged. Registry manifests,
configuration/layer hashes and the extracted ELF were independently read and
verified without running a container or native binary on the Mac/builder.
This source-built image has its own executable identity; CLI/package validation
is distinct from the downloadable native ELF's full-history observer evidence.

The release adds `container-provenance.json` and `container-provenance.sha256`.
The core native assets and original `SHA256SUMS` remain unchanged. The image
provenance JSON SHA-256 is `e6de815318e3a866014727d9a220c2e909640b48c7ec5ff35fb0293f09c53765`. Both supplemental
assets were downloaded again and verified after publication.

## Published native assets

| Asset | Bytes | SHA-256 |
| --- | ---: | --- |
| `SHA256SUMS` | 400 | `8de38db86595590a2332d205ac2df3b4abe896a3166cb3794ca85ec5cc9b0c87` |
| `teleno-node-1.3.0-linux-amd64.tar.gz` | 15605241 | `4e22385ebf3eee9e8675a923ad83b735f87b880cfcf94d35aa6dfc6cda53e009` |
| `teleno-node-1.3.0-qualification.tar.gz` | 61800 | `0b5dedf2b883dba7d512e260f4d496c7f1fa845be2c47fc1b06c25a6681722f9` |
| `teleno-node-1.3.0-source.tar.gz` | 2043398 | `d8f77832f83ea1a7c82c7f0487c81fd973a8dc855f3e0fa671d079bf35570608` |
| `teleno-node-build-info.json` | 113492 | `34074a8b4a3cb2082bfe4afde8949669258b998f2a665024c91ddf1d81cc3fe0` |

The qualification bundle contains reconciled JUnit, native CLI output, source/
binary/dependency identities, resource/canonical summaries and explicit scope.
The source archive and native package are identified independently.

## Preserved development evidence

The earlier S1 Container attempt failed on the missing gRPC archive URL. S2's
first historical-fixture invocation lacked its compile-time path; final checks
use a private unit mount with verified source bytes. Earlier sanitizer helper
selection and TSan compiler-probe/link failures remain preserved. A static
check caught a newline error in the local stop coordinator before execution;
its replacement retained the same native process, workload start and persisted
observer counters. No source/candidate result or native failure was relabeled.

Owned evidence is retained under
`/Volumes/My Passport/teleno-1.3.0-final-20261023-k2s7bouk/final-s3/`.
Its source, build, unit, detector and stopped-observer archives have verified
inventories/hashes. Inactive owned build/test copies were retired only after
external verification; operational databases and services were preserved.

## M1 predecessor, remaining gates and app handoff

[R15](low-resource-lr2-m1-r15.md) completes M1 for its exact source archive
`3be9aa55f128f649d851ec87ca9dfa3574736eec0bdec9e4e9ebaecbae47995f`
and node `e825f1951a583590e9173c57be79626635dbb110f750d1258eb34efffa31eef4`.
Its 104 normal, 63 focused ASan/UBSan, 61 default-TSan cases, five equivalent
pairs per measured workload, controlled 100-block/12,800-test-transaction chain
and observer restart/interruption results remain predecessor evidence.
They do not claim identical measured savings for the combined release binary.
M1 quotas remain accounting/admission policies, not total-heap/RSS ceilings.
Issue 9 and D1/P2/R1/V1/S1/Q1/H1/loaded-producer requirements remain open where
not independently demonstrated. Historical failures and counters stay intact.

Three Koinos One manual pages now identify this native source and Linux ELF;
`mkdocs build --strict` passes with output on the external evidence volume.
The app's existing source changes and native submodule pin are preserved. App
integration requires pinning `2c11229267cff2f9ee39ed8e56ab70af391d85ce`, building the appropriate platform
runtime and passing app package checks. No GUI, app package, app commit/push,
wallet, key, funds or mainnet activation operation is part of this release.

The working release branch advances to `1.3.1-dev.0` after publication; the
stable tag and artifacts retain source `2c11229267cff2f9ee39ed8e56ab70af391d85ce`. For a later deployment,
retain a verified compatible observer binary/configuration, stop its replacement
before rollback, and never open one database with two processes. No rollback or
operational deployment is performed here.
