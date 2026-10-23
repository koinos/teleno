# Native 1.3.0-alpha.2 publication qualification

Date: 2026-10-23. Channel: experimental native alpha.

The [GitHub prerelease](https://github.com/koinos/teleno/releases/tag/teleno-node-v1.3.0-alpha.2)
publishes the Fogata P2P lifetime correction on top of the 1.3.0-alpha.1
native source. Its tag remains on the release commit while development advances
to `1.3.0-dev.2` on a separate branch from `main`.

## Exact released implementation

- Source/tag target: `4659974c1cd80cb12194caccbce4ae06784f2aac` /
  `teleno-node-v1.3.0-alpha.2`.
- Native identity: `teleno_node 1.3.0-alpha.2+4659974c1cd8`.
- Ubuntu 24.04 Linux amd64 ELF: SHA-256
  `30c356ec77dd98baacffd47d1d2f754d6d3ed993bbe2661310d51745c27255b3`,
  45,078,032 bytes.
- Image: `ghcr.io/koinos/teleno:1.3.0-alpha.2`, immutable manifest
  `sha256:69f36ffc89768b54d04a7d7e48c865295589e0ac82a7c45d09792fdd87c3a273`.
  The image executable has the same SHA-256 as the runtime archive.
- The public build metadata verifies all 438 committed source-file hashes,
  clean commit identity, linked archives and executable hash.

The release has a runtime/configuration/documentation bundle, exact source,
normal-test and image qualification logs, build metadata and `SHA256SUMS`.
All five assets were downloaded after publication; each of the four content
assets matched the published checksums. Assets are unsigned. Stable `latest`,
`beta`, `1.2.1` and prior alpha.1 image digests remained unchanged.

## Current-source checks

- [Clean native build](https://github.com/koinos/teleno/actions/runs/39904422851):
  **67/67 normal CTest entries**, including all three Fogata publication and
  listener regressions. Version/help and transaction-benchmark checks pass.
- [Manual same-binary image validation](https://github.com/koinos/teleno/actions/runs/39910497008):
  use the successful same-commit native artifact with publication disabled;
  verify source manifest, ELF, version, normal-test report, image executable,
  CLI and producer-consent refusal.
- [Tag image publication](https://github.com/koinos/teleno/actions/runs/39910694930):
  repeat the same verification before pushing only explicit alpha version,
  native-tag and full-commit aliases.

The focused ASan/UBSan and TSan checks and one-hour isolated observer/reopen in
the [Fogata validation summary](fogata-gossip-publication.md) belong to a
distinct diagnostic ELF with the identified corrected files. The clean CI ELF
and container checks do not prove the exact cause of the 2026-10-21 abort,
loaded mainnet producer lifetime, complete low-resource issue-9 bounds or
general producer readiness. Publication alone does not deploy a node, rotate
a key, or update Koinos One's package/submodule. Start as an observer and
follow the [producer activation gate](../running-producer-node.md) before
any live production.
