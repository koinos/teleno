# Transaction TPS Benchmark

`scripts/benchmark-transaction-tps.py` measures sustained transaction
submission and optional block inclusion throughput against a Teleno JSON-RPC
endpoint. It is designed to answer a narrower question than the earlier
latency smoke tests:

> How many unique valid transactions per second can this node admit, and how
> many can the selected chain confirm, for this workload and configuration?

The benchmark uses only the Python standard library. It does not access wallet
files, derive keys, or sign transactions. It consumes a bounded JSON Lines file
of unique transactions that were signed offline by a separate client.

## Metrics

The report separates metrics that must not be conflated:

- **Target TPS**: configured offered rate. `0` means submit as fast as the
  configured concurrency permits.
- **Offered TPS**: completed HTTP attempts divided by the submission window.
- **Accepted TPS**: successful `chain.submit_transaction` responses divided by
  the submission window.
- **Confirmed TPS**: accepted transaction IDs found in witness blocks divided
  by the end-to-end window from the start of submission through the final
  inclusion poll. This is reported only with inclusion verification enabled.
- **Submission latency**: min, mean, p50, p95, p99, and max for RPC admission.
- **Schedule lag**: delay between the intended send time and actual worker start;
  high lag means the load generator cannot sustain the requested rate.
- **Per-second buckets**: completed, accepted, and rejected requests.
- **Mempool counts**: best-effort pending counts before and immediately after
  the submission window.

Accepted TPS measures admission, not consensus throughput. A node can accept a
short burst faster than blocks can confirm it. Confirmed TPS includes both the
submission window and subsequent polling time; it depends on block interval,
block resource limits, transaction mix, producer behavior, network, and the
observation window.

## Workload Format

Each non-comment line is one JSON object:

```json
{"transaction_id":"0x<UNIQUE_TX_ID>","params":{"transaction":{"id":"0x<UNIQUE_TX_ID>","header":{},"operations":[],"signatures":[]},"broadcast":true}}
```

`params` is passed to `chain.submit_transaction`. Real records must contain
complete valid transactions with sequentially valid nonces, correct chain ID,
appropriate RC limits, and valid signatures. `transaction_id` must match the
ID that appears in block transaction objects.

Requirements:

- every transaction and transaction ID is unique;
- transactions have not already been submitted;
- the workload matches the selected chain and current account nonce state;
- the payer has sufficient Mana and any operation-specific assets;
- secret fields are absent.

The loader rejects duplicate IDs, malformed transactions, workloads above the
explicit cap, and fields whose names resemble private keys, WIFs, passwords,
mnemonics, seeds, secrets, bearer credentials, or API tokens.

The workload contains signed public transaction data. Treat it as operational
test data and do not commit live mainnet workloads.

## Safe Dry Run

Dry-run is the default and does not contact the RPC endpoint:

```bash
python3 scripts/benchmark-transaction-tps.py \
  --workload /path/to/workload.jsonl \
  --concurrency 8 \
  --target-tps 100 \
  --max-transactions 500
```

It validates the workload, writes a dry-run report, and prints the exact live
confirmation phrase that would be required.

## Live Submission

Use a disposable private network or dedicated testnet account first. Live
submission requires all of:

- `--submit`;
- `--expected-chain-id`;
- an explicit `--max-transactions` that permits the workload;
- exact confirmation naming transaction count, RPC host, and chain ID.

Example shape:

```bash
python3 scripts/benchmark-transaction-tps.py \
  --workload /path/to/workload.jsonl \
  --rpc-url http://127.0.0.1:18122/ \
  --expected-chain-id '<TESTNET_CHAIN_ID>' \
  --concurrency 8 \
  --target-tps 100 \
  --max-transactions 500 \
  --submit \
  --confirm 'SUBMIT 500 TRANSACTIONS TO 127.0.0.1:18122 ON <TESTNET_CHAIN_ID>'
```

The benchmark queries `chain.get_chain_id` before submission and stops without
submitting if it does not match. When inclusion verification is enabled, the
witness chain ID must also match before submission begins.

## Inclusion Verification

Add a witness endpoint to distinguish accepted TPS from confirmed TPS:

```bash
python3 scripts/benchmark-transaction-tps.py \
  --workload /path/to/workload.jsonl \
  --rpc-url http://127.0.0.1:18122/ \
  --witness-rpc-url https://testnet.example/jsonrpc \
  --verify-inclusion \
  --expected-chain-id '<TESTNET_CHAIN_ID>' \
  --concurrency 8 \
  --target-tps 100 \
  --max-transactions 500 \
  --inclusion-timeout 300 \
  --submit \
  --confirm 'SUBMIT 500 TRANSACTIONS TO 127.0.0.1:18122 ON <TESTNET_CHAIN_ID>'
```

The witness must provide `chain.get_head_info` and
`block_store.get_blocks_by_height`. Verification scans from the witness head
observed immediately before submission. Missing transactions make the result
fail.

## Recommended Benchmark Matrix

Do not begin by sending the maximum workload. Establish a controlled matrix:

| Stage | Transactions | Concurrency | Target TPS | Purpose |
| --- | ---: | ---: | ---: | --- |
| Smoke | 10 | 1 | 5 | Validate workload, nonce, signing, and inclusion |
| Baseline | 100 | 1 | unlimited | Measure sequential admission latency |
| Ramp 1 | 500 | 4 | 25 | Confirm stable low-rate concurrency |
| Ramp 2 | 1,000 | 8 | 50 | Observe latency and mempool growth |
| Ramp 3 | 2,000 | 16 | 100 | Find the first sustained bottleneck |
| Saturation | bounded | 32+ | increasing | Locate the throughput knee and failure mode |

Generate a fresh valid workload for each row. Stop increasing load when:

- rejection appears;
- p95/p99 latency grows without recovering;
- schedule lag shows the generator is saturated;
- mempool does not drain within the expected block window;
- the head stalls or the node disconnects;
- CPU, memory, disk latency, or severe logs cross the test policy;
- confirmed TPS stops tracking accepted TPS.

Run each meaningful row multiple times and report median plus variability.

## Reproducibility Record

Every published result must include:

- exact Teleno version and Git commit;
- dirty/clean tree state;
- node configuration and enabled components;
- network and chain ID;
- hardware, OS, storage medium, and filesystem;
- workload size and operation mix;
- concurrency and target TPS;
- block interval and producer configuration;
- RPC and witness placement;
- whether the node was synced and healthy before the run;
- result JSON and Markdown files;
- relevant node metrics and sanitized logs.

A result from simple KOIN transfers is not a universal Koinos TPS claim. Add
separate workloads for contract calls, different resource profiles, mixed
operations, and realistic account/nonce distributions.

## Output

Reports are written with owner-only permissions under
`/private/tmp/teleno-transaction-tps/<timestamp>/` by default:

- `result.json`: complete machine-readable measurements and per-request rows;
- `result.md`: concise human-readable summary and interpretation warnings.

The report sanitizes URL credentials and query strings. Workload secrets are
forbidden rather than redacted after loading.

## Validation

The repository test uses a local simulated JSON-RPC server and does not contact
or mutate a Koinos network:

```bash
python3 -m unittest -v tests/scripts/benchmark_transaction_tps_test.py
```

It covers dry-run isolation, concurrent accepted/confirmed TPS, secret and
duplicate rejection, exact confirmation, and chain-ID mismatch blocking.

See [TPS Optimization Assessment](tps-optimization-assessment.md) for the
results of the first private-network execution and the resulting optimization
priorities.
