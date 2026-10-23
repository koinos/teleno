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
unknown-field, arena-string and repeated-field exception cleanup. Hunter packages, installed public headers
and `protoc` remain read-only inputs. The archive SHA-256 is
`824eca40d5a9b7e1090961e84efe7b90fba818156c32d92e447f7daf61522318`.
Use `TELENO_PROTOBUF_ARCHIVE` for a local copy of that exact archive and
`TELENO_PROTOBUF_BUILD_JOBS` to select child-build concurrency (default two).
Parent compiler/toolchain and diagnostic flags propagate to this isolated
runtime build. This is a build setting, not a node memory limit. See the
[ownership validation and remaining Linux gates](validation/protobuf-unknown-field-ownership.md).

The M1 repair also requires a Python 3 interpreter at configure time and
rebuilds the matching generated Koinos protobuf sources from
`koinos-proto-cpp@a24bb5ff91b2f245863bddb02496cc4831ad96c2`
(archive SHA-256
`7152e153ff9a5af0add6a485faa83a1ce93803cd2c6bf9cf7b5fda5542c8832a`).
The build-owned copy repairs generated copy construction and oneof allocation
publication using a checked input/output manifest. All native consumers use the
corrected build-owned inline headers; installed SDK headers are verified against
the original pinned inputs. No protoc run or installed SDK modification occurs.
`TELENO_KOINOS_PROTO_ARCHIVE` can select a local copy of the exact archive for
offline configuration. Rebuild C++ consumers against matching headers and
libraries; unchanged message layouts/wire bytes do not make mixed inline
implementations a qualified build. See the [active repair status](validation/low-resource-lr2-m1-repair.md).

The current M1 development build also stages the pinned nlohmann JSON 3.11.2
headers in its own build tree with an allocation-free destructor backport.
It verifies every installed header and the patched result; mixed or modified
inputs are refused. Installed SDK files remain unchanged. Unchanged pinned
Koinos exception/crypto 1.0.2 sources are rebuilt against those same headers
to avoid old JSON template code remaining in imported libraries. For offline
configuration, `TELENO_KOINOS_EXCEPTION_ARCHIVE` and
`TELENO_KOINOS_CRYPTO_ARCHIVE` may select the exact checked archives; their
SHA-256 values are respectively
`22e9593ca476be7a74a94a5f2d0ab02adc29c163e9c88d288b7ce91c1fc9b5af`
and `45f8bf8f1d337ae18c3f8f0196d4e9da1902ec301b49f453b876d02a9bf8be18`.
Source overrides cannot bypass verification. This build recipe and its passing
dependency regressions do not establish final native or whole-node
qualification; see the [JSON cleanup continuation](validation/low-resource-lr2-m1-json-cleanup.md#r4-and-dependency-internal-cleanup-results).

The continuing M1 build also repairs synchronous interceptor call binding in
pinned gRPC 1.31.0-p0. `TELENO_GRPC_ARCHIVE` can select a local copy of the
exact source archive, SHA-256
`48f52c566a03b3bea26e8440798d2ff4ce181221935e5983fb9495e13618dd55`.
The recipe verifies matching installed public headers, compiles the corrected
`server_cc.cc`, and replaces only that member in a build-owned static SDK
archive. The installed SDK is preserved. Archive identity reports under
`build/teleno-grpc/` identify the original, replacement and patched hashes and
the unchanged-member count. Single-config builds select their matching SDK
configuration; multi-config outputs are kept separately.

An identified instrumented C++ SDK base can be supplied through
`TELENO_GRPC_BASE_ARCHIVE` and its mandatory `TELENO_GRPC_BASE_SHA256`.
Diagnostic target remapping must preserve this repaired archive instead of
later replacing `gRPC::grpc++` with an unpatched SDK. Other SDK dependencies
still need their declared instrumentation closure. Component recipe/server
checks pass; final whole-source Linux qualification remains open. See the
[refusal and SDK lifecycle work](implementation/issue-9-m1-grpc-control-ownership.md#aggregate-refusal-follow-up--2026-10-22).

For scoped ASan/UBSan diagnostics against the normal pinned prebuilt gRPC
runtime, compile every gRPC consumer with its upstream
`GRPC_ASAN_SUPPRESSED=1` compatibility define while keeping the compiler's
`-fsanitize=address,undefined` instrumentation enabled. Otherwise the headers
add ASan-only mutex/CV fields absent from that library and change public context
layout. This compatibility mode omits the SDK's conditional mutex/CV leak
checker; it does not instrument the prebuilt library. Verify application
detection with clean and deliberate callback-fault controls. Default TSan
diagnostics instead use separately identified instrumented pinned gRPC archives,
with their public headers verified against the installation. Preserve all
remaining dependency exclusions; see the
[detector evidence](validation/low-resource-lr2-m1-json-cleanup.md#usable-grpc-detector-closure-and-r8-controls).

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
