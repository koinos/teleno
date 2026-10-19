# LR2 V1-D / D1: host result-count memory access and diagnostic startup

Date: 2026-10-04 (Europe/Berlin).
Status: **alignment defect corrected and focused gates verified; broader gates pending.
V1, D1 and LR2 NOT qualified.**

2026-10-15 follow-up: Linux standalone clean/known-fault controls now pass for
all three sanitizers, including on the replacement VPS. TSan needs ASLR disabled
only in its diagnostic child; five paired layout controls and a positive race
verify that workaround. Separate P2-A helper/codec sanitizer checks also pass,
with explicit exclusions. See [current evidence](low-resource-lr2-p2p-writes.md).
This supersedes the earlier Linux startup prerequisite, not the historical Mac
failure or the remaining integrated D1/whole-node gates below.

The subsequent [P2-B checkpoint](low-resource-lr2-p2p-reads.md) extends separate
Linux ASan/UBSan/TSan checks to the native core/P2P and actual TCP/Noise/Yamux
fixture. It reproduces and corrects retained established connections at stop;
all scoped checks pass with the same exclusions and child-only TSan layout
workaround. State/mempool/VM/RPC, dependencies and full concurrency remain open.

## Scope and baseline

Primary qualification remains Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB
physical RAM. This is portable native memory-safety work and Mac diagnostic
attribution, not a Linux/default-profile change. Full genesis history and all
source block/receipt records remain required. No live node/VPS, VM creation,
production, credentials, transaction submission, source-trust change, pruning,
LR3/LR4, commit/push/release or GUI/package/submodule integration.

Source `/Users/pgarcgo/code/teleno`, branch `codex/reward-reinvestment-macos`,
HEAD `cfd2eb83b8080ca49911555108ce05f0a9068f01`, dirty `1.3.0-dev.0`.
The 184 starting dirty/untracked hashes match finalized
[V1-C](low-resource-lr2-vm-capacity.md). Existing exact-source external Release
and UBSan trees are inspected before reuse; Mac 26.5 (25F71), arm64, Apple
Clang 16, CMake/CTest 4.2.3. Pinned dependencies and instrumentation boundaries
remain [V1-B](low-resource-lr2-vm-allocation.md).

New generated data uses the fresh owned
`/Volumes/My Passport/teleno-lr2-EEMOFW/d1-diagnostics-u6pnEJ/` subrun, with
explicit external `TMPDIR`, `TELENO_TEST_ARTIFACT_ROOT`,
`TELENO_TEST_BUILD_ROOT` and `PYTHONDONTWRITEBYTECODE=1`. Source/committed docs
stay in the repository. No existing databases or dependency installs are moved,
deleted or patched. Volume absence stops data-producing work.

## Confirmed native defect and limited correction

Fizzy bounds-checks four bytes for `bytes_written` but does not impose native
four-byte alignment. The real host and thunk dispatcher previously dereferenced
that address as `uint32_t*`. The new bounded fixture reproduces genuine UBSan
misaligned-store failures in production code, not an allocator override or
fake host's own typed access:

- Serialized thunk: `thunk_dispatcher.hpp:333` before correction.
- Void thunk: `thunk_dispatcher.hpp:306` before correction.
- Initial syscall zero: `host_api.cpp:83` before correction.
- Actual Fizzy execution reaches the serialized thunk failure through the
  VM callback, real `chain::host_api` and real dispatcher.

All four isolated pre-correction modes abort with subprocess status -6
(SIGABRT; runner status 134), `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
Before source files and Release/UBSan executables are preserved with SHA256
in `evidence/before-manifest.json`. The unsanitized pre-correction fixture
passes, which does not make its typed accesses defined.

`vm_manager::store_bytes_written` replaces all eight production typed stores
with a four-byte `memcpy` at their original sites. No heap allocation, new
setting, host signature, alignment rejection, delayed temporary copy-back,
VM memory/depth limit or resource charge is added. Existing native byte
representation is retained on the supported little-endian platforms; no
big-endian protocol qualification is asserted. The host implementor contract
documents that this four-byte address need not be aligned.

Write ordering is compatibility-critical: syscall initialization zeroes the
counter before copying override arguments; serialized results/errors are copied
before storing their lengths. A caller can intentionally overlap these bytes.
Deferring writes until callback return would change those existing effects.
The correction does not change a thunk's observable operation or introduce a
new consensus rule. [WebAssembly's byte-addressable memory model](https://www.w3.org/TR/wasm-core-1/)
permits unaligned loads/stores; that is supporting VM context, not by itself
proof of the Koinos host ABI or historical parity.

## Focused regression coverage

`koinos_host_memory_test` invokes real production writers with aligned and
unaligned addresses (offsets 0/1/2/3), sentinel bytes and void/serialized returns.
It exercises real native thunk/syscall success, unknown IDs, sufficient/zero
return buffers and error-length writes. A synchronous override fixture reads
its arguments through the actual `get_arguments` syscall and proves the
initial zero precedes argument capture when the counter aliases those bytes.
Serialized-output/count overlap is checked without a typed test-side access.

Generated one-page WASM invokes both imported host functions through actual
Fizzy, including valid zero offset, unaligned near-end offset 65,531, exact-end
four-byte offset 65,532, and invalid 65,533/65,536/UINT32_MAX addresses.
Bounds rejection occurs before the host is entered; a subsequent valid instance
still executes. Return bytes/counts and metering are recorded deterministically
for before/after comparison. The fixture reads counters only with byte copies
and has no allocator overrides, network or persistent database.

This is local memory-access/alias/bounds coverage, not ASan/TSan lifetime/race
proof, whole-node memory reduction, machine OOM or historical qualification.
There is no claimed memory, CPU or disk percentage gain from this safety fix.

## D1 standalone sanitizer attribution

`probe.cpp` is an isolated program with no Teleno dependency, allocator
replacement, node, network or file I/O. It writes `probe_main_entered` first in
`main`. Its clean path allocates four integers, joins a single worker, frees
them and checks the result. Bounded intentional lifetime/race controls test
whether a functioning sanitizer can diagnose known local mistakes; they are
not production or external-input experiments.

Source SHA256:
`35306c1ffe194c3cc757512919f746c54c928b3d74fcfee973383c480a01fedd`.

| Existing compiler / configuration | Build | Clean ASan | Clean TSan |
| --- | --- | --- | --- |
| Selected Apple Clang 16, `-arch arm64` | both 0 | -6, before main | -11, before main |
| Installed CLT Clang 14.0.3, explicit installed SDK 13.3 | both 0 | -6, before main | -11, before main |

ASan reports `CHECK failed: sanitizer_malloc_mac.inc:189` with an empty stack;
TSan has no stderr report. Clang 16 controls also terminate before main, so
neither a lifetime nor race report is obtained. Controls are not repeatedly
retried after the alternate clean runtime fails. Initial CLT builds without
an explicit SDK fail with missing `time.h` (exit 1); that build issue is resolved
by selecting the installed SDK, but runtime startup still fails.

Exact versions, resource directories, compiler argv, linked sanitizer libraries,
binary hashes and actual statuses are in `probe-results.json` and
`clt-sdk-results.json`. This attributes the immediate failure outside Teleno:
**it does not identify the underlying runtime/OS defect, prove the node safe,
or justify disabling sanitizer checks/system protections**. No compiler/runtime
is installed or system setting changed. Only the child's halt-on-error options
are selected. Integrated ASan/TSan remains a required blocked gate.

Read-only environment inspection finds no usable local Docker daemon or isolated
Linux instance; only an unrelated stopped Colima VM, which is not started.
No live VPS or new VM operation is authorized. The exact missing prerequisite
is a functioning, authorized isolated diagnostic runtime. Representative
Ubuntu/x86_64/ext4 whole-machine and pinned full-history/reference inputs remain
separate Q1/H1 blockers.

## Reproduction and actual gates

All commands use the owned external environment. `run_gate.py` records argv,
environment, elapsed time, subprocess status, log path/hash in
`evidence/command-results.json`; failing reproductions are retained as failures.
For example:

```bash
export TMPDIR='/Volumes/My Passport/teleno-lr2-EEMOFW/d1-diagnostics-u6pnEJ/tmp'
export TELENO_TEST_ARTIFACT_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/d1-diagnostics-u6pnEJ/evidence'
export TELENO_TEST_BUILD_ROOT='/Volumes/My Passport/teleno-lr2-EEMOFW/ubsan'
export PYTHONDONTWRITEBYTECODE=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  '/Volumes/My Passport/teleno-lr2-EEMOFW/d1-diagnostics-u6pnEJ/before/5-koinos_host_memory_test' --case vm
ctest --test-dir "$TELENO_TEST_BUILD_ROOT" --output-on-failure \
  -R 'koinos_host_memory_test'
```

The standalone source is in the same owned root. Its reproducible compiler flags
are `-arch arm64 -std=c++20 -O1 -g -fno-omit-frame-pointer -fsanitize=address`
or `-fsanitize=thread`; the alternate existing CLT compiler additionally uses
`-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX13.3.sdk`. No downloaded
compiler, special suppression or extra privilege is needed to reproduce the
recorded startup failures on this development machine.

Current focused native and UBSan suites pass **3/3** (1.26 / 1.36 s), exit 0.
The complete before/after deterministic transcript is byte-identical, SHA256
`c3dbc08cc72002ea76b4b16c77f9e902965309c13b0f04a2721f3b5db51a4b3e`.
The required dependency workflow exits 0 (102.89 s). Broader current-source
all-target/full CTest, selected integrated UBSan, CLI and strict documentation
gates are still pending; V1-C's earlier passing reports do not substitute.

The first unsanitized fixture exits 1 because its fake override backend reads
the context's caller frame directly. Correcting the fixture to read through the
actual `get_arguments` host entry resolves this expectation; production code
was still unchanged when the corrected baseline passed. That failed fixture log
is preserved as `host-before-bytes.log`, not presented as a production defect.

## Remaining requirements

V1 caller/queue/active-instance physical bounds, M1 aggregate metadata/workspace,
P2 transport/bookkeeping, R1 simultaneous service/output and S1 fork/caller
accounting remain open. D1 functioning integrated ASan/TSan is not waived.
Q1 needs an isolated authorized Ubuntu/x86_64/ext4 2-vCPU/4-decimal-GB machine
and eligible pinned authenticated full-history input; H1 needs the current
state-delta/differential range from before 30,504,202 through beyond 32,789,378
with reference roots/receipts/resource/head/LIB/index checkpoints. Neither
historical validation nor Linux memory/head-following qualification is claimed.
Docker smoke is unexecuted without a daemon. LR5 long-soak/server qualification
and release/publication/deployment remain separate. **Every LR2 Definition of
Done item proven: NO.**
