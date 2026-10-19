# Native producer head and mempool handoff

Date: 2026-10-18. Status: **scoped Linux repair and the complete finite mainnet
producer observation pass**. No produced block is observed in the fixed window.
The subsequently authorized observation independently confirms own block
**40,323,375** as canonical and irreversible; see
[the separate result](#subsequent-own-block-confirmation). This report does not
close issue 9 or change the original finite result.

## Reproduction and correction

The original finite attempt reports twelve unknown-parent mempool exceptions.
The chain publishes its new head before its block-accepted event creates the
matching mempool fork. A deterministic regression publishes a newer fake-chain
head while the real mempool still knows only the previous block. The unmodified
producer library reproduces the original exception. Warning-time fork IDs from
the first live attempt were not recorded, so its exact interleaving remains
unproven; this regression proves the failure path rather than retrospectively
identifying every warning's cause.

The producer now classifies only an unknown-parent exception from its pending
transaction lookup, or an observed head change during preparation, as a candidate
deferral. Each call performs at most one preparation attempt and requests a 10-ms
retry. It discards an old cached PoB candidate when replacement fails and checks
the observed head around PoB metadata/balance collection and before an eligible
submission. These checks do not create an atomic cross-component snapshot or
replace consensus validation.

One hundred consecutive deferrals expose the original mempool exception, or an
explicit persistent head-change error. Counts do not reset merely because a
different parent was requested. A successfully completed preparation/evaluation
attempt resets the streak. Storage, allocation, contract and other exceptions
are not classified as handoffs. No empty-mempool substitution, transaction
mutation, storage change or history pruning is introduced.

## Regression scope

The fixture covers the real mempool's missing-fork/publication interval, eight
simultaneous mempool reader/writer handoffs, transaction retention and recovery
for both producer algorithms, persistent refusal after 100 attempts, observed
head changes during assembly, PoB metadata head changes, failed cached-candidate
replacement, and unrelated storage/allocation failure propagation. Existing
assembly, resource selection, failed-transaction retry and PoB head-refresh
checks remain in the same executable. This is scoped concurrency evidence,
not complete lifecycle/sanitizer qualification.

The repaired Linux producer regression passes with assertions explicitly enabled
(`-UNDEBUG` after the Release flags). Candidate r1 source manifest:
`775bb4cbae8462cea397c4fd1e9248279bc64c0fa97a82931e2d4c16bf31385b`.
The 312-input baseline changes only the producer header/implementation, native
main/metrics, producer regression and its test linkage. The original identified
source/build and dependency archives are preserved as read-only inputs. Changed
translation units and their producer regression are rebuilt in an isolated
Linux directory. The complete changed translation-unit closure consists of the
producer implementation, main and the producer test; their header consumers are
checked explicitly. Unaffected native test executables are reused only after
their original source inputs are verified. This is an identified incremental
Linux build, not a fresh rebuild of every dependency and test executable.

All **63 distinct native checks pass**, including the repaired producer fixture,
after preserving the initial failed runs and correcting two environmental issues:
the external build selector must name the configured tree used by unchanged
fixtures, and restore tests need their unchanged standard reserve plus their
retained synthetic data. The four checks that directly invoke the native node
are rerun against the exact replacement executable. The SC5 paired synthetic
experiment completes when run alone with enough free space. Restore policy,
reserve thresholds and native source remain unchanged through these retries.
The split JUnit records and effective-case audit distinguish this from a single
clean 63-test invocation.

Generated failed-fixture data and temporarily relocated inactive build/test
artifacts are copied to My Passport and every archive member is verified before
any remote copy is retired. Original source, current link libraries, the original
node binary and operational services are preserved. A partial export and an initial
hard-link-unaware archive check are retained as diagnostic failures, not accepted
copies. No native candidate rebuild or source change is needed for these harness
corrections; the amended run has used **one** Linux candidate build.

After archiving the completed synthetic fixtures, all 29 temporarily relocated
unchanged test executables and all 8,308 members of the inactive predecessor
build are restored to their original locations. Every restored regular file's
bytes and mode, and every symbolic-link target, are verified. The complete
external archives remain preserved.

Candidate executable SHA-256:
`0bd4c3666a93a3b43d7c4c7e72776d9d935eec1a33cdeec22572ac71022d9447`.
Its 44,887,320 bytes are rehashed after transfer. The final link audit records
**168 actual archives**, including the rebuilt producer archive, the new main
object, compiler identity and every source input. Native version/help checks
pass. The exported build reports
`1.3.0-dev.0+d3cfb94455da4a472f651eb4239abbc289e74225`; the source manifest and
binary digest identify the working-tree changes beyond that Git basis.

## Runtime evidence

New per-process log counters distinguish prepared candidates, PoB evaluations,
observed head/mempool deferrals and errors. They count returned producer results,
not all internal operations or accepted blocks. Canonical block inclusion and
irreversibility require independent evidence.

The amended mainnet run starts at **16:53:49.891 UTC**, with preparation ending
by **17:08:49.891 UTC** and a hard stop of **18:53:49.891 UTC**. The actual
preparation completes before its deadline. The candidate is staged separately;
its hot-key guard, dynamic libraries and unchanged configurations pass validation.
Observer readiness takes **65.637 seconds**, with production explicitly disabled.
Head/LIB advance from **40,317,946 / 40,317,886** to
**40,317,960 / 40,317,900**. Independent canonical LIB agreement, registration,
matching hot-key bytes and available VHP pass before deliberate activation.

The producer observation runs from **16:57:11.701 UTC** to
**17:57:11.701 UTC**: **3,600.00018 seconds**. The complete window passes without
a producer error, OOM, swap use or breached resource guard. The run uses **one
Linux candidate build, one new commissioning and one new producer start**;
there is no corrective restart or extension. Linux build/CLI/all-63-check
qualification is already recorded by **16:48:11.767 UTC**, before the immutable
**17:57:49 UTC** development deadline. The original failed attempt retains its
separate deadlines, one-start counter and failed outcome.

The last periodic counters inside the fixed window show **1,178 prepared
candidates and 303,541 PoB evaluations**, one head deferral, 29 mempool deferrals
and zero producer errors. These are lower bounds on preparation/evaluation
activity, not exact end-of-window totals: periodic logs precede the final sample.
The complete raw window records **31 completed handoffs: 30 unavailable-mempool
parents and one changed head**. Every streak recovers after one retry; the
maximum logged recovery interval is **36.628 ms**, with no unpaired deferral.
The full producer log has no producer warning/error/fatal entry.

The first/last captured producer head/LIB advance from
**40,317,965 / 40,317,905** to **40,319,079 / 40,319,019**. All 58 intervals
between the 59 periodic LIB samples advance without a backwards LIB observation.
Steady samples retain at least three peers and finish with four.

Both the controller and an independent scan of the complete raw fixed-window
log find **zero produced blocks**. Therefore no canonical inclusion,
irreversibility, actual successful submission or Fogata authorization of a real
eligible block is demonstrated. The observation is not extended to obtain one.

## Whole-node measurements

The unchanged host is Ubuntu **26.04.1 LTS**, x86_64, kernel **7.0.0-30-generic**,
ext4, two vCPUs and **4,000,034,816 bytes** kernel-reported physical memory.
Swap remains absent. The strict whole-server budget is **40,000,000,000 bytes**.

| Measurement | Complete producer window |
| --- | ---: |
| Resource samples | 577 |
| Kernel native RSS high-water / maximum sampled steady RSS | 223,084,544 bytes / 223,084,544 bytes |
| Maximum service-cgroup memory peak | 276,271,104 bytes |
| Minimum concurrent host MemAvailable | 3,230,367,744 bytes |
| Maximum whole-server requirement including recovery reserve | 38,186,901,504 bytes |
| Minimum margin inside the strict 40-GB budget | 1,813,098,496 bytes |
| Minimum root available space | 11,364,605,952 bytes |
| Recovery reserve maintained | 8,589,934,592 bytes |
| Maximum allocated database bytes | 24,229,888,000 bytes |
| Mean native CPU / fraction of two-vCPU capacity | 18.879% of one CPU / 9.439% |
| Kernel process read-byte / write-byte increases | 56,418,304 bytes / 57,516,032 bytes |
| Successful local head RPC calls | 345 |
| Successful local head RPC mean / p95 / maximum | 2.085 / 3.405 / 8.929 ms |

The CPU/I/O interval spans 3,596.974 sampled seconds. Service-cgroup mean CPU is
18.879% of one CPU; two brief quota-throttling events total 1,041 microseconds.
I/O values are kernel process counters, not whole-device throughput. RPC latency
statistics cover successful calls and do not establish a failed-call rate.
All sampled host OOM/swap counters remain unchanged, native swap stays zero,
and service-cgroup OOM counters remain zero. The preceding observer has a
68,349,952-byte native RSS high-water mark.

Whole-server accounting charges `raw device bytes - root available bytes +
required recovery reserve`, including unavailable filesystem space, other
partitions, OS/software, both binaries, databases/WAL, logs and temporary files.
The physical device remains **40,961,572,864 bytes**; its capacity above the
40-billion-byte budget is not credited. The reserve is
`max(8 GiB, allocated database bytes / 5)`; its 8-GiB floor dominates throughout.

The repair replaces the ordinary five-second error backoff only for classified
short handoffs with a 10-ms retry and extra head checks/diagnostics. The earlier
failed 21-minute window and this complete hour differ in duration, chain inputs,
peer activity and cache lifetime. Their RAM/CPU/I/O/latency values are contextual
measurements, not a controlled savings or regression comparison.

There are **230 other native warnings**: 214 failed outgoing peer connections
and 16 stale-peer removals after stream-protocol negotiation failure. These
categories also exist in the original failed log. No logger, severity, network
configuration or exception handling is suppressed to pass qualification. Usable
peers, advancing head/LIB, independent canonical agreement and resource guards
pass separately; this result does not certify every configured seed/remote peer.

## Final identity, state and rollback

A post-window read-only audit rehashes the actual running executable through
`/proc/PID/exe`, the staged file and original rollback binary. It verifies the
sole database owner, both unchanged configuration digests, existing hot-key
guard, five unchanged mapped runtime libraries and host/resource specification.
Five historical samples retain local receipts and match independent
irreversible canonical IDs, including height 1 and the selected backup revision.
These samples do not repeat the preceding complete archived-record scan. No
storage implementation/configuration, database, archived record or history policy
is replaced or pruned by this producer repair.

The fixed-window final native head **40,319,079** and its complete state root
also match the independent irreversible canonical chain. Its canonical successor
**40,319,080** names the captured native block as parent and contains the identical
previous-state root; reference LIB **40,319,242** makes both comparisons
irreversible. This is a read-only verification of the captured endpoint, not an
extension of the producer observation or proof of an own produced block.

| Configuration identity | SHA-256 |
| --- | --- |
| Retained observer configuration | `1b9c2e798ab7c8537401fd1c28e44dc38b72dc6faf5a49be7b11ce448254a74f` |
| Existing producer configuration | `43178653db96f6d86503b9e747eba206a7e0a2eb8fd29803066f841668da4d02` |
| Retained original executable | `f086e2d71c04cc0922ccd497a2cb1c63d5265bc1d3190b2e17e1156b5a7d10f4` |

Compilation uses **GCC 13.3.0**, identified as Ubuntu package
`13.3.0-6ubuntu2~24.04.1`; the compiler executable, all 168 actual link archives,
new main object and 312 source inputs are individually fingerprinted in private
evidence. Current source and original generated build files are reverified after
their preservation/restoration. The candidate's source/binary digests and full
native version are recorded above.

Final read-only mainnet checks retain the same pool/owner/settings/registration,
**1,000 VHP**, the approved **900-KOIN reserve** and **100 KOIN in the owner
account**. No additional funding, manual burn, operational wallet/key/pool,
custody-key deployment or recovery email is introduced.

The repaired producer remains active as ordinary operation with no restart.
The finite controller has exited and its deadline timer is cancelled only after
the terminal pass. The original observer and old producer stay inactive; the
original binary and observer launch remain intact. The producer uses `Restart=no`
and is **not enabled for boot**. Automatic restart/boot activation is not added
by this finite commissioning.

Rollback is a sequential stop of the repaired producer, verification that it no
longer owns the database, and start of the preserved original observer with
explicit `--disable block_producer`. Verify mainnet identity, advancing head/LIB,
peers and resource margins afterward. If corruption or a persistent state-merkle
mismatch is present, preserve the database and collect diagnostics before recovery.
Exact private unit/path commands are in `evidence/OPERATOR-RESULT.md` in the owned
run; they are not public deployment examples.

Native SemVer remains `1.3.0-dev.0`. There is no new integration commit, submodule
update, release or Koinos One packaged binary. The Git basis alone does not contain
all candidate changes; a future integration must preserve the identified source
snapshot and deliberately establish its own commit/pin/build checks. Koinos One's
producer manual/changelog are coordinated and its strict manual build passes.

Generated evidence is owned by
`/Volumes/My Passport/teleno-producer-repair-20261018-flnbdgu4/`. It includes
`qualified-linux-candidate-audit.json`, `native-distinct-case-audit-independent.json`,
`source-link-audit-20261018T171957Z.json`, `terminal-native-measurements.json`,
`other-native-warnings-reviewed.json`, final runtime/pool audits and
`completion-finalization-verified.json`. The final state-root proof is
`final-captured-head-canonical-state-root-proof.json`; post-window ordinary
producer health is recorded separately. The 25-file terminal archive is copied
losslessly, every member is byte-verified and its private-key scan passes;
SHA-256: `25e068f0b5a0916170715db7dc791eac7e081afbde1efa5a576cd16fa327e120`.
Operational account, host and key bindings remain private.

## Subsequent own-block confirmation

After finite commissioning closes, the operator explicitly requests continuing
observation until an own block is detected. The same identified candidate and
producer process remain running in ordinary operation. This is separate sampled
observation, not an extension or reset of the closed one-hour test.

The native log records **block 40,323,375** at
**2026-10-18 21:35:48.815248 UTC**, containing **zero transactions**:

`0x122040a9d21cb3eacc82c227de8bdd2a1cea5c966f761414038b0793d8b2b4295d34`

Direct local and independent mainnet reads return this same canonical ID and
the selected pool signer. At the complete **21:40:57 UTC** proof, independent LIB
is **40,323,411**, beyond the produced height. This proves real native PoB
submission and canonical irreversible acceptance for this block. It does not
prove a nonempty production workload or close all remaining issue-9 criteria.

There are **21 complete health samples** from **18:18:31** to **21:40:57 UTC**.
Maximum sampled RSS is **265,154,560 bytes**, kernel RSS high-water mark is
**280,399,872 bytes**, minimum sampled host MemAvailable is **3,245,109,248 bytes**,
and minimum strict whole-server margin, including the held 8-GiB reserve, is
**1,793,196,032 bytes**. Every complete sample has at least seven peers and zero
native producer errors, with no host/cgroup OOM or swap. These are periodic
samples, not continuous CPU/I/O/latency measurements; the earlier measurements
remain bound to their original fixed window.

One initial own-block probe fails in the private monitor, which decodes the RPC
signer address as Base64 although the response contains a Base58 address. Both
raw local and independent signer strings match the selected pool. The original
monitor/error/control and independent full-block/receipt reads are preserved.
Only this field decoder is corrected, with actual-header positive and
changed-address negative checks; the complete read-only probe then passes all
identity, canonical, irreversibility, activity and resource checks. No native
source, binary, consensus rule, configuration, service or key is changed, and no
verification condition is weakened.

The same producer remains active with **zero restarts**; no additional producer
start or financial transaction occurs. The ten-minute heartbeat is verified
**paused** after success. Existing observer rollback remains available.
Private evidence is selected by `evidence/continued-production-current.json` and
includes `own-block-proof.json`, `completion-result.json`,
`signer-address-decoder-correction.json` and `automation-paused-verified.json`.
The previous terminal commissioning controls/results remain hash-verified
unchanged. Issue 9 remains open.
