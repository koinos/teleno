# LR2 P2-G: SDK address retention and publication

Date: 2026-10-16. Status: **scoped checkpoint verified: Mac/Linux native 60/60,
paired mechanisms, separate twelve-fixture diagnostics, 15 baseline component
commands, runtime-container and documentation checks pass. P2/LR2 remain open.**

## Original behavior and evidence

The actual pinned cpp-libp2p commit is
`c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`. Its in-memory address repository
owns each address in both an expiry map and an ordered vector. `addAddresses`
and `upsertAddresses` publish the expiry-map element before the vector append.
An allocation failure between those operations leaves a hidden address:
repeating the call returns successful `false`, but `getAddresses` still omits it.

A standalone program links to the existing actual SDK libraries; it does not
reimplement the repository. Both Mac and Linux reproduce six persistent lost
addresses across 12 injected failures, including both APIs at allocation sites
3, 4 and 5. Inputs and a DNS-forbidden fixture resolver are prepared before the
fault. Inspection/retry runs after fault injection is disabled. No networking,
live data, keys or production configuration is used.

Another control inserts 32 peers with already-expired addresses. They remain
visible until explicit `collectGarbage`, then disappear. The preceding native
transport has no SDK metadata collector invocation. This control demonstrates the API behavior,
not a measured live-node leak or complete inventory of SDK ownership.

Growth inputs retain 256 / 1,024 / 4,096 peers with permanent addresses. Linux
ordinary C++ requested bytes retained by the repository are 129,544 / 518,824 /
2,080,504; Mac values are 99,432 / 397,800 / 1,591,464. They return to zero after
repository destruction in these controls. This is retained allocation-request
attribution, excluding prefixes/aligned/direct allocation, allocator overhead,
input preparation and other node consumers; it is not RSS or proof of a leak.

Evidence: `evidence/p2g/` under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/`. Original compiler/link
commands, source/executable/log hashes and raw results are retained. Linux uses
the previously verified isolated builder, one low-priority child and unchanged
10-GiB free-disk / 2-GiB available-memory floors. Existing publisher/configuration
and operational data remain separate.

## Implementation contract

Provide a native SDK-compatible address repository, bound through the existing
host injector, without modifying installed SDK inputs. Preserve address order,
equality, add-versus-upsert TTL semantics, known-peer clear/GC behavior, bootstrap
resolution and complete returned vectors. A single owned address representation
replaces the duplicate expiry-map/vector objects.

Use a shared per-transport policy across runtime generations. Account retained
peer count, address count, actual vector-slot capacity and encoded address/ID
wire bytes. Wire lengths are not private string capacities or physical heap/RSS.
Peer/index/node/allocator overhead, inherited DNS history, decoded input and
caller copies remain explicit additional consumers. No qualified VPS defaults
or YAML/CLI/GUI controls are selected by these component policies.

Stage at most one bounded peer replacement plus a bounded victim-iterator list
while serializing writers through
the shared policy. Prepare owned data and any list/index node before evicting
old optional metadata. Allocation failure preserves the old address/expiry/order
state; successful retry must actually expose the address. Publication must not
allocate. Failure to retain optional advice is a local bypass, never peer blame,
consensus invalidity or a truncated successful query.

Evict oldest unprotected optional peers only. Already-retained configured roots
and live connections remain protected; their authorities/configuration survive
cache refusal. Lifetime accounting follows old repositories until their actual
storage dies, so restart cannot reset retained credits. Observer exceptions
must not turn committed metadata into a reported failed insertion. Reentrant
reads must be safe; reentrant mutation must not invalidate publication state or
create unbounded nested staging.

Add owned, generation-checked metadata collection on the serialized SDK runner;
cancel/drain it before host teardown. Preserve configured TTLs and contain local
collector/scheduling failure without peer scoring. Do not query SDK repositories
from external diagnostic readers or reintroduce the fixed host-pointer race.

### Incremental victim planning

The first implementation inventoried every disposable peer and built a complete
victim-iterator vector for every novel insertion. Five paired common-harness
samples reproduced the resulting allocation/CPU cost. The revised implementation
first computes a bounded desired replacement from borrowed inputs, then scans
only until enough reclaimable room exists. Ordinary unsaturated insertions need
no victim scan or iterator allocation; single-peer saturated churn normally
plans one victim. Two fixed 64-index stack arrays and the necessary victim list
are additional preparation workspace. Protection decisions remain frozen before
publication, including when observers change protection or throw.

## Current verification checkpoint

The first native collection check incorrectly assumed that failed direct dials
populate the SDK address repository. The failed assertion and logs are retained.
Source inspection confirms those dials use explicit `PeerInfo` without storing
addresses here. The corrected integration fixture uses the real DHT bootstrap
path to saturate the injected repository and proves continuing loopback RPC and
preservation of separately owned configured discovery routes. It also checks
collector start/stop/restart, shared limits, positive settings and concurrent
external statistics reads during transport restart. The exercised address
source is DHT bootstrap; no Identify integration coverage is claimed.

Repository tests add 96 new-peer/eviction fault-injection cases across both
add/upsert APIs, successful retry after unchanged refusal, duplicate/per-peer
limits, retained vector capacity after partial expiry, frozen multi-victim
selection and throwing/reentrant observers. Retired DNS callbacks, lifetime
credits, TTL/order/clear and cross-repository concurrency checks remain.

Current Mac all-target/version/help and **60/60 native CTest pass (187.58 s)**.
Native identity is `1.3.0-dev.0+d3cfb94455da-dirty`, executable SHA-256
`724b213d371449a0d4497155f5d02e4bc6dacdfe8d664dade5b6fe0620fe6e77`.
The frozen 304-input source manifest is
`07342ad3c065c0fe6d8bd632994c7a160b9500304c91468b42cde0b69588cb37`;
its source archive is
`af63d6baa769964eaadd6dc4abd9b799b8c1a0f0adf9394c322b358af1c2fee5`.
This is Mac development evidence; the Linux and scoped diagnostic/target-host
results below are separate. Whole-node and historical qualification remain open.

### Paired Mac mechanism measurements

Five alternating pairs per cohort use one common program linked to the actual
pinned SDK libraries, selecting the original SDK repository, preserved prototype
or current native repository. Inputs are prepared before tracking/timing;
retained C++ allocation requests include repository/policy construction. Phase
timing and child CPU/I/O cover insertion; process peak RSS also includes startup
and input preparation. Each tracked repository returns to zero after destruction.

| Inputs | Original SDK / current retained peers | Original SDK / current retained C++ requested bytes | Original SDK / current insertion median |
| --- | --- | --- | --- |
| 256 | 256 / 256 | 100,090 / 64,450 | 52.750 / 72.833 microseconds |
| 1,024 | 1,024 / 1,024 | 398,458 / 254,914 | 161.667 / 231.916 microseconds |
| 4,096 | 4,096 / 1,024 | 1,592,122 / 254,914 | 701.375 / 1,279.959 microseconds |

At 4,096 inputs, incremental planning reduces the preserved prototype's
12,298.458-microsecond insertion median to 1,279.959 microseconds and cumulative
requested bytes from 62,484,090 to 1,041,346. The current original-SDK comparison
still has a CPU cost. For 12,800 equivalent duplicate hits, both allocate zero;
the SDK/current medians are 295.750 / 1,093.500 microseconds. The 4,096-input
retention result includes bounded eviction and is not equal retained coverage.
These are tiny component mechanisms, not full transport, whole-node savings,
Linux qualification or recommended runtime defaults.

Commands, frozen prototype/current headers, the common program, all samples,
failed private driver attempts and provenance are retained in
`evidence/p2g/resume-20261016-rrm0ukg_/` under the owned external run. The
corresponding Linux source export is installed only in the isolated builder;
existing publisher services, configuration and fourteen containers remain
separate. Its unchanged 10-GiB disk / 2-GiB available-memory floors apply.

### Paired Linux mechanism measurements

The same common program and frozen headers run on the authorized larger Linux
builder with the existing GCC 13.3/pinned SDK inputs. Five alternating pairs
per cohort give these medians:

| Inputs | Original SDK / current retained peers | Original SDK / current retained C++ requested bytes | Original SDK / current insertion median |
| --- | --- | --- | --- |
| 256 | 256 / 256 | 130,186 / 66,706 | 291.712 / 250.235 microseconds |
| 1,024 | 1,024 / 1,024 | 519,466 / 264,082 | 1,278.013 / 1,164.239 microseconds |
| 4,096 | 4,096 / 1,024 | 2,081,146 / 264,082 | 9,995.478 / 7,220.954 microseconds |

At 4,096 inputs, incremental planning reduces the preserved prototype's
48,122.859-microsecond median and 62,520,906 cumulative requested bytes to
7,220.954 microseconds and 1,078,162 bytes. For 12,800 duplicate hits, the Linux
SDK performs 38,400 allocation attempts and the current repository performs
zero; medians are 2,865.817 / 2,319.832 microseconds. That allocation behavior
differs from libc++ on Mac, so Mac-only conclusions cannot substitute for Linux.
The raw samples retain insertion CPU/I/O and `ru_maxrss`. The latter includes
an inherited launcher floor and cannot establish a process-RSS improvement in
these small cohorts; see the measurement control below. These short runs do not
establish natural throughput or whole-node capacity.

Current Linux all targets/version/help, the two focused repository/real
transport checks (8.03 s) and the exhaustive **60/60 native suite pass**.
The exhaustive 58/1/1 batches take 129.71 / 1.60 / 11.92 seconds, 143.23 seconds
in total. Native executable SHA-256 is
`0a7e24f9fdbdee65b88d49b8748df8ce782e188cf83dd18759743b8c13891592`.
Each completed batch's fixture inventory is streamed to My Passport and every
archive member/hash is verified before retiring only the duplicate owned
builder fixture directory. The three archives contain 847,752,645 compressed
bytes and preserve all 13,878 inventoried entries. Disk and memory floors
remain unchanged. Existing pinned dependencies were read-only
inputs; the complete dependency workflow was not rebuilt here.

### Current scoped diagnostics and target-host checks

Separate Linux ASan, UBSan and TSan runs each pass twelve current-source fixtures,
including the actual repository, real transport lifecycle, native configuration,
sync/peer bookkeeping and RPC/codec helpers. Clean and known-fault controls pass
for each runtime. The 52-step ledger also verifies the four TSan mutex-lifetime
controls (plain reuse / annotated reuse / real cycle / destroy-held: 66 / 0 /
66 / 66) and compiler dependencies for the hash-pinned diagnostic-only DI overlay.
Ordinary mutex locking remains unchanged; no detector suppression or synthetic
happens-before edge is added. Prebuilt SDK/dependency instrumentation and complete
state/mempool/VM/RPC lifetime qualification remain additional D1 requirements.

The designated replacement VPS passes fifteen normal-ASLR version/help and
synthetic component commands, including the repository, real transport and
controlled-chain producer fixture. Tests use only their new owned directory;
the original server is not contacted. Sampled host headroom/reserve checks pass.
These are separate child fixtures, not a restored observer, sustained producer
or complete 4-GB/40-GB workload.

An offline, read-only, non-root runtime container passes version/help and binary
hash checks using the retained pinned runtime base. Image identity is
`sha256:3d4bafd8ec8524f59e0491b882e25d38bf632848e9e18fdb62f78378b3ad1e46`.
Its base runtime configuration is preserved. This is not a repository Dockerfile/
dependency rebuild or a deployment. The installed backup publisher remains a
separate maintenance artifact.

### RSS launcher control

The baseline fixture's repeated 87,416,832-byte `wait4` peak, and the Linux
microbenchmark's roughly 19-MB `ru_maxrss` floor, include pre-exec launcher
memory. A controlled larger-host experiment keeps the exact benchmark binary
and input unchanged while the Python launcher holds 128 MiB. Direct launch
reports 151,965,696 bytes; a thin `/usr/bin/time` child reports 4,415,488 bytes
inside the benchmark and 4,739,072 bytes at exit. Attributed retained C++ bytes
remain 66,706 in both cases. Thus those original peak fields cannot qualify
the node or substantiate an RSS reduction. Future whole-node measurements must
use a controlled thin launcher and/or the running process's actual memory maps,
with host memory sampled concurrently. Request-byte/CPU/I/O results are kept
separate from this corrected interpretation of the peak fields.

Five new alternating thin-launcher samples per cohort keep the original
benchmark executables and attributed request bytes unchanged. Their complete
instrumented-process exit peaks are:

| Inputs | Original SDK median RSS bytes (range) | Current median RSS bytes (range) |
| --- | --- | --- |
| 256 | 4,886,528 (4,747,264–4,919,296) | 4,734,976 (4,550,656–4,866,048) |
| 1,024 | 5,632,000 (5,509,120–5,652,480) | 5,259,264 (5,246,976–5,267,456) |
| 4,096 | 8,523,776 (8,519,680–8,601,600) | 5,779,456 (5,771,264–5,783,552) |

The 256-input ranges overlap. The 1,024-input cohort retains equal coverage;
the 4,096-input cohort includes eviction to 1,024 current peers. All these
peaks include program startup, prepared inputs and the allocation tracker's
headers. They describe this mechanism program, not production Teleno RSS or a
proportional whole-node saving. Thin-launcher raw CPU/I/O/timing and peak samples
are retained in `thin-benchmarks.json` alongside the earlier raw measurements.

### Preservation and documentation

The complete builder checkpoint is externally archived with all 231 inventory
members verified (155,704,586 compressed bytes; SHA-256
`988572616f64f2fa96953646d197b5dc0f319bd4bb53be02ac1598cd20792d15`).
The baseline component checkpoint is likewise verified for all 38 members
(68,334,936 bytes; SHA-256
`f52820c18d2e9a0012e9840c7e68231ff15f106d29199e4570b69b15e8ea1e61`).
Original source, pinned dependencies, build caches, installed operational data
and unique evidence are preserved. Native operator documentation, the goal/
matrix and Koinos One developer handoff distinguish this uncommitted source from
the packaged app; no submodule/binary integration is performed. Strict MkDocs,
rendered native identities and scoped file preservation are checked separately.

## Required verification

- Original add/upsert fault control, exact retry visibility and allocation-free
  publication; capacity/slot/wire/count boundaries and failed preparation.
- TTL refresh versus add, permanent/expired/overflow handling, clear/GC/order,
  duplicate hits, protected roots/connections and local bypass under saturation.
- Cross-generation shared accounting, actual storage destruction, callback
  exceptions/reentrant reads/mutation and bounded staging.
- Real TCP/Noise native SDK binding and collector start/stop/restart,
  continuing RPC and existing native compatibility regressions.
- Five paired actual old/current SDK-repository measurements per equivalent
  and churn workload, with Linux CPU/I/O/RSS tradeoffs and retention differences.
- Current native, separate supported diagnostic/control, VPS/container and
  documentation/source-preservation gates; prior P2-F passes identify older code.

SDK key/protocol/DNS/implicit work, queues, decoded and caller aggregates,
other M1/R1/V1/S1 consumers, full D1/B1/Q1/H1 and actual target-host producer /
40-GB full-history restore remain required. This package does not waive them.
