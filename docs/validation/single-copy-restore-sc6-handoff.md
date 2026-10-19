# Single-copy restore: SC6 review and release-readiness handoff

Date: 2026-10-03. Status: **documentation package prepared; release readiness
blocked. SC4/SC5 and the feature Definition of Done remain unproven.**

This is the independent documentation part of
[SC6](../implementation/single-copy-backup-restore-plan.md#sc6--documentation-and-release-readiness-handoff),
not permission to bypass its preceding validation gates. The intended result
remains backup-based observer launch on a 2-vCPU, 4-GB-RAM, 40-GB Linux server
without a second full payload. That result has not been demonstrated.

Current follow-up (2026-10-15): the user reaffirmed this complete-backup,
whole-server requirement and designated an accessible replacement VPS for
isolated qualification. [Fresh source/capacity evidence](restore-40gb-current-baseline.md)
now verifies the corrected public backup's checkpoint identity. Native VPS
preflight still reports a 4.140 GB deficit at unchanged reserve; stage refuses
before payload/target creation. This is not a complete restore or qualification. Earlier
scope and validation rows below describe their dated checkpoint; they do not
supersede the active [issue 9 contract](../implementation/issue-9-goal.md).

## Scope and source identity

The 2026-10-03 "Next step" request is used for local documentation/handoff and
bounded existing test checks. No runtime code, default, format, reserve,
retention, version, producer setting or signature policy changes here.
No live host, external source, deployment, data cleanup, commit, push, tag,
release, GUI implementation, packaging or submodule update is performed.

Source HEAD is cfd2eb83b8080ca49911555108ce05f0a9068f01 on
codex/reward-reinvestment-macos, **plus pre-existing uncommitted SC0–SC5 work**
and this handoff. VERSION remains 1.3.0-dev.0. The checked existing binary is
teleno_node 1.3.0-dev.0+cfd2eb83b808-dirty. Neither that HEAD nor a binary hash
captures all the uncommitted feature source. There is no complete committed
feature revision to pin, package or release. A future authorized integration
must produce and verify its own clean source/build identity.

## Operator contract being handed off

- Repository restore remains the default; legacy local/private-SFTP/public/admin
  behavior and preservation under .pre-restore remain intact at the recorded
  local-test evidence level, not an all-platform parity claim.
- Direct mode is opt-in, standalone CLI only. Exactly one action is supported:
  metadata preflight, stage, or explicit validation/activation. Resume modifies
  stage only. Combined convenience restore, SFTP and direct runtime admin are
  unsupported. The desktop GUI does not expose this workflow.
- Use a fresh target and an owned sibling attempt on the same filesystem.
  Existing data is not overwritten. Config is reviewed separately and is never
  imported or overwritten by direct restore. There is no cross-device copy
  fallback and no automatic deletion of backups or failed attempts.
- Authenticate the selected source with independently trusted public material.
  Pin concrete backup ID, manifest/inventory digests and independently expected
  chain identity. Preserve HTTPS/signature policy. Checksums alone are not trust.
- Keep an independent backup. Direct mode materializes a private stage, then
  moves its sole DB into the target; it does not run inside a writable backup.
- Stage completion means download_complete, not DB validity. Once validation
  begins, recover with explicit activation, never redownload over the DB.
  Metadata must still be available from the original source; this version has
  no persistent offline trust-proof cache. Source outage/drift fails closed.
- Preserve the journal, activation intent, common storage gate and observer
  markers. Stop users of the target; do not unlink gates or clear state to
  bypass refusal. Restore never starts services or releases the producer hold.
- Start separately as an observer after reviewed activation. Structural DB
  consistency is not historical consensus replay, advancing head, sustained
  synchronization or producer readiness.
- Keep the operating reserve and declared limits unchanged. A 40 GB data volume
  excludes OS/build/source storage; a 40 GB whole-machine claim must include OS,
  installed runtime, logs, metadata, memory/swap and growth. Neither is qualified.

The authoritative action flags, schemas, exit codes, guards and recovery recipe
are in [the CLI guide](../backup-restore-cli.md#direct-stage-resume-and-explicit-activation-sc4).
Do not use the SC5 synthetic fixture helper with operator backups.

## Evidence and blocking gates

| Gate | Evidence available | Disposition before qualification/release |
| --- | --- | --- |
| Local signed CLI/engine workflows | [SC3](single-copy-restore-sc3-activation.md), [SC4](single-copy-restore-sc4-cli.md) | Local fixture coverage only; retain platform/workload limits |
| Synthetic single-copy allocation benefit | [SC5](single-copy-restore-sc5-capacity.md): five pairs, 34,336,768 vs 68,562,944 allocated bytes; identical input/genesis | Not a mainnet sizing, speed or memory-improvement claim |
| Reserve refusal | SC5 corrected 1 GiB APFS image rejects staging | Correct rejection, not successful capacity qualification |
| Representative backup and successor replay | No selected trusted full-size input/range/reference outcomes | BLOCKED; specify input/public trust and execute comparisons |
| 40 GB data volume | SC5 blocked before image creation | BLOCKED; host backing plus reserve and source/workload space required |
| Linux/ext4 and 2-vCPU/4-GB/40-GB whole machine | Not instantiated or tested | BLOCKED; disposable environment/build and actual whole-machine measurement required |
| Sustained operation | No representative 24-hour workload/WAL/compaction/growth evidence | UNVERIFIED; actual events and bounds must be recorded |
| Full CTest | SC5 standard/fresh native suites 29/30 | FAILED gate: existing SC0 truncated-transfer SIGBUS; do not waive |
| Clean dependency workflow | SC4 SDK-path-with-space bootstrap failure; SC5 reused dependencies | BLOCKED; fix/verify supported build path, then rerun canonical workflow |
| Container smoke | Prior daemon unavailable; no SC5 Linux container test | UNVERIFIED; recheck and run disposable smoke when available |
| Storage/publication integrity | Relevant TQ-01/TQ-02/TQ-10, WP1/WP6 gates remain | BLOCKED for safe operational qualification; scoped remediation/proof still needed |
| Historical/differential replay | No representative current-checkout range validation | UNVERIFIED; synthetic reopen is not parity |
| Koinos One consumer | Documentation-only handoff | No new feature pin/package/GUI; authorized integration checks required |

SC5's Mac sparse-image backing guard is 50,737,418,240 bytes for a 40-billion-byte
image plus 10 GiB host reserve, before separate source/additional writes. This
is an experiment-host prerequisite, **not** a revised node disk minimum. Current
host disk observations are in the SC6 evidence manifest; do not reuse yesterday's
free-space reading or remove retained data to produce a pass.

## Reviewable next execution sequence (not executed here)

1. Resolve prerequisites: selected snapshot/height/network/genesis/public trust,
   deterministic successor range/reference outputs, safe scratch capacity and
   disposable Linux/ext4/whole-machine environment. Review every path, source,
   config and process owner before any writes. No private signer is needed.
2. Investigate the existing SC0 diagnostic failure and bootstrap blocker with
   scoped regression evidence. Resolve relevant storage/publication contracts;
   do not infer authorization for unrelated audit remediation or live changes.
3. On the intended reviewed source, run the canonical repository-local workflow:

   ```bash
   KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh
   cmake --build build --parallel
   ctest --test-dir build --output-on-failure
   ./build/teleno_node --version
   ./build/teleno_node --help
   ```

   Verify source path/test discovery and actual exit status; version/help do not
   make a failed build/test workflow pass. Repeat relevant focused/sanitizer
   checks and isolated Linux/container checks with disabled production.
4. Execute [SC5's qualification matrix](../implementation/single-copy-backup-restore-plan.md#sc5--mac-capacity-experiment-and-linux-portability-qualification):
   same-input legacy/direct comparison, at least two full-size restores when
   resources permit, deterministic replay comparison, 24-hour representative
   observer work, two clean restarts/interruption, actual WAL/compaction/growth,
   Mac and Linux profiles. Keep failed/blocked results, source data and reserves.
5. Review claims independently against recorded ranges and resources. A capacity
   rejection is a valid outcome, but it cannot support an affirmative 40 GB claim.
   Do not publish until required feature/release gates are proven.
6. Only after explicit commit/release authorization, prepare a reviewed source
   commit, clean native build/hash/build-info, matching VERSION/changelog and
   intended artifacts following [native release procedures](../release-builds.md).
   Tag/push/image publication/deployment remain separate unexecuted actions.
7. Only after explicitly scoped app integration, deliberately pin that validated
   native commit in Koinos One, stage/rebuild its binary and run the app's own
   package/identity checks. Documentation coordination alone grants none of it.

Rollback/recovery: keep repository mode available on a separately budgeted fresh
target; preserve the original independent backup and sole direct stage/target.
Never overwrite/delete chain data or remove observer markers as rollback. Any
cleanup of retained owned evidence needs explicit review/authorization.

## SC6 progress and verification record

Changed: this new checklist, the implementation plan, native release guide,
backup/restore CLI guide, docs index and docs/memoria.md; plus two Koinos One
documentation pages (current native implementation and backup/restore manual).
No CHANGELOG/VERSION update is needed: this package changes documentation only.
AGENTS/CLAUDE, README, source/tests and the throughput plan remain unchanged
from this turn's baseline; all pre-existing work is preserved.

Ignored evidence: build-single-copy-evidence/sc6-20261003-tT6vJX/. It contains
starting worktree patches/status/hashes, disk reading, binary version/help,
focused-test log, frontend documentation build and preservation checks.
Actual checks in this documentation task:

| Command/check | Result | Evidence log |
| --- | --- | --- |
| CMake cache source/tests | This checkout, Release, KOINOS_BUILD_TESTS=ON | Existing build/CMakeCache.txt inspected |
| Existing native --version / --help | Both passed; dirty development identity above | version.log, help.log |
| ctest --test-dir build -R '^(teleno_single_copy_measurement_test\|teleno_direct_restore_preflight_cli_test\|teleno_direct_restore_workflow_cli_test)$' --output-on-failure | 3/3 passed, 3.19 s | focused.log |
| Koinos One mkdocs build --strict | Passed, 0.32 s | frontend-docs.log |
| Both repository git diff --check; Teleno AGENTS/CLAUDE cmp | Passed | diff-check.log, agents-check.log |
| SHA-256 preservation against starting files | Only five intended existing native docs changed; source/tests/protected files unchanged | files-before.sha256, preservation-after.txt |
| Local documentation target existence | All newly linked native pages present | links-check.log |

Current df -k reading at check time: 40,295,860 available 1024-byte blocks
(41,262,960,640 bytes), below the sparse-image backing guard. No new capacity
image experiment was run. Initial relative-path log inspection mistakenly ran
from the frontend checkout and failed to find native evidence; it was corrected
to the native checkout. No failed lookup is counted as validation.
SC3–SC5 measurements/full-suite outcomes above are explicitly prior evidence,
not new full-suite, sanitizer, capacity or historical runs. No rebuild was needed
for documentation-only changes; no new canonical dependency build is claimed.

SC6's documentation checklist is prepared, **not a release-readiness pass**.
Every repository-scoped Definition of Done item has not been proven; SC4/SC5,
historical/operational gates and external release actions remain open.
