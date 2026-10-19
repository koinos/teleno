# Restore-baseline provenance for exported source trees

Date: 2026-10-17. Status: **nine focused checks, all 63 normal Mac and Linux
entries pass for the manifest below. The instrumented run finds a separate
Protobuf arena-construction failure; subsequent runtime changes need new checks**.

The SC0 restore-baseline harness previously assumed its source directory was
a Git checkout. The Linux B6 source export deliberately contains no `.git`.
After 41 passing native entries, the baseline harness therefore fails at
`git diff --name-only`, before preparing or restoring its synthetic database.
This is a diagnostic portability failure, not a successful restore or a native
storage failure. Its complete 421-member namespace is preserved in a verified
304,628-byte archive. Both downstream diagnostic/benchmark launchers correctly
refuse to proceed; their failed states remain intact.

## Correction

For an actual checkout, the tool retains real Git status and file/diff hashes.
It first checks that Git's root is the selected repository, avoiding attribution
to an unrelated parent checkout.

For a source export, `TELENO_TEST_SOURCE_MANIFEST` must explicitly select an
absolute `teleno-native-input-manifest` version-1 JSON file. Every listed source
file must have a canonical relative path, be a regular unlinked file and match
its declared size/SHA-256. Missing inputs, changed bytes, empty inventories,
path escapes and linked inputs refuse collection. No Git repository or commit
is synthesized. Recorded provenance includes:

- `mode: verified_source_export` and `git_available: false`;
- the actual manifest digest and verified listed-file digests;
- separately labeled declared base revision/branch;
- `tracked_diff_sha256: null`, rather than an invented clean-worktree result.

This verifies the listed input bytes. It does not authenticate an arbitrary
manifest or prove that unlisted files are irrelevant. Independent source
selection and build-input/command correspondence remain required. CMake source
identity, test-root authorization and every restore assertion are unchanged.

## Exact source and binary scope

The current 311-input manifest is
`6dde3b266993faf8898f277ae8584f0c86eb659056fc604067dbbe84d2612688`.
It differs from B6 manifest
`c8bad5024f0750d566145ba251d3fce72f52a7ee766414eadc57f447e466b1f2`
in exactly three diagnostic paths:

- `tools/backup/record_sc0_baseline.py`;
- `tests/scripts/fixture_paths_test.py`;
- `tools/backup/README.md`.

Native C++/headers, CMake, runtime configuration and dependencies are unchanged.
Both Linux compile-command files and native executables are rehashed before
and after the three-file export update and remain identical. The native source
archive is verified against all 311 manifest entries. There is no integration
commit, release, package or installed-node change.

| Executable | SHA-256, unchanged |
| --- | --- |
| Mac normal | `e00e20045fcd597f931245cbd378d153a203c1054c7a817c8d37fdd2c004611d` |
| Linux normal | `cab9f6ea08bbe67f8236a9dd21d71b8f0fb1cbeb34607c3e66ec09e68f12c2b9` |
| Linux ASan/UBSan | `7ed75fef50a359e135e2186cd1177d89441ece29b12a005a115206e14f01e15e` |

## Executed checks and remaining gates

The artifact/provenance Python entry now runs nine tests, including real
temporary Git and non-Git source trees, exact-byte validation and refusal cases.
It creates no commit. All nine pass locally. The complete normal Mac CTest
run then passes all 63 entries in 156.437 seconds, with fresh owned external
temporary/artifact roots and unchanged version/help checks. No old test passes
are counted toward that fresh full run.

The exhaustive normal Linux continuation completes all 63 entries and builds
all 86 declared targets. An independent audit checks every individual JUnit
record, exact source/commands, complete executable preservation and the new
epoch's unchanged OOM counter. This uses 45 valid passes before the diagnostic
scope race below and 18 fresh passes afterward, including the failed attempt's
rerun; it is not one full CTest invocation. All owned swap samples remain zero.

ASan/UBSan then passes fresh clean/use-after-free/alignment detector controls
and 17 entries before the pending-record-budget test exposes undefined behavior
in arena string destruction after a refused copy. It stops with UBSan exit 92;
the full 540-member failed namespace is preserved. The paired cleanup launcher
correctly refuses that prerequisite and never compiles or measures its case.
The [arena cause and isolated correction](protobuf-unknown-field-ownership.md#arena-string-construction-failure)
are separate from this three-file diagnostic fix. The runtime correction
changes build inputs and requires new normal and instrumented qualification.

The formerly failing Linux SC0 entry now passes its actual file/HTTP restores
and both graceful negative cases. Its retained `environment.json` independently
records the expected manifest digest and exact hashes of all 311 listed files,
with no Git commit or clean-diff claim. The negative cases retain exit 1 and
their expected diagnostics. This closes that reproduced harness failure; it
does not replace the remaining complete-suite gates.

## Diagnostic scope retirement

The fresh Linux run reaches 45 valid passes before its resource sampler races
the removal of a completed cgroup. The P2P command and CTest both exit zero,
there are no surviving owned descendants and the OOM counter is unchanged,
but reading `memory.swap.current` returns `ENODEV`. The wrapper retains a
failed result; that attempted test is not counted as a pass. The full
362-member namespace is preserved in a verified 159,172-byte archive.

Three isolated Linux controls hold an actual counter descriptor until their
owned scopes retire and reproduce the same `ENODEV`, with unchanged 2.5-GiB /
zero-swap limits and no new OOM. Six guard checks verify healthy limits and
refusal of active-group read errors, missing initial proof, surviving owners,
other errors and nonzero swap.

The reviewed private monitor holds each actual command behind a short startup
gate until its cgroup limits are positively read. A later missing or retired
counter is recorded as terminal only with that initial proof, a removed scope
directory and no active owned process in it. Other read failures still abort.
Kernel RSS, owned-process closure, swap, OOM, headroom, disk and deadline checks
remain. Native source, flags, inputs, commands and assertions are unchanged.
The P2P test is rerun successfully with this gate. The continuation retains the
45 earlier valid same-source passes and requires every remaining test; it does
not relabel the failed attempt or claim a single complete CTest invocation.

Closed Mac fixture outputs are independently archived and verified member by
member before fresh hashes and empty recursive handle scans permit retirement.
The complete 14,561-member tar uses long-window Zstandard and occupies
42,400,920 bytes; retired path allocations total 1,253,892,096 bytes with inode
deduplication. APFS exclusive extent savings are not inferred. The first decoder
has an invalid numeric memory argument and refuses before reading a member;
its error remains, and a corrected explicit `-M256MB` decoder verifies the same
unchanged archive before any retirement. These are development-evidence storage
savings, not native database compression or target-server capacity results.

Evidence lives in the owned external run under
`evidence/source-export-provenance-r1-20261017/`; the fresh comparison namespace
is `evidence/cleanup-copy-overhead-r2-scope-20261017/`. Failed predecessors remain
preserved. These diagnostics do not close integrated dependency/thread coverage,
aggregate memory, historical, target-host producer, final 40-GB restore or
sustained whole-node qualification. The reference VPS still runs frozen B3.
