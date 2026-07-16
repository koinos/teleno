# Teleno

Teleno (`teleno_node`) is a monolithic, Koinos-compatible blockchain node: a
single native binary that replaces the legacy Koinos microservice cluster
while preserving Koinos protocol compatibility and mainnet behavior.

It runs as an observer or block-producing node, exposes JSON-RPC and gRPC
endpoints, and includes native backup and restore tooling.

## Build

The build script fetches and compiles all native dependencies (Hunter-based
Koinos packages, cpp-libp2p, RocksDB with zstd) and then builds the node:

```bash
./scripts/build-cpp-libp2p-koinos.sh
./build/teleno_node --version
```

Dependency builds land in `.deps/` and the binary in `build/teleno_node`.
See [docs/install-or-build.md](docs/install-or-build.md) for prerequisites
and details, and [docs/release-builds.md](docs/release-builds.md) for release
build identity.

## Run

```bash
export BASEDIR="$HOME/.koinos-one/testnet-observer"
mkdir -p "$BASEDIR/chain"
cp config/example/harbinger/genesis_data.json "$BASEDIR/chain/genesis_data.json"
./build/teleno_node \
  --basedir "$BASEDIR" \
  --config config/testnet-public-bootstrap-observer.yml \
  --disable block_producer grpc
```

Network config templates live in `config/` and per-network genesis data in
`config/example/` (mainnet, harbinger testnet).

Start with [docs/quickstart.md](docs/quickstart.md), then
[docs/running-observer-node.md](docs/running-observer-node.md) and
[docs/running-producer-node.md](docs/running-producer-node.md).

## Container

A Linux container image is published as `ghcr.io/koinos/teleno`:

```bash
docker run --rm ghcr.io/koinos/teleno:latest --version
```

See [docs/operations/container.md](docs/operations/container.md).

## Versioning

The native runtime version comes from the `VERSION` file at the repo root
(SemVer, no build metadata). Builds report
`teleno_node <version>+<git-commit>[-dirty]` via `--version`. Release tags use
the form `teleno-node-v<version>`. Between releases the tree carries a `-dev`
prerelease segment. See [CHANGELOG.md](CHANGELOG.md) for release notes.

## Documentation

- [docs/README.md](docs/README.md) — CLI guide index
- [docs/configuration.md](docs/configuration.md) — config reference
- [docs/backup-restore-cli.md](docs/backup-restore-cli.md) — native backup/restore
- [docs/rpc-endpoints.md](docs/rpc-endpoints.md) — JSON-RPC and gRPC surface
- [docs/logs-and-diagnostics.md](docs/logs-and-diagnostics.md)
- [assets/branding/README.md](assets/branding/README.md) — original Teleno logo,
  icon variants, and visual identity notes

## Relationship to Koinos One

[Koinos One](https://github.com/koinos/koinos-one) is the desktop app that
manages Teleno nodes. It consumes this repository as a git submodule and
bundles the `teleno_node` binary into its packaged builds. This repository is
fully standalone: cloning and building it is enough to run a node from the
CLI.

## License

[MIT](LICENSE)
