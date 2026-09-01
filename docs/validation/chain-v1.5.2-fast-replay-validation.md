# Chain v1.5.2 Fast Replay Validation

Status: Repository-scoped validation and corrected 1.2.0 observer canary complete

Validation date: 2026-08-31

Repository base commit: `787d7cf37e5d2134ebc72faf944381a6aa4462a3`

Prepared release identity: `1.2.0` / `teleno-node-v1.2.0`

Reference chain commit: `0ae99eced8b585c4145424e9c2a28f667796cc66`

Reference state DB commit: `3a1c904e61afbff59e167f50175519e68046e090`

This report records only validation that was actually executed. Checked boxes
are proven by the named evidence; unchecked boxes are release blockers or
external actions. Paths use placeholders so no host inventory is committed.
The exact release commit is resolved by dereferencing the annotated release
tag (`git rev-parse teleno-node-v1.2.0^{}`) after publication; the artifact
gate must be rerun from that clean source before the tag is created.

## Safety and source identity

Historical input is an archived unified Teleno RocksDB opened with
`rocksdb::DB::OpenForReadOnly`. No Teleno, Koinos, or producer process had the
source open during validation. All writable databases and logs are isolated
under the repository-local ignored build directory.

- Source head: 37,019,561.
- Source `CURRENT` SHA-256:
  `acf8d72267abbeb72d4bd4b421661b52163830dca7cffcfe0345f6aadaf62104`.
- Source `MANIFEST-005101` size: 68,017 bytes.
- Source `MANIFEST-005101` SHA-256:
  `154ac4e506ea91141b8a4c4ec793d02187ef1010df06c912c557f03b52b13ce2`.
- Pre-boundary chain-state checkpoint head: 30,504,141,
  `0x1220631e4951636e58753899595f661a9b9904c9aca4855e1fa167e540bf9c8a8447`.
- The checkpoint was copied from an archived legacy chain-state run; the source
  copy remained read-only. Its `CURRENT` and scratch-copy `CURRENT` hashes both
  equal
  `18dbaacf93de1d031e615ba01c60707c9bc06b97bdeb4b4e950d0bbdb93448e8`.

The checkpoint is valid input for Teleno's state DB format, but it is not by
itself a differential result. A clean run of the tagged reference chain at the
matching checkpoints remains required below.

## Build environment

- Host: macOS 26.5, Apple Silicon.
- CMake: 4.2.3.
- Ninja: 1.13.2.
- Compiler: Apple clang 16.0.0.
- Clean test-enabled build: `build`, configured from this repository with
  Ninja, `KOINOS_BUILD_TESTS=ON`, and all installed dependencies under the
  repository-local `.deps` prefix.
- Test-enabled auxiliary build: `build-chain-v152`, configured from this
  repository with Ninja and `KOINOS_BUILD_TESTS=ON`.

The auxiliary tree remains useful only for the isolated historical runs. It
uses an older borrowed dependency prefix and is not accepted as clean-build
proof. The repository-local rebuild replaced that evidence: its CMake cache
has `CMAKE_HOME_DIRECTORY` set to this repository, enables both tests and
cpp-libp2p, and contains no path to another Teleno, Koinos One, or Knodel
workspace. The exact required dependency command, complete build, CTest, and
CLI smoke all exited zero. The clean binary does not reproduce the auxiliary
tree's pre-main cpp-libp2p `SIGBUS`.

## Focused tests

Command:

```bash
cmake --build build-chain-v152 --target \
  koinos_state_db_pending_root_test \
  koinos_controller_delta_test \
  koinos_indexer_lookahead_test \
  koinos_historical_fast_replay_fixture_test \
  koinos_config_test \
  teleno_node \
  koinos_state_delta_replay_audit \
  koinos_historical_indexing_validator \
  --parallel 4

ctest --test-dir build-chain-v152 --output-on-failure \
  -R 'koinos_(state_db_pending_root|controller_delta|indexer_lookahead|historical_fast_replay_fixture|config)_test'
```

Result: 5/5 passed in 1.14 seconds after the complete four-block canonical
fixture set, nonblocking indexer scheduling fix, restore-marker durability,
and validator-context strengthening. Release builds normally define `NDEBUG`, so
all test executables are explicitly compiled with `-UNDEBUG` (or `/UNDEBUG` on
MSVC). The captured indexer compile command contains `-DNDEBUG -UNDEBUG`; the
assertions below were therefore executed, not compiled out. The indexer test
also completed 100 consecutive four-worker runs after the scheduling fix.

- [x] Fresh pending root and finalized-cache separation.
- [x] Normal absent removal, preserved tombstones, flush, commit, and reopen.
- [x] Normal replay, one-time fallback, failed fallback, and prior-head safety.
- [x] Exact historical triple and all single-field negative mutations.
- [x] One-block lookahead, batching, final target, failure, cancellation, and
  nonblocking single-threaded handler ordering.
- [x] Rejected attempts produce no block-store write or broadcast side effects.
- [x] Restore recovery hold persistence and explicit later release.

The complete auxiliary CTest suite then passed 20/20 in 24.78 seconds with the
same assertion-enabled Release configuration. Enabling those assertions
exposed one stale pre-existing expectation: a newly created unified RocksDB
layout was still asserted to be `legacy`. The test now matches the existing
storage implementation and operator behavior (`unified` for a new basedir;
`legacy` only when a legacy chain database is present).

The same five replay/storage targets were rerun from the clean `build` tree
and passed 5/5 in 2.27 seconds. The clean complete suite passed 20/20 in 33.13
seconds. After the performance guardrail exposed a hot empty-queue repost
loop, the indexer was bound to its supplied `io_context`, empty waits became
bounded asynchronous timer waits, and oversized responses gained nonblocking
bounded backpressure. The final clean suite passed 20/20 in 17.78 seconds; the
indexer executable also passed 100 consecutive runs including a 275-block,
single-worker case whose request batches exceed the 100-item block queue.
Ninja's emitted compile commands contain `-DNDEBUG -UNDEBUG` for the controller
replay test and `-DNDEBUG` without `-UNDEBUG` for `teleno_node`, so test
assertions remain active without changing production assertion behavior.
After adding the duplicate-key remove-then-put regression, the final clean
focused rerun passed 5/5 in 0.54 seconds; its captured log SHA-256 is
`202cb22251a0ba40a4c1643d62f763854b4cd810523f904f3c80603053ed0a2a`.

## Deterministic historical fixtures

Fixtures live in `tests/fixtures/chain-v1.5.2-fast-replay/manifest.json`.

| Height | Artifact | Integrity and result |
| --- | --- | --- |
| 30,504,202 | Serialized public block | 768 bytes, SHA-256 `b8fa5b5fbb356d97cdce683e2908ef362565d673b4d4dc283ab02101d507468c`; the canonical block ID is recomputed and its previous block/root link is verified. |
| 30,504,202 | Legacy incomplete receipt | 1,009 bytes, SHA-256 `c7013c114379dac717add9e5c350283715aa78ae389d8f113e8d44e0ab77d69c`; eight-entry root differs from the successor expectation. |
| 30,504,203 | Serialized public block | 768 bytes, SHA-256 `8c4268893525e09ae0d39e3fa8e1b8522169f4eec7eae3612dac0456d8d727fe`; its header links to 30,504,202 and contains the exact signed expectation that triggers one fallback. |
| 32,789,377 | Serialized public block | 683 bytes, SHA-256 `7ee7f986a89103fe9a753b9ae6fbe233b4d628883766b1e7e943b403c09999fb`; the canonical block ID and preceding normal root are recomputed and verified. |
| 32,789,377 | Honest receipt | 2,242 bytes, SHA-256 `2a4ad905226bcc119926c53e6521fed671cd10b1c54550ad90ee4c6e487a348a`; 12-entry root is the honest root, while removing the historical phantom tombstone reproduces the legacy signed root. |
| 32,789,378 | Serialized public block | 683 bytes, SHA-256 `8e4086a9d37cadad6964a96d0c1653225d99de54b6d72e3b600c7807f5df189a`; the canonical protobuf JSON is emitted directly from the read-only archive, its block ID is recomputed from the parsed header, and the signed previous-state root is the exact scar value. |

The honest receipt contains 12 entries: 10 puts and two removes. The fixture
test also exercises the real Teleno controller with that receipt and proves the
exact scar is accepted without fallback while retaining the honest root.

Every canonical block fixture is reproducible without manual hex/base64
transcription. Run the command separately for each recorded height and compare
the resulting byte count and SHA-256 with the manifest:

```bash
./build-chain-v152/src/koinos_historical_indexing_validator \
  --source-db <READ_ONLY_ARCHIVE_DB> \
  --dump-block-json <30504202|30504203|32789377|32789378>
```

The newly completed four-block integrity test passed in 0.39 seconds in the
assertion-enabled auxiliary build.

## State-delta audit

Command:

```bash
./build-chain-v152/src/koinos_state_delta_replay_audit \
  --scratch-state-dir build-chain-v152/validation/audit-scratch-v4 \
  --journal-dir build-chain-v152/validation/audit-scratch.delta-journal \
  --journal-only \
  --progress-every 5000000 \
  --json
```

The journal was built by the earlier source-read-only scan and its metadata
binds it to the archived source identity recorded above. The final command
exited zero after checking all 37,019,561 source heights and 287,709,633
receipt-delta entries. It classified 36,948,825 ordinary matches, three known
replayable tombstone blocks, one known incomplete-receipt fallback at
30,504,202, and one exact consensus scar at 32,789,377.

The final audit also corrected a blind spot in the first audit implementation:
70,730 receipts between heights 34,686,822 and 35,526,515 each contain one
duplicate key. Original execution removed and then put that key, leaving both
the tombstone and final value in the historical delta; serialization therefore
contains the same final put twice. Receipt replay collapses the two puts to one
pending entry, so the successor-root check correctly requests full
re-execution. These are replay fallbacks, not additional consensus exceptions.
The focused three-block audit around the first instance and the state-DB
remove-then-put regression test both pass.

The audit reported zero known-constant changes, zero additional legacy entry
drops, and zero unexplained mismatches. The final audited block is
`0x1220e09ff3256f395912fbde37c2c823dc8deb32dd2b45d37caacc80da2347683ab4`
with root
`0x122083a131c75f8a91a936715661ca0c027db3c89c138ca9f5549dafaab94a509bcc`.

The local result JSON SHA-256 is
`0281bdd8a9936e795bdda114606dec85a835e6d2e94775974fc14b1b071e1d7c`;
the 31,380,569-byte detailed log SHA-256 is
`18ffaebd72386268352215e40f3f58de437d828f2e74d8aac33245fd31f5721f`.
Both are retained under the ignored validation directory and contain no
producer material.

- [x] Full archive audit exits zero.
- [x] Exactly one known incomplete receipt is classified.
- [x] Exactly 70,730 duplicate-key receipt fallbacks are classified.
- [x] Exactly one consensus scar is classified.
- [x] No changed historical constant, additional entry-drop exception, or
  unexplained mismatch is reported.

## Production-path historical indexing

`koinos_historical_indexing_validator` runs the real Teleno controller and
indexer against the archived block store. The adapter serves only read RPCs;
any write or broadcast attempt throws. Only `<SCRATCH_STATE>` is writable.

Reproducible command shape:

```bash
./build-chain-v152/src/koinos_historical_indexing_validator \
  --source-db <READ_ONLY_ARCHIVE_DB> \
  --scratch-state-dir <SCRATCH_STATE> \
  --genesis config/example/mainnet/genesis_data.json \
  --target-height <TARGET> \
  --threads 8 \
  --json
```

### Boundary 30,504,202

| Mode | Start | Target | Fallbacks | Final block | Final root | Source writes / broadcasts |
| --- | ---: | ---: | ---: | --- | --- | ---: |
| Checked fast | 30,504,141 | 30,504,203 | 1, at 30,504,202 | `0x12208f0ab558a8d90c0abfd1075bc602b96658df29d088f075a396e7fcf21735cc5a` | `0x1220f73169eb40a9977cefbf0e64e95a07f12a1a4050bed0e765ed84e63d9606f088` | 0 / 0 |
| Full execution | 30,504,141 | 30,504,203 | 0 | same | same | 0 / 0 |

Both modes report chain ID
`0x1220592bf18654fd07fdf5d500cde3e8402ecf7f81fa5dde8f14527b08bba8805f48`.
Checked-fast duration was 0.288 seconds and full-execution duration was 0.648
seconds for this 62-block checkpoint range.

The same range was then executed with controller sources compiled directly
from the clean `koinos-chain` v1.5.2 checkout at
`0ae99eced8b585c4145424e9c2a28f667796cc66` and state DB sources compiled
directly from v1.2.1 at
`3a1c904e61afbff59e167f50175519e68046e090`. The reference harness replaces
only GarageMQ with the same read-only unified block-store adapter; it does not
modify either reference checkout. It drives the exact public controller APIs
with the release's one-block lookahead sequence. Reference checked-fast and
full execution both produced the same block ID, root, and chain ID above, with
one and zero fallbacks respectively. Their local result JSON SHA-256 values are
`0d9ca8cb69ad593d9a93ca20706fe9daa619703ef5bbea0d6e4bbfa6569e6593` and
`7f2b203e3c0f659dcf9ceffefd50d04000d033c7015774aad557310426ccb1b1`.

The reproducible reference build is:

```bash
./scripts/build-chain-v1.5.2-reference-validator.sh

./build/reference-chain-v1.5.2/runner-build/koinos_chain_v152_reference_validator \
  --source-db <READ_ONLY_ARCHIVE_DB> \
  --scratch-state-dir <REFERENCE_SCRATCH_STATE> \
  --genesis config/example/mainnet/genesis_data.json \
  --target-height <TARGET> \
  --json
```

Add `--verify-blocks` for the full-execution reference mode. The build script
checks both exact commit IDs and rejects dirty reference checkouts.

The two tagged-reference modes also agreed on these SHA-256 multihashes of
selected raw contract-read results:

| Read | Result hash |
| --- | --- |
| `koin.symbol` | `0x122015264ec0bf78e08b9fc5cce4eb414550d1d746a932ee92757e54242c4b625920` |
| `koin.decimals` | `0x12200b57459772db2f3f6986a135824545af8690c536a865454c3c664767dc2b73f0` |
| `vhp.symbol` | `0x1220c5f7c807e04c1a099b26ade90f4a610e7c2d734bce3d1ed6d0c39d634396479b` |
| `vhp.decimals` | `0x12200b57459772db2f3f6986a135824545af8690c536a865454c3c664767dc2b73f0` |
| `pob.metadata` | `0x12207e5524a89d69545017000dde06621880e872b9b3312e2c79aff07df9b1146fa7` |
| `pob.consensus_parameters` | `0x1220fee136ec00a020d0920b6c5a7b710859181d4dc884f6fd9464171ad241185914` |

Teleno's checked-fast and full modes returned the same six result hashes. A
second run reopened both scratch databases at the durable LIB checkpoint
30,504,143, replayed the range, and again produced the same final block, root,
chain ID, and contract reads. The Teleno restart-result JSON SHA-256 values are
`1cc42a37f3aeabaf0222dfd65451bee228198f13bc9438baa8d5c3e4fa68a37f`
and `9bef06fcad39e079c63fc8d56c8631fb45167a85b13ca7a9930e78d903389bca`.

An additional checked-fast restart matrix first indexed far enough for
30,504,202 to become irreversible, then reopened exactly at 30,504,202 and
30,504,203. Both restarts continued without another fallback, mismatch, write,
or broadcast. Its local JSON SHA-256 is
`3d79d82783d292ebb186bb1e29aa7018789941c917ba36f041a5413287670068`.

The final executor/backpressure implementation was rerun with one worker from
30,504,143 to 30,504,203. It again emitted exactly one fallback, produced the
same canonical block/root, and attempted no write or broadcast. The captured
log SHA-256 is
`2c2926cd611383a62afbc2c5705c78617ed121ec6cb2e74b1dc71303cedc8944`.

### Boundary 32,789,377

The long checked-fast run reopened at durable checkpoint 30,504,143 and
reached 32,789,379 in 15,142.3 seconds. It reported exactly one fallback—the
already documented 30,504,202 fallback—and no mismatch or side effect. Its
resulting block and root were:

- block: `0x1220170b1299234fba884a41a307a316d79501ebae8e75a9c696b537b82f2de05ded`;
- root: `0x12209ad9032cad3ee78fd78cdf11a6e67df5dcea18d2996f36547789a9330206c64a`;
- log SHA-256:
  `a7275c953bf952496c75c39960c0ac6841d6707b58640fcad0f8e041f2e7e7a4`.

The matching continuous full-execution run also started at durable checkpoint
30,504,143 and reached 32,789,379 after executing 2,285,236 blocks in
28,660.2 seconds. It produced the same block, root, chain ID, and six contract
reads, with zero fallback, source write, broadcast, warning, or unexplained
mismatch. Its captured log SHA-256 is
`1bc018e1baddfe7254ba1994fc848999b8b3536431118241f43099623b834a84`.
That process had loaded the auxiliary binary before the final indexer-timer
serialization change; the full-execution controller and consensus paths did
not change. The final scheduler is independently covered by the final CTest,
100 stress runs, and one-worker historical windows in both modes. A reopen of
the same full scratch with the final binary started at its durable LIB
32,789,319, replayed the last 60 blocks in 0.477 seconds, and reproduced the
same complete result with zero side effects. Its log SHA-256 is
`7c73d4e9edd758a12c9c6e220602897b635f1f4dced6856b8956776075b7578c`.

From exact durable checkpoint 32,789,376, Teleno checked-fast, Teleno full
execution, tagged-reference checked-fast, and tagged-reference full execution
all reached that same block/root and chain ID. All four modes also returned
the same six contract-read hashes, including `pob.metadata`
`0x12204ece83996d626a4c755965b9f266d7e5c3a3584bffd5a3d37c138d7bfbe48eaf`.
The four captured log SHA-256 values are, in that order:

- `141e230b19d43fe88230b5268a9624988eb8e5acd337e272d6d1c5aa6e03b5fd`;
- `0f1aa26cf4c53253919fa04c730214b6abd80c5464a58a782955fcc49312eef9`;
- `c1e8a8820569bef2f394039b6429559ca78cc32ca2e6c384d975d9ef5779863f`;
- `364c1ebdf9ea44b726a581a13427a10cf02eabaad922be48464a8b700e66401b`.

Checked-fast restarts at durable heads 32,789,377 and 32,789,378 both reached
32,789,379 with zero fallback, write, broadcast, or mismatch. Their logs hash
to `4dcf8bd9e6e39fa22c824049c4fd7927f3bcd013fda5e78d306a7c94833e4b7c`
and `f302f13c4e46af2b3e226ef078c983b55ad5728fb1499e56ca314e60f07d6dce`.
The final executor/backpressure implementation was also rerun from 32,789,376
with one worker and produced the same result; its log SHA-256 is
`50096142e92e34f6c9a259e232236a54bbf7261bf85efcf7ed377d7aa20c5081`.

### Archive head and durable reopen

The final continuous production-path checked replay opened the durable state
at 30,504,143 and applied 6,515,418 blocks through archived head 37,019,561.
It produced exactly 70,731 fallbacks: the single incomplete receipt at
30,504,202 plus all 70,730 duplicate-key receipts classified by the complete
audit. The last fallback was at 35,526,515, and the count remained unchanged
for the final 1,493,046 blocks. No failed re-execution or unexplained mismatch
was logged.

The resulting values exactly match the full archive audit:

- block:
  `0x1220e09ff3256f395912fbde37c2c823dc8deb32dd2b45d37caacc80da2347683ab4`;
- state-delta root:
  `0x122083a131c75f8a91a936715661ca0c027db3c89c138ca9f5549dafaab94a509bcc`;
- chain ID:
  `0x1220592bf18654fd07fdf5d500cde3e8402ecf7f81fa5dde8f14527b08bba8805f48`;
- source writes / broadcasts: `0 / 0`.

All six selected contract reads completed. Their hashes match the earlier
boundary checks except for `pob.metadata`, whose canonical archive-head value
is
`0x1220ea88c3ce300a2c836d293296b152da430cfb18a3ffd1332e218efc67db40be43`.
The validator elapsed time was 45,299.6 seconds, including deliberate process
suspension during diagnosis of the duplicate-key range, so it is not used as
performance evidence. The 24,957,792-byte log SHA-256 is
`305635bfac97b3a29ee4b9b1aa3751adfb51c1a33e8833912b2c341be549c70f`.

The same scratch was then reopened with the clean `build` binary and one
worker. It started from durable LIB 37,019,501, replayed 60 blocks in 0.0619
seconds, and reproduced the same block, root, chain ID, and six reads with zero
fallback, source write, or broadcast. Its 2,892-byte log SHA-256 is
`3b93562220eb88db683fc5eb4c4137a40a402825b5258f006e89eaa9b394288d`.
After both runs, the source `CURRENT` and `MANIFEST-005101` hashes still equal
the values recorded in the source-identity section.

### Performance guardrail

Both binaries replayed the same ordinary 50,000-block range, 32,789,380
through 32,839,379, from separate APFS copy-on-write clones of exact durable
head 32,789,379. The baseline is base commit `787d7cf37e5d` with only the
read-only validator target added; the final binary includes checked lookahead
and bounded response staging.

| Metric | Baseline unchecked replay | Final checked replay | Change |
| --- | ---: | ---: | ---: |
| Validator duration | 226.287 s | 224.961 s | -0.6% |
| Blocks/second | 220.96 | 222.26 | +0.6% |
| User + system CPU | 29.89 s | 28.62 s | -4.2% |
| Peak RSS | 207,863,808 B | 207,224,832 B | -0.3% |
| Block output operations | 0 | 0 | equal |
| Scratch allocation before / after | 147,272 / 146,448 KiB | 147,272 / 146,448 KiB | equal |

Both modes produced the same final block, root, chain ID, and six contract
reads. An initial benchmark caught a material CPU regression from immediate
empty-queue reposting; that result was rejected, the scheduler was corrected,
and the table records only the final implementation. The block lookahead still
retains exactly one pending block item. Queue memory remains bounded by the
fixed 100-item consumer queue plus at most one staged block-store response.
The accepted baseline and final log SHA-256 values are
`af60c36d6a9b020e56dcea88254e481ed7511a7db976f4369b1cc333b5eeb737`
and `969a1f66f36bb9bbd9c871652d84cd757ecc27489917e08d19bc5654ac901c58`.

- [x] Block 30,504,202 performs exactly one controlled fast-path fallback.
- [x] Checked-fast and full execution agree at 30,504,203.
- [x] Tagged-reference checked-fast and full execution agree at 30,504,203.
- [x] Selected Teleno and tagged-reference contract reads agree.
- [x] Restart before, at, and after the 30,504,202 boundary succeeds.
- [x] The source adapter observed no external side effects.
- [x] Checked-fast reaches 32,789,379 with no extra fallback.
- [x] Full execution reaches 32,789,379 and reopens from its durable LIB.
- [x] Restart before, at, and after both exception boundaries.
- [x] Matching tagged-reference checkpoints and selected contract reads.
- [x] Same-range CPU, peak RSS, storage-write, and blocks/second comparison.
- [x] Production-path checked replay reaches archived head 37,019,561 with
  exactly 70,731 classified fallbacks and zero external side effects.
- [x] The same archive-head scratch reopens from its durable LIB with the clean
  final binary and reproduces every recorded result.

## Clean build and release gates

### Authorized observer canary completed

The authorized Linux observer canary uses an isolated copy whose durable chain
state starts at height 30,504,141 and whose read-only block-store source ends at
37,019,561. The prepared copy contains no wallet or producer-key material,
explicitly disables block production, and is separate from every running node
basedir. Its prepared unified RocksDB `CURRENT` and manifest SHA-256 values are
`74c58b19daad9950fe24797c86f9db859ce597191d7d1afb9b2459e892530e9b`
and
`d57e49b56a791bfbf46b4b8cd153fd6290fb2b3997e35c2607b561bd91a0e086`.

The first valid checked-replay attempt with the prepared 1.2.0 candidate
reached the expected one-time fallback at 30,504,202, then exited with
`SIGSEGV` when P2P was already running concurrently with recovery indexing.
The same checkpoint and replay mode remained stable with P2P disabled for both
one and two chain workers. The captured crash, one-worker diagnostic, and
two-worker diagnostic logs have SHA-256 values
`7638ae0906a8f521595f130672ff52e3751be3ab52f2f1a8820a0afc717312e0`,
`2f6ad00bd8cba72f80ea50b296b9dcc104f9571d4c99a7ba9f39ed7fec8f362f`,
and
`2f24632bb2c68f09ff31a015c8c608fc1d75d8f61916ad4e2c1627c794689565`.

Startup now opens only the block store and chain controller before draining
the recovery backlog. P2P, RPC, mempool, and block production start only after
the indexer validates the block-store head. The staged-start unit coverage
proves deferred external startup, idempotent core startup, reverse-order stop,
unknown-component rejection, and full core shutdown when a later external
start fails. A clean repository-local build and the complete 20-test suite pass
with this correction.

The corrected amd64 canary image is
`sha256:ea39098c6e0491aea0230f8bbf1a906a3376e94d1bd4702deec3d3e084e2ef70`.
It reports `teleno_node 1.2.0+44a7e4cb2756`; the synthetic revision identifies
the exact canary source patch and is not a publishable release commit. The same
continuous corrected run crossed both historical boundaries with no
unexplained mismatch. It then completed checked replay through archived height
37,019,561: the indexer applied 6,515,420 blocks in 48,125.3 seconds and
reported exactly 70,731 re-execution fallbacks, consisting of block 30,504,202
plus the 70,730 previously classified duplicate-key receipts. P2P and JSON-RPC
remained stopped until the indexer finished, then started and reached
`teleno_node ready`. The archive-head canary log has SHA-256
`7ed12bac6c35a2e367ef46520cfbe88fccbc19499a17b0462d76743eb5f3f3d6`.

The canary reached the live canonical head on 2026-08-31. Before the final
restart, 12 samples taken ten seconds apart covered heights 38,973,189 through
38,973,222. Ten samples matched the independent native observer and legacy
microservice reference exactly on height, block ID, state root, and LIB. The
other two observed a reference one or two blocks behind during sequential
polling and matched on the next sample. Immediately before shutdown, all three
nodes matched at height 38,973,249 with block ID
`0x1220fe06298e2be3a13a7316bcd8123c2b7241fcc7890f48cf32c889cd98c7bc7e25`,
state root `EiDQ-kmPIUgZz0EUGIJa2jeEAAxwbCCWJvJ8ILbFHZDF5A==`, and LIB
38,973,189. The canary had five connected peers and no critical or producer
activity log entries.

An intermediate persistence restart was performed during live catch-up at
height 37,397,340 to keep the isolated canary within its disk safety margin.
SIGINT produced an orderly reverse service shutdown, `teleno_node shutdown
complete`, exit status zero, and no OOM or consensus error. On the same image,
configuration, and basedir, the next start opened durable chain state at
37,397,280, held every external service behind the recovery gate, replayed the
60-block block-store backlog in 0.0212131 seconds, and only then started P2P and
JSON-RPC and entered ready state. Block production and gRPC remained disabled.
The reopen also reduced obsolete RocksDB WAL retention from 237 files and
13,548,806,720 bytes after close to one file and 11,827,786 bytes after open,
recovering the canary's disk safety margin without deleting or replacing any
chain or state data. This intermediate result supplements the final
post-convergence restart below.

Live catch-up was also compared at the canary's exact historical height rather
than only against the moving reference heads. At height 37,437,221, the canary
block ID, previous block ID, timestamp, and state root signed by successor
37,437,222 matched both the independent native observer block store and the
legacy microservice block store. The compared block ID was
`0x1220115ffc3bdc050a2f02c281d125531efed689e2de81769155401393414fe694a4`,
and the state root was
`EiAkSmIgvdHufGQm_qZSgV3mo8OtEvN8vdJ62ae9wLBbPQ==`. This proves that the
continued P2P catch-up retained the canonical block and state commitment after
the archive handoff and persistence restart; the sustained-head evidence above
and below completes the live-follow gate.

The final clean stop used
`docker stop --signal=SIGINT --timeout=120 teleno-canary-fix-44a7e4cb2756`
at 18:06:19 Europe/Berlin. The node completed its ordered JSON-RPC, P2P, chain,
and block-store shutdown at 18:06:23, logged `teleno_node shutdown complete`,
and exited zero without OOM or error. It had advanced to height 38,973,266.
The stopped full log has SHA-256
`67585b96cb34ab1fd451aedd1a362481d09b1f9c9b7b63242d48076a14d07445`.

`docker start teleno-canary-fix-44a7e4cb2756` then reopened the same image,
arguments, and basedir. The block store and chain started first, the recovery
gate validated a 60-block durable backlog in 0.0177039 seconds, and only then
did P2P and JSON-RPC start and the node report ready. Reopen reduced the closed
database's 29 WAL files to one without deleting state. The post-restart log
slice through 18:11:35 Europe/Berlin has SHA-256
`d9e2d6905c680ef63a5ece1cb807cda76f7c33af97c7a0c943dd4672945ef52e`.
It contains no critical error, production-loop start, or produced-block entry.

Twelve more ten-second samples after restart covered heights 38,973,302
through 38,973,327. Eleven matched both references exactly on height, block
ID, state root, and LIB; one sampled the canary one block behind and matched on
the next sample. A final exact three-way comparison at 18:12:05
Europe/Berlin matched height 38,973,360, block ID
`0x122000b26e13ecf94c3e6b4a335f87eee30df1733100ce64215cf22aedbf9e8eee21`,
state root `EiDkBuGDDtJfW_OaF5WoS8itFDtkv2Eswok19P-E63S9hw==`, and LIB
38,973,300. Peer samples remained between three and five. Transient tip
differences caused by sequential polling, including a same-height reference
reorganization at 38,970,871, resolved on the next sample; both reference
block stores subsequently confirmed the same canonical block and successor.
No persistent or unexplained consensus mismatch was observed.

The observer-only guard was rechecked after the final restart: both the
container arguments and configuration explicitly disabled `block_producer`
and gRPC,
the isolated basedir contained no wallet or private-key material, and the
post-restart log contained zero producer-start or production-loop lines. The
pre-existing native observer service retained its original process and
activation time throughout the canary work; it was inspected read-only and was
not restarted or reconfigured.

- [x] `KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh` from a clean,
  repository-local configuration. The final post-audit build-script log
  SHA-256 is
  `d8b7aa6b0bbd0c3225fbda5a2e35207c1a81af80c2d2e5624708e18434303dbb`.
- [x] `cmake --build build --parallel`; final captured log SHA-256
  `6a2590f720c1936d1f1b36d6311cb16bd8405e6e9d1fb95efd8c5ab84166e47e`.
- [x] `ctest --test-dir build --output-on-failure`: 20/20 passed in 33.13
  seconds initially and 20/20 passed in 28.94 seconds after the container
  build-identity changes. The final executor/backpressure implementation then
  passed 20/20 in 17.78 seconds, and the final ready-state gate rerun passed
  20/20 in 19.79 seconds. The exact dependency command and unrestricted
  `cmake --build build --parallel` were then repeated on the final worktree;
  their final CTest passed 20/20 in 26.96 seconds.
  After the final timer-serialization change, the complete suite passed 20/20
  in 27.60 seconds; captured log SHA-256
  `1d524a0e803b892dff0f8ad082ab00c970ab0574457d3d93f73dc2ec86f3c8b2`.
  The final post-audit suite passed 20/20 in 22.78 seconds; captured log
  SHA-256
  `0d46d2692ff1f6bee6d459b2f4296b1835c065cb184a24b8d7bad1acdd1ff134`.
- [x] `./build/teleno_node --version`: reported
  `1.2.0-dev.0+787d7cf37e5d-dirty`.
- [x] `./build/teleno_node --help`. The final combined version/help smoke log
  SHA-256 is
  `4b490e54dc6b75ddc74e2fdf9a382416fe4a11c629038d4ae19d7d6048f06a31`.
- [x] Focused tests rerun from the clean tree: final result 5/5 passed in 0.54
  seconds after the duplicate-key regression was added.
- [x] The pinned reference validator was rebuilt from clean checkouts at the
  exact chain and state DB commits recorded at the top of this report.
- [x] Linux/arm64 container build and isolated observer smoke. The final image
  was `sha256:201d21f720d1b41ad3cc079411ca10dd967c6709e4bb466b49c12d0ea29b632f`;
  `--version` reported `1.2.0-dev.0+787d7cf37e5d`, matching the first 12
  characters of OCI revision
  `787d7cf37e5d2134ebc72faf944381a6aa4462a3`. `--help` and the guarded
  producer-helper refusal passed. An offline observer using tmpfs, the public
  Harbinger genesis file, `--network none`, and block producer, P2P, and gRPC
  disabled and `chain_jobs=1` reached `teleno_node ready`, then stopped
  cleanly on SIGINT with exit status zero. The final build-log and observer-log
  SHA-256 values are
  `ea8650fa592aee1ea732cdc54955164dfd49f16d1096267112ab2c8ed783a0a5`
  and `523cde00048cd90b28eeda9f11ed4ecb4446d658b80e4d116c893220f679df9e`.
- [x] Native isolated observer with `chain_jobs=1` reached ready state and
  stopped cleanly; log SHA-256
  `969a3068e5f7d9701f112b39eff179d1d57bd37d2d46a2e53e3f9be2c2cdb3c5`.
- [x] Archive-head checked replay and same-scratch durable reopen.

The required repository-local sequence was repeated on the final corrected
worktree on 2026-08-31. Before execution, both `CMAKE_HOME_DIRECTORY` and
`CMAKE_CACHEFILE_DIR` resolved under the active Teleno repository and contained
no reference to another workspace. The exact
`KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh` command exited zero,
`cmake --build build --parallel` exited zero, and
`ctest --test-dir build --output-on-failure` passed 20/20 in 18.83 seconds. A
focused rerun including configuration, staged service startup, controller,
indexer, historical fixture, and state DB pending-root tests passed 6/6 in 0.28
seconds. `./build/teleno_node --version` reported
`1.2.0+daa919b257bf-dirty`, `./build/teleno_node --help` exited zero, and
`git diff --check` passed. The `-dirty` identity is expected for the reviewed
correction and documentation worktree and is evidence only; it is not a
publishable release artifact.

- [x] Full-range observer reached and followed current canonical head, with
  sustained pre- and post-restart comparisons against both references.
- [x] Observer canary and post-restart health checks passed on the same
  isolated basedir with block production disabled.

## Current conclusion

Every repository-scoped Definition of Done item is proven: the complete audit,
checked archive-head replay, durable reopen, historical boundary modes,
tagged-reference checkpoints, clean build/tests, CLI checks, and isolated
container/native observer smokes all pass with no unexplained mismatch. This
report does not claim that the tagged reference implementation was replayed
through all 37,019,561 blocks; differential reference execution was performed
at the required historical checkpoints, while Teleno's full archive was
covered by the complete receipt audit and production-path checked replay.

The authorized WP7 observer canary also passed full-range live-head follow and
the post-convergence clean restart gate. Release publication, consumer
handoff, and producer rollout remain separate actions and have not been
executed by this validation run. The canary image's synthetic revision is
validation evidence only; publication still requires a clean exact release
commit and a freshly reproduced artifact identity.
