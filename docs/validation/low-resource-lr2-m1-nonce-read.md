# M1 shorter-predecessor nonce read (C9/C10 attempt)

Status: **the frozen C8 source reproduces two Linux ASan heap-buffer-overflow
reads. Frozen C9 corrects both reads and passes 74/74 normal, 22/22 focused
ASan/UBSan and 22/22 focused TSan checks, both separate cleanup checks, and a
controlled 100-block production/reopen profile. Five paired C8/C9 samples and
all ten reopen checks pass. This finite attempt closed unqualified for M1 at
11:11:04 UTC; whole-node qualification remains open.**

The operator's 2026-10-20 standing authorization covers this separate finite
attempt without touching the live VPS producer. Its owned control and evidence
root is `/Volumes/My Passport/teleno-m1-nonce-read-20261020-l2q4z85b/`.
The actual start was **08:00:37.080251 UTC** on 2026-10-20, the unchanged
deadline is **16:00:37.080251 UTC**, and at most two source candidates C9/C10
may be frozen. The C7/C8 attempt stays closed with its own evidence and
counters. Only C9 was frozen; C10 was unused. The control record retains the
original deadline and one consumed candidate slot.

## Reproduced baseline

The starting C8 source archive SHA-256 is
`16e1d33d1ef197b19ceb636e3283baaadfa1b659585f21916eb691a5b52c42ea`.
An isolated Linux fixture first admits one pending transaction, then queries a
longer account whose prefix is that transaction's shorter nonce-index key.
Putting a NUL immediately after the prefix makes the three-iterator
`std::mismatch` compare past the shorter key's terminator. Both
`get_pending_nonce_on_node` and `get_pending_transaction_count_on_node` read
beyond that key: ASan reports heap-buffer-overflow at original source lines
609 and 692. The normal-account clean control exits zero with the correct
nonce/count. An initial longer-account fixture without the NUL did not trigger
ASan; its source, binary and negative results are preserved rather than
relabelled as a failure.

The triggering fixture source SHA-256 is
`2b216dfb5892aa5a74853d855f1d157084646236c7ee135f8d9db3cbe0f3ef72`;
its C8-linked ASan executable SHA-256 is
`d414d2c179912633e34e52088b013333994fc90789e7e1624e7ba7be21c7ba08`.
The three-case result SHA-256 is
`657296f2d56b614197c3654e7e2978c9bc65613bde2287eff3f7f580bd3d9a26`.
Full stdout/stderr and build diagnostics are under the owned evidence root.
These are local fixture inputs, not a live-network test.

## Frozen C9 correction and qualification

The two Teleno read paths now check `key.size() >= account.size()` before
comparing the account prefix. A regression covers both long-account queries,
normal nonce/count results, unchanged accepted transactions and final M1
credit release. The same three-case fixture relinked against the corrected mempool object exits zero
for its control and both previously failing long-account queries, without
ASan findings; the repaired executable SHA-256 is
`455ac6a726918d2b5076ebebf267e3121528b263742e5365d79f5a44fa596135`
and the result SHA-256 is
`e1b8a19e0e94d279154d13d75996fed73ed9cb279645080311d5b076ebeeefce`.
The C9 frozen source archive SHA-256 is
`40c28493e7ff0c2985d85f2ce1a278c47e209e5e23d16af1bbe2641070eab9d5`
(450 files). The final normal Linux binary SHA-256 is
`bea7856f367d008e6ab43a1b9ecb016cab1623197e60dd928b936150347d0aa8`;
its reported version is
`teleno_node 1.3.0-dev.1+d424264b24595786fdba86d9eddbcbf1635284b6`.
Version and help checks pass. The full normal Linux suite passes **74/74**.
Focused ASan/UBSan and default TSan each pass **22/22**, their separate
cleanup checks, and clean/known-fault controls. The generated TSan aggregate
test timeout was raised from 600 to 1,800 seconds without changing source or
binaries. Prebuilt Boost, RocksDB, crypto and SDK objects are outside the
sanitizer rebuild; libp2p is disabled in focused sanitizer builds, and ASan
vptr checking is disabled for mixed prebuilt RTTI. These scoped checks do not
establish whole-process race freedom or a physical memory ceiling. The first
two-job normal build hit the isolated builder's 2.5-GiB build cgroup limit;
the identical frozen source built successfully with one job under a 4-GiB
build cgroup. That build-only OOM is preserved separately from runtime results.

The exact source, linked dependency hashes, toolchain, test outputs and
preserved build archives are recorded under the owned evidence root, including
`c9-dependency-and-toolchain-identities.json` and the normal/ASan/TSan result
files. All archived test-binary hashes were verified before remote cleanup.

With a 768-MiB attributed M1 policy, a locally signed federated fixture
produces 100 blocks containing 12,800 no-operation transactions under a
2-vCPU/2.5-GB/no-swap child cgroup. It reports zero capacity refusals,
564,639,994 peak attributed bytes, 816,521,216 peak RSS bytes, and a
186.43-second creation phase. Reopening replays 60 blocks and matches all
100 block/archive records, final block ID, final state root and archive digest;
the reopen peak RSS is 474,075,136 bytes. This is controlled-chain component
evidence on a larger builder host, not the reference 4-decimal-GB VPS or a
mainnet PoB/VM/network mixed workload.

## Five paired C8/C9 controlled samples

The same fixture source SHA-256
`00a6b4bf262c599923d3cd81cb1b9ad65e7750e26a426c8f8e36305553ba3378`
and 768-MiB policy were used in alternating order, with identical child
2-vCPU/2.5-GB/no-swap limits. C8 and C9 fixture executable SHA-256 values are
`a5be87db63eb128d2c95362bb1d669983433a2f28f02edf6217f83613a0141f2`
and `8c15159060c20a16aa0c5acb35cc24e4a0b1c7c437cdf7f4271cea52353e86ef`.
Every one of ten create phases produced 100 blocks and 12,800 transactions
with zero quota refusals or swaps. Every reopen matched its own terminal block
ID, state root and full block/archive digest. Locally generated block times
and IDs differ across runs, so these are equivalent workloads rather than
byte-identical traces.

| Create-phase metric | C8 median | C9 median | Median paired C9 minus C8 |
| --- | ---: | ---: | ---: |
| Elapsed | 333.25 s | 315.29 s | -14.19 s |
| User + system CPU | 447.62 s | 446.03 s | -33.32 s |
| Kernel peak RSS | 826,904,576 B | 825,638,912 B | -3,915,776 B |
| M1 peak attributed | 564,639,994 B | 564,639,994 B | 0 B |
| Producer latency p95 | 1.793 s | 1.805 s | -0.094 s |
| Physical output, 512-byte blocks | 1,508,560 | 1,508,568 | +8 |

Median physical input is zero for both. The paired elapsed changes range from
**+119.98 s to -85.48 s**; C9 is slower in two pairs and faster in three.
The prior single C9 run completed in 186.43 seconds, illustrating the builder's
large run-to-run variation. Do not attribute a CPU, latency or RSS improvement
or regression to the two-line runtime fix from these samples. The fixture
does not count all allocation calls; attributed bytes are not total heap/RSS.

The complete result SHA-256 is
`4f9cc52d69b81b01df363eb8763cda318365460b5897fa6be725e91f049fb30e`;
the summary SHA-256 is
`0bb7e39ac8648058d566bc21dc1c2ba75c96f4ee6c281364bbd1f45bff184c44`.
All remote evidence was checksum-compared with My Passport after transfer.
The controlled fixture state archives have SHA-256 values
`641ef22ce3c065326f1c2a1b253e7caa0c2ccf3522d25248115d06b7afb60702`
and `edaaf192ef586297289a68374fd4e201e47d163b0d0b6896914dde75d2ff532d`;
all **327** regular-file hashes in those archives match the remote source
manifest. See `c9-paired-external-preservation.json` under the owned evidence
root. No live VPS producer operation occurred.

Remaining M1 gates include complete ordinary nonce/RC/count and escaped
RPC/execution output ownership, representative policy selection, integrated
mixed workloads and actual whole-node/reference-VPS memory and disk margins.
The live producer was not touched. Do not mark M1 or issue 9 complete from
these scoped results.

The inspected legacy `koinos-mempool` source has the same iterator pattern;
see the [legacy applicability register](../implementation/legacy-applicability-of-teleno-findings.md).
No legacy service binary has been tested or changed, so its deployed behavior
is unknown. The C8 five-pair CPU/latency slowdown remains an independent
unresolved observation in the [output report](low-resource-lr2-m1-output-ownership.md).
