# LR2 P2-E: explicit native dial admission and ownership

Date: 2026-10-16. Status: **implemented locally; final native and scoped
sanitizer/VPS/container checks pass. P2 and issue #9 remain open.**

Uncommitted work on `codex/lr2-memory-hardening`, based on
`d3cfb94455da4a472f651eb4239abbc289e74225`. This checkpoint covers explicit native
`connect_peer` calls and startup seed dials. It does not deploy the mixed LR2
binary on the public-backup writer, change production authority, prune history
or qualify the mandatory full-history producer/40-GB outcome.

## Reproduced defect and reviewed correction

Old Mac and Linux executables reproduce a one-shot allocation failure leaving
a peer in `_connecting`; an independent retry silently returns instead of
attempting/validating it. The preserved regression exits 1. The marker was
published before fallible known-peer assignment, address parsing, captures and
queue publication, without a rollback covering all those steps.

Native pending attempts now use scoped, immutable peer owners and a generation
registry. Failures in native preparation, reservation/registry allocation,
validation and queue publication retire the matching registry entry. Credits
return when the last real owner dies; a cancelled-but-retained callback is not
reported as freed memory. A separate actual transport regression measures the
caller's allocation sites for a valid dial, faults every site through Boost
queue publication and checks unchanged pending credits before a successful
independent retry.

Review also reproduced an intermediate R0 defect: retiring a foreign lease with
the same numerical ticket cancelled another registry's lease. The preserved
regression exits 1. Retirement now validates both ticket and owner identity
**before** touching the cancellation flag. The new case is part of the normal
unit test. R0's earlier 57/57 Mac and paired samples remain preserved under
`review-r0/`; they do not substitute for final R2 qualification.

R1 review also reproduced a startup-route omission for SDK-supported `/ipfs/`
seed spelling. The SDK's independent GossipSub bootstrap could still establish
a connection, hiding the missing explicit native admission (observed peak: 0).
R2 obtains the seed identity through the pinned SDK parser and publishes native
seed attempts before releasing the I/O runner. Both `/p2p/` and `/ipfs/` tests
require native admission, successful RPC and a single connection notification.

## Effective component policy

Concrete `Libp2pTransport::Config` has positive internal defaults:

| C++ field | Default | Meaning |
| --- | --- | --- |
| `max_pending_dials` | 32 | Native explicit dial leases across preparation, queues and retained SDK callbacks, including retired generations |
| `pending_dial_peer_capacity_bytes` | 1,048,576 | Charged peer ID/address string capacity for completed native lease construction, plus provisional copy reservations |

Zero is rejected. There is no new YAML, CLI, RPC or GUI control and no qualified
4-GB profile selected here. The ledger lives across transport restart generations.
A borrowed-size/small-string reservation precedes owned copies; actual string
capacities are checked before registry publication. Copy-construction workspace,
allocator rounding/retention and physical allocation peaks remain additional.
This is a retained/provisional accounting axis, not a strict heap/RSS ceiling.
Map/control blocks, parsed peer/multiaddress data, resolver results, SDK storage,
caller/configuration input and other consumers are also outside these bytes.

Admission counts constructing, resolving, queued and callback-held leases. It
refuses locally before creating another owned peer when its budget is occupied.
A duplicate peer in the same live registry reuses the existing attempt; it is
not an RPC replay. Cancel/stop removes registry eligibility while real retained
owners keep their credits. A new generation cannot evade occupied old credits.
Local capacity/lifecycle/allocation errors do not establish remote misconduct
or change block/transaction validity, bytes, execution, roots or history.

## Queue, lifecycle and metadata behavior

Dial queue publication and stop admission share a mutex. A successful publisher
queues work before stop's drain barrier; otherwise it refuses locally. Control
posts for explicit disconnect share that lifecycle barrier, but have **no dial
count/byte quota**. Control/other queue admission remains a separate gap.

SDK callbacks retain the lease and a weak generation context. They do not
create a host → callback → context → host ownership cycle. Retired contexts are
checked before any transport-owner access; retired results are closed without
using the old owner. Matching-ticket retirement prevents old completion from
removing a fresh registry entry. Public lifecycle calls still require their
external serialized owner. This is not arbitrary concurrent destruction safety.

Startup seeds now share explicit native dial admission. Their trusted-config
syntax/identity prepass is outside the peer-copy ledger. Duplicate seed advice
and the SDK's host event/completion paths notify one actual connection once.
Native connected metadata uses strong map insertion or prepared replacement;
optional known-peer advice uses prepared publication and may be bypassed on
allocation failure. An advice-retention failure does not suppress an already
completed connection notification. Native known-peer count/bytes are still
unbounded. Dial identity must match the embedded multiaddress peer ID; a local
inconsistent target is refused before queueing.

Startup exceptions attempt ordinary cleanup before propagating; this does not
prove every third-party constructor failure, global OOM or a bounded stop time.
Synchronous DNS is not cancelled. The pinned SDK's connect interface supplies
no per-attempt cancellation handle here. A cancelled dial's eventual connection
in a still-active generation remains SDK-managed; host connection events can
still observe it. Full pending-SDK cancellation/watchdogs remain open.

Implicit SDK dials from GossipSub bootstrap/mesh, outgoing Peer RPC `newStream`, DHT/discovery, inbound
connections, SDK repositories/snapshots, parsed/decoded buffers, caller owners
and complete lifecycle concurrency are **outside this explicit-dial ledger**.
They must not be counted as covered by its 32-lease policy.

## Diagnostics

The concrete native C++ `dial_stats()` snapshot reports jobs, charged peer
capacity, job/capacity high waters and maxima, capacity refusals, outgoing setup/
completion exception counts and optional native advice-retention bypasses.
Counters span restarts; jobs/bytes include retained retired owners and provisional
copy reservations. They do not report active peer count, complete transport
memory, allocator usage or RSS. This adds no diagnostic endpoint or GUI metric;
other transport implementations do not acquire this typed snapshot.

## Validation identity and current evidence

Owned run: `/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/`;
checkpoint evidence: `evidence/p2e/final/`, with superseded inputs retained under
`evidence/p2e/`. Source and committed English documentation
remain in the repository. The volume guard checks the selected mount before
development writes. Linux builds use isolated owned source/data, one low-priority
job and unchanged 10-GiB free-disk / 2-GiB available-RAM builder floors. Existing
services, key material and operational databases are not test fixtures.

Final reviewed export: 293 native/build/test/config inputs; manifest SHA-256
`947958d848be5906923b8f968d6d19e01a601f8eb3e7b70bce99a6ed16d31275`.
Source archive SHA-256
`6921b307e1b7d992696a5960fd8bdedf10f4d9847b285a4add328186e73b9ffb`.
The export retains pre-completion guidance. Only `AGENTS.md`/`CLAUDE.md` later
change to record these results; final preservation checks require the other
291 input files to match exactly and the two current guides to be identical.
The previous input is P2-D manifest
`10b59877afabe323399ca5a8803a33256ea5f6208803a87640de267f9878027a`.
Its old transport library, standalone fault/measurement executable, harness,
compiler/link/cache records and failed regression are independently archived
and member-verified before replacing the isolated Linux source mirror.

The final R2 Mac all-target build, **57/57 CTest in 153.42 seconds**, version and
help pass. Its native binary SHA-256 is
`c7e136b875144b1da4332b6c3fdf89884d57e100d26c2b6f6b33bf1b2a003226`.
The suite includes peer sync, dial-budget and actual transport. Tests include
count/exact/oversize and actual
capacity growth, allocation rollback, held/retired/foreign owner identities,
stop/publication ordering, 8-thread admission, real valid-dial allocation faults,
duplicate seeds, positive configuration, complete request/response/cancellation/
restart and single connection notification. Platform-dependent small-string
capacity growth is tested when the implementation actually rounds up capacity.

R2 Linux all targets, version/help and **57/57 native tests** pass in exhaustive
55/1/1 batches (130.54 / 1.47 / 9.59 s; 141.60 s summed CTest time).
This is not one full Linux CTest invocation. The normal Linux binary SHA-256 is
`1a3975ba7889936efd0f38e084c8feef250b81f1cce0026ab51f2ab6b26bda47`;
version is `teleno_node 1.3.0-dev.0+d3cfb94455da-dirty`.
Three complete fixture inventories/archives are externally member/hash-verified
before only their completed owned test roots are retired; the builder's original
10-GiB disk and 2-GiB available-memory floors remain unchanged. The first TSan
build was stopped by the disk floor, before fixture execution. Its failed row,
full log and original validation snapshot remain preserved. Only that build was
resumed using a fresh owned child of the miner's existing `/dev/shm` for compiler
temporaries, with the same flags/source, one job and both original floors. No
mount, swap or global host setting changed. Controls and earlier passing phases
were retained; the nine current TSan fixtures ran after the recovered build.

Each separate ASan, UBSan and TSan configuration passes both the clean/known-fault
controls and all nine current fixtures: dial budget, peer errors, node peer sync,
configuration, RPC client, request read, frame write, actual transport and codec.
The known-fault controls fail with the expected detector output and exit; they
are not successful clean programs. TSan requires child-only `setarch -R`; no
machine-wide setting changes. The pinned prebuilt cpp-libp2p/other SDK libraries
remain uninstrumented. These scoped native passes do not close full D1 or prove
arbitrary lifecycle concurrency, global OOM or the whole node race-free.

Twelve normal-ASLR CLI/component commands pass on the actual 2-CPU/4-GB-class
Ubuntu/x86_64/ext4, no-swap replacement VPS, including producer mocks. This is
not a restored mainnet observer or sustained producer workload. The matching
non-root, read-only, network-isolated runtime container passes version/help and
binary hash checks; image ID:
`sha256:eb7dc4f4f0379e22d1885f15a60e3053882c2e065164e05d7d2f009f756afea9`.
It uses a retained pinned
runtime base, rather than a fresh repository Dockerfile/dependency rebuild.

The complete final builder raw archive has 448 verified members:
`6acb9fbe545a2f7e0f12118bc46c8c151c9a417bb6bcc7b4f77cbbc4cdaefb24`.
The component bundle and complete VPS evidence are independently archived and
verified before only unused transferred VPS binary/input duplicates are retired.
Strict Koinos One documentation, rendered manifest/binary identities and 192
relative file references pass. Scope checks preserve 364 other Teleno files,
578 other Koinos One files and both repositories' HEAD/branch/index. The two
guides remain byte-identical. The installed maintenance publisher is active;
its configuration and all 14 existing containers are unchanged. No mixed LR2
binary was deployed. Only 40 externally verified, unused builder copies were
retired; original source/builds/caches/logs/images and operational data remain.

### Intermediate TSan report and controlled diagnosis

R1 Linux 57/57 native tests and separate ASan/UBSan nine-fixture checks passed.
TSan stopped at the actual transport fixture with a lock-order report; its last
codec fixture and all R1 VPS/container checks were not executed. The preserved
report's cycle contains reused main-thread stack mutexes in Boost.DI's shared
scope, rather than native dial-ledger locks. GCC's trivial `std::mutex`
destruction leaves these stack addresses visible to the detector across separate
fixture injector lifetimes.

A reduced single-thread control reproduces the report for independent stack
mutex lifetimes (exit 66). Equivalent heap-owned lifetimes pass (exit 0), while
inverse ordering of the same live mutexes still fails (exit 66). Changing only
the fixture injector to heap ownership also passes against the unchanged R1
instrumented native runtime, with the lock detector active. R2 adopts that
fixture lifetime, matching the existing heap-owned native runtime. No detector
suppression, SDK patch or host-wide ASLR change was added. The original report,
three controls and fixture-only experiment remain in the verified R1 archive:
`3d064167a96345c9266365a90cb3b1f06e567811f1c3f9def5ce4dcef56848de`
(149 members). This diagnosis does not establish integrated race freedom.

P2-F later reproduced the same temporary shared-scope mutex identity problem
with both fixture and runtime injectors on the heap. Heap ownership passed the
recorded R1 experiment above but also changes temporary stack layout; it does
not describe all temporary mutex lifetimes to TSan. The
[peer snapshot follow-up](low-resource-lr2-peer-snapshots.md) identifies those
individual lifetimes and validates a hash-pinned diagnostic-only overlay with
live-cycle and destroy-held negative controls. The earlier reports and passing
runs remain preserved; they do not qualify that later source.

## Measurement contract

Five alternating old/current process pairs use identical 512 valid loopback
peer targets while an actual production transport runner is held in a controlled
RPC callback. Both versions compile the same benchmark source with heap-owned
fixture injectors and identical gate cleanup. Source remains the actual old/current transport, not a reimplemented
model. Old code admits 512 calls; the current component admits 32 and refuses
480 locally. This deliberate policy difference must accompany any memory claim.

Ordinary C++ allocations requested by the caller are attributed with exact
requested sizes; tracking prefixes, aligned/direct allocations, allocator
metadata/retention and SDK-thread allocations are additional. Optional native
advice publication now occurs on the runner, so caller attribution does not
represent its later total footprint. Residual tracked requests after fixture
destruction are a separate observation, not by themselves proof of an
unreachable heap leak. Per-child CPU/I/O/RSS includes startup, input generation,
SDK peers, cleanup and diagnostic output; it is not node throughput or a VPS
working-set measurement. The final Linux medians are recorded below.

Final Mac medians (five alternating fresh-process pairs): retained ordinary
caller allocation bytes 410,112 → 18,976; peak 410,693 → 19,189; total requested
1,584,640 → 78,400; allocation attempts 40,448 → 2,176. Caller-loop elapsed time
is 2.808 → 3.418 ms. Tracked bytes after fixture destruction are 55,296 → 0.
These are intentional admission-policy tradeoffs, not whole-node RSS savings.

Final Linux medians from the same common harness (five alternating fresh-process
pairs after one warm-up per version), linked to the preserved actual P2-D native
library and the final R2 native library with identical pinned SDK dependencies:

| Metric | Old actual runtime | Final R2 |
| --- | ---: | ---: |
| Retained caller C++ bytes | 428,544 | 19,456 |
| Peak caller C++ bytes | 429,146 | 19,761 |
| Total caller C++ requested bytes | 1,743,872 | 87,232 |
| Caller allocation attempts | 41,472 | 2,240 |
| Caller loop ms | 12.84301 | 6.544204 |
| Tracked bytes after fixture destruction | 51,200 | 0 |
| Whole-child user CPU seconds | 0.053506 | 0.040366 |
| Whole-child system CPU seconds | 0.034044 | 0.007358 |
| Whole-child peak RSS bytes | 18,989,056 | 18,989,056 |
| Whole-child input blocks | 0 | 0 |
| Whole-child output blocks | 8 | 8 |

Caller-loop ranges: before: 12.453–13.387 ms; after: 2.961–7.714 ms. Host scheduling and local exception
cost affect these short loops. Lower native admitted work is intentional; none
of these measurements estimates catch-up, sustained node RSS or production CPU.
The source, compile/link commands, executable/library hashes, all ten samples
and complete child CPU/I/O records are preserved.

## Next P2 retention reproduction

An independent Mac API diagnostic against the final native library performs
256 and 1,024 sequential failed loopback dials, waiting for every native credit
to return, then stops the transport before querying peers. It retains exactly
256 and 1,024 native known peers with zero pending dial jobs. Returned snapshot
string capacities are 30,208 and 120,832 bytes; these are not retained-map or
heap totals. This confirms count growth independent of pending dial ownership
and does not reproduce the separate suspected concurrent SDK snapshot race.
`next-p2-probe/results.json` preserves the actual source/library hashes, commands
and observations. Optional native advice and SDK snapshot ownership require
further implementation; this checkpoint does not fix that growth.

## Remaining goal gates

P2 remains open for implicit GossipSub/RPC/DHT/inbound work, native/SDK known peers and
snapshots, all control/transport/decoded/caller queues and bytes, pending SDK
cancellation/watchdogs and integrated lifetime qualification. Full D1 state/
mempool/VM/RPC/SDK diagnostics, B1 dependency/release/full container workflow,
H1 historical differential and Q1 sustained actual-host operation remain open.

The mandatory baseline remains the user-selected replacement: Linux/ext4,
2 CPUs, 4 decimal GB RAM, 40-GB-class whole-server disk, no swap. The
[official cost-optimized catalog](https://www.hetzner.com/cloud/cost-optimized/)
checked on 2026-10-16 still lists CX23 as 2 vCPUs / 4 GB / 40 GB and unavailable;
no current price was returned or inferred. Catalog availability does not replace
or enlarge the engineering target.

The [last full-backup capacity assessment](restore-40gb-current-baseline.md)
retains the 4,140,425,216-byte deficit at the unchanged reserve. No full-size
restore/recovery or sustained mainnet producer is qualified by these fixtures.
The separately maintained observer publisher remains on its isolated installed
build; mixed LR2 deployment, pruning, producer activation and source-trust changes
are not performed.
