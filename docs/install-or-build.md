# Install Or Build

`teleno_node` can be run from a repository build, a packaged Koinos One
installation, a staged bundle, or the Linux container image.

## Repository Build

Run commands from the repository root:

```bash
cd /path/to/teleno
```

Build the native node and its C++ dependency stack:

```bash
./scripts/build-cpp-libp2p-koinos.sh
```

The build applies `cmake/cpp-libp2p-listener-lifetime.patch` after the pinned
cpp-libp2p compatibility patch. The listener callback acquires a weak owner
before dispatch so a queued callback cannot use a destroyed manager during
stop/restart. See the [P2P lifetime validation](validation/fogata-gossip-publication.md).

When `build/` already exists, a focused rebuild is faster:

```bash
cmake --build build --target teleno_node --parallel
```

All three native CMake entrypoints rebuild Koinos Fizzy from pinned commit
`b9bf7feaa8009a3d4f4bdd49245a5cd55d122055` plus Teleno's narrow context-allocation
correction. Existing Hunter installs are not modified. The archive SHA-256 is
`40975339c2e8a0f7aa1693866945becb40a65e3c0cdee862cc1ee760b99b7533`;
downloaded/extracted sources and the isolated upstream child build belong to
the chosen native build tree. The child retains upstream platform/compiler
checks and uses two build jobs by default (`TELENO_FIZZY_BUILD_JOBS`), not a
runtime resource limit. Parent compiler/toolchain and diagnostic flags propagate.

For an offline build, configure `TELENO_FIZZY_ARCHIVE` to an existing local
copy of that exact archive; the same digest is still required. Unverified source
directory overrides and an already-supplied `fizzy::fizzy` target are rejected.
See [V1-B validation](validation/low-resource-lr2-vm-allocation.md) for patch
identity, checks and remaining Linux/historical gates. A source build is not a
Koinos One packaged-binary update.

The native entrypoints also rebuild the pinned Koinos protobuf `3.17.3.0`
runtime from commit `e1b1477875a8b022903b548eb144f2c7bf4d9561`, with a scoped
unknown-field ownership correction. Hunter packages, installed public headers
and `protoc` remain read-only inputs. The archive SHA-256 is
`824eca40d5a9b7e1090961e84efe7b90fba818156c32d92e447f7daf61522318`.
Use `TELENO_PROTOBUF_ARCHIVE` for a local copy of that exact archive and
`TELENO_PROTOBUF_BUILD_JOBS` to select child-build concurrency (default two).
Parent compiler/toolchain and diagnostic flags propagate to this isolated
runtime build. This is a build setting, not a node memory limit. See the
[ownership validation and remaining Linux gates](validation/protobuf-unknown-field-ownership.md).

Verify the binary and command surface:

```bash
./build/teleno_node --version
./build/teleno_node --help
```

## Packaged App Binary

Packaged macOS builds stage the native binary as an app resource:

```text
/Applications/Koinos One.app/Contents/Resources/teleno/bin/teleno_node
```

After local package staging, the same layout is available under:

```text
build/bundle-staging/teleno/bin/teleno_node
```

Verify a packaged or staged binary before using it directly:

```bash
"/Applications/Koinos One.app/Contents/Resources/teleno/bin/teleno_node" --version
```

Use the Koinos One GUI for normal desktop operation. Run the packaged binary
directly only when you intentionally want a CLI-managed node and a separately
selected basedir.

## Linux Container

The published image packages the Linux `teleno_node` runtime:

```bash
docker pull ghcr.io/koinos/teleno:beta
docker run --rm ghcr.io/koinos/teleno:beta --version
```

The container is useful when an operator does not want to install the native
C++ build dependencies on the host. Container examples in this guide keep
JSON-RPC host publishing bound to `127.0.0.1` unless public RPC exposure is
explicitly intended.

## Build Identity

Every packaged app build should be traceable through product version, build
timestamp, Git commit, release channel, and native node build identity. For a
direct CLI binary, keep a record of:

- the `teleno_node --version` output;
- the Git commit or container tag used to build it;
- the config file path;
- the basedir path;
- the selected network.

## Next Step

After the binary is available, start with [Quickstart](quickstart.md).
