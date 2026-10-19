# Native Release Builds

The native node can be bundled into packaged Koinos One releases or released as
an independently installable runtime. Release work must preserve both the app
build identity and the native binary identity.

## Independent Version Source

The **1.3.0-alpha.1** native prerelease publishes the experimental LR2 development
snapshot with [detailed updates and qualification limits](releases/1.3.0-alpha.1.md).
Its explicit operator authorization permits an alpha while the broader issue-9
qualification remains open. Preserve those limitations in release assets/notes;
do not describe the alpha as a stable or fully qualified low-resource release.

The native runtime version is independent from the Koinos One desktop app
version. The source of truth is:

```text
VERSION
```

That file contains SemVer without build metadata, such as `1.1.0` or
`1.2.0-beta.1`. CMake reads it when compiling `teleno_node`, then appends the
Git revision to the runtime build version:

```text
teleno_node 1.1.0+d770931f42b8
```

Use `teleno-node-v<version>` for independent native release tags. A Koinos One
release tag such as `v1.0.4` can include any validated `teleno_node` build; the
two SemVer streams do not need to advance together.

## Native Build

The active binary target is:

```bash
cmake --build build --target teleno_node --parallel
```

Old automation can still invoke the compatibility target:

```bash
cmake --build build --target koinos_node --parallel
```

That target depends on `teleno_node`.

## Package Handoff

### LR1 Development Gate

Before releasing the current LR1 backup/persistence changes, review the
[LR1 report](validation/low-resource-lr1-prerequisites.md). Require current
storage/publication/transfer regression tests, supported-platform build and
container evidence, and an operator-reviewed new checkpoint-identified public
source. Current metadata cannot be repaired by copying a chain ID into an old
manifest. Deploying the corrected writer, creating/signing/publishing a new
backup and integrating its native revision into Koinos One are separate explicit
authorizations. No new release or packaged integration exists from this work.
Do not advertise full-history Linux/40-GB support without LR5 qualification.

LR2 additionally changes native RPC deadlines/session ownership and cache
behavior. Review its [memory report](validation/low-resource-lr2-memory.md),
current lifecycle/cache/regression tests and supported sanitizer results before
release. Integrated ASan/TSan and whole-node resource gates are not replaced by
passing small fixtures. No qualified low-resource profile or committed native
integration pin exists from this local work.

For [R1-A body lifetimes](validation/low-resource-lr2-rpc-bodies.md), require
complete large-response/base64, pipelined requests, gated admitted dispatch,
slow-reader cancellation, repeated stop/restart, CORS and success/error/batch
regressions. Writes must retain their response until the completion callback.
Review actual before/after attribution separately from fixture/client RSS;
supported integrated sanitizer and representative Linux gates remain required.
Body cleanup does not qualify aggregate service/serialization/output admission.

For [R1-B batch serialization](validation/low-resource-lr2-rpc-batch.md), require
exact compact response bytes, escaped/numeric IDs, empty/mixed error batches,
single invocation/order and first/middle/last/multiple invalid-UTF8 regressions.
No partial HTTP output or skipped later accepted call may follow serialization
failure. Keep the final body owned through writing and require integrated
diagnostics/Linux whole-node proof before making capacity claims. A reduced
synthetic serialization working set is not compatible aggregate output admission.

For [V1-C parsed-module capacity](validation/low-resource-lr2-vm-capacity.md),
require pinned-layout inventory, checked overflow, exact/oversize/replacement
and allocation-fault tests, nested execution/metering and held-owner lifetime
regressions. Strict config must reach the actual controller/backend; warm hits
must remain allocation-free. Review five-sample cache-retention and reparsing
tradeoffs separately from total heap/RSS. Keep default zero unless representative
Linux evidence justifies another policy. Caller/instance/queue bounds, integrated
ASan/TSan and whole-node qualification remain required; no GUI/package change
or committed native integration pin follows from documentation.

For [V1-D host-memory access](validation/low-resource-lr2-host-memory.md), require
real thunk/syscall/Fizzy regressions with unaligned and exact-end addresses,
success/error/short buffers, argument/result aliases, identical before/after
bytes and metering, plus bounds rejection before host entry. Do not defer count
writes or reject previously accepted unaligned addresses. Review D1 standalone
runtime attribution: clean pre-main failures are neither passing integrated
ASan/TSan nor proof of a Teleno defect; functioning diagnostics, current
historical/differential and representative Linux gates remain required.

Packaging scripts stage app files, assets, and the native binary before running
Electron Builder. Verification scripts check that the packaged app contains the
expected native CLI surface.

## Build Identity

Every packaged release should expose:

- Koinos One product version;
- release channel;
- build timestamp;
- Git commit and branch;
- dirty/clean source state;
- native node semantic version, build version, release tag, binary hash, and
  metadata.

Use
[`scripts/generate-build-info.js`](https://github.com/koinos/koinos-one/blob/main/scripts/generate-build-info.js)
as the build identity source for the app surface.

Koinos One's packaging pipeline also generates a native build-info record
(`teleno-node-build-info.json`) with the native SemVer, build version, release
tag, source revision, binary hash, and expected container image tags.

## Linux Container Release

The Linux container workflow publishes the independent runtime image:

```text
ghcr.io/koinos/teleno
```

The native binary version comes from `VERSION`. The workflow adds standard OCI
metadata labels from the Git ref/tag set, while the Dockerfile records the
exact `VCS_REF` revision and `BUILD_DATE`. The container smoke test runs:

```bash
docker run --rm teleno-node:ci --version
```

and verifies that the reported version starts with the native SemVer from the
repository. On native release tags or manual dispatch, the workflow publishes
version tags such as:

```text
ghcr.io/koinos/teleno:1.1.0
ghcr.io/koinos/teleno:beta
```

The `beta` tag continues to track `main`. Native prereleases publish explicit
version/commit tags without replacing `latest`; only a matching stable native
release tag advances `latest`. Manual dispatch on another branch does not
automatically update `beta`.

For local Docker builds, pass the exact Git revision so the binary build
identity and the OCI revision label agree. The OCI version tag still comes
from `VERSION`:

```bash
TELENO_NODE_VERSION="$(tr -d '[:space:]' < VERSION)"
TELENO_COMMIT="$(git rev-parse HEAD)"
docker build \
  --build-arg VCS_REF="$TELENO_COMMIT" \
  -t "teleno-node:${TELENO_NODE_VERSION}" .
```

Use `--build-arg JOBS=1` on memory-constrained builders. The limit applies to
the main build and nested Hunter dependency builds.

## Release Safety

Do not ship a native node build that diverges from Koinos protocol behavior.
Producer, backup, restore, and storage changes require validation that matches
their blast radius.

## Native Release Checklist

- Build `teleno_node` from the intended source revision.
- Confirm `VERSION` is the intended native SemVer.
- Run focused native tests for changed components and broader CTest when shared
  behavior changed.
- For LR2 P2P retention changes, run accepted-ID cache and fake-transport sync/
  gossip tests, review eviction/expiry and pending-duplicate semantics, and
  record the [scoped measurements](validation/low-resource-lr2-p2p-retention.md).
  Local cache RSS savings are not total-node memory or mainnet block-following
  qualification; integrated lifetime/race and Linux workload gates remain open.
- For replay-correctness releases, record checked and full historical replay
  across blocks 30,504,202 and 32,789,377-32,789,378, the fallback count, exact
  exception behavior, the audited duplicate-key receipt range, restart
  checkpoints, and differential roots against the tagged reference
  implementation.
- For LR2 sync-worker changes, review the
  [completion matrix](validation/low-resource-lr2-completion-matrix.md) and
  [worker report](validation/low-resource-lr2-workers.md). Require supported
  integrated ASan/TSan and real transport/callback shutdown qualification;
  count limits and fake-transport passes do not prove whole-node memory or a
  universal shutdown deadline. External build caches must still name the exact
  Teleno source checkout. Do not ship a dirty checkout as a submodule pin.
- For LR2 admission changes, run refusal/retry/duplicate/concurrent and
  post-insertion notification regressions, then full CTest. Review
  [the admission report](validation/low-resource-lr2-admission.md); visible-head
  counts are not fork/byte bounds. Require representative Linux/x86_64/ext4
  measurements before claiming low-resource VPS qualification. Mac passes and
  dirty native binaries do not prove this gate or packaged GUI integration.
  Do not publish LR2 as complete with global admission or lifetime gates open.
- For standalone transaction-work admission, verify pre-validation refusal,
  lease allocation failure, concurrent count/wire ceilings, exception cleanup,
  unchanged signed-fixture receipts, native insertion/observer ordering and
  full replay fallback while work is saturated. Verify JSON-RPC `-32005`/ID/
  retry data, gRPC `RESOURCE_EXHAUSTED` and no P2P misconduct/cache on local
  overload. Review [M1-E evidence](validation/low-resource-lr2-transaction-work.md).
  Zero/default axes are unchanged; wire/count limits are not fork/heap bounds.
  Select VPS settings from representative Linux measurements only; no Mac
  fixture or new option qualifies LR2 or packaged Koinos One behavior.
- For multi-fork admission changes, run the complete one-shot allocation-failure
  sweep, readmission and fork-specific nonce regressions, plus the zero-allocation
  map transfer and pre-publication invalid-batch/root/tombstone checks. Review
  [M1-C evidence](validation/low-resource-lr2-admission-atomicity.md). Preserve
  the exact write-only/map/locking scope; this does not qualify persistent
  commits, block/LIB/removal/prune OOM paths or total memory capacity. Require
  integrated Linux/sanitizer/historical evidence before a whole-LR2 claim.
- For fork-head expiry/removal, run the full allocation-failure/retry sweeps,
  duplicate/absent ID and freed-nonce/RC checks, plus prepared batch zero-new,
  ordinary-squash root/tombstone equivalence, ownership and reset regressions.
  Review [M1-F evidence](validation/low-resource-lr2-cleanup-atomicity.md), including
  decoding failures and temporary bucket/iterator costs. This is map-only local
  cleanup, not block/LIB/persistent atomicity, hard OOM or a qualified Linux cap.
  Aggregate capacity and integrated historical/Linux/sanitizer gates remain.
- For state-value ownership changes, run direct/iterator read eviction,
  overwrite/erase/close, uncached oversize/zero-budget, clone isolation and
  state-root/tombstone/reopen regressions. Review
  [S1 evidence](validation/low-resource-lr2-state-ownership.md) and small-value
  overhead. Supported integrated ASan/TSan, current historical differential
  validation and representative Linux measurements remain release gates;
  deterministic ownership probes are not sanitizer or whole-node proof.
- For pending-record sharing/budgets, verify actual owner/capacity boundaries,
  multi-account/concurrent admission, unchanged rejected preparation/retry,
  ancestor/clone/squash/LIB/caller lifetimes and persistent copy/reopen. Verify
  signed receipts, saturated canonical replay and the JSON-RPC reason distinction,
  gRPC status and no P2P penalty/cache. Review [M1-G evidence](validation/low-resource-lr2-pending-records.md).
  Do not ship Mac-derived VPS limits or call live-record string capacities a
  full heap bound: keys/indexes/nonces/RC/tombstones/encoding/cleanup workspace
  remain additional. Full integrated Linux/historical/sanitizer gates stay open.
- For VM ownership/cold-parse changes, run parse/shared-owner allocation faults,
  optional cache-publication failure/retry, concurrent same-ID/warm-during-cold,
  eviction/caller lifetime, nested host calls, fresh mutable instances and
  unchanged metering/error propagation. Review
  [V1-A evidence](validation/low-resource-lr2-vm-lifetime.md) and
  [V1-B allocation correction](validation/low-resource-lr2-vm-allocation.md).
  Verify every native build uses the pinned archive/patch, both context
  constructors fail cleanly, parse/instance/context errors remain local and
  transaction dispatch does not emit an accepted/reverted receipt. Existing
  installs are not silently patched. Require
  actual Linux diverse-contract/queue/instance measurements, functioning
  integrated ASan/TSan and historical parity before V1/LR2 qualification.
  Controlled Mac bursts and entry counts are not whole-node/parsed-byte bounds.
- For indexed-fork retention diagnostics, run clone/owner/tombstone, invalid-lock,
  unmeasured persistent backend/reopen, expiry/LIB, concurrency and scan-allocation
  failure checks. Review [M1-D evidence](validation/low-resource-lr2-retained-memory.md).
  Preserve exclusions and exclusive-lock scope; do not present string capacities,
  empty-head counts or Mac scan timing as aggregate heap/admission/Linux proof.
- For restore changes, verify that the persistent producer recovery hold
  survives a restart, cannot be released on the first post-restore startup,
  and is removed only by a later explicit `--enable block_producer` restart.
- For single-copy direct restore, review the
  [SC0–SC6 plan](implementation/single-copy-backup-restore-plan.md) and
  [SC3 report](validation/single-copy-restore-sc3-activation.md) and
  [SC4 CLI report](validation/single-copy-restore-sc4-cli.md),
  [SC5 capacity report](validation/single-copy-restore-sc5-capacity.md) and
  [SC6 blocked-readiness handoff](validation/single-copy-restore-sc6-handoff.md): local engine/CLI
  tests do not qualify direct runtime admin exposure, Linux filesystem
  recovery, a representative backup or the 2-vCPU/4-GB/40-GB server. Retain those
  pending gates and known runtime-storage audit blockers before rollout claims.
  A Koinos One handoff must pin a committed, validated native identity; a dirty
  local SC0–SC5 build is not a releasable submodule revision. Preparing SC6
  documentation does not waive failed tests or authorize publication/deployment.
- For this replay-correctness release, use the
  [authorization-gated canary and handoff checklist](release-chain-v1.5.2-fast-replay-checklist.md).
  Preparing that checklist does not authorize its deployment, publication,
  producer, or Koinos One commands.
- Confirm `teleno_node --version` matches `VERSION` plus the
  intended Git revision.
- Confirm `teleno_node --help` exposes expected CLI surfaces.
- Generate `build/generated/teleno-node-build-info.json` for independent native
  release notes when publishing CLI/container artifacts.
- Run package staging and packaged-app verification.
- Check About/Build Info for product version, commit, release channel, build
  timestamp, and native runtime identity.
- Update changelog or release notes for user-facing behavior.
