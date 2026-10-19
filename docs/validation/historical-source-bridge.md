# Read-only historical source bridge

Updated: 2026-10-17. Status: **controls, paired Linux boundary and three
50,000-new-block full-VM comparisons pass; fourth range stopped by an attributed
compiler-scope OOM guard; H1 remains open**.

The complete preserved historical corpus stays on My Passport. An isolated
read-only RocksDB process serves bounded protobuf block-store requests over
loopback and existing authenticated SSH forwarding to the authorized Linux
builder. This avoids another complete database copy. The reference VPS needs
no additional endpoint or database for this diagnostic.

The source opens with a quiet logger and absolute-consistency read-only WAL
recovery. Only highest-block, bounded block-by-height and block-by-ID reads are
implemented. Frames are limited to 64 MiB, parsed requests to 1 MiB, and batches
to 1,000 blocks; the replay callers request 50 blocks. Frame operations have a
30-second deadline. A source session has a six-hour-plus-five-minute bound.
Source file metadata is compared before and after each closed session.

Fresh Linux controls verify preservation of an unknown protobuf field,
rejection of an oversized frame before its body is read, matching historical
block/receipt results by height and ID, an independent canonical anchor,
method refusal and batch capacity. All three control cases pass. Both source
and forwarding processes close, and source metadata remains unchanged.

The native full-VM diagnostic uses the current B3 controller/shared backend,
with the same low-resource RocksDB settings and 64-MiB soft WAL flush trigger.
The reference uses the unchanged tagged chain v1.5.2 and state DB v1.2.1
sources, freshly compared with their pinned Git archives. The diagnostic calls
each controller directly and deterministically hashes every complete generated
receipt, including event and resource fields. Each trace row also records the
computed canonical block ID, root and LIB. This direct submission diagnostic
has separate scope from the native indexer's completed checked-range evidence.

Both Linux programs fully execute **62 blocks**, from revision **30,504,141**
through **30,504,203**. Every receipt/head/root/LIB row and all nine common final
fields match. Both complete trace files have SHA-256
`eaf27b1e663c421af081e535dfea07f27be264a1f33b90eb8f7ebc5e6d58c01a`.
Native/reference wall times are 2.854 / 1.864 seconds; kernel peak RSS is
31,068,160 / 27,746,304 bytes. These are correctness diagnostics on the larger
builder, with concurrent development work, rather than VPS performance ratios.
Owned processes use zero swap; the builder itself has configured swap.

The complete 91-member boundary namespace is archived and every member
verified in **86,086,145 bytes**. Native executable SHA-256 is
`31b1251470797b6a548d7d24d9ca4f3e8e15dbd8af904a5dd78ecfc9d799e5ef`;
reference executable SHA-256 is
`df3a5c77df2249432e3812044ca57728c0a3f36535d445776d08abe398a66da6`.
The first **50,000-block** pair, through **30,554,141**, completes. Every
generated receipt/head/root/LIB row and all nine common final fields match.
Both complete trace files have SHA-256
`0ff441b1549ee6585d836dbc259352423fe15e6ece330febff190a172e558949`.
Native/reference wall times are 2,316.034 / 1,567.569 seconds; CPU times are
1,573.37 / 1,417.57 seconds and kernel peak RSS is 96,780,288 / 157,802,496
bytes. Concurrent builder work and differing backends prevent treating these
as whole-node VPS performance ratios. All 77 namespace members are archived
and individually verified in 97,210,980 bytes. Source metadata is unchanged
and the owned source/forwarding processes close.

Three read-only calls against pinned running legacy images independently
confirm the endpoint's canonical ID and calculated root using its successor's
previous-state root. Cold read-only probes of extracted state copies find the
same durable revision **30,554,081**, ID and root in both layouts; these also
match the corresponding earlier trace row. This probe validates state metadata,
not a block index: direct-controller fixtures deliberately bypass indexing.
The first probe's block-store assumption and an intermediate diagnostic compile
error remain preserved; the corrected state-only probe leaves its inputs unchanged.

The second pair completes from that durable checkpoint through **30,604,141**,
with 50,000 new blocks and 60 replayed overlap blocks. Every complete generated
receipt/head/root/LIB row and all nine common final fields match. Both trace
files have SHA-256
`980aab52c65563fd808aa5ca4c1b852da1d77a10ec67a8b1a6f7308156b15dbe`.
Native/reference wall times are 2,050.228 / 1,647.851 seconds, CPU times
1,546.85 / 1,519.77 seconds and kernel peaks 87,207,936 / 152,072,192 bytes.
All 82 members are verified in a 97,387,819-byte archive, SHA-256
`34a8db595146b65902cfc283fffd4c3d7bfc39b724b3355d42492858b964e59f`.
An independent legacy canonical endpoint check also passes.

All 60 native and reference overlap rows match the prior trace exactly,
including LIB, in the separate final archive verifier. The initial assumption
that restarting advances the traced LIB is incorrect and is withdrawn. The
original runner compares four overlap fields; a separate mandatory final gate
compares all five fields against both complete archived traces. No LIB
exemption is accepted. Cold state-only probes of the two stopped layouts find
matching durable checkpoint **30,604,081**, ID/root and its prior trace row.
Probe inputs remain unchanged; indexed-block/full-state validation is not
claimed from this metadata check.

The third pair completes through **30,654,141**, again with 50,000 new blocks
and 60 overlapping blocks. Every trace row matches across native/reference;
all five fields, including LIB, also match the prior trace across the overlap.
An independent endpoint check against the same three running legacy images
confirms the terminal ID/root. Both stopped state copies reopen read-only at
durable height **30,654,081**, matching their earlier full trace ID/root;
file bytes and metadata remain unchanged. All 84 namespace members are
verified in the 97,475,687-byte archive under `range-002/`.

The fourth pair starts from that durable checkpoint toward **30,704,141**.
It retains the same frozen B3 helper, tagged reference, complete read-only
corpus and five-field overlap requirement. Its actual native process and
owned command were verified live after startup. A preparation reference to an
absent, unused old anchor filename failed before remote installation; the
corrected preparation uses the actual metadata roster and verifies the prior
independent endpoint and cold-state proofs directly. That failure is retained.

It subsequently stops after 917.377 seconds when the builder's global OOM
counter changes from zero to one. The kernel attributes that event to the
separate instrumented compiler's 2.5-GiB scope; this is not a completed native/
reference pair. The controller confirms closure of its worker, forwarding and
source processes and unchanged original-corpus metadata. The complete failed
namespace, including the partial trace/state, is preserved in a member-verified
94,778,266-byte archive. Resolve the compiler-resource cause before a fresh
isolated continuation; do not restart solely from a stale process marker.

Two-hour phase deadlines, the builder's 10-GiB root and 2-GiB host-available
floors, a 2.5-GiB owned process peak ceiling, zero owned swap and unchanged OOM
counters apply. The fourth pair, full interval, native-indexer coverage and
applicability to the later B6 source remain incomplete; H1 is not qualified.

Preserved preparation failures include a plain Mac bridge link requiring the
prebuilt dependency's non-RTTI ABI, a diagnostic compile using the wrong pinned
protobuf serialization method name, and a local coordinator parent-path error
before remote installation. Their corrected runs retain the original evidence.
The non-instrumented bridge ABI choice is separate from the native ASan/UBSan
run, which retains its detectors and rebuilds RocksDB with RTTI.

Owned evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/h1-eligibility-20261016/source-bridge-20261017/`.
