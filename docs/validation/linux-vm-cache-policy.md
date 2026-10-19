# Linux parsed-module cache policy measurements

Date: 2026-10-17, Europe/Berlin. Status: **scoped builder and actual-VPS cache
measurements complete; V1 and whole-node qualification remain open**.

## Inputs and method

Native libraries use the frozen 306-input source manifest
`b3f83775d32e815d94adcbee967d2d66333e20eaed33d764daa390bcf9f80364`.
No repository implementation, default or installed service changes. An external
copy of the existing VM measurement uses 32 distinct valid Wasm programs with
2-MiB data segments. Their actual SHA2-256 multihash IDs match an independent
Python construction. The concatenated identity digest is
`71ee27c3342b18995a0e5e2ef6cf6ceb1595f9d73ce4d4b6e0a7054e15607dfb`.
These are VM fixtures, not representative mainnet contracts or proof of Koinos
contract-upload limits.

Each fresh process loads all 32 programs, then cycles 4,096 executions through
the set. The default 32-entry cache, a 16-MiB capacity and a 128-MiB capacity
use identical bytecode, identities and call order. Every case reports the same
12,384 meter ticks. An initial identical-bytecode/synthetic-ID pilot is retained
separately; it is not used as the diverse-code result.

Five alternating triplets measure ordinary C++ allocation requests. Five more
triplets disable that probe and measure process CPU/RSS and 4,096 per-call
latencies. Unmeasured allocation counters are `null`, not zero. All tracked
cases release their tracked allocations after backend destruction. Allocation
requests exclude aligned/direct allocation and allocator retention; the cache's
attributed capacity is not total heap or RSS. The disabled-probe executable is
SHA-256 `088bf4fa6032f01578f1bb6a04a4426a5b26a450d6e5c237ac00875341dcef07`.

## Memory and reparsing

| Policy | Retained entries | Attributed cache bytes | Median process peak RSS bytes |
| --- | ---: | ---: | ---: |
| Default, unlimited attributed bytes | 32 | 67,132,608 | 82,436,096 |
| 16 MiB | 7 | 14,685,258 | 32,215,040 |
| 128 MiB | 32 | 67,132,608 | 82,567,168 |

The smaller policy reduces retained requested bytes by 78.125% and process
peak RSS by approximately 61%. It has no cyclic warm hits and performs 4,121
evictions, compared with 4,096 warm hits and no evictions for both retaining
policies. Cumulative allocation requests rise to 3.909 times the default;
that total is allocation churn, not simultaneous memory consumption.

Ratios below are the median of five candidate/control ratios, with the full
range in brackets. They are not ratios of separately computed medians.

| Metric | 16 MiB / default | 128 MiB / default |
| --- | ---: | ---: |
| Process CPU | 4.048 [3.943–5.980] | 1.041 [0.551–2.808] |
| Timed fixture elapsed | 4.176 | 1.031 |
| Warm-call p95 latency | 5.684 [4.471–11.121] | 1.051 [0.322–5.460] |
| Process peak RSS | 0.391 [0.389–0.392] | 1.001 [0.998–1.004] |

The builder retains its existing services; CPU/latency dispersion is material.
The 128-MiB case keeps the complete test set and saves no memory here. No
general overhead, speedup or production default follows from that noisy timing
comparison. The 16-MiB cyclic case consistently incurs substantial reparsing
cost. Every measured process has zero physical input blocks and eight output
blocks for its measurement output; this in-memory exercise establishes no
database or cold-I/O saving.

Sampled owned `VmSwap` remains zero, the host OOM counter is unchanged, minimum
host `MemAvailable` is 8,166,346,752 bytes and root available space remains at
least 11,923,836,928 bytes. The larger builder has configured swap; these are
not zero-swap-host or 4-GB whole-machine results. All 178 final namespace members
are preserved and independently verified in an 8,134,126-byte external archive.

## Earlier actual-node observations and remaining work

The separately captured one-hour C073 reference-VPS observation has 60 VM
snapshots: 11–19 entries, 1,813,912–3,202,382 attributed bytes, no evictions or
bypasses, and an unlimited-byte policy. This is older-source workload evidence;
one hour does not characterize diverse cold contracts, producer load or a safe
default. The synthetic larger set deliberately explores a different cache regime.

## Actual reference VPS repetition

After historical source users close and their complete namespace is archived
and verified, the same probe-disabled executable runs on the actual two-vCPU,
4,000,034,816-byte-RAM, ext4 VPS with zero configured swap. One alternating
warmup triplet is excluded; five subsequent alternating triplets provide all
ratios below. Metering, all 32 program identities and retained-cache behavior
match in all 18 processes. The stopped complete source retains identical file
metadata.

| Metric | 16 MiB / default, paired median [range] | 128 MiB / default, paired median [range] |
| --- | ---: | ---: |
| Process CPU | 3.684 [3.281–4.055] | 0.969 [0.889–1.070] |
| Timed fixture elapsed | 3.596 [3.164–3.827] | 0.963 [0.872–1.054] |
| Warm-call p95 latency | 7.131 [6.027–7.860] | 1.066 [0.959–1.193] |
| Process peak RSS | 0.392 [0.390–0.392] | 1.001 [1.000–1.002] |

Median peak process RSS for default / 16 MiB / 128 MiB is respectively
83,054,592 / 32,518,144 / 83,116,032 bytes. Median CPU is 2.09 / 7.71 / 2.14
seconds. Each measured process reports zero physical input blocks and eight
output blocks. Minimum concurrent host available memory across all 18 cases
is 3,371,663,360 bytes; root available space remains at least 10,990,129,152
bytes. Owned swap stays zero and host OOM/swap counters are unchanged.

Every member of the complete 99-member, 3,291,015-byte namespace archive is
verified before only that owned diagnostic namespace is retired. Smaller
retention saves memory in this fixture but incurs substantial reparsing and
latency costs. No native default or restored-node VM policy changes follow.

Select any operational policy using representative actual workload evidence.
Active instances, caller-held modules, parse queues, allocator overhead and
aggregate node memory remain additional. These results do not close V1 or the
steady 2-GiB / peak 2.5-GiB node RSS and 768-MiB host-headroom requirements.

Owned evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/vm-cache-mechanism-20261017/`.
