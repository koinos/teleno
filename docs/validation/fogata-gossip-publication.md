# Fogata P2P publication and listener lifetime

The identified 2026-10-21 producer executable (SHA-256
`0bd4c3666a93a3b43d7c4c7e72776d9d935eec1a33cdeec22572ac71022d9447`)
aborted after glibc reported corrupted heap metadata. No readable core was
retained, so the historical corrupting instruction is unknown. The following
defects were reproduced separately against its exact source and pinned SDK.

- Native GDB and separate ASan/UBSan and TSan controls reproduced unsafe
  publication from an external thread while GossipSub heartbeat/cache work ran
  on the SDK I/O runner. Direct `GossipCore::publish` entered mutable cache
  state without runner serialization.
- A no-publication ASan/UBSan control reproduced an SDK listener-manager
  use-after-free: a queued connection callback captured its manager as a raw
  pointer across stop/restart.

The native correction queues owned publication work on the SDK runner, bounds
pending work and retires its generation on stop. The cpp-libp2p patch SHA-256 is
`44a8e8f4b06603f4463ac707c400b1cdb913f2fd4901675e27bd774fdc77d47d`.
The source SHA-256 values of the corrected transport implementation/header
are `26a18a198785a828212ccb1a5366eb489c5575a1f69b8756c35786bb38965c80`
and `296cbc5749745fc7fadab98f18936f1e8ba557191b96603cb6d4f901a975c303`.
The three-mode native fixture SHA-256 is
`2f3e49095036b661f69d59a23d80fd76f6d2d6b106bb7c80a920ff5bc2e0e4a1`.

The combined diagnostic Linux node ELF SHA-256
`70e6375c8d3f28f57cde487997b718486c6fff7f478769c0556f7669a15b60ab`
passed native CLI checks and all three normal regression modes. It ran for one
hour as an isolated private-network observer with a valid Go GossipSub peer,
followed by clean reopening of the same owned state. All 3,540 periodic samples
remained connected and subscribed to both topics; the peer sent 1,769 future
blocks and the node logged 1,800 incoming future-block markers. The 358
resource samples had at least 9,037,225,984 bytes root-free and
3,400,454,144 bytes available memory, with a maximum node RSS of 40,071,168
bytes under this isolated traffic. Neither service nor original production
state was used for the test.

Separate ASan/UBSan and TSan native fixtures passed the three modes, including
the previously failing listener control. Test-only paced valid-peer fixtures
also passed under each detector with overlapping incoming block/transaction
traffic and no sanitizer report. A first unpaced ASan fixture failed its
traffic-overlap criterion without a sanitizer finding; that negative workload
result is retained. The pinned listener and GossipSub SDK objects were
instrumented for the focused sanitizer checks; other prebuilt SDK archives
were outside that instrumentation closure.

These results qualify the identified isolated correction and test traffic.
The diagnostic ELF is distinct from any later release build. They do not prove
the exact origin of the historical abort, a full-mainnet producer lifetime,
loaded-chain operation, general resource margins or issue-9 completion.
Release CI must verify the clean release commit's own binary, CLI, tests and
container package before publication. The original production database,
configuration, keys and failed service were not changed by this investigation.
