# State-cache key ownership experiments

Date: 2026-10-17, Europe/Berlin. Status: **two external layouts measured and
component diagnostics pass; native implementation remains unchanged**.

## Baseline and scope

All 306 frozen native inputs match manifest
`b3f83775d32e815d94adcbee967d2d66333e20eaed33d764daa390bcf9f80364`
before preparation. The baseline compiles the exact native object-cache header,
implementation and backend type header on the authorized Linux builder with
GCC 13.3, C++20 and the same optimization flags for every compared executable.
The builder retains its installed services and configured swap. Its measurements
and the later designated-VPS comparison below have separate component scope;
neither establishes whole-node memory savings.

The existing cache owns each key in both its map and LRU list. Two external
copies explore one key owner:

- **List owns key:** list entries own key/value pairs; map string views borrow
  stable list keys. Map entries disappear before their keys. Splicing preserves
  key addresses, including inline and binary strings.
- **Map owns key:** map entries retain keys/values; list entries borrow stable
  key pointers. List links disappear before map keys. Insertion creates the map
  entry, then the list link; list-allocation failure removes the map entry.

Both keep the same external locking contract, immutable value owners, null
entries, logical capacity, oversized bypass and LRU policy. Warm hits already
allocate zero in the native baseline; this is not a new warm-allocation fix.
The unchanged 64-MiB payload policy does not bound container metadata, allocator
retention, caller-held values or whole-node memory.

## Correctness and diagnostics

Each layout and baseline pass an independent vector-based LRU oracle over
100,000 operations at capacities 0, 2, 64, 1,024 and 4,096 bytes. Cases include
empty, inline, long and embedded-binary keys; null/empty values; replacement,
removal, eviction, clear and held immutable owners. Four externally locked
workers perform another 40,000 operations. Nine allocation-failure positions
exercise coherent map/list/payload state, owner survival and successful reuse.

All three layouts pass separate address/undefined and thread sanitizer
executables: **six component executions**. Address/undefined runs include the
allocation-failure probe. Clean controls pass; actual heap-use-after-free,
misaligned-store and unsynchronized-write controls produce their expected
diagnostics and nonzero exits. TSan uses child-only `setarch -R`, with no host
ASLR change, detector suppression or synchronization overlay.

Instrumentation covers the standalone cache sources, harness and instantiated
headers. The standard library and complete native/dependency graph are not
instrumented here. These results do not close integrated D1 or S1 qualification.

## Paired allocation and process measurements

Each layout has its own five alternating baseline/candidate pairs for each
of three fixtures, first with an ordinary-allocation probe and then without it:
**60 measured processes per layout**. Every process retains 100,000 distinct
entries within the unchanged payload budget, performs 1,048,576 warm lookups
and samples 4,096 additional lookup latencies. Payloads, hit counts and result
digests match in every pair. Probed warm lookups allocate zero and all tracked
cache requests are released after clear. Unprobed allocation fields are `null`.

| Fixture | Baseline retained requested bytes | List owns key | Map owns key |
| --- | ---: | ---: | ---: |
| 64-byte key, 64-byte value | 37,900,000 | 29,800,000 | 29,000,000 |
| 256-byte key, 64-byte value | 76,300,000 | 49,000,000 | 48,200,000 |
| 8-byte key, null value | 13,600,000 | 12,000,000 | 11,200,000 |

Requested sizes exclude allocator overhead and aligned/direct allocations.
The probe adds its own headers, so its process RSS and timing are not used as
unprobed performance claims. The following are medians of five paired ratios
from separate **probe-disabled** processes; brackets give the full range.

| Fixture / layout | Peak RSS ratio | CPU ratio | Lookup p95 ratio |
| --- | ---: | ---: | ---: |
| 64-byte / list | 0.808 [0.806–0.809] | 0.896 [0.861–0.991] | 0.646 [0.421–0.979] |
| 64-byte / map | 0.778 [0.775–0.781] | 0.991 [0.949–1.027] | 1.088 [1.019–1.589] |
| 256-byte / list | 0.676 [0.672–0.676] | 0.789 [0.392–0.810] | 0.832 [0.402–0.838] |
| 256-byte / map | 0.655 [0.655–0.656] | 0.801 [0.614–0.923] | 0.860 [0.339–0.923] |
| Null / list | 0.925 [0.917–0.926] | 0.987 [0.936–1.153] | 0.933 [0.386–1.130] |
| Null / map | 0.841 [0.834–0.846] | 1.239 [0.974–1.288] | 1.064 [0.664–1.249] |

The map-owned layout uses fewer retained requested bytes in every fixture, but
its small-key case has a material CPU regression. The shared builder also
shows substantial timing dispersion and different absolute baseline timings
between the two series. These are separate paired experiments, not one
interleaved three-way CPU ranking. No general speedup or operational layout
selection follows from those builder series alone. Representative native cache
access distributions remain necessary before integration.

## Interleaved actual-VPS comparison

All three unprobed layouts pass functional/oracle/thread checks on the actual
reference server after the observer stops cleanly. Each profile has one
excluded warmup triplet and **five measured triplets**, **45 measured processes**
in total, with cyclic ordering. All payloads, hits and result digests match.

| Fixture / layout | Peak RSS ratio | CPU ratio | Lookup p95 ratio |
| --- | ---: | ---: | ---: |
| 64-byte / list | 0.810 [0.808–0.812] | 0.975 [0.936–1.133] | 0.981 [0.916–1.249] |
| 64-byte / map | 0.780 [0.778–0.781] | 1.053 [1.010–1.095] | 1.012 [0.859–1.211] |
| 256-byte / list | 0.677 [0.677–0.678] | 0.915 [0.840–0.977] | 0.971 [0.741–1.051] |
| 256-byte / map | 0.659 [0.658–0.660] | 0.928 [0.891–1.027] | 0.939 [0.783–0.966] |
| Null / list | 0.916 [0.912–0.927] | 0.948 [0.899–1.086] | 0.963 [0.869–1.030] |
| Null / map | 0.839 [0.838–0.848] | 0.843 [0.778–0.921] | 0.842 [0.801–0.934] |

The map-owned layout retains the memory advantage, but its 64-byte case costs
more CPU. Its small-key CPU result reverses the builder result, so it cannot
be generalized across hosts. The list-owned layout has broader overlap around
no CPU/p95 change. Neither layout is selected or integrated; current B3 remains
frozen for the full-node qualification run. The measurements support potential
metadata savings and specific tradeoffs rather than an unqualified speedup.

Protected full-node source metadata remains unchanged. Concurrent host available
memory stays at least 3,340,038,144 bytes, with zero swap and unchanged OOM/swap
counters, and strict 40-GB accounting stays within budget. All **296 members**
are verified in a **160,449-byte** archive. Every per-case physical I/O counter
remains in the evidence; this in-memory workload proves no storage-I/O saving.

The in-memory fixture establishes no database or cold-I/O improvement; physical
I/O counters are retained per process in the evidence. Full backend roots,
durability, integrated lifecycle/races and physical capacity accounting remain
separate requirements. The completed full restore and ongoing 24-hour observer
use the unchanged frozen native B3 binary.

All members of both 369-entry completed namespaces are verified in external
archives (211,366 / 212,776 bytes). Sanitizer inputs, compiler/runtime identities,
commands, controls and reports are preserved separately. No native source,
runtime default, installed service, operational database or producer changes.

Owned evidence under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/`:

- `cache-key-ownership-20261017/`
- `cache-map-key-ownership-20261017/`
- `cache-key-sanitizers-20261017/`
- `vps-cache-threeway-20261017/`
