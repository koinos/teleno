# M1 output ownership follow-up (C7/C8)

Status: **The finite C7/C8 attempt closed unqualified at 07:58:20 UTC on
2026-10-20 with both candidate slots consumed. C7 fails its focused TSan run.
C8 passes 73/73 normal Linux, 21/21 focused ASan/UBSan and 21/21 focused
TSan entries, both separate cleanup checks, and a controlled 100-block
production/reopen profile. Five paired samples show a material CPU/latency
slowdown with an unresolved cause. M1 remains open.**

This report concerns M1 memory that escapes a native mempool call into the
JSON-RPC result and final HTTP response. It does not qualify total JSON-RPC
memory (R1), gRPC output, the whole node or the reference VPS. The run's
separate control record is
`/Volumes/My Passport/teleno-m1-output-gap-20261020-rF9mPYUo/control.json`;
its deadline is 2026-10-20 11:52:11.516774 UTC. No live VPS or producer operation
is part of this attempt.

## Reproduction and change

The exact C6 starting source archive is SHA-256
`47481eab86268e0f1d5b4f90b9cbde2f348d6d7513854842b23ed114e4064a22`.
An isolated localhost test held a large `mempool.get_pending_transactions`
HTTP response unread. The final HTTP body retained 9,650,176 bytes of capacity
while the M1 domain attributed only 977,851 bytes. This demonstrates an
ownership/accounting gap; it is not a measurement of process RSS savings. A
second fixture reproduced the mixed-batch form after the single-response
development fix: 9,666,560 bytes of held body capacity and 977,851 bytes
attributed. The first version of the baseline oracle was too strict and its
failed result is preserved separately; the corrected oracle passed. Evidence
is in `evidence/c7-output-gap-baseline-v2-*` and
`evidence/c7-mixed-batch-repro-*` under the owned run root.

C7 adds M1 lifetime ownership to allowed mempool JSON-RPC results. A single
request retains allocation ownership through the final response string. A
mixed batch reserves a conservative per-mempool-item contribution before
appending it to the shared body and retains those leases until the asynchronous
HTTP write releases the response. Other services execute outside this optional
M1 domain. Capacity refusal returns the existing retryable local `-32005`
classification; the batch can still dispatch a subsequent non-mempool item.
The change does not discard an accepted transaction or change consensus rules.

Development-only focused tests passed 7/7 before the freeze, including held
single and mixed replies, concurrent responses, successful roundtrips and
mixed-batch refusal/continuation. In those fixtures, the held single response
had 9,650,176 bytes of body capacity and 10,644,476 attributed bytes; the
held mixed response had 11,185,010 bytes of capacity and 12,163,082 attributed
bytes. After server stop, the relevant attributed count returned to 977,851
bytes. These are scoped accounting checks, not an allocator or physical-memory
ceiling. The final frozen C7 source archive SHA-256 is
`808ae567ad42864ac0f85b46cb4ff7bb93f212537e649d90070541f2706bbb7b`.
The development binaries do not qualify that frozen candidate.

## Frozen qualification and limits

The first all-target build used two compiler jobs under `MemoryMax=2500M` and
was stopped by its build cgroup at a reported 2.4-GiB peak without swap. A
single-job rebuild of the **same frozen source** completed. This was a build
resource failure, not a native runtime OOM. The first complete CTest run then
passed 71/73; its only two failures were Python harnesses looking for
executables at the external build root. Generated aliases to the same
SHA-256-identified binaries made those two tests pass 2/2. A fresh complete
run passed **73/73** from 04:44:22 to 04:47:53 UTC, with the source and node
binary unchanged before and after. Both initial failures, the corrected run
and exact file-verified artifact archives remain on My Passport.

The final normal Linux node is
`teleno_node 1.3.0-dev.1+d424264b24595786fdba86d9eddbcbf1635284b6`,
SHA-256 `ee933b8897f7cf6661256d36322c0aa61f54191b0e6ac1e1b2e683dde0ffa8c0`;
`--version` and `--help` pass. The captured dependency/toolchain identity
manifest records 172 linked inputs and hashes to
`3d1001392b37f9cd14899a04f801f8b30e6eaf03b4e19776f992f30cdf351286`.
The verified normal-build archive includes 4,826 files, six symlinks and that
exact node binary. These identities describe the isolated Linux builder, not
the untouched reference VPS producer.

The same frozen C7 source builds under focused Linux ASan/UBSan. Its clean,
known use-after-free and known undefined-behavior controls behave as expected;
all **21/21** selected tests and the separately bounded cleanup check pass.
The source hashes remain unchanged. The ASan build, including 17 identified
test binaries, is preserved in a verified 4,485-file/four-symlink archive with
SHA-256 `ecc5d779207c0f3ce88a382bd97a97b9e74bf2b20b4ad4a17779444ecf863cb8`.
Prebuilt Boost, RocksDB, crypto and SDK objects are not instrumented, libp2p
is disabled in this focused build, and the vptr sanitizer is disabled for mixed
RTTI dependencies. These passing checks do not establish whole-process heap or
RSS bounds.

The frozen C7 TSan build passes its clean and known-race controls and 19/21
selected entries, with separate cleanup passing. The JSON-RPC lifecycle and
mempool-output roundtrip entries fail. A direct repeat of the body-lifetime
fixture reproduces a race between session destruction and Boost.Asio read
handler cleanup. All traces, the 21-entry JUnit result and an exact-file
verified TSan build archive are preserved under the owned run root. C7 cannot
qualify M1 despite its normal and ASan/UBSan passes.

C8 changes the JSON-RPC session to a concrete strand executor and removes the
accept-strand snapshot that could become a final session owner during stop.
Before freezing, the isolated body-lifetime and output fixtures passed, and
two full lifecycle runs passed under default TSan. Its exact frozen source
archive is SHA-256
`16e1d33d1ef197b19ceb636e3283baaadfa1b659585f21916eb691a5b52c42ea`;
the frozen candidate now passes the normal and scoped sanitizer suites below.
These development runs did not replace the frozen-candidate suite.

The frozen C8 normal Linux build passes all 73 CTest entries, including the
JSON-RPC session and output tests that failed under C7 TSan. Its `--version`
and `--help` pass; the native executable SHA-256 is
`071011fc4e49f25377ec19e7e5e7ed5eabd9f6db0b4ed70b0c6476a51936022c`.
The captured dependency/toolchain record hashes 172 linked inputs and has
SHA-256 `f43e8c6394413acd790871cefa8a0af1fb76fb4adeed14adf1db163b78c50d88`;
the exact-file verified normal-build archive SHA-256 is
`0f9cdb618e16abc41491d32babe9b73d92cb2eeb3ed038dbf570ec91c4d25a94`.
The focused C8 TSan build passes its clean/known-race controls, all 21 selected
entries and separate cleanup. Its exact-file verified build archive SHA-256 is
`1a3bd4493df53ce3b475c5f26ba1167d74ecd1073a875df4b51acd495bef51d1`.
The same frozen C8 source also passes the focused ASan/UBSan clean,
known-use-after-free and known-undefined-behavior controls, all 21 selected
entries and separate cleanup. Its exact-file verified build archive SHA-256 is
`433013248d64cb62ab08d37008f962f244ab787b5766b7a88e8627549924a12b`.
These are isolated-builder checks, with the
instrumentation exclusions above.

Two C8 diagnostic probes revisit older C2 findings using the current
allocation-owner path. The same oversized 1-MiB synthetic nonce input requests
5,250,724 bytes across 267 C++ allocations and raises the attributed peak by
5,248,992 bytes; its old unowned-read predicate is false. Holding 256 native
mempool adapter responses retains 2,123,520 wire bytes and 2,245,120 reported
protobuf-space bytes while attributed ownership rises from 83,832 to 2,907,256
bytes; its old escaped-output predicate is false. The first diagnostic harness
link failed because its cgroup launcher changed the working directory. That
failure is retained, and a working-directory-only harness correction produced
the measurements. A separate final-owner probe releases all 256 responses and
observes attributed ownership return exactly to its 83,832-byte starting value.
Its first harness version failed before compilation because of a tuple typo;
the corrected fixture and both results are retained. These probes do not
establish valid-address policy, RSS or all output paths.

C8 also passes one isolated 768-MiB-policy profile on the Linux builder:
100 nonempty blocks, 12,800 locally signed no-operation transactions through
the real native mempool, zero aggregate refusals, complete block/receipt
history, and matching head, root and archive digest after reopening. The
creation phase peaks at 564,639,994 attributed bytes and 819,220,480 kernel
RSS bytes; it uses 469.84 user CPU seconds, 8.32 system CPU seconds and
362.66 elapsed seconds. The same fixture source and profile ran once on C6
in 192.15 elapsed seconds with 808,235,008 peak RSS bytes. The different
chain roots reflect separate locally generated runs; one sample per version
cannot establish a code-caused CPU or latency regression. No saving claim is
made from this comparison. The builder has more physical RAM than the reference
VPS; only the child was restricted to 2 vCPUs, 2.5 GiB and no swap.

Five paired C6/C8 runs then used the same SHA-256-identified controlled fixture
source (`00a6b4bf262c599923d3cd81cb1b9ad65e7750e26a426c8f8e36305553ba3378`),
alternating order, the same 768-MiB aggregate policy and the same 2-vCPU,
2.5-GiB/no-swap child limits. The C6 and C8 fixture ELF hashes are respectively
`1808ab82351c5a704a210114d3f2111b7104a3be3d4d047b4d3f7a94a0531d53`
and `a5be87db63eb128d2c95362bb1d669983433a2f28f02edf6217f83613a0141f2`.
Every one of the ten 100-block/12,800-transaction create runs and its reopen
passed with zero aggregate refusals and swaps. Each reopen matched its own
head ID, state root and complete block/receipt archive digest. Different runs
generated different block times and IDs, so these are equivalent logical
workloads, not byte-identical traces.

| Metric, create phase | C6 median | C8 median | Median paired C8 change |
| --- | ---: | ---: | ---: |
| Elapsed time | 188.59 s | 298.31 s | +44.00% |
| User + system CPU time | 309.87 s | 445.53 s | +29.23% |
| Producer latency p95 | 1.171 s | 1.714 s | +43.85% |
| Kernel peak RSS | 820,051,968 B | 820,150,272 B | +0.27% |
| M1 peak accounted bytes | 564,639,994 B | 564,639,994 B | 0% |
| M1 final accounted bytes | 496,900,569 B | 496,900,569 B | 0% |
| Peak live M1 allocation-owner entries | 520,140 | 520,140 | 0% |
| Physical output, 512-byte blocks | 1,508,488 | 1,508,536 | +0.002% |

Physical input was zero at the median for both; the first C6 run reported 16
input blocks. Total allocation calls were not instrumented in this fixture;
live owner entries are not a substitute for that count. C6 elapsed times span
186.54–349.61 s and C8 times 211.36–354.23 s. The Linux builder also runs
other services, including periods of high concurrent CPU use. The observed
paired slowdown is a material tradeoff and remains unresolved; the samples do
not establish whether the JSON-RPC-only source changes, linked binary layout,
generated input differences or host contention caused it. No RAM or CPU
savings are claimed. The complete paired result SHA-256 is
`de47f73b06fbb71d425a7b13fc6bdfe4b1d683729f2554a2d3df9beed5a540f4`;
the analysis SHA-256 is
`0640e1d6b34e2f5ee4143ec1202e6a10c342b02284ec79ec449e5bb0e201fba0`.
All 62 transferred paired evidence files were checksum-compared with the
builder and listed in `evidence/c8-paired-evidence-manifest.json` on My Passport.

The full C8 native suite includes the oversized nonce/RC/count read/refusal
regression and a controller test that holds 32 locally executed transaction
receipts, checks that their M1 credits remain charged, then observes release
after the final owner is destroyed. These are narrow native paths: they do not
prove every read shape, large receipt/log behavior, gRPC serialization/transport
ownership or concurrent mixed output.
The synchronous gRPC mempool method assigns the native response to a
framework-owned response object. The adapter's scoped C++ allocations retain
their leases through ownership transfer, but the gRPC library serializes and
buffers that object after the handler returns. Its own transport allocations
are outside the demonstrated M1 owner path; this is a source-supported
coverage gap, not a measured leak or an observed gRPC capacity failure.
The mixed HTTP body contains separately owned non-mempool material and can
grow through standard-library allocator capacity beyond the per-item
contribution. Its leases must not be described as a physical heap/RSS bound.
Representative Linux policy selection, whole-node mixed workload and
reference-VPS headroom remain separate unproved gates.
