# Frozen C9: isolated physical-VPS qualification and M1 coverage audit

Date: 2026-10-20; terminal reconciliation 2026-10-21. Status: **restore and
observer readiness pass; observation stopped early, no reopen; closed
unqualified for M1**.

## Terminal observer result

The owned C9 observer stopped cleanly at **2026-10-20 18:32:57.567331 UTC**;
the coordinator recorded `stopped-unqualified-preserve-evidence` at
**18:32:58.009720 UTC**. The stop reason was the additional **1-GiB early-stop
margin**, not consumption of the required recovery reserve. Final strict disk
margin was **998,899,712 bytes**, including the full **8,589,934,592-byte**
recovery reserve within the **40,000,000,000-byte** whole-server budget.

Readiness at 18:11:27 UTC establishes independent canonical head agreement.
Only about **21.5 minutes** of mixed read-only observation followed; the full
60 minutes, clean reopen and optional controlled-chain pair did not complete.
The run consumed **one observer start**, **one native stage**, **one activation**
and **zero producer starts**. No failure restart occurred. A read-only OVH
check during reconciliation finds no native test observer or running attempt
unit; the restored database and original evidence are preserved.

| Entire first-observer phase | Terminal measurement |
| --- | ---: |
| Elapsed, including catch-up | 6,394.762 s |
| Kernel peak native RSS | 421,789,696 B |
| Minimum sampled host MemAvailable | 2,960,740,352 B |
| User / system CPU | 4,239.850 / 43.858 s |
| Physical input / output, 512-byte blocks | 5,466,456 / 5,760,112 |
| Minimum one-second sampled strict disk margin | 1,048,076,288 B |
| Final stop-time strict disk margin | 998,899,712 B |
| Recorded host/cgroup OOM and swap | None |

The stop-time disk value is lower than the one-second sample minimum; neither
is a continuous disk bound. Peak RSS and available-memory measurements apply
to this incomplete C9 workload. They do not establish representative M1 policy,
steady whole-node behavior or qualification of later C11 source. Terminal
reports are `evidence/remote/sequence-result.json` and
`evidence/remote/observer1-summary.json` in the owned root below. Original
pre-terminal snapshots remain historical; pending/running descriptions below
are superseded by this result.

This report belongs to the unchanged
[finite additional-VPS run](../implementation/issue-9-m1-isolated-vps-qualification.md).
It does not replace the closed C9/C10 implementation result or qualify the
operational producer. Native source and candidate build changes are excluded
from this run. Its owned data root is
`/Volumes/My Passport/teleno-m1-ovh-qualification-20261020-sgjo6v1a/`.

## Exact candidate and input

- Source checkpoint: `d424264b24595786fdba86d9eddbcbf1635284b6`, plus the
  frozen 450-file C9 source archive, SHA-256
  `40c28493e7ff0c2985d85f2ce1a278c47e209e5e23d16af1bbe2641070eab9d5`.
- Final C9 native ELF SHA-256:
  `bea7856f367d008e6ab43a1b9ecb016cab1623197e60dd928b936150347d0aa8`.
- Observer configuration SHA-256:
  `c987f917fe4ad9a3877adbab8cc1feb4d0af0bbb73ae80cd2a6f7e05965c33f3`.
- C9 preflight source check: **287/287** selected runtime/build/script/test
  files matched the frozen manifest. Later C11/C12 source development is
  separate; no running C9 executable or configuration changed for this run.
- Dependencies and normal/sanitizer identities: retained C9 manifests under
  `inputs/`, with the [C9 qualification report](low-resource-lr2-m1-nonce-read.md).
- Selected full backup: `20261016T201050Z-ms-1792181450480-files-380`,
  **383 files / 24,114,178,822 payload bytes**. Manifest/inventory SHA-256 values
  match the independently retained pins. The existing HTTPS/unsigned-source
  policy is explicit; object/hash agreement does not constitute a signature
  or historical execution proof.

The additional physical host is Ubuntu 26.04 LTS/x86_64/ext4, two CPUs,
4,000,022,528 physical RAM bytes and no swap. Its physical device is 40 GiB;
the acceptance calculation still uses **40,000,000,000 bytes** and includes
all unavailable/non-root space plus `max(8 GiB, DB bytes / 5)` free reserve.
It does not enlarge or replace the original designated producer baseline.

## Current execution evidence

Version/help and both startup and metadata-only restore preflights pass.
The initial metadata argument refusal is preserved and its harness-only
correction is recorded in the plan. Native single-copy staging began at
**15:56:41 UTC** and completed successfully at **16:22:44 UTC**. All 383 files /
24,114,178,822 bytes transferred and verified, without reuse or a second payload
repository. The staging result alone is `download_complete`; its subsequent
offline validation/activation started at **16:22:46 UTC** and completed at
**16:46:18 UTC**, returning zero with `observer_hold=true` and the attempt
state `activated`. It checked **46,458,363 records**, nine column families
and 917,316 state objects. Restored state height is 40,236,770, with highest
stored block 40,236,830 and zero input WAL bytes. This is structural validation,
not full historical execution. The observer started at **16:46:22 UTC**.
Readiness passes at **18:11:27 UTC** after advancing canonical agreement:
local and independent head **40,375,538**, LIB **40,375,478**, identical head
ID/root and zero height lag. The common irreversible block ID also matches.
The independent API omits that receipt's state root; its serialized receipt
hash is not a receipt-equivalence proof. The full one-hour read-only mixed
observation started at **18:11:27 UTC** and stopped early as recorded above;
the requested complete observation and clean reopen are unproved. These
results belong to unchanged C9, not C11/C12.

| Completed stage measurement | Result |
| --- | ---: |
| Elapsed | 1,563.296 s |
| User / system CPU | 213.901 / 78.256 s |
| Kernel peak native RSS | 64,057,344 B |
| Minimum sampled host MemAvailable | 3,337,166,848 B |
| Maximum sampled whole-server requirement, including reserve | 37,617,991,680 B |
| Minimum sampled margin under 40 decimal GB | 2,382,008,320 B |
| Physical input / output, 512-byte kernel blocks | 41,631,272 / 47,102,840 |
| Host/cgroup OOM or swap | None |

These are staging-phase measurements, not whole-node runtime measurements.
Native allocated staging bytes are 24,116,613,120; the complete reserved/headroom
accounting is separate from that payload allocation.

| Completed offline validation / held activation | Result |
| --- | ---: |
| Elapsed | 1,412.306 s |
| User / system CPU | 1,268.133 / 98.928 s |
| Kernel peak native RSS | 64,163,840 B |
| Minimum sampled host MemAvailable | 3,476,213,760 B |
| Maximum sampled whole-server requirement, including reserve | 37,619,277,824 B |
| Minimum sampled margin under 40 decimal GB | 2,380,722,176 B |
| Physical input / output, 512-byte kernel blocks | 335,560,976 / 240 |
| Host/cgroup OOM or swap | None |

The final activated target occupies 24,115,023,872 allocated bytes; retained
attempt metadata occupies another 1,617,920 bytes. No second payload repository
was created. These terminal restore results do not qualify later runtime
growth, compaction, concurrency or C11/C12. The exact activation report and
one-second samples remain in the owned evidence root.

The finite coordinator requires a passing native stage and matching pinned
hashes before offline validation/held activation. It then requires observer
readiness before a full one-hour read-only mixed observation and one clean
reopen. Resource guards retain all failed evidence; no failed observer is
automatically restarted. No producer, signing, funds or operational-host
operation is part of this sequence. Runtime and final measurement tables will
be populated only after inspecting terminal phase reports.

## M1 ownership evidence: what is covered and what is still unproved

| Allocation or lifetime | Current C9 mechanism/evidence | Remaining proof boundary |
| --- | --- | --- |
| Shared pending-record strings and final caller/ancestor owners | Immutable owners, owned allocator headers and side-table leases; shared aliases do not reserve again, and physical deletion releases credit. Focused owner/retry tests pass. | Attributed reservations are not allocator heap/RSS; representative retained-owner counts and all handoffs still need qualification. |
| Maps, keys, nonce/RC metadata, tombstones, fork/index policy state | Native owned allocators and scoped C++ allocation capture; allocator-bearing container copies propagate the domain. C9 boundary/concurrency and fork tests pass. | The complete inventory must distinguish ordinary admission from protected canonical/recovery allocations and external consumers. |
| Ordinary nonce/RC/count temporary work | Public native read entry points enter the budget; C8's oversized-read control attributes the earlier missing work, and C9 fixes the shorter-predecessor read overflow. | One oversized argument and selected faults do not prove every legal read shape, input size, allocator branch or concurrent output. |
| Native protobuf responses, serialized futures and held execution receipts | Scope-owned C++ allocations survive moves and original-facade destruction; retained-response/future/receipt controls pass. | Large receipt/log shapes, all framework copies and simultaneous heterogeneous callers are not fully demonstrated. |
| JSON-RPC mempool replies | Single-response allocation capture; mixed batches retain per-mempool-item contribution leases until HTTP write completion. Focused normal/ASan/TSan lifecycle checks pass. | Shared-body capacity can also grow for non-mempool material. Its contribution leases do not bound the complete body or establish every framework allocation. |
| gRPC handoff | Native adapter fields retain their C++ owners through response assignment. | Framework serialization/transport follows the handler and is outside demonstrated M1 capture; the relevant M1/R1 boundary needs explicit ownership/admission evidence. |
| Block/LIB, expiry, removal, cleanup and recovery | Prepared reversible publication, owned parser arenas and retained credits; protected operations are counted and quota-exempt. Focused failure and retry controls pass. | Exemption preserves canonical validity but is not a hard aggregate limit; integrated workspace/lifetime and recovery margins still require evidence. |
| VM and dependency allocations | Native callbacks re-enter M1 capture where applicable; declared parser/encoding envelopes are separate from actual native requests. | C allocation, VM instances/global caches and transport consumers need their own explicit boundary and physical measurements. No M1 counter is a whole-process memory ceiling. |

## New reproduced measurement defect: raw-request counter

A bounded header-only Linux diagnostic against the **unchanged C9 headers**
confirms that `OwnedMemoryBudget::Allocator<T>::allocate()` reports the old
raw-wrapper prefix/alignment allowance as `allocator_requested_bytes`.
The C4+ raw allocation primitive returns the malloc/posix_memalign base and
does not allocate that old prefix on the allocator's bypass path. This
therefore mixes a supplementary allowance into the counter labeled as actual
requests. It is an overcount, not evidence of missing credit or a leak.

The diagnostic replaces only the bypassed raw allocation primitive with a
counting malloc/posix_memalign equivalent; it does not change or qualify the
node executable. It was compiled and run once in a separate owned directory
on the existing authorized Linux builder, with 256-MiB/one-vCPU/no-swap child
limits and a 120-second maximum per command. The builder's larger physical
RAM and existing host swap are not reference-VPS evidence. No service or
swap configuration was changed.

| Diagnostic case | Observed raw requests | Reported requested-byte delta | Difference | Final credit release |
| --- | ---: | ---: | ---: | --- |
| Disabled clean control | 37 B | 0 B, intentionally untracked | N/A | Pass |
| Enabled ordinary owner | 69 B | 100 B | +31 B | Pass |
| Enabled 256-byte-aligned owner | 512 B | 783 B | +271 B | Pass |

All three predicates pass; compiler is GCC 13.3.0. Diagnostic ELF SHA-256:
`f5a43b0b8de5798fc7c35e2453406dadc27c7e51eda80047be3907f4b7c25f36`.
The exact source/header hashes, bounded diagnostic counter, command, stdout
and result are preserved under `inputs/allocator-request-audit.cpp` and
`evidence/allocator-request-*` in the owned My Passport run. This is not a new
node candidate build or an attributable RSS/savings measurement.

The follow-up correction should report the actual raw request separately and
retain the existing conservative allowance as supplementary reservation, so
total accounted bytes/admission boundaries need not change. Add an exact
raw-request control for ordinary and over-aligned allocations, final-owner
release and unchanged capacity/refusal boundaries. **Do not modify frozen C9
or silently reinterpret its historical metrics.** A subsequent source repair
requires its own recorded finite candidate allowance under the operator's
standing authorization. No repair is claimed in this report.

## New reproduced ownership gap: transaction-receipt HTTP output

The unchanged C9 JSON-RPC library applies its final-body M1 ownership path
to allowed `mempool` methods. `chain.submit_transaction` returns an M1-owned
native receipt, but its subsequent JSON conversion and final HTTP body do not
enter that path. This makes a concrete next repair necessary.

A synthetic `IChain` fixture creates a 4-MiB log in an M1-scoped protobuf
receipt and returns it. A localhost client pauses response reads. The fixture
has **no controller, ledger, signing keys or external network** and runs in an
isolated network namespace. It proves an interface ownership boundary, not
the validity or frequency of that receipt shape on mainnet.

| Unchanged C9 case | Held HTTP body capacity | Held M1 accounted bytes | After response release |
| --- | ---: | ---: | ---: |
| Owned mempool-response clean control | 9,650,176 B | 10,644,476 B | 977,851 B |
| Single chain-receipt response | 4,538,368 B | 240 B | 240 B |
| Mixed chain-receipt / chain-ID response | 4,546,560 B | 240 B | 240 B |

All three clean/gap predicates pass. The native receipt's attributed peak
reaches 4,211,801 bytes, then returns to the 240-byte bootstrap while the
HTTP body remains held. This is **missing attribution, not a measured leak**.
The mempool control retains some cold immutable metadata after response
release; it is not an assertion that its entire accounted value returns to
the bootstrap.

The diagnostic source SHA-256 is
`84b41dbd332d06f1a06576456116819860d3dd7031c1b7272394c6e619247a99`;
the executable SHA-256 is
`a5e2e8e534dae617c0c1f331cb66e8ac7766751580f53c8cc7af62bea3d676ec`.
All 46 linked object/library inputs match C9's recorded hashes. The final
result SHA-256 is
`12676e87d3d4eb1fd4ef6aa2d03633e9e96e72ebd152c359eda1f2da2dee5a94`.
Source, full link identities, logs, executable and results are preserved on
My Passport in `inputs/chain-receipt-output-gap.cpp` and
`evidence/chain-output-linux/`.

Compilation succeeds. The first link fails because systemd does not inherit
the caller's working directory; that failure is preserved. The one corrective
link uses absolute input paths with the identical object/libraries and passes.
One diagnostic execution then passes the three predicates in 4.439 seconds.
Both allowed diagnostic build/link slots are consumed; no node candidate was
compiled or changed. The diagnostic closed before its unchanged
16:39:59.503452 UTC deadline.

The operator-authorized [separate C11/C12 repair](../implementation/issue-9-m1-receipt-output-repair.md#terminal-result)
expired unqualified at its original deadline. C11's exact OVH component
sequence passes 6/8 invocations; its post-acceptance allocation fault aborts
during JSON destruction and its concurrent-success fixture times out. C12
was unused. This does not change C9's binary, counters or qualification.

## Completion decision

**M1 is not complete.** The raw-request metric needs correction or an explicitly
accurate final interpretation, complete M1 ownership/admission boundaries need
proof, and representative concurrent transaction/output policy and physical
whole-node qualification remain missing. The closed read-only observer
sequence provides incomplete evidence and cannot establish the omitted
workloads. Other issue-9 rows retain their own independent acceptance gates.
