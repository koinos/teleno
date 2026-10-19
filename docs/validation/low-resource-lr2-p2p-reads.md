# LR2 P2-B: incoming Peer RPC frame ownership

Date: 2026-10-15. Status: **scoped correction, Mac/Linux native regressions and
diagnostics verified; target-host/whole-node gates remain open; P2/D1/LR2 open**.

## Defect and acceptance contract

The previous incoming real transport called `readSome` once and decoded immediately.
That API returns any positive prefix. A valid request split across callbacks
therefore becomes a server decode error and the handler is never invoked.
The codec's recursive skip also consumes native stack for each untrusted
MessagePack nesting level. Neither behavior is required by the wire protocol.

This package assembles the unchanged two-object request across short
reads in one owned 64-KiB buffer, preserving the existing incoming read-window
ceiling. The first complete request retains the codec's current trailing-data
behavior. Complete decoding invokes the handler once; a partial, invalid,
failed, oversized, cancelled or timed-out read invokes no handler. Successive
reads use the serialized I/O context, without recursive callback growth.
Borrowed input remains alive through late SDK completion after cancellation.
There is no RPC or transaction replay.

An incoming read deadline matches the existing six-second outgoing RPC
phase timeout. It bounds only request acquisition, not admitted service work
or response writing. Stop/restart generations suppress old handler callbacks.
These are local transport lifecycle policies, not consensus validity rules.

The codec skips nested values iteratively, preserving existing supported
descriptors, consumed bytes and truncation/invalid classifications. No new
nesting limit or protocol-byte change is introduced. Aggregate request/output,
SDK transport queues, full shutdown/cancellation and whole-node memory remain
separate open gates; per-frame bytes and a deadline are not aggregate admission.

Required checks: pre-fix fragmentation and deep-nesting reproductions, complete
boundary/synchronous/deferred/error/cancel/timeout/late-owner tests, separate
working sanitizers, native full regressions, isolated actual transport checks
where feasible and documentation handoff. Measure five paired samples before
claiming an allocation or memory improvement. All generated data stays on the
guarded My Passport run or fresh owned Linux fixture paths. No operational
node/database, producer authority, archive policy or public endpoint changes.

## Reproduced defects and correction

- The pre-fix fragmented request fails before application dispatch. The old
  recursive codec also crashes with SIGBUS when decoding 16,384 supported nested
  arrays on a 128 KiB pthread stack. The incremental scanner performs one pass
  over newly received bytes, with constant native stack use and no nesting
  allocation or new protocol-depth limit.
- The real SDK loopback fixture initially fails reconnect after stop/restart.
  Disconnection subscriptions were removed before stopping the host, leaving
  stale `_connected`/`_connecting` entries. Stop now clears these transient maps;
  known-peer history remains a separate, unbounded capacity axis.
- Functional tests pass while integrated ASan/LSan reports **3,449,633 leaked
  bytes in 240 allocations** after the two transport generations and independent
  fixture peer. This is a scoped diagnostic finding, not an RSS measurement.
  The pinned SDK's `BasicHost::stop()` only stops its listener. Established
  Yamux/TCP connections retain asynchronous reads and their I/O/dependency graph.
  Both production shutdown and independent fixture teardown now explicitly close
  their established connections on the serialized runner. The identical leak
  check then exits 0 with leak detection enabled; no suppression is added.

`RpcRequestRead` owns the reader, fixed input buffer, scanner, timer and
completion. Pending SDK callbacks retain borrowed input through late completion
after logical cancellation. Completion clears application and abort closures
explicitly: a moved `std::function` can retain a small callable in its source
on libc++. Register tracking before initiating I/O, so tracking allocation
failure cannot occur after handler dispatch. Each stop/restart has an active
epoch; old reads cannot dispatch new application work. The I/O context must
outlive SDK callbacks, and lifecycle calls remain externally serialized.

Stop is called by the external lifecycle owner, never from its own I/O callback.
It cancels tracked reads, stops services/listeners, closes established
connections and joins the runner before releasing runtime ownership. If posting
cleanup fails, the runner is joined before fallback cleanup. These bounded
fixtures do not prove every SDK shutdown, in-flight dial or allocation-failure
path, or arbitrary concurrent public lifecycle calls.

## Compatibility and regression evidence

The old/new Teleno codec differential matches all **12,208** seeded cases,
including supported descriptors, truncation, invalid input, trailing bytes and
generated nested values. It compares decoded bytes, outcomes and error text/
truncation classification. This is internal regression evidence, **not** an
independent legacy implementation or H1 historical replay.

The reader fixture covers synchronous/deferred fragments, exact 64 KiB input,
65,537-byte refusal before overrun, one-byte deep input, large declared lengths
without allocation, zero/invalid counts, reset/initiation/allocation errors,
timeout, cancellation, inactive epochs, late owner release and throwing
completion. Supported deep input passes on the same 128 KiB stack that crashes
the prior decoder.

The real fixture uses two pinned cpp-libp2p **TCP/Noise/Yamux** endpoints on
loopback, with no seeds, DHT or public-network traffic. It checks a one-byte
prefix without dispatch/error, the complete unchanged 256 KiB response, a
pending read cancelled by stop, and reconnect/new dispatch after restart.
An additional normal regression waits for the remote disconnection event after
both stops. The SDK source pin is
`c03bf7c44c4fd30a390f3ef93bd1eab482e686a6`.

Mac all targets, version/help and **54/54 CTest pass (127.37 s)** after the
connection-close correction. The subsequently extended disconnection assertion
also passes independently. Native identity is
`1.3.0-dev.0+d3cfb94455da-dirty`; SHA-256 is
`ed02363e1d258a7f820a9a3558eb8e5915e338a89677806bf49d037c3e7e1979`.
This is a development diagnostic on macOS, not Linux resource qualification.

The initial Linux all-target build passes, but CTest is 53/54 because retained
owned synthetic fixtures leave insufficient space above the unchanged reserve
for SC5. After a fully hash-verified external archive and recheck, only that
owned generated fixture tree is removed/recreated; **54/54 pass (112.90 s)**.
The original failure and all fixture bytes remain retained. This does not
change the VPS budget or make a 40 GB restore pass. The final Linux all-target
build, version/help and **54/54 CTest pass (139.29 s)** with the connection-close
production change and extended disconnection assertion. Native SHA-256 is
`290cf112aed00a06b5e839c83268116036d76fa3564ccda30ea49a463df8a6e8`.
The export manifest fingerprints 287 native/build/test inputs; the base HEAD
alone does not identify this dirty implementation. This larger miner result is
not whole-machine baseline qualification or installation on its public publisher.

## Integrated diagnostic scope

Separate Linux ASan, UBSan and TSan builds each pass the reader and actual
transport fixture after the connection-close correction. Six standalone
clean/known-fault controls pass first, including intentional use-after-free,
undefined arithmetic and a race detected with exit 66. There are 18 recorded
control/configure/build/test steps, all passing. The later remote-disconnection
assertion is an additional normal test; it is not in these diagnostic binaries.

Current native core/P2P sources and test translation units are instrumented.
Prebuilt static SDK dependencies remain uninstrumented, although ASan/LSan
intercepts their allocations. GCC TSan warns that Boost.Asio's
`atomic_thread_fence` is unsupported by its instrumentation; a pass cannot
qualify those excluded synchronization semantics. Diagnostic children use
`setarch x86_64 -R` because ordinary-ASLR TSan startup fails before main on this
host. No global ASLR setting changes. Native CTest uses normal ASLR.

One owned diagnostic CMake tree is reused sequentially, with bounded compilation
and disk/memory guards. Each phase's binaries, cache, compilation commands and
logs are preserved separately before the next sanitizer configuration.

## Evidence and remaining gates

Owned evidence root:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/p2b/`.
It retains source-before variants, failures, codec corpus/results, exact source
manifests, Mac/Linux native logs, diagnostic controls/binaries and verified raw
archives. Generated data uses the guarded external environment; miner paths
and access routes remain private. Existing operational nodes/data are untouched.

No aggregate request/response, SDK queue, known-peer/address, service work or
whole-node capacity is established. Outgoing timeouts/cancellation, broader
concurrent shutdown, state/mempool/VM/RPC diagnostics, H1 and representative
Linux producer/restore resource gates remain open. No allocation/RSS saving is
claimed from the framing change; five paired comparable measurements would be
required. The [complete-backup 40 GB gate](restore-40gb-current-baseline.md)
remains required and unproven. No main merge, publication, packaging, mainnet
mutation or block-production activation occurs here.
