# Pending-record copy ownership under allocation failure

Date: 2026-10-17. Status: **B4 passes all 61 normal Mac/Linux checks and paired
admission comparisons; integrated diagnostics stop after 52 passes. The later
B5 cleanup candidate exposes an unknown-field publication leak corrected by
[the later B6 runtime](protobuf-unknown-field-ownership.md); whole-node
qualification remains open**.

The frozen B3 sanitizer run reaches the native CLI and passes its first 15
CTest entries. The pending-record test then leaks one 32-byte string object
during its admission allocation-failure sweep. All 317 failure-evidence members
and 4,200 owned RAM workspace members are preserved and individually verified
in 221,782-byte and 348,516,142-byte archives.

## Cause and correction

The pinned protobuf `RepeatedPtrFieldBase::MergeFromInnerLoop` allocates string
elements before `MergeFromInternal` updates their owning field's allocated
count. If payload copying throws, ordinary destruction misses the new string
object. Admission reaches this path while copying transaction signatures into
a temporary record. This does not establish a leak on successful admission.

A minimal program using the same pinned headers/library independently
reproduces the 32-byte leak at its third ordinary allocation. A clean copy and
other injected positions succeed. Arena ownership produces no report at all
ten positions. The 21 build/execution cases retain both sanitizers and leak
detection; 112 evidence members are verified in a 36,266-byte archive.

Admission now creates its temporary `pending_transaction_record` in a
per-admission protobuf arena. This owns partially constructed repeated elements
when copying throws. It expires before fork publication; immutable encoded
records and reservation lifetimes remain unchanged. No dependency patch,
detector suppression, persistent-format change or total memory bound is added.

## Focused verification and limits

The actual Linux native test passes with the candidate source files and frozen
base/dependencies under ASan, UBSan and leak detection. It records 329 unchanged
allocation-failure rejections with successful retries and six post-publication
logging faults. Concurrent admission accepts four and classifies 76 local
capacity refusals. The new roundtrip preserves empty/large repeated signatures,
an operation, unknown fields at multiple message levels and the caller's bytes
across all four retained checkpoints. Mac's full build, version/help and all
61 CTest entries pass (158.539 seconds for the suite). Mac native ELF SHA-256:
`95329cb81b45ef5d6f349594d0202725c15b2e49b9f624a068ff2342a765a68b`.

Linux test wall/CPU time is 4.054 / 3.77 seconds, kernel peak RSS 436,908,032
bytes and minimum host available 5,229,387,776 bytes. Owned swap is zero and OOM
counters unchanged. Different allocation counts and the added case prevent
claiming a normal CPU/RSS improvement from this diagnostic. Temporary arena
capacity and total mempool memory remain additional resource axes.

All 41 evidence members and four workspace files are verified in 97,144-byte
and 50,544,835-byte archives. Only the closed generated candidate workspace is
retired after fresh hashes and process-reference checks.

The 306-input candidate manifest is
`93a2cc291d447139813441b245ea8575f3764e4653b9edd957053f6388bc4202`.
Only the mempool implementation and pending-record fixture differ from B3.
Normal candidate builds/CLI and all 61 test entries pass on both platforms.
Lifecycle/thread diagnostics and final VPS workloads remain separate gates.
B3's restore/observer measurements do not qualify this candidate.

The normal Linux evidence combines 55 original passes, two successful ext4
reruns and four individually reported successful retries. Source, compilation
commands and native ELF remain identical throughout. Linux native ELF SHA-256:
`152f7a6b4fcc2f9268d9c96d4ab5ab45f22e8a5075bdb0c309e401fb068f9d0c`.
The original full run is retained as failed: six restore/capacity cases refuse
the undersized shared-memory filesystem. The first ext4 attempt also runs out
of staging headroom. Complete archives and fresh process/hash checks precede
offloading 56 unused normal test executables and closed generated fixtures,
recovering 1,227,595,776 available bytes. No reserve is reduced.

A subsequent attempt stops when an owned test process acquires swap, with no
host OOM change. Its full 2,190-member namespace is verified in a 763,882,198-byte
archive. Supported temporary child-only scopes then enforce 2.5-GiB memory and
zero swap; existing processes and host-wide swap settings are untouched. The
first scope-control request uses an unsupported systemd property and is refused
before launching its child; the corrected control verifies both supported
limits. Each final retry has a fresh ext4 root, complete XML, zero owned swap
and unchanged OOM counters. Its generated data is archived/member-verified,
freshly rehashed and retired before the next case. The full prior B3 normal
build and current B4 build remain independently archived and verified.
The Koinos One developer handoff and strict rendered manual checks pass; no
GUI, native pin, package or release is changed.

Evidence lives under the selected My Passport run's `evidence/` directories
`protobuf-copy-exception-20261017/`, `mempool-arena-copy-20261017/` and
`linux-integrated-asan-r4-arena-r1-20261017/`. The first broad-run preparation
correctly refuses an overlay that removes two other target sources. Its
117-command model is preserved before correcting the replacement; the fresh
corrected model retains all 119 own-source commands and unchanged detectors.
The first local broad-run coordinator subsequently stops because its archive
directory is absent, before any inventory command or test. The preserved
failure is followed by a reviewed continuation from artifact preservation;
native code, limits, detector settings and passed build work are unchanged.

That continuation passes the first 20 integrated ASan/UBSan entries, including
the formerly leaking pending-record test, then stops at the persistence-failure
fixture with an invalid-vptr report. Its `fault_db.cpp` wrapper is explicitly
compiled without RTTI, while the diagnostic RocksDB dependency was rebuilt with
RTTI. A separate multi-translation-unit control reproduces the same invalid-vptr
report with mixed RTTI, passes with matching RTTI and still detects a deliberately
wrong dynamic type. Its 55 members are verified in a 31,928-byte archive.
The diagnostic overlay aligns exactly two fault-wrapper commands with the
RTTI-enabled dependency. All other commands and native CLI bytes remain
identical; all 119 own-source commands retain assertions, both sanitizers and
no-recover behavior. Both formerly affected native fixtures then pass.
All 382 original failure-evidence members, 1,920 exact ASan build
members and 41 test-artifact members are preserved and individually verified in
316,909-byte, 258,384,830-byte and 89,488-byte archives. The normal-suite failure
evidence and its 1,009 test-artifact members are separately preserved.

The continuation passes 30 instrumented entries before the direct-activation
fixture reaches its own 180-second CTest property. It is still progressing:
137 refusal-startup logs, 36 validation-child attempts and 73 activation-child
attempts are retained. The ordinary fixture passes in 61 seconds. All 10,731
timeout-namespace members are verified in a 6,818,259-byte archive. A reviewed
diagnostic profile changes only that generated test property's timeout to
300 seconds, matching the already requested diagnostic CTest budget; the outer
case bound stays 600 seconds. All other generated test lines, executable bytes,
checks, detectors and RAM/disk/OOM limits remain unchanged. That exact fixture
then passes in **215.608 seconds**; all 11,699 fixture members are verified in
a 7,149,278-byte archive before retirement.

The continuation reaches 36 passes. A Python negative case then exposes a
fixture-layout error: its deliberately nonlocal output uses `tempfile`'s root,
but the runner had placed `TMPDIR` inside the admitted artifact root. The
unchanged guard correctly treats that path as local, and the test's mocked
process launch fails. No node is launched. Its complete 583-member namespace
is verified in a 696,527-byte archive. The reviewed continuation restores
separate owned ext4 temporary and artifact roots, preserving the original test
and ownership checks. Both roots are independently archived/member-verified
and retired after each passing case. Those unchanged tests then pass and the
suite reaches 52 passes. Cleanup stops during protobuf fixture construction
inside its allocation-failure window. The full diagnostic suite is not a
pass; previously passing entries and all failure evidence remain preserved.

## Cleanup-decoding successor: still unqualified

Preparing removal IDs before enabling allocation failure corrects the fixture
boundary without removing a state or retry assertion. The corrected fixture
passes all state checks but reports 320 leaked bytes in ten allocations while
cleanup decoding is faulted. The native B5 parser keeps each partial decoded
record in a caller-scoped protobuf arena and avoids its return-message copy.
The exact preserved corrected test object then passes removal and expiry
sweeps with no sanitizer/leak report. Only the mempool object differs between
the three-member control and candidate libraries.

The expanded fixture preserves pending-record, transaction and header unknown
fields. Its state/retry checks still pass, but LeakSanitizer reports **640 bytes
in 20 allocations**, with protobuf unknown-field parser frames. This is a
remaining ownership defect to investigate; arena ownership alone does not
close it. The two fault summaries are removal 303 cases / 302 refusals and
expiry 3,623 / 3,622, both with zero invalid state outcomes. No failed diagnostic
is suppressed and no full B5 regression pass is claimed.

B5 source manifest:
`7e8108f2a723aaba569009d5e51f0b2f6e5a3209df67fcd69a949de8beb45500`.
The focused failure namespace and exact workspace have complete member-verified
archives under `evidence/cleanup-decode-arena-20261017/`. Earlier B4 normal,
benchmark and controlled-producer results below retain their B4 source identity.

## Ordinary admission comparison

The normal B3/B4 comparison completes 72 processes: 12 excluded warmups and
five alternating measured pairs for each of three profiles and two independent
probe modes. Every process admits the same 1,000 synthetic library inputs into
four mutable fork views. Every complete pending-record byte digest matches
across all four views and both variants. The oracle sorts by transaction ID;
it makes no iteration-order assertion. Only `mempool.cpp.o` differs between
the two three-member libraries. These inputs do not execute signed transactions
or contracts; the [controlled producer](controlled-native-producer.md) is separate.

| Signature profile | Median paired B4/B3 admission CPU, unprobed | Ordinary `new` count, tracked | Requested `new` bytes, tracked |
| --- | ---: | ---: | ---: |
| One 64-byte signature | 0.9160 | 0.9820 | 1.0042 |
| One 8,192-byte signature | 0.9875 | 0.9820 | 1.0035 |
| Eight 64-byte signatures | 0.9540 | 0.9655 | 1.0130 |

The unprobed executable has no replacement allocation operators. Tracked
counters cover only ordinary calling-thread `new/new[]` requests during
admission, excluding dependency malloc/aligned allocation and other threads.
They are not live heap, retained capacity or node RSS. Requested bytes rise
slightly despite fewer calls. The tracked eight-signature case also has paired
CPU/p95 ratios of 1.0046 / 1.0475; the result is not an unconditional improvement.
Concurrent historical/development work shares this larger builder. Wall/tail
variation is retained rather than presented as whole-node throughput savings.
Maximum benchmark-process kernel peak RSS is 42,770,432 bytes; minimum host
available is 5,299,503,104 bytes. Owned swap is zero and host OOM is unchanged.

All 401 evidence members are verified in a 305,758-byte archive. The two objects
and four executable artifacts are separately verified in a 35,029,938-byte
archive. Raw results, per-pair ratios, ranges and MAD are retained under
`evidence/admission-copy-overhead-20261017/`. The benchmark's original failed
prerequisite is preserved before its reviewed continuation. These component
measurements and the leak correction do not establish final whole-node memory,
sustained producer operation or final-source VPS qualification.
