# Changelog

All notable changes to the Teleno node runtime are documented in this file.
Release tags use the form `teleno-node-v<version>`; the version source of
truth is the `VERSION` file at the repository root.

## [1.3.0-alpha.1] - 2026-10-19

This experimental native prerelease publishes the accumulated low-resource,
storage, networking, memory-ownership and producer work. It preserves full
canonical history and keeps the remaining issue-9/LR2 qualification gates open.
See the [detailed alpha release notes](docs/releases/1.3.0-alpha.1.md).

### Validation and release scope

- The identified predecessor runtime completes finite producer commissioning
  and produces mainnet block 40,323,375, independently verified canonical and
  irreversible. The block contains zero transactions; this does not qualify a
  loaded producer or close all issue-9 requirements.
- Selected-backup restoration and measured real-data compaction fit within
  40,000,000,000 whole-server bytes including the explicitly selected 8-GiB
  reserve. Preserve the source/input-specific evidence and standard 10-GiB CLI
  default; these measurements are not an indefinite storage-capacity promise.
- Preserve the published 1.2.1 compatibility fixes, observer restart regression,
  hash-pinned GMP mirror, bounded downloads, explicit CI test roots and
  deterministic backup missing-object test alongside the accumulated changes.
- Retain the clean Linux binary, native version/help, complete JUnit report,
  linked-library hashes and committed-source manifest as CI release inputs.
  Prerelease container tags cannot replace `latest`; `beta` follows `main`.
- Apply pinned dependency patches independently of enclosing/inherited Git
  worktrees, with unchanged archive and patched-file hashes. Discover the exact
  Protobuf package in directory scope so fresh Hunter builds retain its version.
  Keep reusable dependency state after build failures and avoid recompiling
  the container's native runtime for a timestamp-only image publication.
- Link the restored compatibility fixture against the current mempool library.
  Alpha containers package the exact tested CI executable after source/hash/test
  verification; native downloads and the image retain the same ELF identity.
- The alpha is an independent Teleno release. Koinos One GUI packaging and its
  native submodule pin are separate; no running node is upgraded by publishing it.

### Changed

- Standalone direct restore adds the opt-in `experimental-8gib` reserve profile,
  pinned across stage/resume/activation with consistent space checks. The
  standard 10-GiB minimum and 20% term remain unchanged. The smaller profile is
  an experiment, not a general capacity qualification or a GUI feature.
- Optional `rocksdb.max-total-wal-size` selects a soft WAL flush trigger in
  bytes; zero retains RocksDB's existing automatic policy. Preflight reports
  the configured policy without opening storage or claiming a disk bound.
  Synchronization and record retention remain unchanged. See the
  [WAL measurements and qualification status](docs/validation/wal-retention-budget.md).
- Native direct CLI activation retains both ownership locks across validation
  and activation. A freshly validated, fully rehashed payload avoids one
  duplicate database scan; final validation after rename remains mandatory.
  Interrupted operations and standalone activation still validate afresh.
  See the [validation status](docs/validation/direct-restore-scan-reuse.md).
- Native direct HTTPS restore reserves read-buffer capacity within its existing
  bounds, reducing small checked writes and transport CPU overhead. Size/hash,
  cancellation, deadline, source-trust and durability checks remain unchanged.
  See the [paired measurements](docs/validation/direct-restore-read-buffer.md).
- Native SDK address retention uses one owned address representation and shared
  count/slot/encoding limits across runtime generations. Incremental eviction
  planning avoids scanning/materializing the entire cache for each insertion.
  Owned periodic metadata collection stops with its generation. Failed
  preparation preserves prior addresses and retry visibility; configured roots
  and live connections stay protected from optional eviction. These internal
  C++ settings add no YAML/CLI/GUI control or complete heap/RSS bound. See
  [P2-G development status](docs/validation/low-resource-lr2-sdk-addresses.md).
- Native peer advice has finite count/string retention and preserves protected
  startup/connected advice. Known/connected views use synchronized native
  observations instead of unsafe external SDK reads. Move-only C++
  `PeerSnapshot` results retain shared admission credits through caller lifetime
  and restart; native C++ consumers must rebuild. Local metadata saturation
  remains separate from protocol/peer validity. No new YAML/CLI/GUI control or
  complete transport/RSS ceiling is implied. See
  [P2-F](docs/validation/low-resource-lr2-peer-snapshots.md).

### Fixed

- Native producer candidate assembly retries a temporarily unavailable mempool
  parent after 10 ms and discards candidates whose observed head changes.
  One hundred consecutive deferrals expose the persistent failure; unrelated
  failures retain their original handling. Pending transactions are not replaced
  with an empty response. New log counters distinguish prepared candidates,
  PoB evaluations, handoff retries and errors. See the
  [development validation](docs/validation/native-producer-handoff.md).
- Pinned Protobuf arena strings remain safely destructible when copying a
  payload fails. The runtime constructs an empty string before attempting the
  throwing copy, preserving field/source bytes and public headers. See the
  [causal checks and remaining qualification](docs/validation/protobuf-unknown-field-ownership.md).
- Mempool cleanup owns partially decoded protobuf records through allocation
  failure. Native builds use the pinned Koinos protobuf runtime with temporary
  ownership retained until unknown string/group fields are published, avoiding
  leaks if container allocation fails. Wire data and future fields are preserved;
  see [validation and remaining gates](docs/validation/protobuf-unknown-field-ownership.md).
- Mempool admission releases partially copied protobuf repeated fields after
  allocation failure by owning its temporary record in an arena. Record bytes,
  caller data and fork publication semantics are preserved. See
  [validation and remaining gates](docs/validation/mempool-admission-copy-ownership.md).
- Linux `rss_bytes` diagnostics read resident pages from `/proc/self/statm`
  instead of virtual size. This corrects reporting; it does not reduce actual
  memory use. See [validation](docs/validation/linux-rss-counter.md).
- Explicit native peer dials reserve finite count/peer-string credits and release
  them with the last real owner across cancellation/restart. Preparation and queue
  allocation failures no longer leave a stale connecting marker that suppresses
  retries. SDK-supported startup seed spellings share admission before the I/O
  runner starts; connection notifications are deduplicated
  and optional known-peer advice cannot suppress a completed connection.
  These internal component limits do not bound SDK/implicit dials or whole-node
  memory. See [P2-E](docs/validation/low-resource-lr2-p2p-dials.md).
- Peer scores group changing ports by network address, decay during admission,
  saturate safely and resist repeated-query rounding. Finite native score-record
  and attributed key-capacity limits refuse new unknown bookkeeping locally
  without peer blame or evicting nonzero scores. Optional candidate retention
  trims even with discovery disabled, preserving configured seeds and connected
  metadata; scoped allocation failures no longer terminate discovery. These
  component policies do not bound whole-node memory or qualify the target VPS.
  See [P2-D](docs/validation/low-resource-lr2-peer-bookkeeping.md).
- Outgoing Peer RPC owns open/write/read completion across timeout, stop and
  restart. One six-second deadline spans a complete response; received fragments
  do not renew it. The client validates read counts, enforces its existing
  logical receive ceiling and reuses payload-dominated frames for decoding,
  shedding large ignored regions for small results. Local
  lifecycle/capacity refusals do not penalize peers; a later block sync request
  reduces its batch after local receive-capacity failure, before application.
  Encoded protocol bytes and history are unchanged. Aggregate memory and full
  Linux/producer qualification remain open. See [P2-C](docs/validation/low-resource-lr2-p2p-client.md).
- Incoming Peer RPC requests accumulate fragmented reads in one owned 64 KiB
  buffer, with a six-second acquisition deadline and stop/restart generation
  cancellation. MessagePack skipping uses constant stack space. Reconnecting
  after transport restart no longer retains stale connected/connecting entries.
  Wire bytes remain unchanged. Scoped native regressions do not qualify
  aggregate transport capacity or full-history operation on the target VPS.
  See [P2-B](docs/validation/low-resource-lr2-p2p-reads.md).
- Native Peer RPC requests and responses complete successful partial libp2p
  writes before reporting success or closing the response. Encoded bytes remain
  unchanged; zero progress, invalid counts and transport errors report failure.
  This is an unreleased scoped correction, not whole-node memory or Linux
  producer qualification. See [P2-A](docs/validation/low-resource-lr2-p2p-writes.md).
- Complete a missing archived block-receipt state root from an otherwise
  identical validated accepted receipt, preserving blocks, execution fields,
  existing roots and topology. Conflicting completion is refused. This has
  component regression evidence; Linux/whole-node qualification remains pending
  and no historical archive repair or live producer update is included. See
  [the checkpoint](docs/validation/block-receipt-completion.md).

### Added

- Optional block Zstd dictionaries also accept 128 KiB, with unchanged zero
  defaults and training/buffer limits. Component comparisons expose substantial
  rewrite CPU tradeoffs; complete small-server qualification remains open.
  See [measurements](docs/validation/larger-block-dictionary.md).
- Explicit offline `--compact-rewrite` applies the selected compression policy
  to existing bottommost tables, with argument/existing-DB checks before
  filesystem initialization. New native SSTs omit the RocksDB host identifier;
  existing tables remain readable. Use an isolated copy with temporary capacity
  and create/verify a native backup afterward. Full 40-GB and whole-node
  qualification remain open; see the [rewrite checkpoint](docs/validation/native-archive-rewrite.md).
- Opt-in `rocksdb.blocks-zstd-level` and
  `rocksdb.blocks-zstd-dictionary-bytes` settings retain an explicit block-table
  compression policy across restart. Dictionary training has finite per-builder
  sampling/buffering settings; other column families and existing defaults are
  unchanged. Invalid combinations and unavailable Zstd support are refused.
  Existing tables are not automatically rewritten. See the
  [development validation](docs/validation/native-block-compression-policy.md);
  complete 40-GB restore and whole-node qualification remain open.
- LR0 repository-local full-history observer baseline tooling with a versioned
  resource contract, build/host/source manifest, SST/WAL allocation categories,
  owned-child CPU/RSS and optional Linux/cgroup counters. Missing metrics and
  failing baseline checks remain explicit; this does not change runtime defaults
  or qualify mainnet/full-history operation on a 40-GB server.
- Repository-local single-copy restore experiment tooling: signed synthetic
  same-input comparisons, five paired measurements, allocation/free-space/RSS
  evidence, observer restart/interruption checks and optional bounded APFS images.
  This is diagnostic tooling, not a new runtime default or Linux/40 GB guarantee.

- Opt-in `--backup-restore-mode direct --backup-restore-preflight` metadata-only
  planning with bounded metadata acquisition, immutable input identity,
  fresh-target checks and mode-aware space estimates. The target is not created
  or opened; repository restore remains the default. Planning success alone is
  not activation or 40 GB qualification; the separate SC4 workflow is opt-in.
- Internal direct-staging engine and isolated diagnostic fixtures: bounded
  streamed object transfer, exact size/hash checks, pinned attempt identity,
  checked file/journal synchronization, file-level resume and reserve-aware
  space checks. It never opens or activates the target DB. The SC4 CLI uses this
  same staging engine.
- Internal offline direct-restore validation and crash-recoverable activation:
  read-only unified-DB/genesis/chain consistency checks, generation-bound journal
  and observer markers, exclusive same-filesystem rename with no copy fallback,
  and explicit offline recovery after interruption. Runtime direct admin and
  40 GB server qualification remain pending.
- Standalone opt-in direct CLI staging, `--backup-restore-resume` and separate
  explicit activation with text/JSON reports, source-trust/identity/state and
  measured-byte fields, cooperative cancellation, pinned metadata authentication
  and observer-held recovery. Repository mode remains the default. Resume and
  activation reject `latest`; activation requires the original source metadata
  and independently expected chain ID and never replays payload after validation.
  Direct convenience restore/fetch, SFTP, runtime feature overrides and admin
  exposure are unsupported. Local fixture checks do not qualify Linux, historical
  consensus parity, runtime audit remediation or 40 GB operation.

- A local `--preflight` startup diagnostic with text and `--preflight-json`
  reports, strict configuration type checks, filesystem/resource observations,
  temporary TCP bind probes, producer key metadata checks, and explicit
  unverified requirements. It exits before directory creation, database open,
  restore activation or service startup.

### Changed

- LR2 native host result counts use unaligned-safe four-byte copies at the
  original write sites instead of undefined typed stores. Existing in-bounds
  addresses, overlapping argument/result write order, return codes and VM
  metering are preserved in local regressions. No new memory policy, resource
  limit or setting; historical/Linux/integrated sanitizer qualification and
  packaged inclusion remain separate gates.
- LR2 adds optional `chain.vm-module-cache-max-capacity-bytes` for attributed
  parsed-module cache retention, plus cache-only native metrics. Default zero
  preserves unlimited bytes and the 32-entry ceiling. Oversized modules remain
  executable without retention; fresh instances and protocol metering are
  unchanged. The charge is not total heap/RSS; smaller budgets may cost more
  parsing CPU. Linux whole-node qualification and packaged integration remain
  separate gates.
- LR2 native JSON-RPC serializes and releases each batch result into the complete
  HTTP body rather than retaining every result tree until final serialization.
  Success envelopes are prepared before dispatch and results are moved, not
  copied. Complete response bytes/order and later batch calls on invalid UTF-8
  serialization are preserved; no partial output, retry or new setting. This
  reduces the measured synthetic working set, not an aggregate output limit or
  Linux whole-node qualification; packaged integration remains separate.
- LR2 native JSON-RPC releases completed request bodies after dispatch and
  completed response bodies after asynchronous writing, avoiding large-body
  retention during healthy idle keepalive. In-flight writes retain their owner
  through cancellation; complete responses, pipelining and admitted execution
  are preserved. Native body-capacity attribution is not an output/RSS limit or
  Linux whole-node qualification. No new setting or packaged integration.
- All native CMake entrypoints use the same pinned Koinos Fizzy revision with a
  narrow, build-owned correction: context-allocation failure returns null
  instead of terminating inside `noexcept`. Native parse/instance/context
  allocation failures propagate as local allocation errors, not accepted
  contract reversions. Existing dependency installs remain unchanged. Scoped
  allocation/cleanup tests do not establish whole-machine OOM recovery, Linux
  capacity or historical parity; no packaged integration is supplied.
- LR2 native VM development protects parsed modules if shared-owner allocation
  fails, stages disposable cache inserts before eviction and continues with an
  owned module if cache retention cannot allocate. Cold parsing is serialized
  per backend with a cache recheck; warm hits remain independent of that lock.
  Each execution still creates a fresh mutable instance, with unchanged metering
  and protocol limits. Distinct cold IDs can incur extra latency; parsed-byte,
  waiting-caller/instance bounds and Linux qualification remain open. The pinned
  Fizzy context-allocation termination was separately reproduced in V1-A and is
  corrected by the V1-B dependency change above; other VM resource gates remain.
- LR2 serializes each pending record once per admission and shares immutable
  ownership across eligible forks, clones, squash and in-memory LIB publication.
  Opt-in `mempool.pending-record-max-owners` and
  `mempool.pending-record-max-capacity-bytes` default to zero/unlimited and
  track live record reservations, including ancestors/caller-held values.
  Failure releases credits before fork publication; local overload is retryable
  (`pending_record_capacity`), not consensus invalidity or P2P misconduct.
  Other metadata, encoding/cleanup workspace and consumer memory remain outside
  these limits. No qualified Linux VPS defaults, history or durability changes.
- LR2 native mempool expiry/removal stages every fork-head change before
  publication and transfers prepared map/tombstone nodes without allocating
  during publication. Rejected preparation preserves records, indexes, nonces
  and resources. Cleanup explicitly checks protobuf decoding rather than
  continuing with a partial record after a stream allocation failure.
  No historical retention, normal removal, replay or durability policy changes;
  aggregate capacity, block/LIB allocation safety and Linux qualification remain
  open. Prepared tombstone buckets add temporary workspace and iteration cost.
- LR2 adds opt-in `chain.transaction-work-max-jobs` and
  `chain.transaction-work-max-wire-bytes` (each defaults to zero/unlimited)
  for standalone submissions before controller copying/execution, including
  dry runs and disabled-mempool calls. Reservations release after admission/
  notification or failure. Local overload is JSON-RPC `-32005` with retryable
  local data or gRPC `RESOURCE_EXHAUSTED`, not consensus invalidity or P2P
  misconduct; refused work is not cached, queued or retried automatically.
  Canonical replay/validation is unaffected. Input wire bytes are not heap,
  retained-fork or whole-node RAM; Linux profile qualification remains open.
- LR2 native development adds an on-demand C++ retained-map/delta inventory
  across indexed forks, with shared-owner deduplication and explicit tombstones
  and unmeasured backends. Empty-head expiry does not mean all ancestor memory
  is reclaimed. This is not an RPC/CLI, retained-heap/RSS estimate or global
  admission cap; no runtime limits, protocol/history or durability changed.
- LR2 native mempool admission prepares all fork writes/resets before publication
  and transfers preallocated map entries without second value copies. This fixes
  reproduced partial insertion after allocation failure; allocating map iterator
  wrappers now unwind instead of terminating. Per-fork nonce/resource behavior,
  state roots, history and durability are unchanged. Aggregate memory capacity,
  other lifecycle failure paths and Linux qualification remain open.
- LR2 native C++ state reads carry immutable value owners through cache eviction,
  replacement, iterator destruction and close, correcting a reproduced borrowed
  buffer lifetime fault. State map clones share immutable payloads and replace
  on write. Oversized values now safely bypass the separate state-object cache;
  retained payload stays within its existing 64-MiB budget. Metadata/caller-held
  values remain additional; this is not a process RAM cap or Linux qualification.
  Serialized formats, roots, archival retention and durability are unchanged.
- LR2 native development inserts accepted transactions into the enabled mempool
  before notifying observers. Ordinary insertion refusal propagates instead of
  being caught/logged as success; post-insertion notification errors are counted
  separately without retrying retained work. Pending metrics now count visible
  records without materializing payloads instead of always reporting zero.
  Global/fork memory bounds and Linux whole-node qualification remain pending.
- LR2 development retains ownership of disconnected P2P sync workers until
  joined, checks per-generation cancellation between RPC/application operations,
  and caps owned workers/current peer states at 64 internally. Capacity closure
  adds no peer-error score; stop drains all generations before transport teardown.
  This is not forced cancellation, a shutdown deadline or whole-node qualification.
- Diagnostic fixtures can explicitly select external build/evidence directories
  while retaining fresh-child, source-correspondence and path-confinement checks.
  Runtime backup trust, basedirs and restore policy are unchanged.
- LR2 development bounds disposable P2P accepted-ID retention by FIFO entries,
  owned string-capacity bytes and lazy expiry; LIB purges old block IDs and stop
  clears both caches. Exact current mempool lookups avoid redundant pending
  transaction execution/peer penalties after eviction or concurrent admission.
  Evicted unknown data still follows normal validation; no history, receipts,
  consensus checks or persistence are removed. This is not whole-node capacity
  qualification or a new low-resource deployment profile.
- LR2 development: JSON-RPC owns and drains asynchronous sessions instead of
  detaching per-connection threads. Complete HTTP reads/writes have a configurable
  30-second deadline (`jsonrpc.io-timeout-seconds`, 1–300); jobs/session capacity
  remains four by default, now capped at 64. Already-admitted service execution
  is drained, not interrupted or treated as rolled back on disconnect.
- Object-cache oversized/zero-budget inserts terminate while retaining one
  oversized value's ownership; the 64-MiB budget remains soft payload accounting,
  not a hard heap limit. Object/module warm LRU hits avoid list/key allocations;
  repeated module IDs no longer duplicate entries or evict unrelated modules.
- Offline storage reports expose shared-cache and memtable/reader memory estimates
  without counting the shared cache once per CF or treating missing estimates
  as zero. RocksDB defaults, full-history retention and durability are unchanged.
  These local fixes do not qualify mainnet/Linux or the 4-GB/40-GB target.
- Native backups now measure chain identity and durable-state checkpoint fields
  from the read-only RocksDB checkpoint and verify their genesis/block/receipt
  consistency. Native public promotion refuses missing/unverified identity;
  old backups are not retroactively repaired or authenticated.
- Local backup objects and publication markers use checked file/directory sync
  before publishing latest; corrupted reusable objects refuse publication.
  Direct metadata acquisition is deadline/cancellation bounded and reports safe
  source failure codes. Truncated legacy transfers no longer construct the
  Boost exception that crashed the local SC0 fixture.
- State reads/flushes report storage errors; failed commits retain root ownership
  and refuse session reuse. Explicit close reports failures without throwing from
  destructors. Block records/topology and initial state metadata are batched
  atomically; partial/corrupt metadata is refused rather than reinitialized.
  Cache clear resets accounting for safe close/reopen.
- POSIX node/maintenance startup and direct restore share a lifetime-held sibling
  storage gate. Startup refuses unfinished direct activation and mismatched
  direct observer-marker generations before opening the DB. Restored observers
  retain the producer hold across config-only restarts; legacy restore
  preservation remains unchanged, with a prior direct-restore receipt archived
  alongside the old database when a later legacy restore replaces it.

## [1.2.1] - 2026-10-06

### Fixed

- Increase the external `chain.invoke_system_call` result buffer from 64,000
  to 131,072 bytes, allowing retrieval of larger contract bytecode without
  changing protocol execution or compute limits.
- Complete a missing archived receipt state root when a validated accepted
  receipt matches the same block, identity, height and every other execution
  field. Require a 34-byte SHA-256 multihash; refuse conflicting data and
  preserve existing roots, block topology and archive WAL policy. This does
  not repair historical omissions or replace missing entire receipts.

### Build and validation

- Fetch the identical hash-pinned GMP 6.3.0 archive from the kernel.org GNU
  mirror after the GNU redirect and GMP project endpoints timed out in CI.
  Bound download waits and build GMP before Docker's long dependency build.
  Dependency versions remain unchanged.
- Add a canonical isolated observer test for large bytecode retrieval, buffer
  boundaries, finalized receipt completion and cold restart persistence.
- Give CI explicit owned test roots and make the backup missing-object test
  select an object referenced by the latest snapshot. No backup runtime behavior
  changes are included.
- Runtime changes are limited to the two fixes above on native release 1.2.0;
  unfinished LR2 work and unrelated development changes are excluded. See
  [qualification and operator notes](docs/validation/compatibility-1.2.1.md).

## [1.2.0] - 2026-08-29

### Added

- A dependency-free transaction TPS benchmark with bounded concurrent load,
  target-rate pacing, admission and inclusion throughput, latency percentiles,
  explicit chain-ID and confirmation gates, secret-free signed-workload input,
  owner-only JSON/Markdown reports, and a simulated RPC test suite.
- Trustworthy fast receipt replay matching `koinos-chain v1.5.2`: fresh
  writable-node Merkle roots, tombstone-preserving delta application,
  successor-header validation, at most one controlled full-execution fallback
  per invalid receipt, and the exact historical mainnet state-root exception.

### Changed

- Fast indexing now retains one pending block so every replayed historical
  block with an available successor is validated before finalization. Empty
  replay queues wait asynchronously without blocking or hot polling, all
  handlers use the configured chain executor, and oversized block-store
  responses use bounded nonblocking backpressure so catch-up also progresses
  with one chain worker. Startup now refuses to enter ready state if indexing
  is cancelled or fails before reaching the block-store head. During recovery
  indexing, only the block store and chain controller start; P2P, RPC,
  mempool, and block production remain stopped until the backlog is validated,
  preventing concurrent controller mutation during replay fallback.
- The state-delta audit now models receipt replay semantics for duplicate keys
  and inventories the resulting full-execution fallbacks separately from
  consensus exceptions and unexplained mismatches.
- Observer-first restore startup preserves the explicitly configured
  `verify-blocks` mode instead of claiming that forced full verification can
  repair state already finalized by an older binary. A persistent recovery
  hold now keeps block production disabled across restarts until a later
  explicit `--enable block_producer` activation.
- Container builds now propagate the requested parallel-job limit through
  nested dependency builds and embed an explicitly supplied hexadecimal
  `VCS_REF` in the native binary, keeping its version identity aligned with
  the OCI revision label even when the build context excludes `.git`.

## [1.1.0] - 2026-07-09

First standalone release after the split from
[koinos-one](https://github.com/koinos/koinos-one). The history of the node
sources was preserved during the extraction.

### Added

- Standalone repository layout: node C++ sources, CMake projects, tests, and
  tools at the repository root; native dependency build scripts under
  `scripts/`; node config templates and per-network example genesis data under
  `config/`; the operator CLI manual under `docs/`.
- Independent SemVer versioning through the root `VERSION` file, validated by
  all three CMake variants. Builds report
  `teleno_node <version>+<git-commit>[-dirty]` via `--version`, and
  `git_version.h` carries the `teleno-node-v<version>` release tag.
- Container image published as `ghcr.io/koinos/teleno` (previously
  `ghcr.io/pgarciagon/teleno-node` from the koinos-one repository), with
  version tags derived from release tags and a `beta` tag tracking `main`.
- CI: a native build workflow (Ubuntu build, CLI smoke checks, ctest with the
  C++ test suite enabled) alongside the container build/smoke/publish
  workflow.
- MIT license, README, and repository scaffolding.

### Changed

- `scripts/build-cpp-libp2p-koinos.sh` defaults are repository-root relative
  (`NODE_DIR` is the repo root, dependencies build into `.deps/`); everything
  remains overridable via environment variables.
- The Hunter build variant now defines `KOINOS_BUILD_TESTS` (default ON at
  configure level; the build script passes it through the
  `KOINOS_BUILD_TESTS` environment variable, default OFF) so CI can compile
  and run the C++ test suite.
- The Hunter source copy uses tar with `.git`/`.deps`/build-dir excludes so
  in-repo dependency builds cannot recurse into themselves, and works in
  minimal container build environments without rsync.
- GMP is propagated through the redirected `libsecp256k1::secp256k1` imported
  target so static link order stays correct for every consumer on Linux.

### Notes

- Koinos One consumes this repository as the `node/teleno-node` git submodule
  and stages the built binary into its packaged releases.
