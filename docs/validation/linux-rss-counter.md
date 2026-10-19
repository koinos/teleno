# Linux resident-memory reporting correction

Date: 2026-10-16. Branch: `codex/lr2-memory-hardening`.
Status: **native regression validation passes; whole-node qualification open**.

The reference observer's native `rss_bytes` log substantially exceeds its
independently sampled kernel RSS. The Linux implementation reads the first
`/proc/self/statm` field, virtual pages, rather than the second, resident pages.
Consequently untouched virtual mappings are incorrectly counted as resident
memory. Earlier VPS capacity observations use kernel `VmRSS`/`VmHWM` and GNU
time, so their recorded memory peaks do not depend on the incorrect log.

The Linux branch now reads resident pages and multiplies by runtime page size.
A small shared helper returns zero for unavailable/malformed input, invalid
page size or multiplication overflow. The macOS branch is unchanged. This is
a diagnostic correction, not an actual reduction in RAM use or a new limit.

The regression fixture distinguishes virtual and resident fields at 4-KiB and
16-KiB page sizes and checks malformed/truncated input, numeric overflow and
the exact multiplication boundary. Its Linux integration reserves an untouched
1-GiB `PROT_NONE` mapping, checks it is not counted as resident memory and
compares the helper against `/proc/self/status` `VmRSS`. It does not allocate
or touch 1 GiB of physical RAM. Instrumentation and kernel sampling tolerances
are explicit in the test.

Frozen native manifest, 306 inputs:
`c073ab12836d01ddd761e8146ed829792fee0f92b77c622059cf798951208c3b`.
The preceding manifest is
`af826d523daa1d196808b7a1e69effaadfe6e119bfa4c9be34a15af1b24d3553`.
Changes are the helper, `main.cpp`, regression fixture and its CMake target.
Both binaries report `1.3.0-dev.0+d3cfb94455da-dirty`:

- Mac SHA-256: `3d91e51ecde0acf86933860290fb22467ca0f681c04b2c6273ed1e8ebc2831ae`.
- Linux SHA-256: `52a17d127dcdb0c5f25bb87227d5f41d085c4d3a13ea920e25e160845a0cf2e5`.

Mac all-target build, focused counter/preflight tests, CLI identity/help and
61/61 CTests pass; the guarded full suite takes 164.195 seconds. Linux all
targets, focused tests including the real virtual-mapping check, and CLI
checks pass. Its first 59-test batch and separate direct-restore test pass.
The final synthetic restore comparison initially refuses staging during its
fourth measured pair: retained generated copies leave 10,958,901,248 available
bytes against a 11,040,784,384-byte recommendation. The unchanged native
reserve check correctly refuses the next payload; no resource guard is lowered.

The failed fixture tree is fully archived and member-hash verified externally.
Only verified owned development copies are then retired, including older
diagnostics and completed diagnostic transfer executables. The first retry
prelaunch also refuses because its retained-pair headroom requirement is not
yet met; no CTest or native restore starts in that attempt. After additional
verified duplicate retirement, the same frozen binary passes the single
remaining CTest. Its full fixture archive is independently verified before
remote duplicate retirement. Thus all **61 unique Linux tests pass across
retained attempts**, not an uninterrupted initial 61/61 run.

Owned evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/rss-counter-20261016/`.
`linux-validation.json`, `linux-sc5-retry-r1-result.json`, failed/prelaunch logs
and complete failed/successful fixture archives preserve the sequence. The
correction does not close the [strict disk, sustained observer, producer or
historical gates](reference-vps-observer.md).

The frozen Linux executable subsequently completes a guarded one-hour observer
restart with a clean stop and no resource guard failure. Independent kernel
HWM is 260,673,536 bytes; minimum host `MemAvailable` is 3,218,644,992 bytes.
All 60 native RSS metrics agree with contemporaneous kernel RSS within
835,584 bytes (816 KiB). Captured final height/root 40,262,636 matches the
independently queried legacy reference after reference LIB passes it. This
verifies runtime reporting, not RAM savings, the later dictionary policy or
full resource qualification.
