# Protobuf unknown-field ownership during allocation failure

Date: 2026-10-17. Status: **B6 passes all 63 normal Mac/Linux entries and all
86 Linux build targets. Its full instrumented run finds a separate arena
construction failure after 17 passes. Isolated causal controls pass; the new
combined runtime patch requires fresh integrated checks. Whole-node gates remain open**.

This is a memory-management correction for the user-maintained Teleno node.
It preserves future protobuf fields, wire data and cleanup-state invariants.
It is not a whole-node RAM bound or a final VPS qualification result.

The subsequent [source-export provenance correction](source-export-provenance.md)
changes exactly two diagnostic Python files and their README. Native sources,
dependencies, compile commands and all three executable hashes below remain
unchanged. Its current manifest and fresh regressions are recorded separately;
the earlier B6 source identity continues to describe the ownership experiment.
The following arena correction changes that dependency input and is not covered
by the earlier executable hashes.

## Arena string construction failure

The exhaustive ASan/UBSan continuation uses the complete instrumented pinned
runtime. After 17 valid entries, the real pending-record-budget fixture refuses
a string-payload allocation during mempool admission. `Arena::Create<string>`
has already registered cleanup when the string copy constructor throws.
Arena destruction later invokes a destructor on an incompletely constructed
string, and UBSan reaches `__builtin_unreachable` in `_M_is_local`. The test
stops with exit 92. This failure is independent of the earlier unknown-field
publication leak and was masked by an uninstrumented dependency runtime.

Two shadow runtimes differ in exactly one archive member, `arenastring.cc.o`.
All other members, public headers and each original test object remain identical.
Link commands differ only in the runtime archive and output path.

| Same-object control | Original runtime | Corrected runtime |
| --- | --- | --- |
| Minimal generated message, refused 8-KiB payload copy | UBSan exit 92 | Exit 0; source/default bytes and arena cleanup preserved |
| Actual native pending-record-budget fixture | UBSan exit 92 | Exit 0; admission assertions pass |

The corrected native run records 459,710,464 bytes kernel peak RSS, zero owned
swap and no OOM change. These are diagnostic measurements on the builder, not
target-host memory savings. The initial comparison reaches its unchanged
3-GiB shared-workspace limit before running the final corrected native case.
Its failed state and exact workspace are archived. Only completed, archived
control copies are retired after fresh hash and process-reference checks; a
fresh namespace then links and runs the corrected native case successfully.
The resource stop is not counted as a native pass or an OOM.

The private implementation now constructs a nonthrowing empty string in the
arena before attempting the payload copy. A throwing copy leaves a valid empty
object for registered cleanup. Moving the temporary retains the copy's buffer
capacity instead of triggering assignment growth. The three copy-construction
sites use this helper; public headers, ABI, schema, revision and wire bytes
remain unchanged. Generic `Arena::Create<T>` for other throwing constructors
and other copy/merge paths are not qualified by these controls.

| New private source input | SHA-256 |
| --- | --- |
| Original `arenastring.cc` | `4593e7d1d80677053f3bf79f2d041f56dc0fc4ee810600c69c5b5fedfef6da89` |
| Corrected `arenastring.cc` | `08a52ed990b3707b65f39f342a8f0dfddfef25d9e2fe89b1af5b7be8c3faddc2` |
| Combined ownership/arena patch | `3fb80a565fe46f8a8ebef607fa692ff914243db4f795639053a0d901f93ba4b1` |

The build recipe verifies both pristine inputs before applying either patch,
rejects mixed states and checks both outputs. The native regression now covers
refused arena copying, safe destruction, successful field reuse and exact wire
bytes including an embedded NUL. Recipe tests cover both inputs, idempotence,
mixed/unexpected states and failed preflight without modifying either input.
All five recipe checks and the expanded native regression now pass on Mac,
followed by the complete build, version/help and all 63 normal CTest entries
in 224.218 seconds. The first Mac regression assumes an exact standard-library
allocation size and fails to inject its payload refusal; its exact 316-member
source/fixture archive is preserved. The corrected test sweeps allocation
ordinals and positively verifies a payload-size refusal, safe field reuse,
arena destruction and unchanged bytes across library allocation layouts.

Current combined manifest, 311 inputs:
`a21a8958e1757a8f2f661264b5b8665158c6db33c9c31a2fed8991d39e962a20`.
Mac native ELF:
`e50d79cda78ea16ff31f415bcb717bcf5f9d4975f8d073f54c7ea496331ee98d`.
Exactly five build/patch/test inputs differ from the earlier diagnostic
manifest; the other 306 inputs are identical. The complete new source archive
is member-verified. Fresh Linux normal/instrumented namespaces are prepared,
with no tests yet launched. Prior B6 passes do not qualify this candidate.
Successful-path CPU/RAM tradeoffs and whole-node qualification remain open.

Evidence: `evidence/protobuf-arena-string-20261017/` and
`evidence/protobuf-arena-string-r1-finish-20261017/` in the owned external run.

## Reproduced cause

The earlier B4 integrated run reaches 52 passing entries before the cleanup
test constructs a protobuf input inside its allocation-failure window.
Preparing caller IDs before enabling failure corrects that test boundary.
The corrected fixture then reports a real 320-byte / ten-allocation leak in
partially decoded repeated strings, despite passing its state assertions.

B5 keeps cleanup records in caller-scoped protobuf arenas and returns references
without a further heap-message copy. The identical preserved corrected test
object then passes. Extending the fixture with pending-record, transaction and
header unknown fields exposes another leak: **640 bytes / 20 allocations**.
Removal and expiry assertions still pass; that does not make a leak a pass.

The pinned Koinos protobuf runtime creates a string or nested unknown-field set
before adding its pointer to a vector. If vector allocation throws, the local
field has no owning destructor and the new object is lost. The same publication
order appears in the corresponding
[upstream 3.17.3 source](https://github.com/protocolbuffers/protobuf/blob/v3.17.3/src/google/protobuf/unknown_field_set.cc).
The exact build input is the
[Koinos revision](https://github.com/koinos/protobuf/blob/e1b1477875a8b022903b548eb144f2c7bf4d9561/src/google/protobuf/unknown_field_set.cc),
not a dependency-version upgrade.

## Narrow correction and build identity

`AddLengthDelimited` and `AddGroup` retain temporary ownership in `unique_ptr`
until vector insertion succeeds. Successful calls return the same owned
object; failed insertion frees it. Public headers, field ordering, data bytes,
schema and successful parsing behavior remain unchanged.

All three native CMake entrypoints now invoke `cmake/TelenoProtobuf.cmake`.
The runtime is rebuilt in the selected build tree from the original Koinos
archive, with the verified ownership patch. Existing Hunter packages and the
pinned `protoc` are read-only inputs. Header/version checks refuse mismatches;
the archive and exact patch preimage/result are checked. A local archive is
supported with the same digest, without unverified source-directory overrides.

| Input | Identity |
| --- | --- |
| Koinos protobuf revision | `e1b1477875a8b022903b548eb144f2c7bf4d9561`, version `3.17.3.0` |
| Archive SHA-256 | `824eca40d5a9b7e1090961e84efe7b90fba818156c32d92e447f7daf61522318` |
| Original translation-unit SHA-256 | `bd1d8452a66fea9f415e17b971a8c7c4ee1ba3d6c6d939f56abb46e07af0910e` |
| Corrected translation-unit SHA-256 | `090f66ae063d27fe1ee03b5c34b6be903cf75c69ffa008336cc1ec32c8801d8a` |
| Patch SHA-256 | `5dd517fffbe2606daec4baaad0820d09065e36844d9c0dadd502e5efc07ef842` |
| B6 working-tree native input manifest, 311 inputs | `c8bad5024f0750d566145ba251d3fce72f52a7ee766414eadc57f447e466b1f2` |
| Mac native version | `1.3.0-dev.0+d3cfb94455da-dirty` |
| Mac native executable SHA-256 | `e00e20045fcd597f931245cbd378d153a203c1054c7a817c8d37fdd2c004611d` |
| Linux normal native executable SHA-256 | `cab9f6ea08bbe67f8236a9dd21d71b8f0fb1cbeb34607c3e66ec09e68f12c2b9` |
| Linux instrumented native executable SHA-256 | `7ed75fef50a359e135e2186cd1177d89441ece29b12a005a115206e14f01e15e` |

There is no new integration commit. The base HEAD and dirty version string
alone do not reproduce this implementation. No installed publisher, reference
VPS binary, Koinos One submodule, packaged binary or GUI behavior is updated.

## Controlled Linux evidence

An isolated program exercises both publication methods at six ordinary C++
allocation positions each. Original and corrected libraries differ in exactly
one archive member, `unknown_field_set.cc.o`; the test object is identical.

| Variant | String publication | Group publication |
| --- | --- | --- |
| Original pinned library | Second allocation leaks 32 bytes / one object | Second allocation leaks 24 bytes / one object |
| Corrected library | All six cases pass | All six cases pass |

Both known failures are observed with leak checking enabled. The first control's
coordinator incorrectly expected buffered stdout after LeakSanitizer exit;
its complete namespace and binaries remain preserved. A fresh control flushes
its summary explicitly and passes the unchanged assertions. Neither a detector
nor a failed result is suppressed.

The native B5 cleanup test is then relinked with only that protobuf member
changed. Its exact test object and three-member mempool library are unchanged
from the 640-byte failure. It now passes with both ASan/UBSan, leak checking,
nonrecovering diagnostics and the original resource limits:

- Removal: 303 cases, 302 allocation refusals, zero invalid state outcomes.
- Expiry: 3,623 cases, 3,622 refusals, zero invalid state outcomes.
- Native fixture duration: 138.881 seconds; CPU: 134.12 seconds.
- Kernel peak RSS: 501,239,808 bytes; owned process swap remains zero.
- Minimum concurrent builder availability: 4,718,313,472 bytes; no OOM change.

The builder has more RAM/CPUs than the reference VPS. These are correctness
diagnostics, not VPS memory or throughput results. A single run and sanitizer
quarantine differences do not establish a successful-path RSS improvement.

## Integrated candidate checks

The native regression adds 16 allocation-failure cases across both methods,
empty/populated vectors and complete destruction. An ordinary-allocation live
count catches lost ownership in a normal build. A generated protobuf message
also preserves exact bytes for varint, fixed-width, length-delimited, nested
group and repeated unknown fields through serialization/parsing. This counter
does not measure dependency `malloc`, aligned allocations, heap totals or RSS.

Three recipe checks exercise the pinned preimage, idempotence, rejection of an
unexpected input and preservation when the patch is missing. The full normal
Mac build, version/help and **all 63 CTest entries pass**, in 232.171 seconds
for CTest. The first build's test used an unavailable serialization member;
its source, failure logs, build model and completed runtime library are
preserved. The corrected test uses the public generated-message API.

Fresh normal and instrumented Linux configurations contain all 120 native
compile commands and 63 test entries. The normal complete runtime build and
both new CTest entries pass. The complete instrumented runtime and both new
CTest entries also pass with ASan/UBSan/leak checks. The normal Linux node,
version and help checks pass. That native build takes 1,279.496 seconds,
1,043.21 CPU seconds and 1,598,480,384 bytes kernel peak RSS. It records zero
owned swap and no OOM change during that interval. Its exported-source version
embeds the base revision explicitly:
`1.3.0-dev.0+d3cfb94455da4a472f651eb4239abbc289e74225`. The 311-input manifest
is required to identify its uncommitted implementation.
A missing static-zstd configuration input was corrected by
restoring the exact prior dependency paths after preserving both failures;
no dependency bytes, source or resource ceiling changed.

The subsequent instrumented CLI build stops before producing its executable.
The kernel and the owned scope's `Result=oom-kill` attribute the event to GCC
`cc1plus` while compiling `libp2p_transport.cpp`, inside the unchanged
2.5-GiB / zero-swap scope. The original wrapper first reports a remaining
descendant after its parent exits; its final host sample records the OOM
counter increase. This is a failed build, not a native lifecycle or sanitizer
pass. The complete failed namespace and exact build are verified in
193,401-byte / 136,578,693-byte archives.

The global OOM-counter guard correctly also stops historical range-003 and
refuses the next normal-test action before launch. Both failures/workspaces
are fully preserved. The separate VPS observer is not this builder or scope.
No operational process is moved into the build scope.

A reviewed compiler profile now adds the same GCC garbage-collection tuning
already used for `main.cpp` to `libp2p_transport.cpp`. Exactly one compiler
command gains two GC parameters; every other command, source byte,
ASan/UBSan/assertion setting and resource ceiling remains unchanged. The new
diagnostic runner also records failed-case samples/residual processes before
cleanup and attributes an OOM counter change before its lifecycle check.
Its fresh measurement epoch preserves the prior OOM; it does not erase it.
The first continuation omitted unchanged model files and stopped before
compilation. That failure is preserved; the current continuation supplies the
complete 86-target / 63-test model. The incremental build then succeeds in
284.472 seconds / 276.79 CPU seconds, with 2,090,115,072 bytes kernel peak RSS
and zero owned swap. Its new epoch's OOM counter stays **1 to 1**. Version/help
also pass. Retained objects from before the stop are reused; this does not prove
a clean combined build in one scope, a successful full suite or a measured
heap reduction caused by GC alone. The source and every other compile command
retain their exact prior identities.

The normal staged continuation uses a fresh, explicitly recorded epoch with
the same completed core, executable and earlier valid test passes. Its first
coordinator stops after four passes because CTest omits `command` for unbuilt
executables. The complete namespace is preserved and no test is marked passed
by that failure. The reviewed continuation reads all 63 commands from the
generated canonical CMake registration, builds required executables and checks
the fresh CTest command before execution. It targets all 86 declared build
targets, with separate temporary/artifact roots and verified offload.

A full ASan/UBSan staged suite is prepared behind the successful normal-suite
gate. It requires fresh clean/use-after-free/alignment controls, keeps the
previously reviewed diagnostic-only activation timeout of 300 seconds (normal:
180), and retains every other check and resource ceiling. Closed executable
copies are archived and freshly verified before retirement, then restored by
exact hash when a later test needs them. The five-pair cleanup comparison is
also prepared behind both complete-suite gates; it has no measurements yet.
The later source-export diagnostic run completes all 63 normal Linux entries
and all 86 targets for manifest `6dde3b266993faf8898f277ae8584f0c86eb659056fc604067dbbe84d2612688`.
Its instrumented run and new arena correction are recorded above; final
combined normal/instrumented suites remain required.

## Evidence and remaining work

Generated evidence is on the selected external volume under the owned run
`teleno-issue9-20261015-krvvxm54/evidence/`:

- `cleanup-fault-boundary-20261017/`: corrected fixture boundary and original
  native repeated-string leak.
- `cleanup-decode-arena-20261017/`: B5 identical-object pass and expanded
  unknown-field failure, with complete member-verified workspace copies.
- `protobuf-unknown-ownership-r1-20261017/`: two reproduced control failures,
  twelve corrected controls and identical-object native pass; complete
  diagnostic namespace/workspace verified in 405,063 / 51,164,326 archive bytes.
- `protobuf-runtime-candidate-r1-20261017/`: current manifest, full Mac checks,
  Linux normal/instrumented runners and their current results.

Closed Mac test outputs are archived, checked member by member, freshly
rehashed and verified free of open handles before their working copies are
retired. Both previous Linux build trees are preserved and fully rehashed
before removing their closed duplicates. Existing source, dependencies,
operational databases and the recovery reserve remain intact.

Required next work includes full current Linux/diagnostic regressions, remaining
protobuf copy/merge allocation-failure paths, paired successful-path cleanup
CPU/allocation/I/O/latency measurements and final-source whole-node tests.
This does not qualify the entire protobuf library's exception behavior, other
dependencies, TSan/lifecycle coverage, aggregate admission or retained-fork
bounds. The complete history, producer, final 40-GB restore, sustained Linux
operation and recovery requirements remain open.
