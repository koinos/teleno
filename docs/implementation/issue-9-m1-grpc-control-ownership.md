# M1 gRPC control ownership

This is an implementation step within the continuing M1 objective, with no
overall execution deadline. It creates no new goal, frozen candidate or
deployment authorization. Tests remain exclusive to isolated OVH; the
operational producer is excluded.

**Later C13 checkpoint:** startup/factory controls do not cover the separate
application-error status path. Exact OVH regressions reproduce uncounted
fallback ownership, insufficient percent-encoding workspace and allocation
failures both before and inside status preparation. The unfrozen repair and
permanent regressions pass 7 normal, 13 scoped ASan/UBSan and 9 scoped default
TSan checks with working controls; no C14 candidate is frozen. Preserve the
earlier scoped results without treating them as final status-path qualification.
See the [current failed and repaired controls](../validation/low-resource-lr2-m1-json-cleanup.md#c13-remains-unqualified-resource-guard-and-status-path-defects).

## Reproduced gap

A gated native mempool read retains a live output state, registry entry and
interceptor while the enabled domain reports only its **240-byte bootstrap**.
The source creates those owners outside its allocation-capture scope. The
ordinary-response leases do not inventory this control memory.

The exact baseline ELF SHA-256 is
`9d50f01a7e6834d51d1ecb5c806ab559d06ac26a73c34ed4f8bc589fa470d36f`.
The gap control passes, and the required-admission control aborts because the
native read is still invoked at a full policy. The payload reservation refuses
later; no transaction is accepted or lost in this fixture. Preserve both results
under `evidence/ovh-grpc-control-owner-baseline/` in the current owned run.

## Current repair and scoped qualification

- Attribute fixed registry/factory/vector owners at startup and retain their
  physical allocation credits through stop and final destruction.
- Admit per-call state, registry nodes/buckets and interceptor allocations
  before publication. On local capacity or allocation refusal, clean any
  partial registration and state before publishing a refusal state.
- Return an explicit local `RESOURCE_EXHAUSTED` response before native work.
  Preserve unrelated missing-state/internal failures as internal failures.
- Keep the minimum refusal protocol usable at a full policy: its fixed control
  owners and 4-KiB status workspace are **counted but quota-exempt**, as are
  fixed startup controls required for restart. Expose that protected
  consumption; it is not additional application payload admission. Do not
  claim an aggregate transport-queue ceiling from this per-call fallback.
- Retain registry buckets as real owned memory until their registry is freed;
  never release their credits merely because the map becomes empty.
- Borrow error-classifier and status-input strings during their synchronous
  use. Materialize the SDK-required error string only inside admission and
  failure handling; common RPC-facade overhead remains a separate consumer.

This extends protected control attribution beyond canonical/recovery work.
The protected-allocation metric and operator documentation must say so. SDK
request queues, connections, common RPC facade/context overhead and allocator
retention remain distinct R1/whole-node consumers; their exclusion does not
establish a physical-memory bound.

## Required checks

Hold a real localhost RPC before payload creation; distinguish startup from
per-call credit. Verify final release after stop, restart while the policy is
already full, refusal before native invocation, and one-time failures at state,
map-node, bucket and interceptor construction. Require safe partial cleanup,
explicit overload, no acceptance and a later healthy prepared transaction.

Run existing gRPC behavior/output regressions and the new ownership case under
normal, scoped ASan/UBSan and default TSan with functioning controls. TSan uses
the separately identified instrumented pinned gRPC runtime: the prebuilt
runtime's SDK-only warning is not a passing native test. Final whole-node and
complete M1 qualification remain separate requirements.

The repair passes 2/2 normal, 4/4 default TSan and 5/5 ABI-compatible scoped
ASan/UBSan invocations, including detector controls. Attribution is 240 B
bootstrap, 968 B startup, 2,048 B with the native read held and 240 B after
stop. Full-policy restart/refusal, four distinct setup allocation failures
and healthy prepared-transaction retry pass.

The initial ASan context-destruction failure is preserved. Pinned gRPC's
ASan-only public mutex layout does not match the normal prebuilt runtime.
The upstream `GRPC_ASAN_SUPPRESSED=1` compatibility mode restores that ABI
while compiler ASan/UBSan remains active for Teleno. Its absent SDK mutex/CV
leak checker and prebuilt dependencies remain instrumentation exclusions.
See the [exact report](../validation/low-resource-lr2-m1-json-cleanup.md#usable-grpc-detector-closure-and-r8-controls).

The expanded fixture passes **3/3 normal, 9/9 scoped ASan/UBSan and 5/5
default TSan invocations**, including a clean gRPC callback and intended
use-after-free/unaligned-store controls detected at the actual callback.
Two held native reads retain 2,832 accounted bytes versus 2,048 for one;
client cancellation and concurrent stop retain the live handlers, then return
exactly to the 240-byte bootstrap. Both normal and instrumented executions
pass. Final-candidate and whole-node qualification remain pending.

The later exact R13 native/sanitizer, controlled-chain and full-history
observer gates pass, but they do not answer the aggregate protected-control
question. At a full ordinary M1 quota, each new local-overload call can own a
protected refusal state, registry/interceptor allocations and a 4,096-byte
status lease until RPC completion. The normal limit does not cap the sum of
simultaneous protected calls. The one-call refusal and two-call cancellation
fixtures prove release, not aggregate concurrency. Before an M1 completion
claim, run a held-refusal concurrency sweep with exact owner/peak/RSS samples
and establish a finite control boundary, or explicitly assign and qualify it
under R1 transport concurrency. Preserve the ability to report ordinary
local overload and the existing final-owner release behavior.

## Aggregate refusal follow-up — 2026-10-22

The continuing diagnostic starts at 23:16:37.244404 UTC, with no overall
deadline and no new frozen candidate. Evidence is under
`/Volumes/My Passport/teleno-m1-grpc-refusal-20261023-7u4u7z17/`.
The unchanged R13 state/status component is compiled on the miner and run on
isolated OVH. Holding 1, 8, 64 and 512 refusal states/statuses consumes
4,634, 37,072, 296,576 and 2,372,608 attributed bytes above a full ordinary
quota. Eight workers create the larger cases; every case returns exactly to
the 240-byte bootstrap after the final caller releases its status. This
reproduces uncapped aggregate control growth, not a leak. Registry/interceptor
and real SDK queues are excluded from this diagnostic.

The unfrozen repair adds a separate 1-MiB refusal allowance to the same
lifetime-owned domain. Refusal leases and captured C++ fields keep their
allowance credits through final deletion; canonical/recovery work retains its
separate exemption. The per-allocation lease and side-table layout stay the
same size by tagging an impossible physical-request bit; physical requests
at or above 2^63 are rejected before accounting. The shared bootstrap grows
from 240 to 280 bytes. New native metrics expose live/peak refusal bytes,
capacity refusals and transport cancellations. A full refusal allowance falls
back to an empty `RESOURCE_EXHAUSTED` status with no C++ payload allocation.

Six small normal checks pass on OVH: core exact boundaries/concurrency/final
ownership and five status/fault/retry cases. The 16,384-byte status fixture
holds 47 detailed and 17 code-only statuses, with a 16,336-byte attribution
peak and exact final release. Scoped ASan/UBSan passes the same six cases and
three working clean/known-fault controls. These development components do not
qualify a complete candidate or a physical-memory improvement.

The real gRPC component passes behavior, ownership/fault and concurrent
cancellation/stop checks, but its new exhausted-control test fails with signal
11. Scoped ASan reproduces the failure after the nine successful cases above:
`CreateServerInterceptor` calls `ServerContextBase::TryCancel` before pinned
gRPC 1.31.0's synchronous `CallData` constructor binds `ctx_.call_`. The trace
ends in `cancel_with_error` with a null call. Preserve both normal and ASan
failures; no repaired node is qualified or deployed. A separate diagnostic
SDK object moves call binding before factory callbacks to test this cause.
That object and any eventual dependency recipe require their own identities,
normal/concurrency/sanitizer checks and final-source rebuild before use.

The subsequent SDK-order diagnostic passes **11/11 OVH invocations**: four
real-server cases in normal mode, the same four with scoped ASan/UBSan, and
three functioning detector controls. The exhausted-control case cancels three
calls before native work, returns to the exact 280-byte bootstrap and permits
a healthy transaction retry. Only `server_cc.cc.o` differs in each build-owned
SDK archive; every other member and the installed archive remain unchanged.
The first explicit-object link fails with duplicate SDK definitions and is
preserved separately. Replacing the member in an owned archive resolves it.
This is diagnostic component evidence, not final-node qualification.

Eight small default-TSan invocations also pass: six core/status cases and a
clean/intended-race control pair. Only the test child disables ASLR; no
deadlock detector is disabled. This scope does not include the actual gRPC
callbacks or instrumented SDK closure.

`TelenoGrpcCallBinding.cmake` now fetches the hash-pinned 1.31.0-p0 source,
checks 270 matching installed headers, compiles the corrected source, and
replaces exactly one member in a separately owned SDK archive. The recipe
records original, object, patched and unchanged-member identities. Thirteen
offline OVH recipe tests pass, including source idempotence/drift, wrong pins,
one-member replacement, input preservation, duplicate/missing members and
output-alias refusal, Release/Debug selection, mapped configuration and
explicit instrumented-archive pin rejection. The first expanded recipe run
preserves three tool-PATH preparation failures; the separate verified
CMake/Ninja environment passes all thirteen cases. Its first configure rejects SDK headers intentionally
not installed by the pinned package; an explicit 14-path source inventory
corrects that check. The next component build selects the Debug base for
Release and its reconciliation cannot find the expected Release archive.
Both results remain preserved. Configuration selection is corrected, and
the current repository module configures, builds and links its normal native
component against the expected Release base hash
`602bc6efaeeceb1ecad80f42322cbaf4418d78f6a1f9a6d2aecfcc94b9c6e69e`.

Expanded fallback state/node/bucket/interceptor physical-failure checks pass
eleven normal/ASan/control invocations. Each capacity case verifies seven
cancellations before native work, exact final 280-byte attribution and a
successful later retry. Six additional real-gRPC default-TSan invocations
pass using all five identified instrumented SDK archives and the repository
call-binding recipe: SDK clean and intended callback-race controls, native
behavior, ownership, concurrent cancellation/stop and expanded capacity.
The known-race control detects its actual callback; no deadlock detector is
disabled. Other native stores, Protobuf, Boost and SSL remain prebuilt in this
component scope. A frozen whole-source normal/sanitizer build, paired policy measurements and the exact
new node's controlled/full-history runtime gates remain open. All executable
checks remain OVH-only. No new node or producer has started in this follow-up.

## R15 final qualification checkpoint

The complete normal Linux build, all 104 registered normal OVH checks, both
CLI checks, 63 focused ASan/UBSan and 61 default-TSan unique cases pass.
The earlier artifact-prefix and wrong-SDK-link failures remain preserved.
Five pairs per measured workload, controlled-chain create/reopen and the
exact full-history observer's mixed reads, clean restart and interruption/
recovery pass. The final owner/physical review completes M1 for exact R15.
See the
[R15 report](../validation/low-resource-lr2-m1-r15.md) for identities, scope,
resource accounting and remaining gates.
