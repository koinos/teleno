# Backup And Restore CLI

Native backup and restore are owned by `teleno_node`. The CLI can validate
backup config, create local snapshots, upload private SFTP backups, list
snapshots, fetch public bootstrap snapshots, restore, stage, activate, and
delete selected backups.

Implementation details live in
[`docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md`](https://github.com/koinos/koinos-one/blob/main/docs/current/backup-restore/NATIVE_BACKUP_CURRENT_IMPLEMENTATION.md)
and
[`docs/current/backup-restore/PUBLIC_BOOTSTRAP_RESTORE.md`](https://github.com/koinos/koinos-one/blob/main/docs/current/backup-restore/PUBLIC_BOOTSTRAP_RESTORE.md).

## Safety Rules

- Stop unmanaged node processes before direct CLI restore or restore
  activation.
- Use the local admin API for runtime backup operations only when it is
  intentionally enabled and token-protected.
- Keep backup admin endpoints on loopback.
- Never expose private SFTP credentials, SSH keys, password files, bearer
  tokens, wallet files, or producer private keys.
- Backups do not include producer private keys or wallet files.
- Restored nodes must start as observers first.
- Public bootstrap means public read-only source, not public admin control.

### Compression policy after restore

The optional [block Zstd settings](configuration.md#experimental-block-compression-policy)
control future table writes; they do not compact a selected backup or reduce
its transfer/staging budget. Existing tables retain their encoded codec and
dictionary and remain readable by Zstd-capable builds. Review the active
runtime config after restore: restored config is saved separately, and opening
the DB does not import its persisted writer options as node policy.
Development `--compact-db --all --compact-rewrite` explicitly rewrites existing
tables, including bottommost tables, with the selected policy. Run this on a
stopped, isolated copy with sufficient temporary space, then use
`--backup-create-local` to produce a native object repository with checkpoint
identity. Preserve the original backup and compare every selected record.
The [native rewrite checkpoint](validation/native-archive-rewrite.md) tracks
the actual export and restore evidence. A raw compact DB or its experiment
container is not a valid public snapshot; do not bypass the unchanged reserve
or fabricate checkpoint/publication metadata.

New native SSTs omit the RocksDB host-identity property. Older tables remain
readable and retain their existing properties until explicitly rewritten.
Public promotion still requires a reviewed observer config and the existing
checkpoint/genesis identity checks; omission of a host property does not prove
historical replay or authenticate a publisher.

The development direct HTTPS reader reserves 64 KiB inside its existing
128-KiB wire limit to reduce small writes and CPU overhead. All transfer,
source-trust and durability checks remain in force. See the
[paired transport measurements](validation/direct-restore-read-buffer.md);
whole-server restore and sustained operation require their own evidence.

## Validate Backup Config

`--backup-dry-run` validates config without opening RocksDB or connecting to
SSH:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-dry-run
```

Machine-readable output:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-dry-run \
  --backup-json
```

## Create A Local Backup

!!! warning "Local state and backup write"
    This command opens the node database and writes a native snapshot to the
    configured local repository. Run it with no other unmanaged process using
    the same basedir, or use the local admin API when the running node owns the
    backup service.

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-create-local \
  --backup-json
```

Create the configured backup and upload to private SFTP when
`backup.remote.enabled=true`:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-create \
  --backup-json
```

## Backup Identity And Diagnostics

### New Backup Identity And Publication (LR1 Development)

New native snapshots read `source.chain_id` from the **checkpoint** database and
compare it with the included genesis. `identity_status: checkpoint_verified`
means structural checkpoint identity was checked, not historical replay or
publisher authentication. Metadata includes `state_revision`, `state_id`,
`state_merkle_root`, `highest_stored_height` and `highest_stored_id`.
`head_height_kind: durable_state_revision` distinguishes this persisted state
from the live canonical head. `lib_height_known: false` means no live LIB was
measured; a legacy displayed zero is not evidence of LIB zero.

Empty diagnostic databases may have `identity_status: uninitialized`, without
an invented chain ID. Native public promotion refuses these and old snapshots
without checkpoint-verified identity. Recreate a backup using the corrected
writer; do not patch old manifests to claim that they passed. Deployment and
public publication remain separately authorized operations. The separate
Koinos One promotion script is not changed by this native correction.

Local publication checks writes, file sync, atomic rename and parent-directory
sync for objects, inventory, manifest, COMPLETE and latest. Reused objects are
rehash-checked and synced. Publication errors can leave a complete old or new
generation, but never imply successful completion; preserve the repository and
inspect it before retrying. A corrupt reusable object is not silently repaired.
POSIX behavior has local process-interruption tests; physical power loss and
Linux/ext4 remain unqualified. This durable writer fails closed on Windows,
whose durable-publication implementation is not provided in this milestone.

### Direct Source Failure Codes

Direct JSON failures include `failure_phase`, `failure_code`, `transport_code`
(zero when unavailable), and `cancelled`. Codes distinguish
`source_chain_identity_missing`, `source_chain_identity_conflict`,
`source_signature_verification_failed`, `source_http_status`,
`source_transport_failure`, `source_tls_trust_unavailable`,
`source_partial_message` and `source_metadata_invalid`. Raw exception messages
are omitted. None of these refusals starts services, opens the target DB, skips
identity/trust checks or releases the observer hold. Do not fix a refusal by
disabling authentication or deleting state. See the
[LR1 evidence](validation/low-resource-lr1-prerequisites.md).

## List Backups

Local repository:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-list \
  --backup-json
```

Private SFTP remote metadata:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-list-remote \
  --backup-json
```

Remote listing fetches only metadata into the local repository cache. It does
not download every object until selected fetch or restore.

## Public Bootstrap Restore

Public restore reads an HTTP(S) backup repository and writes into the local
native backup repository before staging and activation.

### Direct Metadata-Only Preflight (SC1)

The default mode is `repository`; existing local, private-SFTP and public restore
commands retain their current behavior. The opt-in `direct` mode supports
metadata planning plus explicit stage/resume and activation (SC4); the planning
command itself still transfers no payload and opens no DB:

```bash
./build/teleno_node \
  --basedir /path/to/new-observer \
  --config /path/to/reviewed-observer-config.yml \
  --backup-restore-mode direct \
  --backup-restore-preflight \
  --backup-public-url https://example.org/public-native-backups \
  --backup-metadata-dir /path/to/separate-metadata-scratch \
  --backup-expected-chain-id '<INDEPENDENTLY_EXPECTED_CHAIN_ID>' \
  --backup-id latest \
  --backup-json
```

Use a reviewed config's `backup.public-restore.network`, HTTPS and signature
settings. Signature verification uses the existing configured trusted public
key; preflight does not override `signature-required`. An unsigned source is
reported as unauthenticated, not silently treated as trusted. The optional
expected chain ID is an independent equality check against metadata, not proof
that the staged DB or genesis belongs to that chain. Without it,
`expected_chain_checked` is false. No public-node queries are needed for the
preflight; use an already independently established identity.

The command resolves `latest` once and returns an immutable specification of
the selected manifest/inventory, source identity, version, layout, network and
chain. It checks hashes authenticated by the configured signature, inventory
counts/totals, portable paths, repeated-object size consistency, entry limits,
and arithmetic overflow. Raw metadata files are limited to 8 MiB each, JSON
nesting to 32 and inventories to 100,000 files. Metadata acquisition makes at
most six successful file downloads (48 MiB logical bytes plus filesystem
overhead); configured retries are limited to ten and the timeout setting to
1–300 seconds. Synchronous transport deadline enforcement remains unqualified;
this is not yet a stalled-transfer or cancellation guarantee.

Only the explicitly selected scratch directory is written: a uniquely owned
temporary child is removed after the call; an empty scratch parent can remain.
Scratch must not overlap target or local source, and symlinked planning paths
are rejected. Credentials, query parameters and fragments in direct-source URLs
are unsupported. `file://` and loopback HTTP are available for isolated fixtures;
HTTP still requires `require-https: false`. HTTPS redirect policy is enforced
for the bounded metadata path.

The target is not created or opened. Existing DBs, legacy chain stores, recovery
holds, staging attempts and unreviewed files make it ineligible. A regular
`config.yml` is permitted and never overwritten. Existing genesis/descriptor
inputs must hash-match the selected inventory. Eligibility is an observation,
not an activation lock: concurrent-startup exclusion and rechecking belong to
the activation milestone.

JSON schema `teleno-direct-restore-preflight`, version 1, reports eligibility and
rejection, signature status, input hashes, logical and modeled allocated bytes,
capacity/available bytes, same-filesystem source/scratch observations, required
and recommended space, and explicit unverified gates. Existing usage is
`capacity - available`; this includes already allocated local source storage
and unavailable/reserved filesystem space instead of adding that source twice.
The model rounds each destination file to at least 64 KiB, budgets metadata and
directories, declares a 128 MiB offline-validation allowance, and retains the
`max(10 GiB, database bytes / 5)` operating reserve recommendation. The allowance
and allocation model are **not measured upper bounds**. A below-recommendation
result is reported, not converted into a 40 GB claim.

Exit codes: `0` means eligible with enough space for the declared planning
minimum; `2` means target ineligibility or insufficient space; `1` means argument,
metadata, authentication or I/O failure. Text output uses the same labeled
fields. Success still reports `ready_to_restore: false`, no payload downloaded,
no DB opened, no attempt initialized and no 40 GB qualification.

Use the [SC4 workflow](#direct-stage-resume-and-explicit-activation-sc4) for
stage-only, resume and explicit activation. Combined actions and convenience
public/configured restore/fetch commands with direct mode remain rejected before
basedir creation or legacy routing. Direct mode is not exposed by the admin API.
See [SC1 validation](validation/single-copy-restore-sc1-preflight.md).

### Internal Direct Staging Prototype (SC2)

SC2 implemented the native staging library and repository-local fixtures;
SC4 now connects that engine to the standalone CLI below. Do not run a diagnostic
fixture executable as a server-bootstrap procedure.

The prototype writes one object at a time through 64 KiB chunks into a single
owned partial file, enforces expected size while receiving, hashes incrementally,
syncs and renames into a private payload tree, and publishes small per-file
records. It creates no complete local object cache, uses no clone/hard-link
shortcut, and never creates the canonical target. Attempt directories must be
fresh, private, disjoint target siblings on the same filesystem. Existing or
unowned data is not adopted, overwritten or automatically cleaned up.

A bounded versioned descriptor pins input hashes, source, network/chain, format,
target/attempt paths and a generation ID shared by state and file records.
Explicit resume uses that selected backup ID, not
`latest`, with a newly validated specification matching the descriptor. Every
completed file is re-hashed; only missing/changed files and the single incomplete
partial are retransferred. No source replay is allowed after offline validation
has begun. A completion record is `download_complete`, **not** database validity
or permission to activate; legacy activation markers are not emitted.

Object transfers have finite retries and a deadline spanning each attempt,
including redirects, with cancellation polling during async network waits.
HTTP stalled/slow responses were tested locally. Metadata still uses the SC1
acquisition path; WAN DNS/TLS and blocked filesystem I/O timing remain unqualified.
TLS object transfer requires trusted certificates and hostname verification.
Failures retain the owned attempt for explicit resume, never publish the target,
and keep write/sync/rename/journal errors visible. The library uses POSIX file and
directory synchronization; physical power-loss and Linux filesystem qualification
are later gates, not consequences of a successful rename.

Staging preserves the recommended operating reserve, counts allocated per-file
journal records, and subtracts only re-hashed complete payload allocations when
resuming. Availability is checked at file transitions and at most every 8 MiB
during writes. Neither this estimator nor tiny synthetic fixtures establish
that a full node can launch or remain synchronized on a 4 GB RAM / 40 GB whole
server. See [SC2 evidence and limitations](validation/single-copy-restore-sc2-staging.md).

List public testnet metadata:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config config/testnet-public-bootstrap-observer.yml \
  --backup-public-list \
  --backup-public-url https://testnet.koinosfoundation.org/backups/testnet/teleno-bootstrap \
  --backup-json
```

Fetch metadata and missing objects without activation:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config config/testnet-public-bootstrap-observer.yml \
  --backup-public-fetch \
  --backup-public-url https://testnet.koinosfoundation.org/backups/testnet/teleno-bootstrap \
  --backup-id latest \
  --backup-json
```

!!! danger "High-risk restore activation"
    This command stages and activates a restored database under `$BASEDIR`. The
    previous DB/runtime paths are preserved under `.pre-restore/`, and the next
    node start must remain observer-first.

Restore public testnet bootstrap:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-public-restore \
  --backup-public-url https://testnet.koinosfoundation.org/backups/testnet/teleno-bootstrap \
  --backup-id latest \
  --backup-json
```

If the selected config path does not exist, public restore writes an
observer-safe config for the selected network. Review the generated config
before starting long-running operation.

### Internal Offline Validation And Activation (SC3)

SC3 adds `validate_direct_restore_attempt()` and `activate_direct_restore_attempt()`.
SC4's explicit standalone activation command calls these offline APIs; runtime
admin direct mode remains unsupported. Do not use a diagnostic executable to
restore operator data.

Validation rehashes the immutable inventory, requires an independently selected
chain ID in native `0x` multihash hex form, checks genesis and state chain identity,
opens all nine unified column families **read-only**, checks iterator errors and
SST checksums, and verifies persisted state/header/receipt/block ancestry
consistency. In-memory WAL recovery is supported; writable DB recovery is not.
The 32 MiB genesis, 128 MiB WAL-input and 64 MiB block-record guards are refusal
limits, not a measured full-node memory bound. Historical execution and
differential consensus parity are not inferred from these checks.

Once `validating` is recorded, recovery stays offline: it must never redownload
the source over that database. `verified` binds the validation report to the
same generation and manifest/inventory hashes. Activation requires a fresh
target and same-filesystem placement. Config is never overwritten or imported;
matching existing runtime inputs are retained. Genesis/descriptors and the
single DB use exclusive rename, with no recursive-copy or overwrite fallback.

On POSIX, ordinary node/maintenance startup and direct restore claim a common
nonblocking lifetime-held gate at the sibling path
`.<basedir-name>.teleno-storage.lock`. The basedir parent must permit access to
this owner-only sidecar. Stop the owning process before a separate CLI storage
operation. Do not unlink the lock file to defeat exclusion. `--version`, `--help`,
startup preflight and direct metadata-only preflight bypass storage startup.
The gate does not control unmanaged programs that ignore it; stop those too.

The sibling `.teleno-storage.lock.activation.json` intent is synchronized before
placing target markers or a canonical DB. `.backup-observer-recovery`,
`.backup-just-restored` and `.teleno-direct-restore.json` bind the target to the
verified generation before the DB rename. An interrupted activation leaves one
owned database and blocks ordinary startup until explicit offline recovery
finishes. Preserve the attempt, sidecar and DB; do not remove markers or clear
state to bypass the refusal. Use the same explicit activation command below for
interrupted recovery, with the original source/policy and pinned identity.

The first restored startup cannot release the observer hold, even with an
explicit producer-enable flag. Later config-only restarts retain it. Only the
existing later explicit producer activation policy may release it after the
operator's validation and authorization. Legacy `.pre-restore/` behavior is
unchanged; a later legacy restore archives the old direct-generation receipt
with its preserved DB rather than applying it to the replacement database.
See the [SC3 evidence and limitations](validation/single-copy-restore-sc3-activation.md).

### Direct Stage, Resume And Explicit Activation (SC4)

#### Experimental reserve profile

Development direct CLI actions accept
`--backup-restore-reserve-profile experimental-8gib`. The default `standard`
still requires `max(10 GiB, restored database bytes / 5)` of recovery headroom;
the explicit experiment requires `max(8 GiB, restored database bytes / 5)`.
Both estimators share the reserve calculation. Stage/resume and offline
validation/activation use the selected profile consistently. Include the same
option on **every** action of an experimental attempt: its descriptor pins the
profile and rejects a change on resume or activation. Old standard descriptors
retain their existing format and behavior.

The option is restricted to standalone direct mode. It does not change runtime
admin, repository restore defaults, or ordinary node configuration. Reports
identify the profile and `experimental_reserve`; CLI success does not qualify
40 GB capacity. A smaller allowance neither saves physical bytes nor bounds
compaction/WAL workspace. The [direct VPS experiment](implementation/vps-8gib-reserve-test-goal.md)
is active, not a qualified general recommendation. Original issue-9 reserve
criteria and producer activation gates remain unchanged.

!!! warning "Development feature, not a capacity or production qualification"
    This opt-in POSIX workflow has local signed-fixture coverage. Existing runtime
    storage/publication audit blockers, Linux/container and historical validation
    remain explicit gates. No 40 GB whole-server claim is proven. Keep an
    independent trusted backup. Never use a producer/live basedir as a test target.

The 40 GB target means the **whole server**, including OS/runtime, transfer
scratch, metadata and recovery reserve. The [2026-10-15 assessment](validation/restore-40gb-current-baseline.md)
now verifies the corrected public source identity but finds a modeled 4.140 GB deficit on the
designated VPS. Single-copy staging alone does not satisfy this requirement.

Select an existing parent directory, an absent or fresh observer target, a new
private attempt **sibling** of that target, and separate metadata scratch that
overlaps neither source, target nor attempt. Use a reviewed existing config
(outside the absent target if needed); direct mode does not generate or overwrite
configuration and never starts services. Stop all processes using this target.

For example, after reviewing the paths, configuration, expected network/chain
identity and source trust policy:

```bash
./build/teleno_node \
  --basedir /path/to/restore-parent/new-observer \
  --config /path/to/reviewed-observer-config.yml \
  --backup-restore-mode direct --backup-restore-stage \
  --backup-output /path/to/restore-parent/owned-attempt \
  --backup-metadata-dir /path/to/separate-metadata-scratch \
  --backup-public-url https://example.org/public-native-backups \
  --backup-expected-chain-id '<INDEPENDENTLY_EXPECTED_0x_MULTIHASH_CHAIN_ID>' \
  --backup-json
```

This resolves `latest` once when no concrete `--backup-id` is supplied, streams
inventory files into the private attempt, and returns `download_complete`, not
database validity. It creates no full object repository or canonical target DB.
`--backup-expected-chain-id` is mandatory for stage/activation and must be
independently selected, not copied from an untrusted manifest. Signatures and
HTTPS use the unchanged `backup.public-restore` policy; unauthenticated input is
reported as such and must not be promoted into a trusted-source claim.

If transfer stops, rerun the same stage command with
`--backup-restore-resume`. Omit `--backup-id` to read the attempt's pinned ID, or
supply that exact concrete ID. **Do not pass `latest` on resume or activation**;
it is rejected rather than selecting a different generation. Completed files are
rehashed; only incomplete/changed payload files are retransferred. A failed
attempt is preserved, not cleaned up or silently replaced.

Activation is a separate explicit local state change. After reviewing the stage
output, rerun the command with `--backup-restore-activate` instead of
`--backup-restore-stage` and without `--backup-restore-resume`:

```bash
./build/teleno_node \
  --basedir /path/to/restore-parent/new-observer \
  --config /path/to/reviewed-observer-config.yml \
  --backup-restore-mode direct --backup-restore-activate \
  --backup-output /path/to/restore-parent/owned-attempt \
  --backup-metadata-dir /path/to/separate-metadata-scratch \
  --backup-public-url https://example.org/public-native-backups \
  --backup-expected-chain-id '<INDEPENDENTLY_EXPECTED_0x_MULTIHASH_CHAIN_ID>' \
  --backup-json
```

Activation re-authenticates metadata for the pinned concrete ID, verifies the
generation/source/policy/digests, validates the complete staged DB read-only and
moves the sole DB by exclusive rename, with durable observer hold and intent.
The direct CLI keeps the target lease and attempt lock across validation and
activation. Only a fresh in-memory proof from that same locked operation can
avoid the duplicate validation before rename. Complete payload hashes and final
read-only validation after rename remain mandatory. Persisted verification
records alone cannot enable this reuse; interrupted operations validate afresh.
See the [scan-reuse validation status](validation/direct-restore-scan-reuse.md).
`validating`/`verified` recover via read-only validation; `activation_pending`
recovers the owned publication/rename, never source payload replay. After an
interruption, repeat **activation**, not transfer resume. Metadata must remain
available at the original source; metadata outage, drift or changed trust/HTTPS
policy fails closed and preserves the DB/intent. This first CLI version does not
cache an offline trust proof. DB validation itself has no networking or services.

Success reports `activated`, actual observer-hold status and the verification
report. Repeating a completed activation is idempotent receipt evidence, not a
fresh health or replay check of a database subsequently used by a node. Review
configuration, then start the restored node separately with an explicit
`--disable block_producer`; verify network, head progress and peers. The hold
remains across configuration-only restarts. No restore command releases it.

Exactly one direct action is accepted: preflight, stage or activation. Resume is
a modifier only for direct stage. Direct `--backup-restore`,
`--backup-public-restore`, fetch/SFTP commands, mixed actions, runtime feature
overrides (including `--enable block_producer`) and orphan resume are refused.
Repository is still the default, with its existing local/private/public/admin
behavior and `.pre-restore` preservation. Direct mode is standalone CLI only.

Text output is a labeled version of the same JSON fields. Successful preflight
retains `teleno-direct-restore-preflight` v1. Stage/activation and handled direct
errors use `teleno-direct-restore-cli` v1, including source trust, input hashes,
attempt state, declared limits/space policy, stage transfer counters, hold,
structural validation and explicit unverified gates. On a refusal,
`observer_hold: null` means its status was not established, never that a hold was
removed; `ready_to_start_observer` remains false. Allocated-byte samples are
post-command observations, not peak/full-server measurements. Busy/changed
filesystem during metric collection is reported as an unavailable sample rather
than changing a completed engine operation into a failed activation.

Exit codes: `0` completed the selected action; `2` preflight target/minimum-space
rejection; `1` refusal/input/configuration/I/O/verification failure; `130`
cooperative SIGINT/SIGTERM cancellation. JSON failures identify `failure_phase`
where available without copying arbitrary transport/config exception text into
logs. Cancellation is cooperative; blocked kernel/filesystem-I/O
timing remain unqualified. Preserve the journal/sole DB and follow phase-specific
recovery; do not remove gate/intent/hold files or spend the reserve to bypass a
refusal. See [SC4 evidence](validation/single-copy-restore-sc4-cli.md).

Local SC5 measurement tools compare the modes on owned synthetic fixtures, not
operator data. See [the tool contract](../tools/backup/README.md#sc5-local-capacity-experiment)
and [actual capacity blockers](validation/single-copy-restore-sc5-capacity.md).
Do not infer 40 GB whole-server capacity from a small fixture, a volume label,
or a successful restore. Keep the reserve and independent backup unchanged.

See the [SC6 review checklist](validation/single-copy-restore-sc6-handoff.md)
for the supported contract, remaining release gates and consumer handoff.
Documentation preparation is not qualification or a packaged feature announcement.

## Restore From Local Or Private SFTP Backup

Preflight local restore:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-restore-preflight \
  --backup-id latest \
  --backup-json
```

Fetch private SFTP remote snapshot into the local repository:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-restore-fetch \
  --backup-id latest \
  --backup-json
```

!!! danger "High-risk restore activation"
    `--backup-restore` fetches remote data when enabled, runs preflight, stages,
    activates, and exits. Run it only after confirming the basedir, network,
    selected backup ID, and available disk space.

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-restore \
  --backup-id latest \
  --backup-json
```

After restore, start as an observer:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --disable block_producer
```

The restore marker preserves the configured `chain.verify-blocks` value. With
`verify-blocks: false`, catch-up uses checked receipt replay with one-block
lookahead and at most one full re-execution for a receipt that cannot reproduce
the successor's signed root. With `verify-blocks: true`, every indexed block is
fully executed. Neither mode repairs a bad state node that was already
finalized inside the restored database.

The first startup consumes `.backup-just-restored` and creates a persistent
`.backup-observer-recovery` hold. While that hold exists, block production is
disabled even if the restored config enables it. After the observer is caught
up and every producer activation check passes, stop the node and explicitly
restart with `--enable block_producer` to release the hold. That command is a
high-risk production action; do not run it as part of restore automation.

## Delete Backups

Delete is dry-run by default:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-delete \
  --backup-id "$BACKUP_ID" \
  --backup-scope local
```

!!! danger "High-risk backup deletion"
    The confirmation value must exactly match the selected backup ID. The value
    `latest` is intentionally rejected for deletion.

Execute deletion:

```bash
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --backup-delete \
  --backup-id "$BACKUP_ID" \
  --backup-delete-confirm "$BACKUP_ID" \
  --backup-scope local
```

Use `--backup-scope remote` or `--backup-scope both` only after verifying
private SFTP settings and retention policy.

## Local Admin API

The backup admin API is optional, loopback-only, and bearer-token protected. It
is meant for local orchestration while the node is running. See
[RPC Endpoints](rpc-endpoints.md) for endpoint and exposure details.

## Restore Verification

After restore:

1. Confirm restore output reports success.
2. Confirm `.pre-restore/` contains preserved previous runtime paths when
   applicable.
3. Start with `--disable block_producer`.
4. Wait for `[node] teleno_node ready`.
5. Query `chain.get_head_info`.
6. Confirm head height advances.
7. Restart the observer and repeat the head and peer checks.
8. Only then consider the full producer activation gate, if this basedir is
   intended for production. The recovery hold remains active until an explicit
   `--enable block_producer` restart.

## Healthy Upgrade Versus Existing Corruption

A healthy node can upgrade the binary in place. Preserve its database, start
as an observer, and verify that it crosses the historical replay boundaries
and continues advancing. No state deletion or forced resync is part of the
upgrade.

If an older binary already finalized incorrect state and the node still halts
with a persistent state-Merkle mismatch after the upgrade, preserve the
database and diagnostics. Start the corrected binary as an observer and use
the documented recovery-first checks. If validation-based recovery cannot
continue, rebuild chain state from the retained block store or restore a
known-good backup. Moving or deleting state remains a separate operator action
requiring explicit approval.
