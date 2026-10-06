# Compatibility 1.2.1 candidate completion checkpoint

6 October 2026. **Final publication withheld; an unpublished draft is retained.**
This checkpoint records completed authorized work and the remaining release gates.

## Exact identity

- Repository: `https://github.com/koinos/teleno.git`.
- Branch: `codex/release-1.2.1-compatibility`.
- Frozen candidate source/build commit: `10d84cf16d4b54f5c559744028f25e6543fddaa6`.
- Base: `b8dab4c08f99ff1ba951e4bd229a154572b8e4ee` (native 1.2.0).
- Candidate `VERSION`: `1.2.1-rc.1`; changelog remains unreleased.
- Exact native identity: `teleno_node 1.2.1-rc.1+10d84cf16d4b54f5c559744028f25e6543fddaa6`.
- Proposed tag: `teleno-node-v1.2.1-rc.1`. No Git tag has been created or pushed.
- GitHub draft ID: `404574883`; draft/prerelease true; target is the frozen source commit.
- Draft URL: [unpublished compatibility candidate](https://github.com/koinos/teleno/releases/tag/untagged-dfcb342afef07ac0cd05).
  Viewing and downloading an unpublished draft requires repository access.
- Latest published release remains native 1.2.0; no new GHCR image is published.

This documentation checkpoint is recorded after the frozen candidate build;
it does not relabel the binary as a later documentation commit.

## Scope and review

The only production changes are the external result buffer (actual base value
64,000, new value 131,072) and exact missing receipt-root completion. The minimal
block-store SHA-256 matches the supplied `53deada5889918566bae8311e72f1ddc1f5158f9563422e8715a2adc614817d6`.
Existing roots, serialized block/execution data, topology and archive WAL policy
are preserved; malformed/conflicting completion is refused. Entire missing
receipts and historical omissions are not repaired. The canonical regression
links the production store and uses a test-only Put failure wrapper.

The source archive contains the exact frozen Git source, not the unrelated LR2
tree. All 248 exported candidate files matched their manifest before and after
building. Added source and public artifacts were checked for secrets, identifying
private infrastructure and generated databases/caches. No temporary CMake overlay
or generated CTest edits are packaged. Artifacts are unsigned.

## Completed evidence

- Fresh native Ubuntu 24.04/x86_64 build from the frozen source completed at
  `2026-10-06T10:15:13Z`, exit 0, OOM false. All native targets built.
- Copied dependency caches and a fresh owned source/build directory were used;
  original cache, outputs and finished build container remain preserved.
- Build limits: two CPUs, 5 GiB RAM, no container swap; network disabled.
- The first configure attempt omitted static-zstd paths and failed before
  compilation. Its log is retained privately; corrected configuration passed
  without any candidate source change. This failure is not counted as a pass.
- Eleven CTests executed and passed, zero executed failures, 1.70 seconds total:
  `koinos_block_receipt_completion_test`, `koinos_config_test`,
  `koinos_service_registry_test`, `koinos_rocksdb_manager_test`,
  `koinos_state_db_durability_test`, `koinos_state_db_pending_root_test`,
  `koinos_backup_plan_test`, `koinos_backup_checkpoint_test`,
  `koinos_backup_snapshot_test`, `koinos_backup_sftp_test`,
  `koinos_gorpc_codec_test`.
- The canonical receipt test passed accepted notification after topology advance,
  cold reopen, late/duplicate preservation, failed write/retry, fourteen conflict
  refusals and absent-receipt preservation. Earlier hash-verified native evidence
  retains the expected original missing-root failure and corrected component pass.
- Ten scenario tests were excluded by an explicit CTest include, not disabled
  or edited in generated files, and were **not run**: controller delta, indexer
  lookahead, historical replay fixture, public restore, backup service, backup
  admin, peer sync, producer, mempool and gRPC. No additional chain/fork/laboratory
  deployment or key generation was executed.
- Exact version, help, libraries and producer guard passed in an isolated
  read-only, network-disabled released runtime container with no chain/key mounts.
- A staged candidate container was built from the exact ELF and digest-pinned
  1.2.0 runtime. Its version/help, ELF hash, libraries and producer guard passed
  with networking disabled and a read-only root filesystem, without chain/key
  mounts. Container archive
  inspection found the exact candidate ELF in its final layer.
- Original provisional ELF still has SHA-256
  `430f6940d7e000577155ac6bcfc74637e4768cfdb32799c37fb9f5529ab85416`;
  it was not relabeled. All 14 original service IDs/PIDs/start times were preserved.
- Koinos One's manual has a source-identity/candidate-boundary handoff; strict
  MkDocs build passed. Those documentation changes remain uncommitted alongside
  its prior changes. No submodule, app binary, GUI, package or app release changed.
- The original development tree remains outside the release branch. A concurrent
  user-requested switch to `codex/lr2-memory-hardening` and its project-memory
  documentation were observed and preserved; this task did not edit those files.
- Six draft assets were uploaded; GitHub's recorded sizes and SHA-256 digests
  match the local assets. Download verification is recorded in the private run receipt.

## Artifacts

The Linux ELF is 41,257,384 bytes, SHA-256:
`4e85f613a3bc1abb9e650c4fa4efd12204ad47115cc6bba6a9d4705cceb6a826`.
The container archive is a Docker-save archive with local tag
`teleno-compatibility-candidate:1.2.1-rc.1`; it stages the exact ELF over runtime
`sha256:da2a2437ca7d8b9c89bba0d5e6471468b7cb780b152819b9314bfad8954b5d72`.
This does not qualify a fresh full build through the standard Dockerfile.

```text
061f914fe7c7464229b1e8083984244e7c2ec7ef9dece44fda4cab4ae08115a8  SHA256SUMS
f2f6931b4a3365f20124d38a13937e41ea7b3ea9907f4cfa4e85bcb3ed1abbb0  teleno-node-1.2.1-rc.1-container-amd64.tar.gz
526bf4a4e34d70ce1736c1d5bc873bd42846bd53cef976f11ce350e2089e83c1  teleno-node-1.2.1-rc.1-linux-amd64.tar.gz
274604b9738b4b309c6b304d4ba96bd4a77c498c3a4989c32c0f697e71588457  teleno-node-1.2.1-rc.1-qualification.tar.gz
ed914735cd2e1793862f024d8419a20f8b207bf183f123c72d48a4b98cfbb077  teleno-node-1.2.1-rc.1-source.tar.gz
4f1bac6a451c9a0217a8683eb8024b1bd8a7fe303bea4e9cf6174c73554db0fb  teleno-node-build-info.json
```

`SHA256SUMS` lists the five payload assets; its own hash is recorded above.
The build-info JSON records the native source, binary hash, staged image identity
and qualification limits. The qualification archive includes the exact build
command, source manifest, permitted test logs, CLI/container evidence and the
runtime staging recipe, with private infrastructure details excluded.

## Remaining gates and maintenance boundary

Full scenario CTest, whole-node finalized-receipt readback plus observer restart,
actual selected large-contract bytecode readback and a fresh standard Dockerfile
build remain unqualified. The restricted component/runtime evidence cannot be
reported as passing those gates. The current task forbids additional simulated
chains, forks and laboratory deployments and does not authorize installing the
candidate on a live node. Retain the candidate rather than publish a falsely
qualified final release. No public tag, final release, producer action, transaction,
database reset/pruning/repair or seed-server change occurred. No PR was created,
because its automatic CI would execute scenarios outside this task's boundary.

A separately authorized final release must close the applicable gates, set the
final version/date, rebuild from its exact release commit, verify assets/checksums
and container provenance, then publish the native tag/release. Follow
[operator and qualification notes](compatibility-1.2.1.md) for reviewed observer
maintenance and rollback; any historical repair or producer action remains separate.
