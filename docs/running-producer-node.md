# Running Producer Nodes

Producer operation signs and produces blocks. It is optional, high-risk on
mainnet, and must be activated only after the observer has proven it can follow
the selected network.

!!! danger "High-risk producer operation"
    Do not enable mainnet block production as a side effect of setup, restore,
    or troubleshooting. Producer setup changes, VHP burns, producer
    registration, default-account changes, config writes targeting a producer,
    and transaction signing/submission require fresh explicit operator intent,
    network and address confirmation, and a reviewable plan.

## Producer Activation Gate

Before starting a producer, verify:

- The node is on the intended network.
- The basedir and config path are correct.
- The observer reaches `teleno_node ready`.
- Head height advances and peer health is acceptable.
- The database was not just restored without an observer-first validation run.
- The producer address is the intended address.
- The producer hot key exists and matches on-chain producer registration.
- VHP and producer registration status are correct.
- JSON-RPC/gRPC exposure is intentional and protected.
- Backup and restore procedures are known before activation.

## Key Behavior

When `block_producer` starts, the current native binary resolves the private key
from `block_producer.private-key-file` or from:

```text
<BASEDIR>/block_producer/private.key
```

If the key file is missing, the binary can create a new hot key and write the
matching public key under:

```text
<BASEDIR>/block_producer/public.key
```

That auto-create behavior is useful for test networks, but unsafe as a mainnet
assumption. A newly created hot key is not evidence that the producer address is
registered for that key.

## Producer Config Shape

!!! warning "High-risk config write"
    The following config enables production. Use placeholders in public docs and
    replace them only in private operator material.

```yaml
block_producer:
  algorithm: pob
  producer: <YOUR_MAINNET_PRODUCER_ADDRESS>
  private-key-file: /absolute/path/to/producer-hot-private.key
  resources-lower-bound: 75
  resources-upper-bound: 90
  max-inclusion-attempts: 2000
  gossip-production: true

features:
  chain: true
  mempool: true
  block_store: true
  p2p: true
  jsonrpc: true
  grpc: false
  block_producer: true
  contract_meta_store: true
  transaction_store: true
  account_history: false
```

Use an absolute key path for mainnet producer operation. Do not publish the key,
the producer address from private local memory, or host-specific inventory.

## Dry Checks Before Activation

Check file permissions and existence:

```bash
test -f /absolute/path/to/producer-hot-private.key
ls -l /absolute/path/to/producer-hot-private.key
```

Start once as an observer using the same basedir and config:

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --log-level info \
  --disable block_producer
```

Verify JSON-RPC head info while still in observer mode:

```bash
curl -sS http://127.0.0.1:8080/ \
  -H 'content-type: application/json' \
  --data '{"jsonrpc":"2.0","id":1,"method":"chain.get_head_info","params":{}}'
```

Stop the observer cleanly and review logs before producer start.

## Start Producer

!!! danger "High-risk production activation"
    This command can sign and produce blocks when the config contains a valid
    producer setup. Run it only after the activation gate passes.

```bash
./node/teleno-node/build/teleno_node \
  --basedir "$BASEDIR" \
  --config "$BASEDIR/config.yml" \
  --log-level info
```

Do not add `--enable block_producer` casually. Prefer a reviewed config file
where the producer fields and feature flags are visible together.

## Verify Producer Runtime

Review startup logs for:

- `[features] block_producer: enabled`
- `[block_producer] Private key: ...`
- `[block_producer] Public address: ...`
- `[block_producer] Producer address: ...`
- `[block_producer] Production loop started`
- `[block_producer] Produced block ...` when production succeeds

If the public address or producer address is unexpected, stop immediately and
return to observer mode. Do not attempt chain-mutating repair actions from the
same session without a fresh reviewed plan.

## After Restore

Native restore writes markers that force observer-first recovery on next start
and disables block production during that first recovery run. Keep that
behavior. Start as an observer, verify database health and network state, then
repeat the full producer activation gate before producing again.

## Out Of Scope

This CLI guide does not provide commands for producer registration, VHP burns,
wallet default-account changes, or transaction signing/submission. Those are
chain-mutating actions and require separate explicit confirmation.
