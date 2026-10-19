# LR2 P2-F: peer advice and owned native projections

Date: 2026-10-16. Status: **implemented locally; final native and scoped
diagnostic/VPS/container checks pass. P2 and issue #9 remain open.**

Uncommitted work on `codex/lr2-memory-hardening`, based on
`d3cfb94455da4a472f651eb4239abbc289e74225`. This changes native metadata retention,
query ownership and read-only discovery. Protocol validity, frames, execution,
roots, receipts, complete history and producer activation are unchanged.

## Reproduced failures

The preceding [P2-E](low-resource-lr2-p2p-dials.md) diagnostic retains 256 and
1,024 known peers after failed loopback dials, with zero pending dial credits.
The native advice map had no count/capacity policy. Querying it copied the map,
including duplicate ID strings, before copying the result again into a vector.
The known-peer query also merged complete SDK peer/address snapshots.

A new standalone program queries known/connected peers and the connection count
while the external lifecycle owner performs serialized stop/start cycles. The
actual preceding instrumented native library reports a **TSan data race, exit
66**: shutdown resets `_host` while the query reads that same shared-pointer
object. Both conflicting accesses are in instrumented native code. This is a
different finding from P2-E's diagnosed fixture stack-mutex report.

The old executable, exact source/compiler/link records, detector report and
instrumented native library are externally member/hash-verified in
`evidence/p2f/before-raw.tar.gz`. SDK internals remain uninstrumented; this proof
does not establish every possible repository race.

## Disposable advice policy

Internal concrete `Libp2pTransport::Config` defaults are:

| Field | Default | Scope |
| --- | ---: | --- |
| `max_known_peer_advice` | 1,024 | Retained optional advice records |
| `known_peer_advice_capacity_bytes` | 1,048,576 | Retained peer ID/address string capacities |
| `max_peer_snapshots` | 4 | Simultaneous constructing/caller-held native projections |
| `peer_snapshot_entries` | 4,096 | Aggregate reserved/live vector slots across projections |
| `peer_snapshot_capacity_bytes` | 4,194,304 | Aggregate attributed vector storage and peer string capacity |

All must be positive. These are internal C++ component policies, with no new
YAML/CLI/GUI setting or qualified 4-GB profile.

An advice entry owns its ID/address once, in a stable list node; its sorted map
index borrows that node's ID. Refresh moves an existing entry to the back.
Admission evicts the oldest unprotected advice when necessary. Already-retained
startup seeds, DHT bootstrap roots and current connections are protected; closing connections releases
their active protection. If protected records occupy capacity, further advice
is bypassed locally. Configured seeds/DHT bootstrap addresses and active metadata
remain owned by their configuration, SDK or native connected state even when an
optional cache insertion is refused. This cache is not a ban/security table.

Borrowed size/count checks precede copies; actual capacities are checked before
publication. A staged list/index insertion must complete before any old advice
is evicted. Allocation refusal leaves old advice/index/capacity intact. Updating
connection protection is independent of a failed optional address replacement.
Repeated unchanged advice needs no new string/node allocation.

The retained string policy excludes one serialized staged publication, conversion
workspace, node/index/allocator overhead, configuration, active metadata, SDK
repositories and caller copies. One staged peer is additional; it is not a strict
physical allocation/RSS ceiling. Meaningful error scores and canonical data are
never discarded to satisfy this policy.

## Native views and ownership

SDK address notifications populate advice on their existing mutation path. The
subscription has explicit runtime ownership, checks its generation, contains
local failures and disconnects before host shutdown. It does not reenter SDK
repositories. A conservative borrowed-length check precedes address/ID conversion.

Known/connected queries now read only the locked native metadata. They never
inspect `_host`, copy SDK peer/address sets or allocate an intermediate map.
Known views merge sorted advice/current connections directly, deduplicating IDs
and preferring current connection addresses. The scalar connected count reads
native published metadata without constructing a snapshot or allocating.
These views describe native observations, rather than a raw SDK connection or
repository inventory.

`ITransport`/`P2PNode` now return move-only `PeerSnapshot`. Existing native C++
consumers must rebuild; this is not a wire-format or Koinos One GUI API change.
The dormant Go bridge keeps its empty implementation with the matching return
type; no legacy build/start surface is revived.

A shared per-transport ledger reserves projection jobs, slots and attributed
capacity before copying. Actual vector/string capacities are checked before
return. Failed allocation or final-capacity admission destroys prepared storage
and returns the reservation. Returned storage is read-only and its admission
owner follows moves until destruction; storage dies before credits return.
Restart cannot evade credits held by earlier caller snapshots. A held result
can outlive its original budget owner. Empty projections allocate no reservation.

Charges include vector capacity times `sizeof(PeerID)` plus string capacities,
including inline capacity. This conservative attribution can count inline
storage twice; allocator/control overhead, construction rounding/workspace and
physical peaks remain additional. Copies made by discovery candidates, protected
admin formatting/serialization or other callers require separate budgets.
Other transports/test doubles may supply their own unbudgeted projection.

Saturated projection admission raises a typed local capacity error, without
truncating a successful result or scoring a peer. The periodic worker records
`discovery-snapshot-refusals`, then waits its ordinary interval; the next cycle
can progress. Protocol RPC remains usable with old-generation projections held.
Protected admin peer queries use the existing error response if their provider
cannot obtain a projection; they do not publish a partial successful inventory.

## Source and current verification

Owned run: `/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/`;
final evidence: `evidence/p2f/final/`. Source stays in the canonical repository.
Selected-volume guards, explicit artifact/build/temporary roots and disabled
Python bytecode writes remain in force. Linux builds use one low-priority job
and unchanged 10-GiB free-disk / 2-GiB available-memory floors. Compiler
temporaries use a fresh owned child of the existing tmpfs; fixtures remain on
builder ext4. No mount, swap or machine-wide setting changes.

Frozen R1 manifest (301 inputs):
`e100ef823bc7843f26f48a893a36893f4c7768119fab7991c11263769cc818d2`.
Source archive: `5108157c97739b90490b387f3f0f985d0ed8d382563b8c52d663ca2c7448f4f3`.
The preceding P2-E input remains
`947958d848be5906923b8f968d6d19e01a601f8eb3e7b70bce99a6ed16d31275`.
Both guides agree. Final documentation changes are recorded separately; native
source hashes must still match the freeze.

Mac all targets, **59/59 CTest (168.53 s)** and version/help pass. Native SHA-256:
`5169c98e9d8929880b428b3e2c41115169776eb1b72d70d0c55ae4ada1ca0605`.
Linux all targets, **59/59 native tests** in exhaustive 57/1/1 batches pass
(124.64 / 1.53 / 7.25 s; 133.42 s summed CTest time), plus version/help.
This is not one complete Linux CTest invocation. Native Linux SHA-256:
`4cd08b91a22dfa4e30f6f12d309419236e4fbbf7bf285ef28ece455cf57e6eb2`.
Version: `teleno_node 1.3.0-dev.0+d3cfb94455da-dirty`. Three full fixture archives are externally
member/hash-verified before completed owned fixture roots are retired.

The actual tests cover protected churn/refill, retained capacities, borrowed
index lifetime, staged failure atomicity, allocation-free duplicate/refusal
paths, snapshot moves/late owners/last release, construction allocation sites
and concurrent reservations. Real TCP/Noise tests require SDK address signals,
startup-seed/DHT-root retention during churn, old caller credits across restart,
usable RPC with old credits held, and concurrent metadata reads over eight
serialized stop/start cycles. Discovery progresses after a local refusal with
no peer score/disconnect.

Each separate ASan/UBSan/TSan configuration passes clean/known-fault controls and
all eleven fixtures: dial budget, advice store, snapshot ledger, peer errors,
node sync, config, client/read/write/actual transport and codec. Known-fault
controls fail with their expected report/exit; they are not clean test passes.
TSan applies `setarch -R` only to diagnostic children. Normal native/VPS tests
retain ASLR. Prebuilt SDK libraries remain uninstrumented and unsupported atomic
fences remain a detector limitation; these scoped passes do not close full D1.
The identical standalone host-pointer race proof exits 66 against the preceding
instrumented library and 0 against the final one. A corrected R0 library also
passes that proof without the scope overlay, separating the actual race fix
from the diagnostic lifetime correction below.

Fourteen normal-ASLR CLI/synthetic component commands pass on the actual
2-CPU/4-GB-class Ubuntu/x86_64/ext4, no-swap VPS, including producer mocks.
This is not a restored mainnet node, sustained producer or whole-machine load.
The matching non-root, read-only, network-isolated runtime container passes
version/help and exact native hash checks. Image ID: `sha256:79805c2443ce9da882ffd2a87915b42cdf3a2dbbc7a5746ba89108af275863ad`.
It reuses a pinned retained runtime base; it is not a fresh repository
Dockerfile/dependency rebuild or deployment.

The final raw builder archive contains 504 verified members:
`69476ab420eaad8baeb8c4897ac4cf3734bd2e8230d08c20c4d1809874c87d2a`.
Normal/instrumented native libraries, source, compile/cache/dependency records,
executables, control failures, samples and logs are preserved. A private runner's
validation-path shadow and action-prefix ambiguity were corrected at an idle
boundary. Its original partial and recovered execution ledgers, failed dispatch
and driver revisions are preserved; existing native/control rows are unchanged.
The standalone proof ran only after that review. The transferred
component bundle and full VPS evidence are separately member/hash-verified.
Final strict Koinos One manual build, rendered source/binary identities and
208 relative file references pass. Scoped comparison preserves 359
other native-repository files, 578 other Koinos One files and both repositories'
HEAD/branch/index. Both guides remain identical; the other 299 frozen inputs
are unchanged. The installed maintenance publisher is active with its verified
image; configuration and all 14 existing containers are unchanged. No mixed
LR2 binary is deployed. Only 49 externally verified, unused copied builder outputs
(338,713,141 logical bytes) and the empty owned compiler temporary directory
are retired. Original source/builds/caches/logs/images and operational data remain.

### Intermediate report and temporary mutex lifetime diagnosis

R0 manifest `ce0ad6a01a151c7c836b4b115399422e2f2ad219c450effdc779a0fc0355209c`
passed Mac/Linux native 59-test and eleven-fixture ASan/UBSan checks. TSan passed
its first nine fixtures, then reported lock-order inversion in the transport
fixture; the last codec fixture and subsequent gates were not run for R0.
The preserved cycle contains temporary Boost.DI `shared`-scope mutexes at reused
main-thread stack addresses, with runtime and fixture injectors already heap
owned. Relocating an injector can change layout without repairing those
individual lifetimes. R0's full verified archive has 448 members:
`43cd2343fa8fd6f63864f4c98dbd739eb15af86e95bdd82a5f31e93490d3ba24`.
The actual host-pointer data race and this lifetime report remain distinct.

The pinned libstdc++ mutex has a trivial destructor, so it supplies no pthread
mutex-destroy event for these ended C++ objects. Reused addresses retain stale
TSan graph identities. A reduced control reproduces that report for independent
plain mutex lifetimes. `ScopeMutex` describes only real constructor/destructor
lifetimes through the public TSan mutex API when `std::mutex` destruction is
trivial. It retains ordinary intercepted `std::mutex` locking, size/alignment,
member destruction order and live-lock detection. It adds no synthetic acquire/
release edges, suppression, ignored region or disabled deadlock detection.

A CMake-generated diagnostic-only Boost.DI header overlay applies only to TSan
native template instantiations. The original dependency header is read-only
and hash-pinned to
`06a42b443b39112094772ec4176a689ec11051e3beba0f5fbfc85daaa71c9a68`;
missing/unknown inputs fail closed. Positive and wrong-hash recipe controls pass.
Actual native transport/test compiler include order and dependency files verify
the generated header/helper, with unchanged original hash. Normal, ASan and
UBSan builds do not use the overlay. The SDK binaries are not retroactively
instrumented. Provenance and generated bytes are preserved.

The actual source control executable produces:

| Mode | Required result |
| --- | --- |
| Independent plain mutex lifetimes at reused addresses | 66; lock-order inversion reproduces |
| The same lifetimes with explicit annotations | 0; completion marker |
| Inverted ordering of the same live annotated mutexes | 66; lock-order inversion detected |
| Destruction of an annotated locked mutex | 66; destroy-held detected |

All four results pass in the final configured TSan build, alongside the eleven
native fixtures and known-data-race detector control. This corrects the scoped
tooling diagnosis; it does not prove arbitrary SDK/native concurrency safe.

## Paired measurement contract

The same standalone C++ source links to the preserved actual old/current native
libraries with identical pinned SDK dependencies. Each input has a warm-up per
version and five alternating fresh-process pairs. Sequential failed loopback
dials wait for all native credits to return before stop and snapshot measurement.
With 256 inputs both versions return 256 rows. With 2,048 inputs, the old advice
retains 2,048 rows and current policy retains 1,024: that policy difference is
intentional and must accompany memory comparisons.

Tracking covers ordinary C++ `new` on all process threads after startup, including
SDK work. Immutable allocation tags follow frees after tracking stops. Prefixes,
aligned/direct allocation, allocator overhead and initialization before tracking
are excluded. Retained requests after stop include SDK/queued owners, rather than
an isolated advice-map heap total. Residual bytes after transport destruction
alone do not prove an unreachable leak. Child CPU/I/O/RSS includes startup,
diagnostic logs and controlled waits, not node throughput or whole-node RSS.

Final Mac medians for equivalent 256-row snapshots: allocation requests
112,640 → 43,064 bytes;
attempts 1,537 → 514;
snapshot time 250.333 → 113.333 microseconds.
Retained tracked requests after stop are 70,248 →
64,104 bytes. These are component observations,
not a qualified VPS profile.

Final Linux medians use five alternating pairs per input, with one warm-up per
version/input. The exact common harness, old/current executable and input
library hashes, all twenty samples and complete child CPU/I/O records remain
in the verified evidence. The two workloads have different retention semantics:

### Equivalent 256-row workload

| Metric | Old actual runtime | Final R1 |
| --- | ---: | ---: |
| Returned rows | 256 | 256 |
| Returned string capacity, bytes | 29,952 | 29,952 |
| Tracked C++ bytes after stop | 75,808 | 67,872 |
| Snapshot requested C++ bytes | 122,112 | 46,896 |
| Snapshot allocation attempts | 1,537 | 514 |
| Snapshot peak extra C++ bytes | 122,112 | 46,896 |
| Held snapshot extra C++ bytes | 46,848 | 46,896 |
| Snapshot microseconds | 390.791 | 127.294 |
| Dial/stop milliseconds | 306.199488 | 299.841897 |
| Whole-child user CPU seconds | 0.071614 | 0.067121 |
| Whole-child system CPU seconds | 0.07241 | 0.067121 |
| Whole-child peak RSS bytes | 18,685,952 | 18,685,952 |
| Whole-child input blocks | 0 | 0 |
| Whole-child output blocks | 104 | 104 |

### 2,048-input churn; intentionally 2,048 old / 1,024 new rows

| Metric | Old actual runtime | Final R1 |
| --- | ---: | ---: |
| Returned rows | 2,048 | 1,024 |
| Returned string capacity, bytes | 239,616 | 119,808 |
| Tracked C++ bytes after stop | 602,656 | 269,856 |
| Snapshot requested C++ bytes | 976,896 | 187,440 |
| Snapshot allocation attempts | 12,289 | 2,050 |
| Snapshot peak extra C++ bytes | 976,896 | 187,440 |
| Held snapshot extra C++ bytes | 374,784 | 187,440 |
| Snapshot microseconds | 5,008.858 | 524.17 |
| Dial/stop milliseconds | 2,307.707878 | 2,319.518729 |
| Whole-child user CPU seconds | 0.33798 | 0.405186 |
| Whole-child system CPU seconds | 0.328104 | 0.336241 |
| Whole-child peak RSS bytes | 19,972,096 | 19,972,096 |
| Whole-child input blocks | 0 | 0 |
| Whole-child output blocks | 808 | 808 |

Churn's whole-child median user CPU rises 0.337980 → 0.405186 seconds; median peak RSS is unchanged at 19,972,096 bytes. The scoped allocation/query savings do not establish an overall CPU or RSS improvement.

Host scheduling, startup/logs and controlled waits affect these short runs.
Lower retained advice at churn is intentional. Allocation attribution is not
physical heap/RSS, and whole-child RSS is not sustained whole-node evidence.

## Remaining goal gates

Native active metadata, SDK address/key/protocol stores, implicit GossipSub/
RPC/DHT/inbound work, control/publish/output queues, caller copies and complete
lifecycle/watchdog admission remain open. M1/R1/V1/S1 aggregate consumers, full
D1/B1/Q1/H1 and target-host producer/restore qualification remain required.

The [official catalog](https://www.hetzner.com/cloud/cost-optimized/) checked on
2026-10-16 still lists CX23 as 2 vCPUs / 4 GB / 40 GB and unavailable; no price
was returned or inferred. The replacement Linux/ext4 2-CPU/4-decimal-GB/40-GB-class
no-swap host remains mandatory. The prior full-backup reserve model still has a
4,140,425,216-byte deficit; no full-size restore or sustained producer is newly
qualified. The installed public-backup publisher remains its separate maintenance
build. No mixed LR2 deployment, pruning, signing, release or source-trust change.

Read-only public reference refresh on 2026-10-16 confirms the same selected
backup ID and exact manifest/inventory byte pins. The input to the last disk
model is unchanged; no full payload read, new restore or reserve reduction
is implied. The refresh is preserved in `public-backup-reference-refresh.json`.
