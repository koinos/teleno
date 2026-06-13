# Start Teleno Node From The Command Line

This guide starts the monolithic `teleno_node` runtime directly from the repository checkout.

Run all commands from the repository root:

```bash
cd /Users/pgarcgo/code/teleno
```

Build the node first if `node/teleno-node/build/teleno_node` does not exist:

```bash
./scripts/build-cpp-libp2p-koinos.sh
```

## Public Testnet

The prepared local testnet basedir is:

```text
/Users/pgarcgo/.kcli/teleno-testnet-producer/basedir
```

Start testnet as an observer:

```bash
./node/teleno-node/build/teleno_node \
  --basedir /Users/pgarcgo/.kcli/teleno-testnet-producer/basedir \
  --log-level info \
  --enable p2p jsonrpc \
  --disable block_producer grpc
```

The current prepared testnet config has `features.block_producer: true`. Keep `--disable block_producer` unless the run is intentionally a producer run.

For an intentional testnet producer run:

```bash
./node/teleno-node/build/teleno_node \
  --basedir /Users/pgarcgo/.kcli/teleno-testnet-producer/basedir \
  --log-level info \
  --enable p2p jsonrpc
```

If `BASEDIR/block_producer/private.key` is missing, `teleno_node` creates a new producer hot key and writes the matching `BASEDIR/block_producer/public.key` when `block_producer` starts.

Check the local testnet JSON-RPC endpoint:

```bash
curl -sS http://127.0.0.1:18122/ \
  -H 'content-type: application/json' \
  --data '{"jsonrpc":"2.0","id":1,"method":"chain.get_head_info","params":{}}'
```

## Peer Snapshot Log Interval

Connected-peer snapshots are written to the node log at most once per configured interval. The default is `60` seconds:

```yaml
p2p:
  peer-log-interval-seconds: 60
```

Set `peer-log-interval-seconds: 0` to disable periodic connected-peer snapshots. Peer connect/disconnect and handshake events are still logged separately.

## Mainnet / Production

The restored mainnet basedir is:

```text
/Volumes/external/knodel-monolith-restore/basedir
```

Start mainnet as an observer:

```bash
./node/teleno-node/build/teleno_node \
  --basedir /Volumes/external/knodel-monolith-restore/basedir \
  --config /Volumes/external/knodel-monolith-restore/basedir/config.yml \
  --log-level info \
  --disable block_producer grpc
```

The current restored mainnet config has `features.block_producer: true`. Keep `--disable block_producer` for normal mainnet operation. Do not enable mainnet block production unless the signer, producer address, network, and operational intent have all been verified first.

The restored mainnet config listens on JSON-RPC `127.0.0.1:8080`. Check it with:

```bash
curl -sS http://127.0.0.1:8080/ \
  -H 'content-type: application/json' \
  --data '{"jsonrpc":"2.0","id":1,"method":"chain.get_head_info","params":{}}'
```

To override the mainnet JSON-RPC port at launch:

```bash
./node/teleno-node/build/teleno_node \
  --basedir /Volumes/external/knodel-monolith-restore/basedir \
  --config /Volumes/external/knodel-monolith-restore/basedir/config.yml \
  --jsonrpc-listen 127.0.0.1:18122 \
  --log-level info \
  --disable block_producer grpc
```
