# M1 application-memory continuation: JSON cleanup

Current qualification is recorded in the
[R15 report](low-resource-lr2-m1-r15.md). R15 passes 104 normal checks, both
CLI checks, 63 focused ASan/UBSan and 61 default-TSan cases, five pairs per
measured workload and source-consistent controlled-chain create/reopen.
Exact-node one-hour mixed reads, clean restart and interruption/recovery also
pass. Final owner/physical review completes M1 for exact R15; the historical
C13/C14 results below remain unqualified.

Historical C13/C14 result: **unqualified.** C13/C14 frozen slots and C14's 93/94
normal result, SC5 strict-disk failure, and sanitizer diagnostics remain
preserved. A later source-consistent R10 build passes 101/101 normal tests and
both CLI checks on OVH and later completes isolated mainnet observer
qualification. Same-source R9 sanitizer results use an earlier
RocksDB dependency closure. The reproduced HTTP refusal ownership gap and
other aggregate-lifetime conditions drive the later corrections recorded below
and the separate R15 qualification. Earlier failures are not relabeled.

This continuation began on **2026-10-21 at 03:33:59.573386 UTC** under the
operator's standing M1 authorization and explicit removal of overall execution
time limits. It has **no overall deadline**. The owned evidence root is
`/Volumes/My Passport/teleno-m1-json-cleanup-20261021-lygqon1e/`.
The two separately tracked candidate labels are C13/C14. Neither was frozen
at the start; both slots have since been consumed and retain their evidence.
Closed C11/C12 evidence, deadlines and unused C12 remain unchanged. Every
executable test uses isolated OVH; other hosts compile only. The operational
Hetzner producer is outside scope.

## Reproduced prerequisite and proposed correction

The [C11 diagnostic](../implementation/issue-9-m1-receipt-output-repair.md#terminal-result)
exits 134 after the fake chain adapter accepts a transaction. Its next
allocation is `std::vector<nlohmann::json>::reserve` inside JSON tree
destruction. That destructor is `noexcept`, so the outer HTTP session catch
cannot handle the injected `std::bad_alloc`.

The development helper empties exclusively owned JSON trees before their
ordinary destructors. It processes leaves from the back, collapses unary
containers in place and retains up to 64 parent pointers in a fixed 512-byte
stack. Deeper branching uses rescans without heap allocation or recursive C++
calls. This fallback can trade traversal time for bounded cleanup workspace;
deep/branched controls and later measurements must expose that tradeoff.

JSON-RPC guards its parsed request, copied ID/parameters and owned response
trees through successful returns and exceptional unwinding. Explicit moves
prevent return-value optimization from aliasing a cleanup guard to the
caller's response. Native transaction admission, receipts and encoded output
ownership remain separate from cleanup. Session-error logging cannot throw a
second exception past the I/O worker when a transport allocation fails.

This correction does not claim to make every internal Protobuf/JSON parser
allocation exception-safe or to handle machine-wide OOM. Those boundaries and
full M1 qualification remain subject to the
[current objective](../implementation/issue-9-m1-aggregate-goal.md).

The concurrent-success fixture now permits 512 MiB of attributed policy credit
so two conservative conversion workspaces can coexist. Its 4-MiB fake receipts
are unchanged. Separate refusal/retry fixtures remain. This is a test-oracle
change, not a selected node default or a measured 512-MiB physical allocation.

## Required evidence and current build state

- The default JSON destructor must pass its ordinary clean control and fail
  the deliberate allocation-fault control with the expected terminate marker.
- The cleanup helper must complete with no allocation attempts under both a
  full optional budget and forced failure of all further ordinary new/new[].
  Include empty/scalar, wide mixed values, binary strings, deep unary trees,
  deep branches beyond the fixed parent stack, repeat cleanup, moved returns,
  independent copies and exception unwinding. Credits must return exactly.
- The actual HTTP post-acceptance regression must preserve the accepted call,
  close the failed response and permit a later successful call without abort.
- Retain exact success/error bytes, concurrent outputs, lifetime ownership,
  cancellation and capacity/refusal/retry. Repeat relevant sanitizer controls
  and tests on OVH before any claim about the final candidate.

The helper and HTTP fixture compile on the build-only host. The first server
component compile fails because its relocated translation unit lacks the
unchanged C11 `jsonrpc_server.hpp` directory in its include path. Its complete
control/logs are preserved; the corrective compile adds that exact include
directory without a native source change and completes both component links.
No tests execute on the builder.

## Normal development result on OVH

All **14/14 planned development invocations meet their expected result**, ending
at **2026-10-21 03:43:31.127497 UTC**. The default-destructor clean control exits
zero; its deliberately faulted counterpart reaches the expected terminate
marker **86**. The new cleanup, actual post-acceptance HTTP regression,
concurrent transaction output, success/error ownership, capacity/refusal/retry,
enabled/disabled roundtrip, concurrent mempool output, mixed capacity, mempool
roundtrip and existing session lifecycle checks all exit zero.

This sequence is not the full native CTest suite. Its synthetic chain adapter
has an observable acceptance counter but no ledger, keys or mainnet submission.
The native controller's final preparation/admission behavior still needs the
complete final-candidate suite.

The cleanup helper records **zero further ordinary new/new[] attempts** for
all nine shapes while both the optional quota is saturated and the fixture
fails every further allocation. Wide/mixed cleanup returns **5,054,704**
attributed bytes; the two 8,192-level unary trees return **3,477,921** and
**4,198,817** bytes. Deep branched array/object cases cross the fixed-parent
stack and return their own credits. These are fixture reservation/release
counts, not physical savings measurements.

The post-acceptance test covers single and mixed replies: two expected
`Session exception: std::bad_alloc` events close their respective failed
responses without process termination. Its accepted-call assertions and later
successful calls pass. The concurrent 4-MiB-receipt fixture retains two accepted
responses and reports **11,239,164** accounted bytes before stop versus
**900,730** afterward. The latter is not asserted to be zero or an unexplained
leak; it is retained attributed ownership in this scoped process, outside the
freed output contribution. Remaining global/caller ownership still requires
the complete inventory.

| Scoped normal invocation | `/usr/bin/time` peak RSS |
| --- | ---: |
| Cleanup helper, including tree creation | 7,900 KiB |
| Post-acceptance single/mixed allocation fault | 17,988 KiB |
| Two simultaneous 4-MiB transaction replies | 74,180 KiB |
| Single/mixed successful receipt ownership | 51,504 KiB |
| Single/mixed error ownership | 32,588 KiB |

Before/after host samples retain at least **3,445,276,672** MemAvailable bytes
and **921,919,488** bytes of strict disk margin including the reserve; no host
OOM or swap change is recorded. These are sparse component samples, not
continuous host extrema or whole-node qualification. No paired physical-memory
saving or attributable latency improvement is claimed.

Exact identities:

- Five-file development input archive SHA-256:
  `bcaf8d537b687b696264fed9e718c0cdf75b5267e5c1dab88ac378bd4cdeb4c1`.
- Cleanup component ELF:
  `8694422c26f90838dcff4ceea698b901feda4821f3c476434c94871414597fb9`.
- HTTP component ELF:
  `d1941d85bc91e11cb50785c2538c3a345794f5d5149ef28bf3daefafd12eb632`.

Both components use identified C11 headers and a retained C9 native link
closure; the HTTP object/test and new helper are rebuilt. This proves the
scoped regression, not a complete source-consistent C13 node. The development
manifest, commands, first failed include compile, corrective build, detector
markers, individual logs, unit properties and host samples are under
`evidence/development-build/` and `evidence/ovh-development/` in the owned root.
The remote duplicate ELF is retired after originals are hash-verified and kept
on My Passport; the restored test database and services are unchanged.

## Initial sanitizer binding failures and correction

The initial scoped ASan/UBSan build completes on the build-only host. Its
static-runtime OVH sequence passes all **3 detector controls**, including the
expected heap-use-after-free and undefined-shift diagnostics, but fails all
**12 component invocations** with `alloc-dealloc-mismatch`. The first HTTP
failure is during generated Protobuf startup, before the selected regression.
The helper also exposes the mismatch on its first budget-state destruction.
The executable's strong test `new` uses Teleno's malloc-backed primitive while
the static sanitizer runtime supplies a strong `delete` that overrides
Teleno's weak replacement. This invalid binding also bypasses Teleno's final
lease release; do not suppress the mismatch detector or reinterpret these
failed component runs as passing.

The initial TSan static-runtime link likewise fails with two strong definitions
of `operator new(unsigned long)` from the fault fixture and `libtsan.a`.
All failed compile/link controls, logs and initial OVH results are preserved.
These are scoped sanitizer-integration failures, not evidence that the HTTP
regression ran successfully or that the cleanup helper itself caused a leak.

The corrective links reuse the same instrumented source objects with dynamic
GCC sanitizer runtimes. Runtime files are copied and hash-verified in an owned
OVH directory, selected only for the test child through `LD_LIBRARY_PATH`.
No host package, loader configuration, global ASLR setting or detector option
is weakened. Static inspection now shows the executable's `new` and weak
`delete` replacements instead of the static runtime's strong delete. ASLR is
disabled only in each controlled child using `setarch x86_64 -R`.

The corrected ASan/UBSan sequence completes **15/15 expected invocations** on
OVH at **03:59:45.628560 UTC**, including all three detector controls, leak
detection, cleanup and the selected HTTP regressions. These are the earlier
development objects, not a final whole-node candidate. Prebuilt SDK/native
libraries remain explicit instrumentation exclusions.

### TSan cold-reflection investigation

The dynamic TSan sequence has **13 uninterrupted passing invocations and one
interrupted capacity test**. The latter blocks in Protobuf field-type lazy
initialization reached by `SpaceUsedLong`, after filling the request quota.
Read-only thread stacks show a worker waiting in TSan's `pthread_once` guard;
the main thread joins that worker during server destruction. The exact owned
diagnostic is terminated after preserving the stacks. No node is restarted.

`systemd-run --wait --pipe` returns zero for this externally terminated unit,
without the native completion or time statistics. The raw runner consequently
labels it passed; that raw result is preserved and explicitly superseded by
`evidence/ovh-development-tsan-dynamic/reconciliation.json`. This invocation
**does not pass**. New runners require an explicit post-child exit marker and
reject interrupted invocations, regardless of the wrapper's zero status.

A standalone once-initializer control retries correctly without TSan. With
the same identified dynamic TSan runtime, it fails its explicit two-second
retry-liveness assertion after an initializer throws. This reproduces a
runtime limitation separately from Teleno; it does not demonstrate a native
node deadlock. The [GCC runtime implementation](https://github.com/gcc-mirror/gcc/blob/releases/gcc-13/libsanitizer/tsan/tsan_interceptors_posix.cpp)
also shows guard release following a normal initializer return. No detector
is suppressed or substituted. The first control launch has an incorrect
private runtime path and exits 127; that environment failure is preserved,
then only the affected control is rerun with the verified runtime path.

Separately, the normal cold-Protobuf regression fails against the previous
helper: `GetReflection()` alone does not warm the field-type metadata needed
by sizing, so a fully reserved request can refuse process-global descriptor
initialization. The development repair warms reachable compiled descriptors
and default prototypes once outside request admission. Its traversal scratch
and immutable tables are explicit process-metadata exclusions and remain
whole-node RSS consumers. Sizing the **actual** message stays inside capture;
message-specific map/extension materialization is not exempted.

New tests require first-use and concurrent sizing at full capacity, unchanged
credits, refusal of an actual message copy, successful later retry and exact
final-owner release. The repaired R3 components pass these checks as recorded
below. An initial relocated-header build includes two different copies of
the budget header and fails redefinition checks; its logs/source are retained.
The corrective component build uses a complete identified source-header tree.

## First five-pair cleanup comparison

The first comparison completes all **30 processes: five baseline/candidate
pairs for each of three shapes**, with identical inputs and 16 cleanup
iterations per process. Pair order alternates. The same ELF selects ordinary
destruction or the development helper; no native node runs concurrently.

| Shape | Ordinary cleanup new calls / requested bytes, 16 iterations | Helper new calls / requested bytes | Median cleanup thread CPU, ordinary / helper |
| --- | ---: | ---: | ---: |
| Wide mixed tree | 32 / 6,291,456 | 0 / 0 | 235.96 / 194.56 ms |
| Unary depth 8,192 | 16 / 256 | 0 / 0 | 21.66 / 23.07 ms |
| Branched depth 1,024 | 176 / 1,048,064 | 0 / 0 | 3.44 / 59.82 ms |

Both variants return exactly the same attributed input credits in every pair.
The helper removes cleanup allocation requests, but the initial deep fallback
has a material **17.4x median cleanup CPU cost** on the branched fixture. This
is a demonstrated development tradeoff requiring correction, not a qualified
improvement. Process RSS medians are close and variable; no whole-node or
attributable physical-memory saving is claimed. Full per-pair latency, RSS,
CPU, I/O and reservation distributions remain in
`evidence/ovh-cleanup-pairs-r1/summary.json` and individual results.

The next development helper consumes leaf siblings at either edge before
descending again, swapping retiring values to avoid array-front shifts. Deep
comb trees then collapse in place. General branching beyond the fixed stack
still uses rescans; no constant CPU bound is claimed. Added reverse-order and
allocated-string sibling cases must retain allocation-free cleanup and exact
release. Repeat the paired comparison after functional/sanitizer checks;
preserve the initial slower result and its exact helper/ELF identities.

## R3 component qualification and second paired comparison

The revised development source-header archive contains 183 files, SHA-256
`971e27e6e21d9f4bba8c9a448835516afb12a4fbb8e51a70e029d7651fadcf9b`.
It is still linked to the explicit prebuilt native/SDK closure; it is not a
frozen C13 whole-node candidate. Every executable invocation remains on OVH.

| R3 sequence | Expected invocations passed | End UTC, 2026-10-21 | Lowest before/after host MemAvailable | Lowest sampled strict disk margin including reserve |
| --- | ---: | --- | ---: | ---: |
| Normal | 16/16 | 04:30:17.965226 | 3,439,693,824 B | 869,552,128 B |
| ASan/UBSan | 17/17 | 04:33:21.742994 | 3,402,596,352 B | 860,139,520 B |
| Default TSan | 16/16 | 04:35:53.438519 | 3,443,499,008 B | 869,638,144 B |

The new cold/concurrent metadata tests and exact copy-refusal/retry accounting
pass in all three sequences. The previously blocked HTTP capacity test
completes its native assertion marker under TSan. Leak detection and the
identified clean/known-fault detector controls pass. No OOM or swap is recorded.
The sampled host margins and scoped sanitizer results do not prove continuous
whole-node limits. Full controls, ELF hashes and rusage are retained in
`evidence/development-r3-summary.json` and `evidence/ovh-development-r3-*/`.

The second comparison completes another **30 processes**, five alternating
pairs per unchanged shape/input and 16 cleanup iterations per process. The
profile ELF SHA-256 is
`959293675ac20a67d04990e1042d83e251921730b65739ca1f5b6c887747af1c`.

| Shape | Median cleanup thread CPU, ordinary / revised helper | Median process peak RSS, ordinary / helper |
| --- | ---: | ---: |
| Wide mixed tree | 230.05 / 203.42 ms | 16,015,360 / 15,667,200 B |
| Unary depth 8,192 | 24.35 / 22.39 ms | 6,184,960 / 6,230,016 B |
| Branched depth 1,024 | 3.69 / 3.50 ms | 4,136,960 / 4,132,864 B |

The revised helper still has **zero cleanup new/new[] calls and requested
bytes in every sample**, with exactly equal final credit release. The original
17.4x comb-fixture CPU penalty no longer appears; baseline/revised CPU ranges
overlap. The wide fixture's measured process-RSS median is 348,160 bytes lower;
this is scoped process evidence, not a whole-node saving or an aggregate RSS
ceiling. Small-shape RSS is nearly unchanged and variable. Retain all CPU,
latency, I/O and allocation distributions in
`evidence/ovh-cleanup-pairs-r2/summary.json`; do not generalize these medians to
representative node workloads.

## Additional serialization boundary found before freeze

A guarded JSON parse diagnostic completes **510 quota settings**: 179 local
capacity refusals and 331 successful parses, all with exact final credit
release. It does not reproduce a parser-cleanup abort for its selected input.
This is a negative result for that fixture, not proof of all JSON allocation
failure paths. Its ELF SHA-256 is
`b8137576a45060ecaa9e0583a44d6b3255242735de2b5c5fac03271a960ebcba`.

A separate full-quota response-serialization control **does** terminate with
marker 86 while destroying an unguarded JSON value after `dump()` refuses.
Its trace reaches `json_value::destroy`. The same ELF's guarded variant
recovers the local refusal and returns exactly its response credits. ELF
SHA-256:
`f51661bdebfb271bdf333d948394a7cfd0ae5873a6eea773f114c1a1afa7303f`.
The source contains equivalent `process_single_request(...).dump()` temporaries;
the direct control reproduces that ownership pattern, not an integrated native
HTTP invocation. Evidence is in `evidence/ovh-dump-capacity-control/`.

The next development revision adds `dump_owned_json` to own responses through
serialization and exceptional cleanup. It replaces those temporary dump sites,
holds normalized parameters explicitly through dispatch and guards additional
local wrapper/result/status/error trees. New regressions require identical
clean bytes, one correctly classified quota/allocation refusal, no second
cleanup allocation and exact final release. R4 compilation is in progress;
R3's passing tests do not qualify these later source changes. No C13/C14 slot
is frozen or consumed, and full native/whole-node qualification remains open.

### R4 and dependency-internal cleanup results

R4 subsequently passes **16/16 normal, 17/17 ASan/UBSan and 16/16 default
TSan** component invocations on OVH, each with explicit child-completion
evidence. These remain mixed retained-C9 native closures, not whole-node
qualification. Preserve their exact identities and controls under
`evidence/ovh-development-r4-*/`.

An outer guard cannot protect a JSON copy that fails inside its constructor.
A separate per-byte capacity sweep against the stock pinned 3.11.2 dependency
terminates at quota **10,793 bytes** in `json_value::destroy`, despite the
outer guard. The proposed build-owned dependency backport removes dynamic
allocation from that destructor while retaining the dependency's namespace,
layout and wire behavior. It uses the fixed parent stack, unary collapse,
edge-leaf cleanup and bounded-stack rescanning algorithm. No constant CPU
bound or general third-party allocation-failure safety is claimed.

The stock header SHA-256 is
`bbf96a24a8d59ef79467761e2a803537e1476909bb7d6baed23094a1833c088c`;
the proposed header is
`237969bba187bfcb9ba681122e2e6bc1495c27faa610f54351758952d4799cbe`.
The patch SHA-256 is
`01b32826e86ff21a3bc3a1608c9e4e3aa30088fd21c933a89efc9a5a55d64892`.
Installed SDK files are unchanged. Staging verifies all 44 installed JSON
headers and the exact patched result in a separate build-owned tree.

Two proposed-header copy controls each complete **130,560 quota settings**,
including a distinct `JSON_DIAGNOSTICS=1` build. Default unguarded and guarded
dump controls also recover with exact final credits: **4/4** invocations pass.
The separate direct-default-destructor regression passes **9/9** OVH
invocations: normal, diagnostics, ASan/UBSan and default TSan, plus functioning
clean/known-fault detector controls. It covers both branch orientations, depth
8,192, partial copies, ordered JSON, primitive/binary/discarded values and
cross-thread final-owner release at full quota. These are dependency-component
results; they do not qualify SDK or whole-node integration.

The staging recipe passes **7/7** tests using a private extracted CMake/Ninja
toolchain on OVH. The initial 6/7 run's missing-Makefiles environment failure
is preserved separately. There is no global package installation. The new
SDK recipe rebuilds unchanged, archive- and file-hash-pinned Koinos exception
and crypto 1.0.2 sources with the same patched JSON headers. Symbol inspection
found JSON instantiations in both installed archives, so merely changing
native include order would leave an inconsistent inline-code closure.
Integrated R5 configuration/compilation is underway, with no tests on the
builder and no frozen C13/C14 slot. The development archive SHA-256 is
`aa000662e081f152cd40413b7f1e802205a1017c0752e2c18255c978221bd71d`.

### Encoded input retention and remaining admission work

An actual HTTP/native-adapter fixture with a 262,144-byte transaction argument
measures **1,019,389 additional attributed bytes at native service entry**
against unchanged R4 product objects. The exact argument bytes and successful
later cleanup are checked; the release requirement fails as expected. Source
now retires the extracted JSON parameters after normalization and the
normalized tree after native decoding, before service execution. The changed
source is not yet qualified by that baseline or by older R4 results.

Still required are normalization/allocation-failure coverage, consistent SDK
and native integration, the complete M1 inventory and admission proof, final
normal/sanitizer suites, representative paired final comparisons, identified
CLI checks and the exact final whole-node resource/restart evidence on OVH.
The existing helper comparison does not qualify the new default destructor.

A private collector cleanup defect appended unintended relative operands to
two owned-binary removal commands. The additional file's prior existence is
unknown; its current absence does not establish that nothing was lost. Original
collectors and reconciliation evidence are preserved, the affected scripts
now name one absolute owned operand, and the operator has been asked about
possible pre-existing data. No operational producer access occurred. This
collector failure is distinct from native test results with completion markers.

### Integrated R5 result and R6 corrections

The integrated R5 normal sequence completes **18/19** invocations on OVH.
Its source-identical native dependencies and the separate exception/crypto
SDK archives compile without test execution on the builder. The input
lifetime regression now passes: its service-entry delta is **281,499 bytes**,
against the earlier **1,019,389-byte** result for the same 262,144-byte argument.
Both return exactly to 961,030 attributed baseline bytes after cleanup. This
is one retention/accounting observation; no paired physical-RSS saving is
established by these two invocations.

R5's gRPC invocation aborts in the large-error fixture. An unchanged-product
diagnostic independently observes status **8 / RESOURCE_EXHAUSTED**, with
`to-be-sent trailing metadata size exceeds peer limit`. The fixture expected
a complete 1-MiB status string while retaining the SDK client's smaller default
metadata limit. The correction changes only that test client's limit to
2 MiB. A separate default-client call must still observe the metadata refusal;
the enlarged test client must receive the complete original INTERNAL status.
Production defaults are unchanged. Preserve the original failure and diagnostic
in `evidence/ovh-integrated-r5-normal/` and
`evidence/ovh-grpc-large-status-diagnostic/`.

A second actual HTTP fault regression then reproduces a native-input correctness
gap against the unchanged R4 implementation: **14 injected allocation/capacity
failures produce four wrong native payer inputs and two misclassified local
capacity refusals**. The base58 normalizer catches resource failures as ordinary
format failures and retains a string that is also syntactically valid base64;
native decoding therefore receives different bytes. The fake service detects
the changed input before acceptance. Later healthy calls succeed and final
credits are exact. The first harness incorrectly required every physical
allocation failure to return on the same connection; that failure is preserved.
The corrected harness accepts an explicitly observed disconnect, rejects a
timeout and requires an exact-input successful retry on a fresh connection.

R6 rethrows `LocalCapacityExceeded` and `std::bad_alloc` before the existing
format-fallback handler. It preserves the normal fallback for valid protobuf
base64 that is not valid base58. The corrected normal regression injects the
same 14 failures with **zero input mismatches and zero misclassified capacity
refusals**, one allowed physical-allocation disconnect, successful retries and
exact final credits. All **14/14 R6 normal RPC invocations pass**, including
the corrected gRPC fixture, concurrent output, post-acceptance failure,
capacity refusal, complete error/success output and session lifecycle.

The R6 development archive SHA-256 is
`98e1aa30b02a5ccb4aae4198145cb93377a345cbd5abb842def8b532fe7669a3`.
Changed RPC translation units, fixtures and the allocation primitive are
rebuilt against source-checked unchanged R5 native dependencies. Scoped
ASan/UBSan and default TSan execution is underway on OVH. Their uninstrumented
R5 native/third-party closure and vptr exclusion must remain explicit; normal
success does not qualify their results or the final whole node. No C13/C14
candidate is frozen.

R6 subsequently passes **17/17 scoped ASan/UBSan** invocations. Default TSan
passes **15/16**; its gRPC invocation reports a cross-thread read in prebuilt
`grpc_core::got_initial_metadata` against `recvmsg`. General clean/known-fault
controls pass, but incomplete gRPC instrumentation may conceal internal
synchronization from the detector. A separate SDK-only control, without Teleno's
server or allocation replacement, reproduces the same metadata signal. Its
intentional callback-race variant instead stops on another SDK publication
warning before validating the intended fault. These results do not establish
a benign false positive, a Teleno race or a usable gRPC-specific detector
control. Do not suppress or count either SDK signal as a pass.

The pinned installed-version gRPC `1.31.0-p0` archive SHA-256 is
`48f52c566a03b3bea26e8440798d2ff4ce181221935e5983fb9495e13618dd55`.
An isolated diagnostic build verifies matching installed public headers and
rebuilds that unmodified source with TSan. Its first configure failure, caused
by Hunter-root discovery, remains preserved; the supported Hunter-disabled
configuration succeeds. Runtime compilation is underway, with tests disabled
and no installed SDK changes. Exact controls and traces are in
`evidence/ovh-development-r6-tsan/` and
`evidence/ovh-grpc-sdk-tsan-controls-v1/`.

### Five paired input-retention measurements

Five alternating pairs run **10 fresh OVH processes** with an identical
256-KiB input argument, complete 1-MiB fake receipt, warmup, fixture and native
dependency closure. The baseline control differs only by retaining the two
encoded parameter trees. Its ELF SHA-256 is
`b333c05f560689c2db96da2336f01b005126342a4e95434d2896704d7b236d77`;
the early-retirement ELF is
`f96af870e8c472c161e83fc8ecc4d678bbdae135abb1a88442c6e7ce77ce189b`.
All native-byte, complete-output and exact-credit-release assertions pass.
The initial relocated-include compilation failure and corrected build are
preserved separately.

| Metric | Baseline median (range) | Early retirement median (range) |
| --- | ---: | ---: |
| Attributed service-entry delta | 1,019,389 B (all five) | 281,499 B (all five) |
| Process peak RSS | 27,787,264 B (26,959,872–28,041,216) | 28,250,112 B (28,123,136–28,573,696) |
| Process user + system CPU | 0.20 s (0.19–0.21) | 0.19 s (0.18–0.22) |
| Process wall time | 0.21 s (0.20–0.22) | 0.21 s (0.19–0.22) |
| Filesystem input/output blocks | 0 / 0 in every sample | 0 / 0 in every sample |

The live attribution falls by 737,890 bytes, but peak RSS is **462,848 bytes
higher at the median and higher in every pair**. CPU/wall ranges overlap;
no speed improvement is claimed. This does not demonstrate physical-memory
savings. The possible allocator/layout explanation is a hypothesis, not a
measured cause. Preserve the complete distributions in
`evidence/ovh-input-retention-pairs-r2/summary.json`.

Development source therefore takes the next optimization step before freeze:
borrow the immutable request parameters while normalization owns its one
mutable copy, and inspect byte-field strings by reference. This removes
avoidable complete-tree/string copies before allocation rather than merely
retiring them later. It is unqualified until new exact-input, failure and
paired measurements complete. None of these synthetic components establishes
the final whole-node RSS or issue-9 outcomes.

### Current ownership review before candidate freeze

This table updates the historical M1-H inventory for the current source. It
is a source review, not final-candidate qualification or a waiver of gaps.

| Consumer | Current attribution/lifetime | Remaining evidence or boundary |
| --- | --- | --- |
| Shared serialized pending records, retained fork maps, keys and tombstones | Owned allocators and shared immutable payload owners retain credits until physical final-owner release. | Final-source boundary, retained-ancestor, failure and block/LIB suites remain required. Supplementary key/allocator headroom is distinct from requested bytes. |
| Mempool policy/control state and fork indexes | Bootstrap/control leases, owned allocations and native allocation capture share the reservation state. | Audit final native closure and any escaped control ownership; counters alone are not heap totals. |
| Ordinary nonce/RC/count reads and decoded vectors | Mempool/read scopes capture native allocations; allocator-bearing vectors and returned fields preserve their final-owner credits. | Re-run final shorter-predecessor, partial-copy, refusal and retained-caller tests. Historical C9 passes do not qualify changed source. |
| Native standalone execution, admission and notifications | Work-count/input-wire reservations, supplementary native workspace and scoped ordinary allocations precede execution/publication. | Preserve failure atomicity and local classification through every observer/receipt path; select the profile from Linux evidence. |
| Canonical block/LIB and cleanup/recovery work | Instrumented M1 allocations are counted with protected quota exemption. | A full admission policy must not invalidate a canonical block. Protected work can exceed the standalone threshold and requires independent physical margins. |
| Transaction parameter copies and decoded native input | The original immutable params are borrowed; normalization owns one mutable copy, which retires before service entry. Native decoding enters M1 capture. | The shared original HTTP body/DOM and SDK-owned incoming gRPC message are borrowed transport inputs outside this allocation domain; include their physical cost in whole-node measurements and the R1 inventory. |
| HTTP transaction/mempool response trees, mixed bodies and asynchronous owners | Prepared responses and their allocation/lease owners survive until final write/cancellation completion. | Final success/error, refusal/retry, post-acceptance and cancellation checks remain mandatory. Other HTTP services are outside M1. |
| Protobuf JSON conversion and serialized gRPC output | Explicit conservative workspace leases cover selected third-party conversion/serialization while quota callbacks are bypassed narrowly. | These envelopes are not measured heap bounds. Native decoder failure coverage, payload/SDK handoff and final lease release require final evidence. |
| gRPC output-state, interceptor and registry control allocations | R8 attributes startup and per-call owners, admission and final physical release. Fixed startup and minimum refusal controls are counted but quota-exempt. | Scoped ordinary, full-policy, four setup-failure and retry cases pass; concurrent cancellation/stop and final-source qualification remain. SDK queues/context overhead remains a distinct transport consumer. |
| Immutable descriptor metadata, allocator retention, static tracking table, stacks, VM and shared database/cache memory | Some process metadata is initialized outside request quotas; other services/allocator arenas are not M1 heap ceilings. | Disclose exclusions and any attribution retained by lazy initialization. Measure actual RSS, CPU, I/O and latency on the exact final node. |

The complete inventory, gRPC control allocation assessment, final normal and
scoped sanitizer suites, comparable Linux profiles and exact whole-node
resource/restart evidence are still required. M1 and issue 9 remain open.

The Koinos One native-developer documentation and changelog are updated without
changing app packaging, the native submodule pin or bundled binary. Its rendered
manual changelog is regenerated and `mkdocs build --strict` passes with generated
output on My Passport. This documentation handoff does not qualify integration.

No whole-node physical-memory saving, final-candidate qualification, whole-node margin,
issue-9 completion or legacy defect is claimed from this development state.

### R7 input-copy optimization and paired process result

R7 avoids the redundant parameter tree and byte-field string copy. Source
archive `ebaa1ae9021c9ec003de3ce3224e7f9a7b47680130b12348be061aea44ef614f`
passes **13/13 normal, 16/16 scoped ASan/UBSan and 15/15 default TSan HTTP
invocations** on OVH. Twelve normalization allocation faults cause zero wrong
native inputs, misclassified refusals or disconnects, with exact final credits.

Five new alternating pairs use ten fresh processes with identical fixture,
inputs and dependencies. Baseline peak-RSS median is **27,652,096 B**
(27,545,600–27,750,400); R7 is **27,103,232 B** (26,824,704–27,316,224),
**548,864 B lower**, with lower RSS in every pair. CPU and wall-time medians
are 0.18 seconds on both sides; filesystem blocks are zero. Service-entry
attribution is 1,019,389 versus 281,499 B in every sample. This is a scoped
synthetic-process improvement, not a whole-node saving. Preserve the negative
R6 comparison. Exact samples: `evidence/ovh-input-retention-pairs-r7/summary.json`.

### Usable gRPC detector closure and R8 controls

The unchanged pinned gRPC source rebuilt with TSan passes its SDK-only clean
control, detects the deliberate callback race and passes the unchanged native
gRPC regression: **3/3 expected outcomes**, without detector suppressions.
All 270 public headers match the installed SDK. The five instrumented archives
and remaining prebuilt-dependency exclusions are identified in
`evidence/ovh-grpc-instrumented-tsan-v1/`. This makes the detector usable for
these cases; it does not prove the absence of every native race.

The [R8 control repair](../implementation/issue-9-m1-grpc-control-ownership.md)
passes **2/2 normal** and **4/4 default TSan** invocations, including detector
controls. Attribution is **240 B bootstrap, 968 B startup, 2,048 B held and
240 B after stop**. Full-policy restart/refusal, refusal before native work,
four distinct setup allocation failures, cleanup and a healthy prepared fake
transaction pass. No production funds or ledger are involved.

The first R8 ASan sequence is **4/5**, with a context-destruction invalid free.
Static compilation probes identify an ABI mismatch: pinned gRPC's ASan header
mode adds a leak-checker pointer to its public mutex. `grpc::ClientContext`
changes from 504 to 512 B and its deadline/authority offsets from 80/96 to
88/104, while the prebuilt library retains the normal layout. The inline
deadline write corrupts the field freed by the prebuilt destructor. This also
invalidates the earlier R6 gRPC ASan pass as gRPC qualification; HTTP-only
components do not instantiate those gRPC context objects.

Recompiling both gRPC consumer translation units with the upstream
`GRPC_ASAN_SUPPRESSED=1` compatibility define restores the 504-byte layout.
Compiler ASan/UBSan remains enabled for Teleno and the fixture; the SDK's absent
conditional mutex/CV leak checker and prebuilt dependencies remain exclusions.
Compatible ELF
`949968ca47b7c2ad01254f9bcf2fc9baf05710f9b385465a03336cbdad6f5ab6`
passes **5/5** invocations, including working clean, use-after-free and undefined
behavior controls. Preserve `evidence/ovh-development-r8-asan/`,
`evidence/grpc-context-layout/` and
`evidence/ovh-development-r8-asan-abi-asan/` separately.

An expanded fixture now covers simultaneous held handlers, client cancellation,
stop waiting for native work and exact final release. It passes **3/3 normal,
9/9 scoped ASan/UBSan and 5/5 default TSan invocations**. Two held handlers
retain 2,832 attributed bytes versus 2,048 for one; cancellation/stop retain
live ownership and final stop returns exactly to 240 B. The clean gRPC
callback passes; deliberate callback use-after-free and unaligned stores
are detected by ASan and UBSan. These controls verify that ABI compatibility
does not disable application instrumentation. Exact build hashes and logs:
`evidence/development-r8-concurrency-build/` and
`evidence/ovh-development-r8-concurrency-{normal,asan,tsan}/`.
Final-source and whole-node gates remain.

### C13 source freeze and final qualification

The complete 469-file C13 snapshot freezes after the above scoped controls.
Its source/dependency/build identities are recorded under the owned
`evidence/c13/` and `inputs/c13-source.json`. Build the complete normal native
suite and exact node from that source; do not substitute the R5/R8 components.
All executable checks remain OVH-only. Normal suites/CLI, scoped integrated
sanitizers with controls, five comparable samples for any final saving claim,
Linux policy selection and whole-node mixed-work/restart evidence are pending.
Closed C1–C12 records and C14 remain unchanged. No overall deadline applies.

The configured normal suite contains **90 entries**, retaining all **74 C9
entries** and adding **16 regressions**. The first four source-recipe entries
pass on OVH while the remaining native executables compile. This is **4/90
suite commands**, not full normal-suite success or a native-binary test pass.
The frozen C13 generated version is `1.3.0-dev.1+d424264b2459-dirty`, derived
from the real base Git checkout and frozen overlay; actual binary version/help
checks remain OVH-only and pending. Exact source archive SHA-256:
`45b4b95350848942d1a787d9ee4841a22bb02da58fe7c5382d71d56eebb8a257`.

An initial recipe-input staging check refuses all symlinks before extraction.
The pinned Fizzy source contains four internal Rust-to-native links; a separate
staging revision verifies that every link resolves inside its own pinned source
tree and succeeds. Preserve that preparation refusal separately from native
test outcomes. It changes no candidate source, installed dependency or counter.

The full normal native build subsequently completes without executing tests
on the builder. All **70 ELF outputs** are hash-verified on My Passport. The
final normal node SHA-256 is
`ac1adb61e6924a3cd8e5b0cd3d2307fd37589820227eb94d3597d6b9fc2b922e`;
version/help pass on OVH and report `1.3.0-dev.1+d424264b2459-dirty`.
All 469 source files remain unchanged in the builder input. Exact toolchain,
link commands and **167 unique linked-library identities** are preserved in
`evidence/c13/full-normal-build/dependency-toolchain-and-links.json`.

The normal phase subsequently stops at the resource guard described below.
No final whole-node observer has started.

### C13 remains unqualified: resource guard and status-path defects

The normal suite reaches **41/90 functional CTest passes**, plus version/help.
The last direct-restore test returns zero, but accumulated generated fixtures
and unnecessarily co-staged executables leave the strict whole-server budget
**11,403,264 B over its limit**, including the unchanged recovery reserve.
The phase is a resource failure, not complete suite success. There is no OOM
or swap. Preserve `evidence/c13/normal-native-suite/` and
`evidence/c13/normal-disk-guard/` without reclassifying their outcomes.

All 295 generated regular files are archived and individually hash-verified on
My Passport before retirement; the five co-staged executable copies are also
verified against their external copies before removal. Only owned generated
duplicates are retired. The stopped full-history database is unchanged. This
restores **622,338,048 B** of strict disk margin at the recorded checkpoint.
Future qualification must stage only the current case's executable dependencies,
preserve/retire generated fixtures between cases, and sample large-test disk
peaks. Freeing test artifacts does not turn the failed phase into a pass.

C13's focused ASan/UBSan build completes with 21 ELF outputs, all independently
hash-verified in the external archive. No C13 sanitizer test has run at this
checkpoint. The first configure/link attempt fails because a dependency probe
loses a separate non-PIE linker flag. Forwarding that flag through its compile
flags corrects the build configuration without changing frozen source. Preserve
both attempts and the existing gRPC ABI/instrumentation exclusions. Archive
SHA-256: `6adb2ddbf5d49d73cba7f79760af14e72dca7f47502f6d613a922826f3b4881c`.

An exact frozen-class OVH diagnostic reproduces three status-response gaps:

- At a full ordinary quota, the application-error fallback's live refusal
  string and minimum SDK workspace are uncounted. Factory setup refusal has
  separate working ownership; it does not cover this later path.
- A 16,384-byte valid UTF-8 status reserves 32,832 B, while the actual pinned
  SDK produces 49,152 encoded bytes. Overlapping raw slice, encoded slice and
  status capacity require at least 81,921 B in this fixture.
- One injected allocation failure copying a status escapes its preparation
  boundary instead of returning the intended local refusal. This direct
  diagnostic does not demonstrate a deployed node crash.

The new permanent status regressions reproduce all four acceptance failures
against the same frozen C13 class, including a separately identified argument
copy: converting `exception::what()` to the old `const std::string&` parameter
can allocate before entering the function's admission/catch scope. The first
unfrozen repair passes refusal ownership, encoding and status-copy checks, but
still fails that argument test. Preserve all eight expected diagnostic outcomes
under `evidence/development-r9-status-normal/`; they are **four failing baseline
requirements, three repaired checks and one remaining failure**, not eight
qualification passes.

The next unfrozen revision borrows the error text as `std::string_view` until
admission, captures the actual SDK-required C++ string allocations, retains
explicit encoding workspace to completion, and counts the fixed refusal with
protected quota exemption. It permits one fallback after a physical copy
failure and exposes a second failure. The new regressions test both ordinary
copies, original bytes, caller ownership beyond state destruction, full-policy
refusal, exact final release, repeated failure and healthy retry. Their revised
normal/sanitizer execution is recorded below. C14 remains unfrozen.

C13 is unqualified; its source, artifacts, failed controls and counters remain
unchanged. M1 still requires a corrected source-consistent final candidate,
complete native and supported sanitizer suites with controls, comparable
measurements, representative Linux settings and whole-node mixed-work/resource/
restart evidence. The operational Hetzner producer remains outside this work.

### R10 status repair: scoped checks pass

The identified unfrozen input archive is
`56be97224fef379abed55b9f8e7a788828057aa12a520876f3f55fa5ce61f2ad`.
It passes **7/7 normal, 13/13 scoped ASan/UBSan and 9/9 scoped default-TSan
invocations**, including functioning clean/known-fault controls. All executable
checks run only on OVH. Four permanent regressions cover fixed-refusal lifetime,
the independent encoding lower bound, each status copy, borrowed C-string input,
bounded physical-failure fallback, exact final release and healthy retry. The
existing real gRPC behavior, control-owner and concurrent cancellation/stop
cases also pass.

At a full 1,048,576-byte ordinary policy, the refusal holds **1,052,927 B**;
after its state is destroyed the live caller status still retains **255 B**
above that full policy. Releasing it returns exactly to the full policy, and
releasing the fixture occupancy returns to the **240-byte bootstrap**.
Protected refusal/workspace use is intentionally visible above the ordinary
admission threshold. These values are policy attribution, not physical heap
or RSS measurements.

The first R10 ASan launch returns 127 before native `main` because its explicit
runtime directory is missing from the environment. Preserve that preparation
failure. The separate qualified-runtime phase uses the **same verified binaries**
with hash-pinned runtime libraries; it passes all 13 checks. ASan retains the
documented public gRPC ABI compatibility define, vptr exclusion and prebuilt
dependency exclusions; its actual gRPC callback memory/UB controls detect the
deliberate faults. TSan instruments the new status fixture, owned allocator,
native gRPC source and concurrency fixture, plus the five pinned SDK archives.
Other native/prebuilt archives in this development closure remain uninstrumented;
complete final-source sanitizer qualification is still required.

Evidence: `evidence/development-r10-status-normal/` and
`evidence/development-r10-status-{asan,tsan}-runtime-qualified/`. The production
gRPC fixture ELF SHA-256 values are:

| Configuration | SHA-256 |
| --- | --- |
| Normal | `6f0c594e870330e32edd62fb32660be425bf1809e169adcfcfa448755152bf86` |
| ASan/UBSan | `7f5e2ed3302c6a8136c06bcb5845ccd7d8c8a7f8357d1d96c774c758c8e85640` |
| TSan | `c1dea9b1817516ecf2876003d2e6108cf63df60169a1ec186a6175a45d9f104f` |

The native CMake suite adds four permanent status cases while preserving the
90 C13 entries. Configuration/build and all **94** entries for a new frozen
candidate remain pending. No new physical-memory saving is claimed from this
repair. C13 remains unqualified; C14 is unused; M1 and issue 9 remain open.

### C14 frozen for full qualification

C14 freezes at **2026-10-21 12:14:23.846369 UTC**, with **471 source files**
and source archive SHA-256
`8400d631109f458c61c82ad12ae24aab2ce35f670c160450d595b0e2bb1b8869`.
Its runtime changes match the passing R10 inputs; four permanent status tests
bring the configured normal suite to **94 entries**, retaining all 90 C13
entries. This consumes the second frozen slot in this continuation. C13 and
all earlier results remain unchanged.

The complete normal node/suite build is running on the build-only miner.
The source export, original CMake cache and test metadata are staged on OVH.
All **2,957 prepared dependency files and four internal source links** match
the frozen C14 builder inputs. The four registered dependency-recipe tests
pass through CTest on OVH; this is **4/94**, not full native qualification.
The remaining tests and version/help await the exact final ELF outputs.

To preserve the strict disk budget, execution stages only the current case's
required binaries. Identical root-layout hardlinks serve inherited diagnostic
tools while CTest keeps its generated `src` executable paths. CMake source
binding, tests and assertions remain unchanged; the explicitly selected build
root names the original CMake cache. The private CMake executable replaces
only its three unavailable `/usr/bin/cmake` argument paths in execution
metadata, with original/execution hashes retained.

Each case has separate generated-data and temporary roots, a 250-ms resource
sampler, explicit native completion and a surviving-child check. All generated
case files are externally archived and hash-verified before retiring their
remote copies. This avoids the accumulation that stopped C13. The first four
cases retain at least **529,813,504 B** of sampled strict disk margin, including
the unchanged recovery reserve. No whole-history observer has started.

Before the build, **15,788 closed builder files** and their metadata are
externally verified and retired, restoring builder free space from
2,351,308,800 to 4,669,300,736 B. The primary worktree and frozen C13 source
remain. Twelve independently verified OVH executable duplicates are retired,
restoring strict disk margin from 384,950,272 to 612,696,064 B before C14 input
staging. These are owned test/build copies; the full-history database and
operational producer are unchanged. Exact inventories, archives and retirement
proofs are under `evidence/pre-C14-*` in the private run.

Complete normal/sanitizer, paired/profile and exact whole-node mixed-work,
resource and restart/recovery evidence remains pending. M1 remains open.

The complete normal build finishes successfully at **2026-10-21 12:57:22 UTC**.
The interrupted local caller leaves its final metadata incomplete after the
native stage records exit zero. The successful systemd journal, all 71 ELF
outputs and all 471 unchanged source files independently confirm completion.
The metadata is reconciled with its original control preserved; no build or
test is restarted. This preparation reconciliation is separate from native
test outcomes.

All **71 ELF outputs** are now hash-verified on My Passport. The final normal
node SHA-256 is
`71bc0b0773cbd108d4783b39eccf9d16289b9f1f5a7cf4e077d10fdf168d568d`.
The complete native artifact archive SHA-256 is
`e41babd7863c9ee4d3dcb7368e3f104f3c95a5a39bf52e4895e9be4c6ce4f05e`.
Source verification, compiler records, link commands and **167 linked-library
identities** are retained under `evidence/c14/full-normal-build/`.

To overlap compilation with OVH qualification, 19 completed normal ELF targets
are collected for a 21-case core phase. Every one of these hashes matches the
later complete build. Their results can count toward the final 94-entry suite
only with that final identity match; the remaining schedule verifies it before
reusing any result. Version/help and the remaining registered cases have not
yet passed at this checkpoint. ASan/UBSan compilation from the same frozen
source is running separately; none of its final-source tests has executed.

The externally verified normal ELF copies are retired from the builder,
restoring free space from 2,104,385,536 to 3,840,081,920 B while preserving
source, libraries, objects and the separate sanitizer build. A closed C13 OVH
source/evidence mirror is also externally archived and verified before retiring
its duplicate copy, restoring strict margin from 536,936,448 to 612,347,904 B.
Its archive SHA-256 is
`52ac4799e7f0b25b8f73b85eee28655308f2487911113dab67f801aa05c4eea4`.
The historical database is unchanged and no whole-history observer starts.


### C14 normal-suite storage failure remains open

C14 passes **49/94 registered normal tests** and both actual native version/help
checks before `teleno_single_copy_experiment_test` exceeds the strict
whole-server disk budget. The 250-ms sampler observes a minimum margin of
**-26,140,672 B**, including the full unchanged 8-GiB recovery reserve.
The owned test is interrupted (exit 130); no node or test unit remains running.
This is a resource qualification failure, not a sanitizer finding. It must not
be reported as a passing complete suite or successful whole-node qualification.

The experiment retains completed synthetic restore cases across five paired
measurements. Its current case data occupies 510,727,603 logical bytes across
647 archived entries. Every file, link and directory is preserved externally
and verified before its owned remote copy is retired; the archive SHA-256 is
`0b6786101cb2c66784d2fd2b427a0f0ab98f709f90b127098eb5d4a6270f874a`.
Strict margin returns to 452,321,280 B with the current native executables still
staged. The full-history database, all failed evidence and candidate counters
are unchanged. See private `evidence/c14/normal-sc5-disk-failure/`.

The **44 previously unexecuted registered cases** are scheduled as a distinct
continuation; they do not rerun or erase the failed SC5 case. Its guard now
samples every 50 ms with a 128-MiB stop buffer to reduce burst-write overshoot.
Even if those additional cases pass, SC5 and complete normal qualification
remain unmet. The original guard and its failure are preserved.

The final-source ASan/UBSan build is complete, with **22 ELF files** externally
verified (archive SHA-256
`169feddfefeb2a5edf2d296c96b6ff7eb4d9dbaa77c11a6235b47f2f467a479b`).
No final-C14 sanitizer test has yet run at this checkpoint; the separate TSan
build is still running on the build-only miner. All executable qualification
remains OVH-only. M1, representative policy, paired measurements and final
whole-node resource/restart gates remain open.


### Completed C14 normal reconciliation and unfrozen fixture repair

The four phases now reconcile **93 distinct passing registered tests** against
all 94 configured entries, with zero failing/error JUnit cases among those
93. Both real `teleno_node --version` and `--help` pass. The single missing pass
is the preserved SC5 strict-disk failure. The minimum strict margin among
passing cases is **228,212,736 B** and minimum host MemAvailable is
**3,262,717,952 B**; the overall run still includes SC5's **-26,140,672-B**
margin. Passing-phase extrema must not conceal that failure. See private
`evidence/c14/normal-aggregate-reconciliation.json`.

TSan compilation completes at **13:55:15.878908 UTC**, with 22 verified ELF
outputs and 59 linked-library identities, including all five pinned instrumented
gRPC archives. Its artifact archive SHA-256 is
`2a3511129a2291e21dfc1de77f7eb71e3122d2c2c88e5a7503ba0f52967a8cb4`.
Verified ELF duplicates are retired from the builder; source, libraries and
objects remain. No test is executed on that machine. The separate final-source
ASan/UBSan sequence begins on OVH after normal execution has ended, with working
clean/UAF/undefined-behavior controls, including actual gRPC callbacks. Native
sanitizer results are still pending at this checkpoint.

Unfrozen development adds opt-in `--retire-completed-pairs` to the SC5 Python
driver and selects it for CTest. It preserves the warm-up, all five measured
pairs and final restart/recovery checks. Both restore modes must verify before
retirement. Only directory identities created by that run are eligible; source,
logs, rows and immutable per-file hash manifests remain. Mismatched results,
changed identities, symlinks or a failed initial evidence write preserve the
pair. Retirement CPU/I/O/wall costs are reported separately. The ordinary tool
still retains cases by default. It does not remove an existing history database,
backup, failed prior attempt or operational data.

Revision 2 passes **15/15 helper unit tests on OVH**, including refusal and
preservation paths; archive SHA-256
`e378a6d0eaf1bb55f90b585b154c45887ac57785abff9ba18a74ad2a6046874e`.
This is not the full SC5 native experiment and does not clear C14's failure.
Revision 3 adds the Python 3.9/3.10-compatible safe-removal call and a regression
for that branch; its execution remains pending. No additional native candidate
is frozen and the original C13/C14 counters remain unchanged.


### Final-C14 scoped ASan/UBSan reconciliation

The matrix reconciles **54 unique successful invocations**: 43 registered
native cases, five explicit cold modes and six functioning clean/known-fault
controls. There are **55 actual invocations**, including the preserved original
120-second cleanup timeout. The exact same cleanup ELF passes with a distinct
600-second diagnostic liveness projection, completing its unchanged 299 removal
and 3,619 prune fault cases in 208.925 seconds. Assertions, fault counts,
detectors and resource limits are unchanged. This does not rewrite the failed
120-second result or establish a production latency bound.

Minimum successful strict disk margin is **450,068,480 B** and host
MemAvailable **2,870,829,056 B**. Mixed prebuilt dependency exclusions, disabled
vptr instrumentation and the supported gRPC public-layout compatibility define
remain explicit. See private `evidence/c14/ASan-aggregate-reconciliation.json`.
The separate default-TSan matrix begins on OVH with the actual generic and
instrumented-SDK clean/race controls; completion is still pending.

SC5 fixture-retention revision 3 separately passes **16/16 helper unit tests**,
including the Python 3.9-compatible call branch simulated on the actual OVH
Python runtime. This is not execution on Python 3.9, a full SC5 experiment or
a passing C14 resource gate. Historical failures and both frozen counters remain.

### Reproduced HTTP refusal ownership gap and unfrozen repair

A supplemental probe linked to C14's exact normal closure fills ordinary M1
policy credit and submits a valid mempool query with a **131,072-byte ID**.
After an actual production quota exception, the native worker makes **four
untracked large allocation requests totaling 524,420 B**, with no tracked large
request. The response retains its original ID, local retryable classification,
zero refused service calls and a successful later retry. The exact final credit
returns to its warm baseline. Its acceptance control exits **91**, as expected
for this uncorrected baseline. These two expected diagnostic outcomes establish
a missing owner; they are not passing ownership qualification.

The first probe stops at its final credit assertion before emitting counters.
The separately identified revision warms the exact conversion before selecting
the baseline and emits counters before that assertion; the original failure is
retained. Revised fixture SHA-256 is
`a003a8f7ecaaede7a87e3d52b8f9d72397140e8f61ad0d85ab35e81c611571b9`,
ELF SHA-256 `456c756bfac25eed8dabcb05013773bf5a442416d4a0f568af152429b0c1dbe9`.
See private `evidence/c14/http-refusal-ownership-diagnostic-r2/`.

Unfrozen development borrows IDs during refusal encoding, attributes actual
fallback allocations to M1 and admits conservative whole-request permits from
a separate **64-MiB shared workspace gate**. Permits remain until the HTTP body
and batch ownership buffers are released. Gate exhaustion closes the connection
as local overload; it does not execute refused work or report invalid consensus
input. The gate's credits are supplementary policy, not additional physical
allocations to add to M1 counters. Its own state follows a captured parent owner.
The envelope and its physical margins still require Linux qualification; this
is not a selected low-resource profile or a new frozen candidate.

Single/mixed exact-byte, concurrent gate exhaustion/release, subordinate last
owner and allocation-fault regressions are prepared. The normal component
build uses explicitly identified unchanged C14 archives; it cannot qualify a
source-consistent whole node. No new test result is claimed at this checkpoint.


The unfrozen revision-1 component build completes without executing tests on
the miner. Its 560-entry build archive is externally verified on My Passport,
SHA-256 `35da467c2744cc3fb9428708184177c367ae20ab36fea54f2306fd25314cedf4`.
HTTP regression ELF SHA-256 is
`74282b207c9cff9c2224eb985cc1f1423d1c2a439ef64ba7cdf5c425fa441149`;
subordinate-budget regression ELF SHA-256 is
`046604b0458837872ad6f37918c7140ea95f843e856eb2efd8b6463b9be2567f`.
Its normal matrix is prepared to run only after C14's existing sequential TSan
matrix finishes. Separate instrumented repair builds are in progress; no repair
regression execution or qualification is claimed yet.

A fresh read of issue 9 confirms the M1 acceptance text is unchanged (issue
body SHA-256 `5855b178d204d74bb47fc1a2ac42b7fb80b97f1d23d94194ff9fb2e4cd083f1c`).
The checked older restore and finite producer milestones do not qualify these
new implementation identities or close the remaining mixed-work, physical
resource, full-suite and recovery gates. The updated Koinos One manual builds
with strict MkDocs and its rendered page is checked; no submodule, package,
commit, push, release or operational producer changes occur.


### Conversion-boundary diagnostics prepared before another freeze

The unfrozen HTTP refusal repair now has independently compiled normal,
ASan/UBSan and TSan components; every builder execution remains compilation
only. Both instrumented 560-entry build archives are externally verified.
Their SHA-256 values are respectively
`a5aed53e1770fd2ca4f26bb933853c7469caf4748698a7403aa25eaa6c3b2f28` and
`bf3c5e7588f03cfd621f5068702ead1540a9cff055fef32e0764a1e7f057cc16`.
Prepared sequential runners require the existing C14 TSan sequence to end
before normal repair execution; subsequent instrumented phases require their
preceding component phases to pass. A failed stage is preserved without
resubmission. There is no overall goal deadline or new frozen candidate.

Two supplemental ASan diagnostics link the unchanged C14 closure. One injects
quota exceptions after a readonly mempool result exists, covering generic
protobuf-to-JSON conversion. The other sweeps native request staging/parsing,
checks the exact decoded nonce bytes and local failure classification, then
performs a successful retry on the same service before checking final credits.
Each sweep has at most 2,048 ordinals and must reach an unfaulted terminal path.
Neither diagnostic has executed at this checkpoint; source inspection alone
does not establish another leak or a successful qualification.

Output diagnostic fixture/ELF SHA-256:
`5a5564cdfe4e69780cf7aa0e1463c84554c86cbed4c27f43b21c2e63793a2c11` /
`589d80d7952292c7ca4e0b2aa444c8d1b8319845b07b5341cc15099f18781cb0`.
Input diagnostic fixture/ELF SHA-256:
`b4a3ddc5b5aaa9cf13e83a7cb075b38661b358cf0b249fdb0e9424d968c5bd48` /
`a709ac6c675f639d5440ffa9e30717e05d9e1683e54344dffe422faa85cdd8df`.
Private build/retention records are under `evidence/c14/generic-json-*-collection/`.
No operational producer, funding, access or deployment action is performed.


### C14 default TSan completion and mixed-batch refusal repair

All **52/52** final-source scoped default-TSan invocations pass: 43 registered
cases, five explicit cold aggregate cases and four working detector/SDK
controls. The deliberate generic and actual gRPC callback races are detected;
normal controls stay clean. Default deadlock detection remains enabled and
child-only ASLR-off leaves host ASLR unchanged. The minimum sampled strict
whole-server margin is **466,751,488 B**, including the unchanged recovery
reserve; minimum host `MemAvailable` is **3,220,783,104 B**. Dependency and
instrumentation exclusions remain unchanged. See private
`evidence/c14/TSan-aggregate-reconciliation.json`. This does not clear C14's
SC5 failure, HTTP ownership gap or whole-node gates.

The unfrozen revision-1 normal HTTP component passes the large-ID refusal
ownership regression: **zero** untracked large allocation requests, two tracked
large requests, preserved local error/ID, zero refused service calls, successful
retry and exact final credits. Its mixed-shapes regression then aborts with
exit **134**. The sequential component pipeline stops and retains the failure;
no revision-1 sanitizer execution is claimed.

An independently identified terminate-trace executable reproduces the failure
on OVH (expected exit **86**). Its fixed-address symbolization identifies the
JSON-RPC version comparison in `process_single_request`: nlohmann's scalar
comparison with a string literal constructs a temporary JSON string inside a
`noexcept` operator. Exhausted M1 admission throws during that allocation and
terminates the process before the surrounding local-capacity catch can run.
The trace ELF SHA-256 is
`4f8a974b0567e44f4c87c6da78136ca0c452ed70a8c146c9c2ce394da171f169`;
raw and symbolized evidence is preserved in private
`evidence/c14/http-refusal-r1-terminate-trace-diagnostic/`.

Unfrozen revision 2 validates the type and compares the already owned string
by reference at all three version checks, avoiding the temporary allocation.
It also requires a successful same-service retry after each injected refusal
allocation failure. Its source archive SHA-256 is
`b30f4d03bff6fda822bf245ee7f8f0b090cf96dc9edc437e5e70646783d8c4f8`.
Compilation uses explicitly identified unchanged C14 archives; no regression
pass or final whole-node qualification is claimed for revision 2 at this
checkpoint. C13/C14 remain frozen with consumed slots; no deadline or counters
are reset and the operational Hetzner producer remains untouched.


### Further application-memory checks and requested full-node build

Revision 2 passes individual refusal ownership, all six exact single/mixed
ID shapes and the concurrent two-permit/refuse/retry test. Its allocation
sweep then retains **240 B** at ordinal 16. An identified trace locates
partial JSON object initialization: the object constructor has published its
raw map pointer but its own destructor does not run if a later insertion
throws. The build-owned backport now keeps the partial object in a completed
local owner until publication. A new quota-boundary initializer regression
checks values and final credits alongside the existing copy/cleanup checks.

The separate C14 output conversion sweep retains **19,434 B** at ordinal 123.
An exact-large warmed run reproduces the same retained-credit outcome, while
the unforced conversion releases its credits. The trace identifies Protobuf
`TypeInfoForTypeResolver::ResolveTypeUrl`: `type.release()` runs before a
throwing cache insertion. The build-owned pinned-source repair retains the
unique owner until insertion succeeds, with the same correction for enum
publication. No installed dependency is modified. The first input-conversion
fixture fails before injection because HTTP does not expose its selected
`check_account_nonce` method; this is a fixture error, not a native defect.
Its correction uses the existing `get_reserved_account_rc` method and checks
exact input bytes.

Unfrozen revision 3 compiles in normal, ASan/UBSan and TSan modes, with all
three component archives externally verified. These components retain explicit
unchanged-C14 archive exclusions. Its mixed-shape and concurrent-refusal cases
pass. The terminal fault-sweep retry then receives the correct session-limit
response because the one-session fixture keeps its successful original
connection open before opening a second one. The identified diagnostic records
zero injected faults at ordinal 61, exact baseline credits and error -32001;
the fixture now closes/drains the first connection before checking its retry.
This does not establish another application memory defect.

The subsequent revision-3 output-conversion regression exits **139** after 180
ordinals. Its cause and baseline applicability remain under investigation; a
separate ASan diagnostic has working clean/use-after-free/undefined-behavior
controls. Preserve the failure. Do not call the revision qualified or assume
its preceding patch caused the crash without a matching comparison.

At the operator's request, the complete source-consistent
`observer-development-r4` node build starts on the build-only miner from
source archive SHA-256
`b1214898658c2ee1e5e7dcdaf1f98f769608bcaa658d846d1b636f2b8a8b9766`.
Configuration passes and compilation is running. Tests and the later mainnet
observer belong exclusively on OVH; no new observer or producer has started.
This development build is not a new frozen source candidate or a successful
M1 result. See the [observer plan](../implementation/issue-9-m1-mainnet-observer.md).

### Source-consistent Linux builds and input-conversion ownership

The R4 complete Linux node build succeeds and its 9,001-entry successor build
tree is externally archived and hash verified. R4 node SHA-256 is
`3ec8e9dd18406842c5d1e74b948246f111821dee600b9f8e8d9d5bb5294c0bbf`.
The R6 full node rebuild also succeeds without executing tests on the builder.
Its source archive is
`e3f51eb0e6819012b8437eb03f07fa3fb97c5de18eff92ce32a71c4af9015a5a`
and its node SHA-256 is
`1b456affbca5edeea78b3524d5c36e7c38527b7003a6225851c56a0e5bc88364`.
The 9,001 R6 build entries and 167 linked-library identities are externally
verified under private `evidence/observer-r6-full-build/`. Neither binary has
run as an OVH observer.

Five R6 normal HTTP component cases pass. The input JSON-to-Protobuf
allocation-failure sweep fails at ordinal 55: a refused conversion leaves
400 B attributed after server stop, despite a correct local-overload response,
unchanged native input and a successful retry. The same failure reproduces
under ASan/UBSan with working clean, use-after-free and undefined-behavior
controls. Preserve both failures; R6 is not qualified. Evidence is under
private `evidence/development-http-refusal-r6-normal/` and
`evidence/development-http-refusal-r6-asan-input-diagnostic/`.

A separate diagnostic ELF traces the sole retained request to a 208-byte
allocation in `ProtoWriter`'s `TypeInfo::NewTypeInfo` initializer. Its
constructor previously acquired that raw pointer before later owned members
finished allocation; an exception constructing those members skipped the
constructor's destructor. The requested payload plus accounting overhead is
400 B. The trace ELF is
`26a45b98177c2af6652561b596f0734eea870c223d6d9a4d07792f7920d0d05d`;
its raw stack and exact source-symbol interpretation are preserved under
private `evidence/development-input-trace-r7-ovh/`.

The build-owned pinned Protobuf patch now initializes its existing owned
members first and creates the `TypeInfo` as the final constructor-body action.
It leaves public headers and wire data unchanged. The R8 component ELF
`60035c9e7889e3ab08ae2b616cb173473951c59e297960634d69290426c79db1`
passes **131 injected allocation failures plus the unfaulted terminal case**:
exact final credits, no wrong native inputs and successful retry. Its private
`evidence/development-proto-writer-r8-ovh/` record contains the sampled disk
and memory guards. This is a component result against unchanged identified R6
objects; a new complete source-consistent Linux binary, native/sanitizer suites
and OVH observer qualification remain required. The Hetzner producer is
unchanged.

The six-file pinned Protobuf patch recipe passes **7/7** registered checks on
OVH after correcting the test fixture's CMake path to the already staged,
owned CMake binary. The first path failure and its corrected result are both
preserved under private `evidence/development-proto-recipe-r9-ovh/` and
`evidence/development-proto-recipe-r9-corrected/`; no native node was run by
that recipe test.

The R9 complete Linux node build succeeds from source archive SHA-256
`8f7f9e37190be31cc6b540c3f3ed775e8bb01b612cd8446ade56c152d4418e02`.
Its node SHA-256 is
`411928cb47cd9b1e5a2db11ae16cbfa9f9fca4f50704df73144ab1e4310a08ab`.
All **12,131** build entries and **167** linked-library identities were
externally archived and hash verified on My Passport before further build-tree
changes. No test or CLI ran on the builder. This identifies a complete binary,
not a qualified observer: R9 native/sanitizer execution and whole-node OVH
checks remain pending. The private preservation record is
`evidence/observer-r9-full-build/preservation.json`.

OVH now holds a hash-verified R9 source and CTest metadata stage for **101**
registered normal tests; no native test has yet run from that stage. The exact
R9 binary passes `--version` and `--help` on OVH under isolated, no-swap
resource controls. Its staged CLI copy was hash verified against My Passport
and retired to preserve the strict disk margin. Evidence is under private
`evidence/observer-r9-test-metadata/`,
`evidence/observer-r9-ovh-test-stage/` and
`evidence/observer-r9-cli-ovh/`. The full native test-executable build is
still running on the build-only miner; these CLI checks do not establish
observer readiness or M1 completion.

R9's final normal build produces **586** ELF files from the identified source;
all **12,348** build-tree entries and extracted ELF hashes are externally
verified. The exact node SHA-256 remains
`411928cb47cd9b1e5a2db11ae16cbfa9f9fca4f50704df73144ab1e4310a08ab`.
After external verification and a process-reference check, the owned builder
copy was retired to restore compilation headroom. No operational builder
service was changed.

The exact R9 build passes **101/101 unique registered normal tests** and
**2/2 native version/help checks on OVH**. The failed setup attempts remain
separate: SC5 first encountered a relocated CMake cache and compiled source
path, then the 128-MiB extra disk stop buffer; two recipe tests initially
lacked build-matching dependency files. Each was preserved, the isolated test
stage was corrected from hash-verified build inputs, and only the affected
case was rerun. The final SC5 run passes with a 237,129,728-byte minimum
strict disk margin. Across all passing normal cases, the minimum strict
margin is **163,143,680 bytes** including the full recovery reserve, and the
minimum sampled host `MemAvailable` is **3,143,913,472 bytes**. These are
component/fixture observations, not whole-node qualification. The full
reconciliation is private `evidence/observer-r9-normal-reconciliation.json`.
ASan/UBSan and TSan final-source builds and OVH execution, paired whole-node
measurements, and the mainnet observer run remain pending. M1 is open; the
Hetzner producer remains untouched.

R9's focused final-source ASan/UBSan build yields 22 hash-verified ELF files
and 59 identified linked libraries. Its complete 5,775-entry builder tree was
archived and verified on My Passport before only that owned duplicate was
retired. The OVH matrix passes **61/61 invocations**: six working clean/fault
controls, 50 registered cases, and five explicit admission, expiry, block,
LIB, and concurrency sweeps. The aggregate fault-sweep and cleanup tests
complete in 427.800 and 223.446 seconds respectively. The latter uses a
recorded 600-second CTest diagnostic allowance in place of the original
120-second timeout; its assertions and detector remain unchanged. Minimum
sampled strict disk margin is 401,952,768 B including the full reserve;
minimum host `MemAvailable` is 2,883,125,248 B. No resource guard fires.
Private evidence is under `evidence/observer-r9-asan-build-preservation/` and
`evidence/observer-r9-sanitizer-asan/`.

The separate final-source TSan build yields 22 hash-verified ELF files and
59 identified linked libraries, including the pinned instrumented gRPC
archives. Its complete 5,776-entry builder tree was externally verified and
only that owned duplicate retired. TSan execution on OVH is underway; the
ASan results do not qualify TSan, the full node, or M1. Private build evidence
is under `evidence/observer-r9-tsan-build-preservation/`.

The first R9 TSan run preserves 22 passing invocations, including working
clean/known-race controls and the aggregate-memory test. Invocation 23,
`koinos_mempool_cleanup_atomicity_test`, hit its original 120-second CTest
timeout (status 8) with no resource guard or detector signal. Its evidence and
failed result remain under `evidence/observer-r9-sanitizer-tsan/`; the exact
staged ELF was retired only after independent hash verification. A distinct
same-source continuation is running with a recorded 600-second test-only
allowance for the unchanged cleanup fault sweep. Later TSan cases and the
whole-node observer remain unqualified.

The same-source TSan cleanup diagnostic passes the unchanged native test with
the 600-second CTest allowance. Its artifact helper rejected the diagnostic
label after the test succeeded. The generated case data was then externally
archived and hash-verified using a versioned helper that accepts that exact
label, before retirement. This preparation failure and rescue remain separate
from the native result. A new run covers the 36 remaining TSan cases; no
complete TSan or observer qualification is claimed yet.

The remaining 36 TSan cases pass. Reconciliation records **59/59 unique
successful cases across 60 invocations**, including the preserved original
120-second timeout and separate artifact-label preparation failure. The
minimum passing-case strict disk margin is **418,795,520 B** including the
full reserve, and the minimum sampled `MemAvailable` is **3,221,651,456 B**.
This is scoped sanitizer evidence; all generated case copies were externally
verified before retirement. The exact R9 node later failed on its first OVH
observer start because its linked Hunter RocksDB lacks the configured Zstd
codec. It exited in about one second, with no OOM, swap or disk guard event.
The first-start evidence is private under `evidence/observer-r9-first-start-failure/`.
R9 is not an operational observer or a completed M1 candidate.

The corrected-dependency R10 build retains R9's exact source archive SHA-256
`8f7f9e37190be31cc6b540c3f3ed775e8bb01b612cd8446ade56c152d4418e02`
and links the separately verified Zstd-enabled RocksDB and static Zstd
libraries. Its node SHA-256 is
`00c6f3c4a87d7ff01ae58eda41a3ed4713c80f1bdb19b7aa8ad52617bd0f9f6f`.
The 6,089-entry build tree and 414 ELF outputs were externally archived and
hash-verified. The builder executed zero tests and zero CLI commands.

On isolated OVH, R10 passes **101/101 registered normal tests and 2/2 native
version/help checks**. An initial recipe test failed because the isolated
stage lacked the new build-owned Protobuf files; a separately verified
50-file Protobuf/JSON input stage corrected it without changing source or
assertions. The archive build prints `1.3.0-dev.1+unknown`; the first CLI
check's older Git-label expectation failed and was preserved, then the exact
output and help surface passed. Reconciliation records a **154,886,144 B**
minimum strict disk margin including the reserve and **3,200,397,312 B**
minimum sampled host `MemAvailable`. These are scoped test measurements.

R10's read-only preflight passes, and its first isolated OVH mainnet observer
start selects `zstd` for both RocksDB profiles. The producer is disabled.
The node caught up to the independent public head at 03:24:46 UTC on
2026-10-22, and the common irreversible block ID agrees. Its first mixed-read
window preserved a reference-only failure after 1,261 correct local cycles:
the rate-limited public API timed out three consecutive times after 30
successful canonical comparisons. A separate, same-binary one-hour continuation
passed 3,573 local read cycles without error, with 15 independent canonical
agreements and 15 public-reference availability errors. The observer stopped
cleanly, reopened the same database, advanced 92 blocks from the first reopened
proof and passed three further independent checks before another clean stop.
Peak sampled RSS was 627,982,336 B in the first run and 109,805,568 B in the
reopen; the lowest strict disk margin including reserve was 495,771,648 B.
Both owned units are inactive, and all 32 runtime evidence files are externally
archived and hash verified. See the
[observer work record](../implementation/issue-9-m1-mainnet-observer.md).
R9 sanitizers use the same source but a different RocksDB dependency closure;
they do not establish an exact R10 sanitizer result. Both R10 exact-closure
focused sanitizer builds are externally hash verified. ASan/UBSan passes
**61/61** OVH invocations, including working clean/known-fault controls,
aggregate admission/expiry/block/LIB/concurrency fault cases and JSON/RPC
failure recovery. Its minimum sampled strict disk margin is 1,131,679,744 B,
minimum host `MemAvailable` is 2,872,823,808 B and peak cgroup memory is
573,571,072 B. At that checkpoint, exact-closure TSan execution continued
separately; its final result is recorded below. The
prebuilt SDK and disabled libp2p in the focused closure remain instrumentation
limits. M1 remains open.

The first exact R10 TSan sequence passed 22 invocations, including its working
race controls and aggregate-memory case. The next unchanged cleanup fault
sweep hit its original 120-second CTest timeout (status 8), without a detector
or resource-guard signal. The case output and exact binary are externally
verified; only its owned staged copy was retired. A separate same-binary
diagnostic continuation projects 600 seconds for the identical test and then
covers the remaining cases. The original timeout remains failed evidence.

That cleanup case passed in 155.78 seconds, but the subsequent gRPC server
case reported a `grpc_chttp2_transport` race. Build-link inspection found the
R10 TSan build had selected ordinary uninstrumented Hunter gRPC archives
because the pinned remap condition named the earlier R9 source path. This
failed result remains preserved. A corrected build-only TSan closure retains
the exact R10 source and Zstd RocksDB, pins five instrumented gRPC archives,
and is externally hash verified (archive SHA-256
`2d392d6c60f5face8758904635e0480921fbc0489910bfbb918f137e4bd7d242`).
Its complete focused OVH matrix passes **59/59** invocations, including the
working detector controls, gRPC server, aggregate-memory and cleanup cases.
The unchanged cleanup uses a documented 600-second test-only allowance.
Minimum sampled strict disk margin is 1,139,589,120 B after full reserve and
minimum `MemAvailable` is 3,189,100,544 B; no resource guard fired. Other
prebuilt libraries and disabled libp2p remain instrumentation exclusions.

The R10 Linux build-and-observer qualification is complete: 101/101 normal
tests, both CLI checks, 61/61 focused ASan/UBSan, 59/59 focused TSan, an
advancing one-hour mainnet observer mixed-read window with independent
canonical comparisons, and a clean same-database reopen. The private
`evidence/observer-r10-final-reconciliation.json` links exact identities and
preserved failed results. This does not close M1: lifetime-owned aggregate
accounting/admission for remaining consumers, representative policy and
physical-memory comparison, and issue-9 whole-node gates remain open.

### R11 decoded-query copy reduction: development checkpoint

The 2026-10-22 closeout run first reconciled the dirty native source against
R10: code and tests were byte-identical; seven documentation files differed.
R10's exact normal mempool-test ELF
`bada17e442463e2b409da922e53b96afb846787e65d324188f5373049a64e9e0`
was measured on isolated OVH before the source change. Five fresh-process
`--measure-query` samples each admitted 64 8-KiB-signature transactions,
retained two decoded query results, expired records and advanced LIB. Median
peak RSS was 19,234,816 B, with 198,059 allocation requests. These are
component measurements and include admission and cleanup as well as queries.

R10 decoded a stored pending record and then copied its nested transaction
into the returned vector in both native query overloads. R11 moves that
temporary's transaction into the lifetime-owned vector. It also extends the
large-message wire-parity regression to selected-ID queries and sweeps
allocation failures and later retry on both query overloads. R11 development
source archive SHA-256 is
`c9cc9b2defb4818fcaff0e4d900ff66f95e36dade08130cbe7327e4591823c7e`;
the build-only miner compiled its component ELF
`d357387665dcca724721a506b5040d2547d30e6b64fc498e7911d055c3a5e303`
without executing tests or CLI commands. The complete normal aggregate-memory
regression passed on OVH, including 839 allocation-failure ordinals for the
limit query, 80 for selected IDs, concurrent/fork cases and the existing
admission/expiry/block/LIB sweeps. This is not a full native suite or a final
source sanitizer qualification.

Five alternating R10/R11 OVH process pairs with the same workload show 1,280
fewer allocation requests and 1,102,748 fewer requested allocation bytes at
the median for R11. Retained and peak aggregate attribution are unchanged in
this fixture. Median peak RSS is **155,648 B higher** for R11, and is higher
in every pair (155,648–458,752 B). The median measured CPU time is about
34–35 ms for both; elapsed-time differences change sign across pairs, and
filesystem input/output blocks are zero. This is a negative physical-memory
result, not a demonstrated RSS saving. Binary layout or allocator reuse may
contribute, but neither is established as the cause. The source change remains
development-only pending stronger representative and whole-node measurement.
All raw samples, exact command outputs, source hashes and builder logs are
preserved under
`/Volumes/My Passport/teleno-m1-closeout-20261022-opl9t4k0/`.
The [working ownership inventory](../implementation/issue-9-m1-owned-memory-inventory.md)
records additional M1 consumers and evidence gaps. No OVH observer was
started by R11; the Hetzner producer was untouched. **M1 remains open.**

The complete R11 Linux normal build subsequently succeeded without test or
CLI execution on the miner. Its 71 native ELF outputs and configured CTest
metadata were externally hash-verified; the node SHA-256 is
`333fdbf40e5cc299036d3f994cbb12154afc453616bf00530b9e9f3cf992ade9`.
Version and help passed on OVH after preserving an initial harness-only text
expectation failure. The 101-case registered normal suite is running on OVH;
an initially missing build-owned Protobuf recipe input was reproduced,
verified against the unchanged miner dependency and staged before a passing
retry. This is not yet a reconciled 101/101 result.

An unfrozen follow-up replaces a vector-plus-string nonce-key construction
with one directly reserved string in admission and native account queries.
The R11 source is the identified baseline; binary-key parity, allocation
behavior, sanitizer and physical-memory evidence for this change are pending.

R12 subsequently froze that direct nonce-key construction and a payer/payee
binary-key parity regression. Its source archive is
`53a4e6b6e10889d8f08a55072e34aae34b362d1e9f2a4e56b0f3afcdc0323500`.
The miner compiled the complete normal native closure without executing tests;
71 ELF outputs and the configured CTest metadata were externally verified.
The exact R12 normal node SHA-256 is
`31dac5e65fada62b800506cef76d4f9d21cbbd6aed6d87b67e8576e20cd50c5a`.
Its 22-target focused ASan/UBSan build is still running on the build-only
miner. The exact normal component ELF was hash-verified and staged on OVH,
without executing it yet. R11's normal suite is still running separately;
R12's normal, sanitizer, paired-profile and observer qualification remain
pending. This checkpoint is not evidence of M1 completion.

R11's suite subsequently stopped at registered case 51 after 49 passing
cases. The baseline-restore harness rejected the OVH exported-source manifest
because the staging helper omitted its required `version: 1` field. The
failed JUnit, stdout, command and stage are preserved; resource guards did
not fire. This is a test-stage preparation failure, not evidence of a native
regression or an R11 full-suite pass. R12 staging corrects that field before
its final-candidate suite. The completed R12 focused ASan/UBSan build has 22
ELF outputs and ran no tests on the miner; its entire build tree is externally
hash-verified before exact owned-copy retirement. TSan compilation follows.

R12's focused normal mempool regression passes on OVH. Five alternating
R11/R12 pairs for each of the same admission and admission-plus-query inputs
show **768 fewer allocation requests and 48,990 fewer requested allocation
bytes** per R12 process. Final and peak aggregate policy attribution are
unchanged. The admission-only median peak RSS is unchanged at 17,240,064 B;
the query-workload median peak RSS rises by 618,496 B, from 17,793,024 to
18,411,520 B. Median CPU time and elapsed time are close, with query elapsed
time 1.44 ms higher for R12; both profiles report zero filesystem input/output
blocks. This is a scoped reduction of avoidable allocations, **not a proven
physical-memory saving**. All 20 exact process samples and their remote/local
file hashes are preserved in the owned run's `evidence/r12-paired-profiles/`.
The whole-node comparison and selected positive policy remain open.

| Paired process workload | Candidate | Median elapsed | Median user/system CPU | Median peak RSS | Median peak aggregate attribution | Median filesystem I/O blocks |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Admission | R11 | 27.209 ms | 24.496/2.800 ms | 17,240,064 B | 4,024,200 B | 0/0 |
| Admission | R12 | 27.501 ms | 24.276/3.313 ms | 17,240,064 B | 4,024,200 B | 0/0 |
| Admission plus decoded query | R11 | 36.575 ms | 31.599/3.527 ms | 17,793,024 B | 5,382,114 B | 0/0 |
| Admission plus decoded query | R12 | 38.015 ms | 29.970/4.533 ms | 18,411,520 B | 5,382,114 B | 0/0 |

Each row is the median of five fresh OVH processes. Process peaks and the
aggregate policy counter measure different quantities; none of these samples
is a steady full-node RSS measurement.
Both candidates retain the same 1,028 indexed entries and 542,826 B of
distinct indexed value capacities at the admitted snapshot. After cleanup,
both retain one map entry, 256 tombstones and zero pending-record owners.
The final aggregate counter is 156,599 B in admission-only processes and
1,514,513 B while the decoded query owners remain held. These are attributed
capacities and credits, not allocator-heap measurements.

R12's first full-suite launch stopped before any case because its local
preflight expected a helper copy that had not been staged locally. A second
launch passed the first case but failed artifact preservation because its
helper still pointed at R10's directory; that passed case's actual files were
subsequently hash-verified, externally preserved and retired with the
corrected helper. The next launch passed that case and retained a failed
Protobuf recipe case because 50 pinned build inputs were missing from the
OVH test stage. Those inputs were verified against R12's externally preserved
builder archive, staged without test execution, and the affected cases now
pass. The source, tests and exact ELF identities did not change. The
remaining 100-case continuation is running; every initial failure remains
separate evidence rather than being counted as a pass.

The R12 focused TSan build subsequently completed with 22 ELF outputs and
zero test/CLI executions on the miner. Its 5,774-entry build tree and all
binary hashes were externally verified on My Passport before the exact
builder copy was retired. The normal build tree was restored from its verified
archive solely to compile a supplemental controlled-chain mixed-transaction
fixture; that fixture must execute only on OVH. The 100-case normal
continuation remains a separate, nonoverlapping OVH execution. Neither the
verified sanitizer builds nor the paired component profiles close M1.

The R12 normal continuation then passed **100/100** cases. Its previously
preserved first passing case and all 100 externally hash-verified JUnit/case
archives reconcile to **101/101 unique registered normal passes**. The exact
R12 Linux node SHA-256
`31dac5e65fada62b800506cef76d4f9d21cbbd6aed6d87b67e8576e20cd50c5a`
also passes `--version` and `--help` on OVH (2/2); no test or CLI command ran
on the miner. The before-CLI strict whole-server margin including the reserve
was 1,011,339,264 B, with 3,393,282,048 B host `MemAvailable`. The original
R11 and R12 staging/preparation failures remain separate preserved evidence.
Focused final-source sanitizer execution, controlled-chain mixed work and the
final observer are still required before M1 can close.

The exact frozen-R12 focused ASan/UBSan matrix then passed **61/61 OVH
invocations**: six working clean/known-fault detector and SDK controls, all
50 registered M1 cases, and five explicit cold fault/concurrency cases. The
source archive is
`53a4e6b6e10889d8f08a55072e34aae34b362d1e9f2a4e56b0f3afcdc0323500`.
Every case result and artifact archive was externally hash-verified on My
Passport before the owned OVH case data was retired. Minimum sampled strict
whole-server disk margin after the full reserve was 905,527,296 B; minimum
host `MemAvailable` was 3,257,004,032 B. These are scoped instrumented
components, not a whole-node memory result. Prebuilt RocksDB, Zstd, Boost,
libssh, OpenSSL and other SDK libraries are uninstrumented; libp2p is disabled
in this closure. The gRPC ASan compatibility define preserves the prebuilt
ABI while consumer compilation remains instrumented, and mixed-RTTI vptr
checks are excluded. The subsequent default-TSan and integrated results are
recorded below. Evidence:
`/Volumes/My Passport/teleno-m1-closeout-20261022-opl9t4k0/evidence/r12-exact-sanitizer-asan/control.json`.

The exact frozen-R12 default-TSan closure now reconciles **59/59 unique
passing OVH invocations** with clean and known-fault detector/SDK controls.
The first run passed 22 cases and retained an unchanged cleanup case that
exceeded its original 120-second CTest allowance. A first continuation ran
that native case successfully in 156.09 seconds, but its evidence helper
rejected the continuation label; the successful native output and the helper
failure are separately preserved. A corrected continuation reran the
unchanged cleanup case with a recorded 600-second allowance and passed all
37 remaining cases. Its generated artifacts were hash-verified externally
before retirement. The original timeout is still a failed invocation; the
59/59 figure counts each unique case once across the original prefix and the
final continuation. Evidence and reconciliation:
`/Volumes/My Passport/teleno-m1-closeout-20261022-opl9t4k0/evidence/r12-tsan-composite-reconciliation.json`.
Prebuilt non-gRPC dependencies remain outside TSan instrumentation; the pinned
gRPC closure is instrumented, and libp2p is disabled in this scoped matrix.

The separate R12 controlled-chain workload on OVH completed **100 blocks
containing 12,800 deterministic locally signed test transactions**, with a
concurrent native mempool query worker, 16 caller-held decoded records per
block and no network adapter or key file. It reopened the same isolated
database and verified the final block ID, state root and SHA-256 of all
stored block/receipt records against the creation phase. The peak kernel
RSS was 734,810,112 B during creation and 478,580,736 B on reopen;
minimum host `MemAvailable` was 2,714,001,408 B and the minimum strict
whole-server disk margin after the recovery reserve was 841,723,904 B.
The shared M1 counter peaked at 564,747,832 B under the configured
805,306,368-B local limit, with zero local refusals. End-of-block
attribution rose to 497,077,847 B by block 60 and stayed near 497 MB through
block 100; the controlled chain retained 60 reversible blocks (head 100,
LIB 40). The reopened process attributed 535,820 B at the final checkpoint.
Those counters describe attributed ownership, not a process heap ceiling.
Creation took 17:21.02 wall time, 1,524.61 s user CPU and 217.32 s
system CPU; its sampled process write I/O reached 772,554,752 B with no
sampled physical reads. Controlled block-production latency was 2.221 s
median, 3.272 s p95 and 3.436 s p99. The clean reopen took 9.20 s wall
time. These are absolute workload costs without a paired whole-node
baseline, so no CPU, I/O or latency improvement is attributed to R12.
Neither phase used swap or triggered an OOM event. The test uses only
deterministic local test signatures, explicitly authorized for OVH; it
does not use operational keys or submit mainnet transactions. Its
source/binary identities, measurements and 43-item externally verified
archive are under
`/Volumes/My Passport/teleno-m1-closeout-20261022-opl9t4k0/evidence/r12-controlled-mixed-preservation/`.
This controlled process is not a whole-node RSS result.

The exact R12 full-history mainnet observer has separately started on
isolated OVH with the block producer disabled. Native preflight passed;
it caught up from a 12,269-block lag to the public head and matched the
independent common irreversible block ID and receipt. A one-hour mixed-read
observation began at 2026-10-22 16:30:45 UTC. Its final result, clean
restart, interrupted-test recovery and complete M1 decision were pending at
that checkpoint.
The operational Hetzner producer remains untouched.

R12's first full-history OVH observer phase now passes and stops cleanly. The
exact one-hour mixed-read window completed 3,597 local cycles with zero local
errors, 31 independent canonical comparisons with zero unavailable results,
and head advancement from 40,429,436 to 40,430,637. Median local cycle latency
was 0.01238 s and p95 was 0.01987 s. The complete observer phase ran
4,227.40 s, with 717.09 s user CPU and 15.01 s system CPU. Kernel peak RSS
was 427,888,640 B; sampled peak RSS was 428,105,728 B. Minimum concurrent
host `MemAvailable` was 2,992,660,480 B and minimum strict whole-server disk
margin after the full recovery reserve was 646,123,520 B. Process input/output
I/O was 825,824/565,856 512-byte blocks. There was no OOM or swap use. The
owned process exited zero with a native clean-shutdown marker and no test
producer start. A clean same-database R12 observer reopen has begun; its
result and interrupted recovery remain separate pending gates.

That clean R12 reopen subsequently passed after 313.46 s and three independent
canonical checks. The local and reference heads were equal at each check, and
the local head advanced from 40,430,660 to 40,430,739. It stopped cleanly with
96,428,032-B kernel peak RSS, 3,363,622,912-B minimum host `MemAvailable`, and
694,857,728-B minimum strict disk margin. The two-phase observer evidence (27
files) is externally hash-verified on My Passport at
`evidence/r12-observer-runtime-preservation/`; its archive SHA-256 is
`305e7a3131db5d53e9ee11918a65857867381cb623ae1f21ab2a24bc5c585ae9`.

A separate owned-observer interruption test then deliberately sent SIGKILL only
to its isolated third unit after two canonical proofs and head advancement.
The first public-reference query in that phase was transiently unavailable;
the next two proofs agreed and no native failure was observed. The fourth
observer reopened the same full-history database, logged recovery indexing of
60 blocks in 2.09 s, and again matched the independent public head at three
checks while advancing from 40,430,838 to 40,430,914. It ran 326.48 s,
stopped cleanly and passed the resource guards: 92,098,560-B kernel peak RSS,
3,361,865,728-B minimum `MemAvailable`, 786,354,176-B minimum strict disk
margin, zero OOM and zero swap. All 43 runtime evidence files are externally
hash-verified under `evidence/r12-observer-interruption-recovery-preservation/`
with archive SHA-256
`9970303424954c9d5055003dddbe8f5940fd7ef9c4a332038702b6c993df1230`.
The earlier clean two-start qualification remains distinct from this additional
interruption/recovery sequence. All four owned observer starts ended; no OVH
node or producer remains running.

Five alternating OVH process pairs compare the identified pre-R7 R6 HTTP
input/receipt baseline with the exact frozen-R12 normal HTTP test executable.
Both receive the same warm request, 262,144-byte transaction argument and
fake receipt, and both return the expected response with final credits
released. R6 median peak RSS is 31,064,064 B versus R12's 21,553,152 B, a
9,510,912-B (30.6%) lower process peak. Every pair has lower R12 RSS. Median
service-entry M1 attribution attributable to the request falls from
1,019,389 B to 281,276 B; this is a counter reduction, separate from the
physical RSS result. Median CPU time falls from 0.18 s to 0.02 s, median wall
time from 0.19 s to 0.03 s, and both report zero filesystem I/O blocks. These
are a scoped process comparison across several source/dependency/build changes,
not an attribution of the physical or CPU difference to the R11/R12 copy
edits and not a whole-node saving. All ten exact process records, binary
identities and host guards are in `evidence/r12-final-input-ownership-pairs/`.
The separate direct R11/R12 comparison still shows no lower peak RSS.

# R13: avoid redundant native block-notification copies

Frozen R13 source archive SHA-256:
`839e77930e20600cb6fdf143a4bf259448ad076c426f5ff4e14b1f0ab368c684`.
Its exact normal Linux `teleno_node` SHA-256 is
`c3205142024a1911244c69bfd2d3a9708f67b2fd53b5f96d3bd6f3c8bf5f5710`.
The authorized miner compiled all 71 normal ELF outputs and 22 focused outputs
for each of ASan/UBSan and TSan without executing an M1 test or CLI command.
The complete R13 normal suite passes **101/101** registered checks on isolated
OVH, and the exact node passes `--version` and `--help` there. The first
normal-suite attempt stopped at its Protobuf dependency-recipe case because
50 pinned inputs had not yet been staged; that preparation failure remains
preserved. After exact-input staging, the other 100 cases passed. The
reconciliation is `evidence/r13-normal-reconciliation.json` in the private
R13 run directory.

Focused R13 ASan/UBSan passes **61/61** unique OVH invocations, including
working clean and known-fault controls, the aggregate failure/concurrency
cases, and externally verified per-case evidence. Its first launch stopped before
native execution because a copied case helper still expected the old build
directory name. The next launch passed its first native control but failed
while preserving it because a second helper still expected the old artifact
label and build directory. Both preparation failures are retained. That
control's result and generated files were then externally verified and
preserved; a separately labeled continuation completed with the same R13
binaries. Neither preparation error is counted as a successful sanitizer
invocation. `evidence/r13-asan-reconciliation.json` records the distinct
outcomes and hashes. Default TSan passes **59/59** unique cases using the same
source archive and focused binaries. Its original cleanup test reached the
CTest 120-second timeout with no reported race or resource failure; a separately
labeled unchanged-binary continuation passed that case with a 600-second CTest
allowance and completed the remaining 36 cases. The original failed result,
the first continuation's local Python-version preparation failure, the clean
and known-fault controls, and the final 59-case reconciliation remain distinct
under `evidence/r13-tsan-composite-reconciliation.json`. Prebuilt non-gRPC
libraries remain outside this focused sanitizer instrumentation.

The R12 baseline synchronously serialized and reparsed each already-built
`block_accepted` protobuf before dispatching to the monolith's `EventBus`.
A 262,162-byte synthetic event, delivered 256 times per fresh process, produced
1,536 C++ allocation requests and about 85 ms median dispatch time in five
alternating OVH pairs. R13 dispatches the const protobuf directly and retains
the serialized-topic fallback on `IRpcClient`. Its focused native regression
passes: the fallback bytes and topic match, the native subscriber receives the
same live object, and subscriber exceptions still propagate. The R13 component
has zero measured allocation requests and 2.419 ms median dispatch time for
the same 256 events. Median process peak RSS is 8,396,800 B for R12 and
8,359,936 B for R13; pairwise differences vary, so no physical-memory saving
is claimed. Both fixture processes report zero filesystem input/output blocks.
Exact process samples, component binaries, build logs and source identities
are preserved under
`/Volumes/My Passport/teleno-m1-block-event-20261022-UITt1Wk0/`.

This optimization reduces temporary application work outside the retained M1
mempool owner domain. It neither expands nor proves the aggregate policy's
coverage. R12's full-node observations are historical checkpoints, not R13
qualification. M1 and issue 9 remain open.

The exact R13 controlled fixture binary (SHA-256
`ef237fd3e917cb1fb3c2ba168880c3e9ed2b6df245bf928938931d9927b845c5`)
passes an isolated 100-block, 12,800-transaction native mempool/producer
workload and a subsequent reopen on OVH. It uses deterministic local test
signatures, zero network adapters and zero key files. The reopen loads a
40-block checkpoint, replays 60 blocks, and matches the created head ID,
state root and archive digest while checking all 100 blocks and their receipts.
The aggregate attribution peaks at 564,456,464 B in creation against the
805,306,368-B configured limit, without admission refusal. The highest
sampled process HWM is 720,809,984 B; minimum host `MemAvailable` is
2,700,333,056 B and the minimum strict disk margin, after the full 8-GiB
reserve, is 498,278,400 B. Creation and reopen evidence and the complete
29-file controlled state are hash-verified on My Passport. The first launch
failed before native execution due to a fixture-directory name mismatch;
the later creation passed, but its same-run reopen did not start because its
preventive 512-MiB free-margin preflight failed. Both outcomes are preserved.
Only the same-state reopen was repeated after retiring a separately verified
101,721,928-B duplicate node input. The composite result is
`evidence/r13-controlled-mixed-composite.json` in the private R13 run.

The exact R13 node (SHA-256
`c3205142024a1911244c69bfd2d3a9708f67b2fd53b5f96d3c8bf5f5710`)
then passed its read-only preflight and started as a non-producing full-history
mainnet observer on isolated OVH. An independent reference check found
canonical irreversible agreement while it caught up from 4,668 blocks behind
to the same head height, 40,435,716, on 2026-10-22. The one-hour mixed-read
window passes in 3,600.666 seconds with 3,599 successful cycles, zero failed
cycles, 31 successful independent comparisons and zero unknown comparisons.
Local head advances throughout. Median and p95 mixed-read cycle latency are
12.365 and 17.536 ms. The first observer stops natively after 3,861 seconds
with a 369,373,184-B kernel peak RSS, at least 2,960,805,888 B of host
`MemAvailable` and at least 563,007,488 B of strict disk margin after the
full recovery reserve. Its exact executable and config remain hash pinned;
block production stays disabled.

The same database then reopens under the same R13 binary. In 323.544 seconds
the second observer advances from head 40,436,953 to 40,437,040, with
repeated independent irreversible agreement and a clean native shutdown.
The first two phases are preserved as an externally hash-verified 23-file
archive. A third owned observer reaches canonical agreement, then receives
the planned SIGKILL only after its process identity, non-producing arguments,
advancing head and resource samples are checked. No native node remains after
that interruption. The fourth observer recovers the same database, advances
from 40,437,118 to 40,437,181 with three independent canonical proofs, and
stops cleanly after 312.518 seconds. The four-phase record is externally
verified as a 39-file archive; all four starts are observers and there are
zero producer starts.

Across these observer phases the maximum kernel or sampled HWM is
369,373,184 B, the minimum host `MemAvailable` is 2,960,805,888 B and the
minimum strict disk margin is 548,851,712 B after the 8-GiB reserve. There
is no sampled OOM or swap use. The three clean phases consume 417.030 s user
and 13.394 s system CPU, 365,008 physical input and 499,984 physical output
512-byte blocks. The exact reconciled native, controlled and observer result
is private `evidence/r13-runtime-reconciliation-v2.json`; the earlier
sample-only margin report is retained as superseded, and both preserved
observer archives retain their original hashes. This qualifies R13's scoped
runtime gates, not all M1 ownership/admission or an attributable whole-node
RSS saving. M1 remains open for final-owner review and representative policy.

The R13 source review narrows one unfinished policy question. Under a full
ordinary aggregate quota, gRPC creates a per-call refusal state, registry
entry, interceptor and minimum status workspace in a counted but protected
scope. The existing ordinary/fault and two-call cancellation checks establish
final release, but no test or explicit transport bound limits the aggregate
of many simultaneous protected refusals. The one-call R10 status control
records 1,052,927 attributed bytes at a 1,048,576-byte ordinary limit and
returns to the 240-byte bootstrap after the caller releases the status. This
is intended overload-reporting headroom, not a leak or physical RSS result.
The remaining review must either measure and bound concurrent control use or
identify it as a separately qualified R1 transport consumer. Per-owner sizes
and final-release/admission coverage for nonce/RC/count and escaped outputs
also require explicit reconciliation before an M1 completion decision.

## Continuing aggregate refusal control work

The 2026-10-22 23:16:37.244404 UTC follow-up has no overall deadline or frozen
candidate. Its generated evidence is under
`/Volumes/My Passport/teleno-m1-grpc-refusal-20261023-7u4u7z17/`.
An exact R13 state/status component holds 1/8/64/512 simultaneous refusal
owners, with 4,634/37,072/296,576/2,372,608 attributed extra bytes and exact
final release. It measures the missing aggregate admission boundary; the
4-KiB policy workspace is not an observed physical allocation.

The unfrozen separate refusal allowance passes six normal small checks and
the same six scoped ASan/UBSan cases plus three working detector controls.
Eight default-TSan small cases/controls also pass. The original real-server
test passes three cases but fails the new fourth exhausted-control case;
ASan identifies a null call in gRPC's factory-time `TryCancel`. Moving the
sync call binding before factories in one hash-pinned SDK source resolves
that diagnostic: four normal, four scoped ASan/UBSan real-server cases and
three detector controls pass. Original failures and the first duplicate-object
link failure are retained. This does not qualify integrated gRPC TSan or a
complete new node.

Thirteen source/archive/configuration recipe checks pass on OVH. The first
expanded recipe invocation preserves its three tool-PATH preparation failures;
the corrected identified CMake/Ninja environment passes all cases. The repository's SDK module
subsequently configures, builds and links its normal component after preserving
its initial installed-header inventory failure, Debug base selection and
metadata reconciliation error. Only a build-owned SDK archive member changes;
the installed SDK remains unchanged. Expanded fallback allocation failures
pass eleven normal/ASan/control invocations, with seven cancellations before
native work and exact final credits. Six real-gRPC default-TSan cases pass,
including functioning clean/known-race callback controls and concurrent
cancellation/stop. Their other native/store/Protobuf/Boost/SSL libraries remain
prebuilt component exclusions. Final source/build reconciliation, paired
policy/resource measurements and exact-node runtime gates remain open.
The operational producer remains outside scope and no observer starts occur
in this follow-up. M1 remains open.

Frozen R14 source archive
`468b442017a949842322f7667e774149f339364c7b73c44e737b3c2e425d907e`
fails CMake generation before compilation: an unrelated P2P test registration
was included while that separate implementation was correctly excluded.
Frozen R15 removes only that registration from its snapshot and preserves
the workspace work. Source archive SHA-256 is
`3be9aa55f128f649d851ec87ca9dfa3574736eec0bdec9e4e9ebaecbae47995f`.
The complete normal Linux build passes with 71 ELFs and 168 identified linked
libraries; node SHA-256 is
`e825f1951a583590e9173c57be79626635dbb110f750d1258eb34efffa31eef4`.
The configured roster contains all 101 R13 checks and three new refusal/SDK
recipe checks. No test or CLI is executed on the builder. Final OVH normal,
CLI, sanitizer, paired/profile and exact-node qualification remain pending.
