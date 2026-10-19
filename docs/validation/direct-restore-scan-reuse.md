# Direct restore validation scan reuse

Date: 2026-10-16. Status: **all 61 tests pass on Mac/Linux; five paired
component comparisons complete on Linux builder/VPS; full restore remains open**.

The earlier full VPS restore performs three complete database validations:
initial validation, validation immediately before rename and validation after
rename. Each scans 46,458,363 records across nine column families. The measured
activation phase takes 3,608.699 seconds, reads 248,005,733,820 logical bytes and
228,512,276,480 physical bytes. Those are measurements of the previous complete
restore, not an estimate of savings from this change.

The direct CLI now calls a combined engine operation that retains the same
target storage lease and attempt lock throughout validation and activation.
A private in-memory proof is returned only after successful fresh validation.
The operation checks the persisted record against that proof, rebinds the
attempt identity and rehashes the complete authenticated payload before
publication. Only the duplicate database validation before rename is skipped.
Final read-only database validation after rename remains mandatory.

Standalone validation and activation APIs retain their previous behavior.
Recovery never treats a persisted report as a fresh in-memory proof. Resuming a
verified attempt validates again; interrupted publication requires a validated
reopen. Source authentication, observer holds, exclusive rename, synchronization,
capacity checks and recovery reserve remain unchanged.

## Source identity and scoped tests

Frozen native manifest, 306 inputs:
`b3469afb4410c0f601771a3a235183e888f697226e37b1e2c12ffe2feba2a5d0`.
Four paths differ from the preceding dictionary manifest: the direct engine,
its header, its CLI integration and the existing activation regression fixture.
Mac executable SHA-256:
`b86bce21a356efc92a027f5a6dac600ea3527781fb973973b2bbcb322b4eead6`,
version `1.3.0-dev.0+d3cfb94455da-dirty`. No new committed revision exists.
Linux executable SHA-256:
`f618cd219d3feb6e78e524104302b9915b1d6c971417b9ccd275263c1602da04`.

The three focused Mac engine/workflow checks pass in 34.996 seconds. Assertions
confirm three scans for separate APIs versus two for combined completion, both
locks retained across the handoff, unchanged DB inode and observer holds.
Changed payload bytes or a changed verification record refuse publication.
Tests also cover interruption after fresh validation and after DB rename,
including rejection of a forged persisted historical-validation claim.

Both all-target builds, version/help and all 61 current-source tests pass.
The quiet Mac suite takes 225.068 seconds, with all source hashes rechecked.
Linux uses exhaustive 59/1/1 batches. All batch artifacts are archived and
member-verified before their owned duplicate copies are retired.

Initial failures remain preserved. Linux's first direct staging check refuses
insufficient space including its unchanged reserve; the other two focused
checks pass. After complete failed-fixture preservation and retirement, the
failed check passes in 2.008 seconds. The first broad Mac run encounters a
30-second transaction-work test timeout during concurrent external-disk work.
That partial suite is stopped. Owned data jobs are paused for the quiet rerun,
then resumed under a bounded guardian. No timeout, reserve or test is weakened.

## Five paired Linux measurements

Each host runs five pairs in alternating order using the same current native
libraries. The control calls the unchanged separate APIs; the candidate calls
the combined operation. Both validate the same deterministic 32-MiB optional-CF
fixture. Every pair reports identical full validation, the original DB inode
and observer holds. Scan counts are three versus two. This is a warm component
fixture, not a historical chain workload or complete backup restore.

| Host | Median paired CPU ratio | Median paired wall ratio | Median paired logical-read ratio | Largest validation-process RSS bytes |
| --- | --- | --- | --- | --- |
| Larger Linux builder | 0.799 | 0.990 | 0.79985 | 19,537,920 |
| Designated 2-vCPU / 4-GB VPS | 0.805 | 0.814 | 0.79985 | 21,917,696 |

On the VPS, separate calls read 342,577,421 logical bytes; combined calls read
274,010,594–274,010,598. Median paired CPU decreases 19.53%, logical reads
20.01% and wall time 18.62%. CPU ratios vary from 0.673 to 1.218, including
one regression; the median is not a latency guarantee. The builder's wall
ratio is approximately unchanged despite lower CPU and logical reads.

VPS physical-read counters are zero in all ten measured calls. The builder
records one 102,400-byte candidate read; other calls report zero. No shared
cache flush occurs and no cold physical-I/O saving is established. Concurrent
VPS `MemAvailable` remains at least 3,359,301,632 bytes with unchanged swap/OOM
counters. These small processes do not prove whole-node RSS or a memory saving.

Each complete pair namespace is archived and member-verified before retirement.
The complete helper/input namespaces are also preserved before cleanup. Fresh
metadata checks confirm that the stopped VPS full-node source is unchanged.

## Remaining measurements

A 32-MiB optional-column-family fixture cannot predict physical I/O or full
restore latency under the target host's cache and disk constraints. A new
complete VPS restore, interruption/recovery and strict
40-decimal-GB whole-server measurement remain mandatory. Full-node, historical,
producer and sustained resource gates remain open.

Koinos One's GUI and packaged native binary are unchanged. Integration requires
an explicitly selected native commit, binary identity, submodule update and app
package checks after the native change is reviewed and committed.

Owned evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/activation-scan-reuse-20261016/`.
