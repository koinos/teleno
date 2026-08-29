# Native Release Builds

The native node can be bundled into packaged Koinos One releases or released as
an independently installable runtime. Release work must preserve both the app
build identity and the native binary identity.

## Independent Version Source

The native runtime version is independent from the Koinos One desktop app
version. The source of truth is:

```text
VERSION
```

That file contains SemVer without build metadata, such as `1.1.0` or
`1.2.0-beta.1`. CMake reads it when compiling `teleno_node`, then appends the
Git revision to the runtime build version:

```text
teleno_node 1.1.0+d770931f42b8
```

Use `teleno-node-v<version>` for independent native release tags. A Koinos One
release tag such as `v1.0.4` can include any validated `teleno_node` build; the
two SemVer streams do not need to advance together.

## Native Build

The active binary target is:

```bash
cmake --build build --target teleno_node --parallel
```

Old automation can still invoke the compatibility target:

```bash
cmake --build build --target koinos_node --parallel
```

That target depends on `teleno_node`.

## Package Handoff

Packaging scripts stage app files, assets, and the native binary before running
Electron Builder. Verification scripts check that the packaged app contains the
expected native CLI surface.

## Build Identity

Every packaged release should expose:

- Koinos One product version;
- release channel;
- build timestamp;
- Git commit and branch;
- dirty/clean source state;
- native node semantic version, build version, release tag, binary hash, and
  metadata.

Use
[`scripts/generate-build-info.js`](https://github.com/koinos/koinos-one/blob/main/scripts/generate-build-info.js)
as the build identity source for the app surface.

Koinos One's packaging pipeline also generates a native build-info record
(`teleno-node-build-info.json`) with the native SemVer, build version, release
tag, source revision, binary hash, and expected container image tags.

## Linux Container Release

The Linux container workflow publishes the independent runtime image:

```text
ghcr.io/koinos/teleno
```

The native binary version comes from `VERSION`. The workflow adds standard OCI
metadata labels from the Git ref/tag set, while the Dockerfile records the
exact `VCS_REF` revision and `BUILD_DATE`. The container smoke test runs:

```bash
docker run --rm teleno-node:ci --version
```

and verifies that the reported version starts with the native SemVer from the
repository. On native release tags or manual dispatch, the workflow publishes
version tags such as:

```text
ghcr.io/koinos/teleno:1.1.0
ghcr.io/koinos/teleno:beta
```

The `beta` tag continues to track `main`.

For local Docker builds, pass the exact Git revision so the binary build
identity and the OCI revision label agree. The OCI version tag still comes
from `VERSION`:

```bash
TELENO_NODE_VERSION="$(tr -d '[:space:]' < VERSION)"
TELENO_COMMIT="$(git rev-parse HEAD)"
docker build \
  --build-arg VCS_REF="$TELENO_COMMIT" \
  -t "teleno-node:${TELENO_NODE_VERSION}" .
```

Use `--build-arg JOBS=1` on memory-constrained builders. The limit applies to
the main build and nested Hunter dependency builds.

## Release Safety

Do not ship a native node build that diverges from Koinos protocol behavior.
Producer, backup, restore, and storage changes require validation that matches
their blast radius.

## Native Release Checklist

- Build `teleno_node` from the intended source revision.
- Confirm `VERSION` is the intended native SemVer.
- Run focused native tests for changed components and broader CTest when shared
  behavior changed.
- For replay-correctness releases, record checked and full historical replay
  across blocks 30,504,202 and 32,789,377-32,789,378, the fallback count, exact
  exception behavior, the audited duplicate-key receipt range, restart
  checkpoints, and differential roots against the tagged reference
  implementation.
- For restore changes, verify that the persistent producer recovery hold
  survives a restart, cannot be released on the first post-restore startup,
  and is removed only by a later explicit `--enable block_producer` restart.
- For this replay-correctness release, use the
  [authorization-gated canary and handoff checklist](release-chain-v1.5.2-fast-replay-checklist.md).
  Preparing that checklist does not authorize its deployment, publication,
  producer, or Koinos One commands.
- Confirm `teleno_node --version` matches `VERSION` plus the
  intended Git revision.
- Confirm `teleno_node --help` exposes expected CLI surfaces.
- Generate `build/generated/teleno-node-build-info.json` for independent native
  release notes when publishing CLI/container artifacts.
- Run package staging and packaged-app verification.
- Check About/Build Info for product version, commit, release channel, build
  timestamp, and native runtime identity.
- Update changelog or release notes for user-facing behavior.
