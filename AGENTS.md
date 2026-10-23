# Teleno Agent Project Guide

## Concise communication

- Keep replies and progress updates very concise by default.
- Lead with the result. Use short, clear sentences and short bullet points.
- Give one idea per bullet. Avoid repetition, long introductions, and unnecessary detail.
- Preserve essential risks, uncertainty, evidence, and confirmation requests.
- Expand only when asked or when necessary for safety or correctness. Do not shorten verbatim text or required document content.


Last updated: 2026-10-23

This file contains operational guidance for agents working in the standalone
Teleno repository. Detailed architecture, operator procedures, validation
reports, and implementation plans belong in `docs/`, not in this file.

Keep `AGENTS.md` and `CLAUDE.md` byte-for-byte identical.

## Active Project Boundary

- Active repository: `/Users/pgarcgo/code/teleno`
- Active remote: `https://github.com/koinos/teleno.git`
- Primary branch: `main`
- LR2 memory, ownership and lifecycle work belongs on
  `codex/lr2-memory-hardening`, as explicitly requested on 2026-10-06.
  Keep it separate from `main` until an explicit integration request. Existing
  mixed, uncommitted work is preserved; review its scope before committing.
- Product and binary: Teleno / `teleno_node`
- This repository owns the monolithic native node: C++ source, CMake projects,
  tests, tools, node configuration, native dependency build scripts, the
  Docker image and CI, CLI operator documentation, native backup and restore,
  and independent native releases.
- Koinos One is the separate desktop application at
  `https://github.com/koinos/koinos-one` and the GUI frontend for Teleno.
  It consumes this repository as the
  `node/teleno-node` git submodule and owns the Electron/React GUI, desktop
  orchestration, app packaging, first-run UX, the MkDocs app manual, and the
  Koinos One product version.
- Make native node changes here. Koinos One GUI implementation, app packaging,
  and first-run implementation changes require explicitly scoped
  cross-repository work. Relevant Koinos One documentation updates are part of
  Teleno maintenance, as described in the documentation handoff below.
- Knodel and the legacy Koinos microservice stack are separate. Use them as
  compatibility references when relevant; do not edit or operate them as a
  side effect of Teleno work.

## Project Mission

Teleno is a community-driven monolithic Koinos node implemented as one native
binary. It must remain compatible with the Koinos protocol and mainnet
behavior. The microservices-based Koinos stack remains the reference
implementation, so optimizations must not create protocol divergence.

Current resource-adaptation work must preserve complete canonical history from
genesis and all block/receipt records present in the selected backup. Do not
meet a disk budget by pruning history, omitting receipts, or switching to a
bounded-history node. `teleno_light` is a separate deferred concept, not a
fallback, current native product, or authorized implementation. See
`docs/implementation/low-resource-mainnet-observer-plan.md` for the active
full-history requirements and `docs/implementation/teleno-light-concept.md`
for the separate idea.

The node must support safe observer operation first. Block production, backup
administration, restore activation, and other higher-risk behavior must remain
explicit and carefully gated.

All project documentation and committed user-facing text must be written in
English, even when discussion with the user happens in another language.

## Repository Map

- Native runtime source: `src/`
- C++ tests: `tests/`
- Native tools and migration/audit utilities: `tools/`
- CMake entrypoints: `CMakeLists.txt`, `CMakeLists.hunter.txt`, and
  `CMakeLists.standalone.txt`
- Dependency and native build scripts: `scripts/`
- Runtime configuration templates and genesis data: `config/`
- Container assets: `Dockerfile`, `docker/`, and `.dockerignore`
- CI and release workflows: `.github/workflows/`
- Version source of truth: `VERSION`
- Native release notes: `CHANGELOG.md`
- Operator documentation entrypoint: `docs/README.md`

## Documentation Map

- Install and build: `docs/install-or-build.md`
- Quickstart: `docs/quickstart.md`
- Configuration: `docs/configuration.md`
- Observer operation: `docs/running-observer-node.md`
- Producer operation and activation gate: `docs/running-producer-node.md`
- Backup and restore: `docs/backup-restore-cli.md`
- RPC and admin endpoint exposure: `docs/rpc-endpoints.md`
- Logs and diagnostics: `docs/logs-and-diagnostics.md`
- Command reference: `docs/command-reference.md`
- Recovery-first troubleshooting: `docs/troubleshooting.md`
- Container operation: `docs/operations/container.md`
- Native release identity and checklist: `docs/release-builds.md`

Read the relevant documentation before making broad architecture, storage,
backup, restore, producer, RPC exposure, container, or release decisions. Keep
the affected documentation and examples current when behavior changes.

Some deeper architecture and parity references still live in `koinos-one` and
are linked from `docs/README.md`. Treat those as cross-repository reference
material. Follow the documentation handoff below; reference links do not
authorize changes to Koinos One application code or packaging.

## Open TODOs

- [ ] Address the bugs and memory/disk-efficiency findings in the
  [software review](docs/performance/teleno-software-audit.md), following the
  [remediation plan](docs/implementation/teleno-audit-remediation-plan.md).
  Prioritize WP1 storage integrity, WP2 RPC lifecycle, and WP3 cache correctness
  and memory efficiency. LR1 implements a scoped subset of WP1/WP6 and cache
  clear accounting. LR2 adds scoped cache invariants/hit allocation fixes, owned
  RPC sessions, DB memory accounting and bounded disposable P2P accepted-ID
  retention; whole-node bounds and integrated
  lifetime/race qualification remain open. The audit is not fully remediated.
  LR2 also corrects native admission refusal/metrics and state read ownership;
  immutable clone payloads and bounded retained cache payload do not bound
  caller-held/fork memory or prove Linux/ASan/TSan qualification.
  M1-C adds locally verified failure-atomic write-only multi-fork admission;
  aggregate capacity and block/LIB allocator safety remain open.
  M1-D inventories indexed map owners/keys/tombstones across retained forks on
  demand; string capacities are not heap/RSS totals or admission ceilings.
  M1-E adds opt-in standalone transaction-work count/input-wire reservations
  and local-overload classification; retained-fork, ingress/output and whole-node
  memory bounds and Linux profile selection remain open.
  M1-F adds scoped failure-atomic fork-head expiry/removal and checked cleanup
  decoding; its map-only publication is not block/LIB, persistent-commit or
  whole-machine OOM qualification. Aggregate capacity remains open.
  M1-G shares immutable pending records across forks and adds opt-in live-owner/
  capacity reservations; head expiry does not free ancestor-held credits. This
  excludes keys/indexes/RC/nonces/tombstones, encoding workspace and other
  consumers; it is not a complete aggregate memory or Linux qualification.
  V1-A protects parsed-module ownership through allocation failure and makes
  cache publication optional/failure-atomic, with coalesced cold parsing. Parsed
  byte/instance/queue policy and Linux tradeoffs remain open. V1-B corrects the
  pinned Fizzy context-allocation termination through a shared hash-pinned build
  recipe; native allocation failures remain local, not consensus reversions.
  Scoped fault cleanup does not prove machine-wide OOM, integrated races or LR2.
  V1-C adds optional attributed parsed-module cache capacity and native metrics;
  default bytes remain unlimited/count 32. Oversize bypass preserves execution.
  Allocator, caller-held, active-instance/queue and Linux memory gates remain.
  V1-D corrects reproduced unaligned host result-count stores with byte copies
  at the original sites; alias ordering and historical behavior must remain.
  D1 historical Mac standalone programs reproduce ASan/TSan startup failure
  before main. Linux clean/known-fault controls now pass; TSan requires child-
  only ASLR-off. P2-A helper/codec and P2-B scoped real-transport sanitizer
  checks pass; prebuilt SDK libraries remain uninstrumented. Integrated
  state/mempool/VM/RPC and full lifecycle/concurrency qualification remain open.
  R1-A releases HTTP bodies after service/write completion and attributes their
  capacities; service/JSON/serialization/output admission and Linux gates remain.
  R1-B serializes/releases batch results individually into a complete HTTP body,
  preserving strict UTF-8 failure dispatch and avoiding result-tree copies.
  The final body, input/services and aggregate admission remain unbounded axes.
  P2-A completes real Peer RPC short writes without changing frame bytes and
  retains each write owner through completion. Aggregate transport capacity,
  full cancellation/shutdown and Linux qualification remain open. P2-B adds
  owned fragmented reads, constant-stack MessagePack skipping and scoped real
  TCP/Noise/Yamux stop/restart checks; native Mac/Linux regressions pass. This
  does not bound aggregate queues or qualify lifecycle concurrency/whole-node use.
  P2-C owns outgoing RPC phases and complete-response deadlines, cancels
  retired client calls and keeps late borrowed buffers alive. Local receive
  capacity narrows later block batches without peer scoring. Small decoded
  results shed large ignored capacity. Final Mac/Linux native checks, separate
  scoped ASan/UBSan/TSan, VPS components and runtime-container CLI/hash pass.
  Aggregate SDK/decoded/caller memory, DNS/server lifecycle and whole-node gates
  remain open.
  P2-D bounds error-record count/attributed key capacity, groups scores by network
  address and corrects idle decay/overflow/query rounding. Optional candidates
  trim with discovery off; seeds/connected metadata stay protected. Allocation
  refusal is local without peer blame. Mac/Linux native regressions and paired
  component measurements, separate scoped ASan/UBSan/TSan, VPS components and
  isolated runtime-container CLI/hash checks pass.
  Protected candidate counts can exceed the configured limit; candidate/SDK/
  caller bytes and whole-node gates remain open.
  P2-E adds native explicit dial lifetime count/peer-capacity reservations,
  failure-atomic setup and seed/stop publication; supported seed aliases share
  admission. Controlled TSan fixture lifetime correction preserves detection.
  Mac/Linux 57/57 native checks, nine fixtures per separate sanitizer, 12 VPS
  component commands and runtime-container CLI/hash pass. Implicit GossipSub/
  RPC/DHT/inbound, SDK/native known peers, control/caller aggregates, DNS/full
  cancellation and whole-node qualification remain open.
  P2-F bounds optional advice and lifetime-owned native metadata projections,
  removes external SDK reads and corrects a reproduced query/stop host race.
  A hash-pinned TSan-only temporary DI mutex lifetime overlay retains real-cycle/
  destroy-held detection. Final Mac/Linux 59/59 native, eleven fixtures per
  separate sanitizer, 14 VPS commands and runtime CLI/hash pass. Active/SDK
  metadata, implicit work, other queues/caller copies and whole-node gates remain.
  M1-H adds an opt-in shared mempool/transaction-work reservation domain, owned
  map/index/tombstone, policy-state and native decoded-vector accounting,
  block/LIB failure preparation and prepared persistent index publication.
  Instrumented canonical/recovery allocations are counted but quota-exempt;
  supplementary workspace and exclusions are not physical heap/RSS bounds.
  Other nonce/RC/count read workspace, escaped RPC/execution output ownership,
  representative profile selection remain open. The final C2 normal 66/66
  and scoped TSan 12/12 pass, but ASan/UBSan 11/12 fails an inherited
  1,008-byte decoded-query fault leak. The finite two-candidate attempt is
  unqualified; do not reset its limits or mark M1 complete.
  The separate 2026-10-19 C3/C4 repair allowance is closed unqualified within
  its original 22:16:04 UTC deadline. C3 failed 44/68 normal checks from a
  RocksDB/global-new pointer incompatibility. Frozen C4 passes 68/68 normal
  and 16/16 scoped ASan/UBSan checks; default TSan reports a per-fork lock-order
  warning (14/15), while the separately labeled race-only run passes 16/16.
  Five paired samples per synthetic workload show higher C4 RSS and a slower
  ordinary admission path; representative Linux policy and whole-node evidence
  remain missing. Preserve all failed results and candidate counters. See
  docs/implementation/issue-9-m1-aggregate-goal.md and
  docs/validation/low-resource-lr2-m1-repair.md. The requested legacy
  applicability document records hypotheses, not reproduced legacy defects.
  See docs/validation/low-resource-lr2-aggregate-memory.md before resuming.
  The operator authorized necessary further finite M1 attempts on 2026-10-20;
  each keeps its own fixed deadline, two-candidate ceiling and evidence. The
  C5/C6 attempt began at 00:35:55 UTC with an eight-hour deadline and closed
  at 03:50:42 UTC with both frozen slots consumed. C5 was unqualified; C6
  passes 68/68 normal, 15/15 focused ASan/UBSan plus cleanup, 15/15 default
  TSan plus cleanup, and two controlled 100-block/12,800-transaction profiles.
  C6's scoped success does not establish complete M1 ownership/admission or
  reference-VPS physical margins; M1 remains open. Preserve its evidence under
  `/Volumes/My Passport/teleno-m1-followup-a5711dmt/`. No live VPS producer
  work was part of this attempt. Keep this TODO open
  until the required fixes and verification gates are completed and documented.
  A separate C7/C8 source-gap run began at 03:52:11 UTC on 2026-10-20 with a
  fixed eight-hour deadline, two frozen candidate slots and owned data under
  `/Volumes/My Passport/teleno-m1-output-gap-20261020-rF9mPYUo/`. It starts
  from the verified C6 archive and must not reset C5/C6 evidence or touch the
  live VPS producer. The C7/C8 run closed unqualified at 07:58:20 UTC, before
  its original deadline and with both candidate slots consumed. Frozen C8
  passes 73/73 normal, 21/21 scoped ASan/UBSan and 21/21 scoped TSan plus
  both cleanup checks and a controlled 100-block/12,800-transaction reopen.
  Five paired C6/C8 builder samples show a material but unexplained C8
  CPU/latency slowdown with nearly unchanged RSS. Preserve every result;
  do not claim an attributable regression, savings or M1 completion from this
  fixture. A separate C9/C10 attempt starts at 08:00:37 UTC with a fixed
  16:00:37 UTC deadline and preserves C7/C8 results. C8 reproduces two Linux
  ASan shorter-predecessor nonce/count read overflows; frozen C9 corrects them
  and passes 74/74 normal, 22/22 focused ASan/UBSan and 22/22 focused TSan
  plus both cleanup checks and a controlled 100-block/12,800-transaction
  production/reopen profile. Five paired C8/C9 controlled samples and all
  ten reopens pass; median CPU and RSS are nearly unchanged with wide latency
  variation, so no attributable saving or slowdown is claimed. The finite
  attempt closed at 11:11:04 UTC before its original deadline with C10 unused;
  M1 remains open. The inspected legacy source has the same read pattern, but no
  legacy binary has been tested. Complete M1 ownership and reference-VPS
  whole-node margins remain unproved. Further attempts remain separately
  finite and must not touch the live VPS producer.
  A separate frozen-C9 qualification on the operator's additional isolated VPS
  began at 15:19:53.510265 UTC on 2026-10-20, with a fixed 23:19:53.510265 UTC
  deadline, zero new candidates/builds and at most two observer starts. Startup
  and selected-input metadata preflight pass; guarded native single-copy
  staging began at 15:56:41 UTC. Runtime qualification and M1 remain open.
  Evidence is under `/Volumes/My Passport/teleno-m1-ovh-qualification-20261020-sgjo6v1a/`.
  See docs/implementation/issue-9-m1-isolated-vps-qualification.md. The additional
  host's larger physical disk does not enlarge the strict 40-decimal-GB budget.
  Do not touch the operational producer or reset any prior attempt's counters.
  C9 native staging completes at 16:22:44 UTC, with all 383 selected files /
  24,114,178,822 bytes and a 2,382,008,320-byte minimum sampled strict disk
  margin including reserve; offline validation starts at 16:22:46 UTC.
  Separate Linux diagnostics reproduce raw-request overcount and chain-receipt
  HTTP bodies retaining about 4.5 MB after M1 attribution returns to 240 bytes.
  A distinct C11/C12 repair begins at 16:26:31.428768 UTC, with the fixed
  2026-10-21 00:26:31.428768 UTC deadline and at most two frozen candidates.
  At that development snapshot no candidate was frozen. Its metric correction passes a Linux
  regression that fails against C9; receipt-output repair and full qualification
  remain pending. Evidence: `/Volumes/My Passport/teleno-m1-receipt-output-repair-20261020-zxyqrmy1/`.
  See docs/implementation/issue-9-m1-receipt-output-repair.md. The old unused
  C10 remains unused, and the running C9 test/operational producer are unchanged.
  C9 offline validation/held activation passes at 16:46:18 UTC, checking
  46,458,363 records with a 2,380,722,176-byte minimum sampled strict disk
  margin including reserve. The first observer starts at 16:46:22 UTC; catch-up,
  observation and reopen remain pending. C11/C12 development adds pre-admission
  native receipt preparation and lifetime-owned HTTP output. Its Linux HTTP
  component and unchanged-C9 gRPC diagnostic compile without executing tests on
  the builder. Initial relocated-include harness failures are preserved.
  A later post-acceptance allocation-failure regression is not in that component
  snapshot. Those pending descriptions are historical. C11 later froze at
  22:09:33.956325 UTC, source archive
  `95b2914962e9127b18bbc88b4fe535cd69dcc96c3465d46cbca7554204e36bd3`,
  and passes 6/8 OVH component invocations. Concurrent HTTP output times out;
  post-acceptance allocation failure aborts in noexcept JSON cleanup. C11/C12
  expired unqualified at its original 2026-10-21 00:26:31.428768 UTC deadline,
  with C12 unused and no whole-node C11 build or complete qualification.
  The separate C9 observer sequence stopped at 18:32:58 UTC on 2026-10-20
  under its additional 1-GiB early-stop disk guard; the final strict margin
  including the full reserve remained positive at 998,899,712 bytes. The
  full observation and reopen did not complete. No test observer remains
  running at terminal reconciliation. Preserve all failed evidence/counters.
  On 2026-10-21 the operator clarified that the objective is actual application
  memory optimization: reduce unnecessary allocations, copies, workspace and
  retention, then qualify lifetime-owned aggregate admission and physical RSS.
  See docs/implementation/issue-9-m1-aggregate-goal.md. This wording starts no
  new run or deadline and does not make M1 complete. Tests remain OVH-only;
  the operational Hetzner producer remains outside scope.
  The no-overall-deadline M1 JSON-cleanup continuation starts at
  2026-10-21 03:33:59.573386 UTC, with evidence under
  `/Volumes/My Passport/teleno-m1-json-cleanup-20261021-lygqon1e/` and no frozen
  C13/C14 candidate. Allocation-free owned JSON cleanup passes 14/14 scoped
  normal OVH development invocations, including the formerly aborting
  post-acceptance fault and concurrent output. Prebuilt C9 linked libraries
  remain explicit scope limits; this is not a source-consistent whole node.
  Static sanitizer binding/link failures are preserved; corrective dynamic
  ASan/UBSan passes 15/15 scoped invocations on OVH. TSan has 13 uninterrupted
  passes and one interrupted cold-reflection capacity test; its wrapper-zero
  result is superseded by an explicit failure reconciliation. A standalone
  control reproduces TSan's once-exception retry limitation. A normal regression
  separately proves lazy global Protobuf metadata entering request admission;
  the proposed correction and concurrent/copy-accounting tests are compiling.
  Thirty paired cleanup processes confirm zero helper allocation requests but
  expose a 17.4x median deep-branch cleanup CPU cost. A leaf-sibling correction
  is in development; preserve the slower result and all build/control failures.
  The later R3 revision passes 16/16 normal, 17/17 ASan/UBSan and 16/16
  default TSan component checks. Thirty new paired processes remove the
  deep-comb CPU penalty, with zero helper cleanup allocations and a scoped
  348,160-byte lower wide-fixture peak-RSS median. A guarded parse sweep passes
  510 quota settings, but unguarded response serialization still terminates
  at full quota. Its guarded control recovers; R4 protects temporary dump
  sites and additional local trees. R4 passes 16/16 normal, 17/17 ASan/UBSan
  and 16/16 default TSan component checks. A separate partial-copy sweep
  still aborts inside the stock dependency at quota 10,793 despite an outer
  guard. The build-owned default-destructor backport passes four copy/dump
  controls, nine normal/diagnostics/sanitizer invocations and seven staging
  recipe tests on OVH. SDK/native integration is compiling and unqualified.
  An HTTP fixture measures 1,019,389 additional attributed bytes at service
  entry for a 262,144-byte argument; earlier encoded-parameter release is
  unqualified. Preserve the private collector cleanup-scope incident and its
  unresolved prior-file uncertainty. No C13/C14 slot is frozen; whole-node
  gates remain.
  Integrated R5 passes 18/19 normal checks; a diagnostic identifies the gRPC
  failure as the large-status client's metadata limit. The corrected fixture
  preserves that default-limit refusal separately. R6 passes 14/14 normal RPC
  checks, including resource-failure base58 normalization (14 injected faults,
  zero wrong native inputs/classification errors, exact final credits).
  The unchanged baseline has four wrong inputs and two misclassifications.
  Encoded input release passes with a 281,499-byte entry delta versus 1,019,389
  before. R6 ASan/UBSan passes 17/17; default TSan passes 15/16, failing in
  prebuilt gRPC metadata/read synchronization. A pure-SDK control reproduces
  that signal without Teleno; a pinned instrumented SDK is being built.
  Five paired input/receipt samples have a 462,848-byte higher median RSS
  despite lower live attribution. R7 avoids the initial tree/string copies;
  it is not yet qualified. Preserve the negative physical-memory result.
  Source review identifies gRPC output-state/interceptor/registry control
  allocations outside capture; assess this ownership before M1 completion.
  The subsequent R7 revision passes 13/13 normal, 16/16 scoped ASan/UBSan
  and 15/15 default TSan HTTP checks. Five new input/receipt pairs show a
  548,864-byte lower median peak RSS (lower in every pair), with equal CPU/wall
  medians; this is scoped process evidence, not a whole-node saving. Preserve
  the negative R6 result. The unchanged pinned gRPC source rebuilt with TSan
  passes clean, intended callback-race and unchanged native controls (3/3).
  R8 attributes startup/per-call gRPC control owners and passes 2/2 normal,
  4/4 default TSan and 5/5 ABI-compatible scoped ASan/UBSan invocations.
  The first R8 ASan failure exposes a public-layout mismatch between ASan-mode
  gRPC headers and the normal prebuilt library. The supported
  GRPC_ASAN_SUPPRESSED compatibility define restores that ABI while compiler
  instrumentation remains active. Its SDK mutex/CV leak-checker exclusion is
  explicit. Earlier R6 gRPC ASan success is not valid gRPC qualification;
  HTTP-only checks remain distinct. Expanded concurrent cancellation/stop and
  actual gRPC callback clean/fault controls pass 3/3 normal, 9/9 scoped
  ASan/UBSan and 5/5 default TSan invocations. C13 source freezes at
  2026-10-21T09:51:26.273826+00:00; C14 is unused. Full native build and
  OVH qualification are next; no operational-producer action occurs. M1 remains open. See
  docs/validation/low-resource-lr2-m1-json-cleanup.md.
  C13's complete normal build and version/help pass, but it remains unqualified.
  The normal suite reaches 41/90 functional passes before generated test data
  and co-staged binaries breach the strict disk budget by 11,403,264 bytes.
  Exact external preservation permits retirement of owned duplicates and
  restores headroom without changing the full-history database. The focused
  ASan/UBSan build completes (21 ELF outputs), with no C13 sanitizer execution
  yet. Frozen-class status controls fail refusal ownership, encoded workspace,
  copy-failure handling and pre-boundary exception-string materialization.
  The first unfrozen repair passes three checks and still fails argument copy;
  its revision borrows error text until admission and passes 7 normal, 13 scoped
  ASan/UBSan and 9 scoped default-TSan checks with working controls. Its first
  ASan launch cannot locate the runtime; the separate verified-runtime phase
  passes with the same binaries. TSan's other native/prebuilt archives remain
  explicit development scope exclusions. C14 remains unfrozen. Preserve C13
  and all negative results; do not convert scoped or
  partial passes into final M1/whole-node qualification.
  C14 then freezes at 2026-10-21 12:14:23.846369 UTC, source archive
  8400d631109f458c61c82ad12ae24aab2ce35f670c160450d595b0e2bb1b8869,
  with 471 files. Its full normal build is running on the build-only miner;
  4/94 registered source-recipe tests pass on OVH. All C13 entries are retained.
  Per-case staging and verified external artifact retirement address C13's
  accumulating disk use. Both frozen slots are consumed; never reset them.
  Complete final native/sanitizer, profile and whole-node gates remain open.
  C14's full normal build subsequently succeeds with 71 externally verified
  ELF outputs and 167 linked-library identities. Exact normal node SHA-256:
  71bc0b0773cbd108d4783b39eccf9d16289b9f1f5a7cf4e077d10fdf168d568d.
  The interrupted caller leaves final metadata incomplete after stage exit
  zero; successful-unit and frozen-source evidence reconcile it without a new
  build. A 21-case normal core phase runs on OVH with 19 ELF files matching the
  final build. The final-source ASan/UBSan build runs separately on the miner.
  Complete normal/CLI, sanitizer, paired/profile and whole-node gates remain.
  C14 subsequently passes 49/94 registered normal checks and both actual
  version/help checks before SC5 crosses the strict disk budget by 26,140,672 B.
  All 647 generated entries are externally preserved and verified before exact
  owned-copy retirement; the full-history database remains unchanged. A separate
  continuation covers 44 previously unexecuted cases without erasing SC5.
  The 22 final-source ASan/UBSan outputs are externally verified; sanitizer
  execution, TSan compilation and complete whole-node qualification remain open.
  The separate C14 continuation completes all 44 scheduled tests. Exact JUnit
  reconciliation now proves 93/94 normal passes and both CLI checks; SC5 remains
  failed. ASan/UBSan and TSan each have 22 externally verified ELF files. Final
  ASan/UBSan execution has started with working controls; native matrices and
  whole-node gates remain pending. Unfrozen SC5 fixture-retention revision 2
  passes 15 helper unit checks; revision 3 and the full experiment are pending.
  This does not change C14's frozen source or consume/reset another slot.

The later C14 scope reconciles 54 unique ASan/UBSan successes across 55
invocations, retaining its original cleanup timeout and a separate diagnostic
pass. Default TSan is running. SC5 revision 3 passes 16 helper units, not the
full experiment. C14 also reproduces a large-ID HTTP refusal ownership gap:
524,420 requested bytes remain untracked after the actual quota exception.
An unfrozen bounded workspace/borrowed-ID correction is being compiled; no
component or whole-node qualification is claimed for it. Both C13/C14 slots
remain consumed, the producer untouched, and M1 open.

  The separate R10 build-and-observer qualification on isolated OVH completes:
  exact Zstd-linked Linux node, 101/101 normal tests, both CLI checks, 61/61
  focused ASan/UBSan, 59/59 focused TSan with instrumented gRPC, one-hour
  mixed-read mainnet observer and clean same-database reopen. Both observer
  starts stopped cleanly; the operational Hetzner producer was untouched.
  Preserve the original R9 no-Zstd start failure, reference-only mixed-window
  failure and initial R10 TSan timeout/uninstrumented-gRPC signal. Final
  reconciliation is under the owned My Passport run. Focused sanitizers and
  this observer do not complete M1 ownership/admission, representative policy,
  paired physical-memory savings or issue 9's whole-node gates; M1 remains open.

  Frozen R13 subsequently passes 101/101 normal OVH checks, both CLI checks,
  61/61 focused ASan/UBSan and 59/59 default TSan cases. Its isolated
  100-block/12,800-locally-signed-transaction workload and same-state reopen
  pass. The exact full-history observer passes one-hour mixed reads, clean
  reopen and separately labeled owned-process interruption/recovery with
  independent canonical agreement and resource guards; all four observer
  starts are stopped and the Hetzner producer remains untouched. Five
  R12/R13 block-event pairs remove redundant C++ allocations but do not
  establish an attributable RSS saving. Final-source review finds that
  protected gRPC refusal controls are accounted but their aggregate
  concurrency is not capped by the ordinary M1 quota. Complete final-owner
  inventory, representative policy and physical-memory attribution remain
  open. Preserve exact R13 identities and evidence in the M1 validation
  report; do not mark M1 complete or reset prior attempt records.

  The 2026-10-22 23:16 UTC refusal follow-up reproduces linear protected
  status/control retention in R13, with exact final release. An unfrozen
  separate refusal allowance passes six small normal OVH checks and the
  same six scoped ASan/UBSan cases plus three working controls. The real
  gRPC component passes three cases but fails its new exhausted-control
  path: pinned gRPC invokes the factory before binding the synchronous call,
  making the proposed early TryCancel unsafe. Preserve the normal/ASan
  failures and subsequent SDK-order diagnostic builds. No new candidate is
  frozen and no node is deployed. See issue-9-m1-grpc-control-ownership.md;
  do not qualify the current repair using R13's older node/runtime results.
  The separate SDK-order diagnostic later passes four normal and four scoped
  ASan/UBSan server cases plus three controls. Eight small default-TSan cases
  and thirteen source/archive/configuration recipe checks pass. The repository's owned SDK
  module builds/links the expected Release archive after preserving its first
  header-inventory and Debug-selection failures. Expanded fallback allocation
  faults pass eleven normal/ASan/control invocations with seven cancellations
  and exact final release. Six real-gRPC default-TSan cases pass with the
  instrumented SDK and clean/known-race callback controls. A new complete
  candidate and its final native/sanitizer, paired and runtime gates remain
  pending; the earlier R13 node does not qualify these changes.
  R14 subsequently freezes and fails configuration because the assembled
  build file includes an unrelated P2P test registration without its source.
  R15 excludes only that registration from its snapshot, preserving all
  workspace P2P changes. R15 source 3be9aa55f128f649d851ec87ca9dfa3574736eec0bdec9e4e9ebaecbae47995f
  builds all 71 normal ELFs and identifies 168 linked libraries. Node SHA-256:
  e825f1951a583590e9173c57be79626635dbb110f750d1258eb34efffa31eef4.
  Its 104/104 unique normal checks, both CLI checks and 63/63 focused
  ASan/UBSan invocations now pass on OVH. The original restore early-guard
  result and a TSan evidence-helper prefix failure remain preserved. Default
  At that checkpoint, TSan, paired and runtime gates were pending with no
  observer/producer starts. The later final R15 result below supersedes that
  pending status; earlier failures and counters remain preserved.

## Build And Test Workflow

The complete repository-local dependency build is:

```bash
./scripts/build-cpp-libp2p-koinos.sh
```

It places dependencies under `.deps/` and the native build under `build/`.
For a configured tree, use a focused rebuild when appropriate:

```bash
cmake --build build --target teleno_node --parallel
```

The build script defaults `KOINOS_BUILD_TESTS` to `OFF`. For a full test-enabled
build matching CI:

```bash
KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

At minimum, verify the binary identity and CLI surface after relevant native,
build, dependency, or release changes:

```bash
./build/teleno_node --version
./build/teleno_node --help
```

Run focused tests for the changed component, then broader CTest when shared
runtime, storage, networking, backup/restore, producer, build, or dependency
behavior changes. Do not claim successful validation for tests that were not
built or run.

## LR2 Development Data Placement

Operator execution-time amendment, 2026-10-21: continuing M1 memory
optimization, builds and qualification have no overall execution deadline or
maximum duration. Do not impose the older eight-hour/fixed-window rules on
future work. Preserve closed-run deadlines, results and counters as historical
evidence. Continue with auditable source/candidate identities and unchanged
non-time resource/acceptance limits. Tool observation timeouts do not establish
failure of a live process; recheck its actual handle/progress. Every executable
M1 test remains OVH-only, with generated evidence on mounted My Passport and
the operational Hetzner producer outside scope. See the current execution
controls in `docs/implementation/issue-9-m1-aggregate-goal.md`.

Operator amendment, 2026-10-20: all future executable M1 tests run exclusively
on the new isolated OVH VPS. This includes development reproductions,
unit/regression/failure-injection tests, full native suites, sanitizer suites
and clean/known-fault controls, controlled-chain fixtures, paired measurements
and native version/help checks. Do not run these tests on the Mac, miner or
another host. My Passport and the authorized builder may prepare/store inputs
and compile without test execution; static source/document checks may remain
local. Keep generated evidence on mounted My Passport and tests in owned OVH
directories. Preserve historical results and all existing deadlines, counters,
resource limits and recovery reserve. Schedule later OVH tests separately from
the active frozen-C9 sequence. Do not touch the operational Hetzner producer.
This M1 amendment overrides earlier M1 test-location permissions below. See
`docs/implementation/issue-9-m1-receipt-output-repair.md`.

The primary optimization and qualification target is the user-designated Linux
baseline VPS, including producer operation: Ubuntu/x86_64/ext4, 2 vCPUs,
4 decimal GB physical RAM, 40 GB-class disk and no swap, as verified for the
replacement server on 2026-10-15. The first 1-vCPU/2-GB server was explicitly
retired by the user; do not contact it or use it as a fallback. Do not enlarge
the mandatory baseline. See `docs/implementation/issue-9-goal.md`.
Use Mac results only as development diagnostics; they cannot close Linux memory,
catch-up, durability or recovery gates. Prefer portable changes justified by
Linux measurements rather than APFS/macOS-specific performance tuning. This
priority does not authorize live VPS operation or creation of a Linux VM.

The subsequent 2026-10-15 request authorizes isolated development/qualification
on the designated baseline and persistent SSH access setup, including necessary
password recovery. Keep access details and private host inventory outside source
and public reports. This does not activate mainnet production or waive network,
producer-target, key/registration, observer-first and recovery gates.

The user explicitly selected the mounted My Passport volume for new LR2
builds, diagnostic artifacts, test databases and temporary data. Keep source
and committed documentation in this repository, but use a fresh owned run
under `/Volumes/My Passport/` for generated development data. Set `TMPDIR`,
`TELENO_TEST_ARTIFACT_ROOT` and `TELENO_TEST_BUILD_ROOT` explicitly and disable
Python bytecode writes for diagnostics. Test-root selection does not authorize
live data access, restore activation or source-trust changes. External CMake
caches must still name this exact source repository. Read existing pinned
dependencies as inputs; do not relocate/delete existing chain databases or
user data. If the volume is absent, stop data-producing work rather than
silently falling back to the internal disk. Record exact owned paths in the
LR2 completion matrix and project memory, not operational credentials.

## Versioning And Release Identity

- `VERSION` is the native SemVer source of truth. It must contain SemVer
  without build metadata.
- Development state uses a prerelease value such as `1.2.0-dev.0`.
- `teleno_node --version` reports
  `<version>+<git-commit>[-dirty]`.
- Native release tags use `teleno-node-v<version>`.
- The native version is independent of the Koinos One desktop app version; do
  not keep the two version streams in lockstep.
- `CHANGELOG.md` documents user-visible and operator-visible native behavior.
  Keep its unreleased version aligned with `VERSION`.

When making a native release, treat versioning, release notes, tests, binary
identity, CLI smoke checks, and container verification as required release
work. Drop the development prerelease only for the actual release, date the
matching changelog section, and create or push the release tag only after the
source, changelog, binary, and intended artifacts agree. Immediately after a
release, advance the working tree to the next intended development version and
start a new unreleased changelog section.

## Protocol And Compatibility Guardrails

- Preserve Koinos protocol compatibility and mainnet behavior. Do not trade
  compatibility for local performance or implementation convenience.
- Treat the legacy microservice stack as the behavioral reference where Teleno
  parity is uncertain.
- Keep compatibility evidence, migration tooling, and validation reports when
  they prove protocol parity, client compatibility, storage durability, or
  restore safety.
- Do not revive GarageMQ or legacy microservice build/start/packaging surfaces
  inside this repository.
- Do not claim full legacy service parity without checking the current
  implementation and the linked service-coverage references. Account history
  and other optional services must be described according to their actual
  current feature coverage.

## Mainnet Safety Guardrails

- Never put private producer addresses, private keys, wallet material, bearer
  tokens, SSH credentials, private host inventory, or other local operational
  secrets in committed code, tests, logs, examples, or documentation.
- Public material must use placeholders such as
  `<YOUR_MAINNET_PRODUCER_ADDRESS>`.
- Do not perform hidden or background mainnet mutations.
- Do not transfer funds away from a protected producer address.
- Mainnet producer registration, VHP burns, producer setup changes,
  default-account changes, producer-targeted config writes, transaction
  signing/submission, and block-production activation require a fresh explicit
  user request, confirmation of the network and target, and a reviewable plan
  or dry run first.
- Before any chain-mutating operation, verify the network, basedir, config,
  signer, target address, and operation type.
- Start and validate restored or newly configured nodes as observers. Enable
  block production only after database health, network identity, head progress,
  peer health, producer address, VHP, and producer-key checks pass.

If local-only operational memory exists, read it when the task requires that
context, keep it untracked, and never quote or copy its private details into
public repository files.

## Backup, Restore, And Recovery Guardrails

- Native backup and restore behavior is owned by `teleno_node`; update
  `docs/backup-restore-cli.md` with behavior changes.
- Keep backup administration local-only unless the user explicitly scopes a
  different deployment. Admin endpoints must remain loopback-bound and
  bearer-token protected by default.
- Public bootstrap is a public read-only backup source, not public admin API
  exposure.
- Backups must not include wallet files or producer private keys.
- Stop unmanaged processes that use the same basedir before direct CLI restore
  or restore activation.
- Preserve observer-first recovery markers and behavior after restore.

If the node reports `block previous state merkle mismatch` or another
persistent state-merkle mismatch:

- preserve the existing state database;
- do not clear `chain/blockchain`;
- do not start from an empty state database as the first action;
- do not force a fresh full resync as the first action;
- collect diagnostics and attempt validation-based recovery first;
- move or delete state only after explicit user approval and evidence that
  validation-based recovery failed.

## Runtime And Deployment Cautions

- Repository work does not authorize changes to live servers or running nodes.
- Check the actual basedir, config path, selected network, process ownership,
  service path, and current runtime state before touching a deployment.
- A host may run legacy services alongside observer or producer deployments.
  Do not assume process or data ownership from a hostname alone.
- Keep JSON-RPC and gRPC exposure intentional. Keep admin surfaces local and
  protected; do not solve connectivity or authorization failures by exposing
  privileged endpoints.
- Prefer observer-safe diagnostics. Do not delete chain state, activate a
  restore, or enable production as the first troubleshooting action.

## Koinos One Handoff

Koinos One is Teleno's GUI frontend. When a Teleno change affects desktop users
or operators, update the relevant documentation in both repositories as part
of the same task. This includes the Koinos One app manual and, where relevant,
its changelog for changes to configuration, CLI behavior, backup/restore,
recovery, safety gates, resource requirements, or user-visible node behavior.

Before editing Koinos One documentation, locate its checkout, read its own
agent instructions completely, inspect its worktree, and preserve all existing
changes. Keep documentation in English and distinguish native capabilities
from functionality actually exposed by the GUI or included in a packaged
release. If the checkout or required evidence is unavailable, report the exact
blocker and the pending documentation updates; do not claim the handoff is
complete.

When a Teleno change affects Koinos One packaging or user-visible behavior,
report the exact Teleno commit and native build identity needed for the
submodule update. An authorized integration must deliberately update the
submodule pin, rebuild or stage the native binary, and run Koinos One's own
package checks. Documentation coordination alone does not authorize GUI or
packaging code changes, submodule updates, commits, pushes, releases,
publication, deployments, live-node operations, or block-production activation.


M1 continuation checkpoint: C14 default TSan completes 52/52 scoped invocations
with working generic/actual SDK controls. Its SC5 failure and HTTP refusal gap
remain. The unfrozen revision-1 HTTP ownership case passes, but mixed shapes
abort134. An OVH terminate trace confirms temporary JSON allocation inside a
noexcept literal version comparison. Revision 2 borrows the existing string and
adds retry after each refusal fault; compilation is not qualification. Preserve
all prior failures/counters and the stopped full-history test database. No
operational Hetzner producer operation is authorized by this continuation.

R15 TSan linkage reconciliation: the initial matrix has 26 passes and fails
its gRPC exhausted-control case. The qualification configure command used an
incorrect -U argument, retaining the old R12 overlay; the actual test linked
the unrepaired instrumented C++ SDK instead of the built replacement. Preserve
that failure. Corrected build-only configuration uses the new four-archive
overlay and validates the resolved final C++ archive input. Normal/ASan results
retain their identities. No paired, controlled-chain or observer phase starts
from the failed TSan result. M1 remains open; see the R15 validation report.

R15 corrected sanitizer checkpoint: 63/63 focused ASan/UBSan and 61/61
unique default-TSan invocations pass with working controls and explicit
prebuilt/libp2p exclusions. TSan uses 40 new executions plus 21 passes of
byte-identical final ELFs; changed binaries are rerun. Its preserved wrong-SDK
link failure is not relabeled. Paired OVH measurements are running; controlled
and full-history final-node gates remain pending. No Hetzner action occurs.

R15 final M1 decision, 2026-10-23: M1 is complete for source archive
3be9aa55f128f649d851ec87ca9dfa3574736eec0bdec9e4e9ebaecbae47995f and node
e825f1951a583590e9173c57be79626635dbb110f750d1258eb34efffa31eef4. All 104
normal, two CLI, 63 focused ASan/UBSan and 61 default-TSan cases pass. Eight
owner groups, bounded refusal controls, five equivalent pairs per measured
workload, exact-source 100-block/12,800-transaction create/reopen and the
exact full-history observer's one-hour reads, clean restart and owned-process
interruption/recovery pass. Observer peak RSS is 419,278,848 B; controlled
peak is 705,712,128 B. Strict host-memory/disk/reserve, no-swap/OOM gates pass.
The isolated observers finish stopped; the full-history database remains.
Earlier failures/deadlines/counters and unrelated P2P source/build changes are
preserved. M1's quotas are not physical RSS ceilings; declared adjacent and
quota-exempt work remains explicit. Issue 9, LR2 and other requirements remain
open. No Hetzner contact, deployment, mainnet transaction/signing, funds,
commit, push, release or app submodule/binary update occurs. See
`docs/validation/low-resource-lr2-m1-r15.md` for exact identities and proof.

Native 1.3.0 publication checkpoint: the separate operator-authorized release
integrates M1 and Fogata corrections at `2c11229267cff2f9ee39ed8e56ab70af391d85ce`.
Native artifacts and exact OVH checks are published; see
`docs/validation/native-1.3.0-publication.md` for identities and container status.
The working release branch resumes at `1.3.1-dev.0`; the original mixed LR2
tree, earlier evidence/counters and operational producer remain outside this
publication change. Koinos One receives documentation only, without a pin/package update.
