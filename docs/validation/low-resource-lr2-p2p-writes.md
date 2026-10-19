# LR2 P2-A: complete Peer RPC writes

Date: 2026-10-15. Status: **scoped correction verified; P2/D1/Q1 open**.

## Defect and contract

The real cpp-libp2p transport uses `Stream::writeSome` once for outgoing
requests and incoming responses, including error responses. The pinned SDK
explicitly allows a successful short write. Teleno treats any positive count
as a complete request and closes responses after the first callback. Valid
gorpc frames can therefore be truncated without a transport error.

This package writes the unchanged encoded frame completely before
reporting success or closing a response. It retains the writer and
immutable input until completion, serializes successive writes, rejects zero
progress and impossible counts, contains initiation/completion exceptions and
avoids recursive stack growth for synchronous short-write callbacks. Errors
must never be reported as full success. Partial transmission is not retried as
a new RPC; the write helper never invokes an RPC handler or transaction.

Required evidence: a regression reproducing the one-write defect before its
correction, exact gorpc bytes through synchronous/deferred short writes,
completion/lifetime/error/exception checks, focused native tests followed by
the full suite and CLI identity. Use only isolated fixtures and external
generated data. Record actual source and dependency identities.

This does not introduce an aggregate capacity limit or claim bounded pending
writes. Incoming fragmented reads, request deadlines/cancellation, transport
shutdown, peer/fork bookkeeping and Linux whole-node/sanitizer qualification
remain separate P2/D1/Q1 gaps. It changes no wire format, validation, mainnet
state, producer setting, configuration default or archive policy.

## Implementation and regression

`peer_rpc_write.hpp` owns one writer, immutable frame, completion and offset.
A successful partial callback schedules one continuation on the existing
serialized transport I/O context. Only one write or continuation is outstanding
for a frame. Completion reports the total only after all bytes are sent; no
frame prefix is replayed. The caller must keep the I/O context alive. Empty
input completes without SDK I/O. A local allocation failure stays distinguishable
from transport failure and the outgoing RPC rethrows it as `std::bad_alloc`,
which existing sync workers handle without a peer penalty.

Both production write paths use the helper: outgoing requests and incoming
success/decode-error/handler-error responses. Response close occurs on terminal
completion, after the final successful write or a failure. Frames are encoded
by the existing codec. The helper does not add a queue limit, deadline, protocol
retry, new setting or buffer copy. Its state and continuation allocations are
additional overhead; no memory/throughput improvement is claimed.

The original single-write behavior was first extracted without correcting it.
The new fixture then failed with `successful completion truncated the gorpc
frame`, after the controlled SDK writer accepted only seven bytes. The preserved
reproduction executable, source and log precede the fix. This exercises the
real Writer API with controlled callbacks; it is not a live-peer interop test.

The passing regression covers exact request/success/error bytes; immediate,
deferred and one-byte writes, including a 256-KiB response; close ordering;
zero progress, overcount, reset/error and initiation/allocation exceptions;
unavailable inputs; no frame replay; caller owner release; and a completion
that throws. One-byte callbacks do not recursively reenter the writer. Borrowed
input is copied by the fixture only when its callback completes.

## Current evidence

Source: `codex/lr2-memory-hardening`, base
`d3cfb94455da4a472f651eb4239abbc289e74225` plus preserved dirty work.
cpp-libp2p input: `c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`.
All generated local data lives under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/`;
package evidence is in `evidence/p2a/`. Private Linux routes/inventory remain
outside the repositories. This is uncommitted native development, not a
release or Koinos One submodule pin.

- Mac all-target build and version/help pass. Full CTest: **52/52**,
  **202.83 seconds**. Native identity `1.3.0-dev.0+d3cfb94455da-dirty`, SHA256
  `187f6b39322c3edc3c3b8139c89890ce98d0d40c7f3f17afb7722a0da63d26d3`.
- Separate Linux ASan, UBSan and TSan builds/runs of the actual frame helper,
  codec and controlled Writer fixture pass. ASan leak detection and UBSan/TSan
  halt-on-error are enabled. All three scoped diagnostic runs use
  `setarch x86_64 -R` for that child only; the paired TSan controls below show
  why that diagnostic layout is needed. Normal native CTest and the target's
  native CLI/component commands retain ordinary ASLR.
  These checks exclude the transport wrapper and other node components; no
  SDK static library is linked into these scoped diagnostic executables.
  GCC reports unsupported `atomic_thread_fence` instrumentation in Boost.Asio;
  the warning is preserved rather than suppressed.
  TSan does not establish concurrent transport/shutdown safety from this
  serialized fixture. They do not close integrated D1.
- Standalone Linux diagnostic controls are now functional: five ordinary TSan
  clean runs fail before `main`, whereas five paired process-ASLR-off runs
  complete. The intentional race is reported with exit 66. No global ASLR
  setting changes. On the designated replacement VPS, all **six** clean/known-
  fault ASan/UBSan/TSan controls pass using a private runtime bundle and isolated
  child processes. This establishes diagnostic feasibility, not node readiness.
- Linux all native targets, version/help and **52/52 CTest (122.88 seconds)**
  pass with GCC 13.3.0. Binary SHA256
  `aa2f9c11d829b797ade8424f7173e92bae5f00efa994bb184bff6790733fb36e`,
  with the same development identity. The miner build uses an owned source
  export with 284 individually verified native inputs, existing dependency
  installs, one low-priority job and resource guards. It is not a fresh full
  dependency build. Two prerequisite portability defects were corrected: propagate the
  pinned SDK's missing C++ gRPC static-link dependency after `Koinos::proto`,
  and explicitly extract a JSON string before comparing the synthetic restore
  digest under GCC. Failed attempts are retained. The corrected static-link
  target passes. On Mac, the corrected fixture rebuild plus signed synthetic
  prepare/restore/reopen digest verification passes.
- The exact Linux binary and selected fixtures also pass **nine commands** on
  the replacement reference VPS: version/help, complete-write/codec/cache tests,
  the producer component with `FakeChain`/`FakeMempool`, the real preflight CLI
  regression script, and signed synthetic restore prepare/restore/reopen checks.
  Generated data and ephemeral fixture keys use a fresh owned directory. No
  node service is started. These are real target-host component/CLI checks,
  not full-history node, catch-up, sustained production or recovery qualification.
- Koinos One networking documentation explains the correction and pending
  packaged inclusion. Strict MkDocs succeeds to external output. No GUI,
  package, native submodule, service or producer authority changes.

Evidence includes `reproduction-result.json`, `final-native-validation.json`,
`fixture-validation.json`, `linux-native-summary.json`,
`linux-component-diagnostics.json`,
`tsan-process-layout-results.json`, private baseline control results and exact
source manifests/logs. Raw Linux evidence is copied back to My Passport with
archive hash verification. No full-history/4-GB production, historical
differential, aggregate admission or complete sanitizer qualification is claimed.

Preservation audit: 352 pre-existing Teleno files remain byte-identical, with
11 intentional extensions and three new files; 577 Koinos One files remain
byte-identical and only its networking manual changes. Git HEAD/branch/index
are unchanged, AGENTS/CLAUDE match and 132 relative documentation links resolve.
The 284 Linux build inputs still match current native code; only the two agent
guides were extended after that export. `preservation-audit.json` records hashes.
