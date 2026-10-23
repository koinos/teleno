# LR2 M1-H: aggregate native memory reservations

Date: 2026-10-19. Status: **finite attempt unqualified; M1 NOT complete**.

This is the finite [M1 run](../implementation/issue-9-m1-aggregate-goal.md),
starting at `d424264b24595786fdba86d9eddbcbf1635284b6`. Its fixed deadline is
2026-10-19 16:30:38 UTC and it permits at most two frozen candidates. Owned
generated evidence is under
`/Volumes/My Passport/teleno-m1-aggregate-20261019-gaqjl_4n/`.
Development checks below do not qualify a later changed implementation.

The last planned native check completes at **13:06:47 UTC**, before the unchanged
16:30:38 UTC deadline. Both frozen candidate slots are consumed. Normal tests
pass 66/66, scoped TSan passes 12/12, and ASan/UBSan passes 11/12 with a required
query-fault leak gate failed. Read/output accounting and representative profile
selection also remain incomplete. Implementation stops at this run's fixed
limit; no third candidate, deadline extension, deployment or M1 completion is
implied. The original objective remains unmet.

## Reproduced gap

The unchanged Linux M1 implementation admits four independent 8-KiB-signature
transactions across the existing forks. After head expiry and normal LIB
retirement, the enabled pending-record policy reports **zero** owners, but the
indexed states still retain **16 tombstones**, **656 bytes** of tombstone string
capacity and a map entry. The record counter works as documented; it does not
account for aggregate metadata or establish an empty allocation set.

The separate original 64-record retention fixture reports 256 tombstones after
LIB. Its record budget is disabled, so its zero record counters are untracked
values, not evidence of record release. Preserve these two inputs separately.

## Ownership inventory

| Consumer | New ownership/reservation mechanism | Important limit |
| --- | --- | --- |
| Immutable serialized pending records | One allocation owner shared across forks and C++ aliases; reserve string workspace before encoding. | Record-owner/capacity policy remains a separate axis, not an additional heap total. |
| Map nodes and copied keys | Stateful allocator reserves its request, ownership header and declared key-string envelope before allocation. | String rounding is supplementary, not measured heap. |
| Tombstone nodes and buckets | Same allocator; copied keys get independent reservations. Node transfer retains the allocation's credit. | Empty sets may retain buckets until their owning state is freed. |
| Fork/mutex indexes | Allocator requests per physical container allocation; shared mutex objects have owned control blocks. | Fixed hash/ID metadata uses supplementary headroom. |
| State wrappers, implementations and control blocks | Lifetime leases and owned allocation headers; facade copies share counters. | Fixed bootstrap/control overhead is reserved before construction, separately from allocator requests. |
| Ancestor/external immutable-value owners | Allocation header owns the budget state beyond the original mempool/facade. | Credit can conservatively remain until a weak control block is physically freed. |
| Admission copies, accepted-message parsing, encoding and staging | Reserve supplementary workspace before the operation; shared domain with concurrent transaction jobs. | Estimates require Linux evidence; wire sizes are not heap bounds. |
| Anonymous standalone-execution map writes | New map/delta owners inherit the active transaction domain. | Existing canonical state/cache owners are not charged again as new allocations. |
| Cleanup arenas, fork work and hash preparation | Scope leases precede the arena/workspace and unwind after it. | Canonical/recovery work is accounted but exempt from standalone admission quotas. |
| Native decoded query vectors | A custom allocator retains the vector request and conservative protobuf parser/copy envelope until its storage is freed; vector copies reserve a new owner. | Native reads remain quota-exempt; extraction into other protobuf/RPC owners is a separate allocation/lifetime. |
| Other nonce/RC/count read helpers and generic database result vectors | Their existing ordinary string/vector/protobuf temporaries are **not completely reserved**. | An active scope routes owned allocators; it does not intercept arbitrary C++ allocations. |
| RPC queues, extracted protobufs and arbitrary execution/receipt ownership | **Not lifetime bounded by this implementation.** | Explicit M1/R1 ownership-integration gap. |
| VM, dependency malloc, allocator arenas, stacks and other services | **No physical-memory bound.** | V1, D1 and whole-node qualification remain separate. |

`accounted_bytes` combines allocator requests and supplementary reservations.
`allocator_requested_bytes` is the narrower sum of requests made by the owned
allocator, including its ownership header. Their difference is supplementary
policy headroom. Each owned allocation additionally reserves 64 bytes of allocator
metadata/rounding headroom, or 16 KiB for requests at least 64 KiB. These are
conservative policy allowances, not measured allocator usable sizes or a bound
on allocator-retained arenas. Shared physical allocations are charged once by their actual
owner; conservative envelopes can reserve more than the memory actually used.
Neither counter is allocator usable size, retained glibc arenas, heap or RSS.
The counter/mutex/control-block bootstrap allowance is a subset of
supplementary reservations and remains charged while its shared state lives.

The native C++ query return alias is `pending_transaction_vector`, whose
allocator carries ownership. The aggregate-budget constructor parameter and
allocator-bearing return types change the native C++ API/ABI; consumers must
rebuild against matching headers and libraries. RPC/P2P protobuf encodings and
consensus behavior are preserved. The complete native build checks internal
consumers; compatibility of separately precompiled C++ consumers is not claimed.

The optional `mempool.aggregate-max-accounted-bytes` defaults to zero/untracked.
An enabled policy governs standalone admission and concurrent native transaction
work. Refusal has reason `mempool_memory_capacity`, retaining retryable local
JSON-RPC/gRPC/P2P semantics. No eviction, automatic retry or consensus rule is
introduced. Instrumented allocations in canonical block/LIB work, native reads
and cleanup can exceed the admission threshold; metrics expose their accounted
consumption. Other read-helper workspace remains additional. **This is not a hard ceiling on
all mempool allocations or the process.**

Canonical chain execution clears the standalone domain; its separate state/VM
working set is outside M1-H. Mempool block/LIB callbacks use the protected
accounting scope. A protected scope does not make ordinary allocations tracked.

## Failure preparation

Block preparation snapshots physical fork-index metadata and publishes only new
cloned delta payloads. Failed preparation restores metadata by swapping already
prepared containers; rollback does not allocate. Accepted records and the
previous head/pending view remain usable for explicit retry.

In-memory LIB preparation clones the root map, shares immutable values, prepares
the complete retirement plan and commits a replacement root off to the side.
It avoids a throwing modifier inside a live Boost multi-index, which can erase
its indexed element on failure. After preparation, pointer/index publication
and retirement do not allocate. Existing C++ holders retain old immutable
payloads and credits. The same prepared index-publication path now surrounds
persistent root commits: a write failure retains the original root/index and
keeps the backend explicitly failed until close/reopen and validation. It never
reinterprets an uncertain durable write as success. A dedicated fault/reopen/retry
regression supplements the existing persistent tests. Database formats, history
and receipts are unchanged.

## Development evidence and outstanding qualification

The second and final runtime candidate is frozen at **11:33:04 UTC**. Its
development revision passes eight focused Linux entries, including the new
decoded-vector lifetime/failure sweep, prepared-transfer ownership and persistent
C++ allocation-failure/retry cases. Those checks precede frozen qualification.
The frozen candidate subsequently passes all **66 normal Linux entries** and
native version/help. Final scoped diagnostics are recorded below; ASan/UBSan fails.

Candidate identities are hashes of the exact sorted JSON manifest bytes:

| Candidate | Runtime/build/config manifest | Test manifest |
| --- | --- | --- |
| C1 | `caa1a8b606e9bc75f413c79f2fc1e148e1a6e62938e7dc619fbb07f22c50d251` | revision 2: `87ec9ed45653e757a0736cad69ceac0904e35cd0c89071d991fd959ff7fea628` |
| C2 | `144e579e5d703ace10e4db403d9c56a34ee0d51a3b3046b0ff16920fb73f79d8` | `f7150eb63d6598d5f1a4e857fa59d30bda3134e8abc04fc81c724f1a1bce65f9` |

C2's complete frozen source manifest is
`4e32092ab7f9d26e2e786c33f129ee6b1afc81b9432afcf2c4ac46533ebddb0e`;
the retained source archive is
`d8a3c916dff403b57f44956cee7f359df3f3c51e59eb4f9a0978e4cd76ca1796`.
Post-freeze documentation updates are separate from those immutable build inputs.
No third runtime candidate is permitted in this run.

### Frozen C2 normal qualification

Linux executable: `teleno_node 1.3.0-dev.1+d424264b2459-dirty`, x86_64,
45,041,880 bytes, SHA-256
`3ead253eb3df8aa2084d410e280aa96b81b27062496c572822ea1917cd3b0eba`.
The complete all-target build, `--version` and `--help` each exit zero. The
runtime/build source and executable remain unchanged through all test groups.

| Serial group | Actual result | Seconds | Retained report |
| --- | --- | --- | --- |
| Single-copy restore experiment | 1/1, exit 0 | 12.708 | `c2-normal-sc5.xml` |
| Native direct restore | 1/1, exit 0 | 1.351 | `c2-normal-direct.xml` |
| All remaining configured entries | 64/64, exit 0 | 154.500 | `c2-normal-remaining.xml` |

This is one exact candidate tested in three groups, not a single 66-entry
invocation. All 64 published alpha entries remain; owned-budget and aggregate
memory regressions are additive. Standard restore reserve checks are unchanged.
Closed generated fixtures are fully archived and hash verified on My Passport
before their Linux copies are retired between groups. The built executables
also have a fully verified external archive before their identical copy is
mounted in an owned no-swap temporary build directory to release workspace.
No operational databases, services or host reserve settings are changed. These
synthetic restore checks do not establish the selected full-history backup's
40-GB whole-server outcome.

Toolchain: Ubuntu GCC 13.3.0, CMake 3.28.3, Release `-O3`, tests and native
libp2p enabled. The retained linked-input/toolchain manifest hashes **178**
objects, libraries, source archives and compiler/system inputs; its SHA-256 is
`28629dde48539f7373d5578b797de2b633d3cb874459ead9e0660accdc09a770`.
The separate compiled-dependency manifest covers **4,736** non-native inputs
from **221** compiler dependency files, SHA-256
`7b90371746a67125fa29856b20024291eeff010e9025a151a06117ff380c63c8`.
Its completeness and every input hash are rechecked after build relocation.
Frozen native files are covered separately by the source manifests above.

The builder has a different physical profile from the reference VPS and retains
its pre-existing host swap. Controlled native test children have swap disabled;
their scope is not a claim that the entire builder or target node is qualified.
Two early Mac header diagnostics did not explicitly capture their compiler
temporary-data placement. Their retained outputs are external, and neither is
used as Linux qualification.

C1 reproduces an additional classification fault: after a typed pending-record
capacity refusal, persistent C++ allocation failure in optional error logging
can replace the primary refusal with `std::bad_alloc`. The deterministic probe
against C1's instrumented native library detects the changed refusal counter and
wrong escaping exception. C2 bypasses optional formatting for capacity/allocation
failures and protects other diagnostic formatting so that the primary exception
escapes unchanged. Its regression preserves accepted state and credits through
failure, expiry/LIB cleanup and successful retry. This is application fault
injection, not a proof of whole-machine OOM recovery.

A separate client linked against exact C2 native libraries also demonstrates a
remaining read-workspace gap. A synthetic `get_pending_nonce` call with a 1-MiB
account argument requests **5,250,724 bytes in 267 C++ allocations**, while the
accounted value remains **23,476 bytes** before/after and its recorded peak does
not grow. The input already exists before measurement; temporary key/vector/
decoder work is additional. This deliberately oversized native argument is not
a representative valid mainnet address or a whole-node workload. It demonstrates
missing accounting coverage, not RSS usage or simultaneous live heap. No claim
of complete native-read admission or M1 closure follows from passing owned-vector
tests. The probe executable SHA-256 is
`67f420a3081348cd73e5a652386a480591e1942edac305517fa8e97926c0e91b`;
its source, commands and result are retained in `evidence/c2-read-workspace-gap*`.

The normal C2 adapter path reproduces the escaped-owner gap directly.
`MempoolAdapter::get_pending_transactions` moves decoded protobufs from the
owned native vector into a plain protobuf response. The vector's destruction
returns its reservation while the response still owns those fields. Retaining
**256** such responses holds **2,123,520 serialized bytes** and a reported
protobuf `SpaceUsedLong()` sum of **2,245,120 bytes**, yet current reservations
remain **50,618 bytes** before and after; the policy peak is **1,249,642 bytes**.
Serialized size and protobuf space are not heap/RSS measurements. The fixture
uses one synthetic native pending transaction and does not submit network
transactions. Its exact executable SHA-256 is
`87d253a2a8ba10a0a802da7f7962cf5e53bb4744d6caace81d82bc09e99f5139`;
source, commands and output are in `evidence/c2-rpc-output-owner-gap*`.
The initial diagnostic link failure is an inherited-working-directory mistake;
its log is retained and only the linker scope is corrected. Native implementation
and compiled probe object are unchanged.

The remaining M1 implementation must reserve ordinary nonce/RC/count and generic
database vector workspace before allocation, carry output ownership through
protobuf moves/copies, serialization and caller retention, and account for
execution/receipt growth beyond the fixed input envelope. A scope alone does
not intercept ordinary allocation. Regression evidence must show that credit
survives each handoff until the final owner frees the allocation, while preserving
retryable local overload and independent canonical processing. Representative
Linux inputs must then qualify the supplementary envelopes and a proposed
profile. These are unmet conditions, not waived exclusions that complete M1.
No third runtime candidate is permitted by this run's fixed limits.

### C2 ASan/UBSan failure

The instrumented build and clean/intentional-fault controls succeed. The focused
CTest group executes eleven entries: **ten pass and the aggregate-memory entry
fails**, with **1,008 bytes leaked in 28 allocations**. The separate cleanup
entry also passes in **145.883 seconds**: overall **11/12**, with a required
memory-safety gate still failed. The leak occurs during
`decoded_query_allocation_failures`, not during the deliberate detector controls.
Counter rollback and accepted-transaction checks pass; those do not prove heap
cleanup. Leak detection remains enabled and the failure is retained.

The allocation stacks identify both parsing a pending record and copying its
transaction into the decoded vector in `mempool_impl::get_pending_transactions`.
They pass through generated protobuf 3.17.3 message copy constructors,
`ArenaStringPtr::Set` and repeated-field allocation/publication. A failed copy
constructor does not complete the owning message whose generated destructor
would normally release its fields. Source inspection additionally confirms that
`RepeatedPtrFieldBase::Add` creates a raw element before calling
`AddOutOfLineHelper`, whose `InternalExtend(1)` can throw before the pointer is
published. That interval has no scoped element owner. Both the installed SDK
header and rebuilt pinned runtime retain this ordering; the existing ownership
patch does not cover it. Generated copy-constructor/repeated-field constructor
failure paths require their own cleanup proof as well.

A separate probe against the freshly built, unchanged starting checkpoint
confirms the same **1,008-byte/28-allocation** leak under a bounded query
allocation-failure sweep. Its clean query exits zero with no leak report; the
fault sweep exits one with the matching LeakSanitizer summary. Probe ELF:
`3ce6a4dec3ff1cdc187ecdf637a7394506ad0a0921153c39a7bba8044a414dcc`.
This baseline probe instruments the client and intercepts allocations; baseline
native/protobuf and prebuilt SDK objects remain uninstrumented. It establishes
inherited leak reproduction, not full baseline sanitizer qualification. Inputs,
commands and output are in `evidence/baseline-decoded-query-*`.

The failed C2 aggregate entry is not repeated to obtain a green result. The
diagnostic driver separately assumes a wrong executable location for its final
cleanup step and hash collection: these executables are in `build/src/`.
That harness error and the non-executed cleanup command are preserved. Only
executable discovery is corrected; the existing cleanup binary is run with its
previously justified bounded diagnostic allowance. No native source or frozen
candidate changes. The corrected cleanup passes; the original failed command
and metadata exception remain retained. Clean, intentional-use-after-free and
intentional-UB controls are repeated with the identical control binary and
return **0/1/1** with their expected detector results. All twelve instrumented
executable hashes are verified. The seven-runtime-library identity manifest is
`29f08583a25c21361cfab0250a0b8206356043e2feaa3de48b08924f6d888b48`.
The separate TSan result is recorded below.

Consequently C2 has **failed** a required memory-safety gate. Normal 66/66
results cannot override it. Repair must establish exception-safe ownership for
both decoding and message construction/copying, retain a deterministic fault
regression and qualify the repaired dependency/native input combination in a
newly authorized finite implementation run.

### C2 scoped TSan result

All **12** selected native entries pass: eleven CTest entries in **340.30
seconds**, plus the separately bounded cleanup in **145.521 seconds**. The
aggregate allocation/concurrency entry takes **306.20 seconds**. Clean and
intentional-race controls return **0/66** with the expected results. The exact
source remains frozen C2; twelve executable hashes and non-PIE ELF identities
are verified. Runtime/test proof manifest:
`cfcde917284578f00da1988cfd474820a650f5131881b2b96f98deb93ce4e5d7`.

Flags are `-fsanitize=thread -fno-omit-frame-pointer -fno-pie -no-pie`, with
`setarch x86_64 -R` confined to the instrumented children. Host ASLR settings
are unchanged. Native code and rebuilt protobuf/Fizzy are instrumented;
prebuilt generated SDK, Boost, RocksDB and crypto objects remain excluded.
No race is detected in these focused cases. This is not whole-program or
complete D1 qualification, and TSan does not clear the ASan leak failure.

### Final paired component comparison

The authoritative baseline is rebuilt from the clean, complete starting commit
`d424264b24595786fdba86d9eddbcbf1635284b6`, not inferred from older component
objects. Its 434-file source manifest SHA-256 is
`1a4d3d1da5305e23d9f5fa1e9cb38285d130aedd4b11b021df0278794ade3ca0`;
the source archive is
`b92726ca956f61bd0955b48775f44f5e240cefcae51e83388bd6c1d8eb2289c4`.
The fresh baseline fixture ELF is
`0b5bb0886c1e9e889824e85dfd7ad8bfc8059a09a9493733ed28dfb063ab90bf`;
the frozen C2 fixture ELF is
`a5f8eadede8b82aae621a18f5ec2869c5d55fb4b729b1ba48927b3cc81bc5607`.
Both compile identical final fixture source/header bytes, differing in the
baseline-selection define and the native implementation they link. Earlier
C1/historical-library comparisons remain separate retained evidence.

Each workload has one unmeasured warmup per implementation followed by **five
paired processes**, alternating baseline/candidate order. Each process admits
64 synthetic transactions with 8-KiB signatures across the same four pending
states, inventories ownership, expires records and processes LIB. The query
variant additionally holds two independent decoded result vectors through LIB.
Accepted inputs and the resulting retained-record/tombstone counts agree.
These are trusted native mempool fixtures with synthetic IDs/payers, not signed
mainnet execution, RPC, VM, durable database or producer workloads.

Medians, five samples per cell; byte figures are decimal bytes:

| Metric | Baseline ordinary | C2 ordinary | Baseline with queries | C2 with queries |
| --- | ---: | ---: | ---: | ---: |
| Timed batch, ms | 10.415 | 11.156 | 25.944 | 28.080 |
| C++ allocation calls | 142,117 | 142,451 | 198,795 | 252,193 |
| C++ requested bytes, cumulative | 12,529,779 | 12,917,655 | 22,536,867 | 25,529,943 |
| Active glibc heap after the batch | 1,067,520 | 1,199,936 | 2,203,024 | 2,335,648 |
| Process peak RSS | 14,163,968 | 14,589,952 | 15,175,680 | 15,872,000 |
| User CPU, microseconds | 10,160 | 10,189 | 25,882 | 25,924 |
| System CPU, microseconds | 117 | 1,062 | 108 | 2,040 |
| Input/output block operations | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 |
| Current policy reservations | untracked | 87,482 | untracked | 136,227,962 |
| Owned allocator requests still held | untracked | 25,408 | untracked | 35,712 |
| Peak policy reservations | untracked | 3,076,728 | untracked | 139,217,208 |

All samples retain one map entry and 256 tombstones after LIB, with zero
serialized-record owners. The admitted unique-value capacity is 542,826 bytes
in both implementations. Query results are separate decoded owners, so zero
record owners does not imply that their memory is freed.

Ordinary batch ranges are 9.098–12.439 ms baseline and 9.738–12.322 ms C2;
median absolute deviations are 1.317 and 1.166 ms. Query ranges are
25.361–27.617 and 26.579–30.728 ms, with deviations 0.583 and 1.502 ms.
Ordinary RSS ranges are 14,090,240–14,266,368 and 14,360,576–15,056,896 bytes;
query RSS ranges are 14,995,456–15,249,408 and 15,667,200–16,044,032 bytes.
Raw paired values, all distributions and external process measurements are
retained in `evidence/c2-paired-measurements-{ordinary,query}-fresh-baseline/`
and `evidence/c2-paired-fresh-baseline-summary.json` on My Passport.

The measured effect is **accounting and failure preparation with overhead**;
these workloads demonstrate no RAM saving. The additional query pre-scan also
increases allocation calls. The two protected query envelopes exceed the test's
64-MiB standalone threshold despite a much smaller active heap. This makes the
need for representative envelope calibration explicit; no VPS value is selected.

The C++ counters cover the measuring thread's `new` requests, not dependency
`malloc`. The glibc figure excludes retained free arenas. RSS is a process peak
through the timed batch, including startup/input setup; CPU/I/O are batch deltas.
The final ownership inventory occurs after that timing/RSS sample, so policy
peak and final heap observations have a slightly longer window. Batch latency
is not individual transaction tail latency. These counters and windows must not
be equated with one another or with whole-node limits. Native measured children,
including warmups, use a 200% CPU quota, a 2,500-MiB memory maximum and zero
swap on the separate builder;
that does not emulate the physical reference VPS.

The first Linux checkpoint passes the owned-budget test, aggregate mempool
failure sweeps and existing pending-record regression (3/3). It precedes later
shared transaction-work/adapter/serialization changes and is not final evidence.
A Mac header test is a development diagnostic.

Initial Linux GCC ASan/UBSan controls run cleanly and detect intentional use-after-free
and invalid-shift faults. Initial PIE TSan controls fail before main with
`unexpected memory mapping`; retain those failures. Non-PIE and child-only
ASLR-disabled controls both run cleanly and detect the intentional data race.
ASLR changes are confined to instrumented children; host settings/services are
unchanged. Final diagnostics must use working controls and disclose exclusions.

The final normal identities, all 64 existing normal entries plus regressions,
version/help and the fresh-checkpoint paired comparison are recorded above.
Scoped diagnostics are complete with the failure above. Representative workspace
coverage, complete lifetime admission and profile selection remain unmet. No VPS
deployment or producer operation occurs in this run. Component
results cannot close whole-server RAM/disk, sustained-operation, loaded-producer
or historical requirements.
