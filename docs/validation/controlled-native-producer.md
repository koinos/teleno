# Controlled native producer and reopen fixture

Date: 2026-10-17, Europe/Berlin. Status: **B3 empty fixtures pass on builder/VPS;
B4 real-mempool fixture with 12,800 signed transactions passes on the builder;
target-host nonempty, PoB and sustained producer qualification remain open**.

The external fixture links the frozen current native B3 libraries, manifest
`b3f83775d32e815d94adcbee967d2d66333e20eaed33d764daa390bcf9f80364`.
All 306 source inputs are freshly checked, and linked library hashes, actual
compiler flags and commands are retained. Native repository implementation and
installed services remain unchanged.

The fixture uses the real `BlockProducer`, chain controller, shared nine-family
`RocksDBManager`, `BlockStore`, `MonolithRpcClient` and in-process `EventBus`.
Its custom genesis and generated test key select **federated** signing; the
mainnet chain ID is explicitly refused. The mempool is empty. No network
adapter, RPC listener, key file or operational database is used. This establishes
real native construction, signing, application and storage for this controlled
case, not mainnet PoB or contract/transaction workload qualification.

The profile uses a 128-MiB block cache, two 8-MiB write buffers, 64-MiB aggregate
memtable threshold, one background job, Zstd level 15/128-KiB block dictionary
and a 64-MiB soft WAL flush trigger. The fresh owned fixture directory and its
owner marker are required; create refuses existing data and reopen requires
that marker and the recorded checkpoint.

| Phase | Wall seconds | Process CPU seconds | Kernel child peak RSS bytes |
| --- | ---: | ---: | ---: |
| Create 1,000 blocks | 6.100 | 2.79 | 26,292,224 |
| Clean reopen and execute tail | 0.311 | 0.27 | 34,803,712 |

The creation loop includes a two-millisecond sleep after each block. Measured
producer-call p50 / p95 / p99 is 3.787 / 4.631 / 5.344 milliseconds. These are
after-only builder measurements; no before/after improvement or 4-GB-machine
claim follows. The builder retains its existing services and configured swap;
owned `VmSwap` is zero, host OOM is unchanged and minimum host available memory
is 6,935,048,192 bytes. Root space stays at least 11,819,208,704 bytes.

Before close, head is 1,000 and LIB 940. Reopen recovers durable height 940,
fully executes the remaining 60 original signed blocks and reproduces the exact
head ID/root. All 1,000 canonical blocks and receipts are present and contiguous.
The complete framed raw block-family digest is identical before and after
replay:
`1136569fca29d015758f459e67a3c2cf3c95db12723b5dea50f64ab14c236ee3`.
This is clean reopen with synchronous WAL flush, not abrupt/power-loss proof.

An initial external compile refuses an ambiguous native/protobuf namespace
before linking, execution or fixture creation. Its complete 15-member namespace
is verified in a 52,491-byte archive, then retained separately. Qualifying the
native namespace corrects the helper. All 53 members of the completed namespace
are verified in a 12,579,427-byte archive before any later reuse or cleanup.
The frozen ELF is 30,496,520 bytes, SHA-256
`0d3b8344c6fbdc5954fcc1d3cd6b08ff4e675403adf35f1a16354fbae5744754`.

## Actual reference VPS

The exact frozen helper repeats on the 2-vCPU / 4-decimal-GB / ext4 server
after its complete restore and clean stopped observer. Every fixture creates
its own controlled chain; no node data or operational key is used.

| Phase | Wall seconds | Process CPU seconds | Kernel child peak RSS bytes |
| --- | ---: | ---: | ---: |
| Create 1,000 blocks | 7.253 | 3.29 | 27,025,408 |
| Clean reopen and execute tail | 0.979 | 0.84 | 35,434,496 |

Producer-call p50 / p95 / p99 is **4.444 / 7.343 / 9.292 milliseconds**.
Reopen again recovers LIB 940 and fully executes the 60 original reversible
blocks, reproducing its own head/root and all 1,000 block/receipt records.
The before/after framed digest is
`d7c63bad3751cbafbe36ed73ab6b6c81dfe8d33cba5ce97d473f9b9d66a9c4a7`.
The generated block timestamps differ between runs, so builder/VPS archive
digests are not compared as identical inputs.

Minimum host available memory is **3,451,260,928 bytes**, with zero swap and
unchanged OOM/swap counters. Strict whole-server headroom remains at least
**106,074,112 bytes** after counting the complete held mainnet database, OS,
runtime, temporary fixture and unchanged recovery reserve. Mainnet source
metadata remains unchanged. All **40 members** are verified in a
**12,378,422-byte** archive. Fresh process/handle checks and a complete rehash
precede retirement of only that generated fixture, before the next observer.

These are controlled federated component runs with the observer stopped.
Actual PoB
contracts/registration/VHP, nonempty mempool, VM/transaction load, concurrent
traffic and sustained producer recovery remain required. Mainnet activation
retains its separate network/target/plan and confirmation gate.

Owned evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/controlled-producer-20261017/`.

## B4 native mempool and signed transaction workload

A later normal Linux helper links the current B4 libraries, source manifest
`93a2cc291d447139813441b245ea8575f3764e4653b9edd957053f6388bc4202`.
It replaces the empty test mempool with the real native mempool and
`MempoolAdapter`, uses the actual transaction-admission callback, and connects
accepted-block and irreversible-block events as the node does. Every signed
transaction goes through the controller's ordinary validation and admission;
every block uses the real federated producer, controller and shared storage.
There are no network adapters, RPC listeners or operational key/data inputs.
The custom chain ID is checked and mainnet is refused.

The fixture creates **100 blocks with 128 signed no-operation transactions per
block**, across 32 deterministic local test payers. It verifies a nonempty
128-entry visible mempool before each block and zero visible entries after
inclusion. All 12,800 transaction identities match their receipts. Complete
block/receipt continuity and the framed raw block-family digest pass before
and after clean reopen and full execution of the 60 reversible blocks.

The first pilot correctly refuses further admission at its original
10-million-RC fixture allowance. Its 50-member namespace is verified in a
14,555,390-byte archive. A fresh custom genesis supplies one billion RC per
test payer for the planned 400 one-million-RC requests per payer across
retained fork views. This is fixture funding, not a change to native resource
checks, production state or account balances. The second pilot executes all
requested work with those checks enabled.

The mempool's 16,384-owner / 64-MiB record policy and four-job / 16-MiB work
policy are fixture settings, not qualified VPS defaults or total heap bounds.
Other storage settings retain the prior low-resource fixture profile. Temporary
child-only scopes enforce 2.5-GiB memory and zero swap while the unchanged host,
root-space, RSS, log and OOM checks remain active.

| Phase | Wall seconds | Process CPU seconds | Kernel child peak RSS bytes |
| --- | ---: | ---: | ---: |
| Create/admit/produce 12,800 transactions | 160.492 | 228.77 | 738,041,856 |
| Reopen and execute 60 nonempty tail blocks | 21.874 | 19.22 | 621,223,936 |

Producer-call p50 / p95 / p99 is **284.334 / 673.846 / 815.776 milliseconds**.
Admission and local signing are additional and included in the whole-process
figures. The larger builder also runs isolated development/history work;
these are after-only workload observations, not throughput improvement or
2-vCPU-machine qualification. Minimum host available memory is 4,975,947,776
bytes; owned swap is zero and host OOM is unchanged. Root available space stays
at least 11,774,832,640 bytes.

Durable reopen starts at LIB **40**, re-executes **60** original blocks and
reproduces the exact head ID/root at 100. The complete raw block-family digest
before/after is
`0xa913ad87cb9266ff9c6736f1cfb9c83276c0cc937b1f7fcb8f9aa45b4adf6739`.
All **79 members** are archived and individually verified in **15,084,577
bytes**. Helper ELF SHA-256:
`d19fcd51da0a8b4021318b0680e6bdc1f3942fefa131a49988e9eb75faf7a780`.
The complete fixture remains preserved; it is separate from the running B3
VPS observer. Source, installed services and the operational publisher are
unchanged. No producer is activated on mainnet.

This expands nonempty admission/inclusion/recovery evidence. It does not cover
contract execution, actual PoB/VHP/registration, abrupt or power-loss recovery,
sustained mixed traffic, all aggregate memory axes or the target-host workload.
Those remain required. Owned evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/controlled-producer-transactions-r1-20261017/`.
