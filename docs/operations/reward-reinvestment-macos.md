# Reward Reinvestment With kcli On macOS

This optional operator utility reinvests **95% of counted irreversible producer
rewards** into VHP through kcli. It is separate from the native Teleno binary.
It is not a wallet, an isolated signer, or a guarantee of growing VHP.

> **Mainnet mutation:** KOIN burns are irreversible. Installation and previews
> do not authorize execution. Confirm network, custody account, reserve, exact
> proposed amount and VHP beneficiary before approving any burn. Never use a
> producer hot key as a substitute for the account's custody key.

## What Is Included

The implementation lives in [`tools/reinvestment/`](../../tools/reinvestment/):

- `reinvest-teleno-rewards.cjs`: irreversible receipt accounting, explicit-amount
  burns, reserve/cap/minimum controls and persistent reconciliation.
- `reinvest-rpc.cjs`: bounded read-only JSON-RPC transport and pinned-head block batches.
- `reinvest-keychain.swift`: native Keychain import and noninteractive access.
- `reinvest-macos-runner.cjs`: private configuration and scheduler entrypoint.
- `install-reinvest-macos.py`: observation-only hourly LaunchAgent installer.
- `reinvest-teleno-rewards.test.cjs`: focused arithmetic and execution-gate tests.

No wallet, password, producer address, private state or host inventory is bundled.
Every operator supplies their own account with `--account`.

## Prerequisites

- macOS, Python 3, Node.js 22 and Swift command-line build tools.
- A trusted kcli checkout with `node_modules/koilib` installed, its CLI built,
  and `src/abis/pob.json` plus `src/abis/token.json` present. Follow that project's installation instructions.
- An encrypted custody wallet at `~/.kcli/wallet.json` matching the producer
  account. The wallet and password file must be owned by the current user and
  inaccessible to other users (0600).
- An unlocked user Keychain and a trusted mainnet RPC serving complete receipts.

Keep custody on the Mac rather than importing it into the producer host.
Koinos production needs liquid KOIN and Mana; a reserve is mandatory. The
[official production guide](https://docs.koinos.io/validators/guides/block-production/)
warns against burning the entire liquid balance. No numeric reserve in this
document is a validated minimum for an arbitrary producer.

## Install In Observation Mode

From the Teleno checkout, replace the placeholders with your private operator
values. Do not paste a password or WIF into any argument.

```bash
python3 tools/reinvestment/install-reinvest-macos.py \
  --account '<YOUR_MAINNET_PRODUCER_ADDRESS>' \
  --password-file '/absolute/path/to/private-wallet-password-file' \
  --kcli-root '/absolute/path/to/kcli-checkout' \
  --node '/absolute/path/to/node' \
  --kcli '/absolute/path/to/kcli'
```

This imports the existing password into the Keychain without exposing it in
arguments or environment variables, preserves the source password file,
initializes future-reward accounting, and installs `org.koinos.reinvest` with
`execute: false`. Existing installation/configuration is never overwritten.
Existing unresolved state or a conflicting Keychain item requires manual review.

Locations:

| Artifact | Location |
| --- | --- |
| Installed scripts/helper | `~/Library/Application Support/KoinosReinvest/releases/keychain-v1` |
| Private configuration | `~/Library/Application Support/KoinosReinvest/config.json` |
| Persistent state | `~/.kcli/teleno-reinvest/state.json` |
| LaunchAgent | `~/Library/LaunchAgents/org.koinos.reinvest.plist` |
| Private logs | `~/Library/Logs/KoinosReinvest/{stdout,stderr}.log` |

The agent runs every hour and on login/load. It only runs in the logged-in user
session, not while the Mac is off, asleep or logged out. It catches up from its
durable checkpoint on the next successful run. A locked or denied Keychain
fails closed without an unattended authorization dialog.

The installer references your existing Node/kcli paths rather than bundling
them. Preserve those dependencies; upgrading/removing them requires revalidation.

## Choose Limits And Preview

Edit only the limit fields in the private installed configuration. Example
values, not financial advice or guaranteed Mana sizing:

```json
{
  "execute": false,
  "reserve": "1000",
  "cap": "50",
  "minimum": "10"
}
```

Retain the installer's account, state and dependency fields. Reserve is a floor
for liquid KOIN; cap limits one transaction; minimum avoids tiny burns. Neither
reserve nor cap limits an attacker who obtains the custody key.

Run a preview through the installed entrypoint:

```bash
NODE='/absolute/path/to/node'
BASE="$HOME/Library/Application Support/KoinosReinvest"
"$NODE" "$BASE/releases/keychain-v1/reinvest-macos-runner.cjs" "$BASE/config.json"
```

Review account, irreversible height, counted rewards, prior burns, balance,
reserve and proposed burn. The original wallet balance and incoming transfers
are not counted as new production rewards. Developer rewards are excluded.

## Supervised Burn And Automation

Before automation, explicitly authorize one reviewed burn. Stop the agent while
testing so the timer cannot start a concurrent signing attempt:

```bash
launchctl bootout "gui/$(id -u)" "$HOME/Library/LaunchAgents/org.koinos.reinvest.plist"
```

Run the core script once with the reviewed account/limits and explicit execution:

```bash
BASE="$HOME/Library/Application Support/KoinosReinvest"
node "$BASE/releases/keychain-v1/reinvest-teleno-rewards.cjs" \
  --account '<YOUR_MAINNET_PRODUCER_ADDRESS>' \
  --kcli-root '/absolute/path/to/kcli-checkout' \
  --kcli '/absolute/path/to/kcli' \
  --reserve 1000 --cap 50 --min 10 --execute \
  --keychain-helper "$BASE/releases/keychain-v1/reinvest-keychain"
```

The pending attempt is persisted before kcli is invoked. After finality, run
the installed preview entrypoint to reconcile it. This checks irreversible
inclusion, receipt success, PoB burn amount, payer and VHP beneficiary. A kcli
exit code or inclusion message alone is not finality evidence.

Only after supervised verification and explicit authorization of recurring
burns, set `execute: true` in the private config and reload the agent:

```bash
launchctl bootstrap "gui/$(id -u)" "$HOME/Library/LaunchAgents/org.koinos.reinvest.plist"
launchctl print "gui/$(id -u)/org.koinos.reinvest"
```

## Accounting And Recovery

- Amounts use integer atomic units. The script compensates for current kcli's
  Number-based `--amount` conversion only when an exact round-trip is possible;
  otherwise it refuses. Small downward adjustments remain owed for future burns.
- State is private, atomically replaced and fsynced. A lock prevents concurrent
  runs. Back up state: deleting and recounting old rewards can burn twice.
- Only irreversible blocks are counted; RPC chain identity, checkpoint and
  block continuity are checked. This does not authenticate a malicious RPC.
- RPC reads send explicit JSON headers, use a 15-second request/body timeout,
  and attempt at most five times for network errors, non-JSON responses,
  HTTP 408/429 and server errors. Retries use exponential backoff plus jitter;
  Retry-After is respected up to 30 seconds. Permanent HTTP/authorization and
  JSON-RPC application errors fail closed rather than being retried.
- Reads are paced at least 500 ms apart and blocks are fetched in batches of
  20 against one sampled head ID. Exact batch coverage, per-block continuity,
  receipts and the persisted checkpoint remain validated. Each processed block
  is still checkpointed individually.
- Exhausted RPC retries stop the current run. The next hourly scan resumes
  from saved state. Scans are bounded at 10,000 blocks; incomplete scans never
  burn. No alternate RPC is silently selected.
- The retry transport allows only named read methods. It rejects transaction
  and block submission. kcli signing/submission remains separate and is never
  automatically retried by this transport.
- Pending attempts block automatic resubmission. A timeout, rejected transaction
  or missing transaction ID requires investigation, not clearing the pending
  state. Inspect canonical transactions and receipts from `scanFrom` onward.
- After a hard process kill, inspect a stale `.lock`/`.tmp` and confirm no active
  process owns it before removal. Never reset the accounting state blindly.
- Avoid concurrent manual custody transactions. Reserve checks cannot prevent
  another process spending after the check. The Mana preflight is a heuristic,
  not a guarantee of acceptance.

## Keychain Security Limitations

The helper requests no allow-all-applications ACL. The password never appears
in service logs. Keychain is storage/access control, **not** a policy-enforcing
signer: compromised same-user code may invoke the authorized helper.

Current kcli requires a regular password file. During signing, the script uses
a private random temporary directory/file (0700/0600), deletes it in `finally`,
and overwrites its captured password buffer. A hard kill may leave the temporary
copy. Inspect `koinos-reinvest-secret-*` in the user's temporary directory after
a crash; do not remove files needed by an active invocation. Deletion does not
guarantee secure erasure. Eliminating this bridge requires a pipe/stdin credential
interface in kcli. The original password file is not deleted by installation.

Logs are not rotated automatically. Monitor their size. There is no automatic
release updater, notification service or isolated signing backend.

## Validation Scope

```bash
node --test tools/reinvestment/reinvest-teleno-rewards.test.cjs tools/reinvestment/reinvest-rpc.test.cjs
KCLI_ROOT='/absolute/path/to/kcli-checkout' node --test tools/reinvestment/reinvest-rpc.test.cjs
python3 tools/reinvestment/install-reinvest-macos.py --help
swiftc tools/reinvestment/reinvest-keychain.swift -o /tmp/reinvest-keychain-check
```

The RPC/ABI repair was verified by a complete live read-only catch-up and amount
preview, including successful recovery from HTTP 429 rate limits. Balance reads
use kcli's bundled token ABI rather than the incompatible SDK convenience ABI.
The optional `KCLI_ROOT` test exercises the actual installed ABI without network
access; transport tests simulate HTML, 429, server errors, timeouts, permanent
failures, refusal of mutation methods and incomplete block batches.

The original local variant passed live read-only producer-receipt parsing,
Keychain access, custody-address verification and observation-mode LaunchAgent
runs. That evidence is not an end-to-end mainnet burn test of this generalized
distribution. Test signed execution, hard restart, dropped/reverted transactions
and recovery on a separately adapted testnet implementation before unattended
financial operation. This implementation deliberately pins mainnet identity.
