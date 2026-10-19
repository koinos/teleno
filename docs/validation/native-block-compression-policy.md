# Native block compression policy

Date: 2026-10-16. Status: **scoped native implementation verified on Mac/Linux
and the designated VPS; issue #9 and LR2 remain open**.

## Problem and change

The [full-archive experiment](lossless-archive-storage-experiment.md) preserves
every visible record while reducing the selected database by 16.422%. Its
64-KiB data blocks / Zstd level 9 / 64-KiB dictionary avoid the much larger
point-read cost of 1-MiB data blocks. Previously, the native manager selected
the codec but always constructed default level/dictionary options on reopen.
An exported compact database could be read, but future compaction would use
the default policy again.

This checkpoint exposes `rocksdb.blocks-zstd-level` (`0..19`) and
`rocksdb.blocks-zstd-dictionary-bytes` (`0`, `32768`, `65536`). Both default to
zero. Nonzero settings apply only to new `blocks` tables at normal and bottommost
levels. Dictionary training uses 1 MiB of samples and an 8-MiB buffering limit
per active builder. Other families, existing defaults, schema, record bytes,
WAL and persistence policy are unchanged. There is no automatic rewrite.

Normal and strict config loading reject malformed/overflowing scalars and
invalid ranges/combinations. The manager validates programmatically constructed
config too. Enabled parameters require actual Zstd support, with no fallback,
before the manager creates the database directory. Preflight remains side-effect
free and explicitly leaves linked support unchecked.
Normal CLI startup retains its existing behavior of creating an empty basedir
before loading config; refusal before a DB directory is created does not mean
normal startup is filesystem-preserving. Preflight remains filesystem-preserving.

## Scoped regression evidence

The frozen manifest covers 304 native inputs, with seven changed paths from
the preceding P2-G source. SHA-256:
`5f89837ed3d002ece2355865d6bde342f50441581e15b8719603c89fe09b247a`.
Source remains dirty on `codex/lr2-memory-hardening`, base commit
`d3cfb94455da4a472f651eb4239abbc289e74225`, native `1.3.0-dev.0`.

- Config tests exercise both loading modes, aliases, independent options,
  zero defaults, malformed values, overflow and incompatible codecs.
- Storage tests check invalid programmatic config before DB creation, native
  normal/bottommost options, unchanged state-family policy and independent
  level/dictionary settings. They write 12,000 deterministic binary values,
  flush/compact, inspect SST properties, reopen with ordinary default options
  and compare every ordered key/value and count.
- Actual CLI tests check valid parameters leave codec support `not_checked`
  and invalid parameters fail without filesystem changes.
- Mac all targets, version/help and **60/60 CTest pass in 196.18 seconds**.
  These are development checks, not Linux capacity evidence.
- Larger Linux builder all targets, version/help and all **60 native tests**
  pass across three retained batches. The first single-copy experiment stops
  when accumulated synthetic copies leave 11,007,045,632 bytes available,
  below its unchanged 11,040,784,384-byte recommendation. The failed evidence is
  preserved completely. After externally archiving and verifying older owned
  development duplicates, the unchanged source/binary passes that test. No
  reserve or runtime code is adjusted to obtain the pass.
- The mandatory 2-vCPU / 4,000,034,816-byte-RAM / ext4 / no-swap VPS passes
  **nine native command invocations**: version/help, config/storage fixtures,
  actual preflight CLI, two configured offline DB opens and the invalid-normal-
  startup control plus its repeat. Both offline opens log level 9, 65,536-byte
  dictionary, 1-MiB training and 8-MiB buffering. The storage fixture checks
  12,000 binary records through flush/compaction/default-policy reopen.
- The first VPS harness incorrectly expected normal invalid startup to leave
  no basedir. All eight command exits and preceding assertions matched; the
  final harness assertion failed on the existing empty-directory behavior.
  A fresh negative control with the same binary checks the correct boundary:
  empty basedir permitted, no DB or other contents created. Both attempts remain.
- VPS component peak RSS is **40,828,928 bytes**; sampled host `MemAvailable`
  stays at least **3,404,636,160 bytes**, with zero swap and unchanged zero OOM
  counter. Thin `/usr/bin/time` measurements describe these component processes,
  not a running node or mixed producer workload. The unsupported-library branch
  remains conditional and is not exercised by the pinned Zstd-enabled library.

The Mac/Linux native version is `1.3.0-dev.0+d3cfb94455da-dirty`.
Executable SHA-256 values are:

- Mac: `bf5338b88598d4be57388733f45f19931dff6256463f4ebd9053d0615dd16643`.
- Linux builder and actual VPS: `670798389cc53023e7f9a16b90684a4ede7ed01320171920f62a6d12e691ae7f`.

This dirty source has no new integration commit or submodule pin. The Koinos
One manual is coordinated and its strict build passes; GUI, packaging and
submodule files are unchanged.

## Preservation and disk accounting

Owned evidence is
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/storage-policy-28a3bhny/`.
It retains source/input manifests, before/after binaries, native/test logs,
both initial failures, corrected checks and verified remote evidence archives.
Only archived owned development duplicates are retired remotely after member/
hash verification and open-file checks. Source, pinned full archive, compact
candidate and operational data remain. The miner's observer publisher stays
active with the same config/image and fourteen running containers.

After baseline cleanup and the new isolated executable/fixtures, available
root bytes are 36,177,453,056. Against the compact copy's unchanged modeled
recommendation of 35,866,214,400 bytes, the margin is **311,238,656 bytes**.
This includes the current OS/runtime footprint and the 10-GiB reserve in the
model. It is a fresh sizing observation, not an actual complete restore,
measured compaction peak or capacity recommendation.

## Remaining outcome

This implements persistent writer policy, not a native lossless exporter or a
complete mainnet restore. The prior experimental copy and its CPU/I/O/latency
measurements remain separately attributed. No additional full-copy experiment
is needed merely to expose these options.

Supported export/metadata generation, full 40-GB restore and restart/recovery,
historical execution parity, catch-up, sustained node RSS/host headroom and
producer workloads remain required. Finite dictionary buffers are component
settings, not a whole-node memory guarantee. Nothing is published, integrated
into Koinos One packaging or activated as a live producer.

The subsequent [native rewrite checkpoint](native-archive-rewrite.md) adds an
explicit offline bottommost rewrite and removes host identifiers from newly
written tables. It records a fresh native full-archive rewrite and tracks the
actual native backup/restore verification. The policy results above retain
their original source and executable identities.
The subsequent [larger-dictionary checkpoint](larger-block-dictionary.md) also
allows `131072`, with unchanged defaults and training/buffer ceilings. Its
measurements and validation have their own source identity and do not alter
the preceding checkpoint's results.
