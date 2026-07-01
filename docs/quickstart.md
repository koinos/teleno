# Quickstart

This quickstart starts a testnet observer from the repository build. It keeps
block production and gRPC disabled and binds JSON-RPC to loopback.

## Observer Testnet Start

Create a basedir outside the repository:

```bash
export BASEDIR="$HOME/.koinos-one/testnet-observer"
mkdir -p "$BASEDIR"
```

!!! warning "Local state write"
    The next command writes chain, block, mempool, and runtime data under
    `$BASEDIR`. Use a fresh basedir when testing.

Start the node with the bundled testnet observer config:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config config/testnet-public-bootstrap-observer.yml \
  --jsonrpc-listen 127.0.0.1:18122 \
  --log-level info \
  --disable block_producer grpc
```

The process runs in the foreground. Stop it with `Ctrl-C` and wait for the
shutdown log before moving or restoring the basedir.

## Verify Startup

Wait for the log line:

```text
[node] teleno_node ready
```

Then query the local JSON-RPC endpoint from another terminal:

```bash
curl -sS http://127.0.0.1:18122/ \
  -H 'content-type: application/json' \
  --data '{"jsonrpc":"2.0","id":1,"method":"chain.get_head_info","params":{}}'
```

Expected result: a JSON-RPC response with head information. A new or restored
node can still be catching up; compare repeated calls to confirm head height
progress.

## Keep The First Run Safe

Use this checklist before changing anything:

- Confirm the basedir is the one you intended.
- Confirm the selected config is for the intended network.
- Confirm JSON-RPC is bound to `127.0.0.1` unless public RPC is intentional.
- Confirm `block_producer` is disabled.
- Confirm logs show P2P activity or useful connection errors.
- Confirm disk space is sufficient for the selected network.

## Public Bootstrap First

For a faster new testnet setup, restore from the public read-only bootstrap
source before starting the observer. See
[Backup And Restore CLI](backup-restore-cli.md).

## Mainnet Note

Mainnet observer operation is allowed, but use a mainnet observer config and
keep block production disabled:

```bash
export BASEDIR="$HOME/.koinos-one/mainnet-observer"
mkdir -p "$BASEDIR"

./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config config/mainnet-public-bootstrap-observer.yml \
  --jsonrpc-listen 127.0.0.1:8080 \
  --log-level info \
  --disable block_producer grpc
```

!!! warning "Local state write"
    Mainnet observer startup writes local chain state under `$BASEDIR`. It does
    not register a producer, burn VHP, sign transactions, or enable production.
