# Observer backup publisher identity update

Date: 2026-10-15. Status: **installed observer and new public backup verified**.
The required 40 GB whole-server restoration remains unqualified.

The user identified their miner's installed Teleno as the writer/publisher for
the configured seed bootstrap repository and directed updating it as needed.
This maintenance updates that observer only. Its configuration, data ownership,
protected loopback administration, daily schedule and explicit production disable
are preserved. Unrelated legacy services were used as read-only references.
Private access routes and exact deployment paths remain outside the repository.

## Cause and isolated correction

The old writer was `1.2.0+b8dab4c08f99`. Its selected public manifest had empty
chain identity. A preserved read-only view also proves that durable revision
40,223,698 has a matching archived block/header but a receipt with **zero root
bytes**. The strict checkpoint reader correctly refuses that source. Adding a
chain label to old metadata would conceal the second defect.

A separate managed branch, `codex/backup-publisher-identity`, starts at clean
`d3cfb94455da4a472f651eb4239abbc289e74225`; its original native runtime source
matches installed `b8dab4c08f99`. The candidate contains checkpoint identity
reading, manifest/publication propagation and minimal accepted-receipt root
completion. Completion requires every other block/receipt field to match and
never replaces an existing root. Old missing roots are not bulk-repaired.
Execution, protocol formats, history retention and producer roles are unchanged.
The mixed LR2 memory, VM, RPC, admission and transport work was not deployed.

## Artifact and pre-install checks

| Item | Verified identity or result |
| --- | --- |
| Native identity | `1.3.0-dev.0+d3cfb94455da4a472f651eb4239abbc289e74225-dirty` |
| Exported source archive SHA-256 | `4a3a4cf0e45899e218ebbacf9e157af194ea512a2eced115eb080cff3ea68109` |
| Native executable SHA-256 | `759bed551b4e6a6f13c019c37a5cc1af35c10f02dde9a99063426df75262f718` |
| Installed image ID | `sha256:37d7b274440b8ff7143be0d5ab60170e98004d1f1d3a64b504dc6931b946faa3` |
| Linux native build and regressions | All targets, version/help, **22/22 CTest**, 15.08 s |
| Container smoke | Exact previous runtime base; version/help/hash, read-only root, no network, original user/entrypoint |

The build reuses the installed generation's pinned dependency inputs; it is not
a fresh dependency rebuild or LR2 diagnostic qualification. Tests cover actual
checkpoint/genesis/state/block/receipt consistency, publication refusal and
identity propagation, and scoped failure-atomic accepted-receipt completion.
The dirty suffix and source archive identify this uncommitted maintenance patch;
the shared base commit alone is not an integration pin or a release.

## Installed observer and publication evidence

A task-owned service override selects the verified image while retaining the
original unit/drop-ins and image for rollback. Graceful stop preceded reopening
the same database. Installed executable/image hashes, unchanged configuration,
protected admin access and explicit producer disable all pass. Head and LIB
advance. At 19:38:02 UTC, head 40,237,205 / LIB 40,237,145 and the entire selected
block and receipt, including the state root, match the independent local legacy
implementation. An earlier newly accepted receipt at height 40,236,726 also
matches in full. The public RPC returning HTTP 403 is unavailable evidence.
These checks do not prove historical replay or sustained producer readiness.

A new local checkpoint was created after durable state advanced beyond the
pre-update head. Local creation and configured private upload/public promotion
both completed. The public selected ID is
`20261015T191906Z-ms-1792091946641-files-494`, available through the
[configured HTTPS repository](https://seed.koinosfoundation.org/backups/prodnet/teleno-bootstrap/latest.json).
It contains **497 files / 29,721,611,361 logical bytes**, including every selected
database/runtime entry. Checkpoint identity is `checkpoint_verified`, durable
state revision 40,236,770, highest stored height 40,236,830; full historical replay
and the exact LIB height are explicitly unverified/unknown in the manifest.

Public manifest SHA-256 is
`0bd864df4ea7a526f6096dd5d0f952e42062e8eb4a0d84742565c013f483fdd5`;
inventory SHA-256 is
`e1b83cb557e800554d89bb0f347f4c87795120ea7a93a9c2b4ba50f83d8f82bb`.
HTTPS metadata matches the independently SSH-authenticated writer's complete
inventory and checkpoint fields. Only `config.yml` differs, replaced by the
reviewed observer configuration, SHA-256
`5ab6d85df9276acbc9b1e25df11522d1dcdd8b33135561d2b7959425f8b9ff97`.
This is operator provenance for the pinned metadata, not an on-wire signature
or a complete public-payload rehash. Native preflight still honestly reports
`signature_required: false` / `signature_verified: false`; policy is unchanged.

## Preserved input, rollback and remaining gate

All 500 old local payload files (29,711,903,976 bytes), plus the complete prior
public generation (29,711,902,447 bytes), were hash-verified/preserved before
retention removed that published generation. Immutable objects use hard links;
mutable inspection metadata is copied. This preserves the original input on the
larger writer host; it is not an independent external disaster-recovery copy.
No old metadata or receipt was rewritten to fabricate eligibility.

Rollback is prepared, not exercised: verify and remove only the task-owned
image-selection override, reload systemd and restart the original observer with
the retained image and original service configuration. Preserve the database
and failed evidence; never clear state or force a fresh resync to hide failure.

Current native metadata preflight on Mac and the actual designated VPS now
passes identity/chain checks with the concrete ID and independent expected chain.
On that VPS it reports `below_recommended: true`; stage refuses before creating
a target or attempt, with no payload transfer. The unchanged model exceeds
available space by **4,140,425,216 bytes**. See the
[40 GB restoration assessment](../validation/restore-40gb-current-baseline.md).
Correct publication closes this writer prerequisite, not source signatures,
complete-history parity, full-size restore, whole-node memory or production.

Private evidence root:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/publisher/`.
No mainnet signing/registration/production activation, Git publication, app
packaging/submodule change or main-branch integration occurred.
