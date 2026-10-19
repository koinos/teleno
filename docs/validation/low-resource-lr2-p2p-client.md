# LR2 P2-C: owned outgoing Peer RPC calls

Date: 2026-10-15. Status: **Mac/Linux native, separate scoped ASan/UBSan/TSan,
VPS components and runtime-container CLI checks pass. Full P2/LR2/producer/40 GB qualification remains open.**

## Verified defect and scope

The prior client restarted its six-second wait after every response fragment.
An isolated actual pinned TCP/Noise/Yamux peer sends a valid response in three
pieces at about 0, 4 and 8 seconds. The old production client accepts the response
and fails the new complete-body-deadline regression in 8.48 s. The corrected
client rejects it after the existing six-second response-acquisition period.
No public peer or running database is used by this fixture.

The first fixture attempt constructed its second SDK injector before production
logging initialization and crashed in `libp2p::log::createLogger`. Its sanitized
crash report is preserved. Correcting fixture initialization precedes the valid
old-production reproduction; the fixture crash is not attributed to the client.

Review also found that the client's declared 64 MiB receive ceiling was tested
only after a truncated decode, allowing a completing read to overshoot it.
A valid requested block batch can exceed our local capacity. That condition
must neither penalize the peer nor repeat an oversized batch indefinitely.
Aggregate transport/SDK/decoded memory remains a separate unfinished requirement.

A final decoder review reproduced another regression: reusing the complete
frame retained a large ignored header or trailing suffix for a tiny result.
The new 512 KiB ignored-data regression failed before correction. The decoder
now copies a small payload when ignored capacity dominates, and reuses the
allocation when payload bytes dominate. Both cases preserve the original
validated payload bytes and accepted trailing-byte behavior.

## Implementation and invariants

`RpcClientCall` owns asynchronous open, complete request write and incremental
response acquisition. Open and write retain their existing six-second phases;
one six-second deadline now spans the complete response instead of each fragment.
Timer generations discard superseded timer callbacks. An additional 19-second
external wait prevents a blocked I/O runner from indefinitely holding the caller;
its local cancellation request touches only an atomic flag before serialized
cleanup. Synchronous DNS resolution precedes that wait and is **not bounded by
this correction**. Admitted synchronous server work can still delay shutdown.

A published immutable client context prevents outbound callers racing mutable
host ownership. Admission and shutdown share a mutex so accepted posts precede
the shutdown drain. Stop retires the context, cancels tracked calls, then closes
established connections. Calls before start/after stop fail locally; old epochs
cannot issue requests after restart. Late opens reset their stream. Pending SDK
read/write callbacks retain borrowed buffers until physical completion, while
logical completion/cancellation occurs once. A cancelled short-write continuation
cannot send another suffix; already-issued I/O cannot be undone or replayed.

Response reads validate positive counts against their actual remaining window
before pointer arithmetic. A constant-stack incremental scanner locates the two
MessagePack objects; typed decoding runs once. Reads never append beyond the
existing logical 64 MiB ceiling. `decode_response_owned()` uses the same header/
payload validation and trailing-byte behavior. Payload-dominated frames reuse
their allocation; small results shed capacity from large ignored regions. Encoders, protocol/service/method names and bytes,
block validity, execution/metering, receipts, roots and history are unchanged.

Lifecycle cancellation and receive capacity use distinct static local reasons.
Sync/handshake workers do not add peer error scores for these conditions.
For block batches, a receive-capacity refusal halves that peer generation's next
requested count, down to one, preserving the same anchor before any application.
The ordinary sync loop then makes the later read-only request; the failed RPC
is never automatically replayed. A single block that cannot fit remains a local
capacity failure. This does not establish catch-up throughput or a complete
memory profile.

## Native evidence and identity

Canonical checkout: `codex/lr2-memory-hardening`, base
`d3cfb94455da4a472f651eb4239abbc289e74225`, with preserved mixed uncommitted work.
The reviewed export enumerates 290 input files; its manifest SHA-256 is
`747a9bbacd58aa83e53c9f24b006e4796661e6ca4232fe0911fbe9b337e48211`,
and its source archive SHA-256 is
`61ef1a7da16f883bfcb586c687fce213defe8ea916a910b1cc2939eb9678799a`.
Later English guidance/handoff edits are documentation only; compiled runtime
inputs remain pinned by this export. The base commit alone does not identify
the dirty implementation or provide a clean integration pin.

Current Mac final focused **3/3**, all targets, **55/55 CTest (144.49 s)** and
version/help pass. Native identity is `1.3.0-dev.0+d3cfb94455da-dirty`, executable
SHA-256 `74509f33b763ea5a6251393823b8ce32500a25c8dc073b4151733656c08d9be9`.
Larger-Linux all targets, **55/55 CTest (130.72 s)** and version/help pass.
The native executable SHA-256 is
`a5189313dc3e3d57e583bacce2541abf32389675cbb8c1568f30b6b3b9632928`,
with the same native version. Each separate ASan/UBSan/TSan run passes its six
P2P fixtures and clean/known-fault controls. Positive controls produce the
expected heap-use-after-free, undefined-behavior and race diagnostics with exits
1/1/66; clean controls exit zero. All 34 final native/diagnostic steps meet their
expected results. Earlier P2-B and reviewed R2 remain historical evidence.

The controlled client fixture exercises inline/deferred fragments and writes,
exact-capacity success/one-byte-over-capacity local refusal, invalid read/write
counts, zero progress, disconnect/throw/allocation faults, all three deadlines,
queued-write cancellation, late open/read/write ownership, inactive generations,
concurrent admission/shutdown and owned-decoder prefix/error/byte compatibility.
The final focused set covers codec/client/actual transport, including allocation
reuse for large payloads and shedding large ignored headers/suffixes for small
ones. A late-read timeout fixture now holds its borrow deterministically rather
than depending on relative scheduler delays; the actual SDK fixture separately
proves the complete-response deadline.
Actual SDK fixtures cover the slow response, complete binary payload, outgoing
stop cancellation and fresh restart, plus earlier incoming/read/write regressions.
Native sync tests prove no peer penalty after local lifecycle failure, and a
4-to-2 block-request reduction applying the first two canonical blocks from the
same anchor without penalizing the peer. These fixtures use controlled chains.

An intermediate Mac source revision passed 55/55 in 178.73 s. Its Linux build
was explicitly stopped at 328.22 s with SIGTERM/-15 after review found the local
capacity-classification/progress gap. The revised source and current Mac result
above supersede it. Source, partial build log and cancellation receipt remain;
this was an intentional development-build interruption, not a successful Linux
gate or an unexplained production failure. No node service was stopped.

Reviewed R2 subsequently passed Mac/Linux 55/55 (142.03/129.29 s) and all six
P2P fixtures under each separate sanitizer, with functioning clean/known-fault
controls. Its first revised Linux build hit a 900-second command timeout; the
source-identical incremental resume used a 2,400-second build deadline and
passed. The ignored-capacity regression then required the final source above.
All interrupted, intermediate and final identities/logs remain separate.

The final binary also passes nine normal-ASLR CLI/synthetic component commands
on the designated 2-vCPU/4-GB/ext4/no-swap VPS: version/help, six P2P fixtures and
the FakeChain/FakeMempool producer test. Sampled host MemAvailable stayed at
least 3,462,856,704 bytes during these short checks. Per-child `wait4` RSS includes
fork/exec launcher lifetime and is not an isolated native peak. No restored
full-history service, public peer, real producer or operational database is used.

An isolated runtime-container build, non-root/read-only/network-disabled
version/help and executable hash check also pass. Image identity is
`sha256:bb4c61f44310926ea12bbbc251306f5d4544323e05b9e61ffef80d2a693303e2`.
This inserts the verified native binary into the retained runtime base; it does
not reproduce the repository Dockerfile/dependency build or install an image
on a running service. Labels retain the dirty-source manifest and binary hashes.

## Five-pair allocation comparison

Each platform runs five alternating fresh-process pairs with the same 8 MiB
synthetic ID payload in an already-owned complete wire frame. The decoder's
C++ `operator new` requests are measured separately from input generation,
physical allocator retention and process RSS.

| Median metric | Copy decoder | Owned decoder |
| --- | ---: | ---: |
| Mac requested C++ bytes during decode | 8,388,624 | 0 |
| Linux requested C++ bytes during decode | 8,388,618 | 0 |
| Decode allocation attempts, both platforms | 1 | 0 |
| Mac process peak RSS bytes | 18,415,616 | 18,415,616 |
| Linux process peak RSS bytes | 20,197,376 | 20,041,728 |

This establishes removal of one attributed payload allocation for this fixture.
Process peaks include frame generation and allocator behavior; no 8 MiB RSS
saving, whole-node ceiling or target-host improvement is inferred. Linux timing
medians were 4.15/0.89 ms during concurrent guarded tests, and Mac 0.184/0.142 ms;
they are informational samples, not CPU/latency qualification or selected policy.
Exact per-pair outputs, compiler/input/binary hashes and the measurement source
are retained. Mac uses Clang; the larger Linux miner uses GCC 13.3. Both reuse
the actual final codec source, SHA-256
`33224629912b141f5c62db28e68e5a1d9abc7d0cb3255cd599fdd145c96a0ea0`.

## Data preservation and remaining gates

Development uses the guarded existing My Passport run with fresh `p2c/` and
`p2c/final/` and `p2c/retention-final/` evidence children, explicit external
temporary/test roots and Python
bytecode disabled. Mac CMake points to the exact canonical repository. The Linux
owned source copy is verified against every exported input before compilation;
unchanged files retain their timestamps after the reviewed update.

To preserve the builder's unchanged free-space floor, 114 completed maintenance
build object/test files were externally archived and every member hash-verified
before the remote originals were rehashed, checked unused and removed. Logical
bytes were 449,379,744; archive SHA-256 is
`517e62d294dae0d2794bd2758daed29a47f867cb0e5d1d2c5444fe976a202be7`.
The installed node executable/images, tools, source/dependencies, databases,
backups, configuration and original evidence remain. This is owned development
cleanup on the larger builder, not a target-server disk saving.

The completed reviewed R2 normal synthetic outputs (10,800 files/links,
1,269,280,942 logical bytes) were separately archived and every member verified
before retiring only that owned test-artifact tree. Raw R2 evidence and eight
normal executables were then archived (106 verified members, SHA-256
`ceb4ba9b6b93751338c6dcb34a30883a52f502d6004e3c786e7619f6e1fcbd15`)
before retiring only 18 externally preserved diagnostic executable copies.
Original logs, source/cache records and normal executables remained for the
verified incremental final update. These are development artifacts; no chain
state, history, receipts, backup payload or active process was removed.
Final R3 raw evidence (83 members including diagnostic executables) is also
externally archived and every member verified, SHA-256
`c96a36aba288c33d1ac04dab85da98b1dcafbc964c5d737c72289126ee824121`.

The VPS component binary bundle and every raw evidence member were also verified
on My Passport before retiring only eight owned component executables and their
input archive (121,832,230 bytes). Logs/manifests/receipts and earlier component
roots remain. This restores test space; the dated restore assessment remains
a separate capacity observation. The larger builder also retired 10 verified
duplicates from that bundle/container context (166,550,022 bytes), preserving
normal build executables, diagnostics, images, source, manifests and logs. Its
10 GiB free-space and 2 GiB available-memory build guards were never reduced.
The final native synthetic outputs (10,800 files/links, 1,269,337,884 logical
bytes) were independently streamed to My Passport and every member verified,
archive SHA-256
`325175da2e92e59c23e5278c1165ab3e36ce89f6433233ae71913cbdbdb62e0d`.
Only that completed artifact tree and 18 externally preserved diagnostic copies
were then retired after source rehash/unused-file checks. Original build
executables, source/cache records, logs, images and operational data remain.

Strict Koinos One MkDocs compilation and both repositories' diff checks pass.
The preservation audit confirms 351 unchanged pre-existing Teleno files, 25
scoped modified/new paths, and only the networking manual changed in Koinos One.
Both HEADs, branches and indexes remain unchanged. AGENTS/CLAUDE are identical.
All 288 non-guide exported inputs still match the tested manifest;
only the two agent-guide documentation files changed afterward. This is a
verified local handoff, with no clean integration commit or packaged-app claim.

Full P2 still needs aggregate SDK/queue/decoded/output/caller accounting and
capacity, DNS/complete lifecycle concurrency, server-output behavior and bounded
peer/error/watchdog retention. Complete state/mempool/VM/RPC diagnostics, whole
node/producer load, current dependency/container gates and historical H1 remain.
All diagnostic controls/fixtures use per-child `setarch x86_64 -R`; global ASLR
is unchanged and normal native/VPS commands retain ASLR. SDK static dependencies
are not fully instrumented. GCC TSan unsupported-fence warnings remain a
limitation of that diagnostic coverage. No change here qualifies
Linux physical memory or CPU, catch-up, sustained producer operation or a full
restore within 40 GB. The [current restore assessment](restore-40gb-current-baseline.md)
records a 4,140,425,216-byte reserve-preserving deficit at its dated VPS sample
after corrected public writer identity. No pruning, reserve reduction,
source-policy change or larger baseline is used.

Private evidence root:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/p2c/`.
The separate installed publisher maintenance artifact is unchanged. No main
integration, Git publication, Koinos One code/package/submodule change, producer
activation or mainnet signing/registration occurs in this tranche. The persistent
issue-9 goal remains active with every unfinished row retained.
