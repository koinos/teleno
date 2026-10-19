# Direct restore read-buffer experiment

Date: 2026-10-16. Branch: `codex/lr2-memory-hardening`.

A final native HTTPS resume-phase sample exposes a CPU cost: about 46.3 million
write calls for 23.7 GB of process output. The streaming transport constructs a
Beast `flat_buffer` with a 128-KiB maximum but leaves its initial capacity
empty. The pinned implementation consequently reads approximately 512 bytes
at a time. Each received body fragment invokes checked writing and hashing.

The change reserves 64 KiB within the existing wire limit before reading the
response. Body chunks remain bounded at 64 KiB. Size/hash validation,
redirects, deadlines, cancellation, retries, fsync, publication and source
trust policy are unchanged. This is transport buffering, with no database,
record, receipt or consensus change.

## Paired Linux builder measurements

Two diagnostic executables use the real native transport, the same compiler,
dependencies and checked write/SHA-256/fsync sink. Their transport sources
differ only by `wire.reserve( 64 * 1024 )`. Five pairs alternate execution order
against one existing HTTPS backup object, exactly 66,533,451 bytes, SHA-256
`752dd3a2a7034770889c90ee100573b7fbc5ff279ea462da7f69342ddfe5edbb`.
Every output size and hash passes. Only those verified generated output copies
are retired between samples; original backup data and measurements remain.

| Pair | CPU seconds before / after | Wall seconds before / after |
| --- | --- | --- |
| 1 | 1.015012 / 0.471685 | 3.607389 / 2.174299 |
| 2 | 0.972691 / 0.471353 | 2.167744 / 1.938865 |
| 3 | 0.942455 / 0.493282 | 2.102793 / 2.020935 |
| 4 | 0.945564 / 0.494771 | 1.944777 / 2.223869 |
| 5 | 0.921775 / 0.488226 | 1.970836 / 2.272513 |

Median paired CPU consumption decreases **47.67%**. Kernel write calls decrease
from **129,950 to 4,062** in every pair, about **96.87%**. Body callbacks decrease
from 129,949 to 4,061, with the TLS transport delivering larger fragments.
Both variants' maximum process peak RSS is 15,642,624 bytes. Elapsed time
varies with network conditions; these samples establish no reliable download
speedup or whole-restore resource bound.

## Paired reference-VPS measurements

Five further pairs use the designated 2-vCPU / 4-decimal-GB Linux/ext4 VPS,
the same object and verified sink, alternating execution order. All ten sizes
and hashes pass. No observer runs concurrently with these transport samples.

| Pair | CPU seconds before / after | Wall seconds before / after |
| --- | --- | --- |
| 1 | 2.156607 / 0.595553 | 2.246825 / 0.638997 |
| 2 | 2.087979 / 0.529410 | 2.153008 / 0.556279 |
| 3 | 2.402127 / 0.540148 | 2.453944 / 0.581141 |
| 4 | 2.112432 / 0.761424 | 2.163881 / 0.798356 |
| 5 | 2.075594 / 0.560085 | 2.131609 / 0.612072 |

The median paired CPU ratio is 0.269843: **73.02% less CPU** for this object.
The median paired wall-time ratio is 0.284400: **71.56% shorter elapsed time**
in these samples. Write counts are again 129,950 before and 4,062 after in
every pair. Maximum process peaks are 19,619,840 bytes before and 19,865,600
after; this does not establish a memory reduction. These object-transfer
measurements do not estimate complete restore time, database validation cost
or sustained node capacity. The complete 48-entry VPS archive is externally
verified against every member hash.

## Native validation and remaining checks

The production change includes an explanatory comment. Its frozen 304-input
manifest is
`af826d523daa1d196808b7a1e69effaadfe6e119bfa4c9be34a15af1b24d3553`.
Only `src/backup/public_restore.cpp` differs from the preceding
`c3b1cc4e2dad577874d5e0bb0afd8d303a89a5afe0b1d57ba11d016cd99fdb10`
native inputs. All-target native builds, focused restore checks, version/help
and all 60 native tests pass on Mac and Linux. Mac's complete CTest takes
154.15 seconds. Linux's 58/1/1 batches take 138.51, 1.84 and 7.28 seconds;
all generated fixtures are externally archived and verified before retirement.
Strict Koinos One manual build and rendered-text checks pass.
Existing HTTP fixtures cover truncation, wrong sizes/hashes,
chunked overrun, redirects, outages, stalls, cancellation and interrupted resume.

The complete baseline restore succeeds with the preceding frozen executable.
Its result does not establish full-restore performance for this new executable.
The new executable subsequently completes a guarded 900-second observer restart,
catches the recent head and agrees with the independently queried legacy node
at its captured final height/root. See the
[reference VPS report](reference-vps-observer.md) for the short observation,
strict disk shortfall and remaining sustained, recovery, producer and history
gates.

New native executable SHA-256 values are
`b68994f0bdf4ec2b46c92606b1d8e162306edeb648efd1ed68809d9cd4510e0b`
(Linux) and
`ca22f19e6944ce7908f6a13faa2cf8f228f7f1dc888b8b2502ec2303b85211a6`
(Mac). Both report `1.3.0-dev.0+d3cfb94455da-dirty`; the source manifest and
binary hashes identify this change separately from earlier dirty builds.

Owned evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/native-export-k3ky3m61/transfer-buffer-experiment/`
and
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/restore-read-buffer-20261016/`.
The builder's complete 48-entry experiment archive is independently verified
against every member hash. No operational publisher, stable public latest,
GUI, package, submodule pin or mainnet producer activation changes.
