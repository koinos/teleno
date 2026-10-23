# Issue 9 M1: R15 application-memory ownership inventory

Status: the R15 source review below maps the M1 owners, release points and
separate policy domains. R15 passes 104 normal checks, both CLI checks,
63 focused ASan/UBSan and 61 default-TSan cases. Its paired measurements and
source-consistent controlled-chain creation/reopen pass. The exact-node
one-hour observer, clean restart, interruption/recovery and final owner/physical
result reconciliation pass. **M1 is complete for exact R15.** The
[R15 report](../validation/low-resource-lr2-m1-r15.md) records the decision and
the separate issue-9 requirements that remain open. This is not a
physical-memory ceiling claim; earlier R13 results remain separate.

The aggregate `OwnedMemoryBudget` is an admission policy and an attribution
counter. It is not a physical heap or RSS limit. Its global C++ allocation
table owns leases until `delete`; owned STL allocators bypass that global
capture for their raw allocation, so that request is not charged twice.
Supplementary workspace and key reservations may deliberately overlap the
bytes requested from the allocator. The pending-record owner/capacity policy
is another view of the same serialized record, not additional memory to add
to the aggregate counter. Shared fork aliases of one immutable record count
once; independent decoded copies and transport outputs have their own owners.

| Consumer and creation path | Owner and final release | Admission treatment | R15 evidence and boundary |
| --- | --- | --- | --- |
| Serialized pending transaction in `pending_record_budget::encode` | Immutable `shared_ptr` alias to `aggregate_stored`; credit survives every fork/ancestor/caller alias and leaves after the last owner frees its string. | Pre-encoding record and aggregate leases; global capture owns separate wrapper allocations. | Final normal, ASan/UBSan and default-TSan ancestor/caller and exact-capacity regressions pass. |
| Retained fork overlays, pending keys/indexes, nonce/RC records and tombstones in `mempool_impl::add_pending_transaction_to_node` and `state_db` | State nodes, maps and retained ancestors; release follows node retirement, physical map-node free and last external owner. | Scoped global C++ capture plus owned STL allocations/key allowance. | Retained-owner, boundary, concurrent fork/LIB and allocation-failure regressions pass; paired fixtures preserve owner/key/tombstone counts. |
| Nonce/RC/count read workspace in `mempool_impl::check_account_nonce`, `get_pending_nonce_on_node`, `get_pending_transaction_count_on_node` and `get_reserved_account_rc_from_node` | Local vectors, strings and decoded records; release at return, except returned nonce strings held by callers. | Scoped allocation capture, with no separate fixed read-workspace envelope. R12 removes the temporary vector-to-string key copy; C9 corrects shorter-predecessor length checks. | Final read/caller, large-key, shorter-predecessor and allocation-failure regressions pass. No physical saving is attributed to the key-copy edit. |
| Standalone transaction input in `MonolithRpcClient::reserve_transaction_work` and `mempool_impl::add_pending_transaction` | Work reservation, decoded input/arena, anonymous fork overlays and published state. Temporary leases release after work; committed owners retain their credits. | Work count/wire gate, aggregate native message/fork envelopes and scoped allocations before publication. | Final mixed-work concurrency, exact-boundary, post-acceptance fault and successful-retry regressions pass. |
| Decoded native query in both `mempool_impl::get_pending_transactions` overloads | Temporary parsed record, then an owned caller vector; fields release only with the final caller. The allocator also enters capture when an escaped vector is copied outside its original scope. | Scoped capture and owned vector allocator. R11 moves the parsed nested transaction instead of copying it. | Final caller/facade destruction and decoded-copy fault regressions pass. Five final paired processes show fewer requests with variable RSS; no physical saving is attributed to this edit. |
| HTTP transaction/mempool response in `JSONRPCServer::Session` and `process_body` | Session-owned response body and output leases survive asynchronous write/cancel; parser body retires after service dispatch. | Selected M1 output leases and refusal envelopes, plus captured allocations. | Final response overlap, cancellation and mixed-batch regressions pass in normal and focused sanitizer checks. Other HTTP services remain separate R1 consumers. |
| gRPC M1 response in `MempoolOutputState` and completion interceptor | Per-call state and message/status lease retire at SDK completion; a caller-held status string keeps its own captured credit until final deletion. Registry buckets remain attributed until physical registry destruction. | Native message/status envelope. R15 puts fallback state/map/interceptor and status workspace in a separately bounded 1-MiB refusal class. At exhaustion it cancels before native work or emits a code-only status, depending on the boundary reached. | Final boundary, fault, retry and actual instrumented-SDK concurrency checks pass. Whole-node physical evidence uses the identified observer profile; other SDK queues/context remain separate transport consumers. |
| Transaction-work count/wire policy and `MonolithRpcClient::AggregateWork` | Shared policy state outlives its facade through active jobs; a job owns its count/wire credit and native workspace until completion. Its C++ state/control allocations retain aggregate credits. | The facade enters the ordinary M1 scope before constructing the work lease. Count, wire and byte policies are intersecting admission views, not additive heap totals. | Final exact-boundary, physical-allocation-failure, concurrent-refusal and retry regressions pass without acceptance loss. |
| Producer transaction candidate in `BlockProducer::fill_block`, `set_merkle_roots` and `submit_block` | Decoded pending results, the candidate's transaction fields, hash workspace and proposal copy. Escaped transaction allocations retain credits until candidate/bundle destruction; local workspace retires at return. | These functions enter the mempool's ordinary allocation domain. Canonical chain execution re-enters its separate domain; this does not apply a local quota to consensus validation. | R15's normal producer/transaction tests and exact-source controlled 100-block/12,800-transaction create/reopen exercise this path. Producer configuration/header metadata and VM/chain execution are separate consumers; no mainnet producer is qualified by this run. |
| HTTP refusal workspace in `JSONRPCServer::process_body` | One session lease survives response writing/cancellation; its shared child counter state is captured in the parent domain until final owner release. | A shared 64-MiB child policy admits `32 * request bytes + 65536` before protected fallback work. Full child capacity closes only the refused request's connection. Borrowed IDs avoid duplicate large trees. | Final refusal-shape/gate/fault and mixed-transaction-batch regressions pass. This is a conservative workspace policy, not measured physical heap. |
| Block/LIB, expiry and recovery workspace in `handle_block`, `handle_irreversibility`, `remove_pending_transactions` and `prune` | Scoped overlays, arena decoders and nodes; committed owners remain until DB/fork retirement. | Instrumented and counted, but canonical/recovery allocation is quota-exempt. | Full-quota canonical and failure-atomic publication regressions, exact create/reopen and interrupted-observer recovery pass. The exemption is not a machine-memory ceiling. |
| Diagnostic total count and retained-memory inspection in `pending_transaction_count` and `get_retained_memory_usage` | Temporary cursor/key and identity sets retire at return; only numeric inventory escapes. The full scan excludes concurrent backend mutation with its unique DB lock. | Requests are attributed in a protected inspection scope and are quota-exempt. They are observational workspace, outside ordinary transaction admission; the scan workspace is not added to its reported retained payload. | Normal/native and sanitizer retained-owner/count regressions plus paired inventory measurements cover their lifetimes. This exemption remains explicit; it does not establish a scan or process heap ceiling. |
| Allocation-tracking infrastructure in `owned_allocation.cpp` | Static 65,536-bucket table for process lifetime; malloc-backed per-allocation lease nodes retire with the matching C++ allocation. | Tracked nodes have conservative side-table/headroom credits; the static table is outside the optional quota. | The final observer and controlled workload include static-table and libc retention in physical RSS; the controlled table reaches 520,731 entries. Accounted bytes alone do not establish savings. |

Transport ingress remains an explicit exclusion from the M1 quota. Beast's
`Session::buffer` and HTTP parser body are created before `process_body` enters
M1 admission; the body has a 1-MiB limit and headers an 8-KiB limit per
session, but the 64-worker limit is not an aggregate session-memory limit.
The gRPC SDK parses inbound messages before Teleno's handler and output
interceptor can establish an M1 scope. Neither path can be reported as covered
by the native M1 counter. Their concurrent physical memory belongs in the
whole-node RSS measurement and issue 9's separate R1 RPC-capacity review.

The baseline R10 OVH component fixture admits 64 transactions with 8,192-byte
signatures, holds two decoded query copies, expires the pool and advances LIB.
Its five fresh-process `--measure-query` samples have a 19,234,816-byte median
peak RSS and 198,059 allocation requests per process. These measurements cover
admission plus queries and cleanup, not just the query copy. The exact raw
samples and source/binary identities are under
`/Volumes/My Passport/teleno-m1-closeout-20261022-opl9t4k0/evidence/r10-baseline/`.

The subsequent five alternating R10/R11 pairs show that the R11 move reduces
allocation requests by 1,280 and requested bytes by 1,102,748 at the median,
while the candidate's median peak RSS is 155,648 B higher and higher in all
five pairs. The aggregate high-water attribution is unchanged in this fixture.
Thus the copy is avoidable application work, but an actual physical-memory
saving has not been demonstrated. The full distributions are in the same run's
`evidence/r11-query-pairs/` directory. M1's representative whole-node and
ownership/admission gates remain open.

Frozen R12 removes an additional vector-to-string copy when constructing
payer/payee nonce keys during admission and native account reads. R11 source
and allocation profile are its baseline. The exact R12 normal, focused
ASan/UBSan and focused TSan Linux builds were compiled without test execution
on the miner; the normal and sanitizer build trees were externally verified.
The R12 focused normal mempool regression and all 101 registered normal tests
plus both version/help checks pass on OVH. Five alternating
R11/R12 pairs of each admission and admission-plus-query workload show 768
fewer allocation requests and 48,990 fewer requested bytes per R12 process,
with unchanged aggregate high-water attribution. Admission-only median peak
RSS is unchanged; admission-plus-query median peak RSS is 618,496 B higher.
Thus the avoidable copy is demonstrated, but a physical-memory saving is not.
The exact R12 normal, ASan/UBSan and default-TSan matrices now pass on OVH,
and an isolated 100-block/12,800-transaction controlled-chain run passes
creation and reopen within its resource guards. Neither the paired component
profiles nor that single controlled run demonstrate a physical saving
attributable to R12. At that checkpoint, the full-history observer was still
in its one-hour mixed-read window, with representative policy selection,
deliberately interrupted recovery and final-owner review outstanding. See the
current M1 validation report.

The subsequent exact R12 full-history observer passes its one-hour mixed-read
window, clean same-database reopen and separately labeled owned-process
interruption/recovery on OVH. Five alternating R6/R12 HTTP input/receipt
process pairs also show lower R12 peak RSS under that scoped workload, while
the direct R11/R12 pairs remain physically neutral or worse. These results do
not prove that all M1 owners are admitted before publication, that the
768-MiB policy covers excluded consumers, or that the process difference is
attributable to either R11/R12 copy edit. Final-owner/aggregate boundary and
representative mixed-load policy review remain open.

R13 addresses an adjacent canonical notification allocation: the controller
previously serialized an already-built `block_accepted` protobuf and the
monolith client immediately parsed it into another protobuf before delivering
it to in-process subscribers. This work is quota-exempt canonical processing,
not retained mempool state or an additional M1 admission credit. R13 sends the
existing const reference directly through `EventBus` while `IRpcClient` keeps
the identical serialized-topic fallback for other implementations. Five
alternating R12/R13 OVH fixture pairs of 256 synthetic 262,162-byte events
reduce measured C++ allocation requests from 1,536 to zero and median dispatch
wall time from 85,083 to 2,419 microseconds. Median process peak RSS changes
from 8,396,800 to 8,359,936 bytes, too small and variable to claim a physical
memory saving. These component results do not close M1 or qualify the R13 node.
The R13 `koinos_jsonrpc_session_lifecycle_test` ELF is byte-identical to R12
(SHA-256 `d89a77cca875faeab9069118231ee8e7d6cadfd024b4981031b36fb708d32d1a`).
Thus the five R6/R12 input/receipt pairs above remain exact-binary evidence for
that unchanged R13 component; they do not attribute savings to the R13 block
notification edit or substitute for a final whole-node comparison.

The exact R13 controlled-chain run uses a 768-MiB aggregate admission limit,
32 test payer accounts, 100 blocks and 12,800 locally signed test transactions.
It completes 8,908 concurrent pending-query cycles and reopens the same state,
replaying 60 blocks to the identical head, state root and archive digest. The
creation phase peaks at 564,456,464 attributed bytes and 720,809,984 B
sampled process HWM, with zero aggregate refusals. Host `MemAvailable` stays
at or above 2,700,333,056 B; the minimum strict whole-server disk margin is
498,278,400 B after the full 8-GiB reserve. The quota leaves 240,849,904 B
between this workload's peak attribution and refusal, but this difference is
policy headroom, not measured free heap. Its 520,788 peak allocation-owner
entries and 24,190,176 B of side-table nodes at final capture are included in
the request accounting; libc retention and RocksDB/SDK allocations remain
separate physical consumers. This single run supports the candidate setting
for the tested controlled workload. It does not establish that every possible
concurrent input fits, or an RSS ceiling when canonical/recovery work is
quota-exempt. See the private `evidence/r13-controlled-mixed-composite.json`.

The final-source review identifies a distinct policy boundary: when the
ordinary M1 quota is full, `MempoolOutputInterceptorFactory` may create a
separate per-call refusal state, registry entry and interceptor under a
protected scope; `MempoolOutputState::fixed_refusal` also retains a protected
4,096-byte status lease. These allocations are attributed and released with
their final owners, and one-call/fault/cancellation tests pass. However, the
ordinary 768-MiB limit does not bound their *aggregate concurrent* total.
The existing two-call cancellation fixture does not characterize many
simultaneous refusals. This is not evidence of a deployed leak or of a failed
whole-node resource guard. It is an unresolved admission-policy distinction:
either establish a separate bounded control policy with a working overload
response, or justify and measure this control as an R1 transport exception.

## R15 source-review reconciliation — 2026-10-23

R15 source archive SHA-256 is
`3be9aa55f128f649d851ec87ca9dfa3574736eec0bdec9e4e9ebaecbae47995f`.
Its 478-file snapshot extends R13's ownership implementation with the separate
refusal allowance and pinned gRPC call-binding correction. R14's source
assembly accidentally included an unrelated P2P test registration while
excluding that work's source; its configuration failure is preserved. R15
removes only that registration from the qualification snapshot. The workspace's
P2P source, fixture and registration remain untouched.

The source audit maps every public native mempool read/mutation to the same
allocation domain. Ordinary reads include local key construction, node-vector
enumeration and Protobuf decoding. Returning a nonce string, decoded vector,
RPC future or HTTP/gRPC response transfers the physical allocation's credit;
leaving a scope or deleting a facade does not return another owner's credit.
The permanent regressions cover retained ancestors and weak/caller owners,
map-node transfer, expiry/LIB, persistent preparation, decoded-query faults,
shorter predecessor reads, serialization and post-acceptance failures. Their
R15 normal, ASan/UBSan and default-TSan executions, exact physical workloads
and final owner review are reconciled in the
[final-candidate report](../validation/low-resource-lr2-m1-r15.md).

Accounting and measurement units are deliberately distinct:

- A shared immutable record and its fork aliases have one physical owner.
  The record count/capacity view and aggregate workspace are overlapping
  policies, not additional heap bytes to sum.
- C++ allocation requests include the 48-byte tracked side-table entry on
  this Linux ABI. The 65,536-bucket static table, libc metadata/retention and
  process-global Protobuf descriptors are outside the optional quota and
  remain in process RSS. The new refusal tag preserves the per-allocation
  lease size; the shared counter bootstrap grows from 240 to 280 bytes.
- Refusal state/map/interceptor/status allocations share a 1-MiB allowance.
  The 16-KiB development fixture reaches 16,336 attributed bytes and keeps
  status-string credits until the final caller releases them. Actual-server
  tests cover a full allowance and four partial allocation failures, with
  seven cancellations, zero native admission and an exact 280-byte final
  bootstrap. These values are attributed policy bytes, not physical RSS.
- HTTP fallback has its existing shared 64-MiB workspace gate. Each request
  reserves before encoding and retains its permit through the asynchronous
  response. Its parent attribution is protected so an already full ordinary
  policy can report refusal; the separate child gate provides the bound.
- Instrumented native mempool block/LIB and recovery work remain counted and quota-exempt.
  Quota exhaustion must not change valid block processing. The 768-MiB
  ordinary setting therefore cannot be described as an all-process RAM cap.

The completed qualification runs the exact R15 node, the source-consistent
controlled workload and five equivalent pairs per measured workload. It
includes allocator retention, transport/SDK ingress, VM/database caches and
other present consumers in whole-node RSS. Their separate R1/P2/V1/S1
requirements remain open. No whole-node saving is inferred from a
component-only comparison or from the earlier R13 observer.

The final source pass also distinguishes protected **inspection** from
canonical/recovery work. The no-account total-count scan and retained-memory
inventory must remain callable when ordinary admission is full. Cursor and
identity-set C++ requests are attributed for their temporary lifetime but are
not quota-admitted. They return scalar counts, not escaped payload owners.
The full retained-inventory method is a native diagnostic interface, not a
new public RPC. Its scratch space is outside the retained-payload numbers;
normal component measurements and whole-process RSS include the work they
actually execute. This explicit observational exclusion does not weaken
ordinary transaction, nonce/RC/account-count or RPC-payload admission.
