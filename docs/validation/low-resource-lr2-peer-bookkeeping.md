# LR2 P2-D: peer scores and optional candidate retention

Date: 2026-10-16. Status: **implemented locally; current native and scoped
sanitizer/VPS/container checks pass; P2 and issue #9 remain open**.

This uncommitted checkpoint belongs to `codex/lr2-memory-hardening`, based on
`d3cfb94455da4a472f651eb4239abbc289e74225`. It corrects native peer-bookkeeping
behavior without changing block/transaction validity, wire formats, history,
receipt retention or production activation. It is separate from the installed
public-backup publisher and is not a packaged Koinos One update.

## Reproduced failures and review correction

Preserved old executables and sources demonstrate:

| Case | Reproduced result | Correction |
| --- | --- | --- |
| Same IP, changing TCP port and peer ID | A scored address reconnects through a different full address, exit 1 | Key by the first TCP/DNS network component; normalize numeric IP spellings |
| Extreme accumulated score | Unsigned addition wraps, exit 1 | Saturate at `UINT64_MAX`; clamp floating conversion before the out-of-range case |
| Idle scored address after actual half-life | Reconnect still refused after 62 seconds with a one-minute half-life, exit 1 | Project decay when checking admission, without requiring another error |
| Discovery disabled, repeated connect/disconnect | 128 optional candidates retained with a configured limit of 8, exit 1 | Trim after every candidate mutation and disconnect, preserving protected owners |
| Allocation failure in discovery snapshot | Periodic worker terminates, controlled exit 91 | Catch local allocation exhaustion at the periodic boundary; count it and continue on the ordinary interval |

Review of the first P2-D implementation also reproduced repeated-query integer
rounding eroding an existing ban, exit 1. Queries, zero-score events and unrelated
GC now project from the last **score-changing** event without mutating that
anchor. A thousand nanosecond-spaced queries/zero events must preserve the
expected 999 score. These are scoped reproductions, not a claim that every
allocation failure, peer policy or transport lifetime is corrected.

## Error-record contract

Two native YAML settings have positive finite defaults:

```yaml
p2p:
  max-peer-error-records: 4096
  peer-error-key-capacity-bytes: 1048576
```

Zero, negative, fractional, boolean, container, null, overflow and malformed
values are rejected even when ordinary configuration parsing is lenient. They
are not CLI flags or GUI controls. These defaults are provisional component
policy, not a qualified 4-GB runtime profile.

The retained map is bounded by record count and the sum of actual retained key
string capacities, including the implementation's small-string capacity.
Borrowed lookup checks precede key allocation; actual capacity is checked before
publication. Map/node overhead, caller address storage, allocator retention,
transport queues and pending connections are additional. The handler retains
only scalar options rather than copying the configured seed vector.

Scores belong to the first `/ip4`, `/ip6`, `/dns`, `/dns4`, `/dns6` or `/dnsaddr`
component. Ports and trailing peer IDs do not split a score. Numeric addresses
use allocation-free canonicalization; DNS values remain exact without resolver
calls. Peers without an available transport address use their identity as the
native fallback. IP-wide scoring also groups clients behind a shared NAT; it
must not be described as independent scoring of every connection.

At most 16 records are examined by each maintenance call. Only a projected
zero score is disposable when the configured thresholds permit it. Nonzero
scores, including scores below the reconnect threshold, are never discarded
to admit a new identity. Saturation or allocation failure refuses new unknown
bookkeeping locally and closes the affected connection **without peer blame**.
Existing records remain evaluable. Occupied capacity can deny legitimate new
or seed addresses until scores expire: this is an explicit availability tradeoff,
not a claim of adversarial fairness or fully reserved connection admission.

## Candidate and periodic-worker contract

Optional dial advice prepares strings before map publication; allocation failure
leaves no partial advice record or capacity charge, while the active generation
retains its own peer metadata. Existing entries prepare replacement strings
before swapping them. Trimming runs independently of discovery configuration.
Configured seeds and connected candidate records remain protected. The effective
count bound is `max(configured limit, protected candidate count)`; an existing
zero candidate limit continues to mean unlimited. Candidate string capacities
are attributed, **not bounded by a byte budget**.

The periodic worker catches scoped `std::bad_alloc` without allocating a log
message or penalizing a peer. It waits the ordinary interval before the next
cycle. This does not make the entire acquisition cycle failure-atomic: dial
attempt timestamps can advance before a later vector allocation fails. It does
not bound transport/SDK known-peer repositories or snapshots, pending dials,
synchronous DNS, callback queues, all logging or every worker launch failure.

`P2PNode::bookkeeping_stats()` exposes a locked C++ snapshot of candidates,
configured/effective limits, protected candidates, attributed string capacity,
evictions, candidate/discovery allocation refusals and error-record counters.
The periodic `[p2p] Peer bookkeeping:` line prints the current counts, capacities,
limits and refusals. This is not a new RPC/CLI endpoint, aggregate memory budget
or heap/RSS measurement.

## Reference provenance and compatibility boundary

The local Knodel vendor copy was checked byte-for-byte against official
[koinos-p2p error handling at e2267ba](https://github.com/koinos/koinos-p2p/blob/e2267ba230960b5e4100c16ad84c42cfc12eec4b/internal/p2p/error_handler.go).
The upstream Git blob is `f5e1a9e1e1e02ad05fe6759be6c5d7cba05e6c42`;
source SHA-256 is
`25632808e168b1f6a6c87ca81a4e91521dc91ed1d01a5cc67cbda0581d071876`.

The network-component key and single-query exponential decay follow that
reference. Sixteen arithmetic vectors match independently compiled Go
adaptations on both Mac and Linux. The read-anchor correction intentionally avoids the Go
implementation's repeated-read truncation behavior. This is neither bit-for-bit
peer-policy equivalence nor a full legacy-node differential test. Saturating
scores and local capacity backpressure are explicit policy hardening; consensus
validity and encoded protocol bytes remain unchanged. Historical H1 is open.

## Validation and source identity

Owned evidence root:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/p2d/`.
Builds, temporary files, fixtures and compiled probes use the verified external
volume. Linux work uses isolated owned development data, one low-priority build
job and unchanged 10-GiB free-disk / 2-GiB available-RAM builder reserves.
Existing service configuration, operational databases and producer keys are not
fixtures. The Linux miner is a larger host, not the mandatory 4-GB/40-GB VPS.

The frozen export contains 291 native/build/test/config inputs. Manifest SHA-256:
`10b59877afabe323399ca5a8803a33256ea5f6208803a87640de267f9878027a`.
Source archive SHA-256:
`33c24ccb24c5edfb668bfd64f1db6c8edae20e3644e81586ee4f4593917a0924`.
Later report/operator/guidance edits are documentation-only; exact input and
final scope audits distinguish them from the frozen tested runtime.

Final reviewed focused checks pass 3/3: configuration, native peer-sync and
peer-error handler, 30.39 seconds. Mac all targets, version/help and complete
**56/56 CTest pass in 186.07 seconds**. A separate actual-clock 62-second decay
check passes. Native identity is `1.3.0-dev.0+d3cfb94455da-dirty`; Mac executable
SHA-256 is
`26fde63fd0efc134b41089bedbb9e35f5d5000122c837379ebf82b9cd05bbf5d`.

Tests include alias/port/IPv6 ownership, backward clocks, zero thresholds,
saturation, exact bounded sweep work, all-nonzero retention, count/key capacity,
one-shot key/map allocation failures, concurrent admissions, protected seeds,
active-generation metadata, disabled-discovery churn, pre-worker local refusal,
four optional-candidate publication allocation sites and discovery recovery.

Linux all targets, version/help and actual-clock decay pass. The full roster
passes **56/56 in three exhaustive batches (54/1/1)**, with CTest times
121.81 / 1.94 / 8.15 seconds (131.90 seconds summed). The unchanged source and
native executables serve all batches; no CTest runtime fixture/dependency links
were omitted. Completed fixtures were streamed directly to My Passport, verified
member-by-member and only then retired from the builder, preserving its reserve.
This is not a single invocation of the complete suite. Linux native executable
SHA-256 is
`cfb5a0d1bcb407d52f8f6cbf618bf0586a80e7237fb8bfb965760cb0e2d8f700`.

Separate **ASan, UBSan and TSan each pass eight current fixtures**: peer-error
handler, peer sync, configuration, RPC client/read/write/transport and GoRPC
codec. Each diagnostic has a fresh clean/known-fault control; expected failures
are verified as failures. All 44 native/diagnostic command steps match their
expected results. Diagnostic children alone use ASLR-off; prebuilt SDK libraries
remain uninstrumented, and TSan's unsupported atomic-fence warnings remain an
instrumentation limitation. This is scoped evidence, not all-node race freedom.

The actual replacement VPS passes **11 normal-ASLR CLI/synthetic component
commands**, including the producer mock, new bookkeeping and configuration tests.
It retains 2 CPUs, 4,000,034,816 RAM bytes, no swap and ext4; sampled host headroom
stays above the 768-MiB/10-GiB guards. These short isolated children are not a
full-history observer, catch-up, sustained producer or whole-machine qualification.
The matching native binary also passes version/help/hash as a non-root, read-only,
network-disabled runtime container. Image identity is `sha256:7b51ece5953e4600595506ee6ba05a73de5df55ce9219fe89ca9413f30d35248`.
This uses a retained pinned runtime base; it is not the complete repository
Dockerfile/dependency/release workflow or a deployment.

Raw source, compiler/cache/command logs and original normal/scoped diagnostic
executables are preserved in the verified external archive
`linux-final-raw-evidence.tar.gz` (`195` members, SHA-256
`077ee7ad0ed5460c916d7459321c651030d10a01e89d7a20b6c253e8059fb917`). The three complete native fixture archives and their inventories,
member proofs and retirement receipts are retained beside it. VPS raw evidence
and its duplicate-retirement receipt are private external artifacts. Earlier
P2-C results retain their own source identities.

The coordinated Koinos One manual passes `mkdocs build --strict`; the rendered
page contains both finite native settings, the current manifest and both platform
binary hashes. Final preservation checks retain branch/HEAD/index in both repos,
357 unchanged pre-existing Teleno files and 578 Koinos One files; only the scoped
21 native/report paths and one manual page differ. AGENTS/CLAUDE are identical.
All 289 non-guidance frozen inputs remain unchanged; the two guidance files
contain only the later documented checkpoint update.

After full external verification, 36 unused duplicate evidence executables/
transfer copies (334,044,011 logical bytes) were retired from the builder.
Original native/diagnostic build trees, source, caches, metadata, logs and images
remain. The external member inventory and retirement receipt identify exact
copies; no operational database or installed binary is retired.

## Paired component measurements

Five alternating fresh-process before/after pairs per cohort apply the same
8,192 positive-score events to actual old/current headers. Mac medians:

| Cohort | Retained C++ requested bytes, old → new | Allocation attempts, old → new | Fixture time, old → new |
| --- | --- | --- | --- |
| One IP, rotating ports | 851,968 → 72 | 16,384 → 1 | 1.468 → 1.566 ms |
| 8,192 distinct IPv6 addresses | 851,968 → 294,912 | 16,384 → 4,096 | 1.257 → 3.202 ms |

Five corresponding Linux pairs use the same event cohorts and actual headers:

| Cohort | Retained C++ requested bytes, old → new | Allocation attempts, old → new | Fixture time, old → new |
| --- | --- | --- | --- |
| One IP, rotating ports | 860,160 → 80 | 16,384 → 1 | 4.688 → 3.837 ms |
| 8,192 distinct IPv6 addresses | 900,013 → 408,493 | 16,384 → 8,192 | 6.255 → 11.575 ms |

Current retained records are 1 / 4,096 respectively; the distinct-address cohort
has 4,096 local capacity refusals. All scoped tracked allocations return to zero
on handler destruction. This compares deliberate policy changes, not identical
admission decisions. The bounded sweep adds CPU work, especially for unique
addresses; the Linux unique-address fixture takes about 1.85 times the old time.
The larger miner has existing background services; these timings are component
observations without isolated-host or whole-node throughput claims.

The tracker reports ordinary C++ allocation requests. It excludes prebuilt input
strings and tracking-prefix/allocator/aligned overhead; process peak RSS includes
those and startup. Mac median peak RSS was 8,536,064 → 8,421,376 bytes for both
cohorts. These tiny process measurements do not establish a node RSS saving,
whole-machine OOM safety, sustained network throughput or target-host capacity.
String small-buffer layouts and allocation costs differ across platforms.
Linux reported the same 19,038,208-byte process peak across cohorts/modes;
process startup/launcher high water and input storage are additional, so this
value establishes no RSS reduction. The Linux results are authoritative for
this component's platform tradeoff, not the mandatory whole-machine profile.


A supplementary set of five Linux pairs captures actual per-child `wait4`
user/system CPU and input/output block counts, retaining the first measurement
set separately:

| Cohort | Child user + system CPU, old → new | Median (input, output) blocks, old → new |
| --- | --- | --- |
| One IP, rotating ports | 10.795 → 9.005 ms | (0, 8) → (0, 8) |
| 8,192 distinct IPv6 addresses | 11.196 → 17.054 ms | (0, 8) → (0, 8) |

CPU includes process startup, input generation, handler work/destruction and
printing; I/O counters include diagnostic output and cache effects. They are
not isolated function CPU, payload storage I/O or node throughput. All 20
supplementary samples free their tracked handler allocations; exact rows,
minor/major faults, compiler commands and executable hashes are preserved.

## Remaining gates

P2 stays open for SDK/transport/decoded/caller aggregates, known-peer snapshots,
pending dials, complete lifecycle cancellation, watchdogs and mixed-load bounds.
D1 still needs integrated state/mempool/VM/RPC and whole-node concurrency proof.
B1 needs the complete current dependency/release/container workflow, not only
warm native or isolated runtime checks. Q1/H1 and sustained producer operation
on the user-designated VPS are open.

The independently checked public backup has corrected checkpoint identity after
the isolated publisher update. The [2026-10-15 capacity assessment](restore-40gb-current-baseline.md)
records a 4,140,425,216-byte restore deficit at the unchanged reserve. This checkpoint neither deploys its mixed
binary there nor proves complete history, full-size restoration or operation
within the 40-GB whole-server envelope. No pruning, receipt removal, reserve
reduction, larger baseline or production activation is used to manufacture a pass.
