# Teleno application RAM and disk optimization for the operator's own VPS

Date: 2026-10-03. Status: **LR0 local exit satisfied; LR1 local implementation
verified; complete mainnet source/history validation remains open. LR2 local cache/RPC/P2P hardening
implemented; whole-node memory exit remains open. LR3–LR6 proposed.
Final-source whole-node Linux/40-GB qualification remains open.**

Current issue-9 reserve amendment, 2026-10-18: the operator explicitly replaces
the original 10-GiB acceptance requirement with an **8-GiB recovery-reserve
floor (8,589,934,592 bytes)**, retaining `max(8 GiB, database bytes / 5)` inside
the unchanged 40-decimal-GB whole-server budget. The completed final-candidate
[8-GiB VPS qualification](../validation/vps-8gib-recovery-reserve.md) now supplies
selected-input restore and measured-workload disk evidence for original issue 9.
The standard CLI default remains 10 GiB; use the explicit tested profile.
Remaining LR2/diagnostic/historical/producer gates remain open. Earlier reserve
instructions and failed measurements below retain their historical context;
they do not reinstate a 10-GiB acceptance requirement.

Purpose clarification, 2026-10-17: reduce the RAM and disk requirements of
Teleno, the user-maintained open-source C++ Koinos cryptocurrency node, to
lower the cost of operating it on the user's own designated VPS. Profile
application allocations, cache lifetimes, database storage and backup/restore
workspace; implement measured memory-management and lossless storage changes.
The active
target includes the producer workload and complete backup restoration on the
user-designated 2-vCPU / 4-decimal-GB / 40-GB Ubuntu/x86_64/ext4 server.
Profile RAM, CPU, disk and I/O costs; make focused code improvements; verify
compatibility and recovery; measure before/after results. This is software
maintenance and performance engineering in the project's source code and the
operator's own build/test environments. The [reformulated loop](issue-9-goal.md)
preserves the existing
history, reliability, access-control and acceptance requirements. Earlier
observer-only and retired-server descriptions below retain their historical
context; they do not replace the current target.

Persistent-goal follow-up, 2026-10-15: the user requested completion of issue #9
as a durable implementation loop, with My Passport or their miner for builds
and development data. The [goal contract](issue-9-goal.md) and
[completion matrix](../validation/low-resource-lr2-completion-matrix.md)
record the current source-preservation baseline, external roots and remaining
implementation/proof gates. Isolated miner builds/tests are now authorized
after identity/resource/ownership checks; live nodes, global host changes,
operational data and deployment remain outside that authorization. The target
stays Ubuntu/x86_64/ext4, 2 shared vCPUs, 4 decimal GB RAM and 40 decimal GB
whole-server disk. Catalog availability is separate from this engineering
target. LR2 is not newly qualified by setting the goal.

Latest replacement clarification, 2026-10-15: the user replaced the first VPS
and explicitly retired it. The only active baseline is the replacement,
verified through read-only SSH as **2 vCPUs, 4 decimal GB RAM, 40-GB-class disk,
Ubuntu/x86_64/ext4, no swap**. Its dedicated restricted-key login is verified.
The producer must work on this server; the first host is not contacted or used
as a fallback. Original 4-GB RSS/headroom gates apply with producer work included.
The following first-server clarification is retained as retired context.

Subsequent baseline clarification, 2026-10-15: the user selected a specific VPS
and requires the producer to work there. Its verified shape is stricter:
**1 vCPU, 2 decimal GB-class RAM, 40 GB-class disk, Ubuntu/x86_64/ext4, no swap**.
Use that actual host as the active qualification baseline, including producer
work, while retaining this plan's earlier 2-vCPU/4-GB observer evidence as
historical context. The [goal contract](issue-9-goal.md) and current matrix define
the strengthened memory/host-headroom gates; earlier 2-GiB/2.5-GiB RSS ceilings
cannot qualify a 2-GB host. Persistent SSH setup and necessary password recovery
were explicitly authorized and verified. Keep access details private. Existing
production services/data, mainnet activation, source trust, full genesis
history/receipts and observer-first validation remain protected.

User clarification, 2026-10-03: **Teleno must retain the complete history from
genesis.** This supersedes the earlier bounded-history direction in this file.
The proposed `teleno_light` is a [separate deferred concept](teleno-light-concept.md),
not an alternative success path for this plan.

Restore clarification, 2026-10-15: restoring the complete backup inside the
40 GB whole-server envelope is explicitly required alongside the active memory
work. Do not defer it merely because LR3/LR4 retain separate milestone names.
The [fresh source/capacity assessment](../validation/restore-40gb-current-baseline.md)
now records checkpoint-verified public identity after the isolated writer update.
The earlier 4.140-GB reserve deficit and staging refusal are preserved historical
results. The denser frozen B3 backup subsequently passes complete restore,
validation and held activation on the actual VPS at a measured 39,760,650,240-byte
whole-server peak including the unchanged reserve. Its one-hour observer also
passes; the longer run is in progress. These exact-input results do not qualify
the later source candidate or close sustained operation and producer requirements.

## 1. Revised outcome and scope

Development branch clarification, 2026-10-06: keep LR2 memory, ownership and
lifecycle improvements on `codex/lr2-memory-hardening` for now. The local branch
starts at `cfd2eb83b8080ca49911555108ce05f0a9068f01`; `main` remains unchanged.
Do not merge, publish or release these changes without an explicit request.
All pre-existing files are preserved, including mixed backup/restore work.
Uncommitted changes are not permanently isolated by a branch switch: review
their scope and dependencies before any separately authorized commit.
Historical checkpoint branch identities below remain historical evidence.

User outcome clarification, 2026-10-04, retained in the current scope:
the optimized Teleno must demonstrate lower resource use together with reliable
application behavior and recovery. Resource efficiency, correctness, resilience
and existing operational protections are joint acceptance
requirements. Lower memory/disk consumption does not justify weakened
validation, durability, recovery, input checks, access controls or source trust.

For each change, distinguish a reproduced defect correction, a measured resource
improvement and an unverified hypothesis. Exercise relevant failure, overload,
disconnect, cancellation, shutdown and restart paths; prove that rejection does
not leave partial state, duplicate side effects or false success. Preserve
bounded handling of inputs, valid memory ownership, protected administrative
surfaces and observer-first recovery. Record any extra parsing, locking,
allocation or I/O cost rather than assuming that a lower-memory result is an
unconditional improvement. Compare against the release baseline with equivalent
inputs and disclose missing Linux/historical proof.

This clarification strengthens acceptance within the authorized implementation
scope. Regression and memory diagnostics exercise the project's application in
owned test environments. Existing deployment and production-activation
requirements remain in force; passing these tests proves only their stated scope.

Adapt Teleno to launch from an independently stored Koinos Foundation mainnet
backup and remain a verified observer on the selected Linux profile:
**2 vCPUs, 4 GB physical RAM, and a 40 GB whole-server disk**. Avoid a second
complete payload on that disk. Budget the OS, installed runtime, databases,
WAL, compaction, logs, restore metadata, and recovery, not just SST bytes.

This supplements the [single-copy restore plan](single-copy-backup-restore-plan.md).
SC0–SC6 retain their actual results and unresolved gates. LR packages add
resource adaptation; they do not retroactively turn synthetic SC5 evidence
into Linux/mainnet qualification. The subsequent first-milestone request
authorizes LR0 repository-local diagnostics, synthetic fixtures and documentation
handoff only, not LR1+ runtime implementation, VPS modification, audit
remediation, pruning existing data, publication, or producer activation.

The subsequent "Haz LR1" request authorizes LR1's repository-local source,
persistence and publication fixes, tests and documentation. The user also
explicitly requested correcting the native backup writer so future backups
carry consistent checkpoint identity. It does not authorize rewriting old public
metadata, deploying the writer, publishing a replacement backup, operating the
VPS, other audit packages, or proceeding with LR2 before its authorization.

The subsequent "Siguente" request authorizes LR2 repository-local work. Its first
tranche corrects cache invariants/hit allocations, owns/drains RPC sessions and
exposes existing DB memory accounting. This is independent local progress, not a
waiver of LR1 source eligibility or authority to deploy, publish or operate a node.

The subsequent "Procede" request authorizes the scoped LR2 P2P accepted-ID
retention package described in the [P2P report](../validation/low-resource-lr2-p2p-retention.md).
It bounds disposable deduplication metadata, not history or mempool contents.
It does not authorize LR3, other audit packages, server operation or publication.

The subsequent explicit LR2 persistent-goal request authorizes remaining LR2
repository implementation, bounded diagnostics and documentation, including the
[completion matrix](../validation/low-resource-lr2-completion-matrix.md).
It does not waive source/historical/sanitizer/whole-node gates or authorize LR3,
LR4, server operation, publication or GUI/package integration. A further explicit
request selects My Passport for development build, fixture and evidence data;
source remains in this repository and existing node data is untouched.

User priority clarification: optimize and qualify for the Linux VPS, not the
development Mac. The primary target is Ubuntu/x86_64 with ext4, 2 vCPUs and
4 decimal GB physical RAM. Mac/APFS measurements are supplementary diagnostic
evidence only. Select runtime policies from Linux whole-node measurements and
compatibility proof, not Mac-specific speedups. This does not authorize live
VPS operation, Linux VM creation or deployment.

The archival behavior and historical coverage remain unchanged. Add an explicitly
selected low-resource **full-history observer** profile only after its
compatibility and resource gates pass. Preserve genesis/runtime inputs, complete
current state, every block/receipt record in the selected backup, and complete
canonical coverage from the first network block through the restored/advancing
head. Verify that the selected backup actually supplies that history; do not
assume it from a publication label or copied root metadata.

Qualification asks whether that full archive can restore, reopen, catch up and
run for a stated duration within the measured whole-server budget. A smaller
compressed representation is acceptable only when every required historical
record is preserved. No pruning, receipt removal, bounded retention, state-only
backup or archive kept solely elsewhere counts as satisfying this requirement.
No universal minimum or indefinite 40 GB guarantee follows: history and state
continue to grow. If lossless improvements cannot fit safely, report the capacity
blocker and required headroom; do not silently substitute `teleno_light`.

## 2. Point-in-time evidence and blockers

Read-only observations on 2026-10-03, not a completed restore or current mutable
`latest` guarantee:

| Item | Observed or declared value |
| --- | --- |
| Target OS / architecture / filesystem | Ubuntu 26.04.1 LTS / x86_64 / ext4 |
| Target processors / physical memory | 2 / 4,000,043,008 bytes; no swap |
| Root capacity / available bytes at 06:28:29 UTC | 39,956,590,592 / 36,146,835,456 |
| Selected public source | `https://seed.koinosfoundation.org/backups/prodnet/teleno-bootstrap` |
| Concrete metadata ID inspected | `20261002T114351Z-ms-1790941431745-files-487` |
| Declared payload / file count | 29,396,360,941 logical bytes / 490 |
| Current direct estimator, informational only | 29,672,800,256 bytes before operating reserve |
| Existing reserve / recommended free bytes | 10,737,418,240 / 40,410,218,496 |
| Difference from sampled available space | 4,263,383,040 bytes short under existing policy |
| Sampled free minus modeled restore minimum | 6,474,035,200 bytes, before additional runtime and growth |

The manifest has empty declared chain IDs; its genesis inventory hash agrees
with the local mainnet genesis, but that does not authenticate the DB. The
native metadata-only preflight failed with `metadata_authentication`; its
generic error does not identify the first underlying exception. No payload
was fetched. A conventional signature path returned 404; no independent trusted
signer/digest was supplied. Do not infer that no signature exists elsewhere.

Local ignored evidence is in:

- `build-single-copy-evidence/mainnet-source-20261003-9qQ0iV/`:
  `source-review.md`, metadata, response headers, native preflight JSON and
  `informational-budget.json`.
- `build-single-copy-evidence/linux-preflight-20261003-5IHv7C/preflight.md`:
  bounded host/resource observations. Access identifiers are deliberately omitted.

Payload hashes, allocated bytes, actual snapshot head/durable root, Linux
feature build, mainnet replay parity, and sustained capacity are unverified.
Re-read and pin inputs before future execution. Public `head_height: 0` metadata
is not proof of the snapshot's actual height.

## 3. Requirements to change, and requirements to keep

| Area | Revised requirement | Non-negotiable boundary |
| --- | --- | --- |
| Deployment | Install a traceable prebuilt native x86_64 runtime; build dependencies elsewhere | Count installed files and services; no full toolchain/build tree on the target by default |
| Restore | Reuse SC direct streaming, bounded transfer and same-filesystem activation | Fresh owned target; no hidden copy fallback; preserve the independent source |
| Memory | Budget the whole node, queues, VM/module caches and RocksDB together | Cache configuration is not a hard process-memory bound; no omitted validation |
| Disk reserve | Issue 9 requires `max(8 GiB, database bytes / 5)` held free within the whole-server budget | Standard CLI default remains 10 GiB; explicit profile and measured workspace required; no zero-reserve or generic space-check bypass |
| History | Full canonical history from genesis, all backed-up block/receipt records and complete current state | No pruning, rolling window, receipt omission or remote-only archive substitute |
| Services and indexes | Reduce redundant storage only with verified reconstruction/lookup equivalence | Preserve existing historical RPC/P2P behavior and configured service coverage; no new account-history parity claim |
| Logs / backups | Bounded operational logs and independent off-target backups | Keep recovery evidence and required durable metadata; no automatic deletion of user data |
| Failure | Stop admission/catch-up in a controlled, tested manner before resource exhaustion | No false commit, partial index effects, lost recovery marker, or unexplained mismatch |

RocksDB memory includes caches, memtables and iterator-pinned data; smaller
buffers can increase I/O or CPU work. Teleno already shares a block cache across
column families. Measure and budget that existing design rather than claiming
that shared caching is a missing feature. See the primary
[RocksDB memory guidance](https://github.com/facebook/rocksdb/wiki/Memory-usage-in-RocksDB)
and the locally linked 8.8.1 headers for the exact APIs used by this checkout.

### Provisional memory acceptance contract

These are experimental targets, not achieved requirements or new defaults:

- Node steady-state RSS at or below 2 GiB and sampled peak at or below 2.5 GiB
  during restore validation, catch-up, ordinary operation and recovery.
- At least 768 MiB host `MemAvailable` at the minimum observed sample, without
  OOM termination or reliance on added swap to conceal over-budget behavior.
- Record process RSS, allocator/heap evidence where available, cgroup memory
  including file cache when applicable, and host memory together. Account for
  compaction, decompression, WASM instances, thread stacks, RPC/P2P and indexes.
- Bound queues/caches and verify cleanup after overload, cancellation and stop.
  Compare warm-cache throughput with a reference; lowering memory must not make
  the observer fall behind the incoming block rate.

Candidate experiments: shared block cache 128 versus 256 MiB; aggregate
memtable budget 128 versus 256 MiB; one versus two background jobs with one
subcompaction; 32 versus 64 MiB per-CF write buffers. Keep Zstd and durability
unchanged. Check actual `db_write_buffer_size` behavior before adding a shared
`WriteBufferManager`; its stall/cancellation/lock interaction needs tests.
Pinned cache entries and cache admission failures must be handled correctly;
do not turn on strict capacity blindly. Select final values from measurements.

### Replacement reserve policy, not an arbitrary smaller constant

For the low-resource profile, calculate and expose:

```text
required_free = allocated_restore_upper_bound + restore_overhead + R
R = worst_qualified_additional_disk_peak
    + measurement_uncertainty
    + growth_allowance_for_stated_operating_horizon
    + emergency_stop_and_recovery_margin
```

Measure the combined additional peak above the restored DB, including WAL,
flush output, obsolete SSTs pinned by readers/checkpoints, compaction output,
restart/recovery, logs, installed runtime and filesystem overhead. State which
terms are already installed/count against available bytes; do not double-count
or omit them. Model overlapping peaks, not just average per-component sizes.

Calibrate on an isolated larger Linux/ext4 environment first, using the exact
profile, file layout and representative workload. Reject if a conservative
bound cannot be established or exceeds the target's available space. RocksDB
8.8.1 describes `max_compaction_bytes` as **not guaranteed**; tuning it is not
a proof of a hard disk ceiling. The operator-adopted 8-GiB floor has finite
selected-backup/workload evidence; additional workloads, overlapping peaks or
future growth still require recorded review and qualification. Earlier 6.47-GB
arithmetic headroom alone never established that qualification.

The standard policy stays `max(10 GiB, DB / 5)`. The issue-9 qualification profile
uses `max(8 GiB, DB / 5)` through the explicit `experimental-8gib` selection.
Its policy must remain bound to preflight, resume, activation and the
runtime guard. Source declarations alone cannot select a smaller reserve. Stop
new work early enough to complete any accepted atomic write, flush/recovery
and observer-safe shutdown; test the guard on reserved-space/ENOSPC fixtures.
Do not suppress required compaction, remove WAL or weaken fsync to fit.

## 4. Source compatibility and trust design

First establish why the native metadata preflight fails, with useful sanitized
phase/error diagnostics. Do not patch downloaded declarations and call them
publisher metadata. Prefer a coherent versioned publication with explicit
chain/durable-root identity and authenticated inventory.

If supporting the observed older publication locally, design a narrowly scoped
legacy-metadata adapter. It must require independently expected mainnet genesis
and chain identity, pin the original inventory/manifest bytes, and compare the
actual restored DB chain/durable-root identity before activation. An absent
legacy declaration is distinct from a contradictory declaration; contradictions
always fail. Never add an `ignore-chain-id` or `ignore-mismatch` switch.

Trust remains a separate gate: prefer a verified signature or an independently
trusted pinned digest. A digest fetched from the same source is integrity/drift
evidence, not independent authentication. An operator-trusted HTTPS-only policy,
if proposed, needs an explicit review/approval of that trust tradeoff and must
not be reported as signed or independently verified. Preserve required-signature
behavior. This planning task approves no changed source-trust policy.

Do not treat copied Merkle-root metadata, matching genesis, or public head
agreement as authentication of every state object. Any lossless export must
compare complete state, native root identities and the complete historical
block/receipt corpus to the trusted input. Export digests are integrity artifacts,
not new consensus commitments or proof of independent genesis validation.

## 5. Lossless full-history storage design

1. Reuse direct single-copy restoration to avoid storing an object repository
   and the restored archive together on the target. Keep the independent backup
   elsewhere. Existing source history remains intact.
2. Measure on an eligible representative archive before choosing storage
   changes. Zstd is already enabled. Evaluate compression levels, block/chunk
   layout and duplicate representations with random-read, replay and CPU/RAM
   costs; do not assume enabling compression will solve the remaining gap.
3. Change physical encoding only if lossless: preserve canonical blocks,
   transactions, headers, receipts/events and state, including all historical
   source records. Required receipts cannot be discarded because they appear
   large or potentially recomputable. Preserve existing RPC/P2P lookup behavior.
4. If a versioned lossless re-encoding/export is beneficial, build a new archive
   off-target in an isolated larger builder. Preserve the original input; budget
   the builder's extra space explicitly. Stream the complete resulting archive
   into a fresh VPS target. Do not attempt an unbudgeted in-place rewrite on the
   40 GB server or change a live publisher/legacy repository.
5. Prove coverage from the first network block through the verified head:
   continuity/parent IDs, canonical topology, record counts, and deterministic
   full-corpus logical block/receipt hashes. Compare source/destination decoded
   records, not only a sample, stored Merkle root or new manifest size. Disclose
   missing source history as an input blocker; do not call that a full archive.
6. Reopen and replay selected recorded ranges; test historical queries and peer
   requests for early, middle and recent data. Any encoding/schema change needs
   explicit format/version compatibility, migration and rollback gates. Keep
   the original source available for rollback, never delete it implicitly.
7. Bound logs and transient work, not historical retention. Monitor archive
   growth and warn/stop safely before capacity exhaustion. If the full archive
   and required operating slack cannot fit, report the limitation and measured
   extra bytes needed; do not prune or switch products to turn failure into pass.

The proposed `teleno_light` belongs to a separate future project decision with
different historical-service requirements. Its state/window/GC design and
producer research are excluded from the current full-history milestones.

## 6. Incremental milestones

LR0's local exit is recorded in the [baseline report](../validation/low-resource-lr0-baseline.md).
LR1's local changes and outstanding source gate are recorded in the
[LR1 report](../validation/low-resource-lr1-prerequisites.md).
LR2's local hardening is recorded in the
[memory report](../validation/low-resource-lr2-memory.md); its full exit is not
satisfied. LR3–LR6 are **proposed / unimplemented**. Verify each exit before proceeding;
independent diagnostic work may continue while a prerequisite remains blocked.

| Milestone | Implementation and likely files | Verification / exit |
| --- | --- | --- |
| LR0 — Resource contract and baseline | Implemented: `measure_low_resource_baseline.py`, isolated sampler/timeout tests, CTest registration and [baseline report](../validation/low-resource-lr0-baseline.md) | Local exit satisfied: five paired eligible synthetic runs, current build/test failures recorded, prior pinned metadata/host samples identified; absent Linux/source/combined peaks remain explicit blockers, not capacity estimates |
| LR1 — Eligible source and persistence prerequisites | Locally implemented: typed source diagnosis, bounded direct metadata, checkpoint-derived native backup identity, native public-promotion gate, atomic storage and durable publication with fault/reopen/process tests; [evidence](../validation/low-resource-lr1-prerequisites.md) | Local tests pass, SC0 crash fixed and cached ARM64 workflow verified. Fresh mainnet preflight identifies missing chain identity; source eligibility remains blocked pending a new independently verifiable backup and full corpus. No adapter/trust bypass, physical-power-loss or clean-from-empty proof |
| LR2 — Whole-node memory profile | Local cache/RPC hardening and offline RocksDB accounting implemented; [report](../validation/low-resource-lr2-memory.md). Whole-node profile/admission policy still open | Local normal/overload/shutdown and cache checks; full exit NOT satisfied: all-consumer byte/lifetime bounds, supported integrated lifetime/race checks and representative Linux memory/block-following qualification remain required |
| LR3 — Measured disk policy and runtime guard | Restore estimator, pinned policy/journal/resume/activation, storage metrics and ingress/catch-up backpressure | Existing default unchanged; model calibrates/overlap uncertainty covered; adequate and inadequate space cases, ENOSPC, WAL/compaction/restart fault tests; no skipped writes or lost observer hold |
| LR4 — Lossless archival storage improvements | RocksDB/block-store encoding and options, native full-archive export/manifests and tests, only where measurement justifies changes | All source historical block/receipt records and full canonical continuity preserved; complete logical corpus equivalence; random historical access, reference replay and migration/reopen tests pass; disk savings and CPU/RAM cost actually measured |
| LR5 — Exact-profile full-history qualification | Linux x86_64 native artifact, disposable Ubuntu/ext4 environment, measurement harness and `docs/validation/` reports | Complete archive passes direct restore/validation/activation, catch-up, historical coverage, restarts, interruption and sustained resource gates; repository tests pass; no unexplained mismatch or light-node fallback |
| LR6 — Operator and GUI documentation handoff | Native config/backup/observer/troubleshooting/release docs and changelog; Koinos One manual/current native docs | Document tested full-history coverage, workload horizon and archive-growth boundary; identify exact native pin/build; keep `teleno_light` separate; GUI/package/release/deployment checklist execution separately authorized |

The first implementation should be **LR0**, followed by LR1 and LR2, not a
smaller reserve constant. LR0 establishes the resource contract and measurement
baseline, not an archival-fit pass when representative inputs are blocked;
LR1 prevents unauthenticated or incoherent restore; LR2 constrains the 4 GB
machine before tuning storage. LR3 requires LR0/LR1 and LR4 requires persistence
prerequisites. LR5 requires LR0–LR3 and all tests for any adopted LR4 changes.
LR4's storage changes are conditional on measured benefit; full-history coverage
verification is mandatory whether or not a new encoding is selected. If archival
qualification fails, report the exact capacity blocker, not a compact substitute.
Do not start unrelated audit remediations by implication.

LR3's reserve calibration must add representative combined-peak measurements
in the larger Linux environment once LR1 makes the input eligible; unavailable
LR0 metrics cannot qualify a smaller reserve. Existing tooling measurements may
be reused only with their exact source/build/workload correspondence disclosed.

For each milestone record files, invariants, commands and actual results,
artifact paths, remaining risks/blockers, exit status and rollback. Preserve
legacy defaults and independent source data. Encoding rollback uses the original
compatible archival source in a fresh reviewed target; no history is retired.

## 7. Qualification and acceptance matrix

Every package must satisfy the quality-first contract in section 1 as well as
its resource gates. A smaller working set alone cannot close a milestone:
document correctness/compatibility evidence, failure atomicity, safe lifetime
and shutdown behavior, recovery results and preserved operational protections.
Any unresolved required safety or integrity check remains a release blocker,
even when the memory target is met. Local fixes do not establish whole-node
security or Linux qualification.

Before target execution, obtain an eligible pinned input, independent trust
policy, known durable-root/head and deterministic successor/reference ranges,
matching Linux feature build, isolated capacity environment and reviewed
authority for the exact new observer basedir. Missing builder space or source
evidence is a blocker, not authority to change the provider or live publisher.

- Five paired short fixture measurements with median/spread, comparable state
  and cache conditions. Restore the representative full-size input at least
  twice in the larger isolated environment; disclose counts and all exclusions.
- Use the whole-machine disk envelope from section 2, or a stricter measured
  Ubuntu-equivalent envelope. Count runtime, OS logs, evidence, local snapshots,
  open deleted files and compaction outputs. No clone/hard-link tricks to hide
  allocations. An empty 40 GB data volume is not the selected server.
- Verify same canonical IDs, full state results, roots, receipts/events and
  head/LIB behavior against archival/reference processing for recorded ranges.
  Separately prove full source/destination historical corpus equivalence and
  canonical coverage from the first network block; sampled replay alone cannot
  establish that all history was retained.
  Protect normal remove/tombstones, fresh pending roots, exact historical
  exception, one-block lookahead/final target, at most one full reexecution, and
  no partial index or canonical effects on failure. Required historical audit
  and differential fixtures/ranges remain required; run those off-target if
  their archive does not fit. No generic mismatch exception.
  Include deterministic cases at blocks 30,504,202, 32,789,377 and 32,789,378
  and the historical/reference range required by the
  [Chain v1.5.2 plan](chain-v1.5.2-fast-replay-plan.md); record actual execution,
  not just fixture creation or a previous report.
- Record faster-than-incoming-rate catch-up where a backlog exists, then stable
  verified head progress/lag versus selected read-only references. A connection,
  RPC response or genesis smoke is not synchronization proof.
- Initial 24-hour run and then a seven-day isolated observer soak, two clean
  restarts and one owned-process interruption; measure WAL rotation, compaction,
  recovery and archive growth actually occurring. Add deterministic stress
  fixtures if the natural workload does not exercise these paths. State crash
  injection level; a process kill is not physical power-loss evidence.
- Meet the provisional whole-node memory contract and calibrated reserve for
  the stated operating horizon, with no OOM, disk-full damage, sustained queue
  growth or behind-rate compaction debt. Calculate observed growth and remaining
  headroom; finite soak results do not promise a fixed future lifetime.
- Container smoke uses disposable mounts if container delivery is intended;
  native profile qualification cannot be inferred from an older published image.

Required repository workflow after implementation (LR0's historical failing
baseline is in the [baseline report](../validation/low-resource-lr0-baseline.md);
later passing local LR1/LR2 results are recorded in their validation reports):

```bash
KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/teleno_node --version
./build/teleno_node --help
```

Use clean repository-local configurations, focused component suites and separate
supported sanitizer builds. Record dependency versions/source correspondence;
never reuse a cache pointing at `teleno-extract` or another workspace. Local
SC0/build/full-suite and storage/publication results are recorded in LR1/LR2;
passing them does not waive representative Linux and historical gates.

Definition of Done requires all applicable LR exits, retained SC safety gates,
reproducible reports and docs handoff. Report full-history support as pass/fail/blocked.
If no safe whole-server fit is proven, state the exact missing measurement or
physical limitation. Do not claim support merely because the requirement changed.

## 8. Planning progress log

### LR2 V1-D / D1 host-memory checkpoint (2026-10-04)

[V1-D / D1 evidence](../validation/low-resource-lr2-host-memory.md) reproduces
unaligned result-count stores in actual thunk, host and Fizzy execution paths.
Four-byte copies at the original sites preserve accepted addresses, overlapping
arguments/results and native output/meters; no new resource policy or memory
saving is claimed. Standalone clean ASan/TSan programs fail before main with
both installed compilers, so integrated tooling remains blocked and its deeper
cause unattributed. Working UBSan supports independent native progress.
Actual current-source gates are recorded in the report. Linux whole-machine,
eligible full-history/differential inputs and all-consumer bounds remain required;
V1, D1 and LR2 are not complete. No live node, VM or publication operation.

### LR2 V1-C parsed-cache capacity checkpoint (2026-10-04)

[V1-C evidence](../validation/low-resource-lr2-vm-capacity.md) adds opt-in
attributed parsed-module cache capacity alongside the existing 32-entry limit,
strict native config and cache-only metrics. Default bytes remain unlimited.
Oversize bypass/eviction preserves caller ownership, fresh instances and
metering; it neither rejects valid execution nor changes history/durability.
Five comparable Mac samples quantify reduced retention and increased reparsing
cost at 512 KiB, not a recommended Linux policy. Caller/queue/instance overhead,
supported integrated diagnostics, historical and Linux whole-node gates remain.
The checkpoint report retains actual gates; V1 and LR2 remain open.

### LR2 R1-A completed HTTP body lifetime checkpoint (2026-10-04)

[R1-A evidence](../validation/low-resource-lr2-rpc-bodies.md) reproduces needless
idle keepalive response retention. Native request/response destruction occurs
only after service/write completion, preserving full output, unread pipeline
data, cancellation safety and admitted work. Five instrumented before/after
Mac fixtures remove 18,087,935 bytes of idle response capacity; body-capacity
high water is effectively unchanged. Fixture-process RSS includes client and
comparison buffers and cannot qualify node RSS or Linux. Aggregate service/
serialization/output admission, integrated sanitizers and representative Linux/
historical proof remain required. No new profile/default, history/durability,
LR3/LR4, live server/VM or packaging/publication change; R1/LR2 stays open.

### LR2 R1-B batch working-set checkpoint (2026-10-04)

The subsequent [R1-B batch checkpoint](../validation/low-resource-lr2-rpc-batch.md)
serializes/releases one result tree at a time into the complete HTTP body and
moves results into pre-dispatch success envelopes. The twelve-item synthetic
comparison lowers attributed peak C++ requested bytes from 21,960,048 to
14,265,952; it is not a byte cap or Linux node RSS. Exact output, call ordering
and later-call behavior after invalid UTF-8 remain covered. Complete final
bodies, service/input/transport memory, integrated diagnostics and representative
Linux/historical proof remain required. R1/LR2 stays open.

### LR2 V1-A portable VM ownership checkpoint (2026-10-03 to 2026-10-04)

[V1-A evidence](../validation/low-resource-lr2-vm-lifetime.md) reproduces parsed
module leakage when shared ownership cannot allocate, protects raw ownership,
stages optional cache publication before eviction and coalesces retained same-ID
cold loads. Warm hits are independent of the parser lock; execution and nested
callbacks occur after unlocking, with fresh mutable instances and unchanged
metering. This preserves protocol/replay/history/durability behavior.

Linux Ubuntu/x86_64/ext4, 2 vCPUs/4 decimal GB remains primary. Mac samples show
no single-thread heap/RSS reduction; controlled same-ID bursts remove duplicate
parses, not proof of whole-node performance. Distinct cold calls serialize and
can incur latency. Required parsed-byte/queue/instance policy, functioning
ASan/TSan, historical inputs and representative Linux proof remain open. An
additional pinned Fizzy context-allocation termination defect is reproduced and
requires a dependency correction; V1/LR2 are not complete. No Mac-derived VPS
defaults, live server/VM, LR3/LR4, history/durability or source-trust changes.

### LR2 V1-B portable VM allocation checkpoint (2026-10-04)

[V1-B evidence](../validation/low-resource-lr2-vm-allocation.md) records the
narrow correction to both pinned Fizzy context constructors and local native
parse/instance/context allocation-error propagation. All native CMake entrypoints
use the same hash-pinned archive and patch in a separate upstream child build;
existing installs, ABI, module ownership, depth and metering are preserved.
Scoped faults/retries and transaction dispatch are local correctness evidence,
not machine-wide OOM or representative-history validation. Parsed-byte/queue/
instance policy, integrated ASan/TSan and Linux whole-node gates remain required.
Ubuntu/x86_64/ext4 remains primary; no Mac-derived VPS policy is selected.

Initial proposal on 2026-10-03 (bounded-history direction superseded below):
inspected current single-copy code/plans, source review and Linux
resource evidence; revised the target as an explicit low-resource observer
profile with separate finite-archival and bounded-history claims. Added LR0–LR6,
preserved protocol/durability/source-trust gates and the existing default reserve.
No new restore, runtime build, historical validation, server operation or resource
improvement was performed. Planning documentation checks are recorded after
execution; milestone implementation and capacity exits remain unproven.

Files changed in this planning task:

- This new plan, `docs/implementation/single-copy-backup-restore-plan.md`,
  `docs/README.md` and `docs/memoria.md` in Teleno.
- `docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md` and
  `docs/manual/koinos-one/backup-and-restore.md` in Koinos One, documentation only.

Actual documentation verification on 2026-10-03:

- `git diff --check` in both checkouts: exit 0.
- New-plan trailing whitespace / extra EOF-blank check: exit 0, no findings.
- Relative Markdown file-link check across the four Teleno documents: exit 0,
  61 references checked, no missing files. This is not a GitHub publication check.
- `cmp AGENTS.md CLAUDE.md`: exit 0; neither guidance file changed in this task.
- Koinos One `mkdocs build --strict`: exit 0. No app package/native integration
  tests were run because no integration was implemented.
- SHA-256 comparison with the turn-start dirty/untracked worktree manifest:
  57 other pre-existing Teleno files unchanged, including `README.md`, native
  source/tests and `KOINOS_HIGH_THROUGHPUT_IMPLEMENTATION_PLAN.md`. Only the three
  listed existing Teleno documents changed and this plan was added. Koinos One
  changed only its two already-dirty documentation files through additive edits.

Planning exit: revised requirements/milestones and docs handoff prepared.
Implementation exit: **not satisfied**; LR0–LR6 and actual 40 GB support remain
unproven. No native build/test or historical/differential result is claimed for
this documentation-only change; existing baseline blockers remain recorded.

### LR2 local hardening tranche (2026-10-03)

Implemented owned asynchronous JSON-RPC sessions, complete-read/write deadlines,
drained stop and repeated lifecycle tests. Corrected object-cache oversized
handling and unique module LRU replacement; warm hits use allocation-free splice.
Retained a single oversized object rather than introducing an unsafe uncached
borrowed pointer. Added truthful shared-cache/memtable/reader accounting with
unchanged RocksDB defaults and a bounded offline genesis/RPC measurement tool.
The [LR2 report](../validation/low-resource-lr2-memory.md) contains the exact files,
commands, measurements, failed diagnostic attempts and remaining gates.

LR2's whole-node exit is **not satisfied**. P2P deduplication, global mempool,
response/result, VM parsed-byte/instance and reader-lifetime budgets require
additional work and workload evidence; no experimental low-resource profile is
selected. Mainnet source/trust/corpus and isolated Linux inputs are unavailable.
No history/receipt pruning, durability/reserve reduction, VPS/live-node change,
producer activation, commit/push/release or GUI/package integration occurred.

### LR2 accepted-ID P2P tranche (2026-10-03)

The subsequent "Procede" authorizes bounded disposable accepted-ID caches,
not other audit packages or LR3. Added single-owned-key FIFO indexes, entry/
string-capacity/expiry limits, LIB/stop cleanup, mempool exact-ID probes, metrics
and deterministic cache/fake-transport sustained tests. Blocks outside the
cache retain normal controller validation; failed unknown data is not cached
or silently accepted. No persistent history/index/schema/trust/reserve changes.

Files, exact commands, measured cache RSS/CPU tradeoffs and actual failures
are recorded in the [P2P report](../validation/low-resource-lr2-p2p-retention.md).
Repository tests pass locally; ASan/TSan and mainnet/Linux/source gates remain
explicit blockers. Scoped local package exit is satisfied; whole-node LR2 and
full-history 4-GB/40-GB qualification are **not satisfied**. The next independent
local LR2 package should address global mempool/admission accounting and bounds
with measured compatibility gates, not begin disk-reserve reduction.

### LR2 persistent-goal worker checkpoint (2026-10-03)

Subsequent [state ownership checkpoint](../validation/low-resource-lr2-state-ownership.md)
confirms read-caused eviction freed a borrowed buffer before consumption. Native
reads now retain immutable ownership through direct and iterator results; map
clones share immutable payloads with independent maps/replacement-on-write.
Oversized values can safely bypass object-cache retention, making retained
payload capacity strict while leaving caller-held bytes and metadata outside it.
Small-value clone overhead is explicitly measured; Linux profile decisions and
supported race/lifetime/historical qualification remain open. The required
external dependency workflow now passes; current regression evidence follows
in the checkpoint report. This does not close S1 or LR2.

Subsequent [native admission checkpoint](../validation/low-resource-lr2-admission.md):
ordinary mempool insertion refusal now precedes accepted-work notification;
post-admission notification failures are observable without retry insertion.
Visible-head pending diagnostics count without payload materialization instead
of incorrectly reporting zero. Current local full suite passes 38/38 and UBSan
subset 3/3. Global/fork byte and transient admission limits are not implemented;
Linux whole-node/historical gates remain required, with Linux VPS optimization
explicitly prioritized over Mac diagnostics. Neither M1 nor LR2 is complete.

Created the active completion matrix before further runtime implementation.
Reproduced stop returning while a disconnected worker remained inside a gated
RPC. Own all worker generations through join, bound connected/owned counts,
cancel between operations and drain before transport teardown. Functional
fake-transport tests cover churn, overlap, capacity, stop and startup failure;
supported lifetime/race and real transport cancellation still require proof.
See [worker evidence](../validation/low-resource-lr2-workers.md). No archive,
consensus, durability, reserve, source-trust, VPS or producer change.

New generated data is on the explicitly selected My Passport directory, with
external test-root selection preserving fresh-child and path-confined fixtures.
This does not qualify Linux/ext4. Mempool/VM/RPC/state/transport work and actual
representative memory/historical gates remain open; LR2 exit is **not satisfied**.

### LR2 M1-C admission atomicity (2026-10-03)

Subsequent [LR2 M1-C](../validation/low-resource-lr2-admission-atomicity.md)
reproduces and fixes allocation-failure partial mempool insertion across mutable
forks, using preallocated map-node transfer and preprepared overlay resets.
Original anonymous root/tombstone and per-fork nonce/resource semantics remain.
The zero-allocation publication test and one-shot failure/readmission sweep are
local correctness evidence, not aggregate capacity or Linux qualification.
Measured C++ allocation requests decrease in a synthetic candidate; physical
retention/whole-node improvement remains unproven. Block/LIB/prune/removal OOM,
global reservations/refusal classification, consumer budgets, functioning
sanitizers and representative historical/Linux gates remain required. No LR3,
LR4, data pruning, VPS or external operation follows; LR2 stays incomplete.

### LR2 M1-D indexed-fork accounting (2026-10-03)

The [retained-fork checkpoint](../validation/low-resource-lr2-retained-memory.md)
adds on-demand exclusive-lock inspection of indexed map owners/keys/tombstones
and unmeasured backend counts. Five synthetic cases demonstrate that empty-head
expiry retains older payloads until LIB; scan failures leave state unchanged.
This is characterization, not an admission ceiling or new memory improvement.
No runtime limit is selected from Mac string capacities/timing, and no protocol,
history, durability, source policy or server configuration changes. Ubuntu/
x86_64/ext4 remains the primary optimization target. Incremental global/transient
accounting, reservation/refusal, other consumer policies, supported sanitizers
and representative Linux/historical evidence remain required. M1/LR2 stays open.

### LR2 M1-E pre-execution transaction work (2026-10-03)

The [transaction-work checkpoint](../validation/low-resource-lr2-transaction-work.md)
adds opt-in global standalone-submission count/input-wire reservations before
controller copying/state/execution, with cleanup through native insertion and
notifications. Local overload is retryable, not consensus invalidity or peer
misconduct. Canonical block processing/replay remains outside this gate.
Defaults are unchanged; no Mac-derived VPS policy is selected. This is one
transient-work boundary, not a heap, retained-fork, ingress/output or whole-node
memory ceiling. Its local tests and commands are recorded separately; aggregate
capacity, other consumers, integrated sanitizers and actual Ubuntu/x86_64/ext4
whole-node/historical gates remain open. M1 and LR2 stay incomplete.

### LR2 M1-F fork-head cleanup atomicity (2026-10-03)

The [cleanup checkpoint](../validation/low-resource-lr2-cleanup-atomicity.md)
reproduces partial expiry/removal on allocation failure and incomplete stream
decoding followed by a null-resource read. Cleanup uses checked direct decoding,
stages all fork-head changes and prepares bucket/reset storage before an
allocation-free map publication. Ordinary removal, squash/durable commit,
historical replay, retained ancestors and protocol/resource rules are unchanged.
Prepared buckets/extra overlay iterators have explicit cost; this is a portable
correctness prerequisite for capacity handling, not a Mac-derived VPS limit.
Block/LIB failures, aggregate capacity and all remaining Linux/historical/
supported sanitizer/whole-node gates stay open. M1/LR2 is not complete.

### LR2 M1-G lifetime-owned pending records (2026-10-03)

The [pending-record checkpoint](../validation/low-resource-lr2-pending-records.md)
removes duplicate serialized pending payloads across eligible forks and preserves
shared immutable ownership through clones/squash/in-memory LIB. Optional
owner/capacity reservations follow staged, ancestor and caller lifetimes;
rejected preparation unwinds credits without parent publication. Head expiry
does not prematurely release retained ancestors. Local overload retains the
retryable RPC/P2P contract, with canonical validation/replay unaffected.

Defaults stay unlimited. Other metadata, encoding/cleanup workspace and consumer
policies remain required; Mac fixture sizes/timing select no Linux VPS defaults.
Current commands, failures, measurements and source preservation are recorded
in the checkpoint report. Supported integrated sanitizers, historical inputs
and representative Ubuntu/x86_64/ext4 whole-node qualification remain open.
This is independent M1 progress, not full M1/LR2 or 4-GB/40-GB qualification.

### Full-history requirement correction (2026-10-03)

The user requires complete history in Teleno for now and treats the limited-
history idea as a different future node, provisionally `teleno_light`. Removed
the rolling-window/compact-node success path from the active requirements and
redefined the unimplemented LR4 as lossless archival storage work. Full-history
corpus/continuity verification is mandatory; no light fallback can satisfy LR5.
Separated the idea into `teleno-light-concept.md`, with no new repo, binary or
implementation authorized. Updated parent plan, project guidance/memory/index
and Koinos One docs only. No runtime, basedir, target or resource policy changed.

Actual checks for this correction: `git diff --check` in both repositories,
`cmp AGENTS.md CLAUDE.md`, and Koinos One `mkdocs build --strict` all exited 0.
A local link/whitespace check over the eight Teleno documents checked 84 relative
file references with no missing files or whitespace findings. SHA-256 comparison
against this turn's initial worktree preserves 54 other pre-existing Teleno
files, including native source/tests, `README.md` and the high-throughput plan.
The seven existing documentation/guidance files listed by this correction were
edited; `teleno-light-concept.md` was added. Koinos One changed only its two
already-dirty backup documentation files. No native tests/build or actual
full-history resource qualification was run for this documentation-only correction.

### LR0 implementation (2026-10-03)

Implemented the first milestone's versioned full-history resource contract,
repository-local measurement driver, owned-child resource sampling and bounded
command cleanup. Added tests and CTest registration; reused SC5's signed
genesis/padding fixture, exact-byte checks, activation inode checks and observer
holds rather than creating another restore implementation. Missing live-DB,
Linux and representative-history measurements stay unavailable, never zero.

The [LR0 report](../validation/low-resource-lr0-baseline.md) records all files,
commands, versions, five paired measurements and retained artifacts. The standard
dependency workflow completed using local cached dependencies, native build and
CLI checks passed, and full CTest recorded 30/31 with the pre-existing SC0
truncated-transfer `SIGBUS` still failing. This is an explained failing baseline,
not a fix or a waived LR1 gate. Local LR0 exit is satisfied; source eligibility,
historical corpus/differential parity, Linux resource peaks and actual 40-GB
operation remain blocked. No VPS, live node, producer setting, data retention,
runtime default, trust policy, reserve, release or GUI/package pin changed.
