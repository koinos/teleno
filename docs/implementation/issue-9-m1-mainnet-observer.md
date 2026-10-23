# M1 updated-node mainnet observer on OVH

The operator requested a complete Linux build and an observer run on the
isolated OVH test VPS on 2026-10-21. Continue the application-memory
optimization objective without an overall execution deadline. The Hetzner
producer remains unchanged.

Build a source-consistent Linux node from the identified development snapshot.
Complete the relevant regression checks, resolve the reproduced conversion
failure, verify the executable and CLI on OVH, then run that exact binary with
block production disabled. A successful compile alone is not qualification.

Reuse the stopped, complete full-history test database and its known mainnet
configuration. Preserve the selected backup and every block and receipt. Verify
that no other process owns the database, the storage gate is available, chain
identity matches the selected mainnet input, and the explicit launch disables
block production. Keep RPC local to the test host.

Record observer starts separately from the closed C9 observation and preserve
all earlier counters. Sample RSS, host MemAvailable, CPU, I/O and total disk
use, including the existing recovery reserve. Keep the existing 2-vCPU,
4-decimal-GB RAM, no-swap and strict 40-decimal-GB acceptance limits. Stop only
the owned observer if its confirmed resource or correctness guard fails;
preserve the database and evidence. Do not interpret a tool polling timeout as
a native failure or restart an unchanged failed process automatically.

Verify advancing head/LIB and canonical agreement with an independent mainnet
reference. Exercise the recorded readonly mixed workload and clean reopen as
separate measurements. Keep these results distinct from synthetic transaction
fixtures, producer evidence and the final M1 acceptance decision. Neither
funding nor block production is part of this observer request.

## Safe Linux build and observer goal

Compile the operator-maintained Teleno source in an owned, isolated directory
on the authorized Linux builder. The builder compiles only: execute native
tests, sanitizer controls, version/help checks and runtime profiles exclusively
on the isolated OVH VPS. Keep build artifacts and evidence on mounted My
Passport. Verify hashes before retiring an owned duplicate to recover disk
space. Preserve builder services, operational files and unrelated data.

Qualify the exact rebuilt binary before any observer start. Check the source,
dependency, configuration and executable identities; run the relevant memory,
JSON/RPC and failure-retry regressions; then complete the normal and sanitizer
gates. Treat compilation and component checks as separate evidence. If a gate
fails, retain its result, correct the application source and rebuild under a
new identity. There is no overall execution deadline or automatic repeat loop.

Start only the qualified binary against the existing stopped OVH full-history
database after checking its chain identity, exclusive ownership and resource
headroom. Require observer configuration, disabled block production and local
RPC. Stop only the owned test observer on a confirmed failure and preserve its
data and evidence. Do not contact or alter the Hetzner producer.

The R4 complete Linux build passed, with source archive SHA-256
`b1214898658c2ee1e5e7dcdaf1f98f769608bcaa658d846d1b636f2b8a8b9766`
and node SHA-256
`3ec8e9dd18406842c5d1e74b948246f111821dee600b9f8e8d9d5bb5294c0bbf`.
R4 predates the later Protobuf and request-parsing fixes and is not an observer
candidate. The R6 complete Linux build also passed, with source archive
SHA-256 `e3f51eb0e6819012b8437eb03f07fa3fb97c5de18eff92ce32a71c4af9015a5a`
and node SHA-256
`1b456affbca5edeea78b3524d5c36e7c38527b7003a6225851c56a0e5bc88364`.
R6 has not passed its OVH gates or run as an observer. Neither development
build consumes or resets a frozen C13/C14 candidate slot. The private build
and observation records live on mounted My Passport; results for older
binaries do not transfer automatically.

The R6 input-conversion fault sweep subsequently fails at ordinal 55 with
400 B retained after a local refusal. An isolated trace identifies the
`ProtoWriter` constructor's early raw `TypeInfo` allocation. The pinned,
build-owned repair passes 131 injected failures plus successful retry in an
R8 component on OVH. R6 remains unqualified; rebuild and qualify a complete
source-consistent node with this repair before any observer start. Preserve
the R6 failure and R8 component evidence separately.

R9 incorporates the pinned Protobuf repair. Its complete build-only Linux node
and 12,131-entry build tree are externally hash verified. Source archive
SHA-256:
`8f7f9e37190be31cc6b540c3f3ed775e8bb01b612cd8446ade56c152d4418e02`;
node SHA-256:
`411928cb47cd9b1e5a2db11ae16cbfa9f9fca4f50704df73144ab1e4310a08ab`.
The corrected pinned Protobuf recipe passes 7/7 on OVH. The complete R9
native test-executable build is in progress on the build-only miner; no test
or CLI has run there. R9 must pass its OVH checks before any observer start.

The exact R9 node passes `--version` and `--help` on OVH. Its temporary staged
CLI copy was verified and retired, while the source and 101-test metadata
remain staged. The native test-executable build and runtime qualification are
still pending; no R9 observer has started.

The complete R9 normal build and all 586 ELF files are now externally verified.
OVH passes all 101 registered normal tests and both native CLI checks. The
test-stage path and dependency-input failures are retained with their separate
corrections; passing cases are uniquely reconciled. The minimum passing-case
strict disk margin is 163,143,680 bytes including the reserve. A final-source
sanitizer build is in progress on the build-only miner. Sanitizer execution and
the exact-binary mainnet observer gates remain prerequisites; no R9 observer
has started.

A read-only OVH preflight on 2026-10-22 confirms that the stopped test
database still exists, the selected observer configuration has its original
SHA-256, no native node is running, swap is disabled, and host
`MemAvailable` exceeds the required floor. Its strict 40-decimal-GB disk
margin, after the full 8-GiB recovery reserve, is only about 545 MB before
catch-up. Do not infer that this is enough for WAL growth or a major
compaction. Reassess disk headroom and set a guarded stop threshold before an
observer start; preserve the full-history database and reserve.

The exact R9 source passes its 61/61 focused ASan/UBSan OVH invocations,
including working sanitizer controls. Its separate 22-executable TSan build
is complete and externally verified; TSan tests are running on OVH. These
component results do not authorize an observer start before the remaining
qualification and disk-headroom review.

The first R9 TSan sequence passed 22 invocations, including the working
detector controls and aggregate-memory case, then retained a failure at the
cleanup case's original 120-second CTest timeout. CTest exited with status 8;
resource guards did not fire, so this is not evidence of a race. Its exact
staged binary was externally verified before retirement. A separate
diagnostic continuation keeps the source, instrumentation, assertions and
fault sweep unchanged while allowing 600 seconds for that case. The original
failure remains in the record. No R9 observer has started.

The unchanged TSan cleanup case passes with that 600-second allowance. Its
first diagnostic run could not use the artifact helper because of a new case
label, so it was preserved by a separately hash-verified helper before the
generated copy was retired. The remaining 36 TSan cases now run under a
supported label. These are separate records; no observer has started.

TSan reconciliation now establishes 59 unique passing same-source invocations
across 60 actual invocations, with the original 120-second timeout and the
artifact-helper name failure preserved separately. Both externally preserved
OVH test-stage copies were retired. R9's exact node binary passed native
read-only preflight with 596,082,688 B strict disk margin after the reserve.
Its first observer start then exited after about one second with a fatal
RocksDB compression requirement error: the build linked a Hunter RocksDB
library without Zstd, while this database requires Zstd. No native resource
guard fired, no block production occurred, and the failed process is stopped.
The R9 build's CMakeCache identifies the unsupported Hunter installation;
the authorized miner also has a hash-verified Zstd-enabled RocksDB and matching
static Zstd dependency. Build a separate same-source, corrected-dependency
candidate and repeat the relevant OVH qualification before another observer
start. Do not reinterpret the R9 test passes as whole-node readiness.

R10 uses the identical source archive with the builder's Zstd-enabled RocksDB
and static Zstd libraries. Its complete build-only Linux tree is externally
verified under the owned My Passport run; node SHA-256 is
`00c6f3c4a87d7ff01ae58eda41a3ed4713c80f1bdb19b7aa8ad52617bd0f9f6f`.
On OVH, all **101 registered normal tests** and both native CLI checks pass.
The initial missing-dependency recipe input and expected Git-version-label
failures remain preserved; exact build-owned inputs and the archive build's
actual `1.3.0-dev.1+unknown` output were verified in separate continuations.
Minimum test strict disk margin was **154,886,144 B**, including the full
recovery reserve; minimum host `MemAvailable` was **3,200,397,312 B**.
R9's same-source ASan/UBSan and TSan results remain scoped to its earlier
RocksDB dependency closure. R10's native read-only preflight passes, and its
first isolated OVH observer start shows RocksDB selecting Zstd for both
databases. The node ran as an observer with block production disabled.
It caught up to the independent public head at 2026-10-22 03:24:46 UTC (both
at height 40,414,135). A read-only comparison at 03:24:53 UTC agrees on
mainnet chain identity, head height and ID, and irreversible block ID at
height 40,414,076. The public receipt omits `state_merkle_root`, which Teleno
includes; the remaining common receipt fields in a separate read-only
diagnostic agree. Public historical-block queries have also intermittently
timed out; record these as reference availability, not native correctness
failures. The first one-hour mixed-read workload began at 03:25:12 UTC and
stopped at 03:46:33 UTC after three consecutive public-reference timeouts.
Its failed result is preserved: 1,261 successful local cycles, zero local
cycle errors, 30 successful independent comparisons and 14 unavailable
comparisons. The public endpoint is
[documented as rate-limited](https://docs.koinos.io/exchanges/koilib/).
At 03:49:14 UTC its reported head was 343 blocks behind the local observer;
this single observation is evidence of reference lag, not proof of a native
fork or of current canonical agreement at the local head. A separate one-hour
mixed-read continuation began at 03:49:38 UTC on the same exact observer and
**passed** after 3,600.6 seconds. It completed 3,573 local read cycles with
zero local failures and 15 successful independent canonical comparisons.
Fifteen public-reference attempts were unavailable; their errors are retained
separately. The local head advanced from 40,414,650 to 40,415,803. Median
local cycle latency was 0.0130 seconds and p95 was 0.0194 seconds. No
canonical mismatch was observed.

The first observer then stopped cleanly with exit status zero. Its peak sampled
RSS was 627,982,336 B; minimum host `MemAvailable` was 2,769,764,352 B and
minimum strict disk margin, including the full reserve, was 495,771,648 B.
It used 1,667.26 CPU user seconds and 32.04 CPU system seconds. No OOM or swap
use occurred. The same binary and database reopened at 04:51:42 UTC without a
simultaneous database owner. The second observer advanced 92 blocks from its
first reopened canonical proof and passed three independent head/irreversible
checks. It stopped cleanly after 306 seconds with exit status zero, peak
sampled RSS 109,805,568 B, minimum host `MemAvailable` 3,320,688,640 B and
minimum strict disk margin 1,172,430,848 B. Both owned observer units are
inactive; no test producer start occurred.

All 32 runtime evidence files are externally archived and hash verified under
the private My Passport run (`artifacts/observer-r10-runtime-evidence.tar.gz`,
SHA-256 `a3fb58913a98a803215be867ba4a29cf40ed050f871d3dac482a6e2d7a7887dd`).
The exact R10 Zstd-closure ASan/UBSan focused build is externally verified and
passes **61/61** OVH invocations, including clean/known-fault controls,
aggregate admission, cleanup and JSON/RPC failure cases. The minimum sampled
strict disk margin across those cases is 1,131,679,744 B, minimum host
`MemAvailable` is 2,872,823,808 B and peak cgroup memory is 573,571,072 B.
The separately hash-verified exact-closure TSan build underwent OVH execution.
Prebuilt SDK libraries and the focused build's disabled libp2p
remain instrumentation exclusions. The earlier R9 sanitizer results used a
different RocksDB closure.
Its first TSan sequence passed 22 invocations, including working race controls
and the aggregate-memory test, then retained a CTest timeout at 120 seconds
for the unchanged cleanup fault sweep. The native result was status 8, with no
resource guard or detector alert. A separate diagnostic continuation allows
600 seconds for that same test, without changing the source, assertions or
instrumentation. The original timeout remains failed evidence.
The cleanup continuation passed in 155.78 seconds. Its next gRPC server test
failed under TSan with a `grpc_chttp2_transport` data-race report, so that
continuation stopped and retained its output. Build-link inspection shows the
R10 focused TSan build selected the ordinary, uninstrumented Hunter gRPC
archives: the pinned gRPC remap overlay was conditional on the earlier R9
source directory and did not apply to R10. The R9 TSan build used the pinned
instrumented gRPC archives. This is a dependency-instrumentation gap, not
evidence by itself that Teleno's gRPC ownership code races. The failed
uninstrumented result remains preserved.

A corrected, hash-pinned R10 TSan build uses the same source and Zstd RocksDB
closure, with five instrumented gRPC archives replacing the ordinary gRPC
archives for diagnostic coverage. Its verified build archive has SHA-256
`2d392d6c60f5face8758904635e0480921fbc0489910bfbb918f137e4bd7d242`.
The complete focused OVH matrix passes **59/59** invocations, including clean
and known-race controls, the gRPC server case, aggregate-memory cases and the
unchanged cleanup sweep under its recorded 600-second test-only allowance.
Minimum sampled strict disk margin is 1,139,589,120 B after the full reserve;
minimum host `MemAvailable` is 3,189,100,544 B. No resource guard fired. This
is focused sanitizer coverage, not a whole-node race-freedom claim.

R10 therefore completes this separate build-and-observer qualification:
101/101 normal tests, 2/2 CLI checks, 61/61 focused ASan/UBSan and 59/59
focused TSan checks, one hour of successful mixed reads with independent
canonical comparisons, and a clean full-history database reopen. Both test
observer starts ended cleanly; no test process or producer remains active.
The consolidated private evidence is
`evidence/observer-r10-final-reconciliation.json` on My Passport. The earlier
R9 compression failure, first reference-dependent mixed-window failure,
initial R10 TSan timeout and uninstrumented-gRPC TSan signal remain separate
failed results.
This observer evidence does not complete M1 aggregate ownership, measured
policy selection or paired physical-memory qualification.
