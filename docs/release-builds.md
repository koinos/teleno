# Native Release Builds

The native node can be bundled into packaged Koinos One releases or released as
an independently installable runtime. Release work must preserve both the app
build identity and the native binary identity.

## Independent Version Source

The native runtime version is independent from the Koinos One desktop app
version. The source of truth is:

```text
node/teleno-node/VERSION
```

That file contains SemVer without build metadata, such as `0.1.0` or
`0.2.0-beta.1`. CMake reads it when compiling `teleno_node`, then appends the
Git revision to the runtime build version:

```text
teleno_node 0.1.0+d770931f42b8
```

Use `teleno-node-v<version>` for independent native release tags. A Koinos One
release tag such as `v1.0.4` can include any validated `teleno_node` build; the
two SemVer streams do not need to advance together.

## Native Build

The active binary target is:

```bash
cmake --build node/teleno-node/build --target teleno_node --parallel
```

Old automation can still invoke the compatibility target:

```bash
cmake --build node/teleno-node/build --target koinos_node --parallel
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

Use the native release metadata generator for standalone CLI/container release
notes:

```bash
npm run generate:teleno-node-build-info
```

The generated `build/generated/teleno-node-build-info.json` records the native
SemVer, build version, release tag, source revision, binary hash, and expected
container image tags.

## Linux Container Release

The Linux container workflow publishes the independent runtime image:

```text
ghcr.io/koinos/teleno-node
```

The image is labelled with `org.opencontainers.image.version` from
`node/teleno-node/VERSION`. The container smoke test runs:

```bash
docker run --rm teleno-node:ci --version
```

and verifies that the reported version starts with the native SemVer from the
repository. On native release tags or manual dispatch, the workflow publishes
version tags such as:

```text
ghcr.io/koinos/teleno-node:0.1.0
ghcr.io/koinos/teleno-node:teleno-node-v0.1.0
```

The `beta` tag continues to track `main`.

For local Docker builds that should carry the same OCI version label, pass the
version file as a build argument:

```bash
TELENO_NODE_VERSION="$(tr -d '[:space:]' < node/teleno-node/VERSION)"
docker build --build-arg TELENO_NODE_VERSION="$TELENO_NODE_VERSION" -t "teleno-node:${TELENO_NODE_VERSION}" .
```

## Release Safety

Do not ship a native node build that diverges from Koinos protocol behavior.
Producer, backup, restore, and storage changes require validation that matches
their blast radius.

## Native Release Checklist

- Build `teleno_node` from the intended source revision.
- Confirm `node/teleno-node/VERSION` is the intended native SemVer.
- Run focused native tests for changed components and broader CTest when shared
  behavior changed.
- Confirm `teleno_node --version` matches `node/teleno-node/VERSION` plus the
  intended Git revision.
- Confirm `teleno_node --help` exposes expected CLI surfaces.
- Generate `build/generated/teleno-node-build-info.json` for independent native
  release notes when publishing CLI/container artifacts.
- Run package staging and packaged-app verification.
- Check About/Build Info for product version, commit, release channel, build
  timestamp, and native runtime identity.
- Update changelog or release notes for user-facing behavior.
