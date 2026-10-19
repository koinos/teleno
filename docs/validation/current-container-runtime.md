# Current native binary in an isolated runtime container

Date: 2026-10-17, Europe/Berlin. Status: **seven runtime cases pass; repository
Dockerfile build, release packaging and whole-node qualification remain open**.

The exact current Linux executable is
`b38318065a982d9a83de30f4dafd0dd85a00a2709f32aa9bec23e9c28788332c`,
from the 306-input native manifest
`b3f83775d32e815d94adcbee967d2d66333e20eaed33d764daa390bcf9f80364`.
It reports `teleno_node 1.3.0-dev.0+d3cfb94455da-dirty`.

An owned temporary container uses the already present Linux/amd64 runtime image
`sha256:37d7b274440b8ff7143be0d5ab60170e98004d1f1d3a64b504dc6931b946faa3`.
The frozen binary and four current test executables are individual read-only
mounts. The image is not rebuilt or replaced. The entrypoint is the controlled
test runner; no installed node data, keys or Docker socket are mounted.
Networking is disabled, root is read-only and no ports are published. Generated
files use only the fresh owned miner directory.

| Case | Exit | Seconds |
| --- | ---: | ---: |
| Version | 0 | 0.016 |
| Help | 0 | 0.008 |
| Side-effect-free preflight | 0 | 0.008 |
| Controlled producer fixture | 0 | 0.767 |
| VM lifetime/allocation fixture | 0 | 0.033 |
| Storage/configured-WAL recovery fixture | 0 | 6.936 |
| Real-loopback RPC batch/retention fixture | 0 | 0.215 |

Producer tests use a fake chain/mempool and synthetic keys. They do not enable
mainnet production. Preflight reports the soft WAL trigger as unverified and
leaves its absent basedir absent. The storage child exercises synced abrupt
process exit and complete default-reader recovery; this is not power-loss proof.

The container enforces a CPU quota equivalent to two cores, a 2,684,354,560-byte
memory maximum and
zero swap allowance. Its cgroup memory peak is 106,172,416 bytes, including
charged file/cache memory; this is not standalone node RSS. Every cgroup OOM
event counter is zero. Host available memory stays at least 8,250,658,816 bytes
and root available space at least 11,862,945,792 bytes. The host is the larger
builder, so these limits do not establish a physical 4-GB-machine result.

All 26 pre-existing containers retain their image, running state, start time
and restart count. The temporary container is removed after inspection; only
the fields required for that comparison are retained. The complete 32-member
test namespace is preserved and every archive member is independently verified.

This validates the mounted executable's runtime compatibility with an existing
image and these fixtures. It does not validate a new image from the repository
Dockerfile, a release artifact, the Koinos One package or sustained producer/node
operation. A read-only build preflight finds no cached `ubuntu:24.04` tag and
only about 1.2 GB above the unchanged miner root-space floor; no new image build
or cache eviction is attempted in this check.

Owned evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/container-runtime-20261017/`.
