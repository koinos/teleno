# Teleno Conversation Memory

## Pool production moved to legacy after native heap-corruption abort (2026-10-22)

The operator explicitly approves the existing mainnet pool's signing-key
rotation to the selected legacy host's existing primary key and requests actual
production verification. One registration transaction is signed by the existing
pool owner, simulated without broadcast, then broadcast once. It is canonical
at height **40,429,208**, becomes irreversible and clears its H+20 activation
boundary before the dedicated legacy producer starts at **16:22:13 UTC**.
The owner and pool terms are reverified unchanged; there is no burn, transfer,
new pool, operational key generation or private-key copy. Both original legacy
producer instances and their shared runtime configuration remain unchanged.

The pinned existing legacy image is staged as a separate Compose deployment
with read-only existing key/config mounts, a missing-key startup guard and
`unless-stopped` restart policy. Its image tag is v1.3.1; the observed native
banner separately reports `v1.3.0 (12cc060)`. Do not conflate those identities.
The first new producer block **40,429,295**, logged at **16:23:25.962039 UTC**,
matches local and independent mainnet canonical IDs and the selected pool
signer. At the **16:26:48 UTC** audit, reference LIB is **40,429,296**. Five
blocks are logged by that checkpoint; only the separately checked block is
claimed irreversible. All readiness/identity checks pass with zero new-producer
errors or restarts. This short cutover observation is not long-duration or
nonempty-block workload qualification.

The retired native binary remains exactly
`0bd4c3666a93a3b43d7c4c7e72776d9d935eec1a33cdeec22572ac71022d9447`.
Its original process aborts on **2026-10-21 at 16:18 UTC**, with `SIGABRT` and
`corrupted double-linked list`. Apport retains no core dump, so there is no
crash stack and no established corrupting call site. Crash evidence and the
database remain preserved; no native repair, build, deployment or restart is
performed by this cutover. The retired signer remains inactive, with restart
and boot activation disabled; its former key is no longer the pool's registered
key. Do not reactivate it from the older commissioning instructions.

Private approved scope, transaction/receipt/finality, exact public-key and
runtime bindings, validated Compose document and operating instructions are
under `/Volumes/My Passport/fogata-legacy-cutover-preflight-20261022-72ji4m_r/`.
Crash investigation evidence is under
`/Volumes/My Passport/teleno-fogata-crash-debug-20261022-8xu4xhlu/`.
The canonical cutover result is `CUTOVER-RESULT.md` in the former directory;
it does not authorize restoring the old registration automatically. The native
handoff report is annotated with the later failure. Issue 9 and M1/LR2 gates
remain open; this action is separate from isolated M1 qualification.

## Native alpha published; issue 9 remains open (2026-10-19)

The operator authorizes publishing all accumulated native changes. Native
[1.3.0-alpha.1](https://github.com/koinos/teleno/releases/tag/teleno-node-v1.3.0-alpha.1) is published from `2380fcb545ed056a548adbcd5e1cffcefe61ecc0` after 64/64 normal Linux tests,
version/help and exact-binary container verification. The startup syscall buffer
restoration found by the compatibility regression is included; original failures
remain retained. The published alpha and the running measured producer have
distinct identities. No VPS deployment or producer restart occurs. `main` remains
unchanged; LR2 continues at `1.3.0-dev.1`. See the
[publication qualification](validation/alpha-1.3.0-publication.md).

## Continued own-block observation completed (2026-10-18)

After the finite repair goal is complete, the operator requests extending
observation until an own produced block is detected. Continue the same qualified
mainnet producer in ordinary operation; do not reset the closed one-hour test,
its deadlines or its build/start counters. The new observation has no fixed
block-wait deadline and completes only after independent canonical inclusion
and irreversibility are proved. Preserve resource and recovery requirements.
There are no new producer starts, rebuilds, financial operations or keys.

The first fresh live audits confirm the same process and binary, one database
owner, healthy resource margins, advancing canonical history and zero own blocks
through the 18:18 UTC probe. Ten-minute thread checks continue until the same
producer generates block **40,323,375** at **21:35:48.815248 UTC**. Its ID is
`0x122040a9d21cb3eacc82c227de8bdd2a1cea5c966f761414038b0793d8b2b4295d34`.
Direct local and independent mainnet canonical IDs and selected-pool headers
match. The complete proof at **21:40:57 UTC** has independent LIB **40,323,411**,
so the own block is irreversible. It contains zero transactions. The heartbeat
is verified **paused** after success; the same producer stays active with no new
start/restart, native build/configuration or financial operation.

The first own-block probe exposes a private monitor format error: its signer
decoder expects Base64 instead of the actual Base58 address. Preserve the
original script/error, direct header/receipt review and scoped decoder correction;
the corrected complete read-only probe passes. No native implementation or
verification requirement changes. Twenty-one complete samples retain healthy
resources/peers and zero native producer errors. Private control/probe/results are selected by
`evidence/continued-production-current.json` in the existing owned repair run.
The observation amendment is recorded in
`docs/implementation/mainnet-producer-commissioning-goal.md`.
Own-block proof, completion result and paused-heartbeat verification remain in
that separate observation directory. Issue 9 is still open for its other gates.
Keep the completed result below historical; its former no-extension instruction
does not cancel the user's subsequent observation request.

## Finite producer repair completed; repaired producer in ordinary operation (2026-10-18)

The operator explicitly authorizes source repair, recompilation, deployment and
one new finite mainnet commissioning run, then supplies its full objective.
The prior permission blocker is resolved. The scoped repair and finite
commissioning pass; do not begin another observation or restart its clock.
The native goal is marked **complete** after the final evidence audit.
Follow `docs/implementation/mainnet-producer-commissioning-goal.md`.
Owned run: `/Volumes/My Passport/teleno-producer-repair-20261018-flnbdgu4/`.
Development deadline remains **17:57:49 UTC**, overall deadline **19:57:49 UTC**;
do not reset them. The prior failed attempt retains its own one start/deadlines.

The original producer library reproduces the unknown-parent failure with a real
mempool. The repair classifies only that lookup failure/observed head changes as
10-ms candidate deferrals, invalidates a failed cached replacement, and exposes
the persistent fault after 100 consecutive deferrals. Other exceptions propagate.
New log counters distinguish preparation, PoB evaluation, handoffs and errors.

One Linux candidate is compiled; all **63 distinct checks** and CLI pass after
retained environmental failures are resolved without changing source/reserves.
Source manifest: `775bb4cbae8462cea397c4fd1e9248279bc64c0fa97a82931e2d4c16bf31385b`.
Executable: `0bd4c3666a93a3b43d7c4c7e72776d9d935eec1a33cdeec22572ac71022d9447`.
The exact-source/link audit covers 312 inputs and 168 actual archives.

The new VPS run starts at **16:53:49.891 UTC**, with preparation deadline
**17:08:49.891 UTC** and hard stop **18:53:49.891 UTC**. Candidate observer
preflight/readiness, independent canonical LIB agreement and pool/key/VHP gates
pass. Its one producer start completes **3,600.00018 seconds** from
**16:57:11.701 UTC** to the immutable **17:57:11.701 UTC** endpoint. All 31 logged
handoffs recover in one retry, at most **36.628 ms**. Last periodic counters are
1,178 preparations, 303,541 PoB evaluations and zero errors; these are activity
lower bounds, not produced-block totals. Raw-window audit confirms **zero
produced blocks**, so canonical production remains unproved and issue 9 stays open.

Peak native RSS is **223,084,544 bytes**, minimum concurrent host MemAvailable
**3,230,367,744 bytes**, maximum whole-server accounting with the held 8-GiB
reserve **38,186,901,504 bytes**. Mean CPU is 18.879% of one core; kernel process
I/O increases by 56,418,304 read / 57,516,032 written bytes, and successful local
head RPC p95 is 3.405 ms. No OOM/swap/resource failure occurs. All 230 other
warnings concern failed outgoing connections or stale-peer protocol negotiation;
they remain visible. Steady peers stay at least three and finish at four.

Current read-only handle helper: `evidence/read-finite-progress.py` in the owned
run. Controller/unit/plan paths and access bindings remain private. The old
failed-run helper/control are historical and must not override current actors.
The repaired producer remains active as ordinary operation with the same PID;
the controller has exited and its timer is cancelled after terminal qualification.
The original observer is stopped but preserved for rollback. `Restart=no` and
boot activation stay disabled. Final read-only audits reverify the actual running
binary, sole database owner, five unchanged runtime libraries, configurations,
historical canonical receipt samples, pool/settings/registration and positive VHP.
The captured final native head **40,319,079** and full state root match the
independent irreversible chain through successor **40,319,080**, with reference
LIB **40,319,242**. This endpoint proof does not extend the observation or prove
an own produced block.
The pool retains 1,000 VHP and the approved 900-KOIN reserve; owner KOIN remains
100. No additional funding/manual burn, operational key/account or email occurs.

Authoritative private results: `terminal-native-evidence/result.json`,
`terminal-native-measurements.json`, `terminal-native-evidence-retention-proof.json`,
`other-native-warnings-reviewed.json`, `completion-finalization-verified.json`,
`final-captured-head-canonical-state-root-proof.json`, post-window ordinary
producer health, final runtime/pool read-only audits and `OPERATOR-RESULT.md`. Public details are
in `docs/validation/native-producer-handoff.md`. Keep exact deployment commands
and operational bindings private. No new integration commit or Koinos One native
package/submodule update is created by this repair.

Inactive prior generated build/test files are temporarily relocated only after
complete external byte verification to accommodate unchanged standard-reserve
fixtures. The retention/retirement proofs and complete archives are in the new
owned run; source, current link libraries and the original native binary remain
untouched. Completed synthetic data is externally preserved and retired. All 29
unchanged test executables and 8,308 predecessor-build members are now restored
to their original paths with full byte/mode/link verification. Both
`original-generated-*-restored.json` proofs are in the owned run; no restoration
remains pending.

## Historical bounded producer-failure diagnosis; authorization subsequently granted (2026-10-18)

The failed commissioning attempt stays terminal and its original deadlines/start
counter are preserved. A separate **30.0003-second read-only observer probe** makes
**300** explicit-parent mempool requests across **13** heads without reproducing
the exception. The same observer process remains active and production stays off.
This is diagnosis only, not a new producer observation or successful qualification.

The source publishes the chain head before the mempool's block-accepted handler
creates the associated transaction view. That possible handoff window, the named
mempool handlers and the producer source match repository HEAD. Warning-time
head/mempool IDs remain missing, so exact causation and legacy behavior are still
unproven. Each saved warning is followed by an auxiliary refresh about five seconds
later; retries are confirmed, successful candidate assembly is not.

At this earlier checkpoint, the explicit authorization question for source repair,
an identified build and a new finite commissioning scope was unanswered. The
operator's question "Hay un bug?" alone was not approval. The later explicit
authorization and separate amended run above supersede that permission state;
the failed run's clock must still never be reset. No new financial operation,
pool deployment, wallet/key generation or email is needed.

New private evidence under the existing owned commissioning run:
`producer-warning-retry-correlation.json`,
`head-mempool-source-baseline-audit-v2.json` and
`read-only-head-mempool-probe-20261018T155204Z.json`. The v2 audit supersedes an
incorrectly broad initial source-comparison span. The detailed failed-attempt
report includes the reviewable next repair scope and these limits.

## Finite producer attempt stops on recurring mempool lookup failures (2026-10-18)

The initial **1,000-KOIN burn/Fogata stake is canonical and irreversible**.
Post-attempt read-only mainnet checks still show **1,000 VHP credited to the
owner's pool stake, 900 KOIN reserve and 100 KOIN in the owner account**.

The exact existing Linux candidate passes observer readiness, independent
canonical LIB comparison, matching hot key/registration and positive-VHP checks.
Production is deliberately activated once. Native key loading and explicit
restore-hold release pass. The owner custody key stays off the VPS. A future-start
key guard checks the existing hot key's permissions/public bytes and prevents
native auto-generation if that key is missing or mismatched; no restart is used
to install this guard.

Qualification stops after **12** recurring native producer exceptions:
`cannot retrieve pending transactions from an unknown block`. The last producer
resource sample is at **1,253.23 seconds**, about 21 minutes, with zero produced
blocks. Native high-water RSS is **159,924,224 bytes**, minimum host MemAvailable
**3,301,134,336 bytes**, and minimum strict disk margin including reserve
**1,869,701,120 bytes**. The failure is the unresolved producer/mempool path,
not a breached RAM/disk guard. The one-hour producer gate and issue 9 remain open.

Both controller and producer are stopped. The original observer is restarted
and verified: correct mainnet identity, advancing head/LIB, three peers,
**76,464,128 bytes RSS** and **3,410,071,552 bytes host MemAvailable**. The producer
service is inactive; the deadline watchdog is stopped. One producer start and
the original deadlines remain recorded. Do not resume/reset this failed attempt
or rebuild under its no-automatic-rebuild scope. No producer was enabled for boot.

The likely head-publication/mempool-fork handoff race is an **unproven hypothesis**;
warning-time IDs were not captured. Do not blame the reserve/Fogata funding,
claim legacy is unaffected, or claim successful canonical production. The exact
source path and concrete proposed repair are documented in
`docs/performance/mainnet-producer-commissioning-2026-10-18.md`.

Private evidence: `native-failed-attempt-evidence/failed-attempt-summary.json`,
`observer-rollback-verified.json`, `native-evidence-archive-verified.json`,
`post-native-attempt-pool-balances-reverified.json` and `commissioning-control.json`.
The lossless My Passport evidence archive contains raw logs/samples and no private
key file. Diagnostic parser, transient post-stop port-probe and archive extraction
corrections neither reset the test nor repeat a producer start or transaction.

## Exact initial Fogata burn/stake verified; finite native commissioning begins (2026-10-18)

The operator explicitly requests a **1,000-KOIN burn** from the original funding
and confirms reducing the pool reserve after its purpose and the resulting
allocation are explained. Reuse the existing owner vault and pool; no other
wallet, owner generation, email, pool deployment or hot-key registration occurs.

An exact four-operation atomic transaction removes 1,000 KOIN of the owner's
reserve, approves the same PoB burn and VHP transfer, and deposits through Fogata.
Owner-only signing and nonbroadcast simulation pass. Simulation and the actual
receipt use **3.19472266 Mana**. A single broadcast is independently verified in
canonical irreversible block **40,315,460**. Exact burn/transfer events and live
balances prove **1,000 VHP credited to that owner's pool stake, 900 KOIN reserve
and 100 KOIN in the owner account**. This supersedes the initial-VHP blocker below.
The 900-KOIN reserve is an approved allocation, not measured payout sufficiency.

The finite native commissioning clock begins at **14:52:33 UTC**, with preparation
ending by **15:07:33 UTC** and a hard stop at **16:52:33 UTC**. Preserve these limits;
do not reset or extend them. Read-only live VPS preflight passes: no native owner,
the exact existing ELF/configuration, 3,463,561,216 bytes host MemAvailable and
1,928,228,864 bytes strict 40-decimal-GB margin with the 8-GiB reserve. The reviewed
private configuration, separate matching hot key and services are installed,
with automatic service restarts disabled. The owner custody key stays off the
VPS. The original observer configuration and restore hold remain unchanged.
Observer readiness and explicit hold release still gate production.

Authoritative private evidence: `initial-burn-operator-authorization.json`,
`initial-burn-unsigned-review.json`, `initial-burn-simulation-pass.json`,
`initial-burn-broadcast-intent.json`, `initial-burn-canonical-block.json`,
`initial-burn-mainnet-verified.json`, `native-commissioning-reviewed-plan.json`,
`native-preflight-live.json`, `native-activation-prerequisite-reads.json`,
`native-install-private-verified.json` and `commissioning-control.json` in the
existing owned run. The remaining native result must be refreshed live.

## Authorized Fogata pool deployed once and verified irreversible (2026-10-18)

The operator explicitly confirms the proposed terms and says to launch the
pool. That clears the fee/period/reserve blocker; do not request those approvals
again. Name, beneficiary and exact description were already confirmed. The
existing owner vault is unlocked privately without redisplaying secrets,
creating another owner, reading the deleted email or resending anything.

The exact eight-operation deployment, signed by the owner and the distinct
contract key, passes nonbroadcast simulation: **88.86588109 Mana**, approved
**1,900 KOIN** reserve transfer, **zero KOIN burned**. A fresh preflight proves
simulation changed no balances/code and the nonce remains current. One and
only one broadcast succeeds. Canonical block **40,315,070** is verified as an
ancestor of the current head and below LIB, with the intended successful
transaction receipt. Its owner, 1% beneficiary fee, four-day period, exact
description, 1,900-KOIN owner-attributed reserve, registered hot-key bytes,
accepted 2.0.0 WASM hash and registry entry all pass. Owner retains **100 KOIN**.

The public Fogata detail page is visibly verified in Chrome, with the expected
name/description/fee/period and **Paused, 0 VHP, zero produced blocks**. The
displayed annual-yield estimate is not evidence of this pool's performance.
The result tab is marked deliverable. Actual account/contract/transaction
bindings stay in the private run, not this public source.

The first final-report construction encounters omitted zero-valued VHP after
canonical proof is already saved. Correcting the Protobuf-default handling
resumes from that saved block with a fresh canonical/LIB check. No broadcast,
key generation, observation window or deadline is repeated/reset.

Authoritative private evidence: `pool-launch-operator-authorization.json`,
`pool-deployment-simulation-pass.json`, `pool-authorized-signature-verification.json`,
`pool-broadcast-intent.json`, `pool-broadcast-response.json`,
`pool-canonical-deployment-block.json`, `pool-canonical-finality-observations.json`,
`pool-canonical-resume-check.json`, `pool-mainnet-deployment-verified.json`,
`pool-web-visible-verification.json` and `commissioning-control.json`.

The complete goal remains unfinished: **positive initial VHP/source is still
unspecified**, and native observer/producer commissioning has not started.
Do not burn the remaining KOIN or use another producer's wallet to fill that
gap. The hot key/config remain locally staged; no VPS service/config mutation
or producer start occurs. A fresh asynchronous question requests exact initial
VHP amount/source or separately authorized conversion. The resumed blocking
audit is fresh; pool deployment is real progress. Do not create an indefinite
funding/monitoring/preparation loop while awaiting that input.

## Fogata unsigned deployment and private producer configuration prepared (2026-10-18)

The operator confirms the pool name, beneficiary address and exact description;
the actual values remain only in private evidence. The proposed economic terms
are **1% beneficiary fee, four-day period, 1,900 KOIN pool reserve and 100 KOIN
retained by the owner**. They remain a proposal, not approved spending or a
measured Mana requirement. Positive initial VHP/source is still unspecified;
no KOIN conversion is authorized.

Read-only mainnet registry inspection accepts the captured WASM hash as Fogata
**2.0.0**. Separate local producer-hot and contract-deployment keys are created
once under the private run, WIF/public/address round trips pass, and owner,
contract and hot identities are distinct. Keys are mode 0600 in an owned 0700
directory; the owner wallet is neither regenerated nor placed on the VPS.
Nothing is emailed again. Neither new key is installed or registered remotely.

The proposed transaction contains the exact eight operations from the reviewed
Koinscan flow: upload, listing, owner, parameters, initial snapshot, reserve
allowance, reserve funding and producer-key registration. It has **zero
signatures**. All seven call operations decode to their intended arguments;
the installed kcli PoB ABI independently decodes the registration. Standard
Base64 native public-key text must be normalized to Base64url for koilib's
wire field, preserving the same 33 bytes. Empty image omission is the declared
Protobuf default. Earlier data-literal/encoding refusals occur before signing;
the same key files are reused, not replaced.

The proposed contract address has no code and zero KOIN/VHP at the bounded
read-only check. Recheck that condition immediately before any deployment;
the initial snapshot must not be allowed to consume pre-existing assets.
The private producer YAML/diff is staged against the saved hash-identical
observer configuration. No runtime config, service or database changes.

Use `fogata-deployment.proposed-r2.review.json`, its `.unsigned.json` and
`.validation.json`, `producer-runtime.proposed-r2.yml`/`.diff`,
`fogata-registry-input-readonly.json`, `offline-preparation-corrections.json`
and `commissioning-control.json` in the existing private run. The draft's
100-Mana RC ceiling is provisional, **not a simulated cost**. Refresh nonce,
review the exact final transaction and prove Mana before signing. Native C++
key loading, live observer readiness and producer qualification remain open.
Zero submissions/producer starts; commissioning deadlines have not started.
Await the remaining economic terms and VHP input; do not create an indefinite
preparation/funding loop or mark wallet/draft evidence as deployed production.

## Owner funding verified; recovery email moved to Trash on request (2026-10-18)

The operator reports transferring 2,000 KOIN. An explicit-mainnet chain-ID
check and installed-kcli 1.7.1 explicit-owner read now verify 2,000 KOIN,
2,000 Mana and zero owner VHP. The earlier HTTP 403 is superseded; it never
proved zero balance. The incoming transaction's identity/finality is not yet
verified. Pool terms, initial VHP and exact deployment Mana remain pending;
the observed website defaults do not authorize spending or burning.

The operator then explicitly requests deletion of the recovery email. The
authenticated mailbox and exact message metadata are reverified without
reading its secret body. Gmail deletion succeeds; minimal readback confirms
Trash/no Inbox, and the exact message search outside Trash returns no matches.
This is recoverable deletion, not permanent erasure. Keep the original verified
delivery as historical evidence, preserve the encrypted owner vault and do not
resend the email. No owner key is regenerated or changed.

The Fogata-amended goal is actually registered and read back; the earlier
objective-edit limitation is superseded. Nine Koinscan source/ABI/bytecode
inputs are captured from commit `6ef27ffb7fc19d51fc37729362c7103b942c479c`,
with Git blob and SHA-256 verification. The WASM is 112,895 bytes, SHA-256
`0ed58d477b0860e1f61960c7566a6b72663cf6eca53bca00f32c02ed8b63fdc4`.
This does not establish deployed frontend equivalence or contract acceptance.
The guide's separate Fogata contract-source repository/pinned tree returns
HTTP 404 through GitHub API; do not claim a completed contract-source audit.

Private evidence remains in the existing commissioning run:
`owner-funding-live-20261018T135813Z.json`,
`owner-recovery-email-trash-verified.json`,
`fogata-goal-registration-readback.json`, `fogata-source-inputs-pinned.json`
and `commissioning-control.json`. Commissioning has not started: zero producer
starts and zero pool-deployment submissions. No transaction is signed/submitted,
no VHP is converted, and no VPS config/service is changed.

## Goal amended for Fogata; new kcli owner account and recovery email verified (2026-10-18)

The operator explicitly adds **a new kcli account, recovery-secret delivery to
their own mailbox and a Fogata mainnet pool**, then confirms that they will fund
the new account. The earlier existing-producer-address/hot-key-location questions
are superseded. The current goal contract is
`docs/implementation/mainnet-producer-commissioning-goal.md`; its 120-minute
commissioning, 60-minute producer observation and maximum two producer starts
remain. No funding transfer from an existing account or unspecified burn is
authorized. Pool name, beneficiaries/operator commission, payout period and KOIN
reserve remain operator choices; four days/2,000 KOIN are only observed UI defaults.

The installed **kcli 1.7.1**, independent of the older dirty source checkout,
actually creates the owner account once. Its encrypted named vault is mode
0600 and key/address/decryption round trips pass. The original generated packet
is recovered/reverified after a JSON framing refusal; **no second account is
generated**. The requested recovery email is sent to the independently verified
operator mailbox, and full sent-message readback verifies address, WIF, vault
password and the encrypted attachment. Default wallet/configuration metadata
remain unchanged. Exact account/mail bindings and vault path are only in private
owned evidence, including `new-owner-wallet-creation-private.json`,
`owner-wallet-email-redacted-receipt.json` and the decoder correction receipt.
Do not generate another owner account or resend the email without a reason.

Private owned evidence remains
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/mainnet-producer-commissioning-20261018-8vhde0kn/`.
The owner/custody key must not become the VPS hot key. The Fogata contract will
have a distinct pool address, which becomes the producer target after reviewed
deployment/registration. No real transaction is signed/submitted, no pool is
deployed and production is not activated. The operator will supply KOIN; continue
only with confirmed pool terms, verified funding/Mana and a reviewable exact plan.

## Finite mainnet producer goal registered; account/key pending (2026-10-18)

The operator explicitly requests configuring a producer on this node in
**mainnet** and creating a clear, finite goal without indefinite loops. The
goal is actually registered; its concrete contract is
`docs/implementation/mainnet-producer-commissioning-goal.md`. Limits after
prerequisites: **120 minutes maximum**, split 15-minute preparation,
45-minute observer readiness and 60-minute producer observation; **two producer
starts maximum**. Persist deadlines/counters; no automatic extension, funding,
rebuild/restore or optimization loop. Canonical production must be independently
verified if observed; absence within the fixed window remains explicitly
unproved, not an indefinite wait.

Initial read-only access to the designated VPS verifies the same ELF/config,
2 CPUs, 4,000,034,816-byte RAM, MemAvailable 3,427,299,328 bytes and zero
swap/OOM counters. No native/probe process runs. Observer recovery hold is
present and the test basedir has no producer private key. No service/config
mutation or secret-key read occurs. An asynchronous question requests the
operator's intended public mainnet producer account; do not guess from another
producer or silently provision an unrelated new key. Registration/key/VHP and
live observer readiness remain unverified, so production is not activated.

Private evidence is under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/mainnet-producer-commissioning-20261018-8vhde0kn/`.
The exact public account binding and hot-key path stay outside public source.
The 8-GiB issue-9 amendment remains in force. No burn, transfer, registration
change, unrelated producer cutover, commit/release or app integration is scoped.

## Original issue 9 now requires an 8-GiB reserve (2026-10-18 13:11 UTC)

The operator explicitly requests changing **original GitHub issue 9**: the
recovery-reserve requirement is now **8 GiB (8,589,934,592 bytes)**, retaining
`max(8 GiB, database bytes / 5)` inside the unchanged 40-decimal-GB whole-server
budget. The issue body is actually edited and fetched again; exact body equality,
unchanged title and OPEN state are verified at **13:11:06 UTC**. Receipt:
`github-publication-receipt.json` under the owned My Passport evidence directory
`teleno-issue9-20261015-krvvxm54/evidence/issue9-8gib-requirement-20261018-7wtdrudk/`.

The completed exact-source 8-GiB VPS results now count toward original issue 9.
The selected-input restore checkbox is checked with its measured scope:
37,915,475,968-byte restore peak, 39,352,967,168-byte retained-reader compaction
peak, and all 45,076,063 original block/receipt records unchanged. No additional
1.50 GB saving is required merely to emulate the former 10-GiB criterion.
Remaining aggregate-memory, integrated diagnostics, historical execution and
representative operational producer gates stay open. Five-hour evidence is
retained; no longer duration is newly completed or claimed.

Native goal/plan/matrix and finite-experiment reports, plus the two scoped
Koinos One documents, are coordinated with the amendment. This is a requirement
and documentation change, not a native code/default change: the standard CLI
still uses 10 GiB and the verified candidate explicitly selects
`--backup-restore-reserve-profile experimental-8gib`. No server operation,
production activation, source/binary change, commit, release or app integration
is performed. Earlier references below to an unchanged original issue-9
10-GiB contract are historical and superseded by this explicit amendment;
their original measurements and failed outcomes remain preserved.

## Experimental 8-GiB VPS goal complete; all processes closed (2026-10-18 13:02 UTC)

The final independent requirement audit passes all eight finite experimental
requirements with **no unmet finite requirement**. Evidence:
`final-experimental-qualification-audit.json`. This closes only the explicitly
amended **five-hour, selected-backup, bounded-workload 8-GiB experiment**.
Original issue-9/default-10-GiB and broader LR2 qualification remain open.

The final read-only archive verification actually completes in **1,765.567 s**:
all **45,076,063 original block/receipt records are byte-identical**, including
the entire frozen prefix and all 75 tail records. The current archive contains
**45,153,063 records**, including **77,000 new records** from synchronization.
No tail-root completion or other original-record change occurs. All 21 closed
control members are archived and independently verified in
`final-selected-archive-retention-summary.json` and its preservation archive.
The archive service is closed/collected, MainPID 0 and exit 0. Do not rerun the
scan or its exclusive-create preservation helper.

Across the completed application cases: peak native RSS **745,668,608 bytes**,
maximum measured steady observer RSS **635,420,672 bytes**, minimum concurrent
host MemAvailable **2,760,302,592 bytes**, and whole-server peak including the
full 8-GiB free reserve **39,352,967,168 bytes**. The smallest margin below
40 decimal GB is **647,032,832 bytes**; no OOM/swap occurs. The main observation
lasts **18,007.577 s**, with **9,360 successful steady reads / zero failures**;
the clean reopen and controlled abrupt recovery also pass. Real retained-reader
compaction and bounded isolated producer/create/replay evidence remain distinct
from operational mainnet production. All CPU/I/O/latency counters and limits
are retained in the final report and resource ledger.

`final-vps-host-and-runtime-identity.json` freshly verifies the unchanged actual
2-vCPU/4-decimal-GB/ext4 host, executable/configuration and five runtime libraries;
**no native or probe owner remains**, observer recovery hold is present and
producer key is absent. The node is deliberately left stopped after the finite
qualification. No restore, signing, production activation or service change is
pending. Source manifest remains
`6456879f2162baf57d6b932054bf92e19569fb8e47c89d109fbd35e11a964ec5`;
ELF remains `f086e2d71c04cc0922ccd497a2cb1c63d5265bc1d3190b2e17e1156b5a7d10f4`.
All 312 source inputs, 168 link archives and 63 original Linux test reports are
verified. Do not rebuild or rerun successful cases without a new reason.

Owned evidence remains under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/vps-8gib-goal-20261018-72a46f/`.
English native/implementation/LR2 reports and the two scoped Koinos One documents
are updated. Its actual strict MkDocs build and rendered qualification text
pass; unrelated changes and submodule are preserved, as proved by
`koinos-one-final-documentation-verification.json`. No commit, push, release,
GUI/package integration or default-reserve change occurs. The reserve flag
saves **zero physical data bytes**; larger/concurrent compactions, future growth,
historical execution, longer operation and mainnet producer qualification are
not proved. Earlier pending checkpoints below are historical, not current state.

## Five hours/reopen preserved; final canonical proof passes; entire archive scan live (2026-10-18 12:29 UTC)

This goal turn makes substantive progress. The original native completes
**18,007.577162623 seconds** and exits **0**. All raw resource/read/head checks
independently pass: **9,360 steady reads / zero failures**, peak RSS
**635,420,672 bytes**, minimum host MemAvailable **2,769,223,680 bytes**, minimum
strict disk margin **1,833,799,680 bytes**, OOM/swap zero. CPU **3,252.18 s**,
maximum DB/WAL allocations **24,288,677,888 / 82,173,952 bytes**. The original
86,400-second harness's sole duration refusal is preserved verbatim, not
rewritten as a 24-hour pass. Original day/workflow units are actually closed.

The corrected controller automatically runs the same 300-second reopen using
native **459315 / 24515033**, invocation `2cf4db53e51641109e8ffaaf29153080`.
It completes **300.706915711 seconds**, native exit **0**, catch-up **10.647 s**,
**168 steady reads / zero failures**, peak RSS **115,625,984 bytes**, minimum
host MemAvailable **3,315,453,952**, strict margin **1,867,513,856**, no OOM/swap.
Two expected startup connection failures remain in the complete counters.
The R2 terminal is `all-amended-five-hour-phases-passed-awaiting-final-review`;
actual R2/post units are now collected, inactive, MainPID 0 and exit 0. Live
mapped-library/config/source/PID proof is captured during the reopen. Both local
waiters **28444 and 2299 are complete/closed**. No local session remains pending.

Strict closed-gate preservation actually executes and verifies every archive
member: original day **10**, corrected controller **9**, superseded controller
**2**, and clean reopen **10**. Evidence: `five-hour-closed-proof.json`,
`vps8-five-hour-summary.json`, `five-hour-day-closed-preliminary-review.json`
and their `actual-vps-five-hour-*`/post-observation archives. Do not rerun these
preservers or the observation. The 27-check original-process waiter history is
durable in `verified-five-hour-wait-20261018T1153Z.jsonl` and its summary.

Final legacy proof **passes** for native height **40,312,594**, ID
`0x12203abc2bd1eb46eff6175ee248e29f35ec19b318a37233c537f6bcbcc73ac869fa`,
full state root
`0x12205549677cf5e17d1bc9f24f5f647ec776e2ba16bba69acc606caea0445cb189af`.
Actual decoded block ID/timestamp, mainnet chain ID, canonical successor
40,312,595 parent and previous-state root are independently compared. Reference
LIB **40,312,601** makes it stable; all three reference images are verified
running/identical before and after RPCs. `final-head-legacy-proof.json` and
`final-head-legacy-raw-review.json` retain the evidence. Its local SSH session
**90652 is complete/closed**. Do not replay it merely because time passes.

**Required final all-selected-record scan is currently live** on the same VPS:
unit `teleno-vps8-archive-retention-verify-r1.service`, invocation
`593916c3e4614157bff1ce0c3c539ca2`; controller **460922 / 24563843**, actual
native archive probe **460926 / 24563892**, executable
`probes/archive_retention_probe`, SHA
`263bb8ca59b2be8cdeb088e8ab7bc5b0aa6080bd11eabb67fa4f2453bc717185`.
It owns the original storage/qualification gates; node processes are closed.
No new restore/database copy or native implementation change occurs. Limits:
1 CPU, 1 GiB, swap 0; 40-minute controller/42-minute unit bounds, original
half-second host/disk/observer-hold guards. Do not restart/re-dispatch it based
on an observation timeout. At **12:29:35 UTC**, actual PID/start/cgroup are
freshly verified live: **315.135 s**, **7,340,032 records scanned**, HWM
**33,710,080 bytes**, minimum MemAvailable **3,426,619,392**, strict margin
**1,937,727,488**, no OOM/swap.

Latest refresh **12:32:58 UTC** confirms the same actual archive controller/probe
live: **518.222 s**, **12,582,912 records scanned**, HWM **34,971,648 bytes**,
minimum MemAvailable **3,426,619,392**, strict margin **1,937,715,200**, no
OOM/swap or review flag. No local tool session is pending; only the verified
remote scan is active. The native application remains closed as required for
this final read-only archive verification.

Use **`read-final-archive-checkpoint.py`** for a fresh concise actual-process
check. The existing full `read-vps8-progress.py --brief` now includes actual
archive-retention units/PIDs as well as files; it never starts/stops work.
No continuous local archive watcher is active. Once the actual scan passes and
its unit is closed, execute **`preserve-final-archive-retention.py`** exactly
once. It independently checks all **45,075,988 frozen records**, exact prefix
SHA `a7e54dcdcf1b62c5518654fc544225dd519bd709b1831aa5242ad9673eb14e40`,
all **75** selected tail records, permitted missing-root completions only,
resource limits and closed actual native processes, then archives/verifies all
controls. This helper is prepared/AST-checked, not yet executed; do not credit
its result in advance. Source/backup genesis content lineage is already proved
in `selected-backup-genesis-continuity-lineage.json` and remains input evidence,
not older-binary runtime qualification.

After final retention: complete the requirement/resource/CPU/I/O/latency/source/
dependency audit, fresh final physical host/spec/budget checks and English docs.
All 312 local/builder source inputs and 168 link archives are independently
identified in `preclosure-source-build-dependency-audit.json`; final native
source/ELF/config remain frozen. Native `--version`/`--help` and all 63 Linux
tests already pass; do not rerun them without a new change/failure. Finish the
Koinos One two-document handoff only with actual final outcomes; its complete
AGENTS and tracked changes are read this turn. Available pre-existing MkDocs:
**`/usr/local/bin/mkdocs`**, Python 3.9. Use direct `mkdocs build --strict` with
a fresh owned external `--site-dir` (for example `koinos-one-manual-site-final`
under this evidence root). Keep directory URLs disabled, old site preserved,
and avoid `npm run docs:build` because it also syncs a changelog not being edited.
Check the full app worktree before editing and preserve its unrelated changes.
Do not change GUI, package, submodule, commits/pushes/releases or mainnet signing.
Original issue-9/default-10-GiB and wider LR2 remain open even if this finite
five-hour, selected-backup, bounded-compaction/isolated-producer experiment passes.
Only complete this goal after every finite experimental requirement is proved.

## Current source/build/dependency identity verified; durable waiter live (2026-10-18 11:53 UTC)

This goal turn makes concrete provenance progress. The read-only audit verifies
all **312 local and Linux-builder source inputs**, source archive, exact ELF,
CMake home/cache/compile/link inputs, compiler/profile and dependency-source
archives. It hashes **168 resolved link archives** and records Ubuntu GCC
**13.3.0**. Evidence: `preclosure-source-build-dependency-audit.json`; the
reusable diagnostic is `audit-final-source-build-dependencies.py`. No source,
binary, build or test rerun occurs. A relative-link-path diagnostic refusal is
preserved separately as `preclosure-source-dependency-r0-refusal.json`; resolving
paths from the actual CMake target build directory fixes that diagnostic only.

Local read-only waiter **PTY session 28444 is live**, running
`wait-original-five-hour-observer.py`. Poll that same session; do not start a
second reader while it is running. Each completed actual process check is now
flushed/fsynced immediately to `verified-five-hour-wait-20261018T1153Z.jsonl`,
so interrupting a local observation cannot lose previous checks. It never
signals, restarts or dispatches remote native work. It exits on the first remote
phase change/review condition, or its bounded local observation deadline.

Its first actual check at **11:53:17 UTC** sees the unchanged native/controller,
native elapsed **16,778.303 s**, remaining **1,221.697 s**, head **40,312,066**,
**8,622 steady reads / zero failures**. Peak RSS **619,962,368 bytes**, minimum
host MemAvailable **2,786,824,192 bytes**, strict margin **1,833,799,680 bytes**,
no OOM/swap. The five-hour cutoff remains approximately **12:13:39 UTC**, then
the corrected controller automatically waits/cools down and performs the
300-second clean reopen. Final retention/canonical/source/resource review is
still required; the goal remains active.

Koinos One's complete AGENTS and current tracked worktree are read again before
the pending final documentation handoff. Six pre-existing tracked modifications
are preserved. Its current experimental-reserve paragraphs still correctly say
qualification is active; update them only after final outcomes exist, without
app/native packaging or submodule changes.

## Five-hour observation still live after separate operator request (2026-10-18 11:48 UTC)

This goal turn is a verified wait: actual native/controller units and exact
PID/start identities are repeatedly checked live. At **11:48:21 UTC**, native
**414144 / 22700028** and corrected controller **438740 / 23880217** remain
unchanged. Native elapsed **16,480.923 s**, remaining **1,519.077 s**; head
**40,311,952**, age **22.158 s**; **8,442 steady reads / zero failures**. Peak
RSS **619,962,368 bytes**, minimum host MemAvailable **2,786,824,192 bytes**,
minimum strict disk margin **1,833,799,680 bytes**, no OOM/swap. The five-hour
cutoff remains approximately **12:13:39 UTC**, followed by automatic cooldown
and the 300-second reopen. Final archive retention and final review are pending.

Local read-only observation cell **3046 is terminated** solely to handle a
separately authorized operator-access request; the remote native/controller
remain independently live. Fourteen verified polls appear in the tool record,
but the interrupted cell's in-memory history is not retained on termination.
`verified-five-hour-wait-20261018T1132Z.json` therefore contains an empty checks
array, not an archived fourteen-row time series. Do not cite it as such.
The authoritative remote resource/workload telemetry is uninterrupted, and the
fresh full reader packet is `actual-vps-current-progress.json`. That separate
operator request is handled and verified without restarting the native process
or altering its configuration. Keep all credential/access details outside
public documentation. No local tool session remains pending.

## Complete-genesis input lineage verified; final retention still pending (2026-10-18 11:29 UTC)

The previous coverage question is resolved for the selected immutable input.
`selected-backup-genesis-continuity-lineage.json` independently checks 23
evidence-file hashes, the successful full canonical continuity audit and its
negative controls, both complete raw comparisons, original/promoted/current
payload metadata, and the actual final-native restore result. The audit follows
**40,236,770 canonical blocks/receipts** to the zero SHA-256 genesis parent and
verifies every one of **45,076,063 archive header IDs**. Both later block-family
raw digests equal `c5953863ebfa9909adda21b26ace7e88bc1d3c63766259d7990a42205a54856b`.
All **380 database path/size/SHA-256 entries** are unchanged through publication
and match the actual restore pins; only the public runtime configuration changes.
Checkpoint chain/state identities agree. This is immutable input-content
evidence, not reused older-binary runtime qualification or historical execution.

The old comparisons use little-endian length framing; the VPS compaction and
retention probes use big-endian framing. Do not directly equate their different
hashes. The exact payload lineage and complete pre-observer baseline bind them.
No new database scan, build, native restart or historical replay is needed for
this input proof. The final closed all-original-record retention scan remains
mandatory after five hours and the clean reopen. Do not credit it in advance.

At the latest actual read-only check, **11:30 UTC**, the original native and
corrected controller remain live and unchanged. Native elapsed **15,425.416 s**,
remaining **2,574.584 s**; head **40,311,604**, age **35.307 s**; **7,812 steady
reads / zero failures**, peak RSS **619,962,368 bytes**, minimum MemAvailable
**2,801,008,640 bytes**, strict disk margin **1,833,799,680 bytes**, OOM/swap zero.
This is still a partial observation. The expected cutoff remains approximately
12:13:39 UTC, followed automatically by the cooldown and 300-second reopen.
No local tool session remains pending. Repository whitespace validation and
byte-identical AGENTS/CLAUDE checks pass after this documentation update.

## Four-hour partial checkpoint; verify full-history coverage before completion (2026-10-18 11:14 UTC)

Read-only observation cell **3015 is complete/closed** after 45 actual service/
process checks. It never stops or restarts the native process. At 11:14:17 UTC
the unchanged native **414144 / 22700028** and corrected controller
**438740 / 23880217**, invocation `a2dfa1682bd24269adc27dc2b3467eb2`, are live.
Native elapsed **14,436.008 s**, observer telemetry **14,434.834 s**, remaining
to five hours **3,563.992 s**. Head **40,311,275**, age **8.813 s**; sample age
**3.435 s**. **7,236 synchronized reads / zero failures**, native peak RSS
**619,962,368 bytes**, minimum host MemAvailable **2,801,008,640**, strict disk
margin **1,833,799,680**, OOM/swap zero. This is a partial four-hour checkpoint,
not a completed five-hour pass. Evidence:
`vps8-five-hour-four-hour-checkpoint.json` and
`verified-five-hour-wait-20261018T111417Z.json`. No local observation process
remains pending; continue the same remote test and corrected controller.

The user asks for the VPS IP; the current SSH connection's server address and
host identity are verified and the private address is returned in chat only.
Do not put private host inventory or usable access routes in public documentation.

Final-review coverage check: the actual native restore validator reads all
46,458,363 records across nine families, validates every block/receipt record
and checks ancestry from the stored head down to the persisted state revision.
Current source `offline_restore_validation.cpp` explicitly stops that ancestry
walk at revision **40,236,770**. Therefore its record count/structural pass is
not, by itself, proof of canonical ancestry all the way to genesis. The final
archive oracle proves preservation against the sealed complete selected-record
baseline; inspect the existing independent source/history audits for a complete
canonical-genesis continuity proof as well. If that evidence is absent, obtain
a bounded read-only full canonical ancestry/continuity proof on the closed
final database before claiming the goal's full-history requirement. This is
an evidence-coverage question, not evidence that history is actually missing,
and does not require changing the frozen native binary or replaying historical
VM execution. Keep it explicit in the final completion audit.

## Corrected five-hour controller live; final helpers prepared (2026-10-18 10:35 UTC)

Previous goal turn made concrete progress: the operator's five-hour amendment
was installed and verified on the VPS. This continuation preserves that scope.
The original native **414144 / 22700028** and day invocation remain unchanged.
At 10:34:59 UTC the native has run **12,079.121 s**, with **5,920.879 s** remaining;
head **40,310,505**, age **18.364 s**, **5,832 synchronized reads / zero failures**.
Peak RSS **602,464,256 bytes**, minimum MemAvailable **2,819,530,752**, strict
whole-server margin **1,833,799,680**, no OOM/swap. The five-hour test is not done.

Actual already-closed smoke/recovery services are `not-found/inactive/MainPID=0`
with an empty InvocationID after systemd collects them. The first private
amendment controller incorrectly required a nonempty ID after a successful
post-observation exit. Before reaching five hours, only that private controller
is retired and replaced with revision 2. Native PID/start/unit identity is
verified identical before and after; no native stop, restart or replay occurs.
Its original files, controls and install receipt remain preserved. Empty IDs
are now accepted only for a collected closed unit; different nonempty IDs
remain rejected. This is a diagnostic-control correction, not a native change.

Authoritative current controller:
`teleno-vps8-five-hour-amendment-r2.service`, invocation
`a2dfa1682bd24269adc27dc2b3467eb2`, PID/start **438740 / 23880217**.
Its files are `five-hour-controller-r2.py`, `five-hour-plan-r2.json`, and
`five-hour-r2/`. The separately named reopen diagnostics end in `-r2.py`.
The old revision-1 unit is confirmed closed; its stale live file is explicitly
marked superseded by the updated reader. Expected cutoff remains approximately
**12:13:39 UTC / 14:13:39 Europe/Berlin** today.

Use `read-five-hour-checkpoint.py` for a concise fresh check; it calls the
original reader, verifies actual current native/controller PID-start/cgroup
members, and reports resource/head/sample-age checks without any native action.
`read-vps8-progress.py --brief` remains available for the full service view.

Prepared final helpers in the existing My Passport evidence directory:

- `preserve-five-hour-observation.py`: requires the amended closed pass and
  post-observation clean reopen, preserves original raw day and both controller
  revisions, and verifies every archive member. It never rewrites the original
  86,400-second refusal as a native pass.
- `preserve-observer-episode.py vps8-postday-r1`: unchanged, only after actual
  clean closed post-observation success.
- `dispatch-archive-retention-verify-five-hour.py`: requires independently checked
  final five-hour/reopen evidence, then starts the original entire selected-
  archive read-only scan with the existing native storage gate and all guards.
- `verify-final-vps8-head-legacy-five-hour.py`: captures the final closed native
  head and compares canonical ID/root against the stable independent reference;
  reference image/running identities are checked both before and after RPCs.

The shared `five_hour_final_gate.py` verifies actual raw results, every amended
input hash, closed services, full resource/read counters, mapped-library proofs,
and that the original workflow's sole refusal is its superseded duration. A
current read-only invocation correctly refuses because the five-hour terminal
result does not exist yet (`five-hour-final-gate-live-refusal.json`). Prepared
helpers are not executed qualification. The original 24-hour helpers are kept
unchanged and must not be used for this amended result. See
`five-hour-controller-r2-correction-review.json`, `five-hour-r2-install-receipt.json`
and `five-hour-final-helper-preparation.json`. Complete five hours, post-observation
reopen, all-original-record scan and final source/canonical/resource review
remain mandatory. Goal stays active.

## Operator amendment: five hours on the same VPS process (2026-10-18 10:23 UTC)

The user explicitly requests reducing the current observation from 24 hours to
**five hours**, counted from the original launch. This supersedes the prior
experimental-duration references below. The goal remains active; original
issue-9/default-10-GiB and broader LR2 requirements are unchanged.

The actual native PID/start **414144 / 22700028** and day service invocation
`d84053ccc96a490dbbcf6ec4f6b8e2d5` remain unchanged. The installed bounded
`teleno-vps8-five-hour-amendment-r1.service` is verified active: invocation
`0020f7939621456ba69006fde4713ca8`, PID/start **437566 / 23832723**. At 10:23 UTC
native elapsed is **11,367.347 s**, with **6,632.653 s** remaining to five hours.
Expected cutoff is approximately **12:14 UTC / 14:14 Europe/Berlin** today.
No native source, executable, configuration, database or original controller
input is modified. Seven original controller inputs are copied and hash-verified
before the new files are installed. The new controller has no automatic retry.

Use the updated `read-vps8-progress.py --brief`; it reads the new
`five-hour-r1/` controls and actual service/cgroup process identities as well as
the original workflow. The active goal's objective has no hard-coded 24-hour
duration; the implementation goal/report now record the explicit five-hour
acceptance amendment. The original frozen 24-hour harness cannot reload its
duration. Do not patch it in memory, overwrite its results, or relabel it as a
24-hour pass. At five native hours the new controller sends verified PID-fd
SIGINT, waits for owned closure, requires the old raw duration refusal to be
the sole refusal, and independently audits all amended resource/workload checks.
`five-hour-r1/observer-result.json` may then say `five-hour-observation-passed`.
The unchanged original raw day/workflow results remain visibly incomplete.

After the unchanged 130-second TCP cooldown, the new controller dispatches
`vps8-postday-r1` for 300 seconds using separately named private diagnostic
adapters. Only that adapter's prerequisite changes to the audited five-hour
result; native code/configuration and the workload/guards do not change.
`five-hour-r1/postday-live-provenance.json` records actual mapped libraries,
native launch, configuration, host and cgroup limits during reopen. The target
controller terminal state is
`all-amended-five-hour-phases-passed-awaiting-final-review`; it is not goal
completion. Any other resource/workload failure remains a failure.

Evidence stays in the existing My Passport `vps-8gib-goal-20261018-72a46f/` run:
`five-hour-plan.json`, `five-hour-controller.py`,
`install-five-hour-amendment.py`, `five-hour-amendment-install-receipt.json`,
and `five-hour-controller-audit-controls.json`. One positive synthetic audit
and nine negative controls pass; these do not qualify the live native run.
The actual VPS five-hour observation, closed-result preservation, post-observation
reopen, complete original selected-archive scan, final canonical comparison and
final exact-source/library/resource review are still pending.

At **10:25 UTC**, a second actual readback confirms both the native and amendment
controller PID/start identities are unchanged and live, with **6,515.463 s**
remaining. The calculated cutoff is **12:13:39 UTC / 14:13:39 Europe/Berlin**.
Linux PID-fd opening and the identity-bound signal API are verified available
without sending a signal. `five-hour-amendment-active-readback.json` retains
the full packet. No local process remains pending from this installation.

The original `preserve-observer-episode.py vps8-day1-r1` and final archive/
legacy helper preconditions still require the old 86,400-second result and
all nine original workflow passes. They must not be used to claim an amended
pass. For final work, preserve the unchanged original day controls plus the
independent `five-hour-r1/observer-result.json`, require the new controller's
closed `all-amended-five-hour-phases-passed-awaiting-final-review` result and
postday clean pass, and adapt the final read-only helpers to those explicit
amended prerequisites. Verify raw result hashes, every retained archive record,
the final canonical ID/root, and exact final inputs as before. Do not overwrite
the original failed/incomplete duration result or shorten any other gate.

## VPS three-hour checkpoint; 24-hour observation live (2026-10-18 10:14 UTC)

**10:14 UTC verified wait:** 58 fresh process/service checks confirm the same
native PID/start **414144 / 22700028**, day invocation
`d84053ccc96a490dbbcf6ec4f6b8e2d5`, and active observer-only service. Day elapsed
**10,827.535 s**, head **40,310,108**, head/telemetry age **34.933 / 29.153 s**.
Synchronized reads total **5,094 successes / zero failures**; peak native RSS
**602,464,256 bytes**, minimum host MemAvailable **2,827,907,072**, strict
whole-server peak including 8 GiB **38,166,200,320**, OOM/swap zero.
`vps8-day1-r1-three-hour-progress.json` preserves the full packet and
`verified-wait-20261018T101435Z.json` all 58 checks. Read-only observation cell
**2984 is complete/closed**; the actual 86,400-second VPS test remains active,
without a native stop or restart. This is a partial checkpoint. Keep the full
day, postday reopen/live provenance, entire original-archive scan and final
canonical/source/library/resource review open. The user asks whether execution
is on Hetzner: actual qualification is directly on the reference VPS; the miner
builds and My Passport retains evidence. No goal pause or scope change occurs.

**09:14 UTC verified wait:** same native PID/start **414144 / 22700028** and
day invocation `d84053ccc96a490dbbcf6ec4f6b8e2d5` remain live. Day elapsed
**7,219.365 s**, synchronized **4,982.089 s**, head/telemetry age **38.557 /
30.107 s**. **2,952 synchronized reads pass / zero failures**, observed native
peak RSS **593,633,280 bytes**, minimum host MemAvailable **2,832,052,224**,
strict whole-server peak including 8 GiB **38,166,200,320**, OOM/swap zero.
Evidence: `vps8-day1-r1-two-hour-progress.json` and the 35 actual process/unit
checks in `verified-wait-20261018T091428Z.json`. Read-only observation cell
**2980 is complete/closed**; it performs no native stop or restart. The actual
24-hour VPS test remains active. Keep the completed day, postday, final entire
selected-archive scan and final source/library/canonical/resource review open.

**08:41 UTC verified wait:** a bounded read-only watcher finishes 12 actual
process/service checks; it does not stop or restart the native test. At
08:40:51 the same PID/start ticks **414144 / 22700028** and day invocation are
live, elapsed **5,218.128 s**, head age **23.686 s**, **1,764 synchronized reads
pass / zero failures**, peak RSS **568,549,376 bytes**, minimum MemAvailable
**2,852,057,088**, strict disk margin **1,833,799,680**, OOM/swap zero.
`verified-wait-20261018T084051Z.json` records the live checkpoint. Observation
cell **2977 is complete/closed**; the VPS native job remains independently
active. This turn is a verified wait, not completed-day qualification. Keep
the 24-hour, post-day, whole selected-archive and final-review gates unchanged.

**08:23 UTC configuration review:** same native process is confirmed live;
the exact runtime config SHA remains `1b9c2e798ab7c8537401fd1c28e44dc38b72dc6faf5a49be7b11ce448254a74f`
and command arguments have no extra overrides. Explicit settings are shared
block cache **128 MiB**, per-family write buffer **8 MiB**, aggregate memtable
flush threshold **64 MiB**, two buffers, one background job. Existing native
startup telemetry confirms cache/job selection. Evidence:
`vps8-day1-r1-runtime-cache-configuration.json` and
`vps8-day1-r1-runtime-cache-startup-tuning.json`. These are component settings,
not a hard RSS bound or total live allocation; the kernel/host guards remain
the qualification evidence. No runtime configuration is modified.
Local **69810 and 54131 are complete/closed**.

**08:15 UTC update:** the first-hour checkpoint is preserved in
`vps8-day1-r1-first-hour-progress.json`: **3,671.706 s** total, including
**1,434.430 s** synchronized; native/service identities are still live.
Head age **12.194 s**, telemetry age **6.874 s**, **846 synchronized reads pass /
zero failures**. Partial native peak RSS **516,374,528 bytes**, minimum host
MemAvailable **2,913,595,392**, strict whole-server peak with reserve
**38,166,200,320**, OOM/swap zero. This is not completed-day qualification.

The live reader now also reports head/sample age, synchronized counts and
current native/host memory, OOM and swap values. Actual systemd state and native
PID/start ticks remain authoritative; a stale status file or observation timeout
never authorizes restarting a phase. Native source and the remote workflow
remain the frozen versions.

A bounded read of 28 native metrics is preserved in
`vps8-day1-r1-native-cache-metrics-20261018T081129Z.json`: 32 parsed modules,
attributed capacity **4,471,894 -> 4,543,721 bytes**, logged RSS
**447,643,648 -> 499,679,232**. Sampled pending-owner/transaction-work counts are
zero at both endpoints. This scoped cache accounting does not explain all RSS
or prove a leak/bound; other consumers and allocator memory are excluded.
Local **95365 is complete/closed**. Keep observing the same native process;
the complete day, post-day, original-archive scan and final review remain open.

**07:58 UTC update:** day episode catches up at **2,237.276 s** (37m17s), within
its two-hour gate. This is the day restart from an already advanced database,
not a cold full-backup benchmark. Native PID/start ticks remain
**414144 / 22700028**, day invocation `d84053ccc96a490dbbcf6ec4f6b8e2d5`.
The initial near-tip head **40,307,373**, age **13.865 s** at capture, matches
the independent legacy canonical ID and complete root after becoming
irreversible. Reference head/LIB **40,307,435 / 40,307,375**; all three image/
running identities are rechecked after the RPCs. Evidence:
`near-tip-day-head-legacy-20261018T075345Z-proof.json`,
`near-tip-day-canonical-raw-review.json`, and
`vps8-day1-r1-catchup-transition.json`. Local **39424 is complete/closed**.

Initial synchronized read count **180 successes / zero failures**. A single
**151.360-s** natural post-catchup interval uses **6.33 native CPU seconds**,
average **0.04182 cores**, reads **1,077,248 bytes**, writes **581,632 bytes**.
Native peak remains **463,765,504 bytes**, minimum host MemAvailable
**2,964,951,040**, OOM/swap zero. Partial counters are preserved in
`vps8-day1-r1-early-synchronized-counter-window.json`; differing workloads
mean this is not a causal code-saving comparison. The actual completed day,
post-day reopen, final entire selected-archive scan, final canonical sample
and final source/library/resource review remain required. Goal stays active.
Use the same authoritative live reader and existing unit; do not restart it.

**07:40 UTC update:** the actual day unit/native PID remains live and advancing.
A second independent canonical sample at **40,295,164** matches the legacy block
ID and complete root from its successor header; legacy head/LIB
**40,307,104 / 40,307,044**, all three reference images/running states verified.
This sample is 58,334 heights above the selected backup maximum. Evidence:
`in-progress-day-head-legacy-20261018T073955Z-proof.json` and
`in-progress-day-canonical-raw-review.json`; the latter independently decodes
and checks the original RPC results. Local handle **40336 is complete/closed**.
The node is still catching up. No completed day, near-tip or final canonical
qualification is credited. Final post-day archive and canonical checks remain
required; the in-progress check is not their substitute.

This checkpoint supersedes the live handles below. Goal remains active, not
qualified. The exact source manifest and native ELF remain `6456879f2162baf57d6b932054bf92e19569fb8e47c89d109fbd35e11a964ec5`
and `f086e2d71c04cc0922ccd497a2cb1c63d5265bc1d3190b2e17e1156b5a7d10f4`.

Clean restart completes **301.293 seconds**, exit 0, head
**40,252,427 -> 40,257,401**, kernel peak RSS **299,884,544 bytes**, minimum
host MemAvailable **3,194,441,728**, strict whole-server peak including 8 GiB
**38,053,023,744**. The planned abrupt episode completes **300.791 seconds**,
expected exit 137, head **40,257,650 -> 40,262,758**, peak RSS **304,177,152**,
minimum MemAvailable **3,177,730,048**, strict whole peak **38,042,226,688**.
Both preserve all guards, holds and key absence, with no OOM/swap. All ten and
eleven closed episode archive members are verified, respectively.

Recovery completes **301.397 seconds**, exit 0, head
**40,262,990 -> 40,268,199**, kernel peak RSS **302,538,752 bytes**, minimum
host MemAvailable **3,172,044,800**, strict whole peak including 8 GiB
**38,059,630,592**. All eleven closed episode members are archived and verified.
Its former native PID/start ticks **412737 / 22655711** are closed.
Recovered irreversible height **40,262,698** has exactly the same
block ID and canonical block/receipt record digest as the archived fresh
pre-interruption oracle. Independent closed comparison is in
`completed-abrupt-recovery-oracle-audit.json`; the archived complete recovery
proof also exactly matches the initial live readback.

Remote workflow invocation `6f5cf4b636734a5a988341e98c06db0d` has seven completed
phases. **vps8-day1-r1 is live**, service invocation
`d84053ccc96a490dbbcf6ec4f6b8e2d5`, native PID/start ticks **414144 / 22700028**,
explicit duration **86,400 seconds**, launch approximately 07:13 UTC. Its exact
executable, all five mapped libraries/inodes, config, source manifest, actual
2-vCPU/4-GB/ext4/no-swap host and service limits are independently verified in
`live-day-observer-execution-proof.json`. At 07:29 UTC the initial **941.333 s**
advance head **40,268,418 -> 40,284,403**; it is still catching up. Kernel peak
RSS **444,514,304 bytes**, first post-warmup sampled maximum **443,752,448**,
minimum MemAvailable **2,994,348,032**, strict whole peak with reserve
**38,166,200,320**, OOM/swap zero. These are partial values, not final bounds.
The early sample and actual live unit/PID identity are preserved in
`vps8-day1-r1-first-steady-progress.json`, which explicitly does not claim a
completed day or synchronized steady-read window. Do not duplicate or restart
the running episode.
Use `read-vps8-progress.py --brief` for authoritative live units/processes.
The day has now passed its declared two-hour catch-up gate. It must still complete
all 86,400 seconds and clean stop, then pass post-day reopen. No completed-day
or final shutdown near-tip result is credited in advance. Preserve each only after its closed pass with
`preserve-observer-episode.py`.
Final original-archive scan and final stable legacy head/root proof still follow.
For the live post-day process, run the read-only
`verify-live-observer-provenance.py vps8-postday-r1` from this owned evidence
directory while that episode is running. It records fresh executable/config,
mapped-library device/inode/hash, launch identity, actual host and service-limit
checks in a unique local JSON. The helper passes against the existing day
process at **07:24 UTC**, preserving PID/start ticks **414144 / 22700028** and
all five original dynamic libraries. Its evidence is
`live-provenance-vps8-day1-r1-20261018T072420841954Z.json`. This checks current
provenance only; complete duration, retention and final review are separate.
Local handles **39640, 44494, 43030, 61468, 17652, 46770, 8540, 32493, 73327,
70490, 23087 and 26356 are complete/closed**. There is no pending local process.
Owned evidence remains on My Passport in the same `vps-8gib-goal-20261018-72a46f/`
run. The original 10-GiB default and issue-9 acceptance remain unchanged.

## VPS compaction and isolated producer pass; whole observer live (2026-10-18)

Latest checkpoint supersedes earlier live handles. Goal is active, not qualified;
default 10 GiB and original issue-9 contract remain unchanged. Exact native
source manifest `6456879f2162baf57d6b932054bf92e19569fb8e47c89d109fbd35e11a964ec5`;
native ELF `f086e2d71c04cc0922ccd497a2cb1c63d5265bc1d3190b2e17e1156b5a7d10f4`.

**06:53 UTC update:** first four remote workflow phases pass. Smoke completes
**900.605 s**, clean exit/closed native, head **40,236,951 -> 40,252,374**;
peak kernel RSS **347,844,608 bytes**, host MemAvailable >=**3,118,170,112**,
strict whole-server peak+8 GiB **38,061,752,320 bytes**, no OOM/swap. All ten
episode members archived/verified. 534 successful/two failed read calls; all
four block/receipt/resource/mempool methods have 89 successes/no failures.
The p95 mixed-block latency bucket upper bound is 20 ms, max 40.522 ms.
Logged/kernel RSS comparisons: 15, zero mismatches, max delta 946,176 bytes.
Native CPU 880.96 s; sampled WAL peak 75,141,120 bytes (soft trigger, not a hard
64-MiB cap). It is still catching up; no near-tip or 24-hour result is credited.

**Clean restart vps8-restart-r1 is now live**, after the 130-second TCP cooldown.
Unit invocation 9fc29b78d95d4df8bef9f04cd5461654, native PID/start ticks
410821 / 22567868; it has reopened the same DB and is advancing. Do not duplicate
the episode. `read-vps8-progress.py --brief` now saves the full packet but prints
concise progress, with actual systemd states and cgroup member PIDs/start ticks
to verify liveness rather than trusting a stale file. Helper changes are local
diagnostics; the running native/supervisor/workflow source is unchanged.
Local handles **74221, 57652 and 40598** refer only to closed or short readback
operations; no new native process should be started from an observation timeout.

Complete actual-VPS real compaction passes in **4,118.362 s**, including native
job **1,245.788 s / 1,390,521,101 output bytes**. All **46,458,363** records in
nine current/checkpoint/reopened families and SST checksums match. Timed native
probe RSS **143,949,824 bytes**; host MemAvailable minimum **3,264,700,416**;
no OOM/swap. Additional unique allocation **1,424,293,888 bytes**, strict
whole-server peak including 8 GiB **39,352,967,168 bytes**, margin **647,032,832**.
Snapshot retired only after all oracles; all 24 closed controls archived and
every member verified. `closed-compaction-and-retention-receipts.json` is local.

The full read-only original-archive baseline passes in **1,815.390 s**: all
45,076,063 raw records match the selected pre-compaction digest, with
45,075,988 frozen records and 75 tail records. Peak reader RSS 38,785,024 bytes.
No checkpoint pins remain; all 13 controls/metadata archived and verified.
**Final original-archive verification after the post-day observer shutdown is
still required.** `dispatch-archive-retention-verify.py` is prepared and must
only run after all nine bounded workflow phases pass with writers stopped.

Isolated current-library actual-VPS producer create/reopen passes: 100 custom
federated blocks, 128 signed no-op transactions/block (12,800), 32 synthetic
payers, zero external adapters/key files. Create **165.933 s / 745,459,712 peak
RSS bytes**, reopen **12.725 s / 605,413,376**. Reopen replays 60 blocks from
LIB 40 to the same height-100 head/root and unchanged complete archive digest.
Production p50/p95/p99 **315.760 / 584.047 / 664.008 ms**, excluding admission.
Both preserve all guards/OOM/swap; all 48 fixture/control members archived and
verified. This is not integrated operational PoB/mainnet-producer readiness.

Workflow completes the first three phases and launches **vps8-smoke-r1**, the
900-second final whole observer. Native PID/start ticks **409125 / 22463913**,
verified executable and dedicated service cgroup. Five actually mapped dynamic
libraries match the captured runtime. At early catch-up, native head/root at
height **40,237,506** independently match legacy canonical ID/next-block root;
legacy head **40,305,834**, LIB **40,305,774**. This is a catch-up sample, not
near-tip/sustained proof. Local capture handle **88867 is complete/closed**;
`current-running-head-legacy-proof.json` is authoritative. The observer's first
sampled heads advance 40,236,951 -> 40,241,956 with peak RSS 306,315,264 bytes,
host MemAvailable >=3,139,919,872 bytes and all guards intact in that checkpoint.

The remote workflow continues automatically: smoke, cooldown, restart, cooldown,
controlled abrupt interruption, cooldown, recovery oracle, cooldown, 86,400-second
day, cooldown, post-day reopen. **Do not duplicate phases or restart solely from
an observation timeout.** Read `workflow-r1/live.json`, completed/result records
and actual unit/native owners with `read-vps8-progress.py`. No 24-hour duration
has been credited yet. Extra final source/library/whole-measurement review,
archive retention scan and final stable legacy head/root sample still follow.
`verify-final-vps8-head-legacy.py` is prepared for the final closed day/postday
head; it is not executed yet. Controls are kept under the existing owned
My Passport run; no global memory update, publication or producer activation.

Local handles **13967, 21000, 46949, 61958 and 71103 are complete/closed**.
The remote compaction and baseline units are verified inactive/MainPID 0.
Current run root:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/vps-8gib-goal-20261018-72a46f/`.

## VPS complete restore passes; real compaction running (2026-10-18)

Current goal is active, not qualified. This entry supersedes earlier live handles.
Source manifest remains `6456879f2162baf57d6b932054bf92e19569fb8e47c89d109fbd35e11a964ec5`;
native SHA remains `f086e2d71c04cc0922ccd497a2cb1c63d5265bc1d3190b2e17e1156b5a7d10f4`.
All 63 distinct current-source native tests pass. Handles **49556**, **71358**
and **47742** are complete/closed. The stale-scope callback diagnostic failure
is preserved; its one affected native test is rerun with a fresh live callback
and passes. No native failure is waived or duplicate pass counted.

Actual VPS complete restore passes: preflight, controlled interruption after
1 GiB, same-source/profile resume (394.161 s), and complete read-only
validation/held activation/reopen (2,485.981 s). The maximum whole-server peak
including the complete 8-GiB reserve is **37,915,475,968 bytes**, leaving
**2,084,524,032 bytes** below the strict 40,000,000,000-byte limit. Validation
checks 46,458,363 records/nine families, 917,316 state objects, state revision
40,236,770 and highest stored height 40,236,830. Peak native RSS is 54,222,848
bytes, minimum validation host MemAvailable 3,391,688,704 bytes, no OOM/swap.
No observer process has yet started. Historical execution remains distinct.

A real-data compact job is now running in an owned bounded transient VPS
service: `real-compaction-r1/compact-live.json` and native `events.jsonl` are
authoritative. The read-only reviewed plan selects 21 contiguous level-6 blocks
SSTs / 1,389,104,671 bytes, level-6 output, planned additional workspace
1,801,143,333 bytes versus strict margin 2,082,353,152 bytes. The diagnostic
retains a reader and verified hard-link checkpoint, compares all nine families
and reopens before releasing the checkpoint. Actual output must reach 1 GiB;
small successful builder pilots are not qualification. Exact current diagnostic
SHA `bb8ad80bd081677233dbefa93132d01e2a1fcab3fee6eec56e33f140e850b6c9`.
Both private diagnostic versions, inputs, maps, link-library hashes and complete
fixtures are archived with every member verified on My Passport. The main
native executable is unchanged. No source replay or replacement restore occurs.

New checkpoint at 2026-10-18 06:00 UTC: actual native compact job returns `OK`,
input 1,389,104,671 / output **1,390,521,101 bytes**, 2,603,865 input/output
records, **1,245.788 s**. The whole case is still running its all-record current,
checkpoint/checksum and reopen oracles; do not call it passed prematurely.
Native/probe processes and exact transient invocation IDs are verified live.
Independent original JUnit inspection confirms all 63 named passes. The same
backup/config/hardware restore comparison verifies identical validation oracles:
activation elapsed +0.32%, CPU +0.24%; this is a separate single observation,
not a causal timing gain. The 8-GiB profile saves zero physical data bytes.
Current runtime provenance hashes all five resolved dynamic libraries;
actual OS is **Ubuntu 26.04.1 LTS**, not an assumed Ubuntu 24 release.

At 06:15 UTC the complete **current** before/after comparison and SST checksums
pass: all 46,458,363 records/nine families, including 45,076,063 archived
block/receipt records and 917,316 state objects, are unchanged. Retained-checkpoint
scan is active; ordinary reopen still follows. The independent retention baseline
has scanned 24,117,248 archive records. Both exact native processes are verified
live; this is not a stale-file wait. The completed compaction timed-region audit
uses closed event/counter windows and bounded process/clock alignment: native
CPU user+system **1,226.01–1,250.57 s**, kernel read_bytes
1,326,161,920–1,568,563,200, write_bytes 1,376,485,376–1,391,046,656.
`completed-compaction-job-counter-bounds.json` preserves the evidence and scope;
whole-probe totals still include all integrity scans. No new workload is run
for this counter audit. Local handle **58664 is complete/closed**.

A separate **actual-VPS read-only archive retention baseline is live**.
Use `archive-retention-r1/baseline-controller-live.json` and `baseline-live.json`;
local `read-vps8-progress.py` now includes this case. Native reader PID/start
399785 / 22251099, bounded unit invocation
9012fb16442b480d810f1a07ed8d0d95. It reads an owned hard-link clone of the
existing retained checkpoint; no second 24-GB data copy or primary DB writes.
Exact private executable SHA
`263bb8ca59b2be8cdeb088e8ab7bc5b0aa6080bd11eabb67fa4f2453bc717185`.
Builder compile and five negative/positive controls pass. The two initial
private-checker compile/relative-library-path failures and complete current
diagnostic/fixtures are archived and every member verified. Native product
source and executable are unchanged. Handles **28390, 46934, 9838, 68095 and
89475 are closed**; the current baseline is a remote service, not a local handle.

Its full raw baseline digest must match `real-compaction-r1/before-digest.json`
for every original archive record. It records a frozen raw digest through
height 40,236,770 plus each original tail key through 40,236,830, allowing only
completion of an originally empty receipt root. Missing data, changed existing
roots and other changed fields are refused. **After the observer/post-day run,
execute its actual-VPS verify phase while holding the real native storage gate
and with all writers stopped.** The existing bounded workflow does not run this
additional final scan and cannot itself complete the goal. Preserve its baseline
and controller outputs. If `pinned_checkpoint_retained_for_review` is nonempty,
review/preserve that owned copy before retirement; never delete unknown data.
Check/report concurrent reader CPU/I/O alongside the compaction integrity scans.

Owned evidence remains
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/vps-8gib-goal-20261018-72a46f/`.
The independently bounded **remote workflow is now active**, waiting for the
exact existing real-data compaction. Do not launch duplicate phases manually.
`workflow-r1/live.json`, `completed-phases.json` and eventual `result.json` are
authoritative; local `read-vps8-progress.py` refreshes and saves the full current
packet. No automatic retry. Every native stage runs in its own verified service
cgroup/deadline, not a sibling scope escaping its supervisor. Observer diagnostics
add irreversible block/receipt oracles for controlled abrupt/recovery episodes,
two-hour catch-up and sustained synchronization freshness checks, and post-day
reopen. None of the observer episodes has executed at this checkpoint.

Automatic phase order after a passing compact result: current isolated producer
create/reopen; smoke 900 s; restart 300 s; controlled abrupt 300 s; recovery
300 s; full mixed-read day 86,400 s; post-day reopen 300 s. A 130-second recorded
TCP cooldown preserves bind preflight between observer episodes. Any failed
phase stops and preserves output for diagnosis. Existing services/data are not
changed. Final review is still required; the workflow cannot complete the goal.

Local handle **88867 is live**: bounded read-only current-running-head capture
and independent legacy canonical head/root comparison, waiting for a ready
final observer. Preserve its result/failure and do not duplicate its output
filenames. Exact executable/owner/cgroup and unchanged reference container images
are checked before comparison. No service, signing or chain-authority changes.

Next: refresh remote workflow/compact result and handle 88867; diagnose any
failure before a scoped continuation, or retain completed measurements through
the full day and final review. Public report is `vps-8gib-recovery-reserve.md`.
Default 10 GiB, original issue-9 reserve criterion, external 70-GiB and builder
10-GiB floors remain unchanged. No mainnet signing/production activation.

## VPS experiment: final binary tested; full restore running (2026-10-18)

Current goal remains active. See `docs/validation/vps-8gib-recovery-reserve.md`.
The default 10-GiB policy and original issue-9 contract remain unchanged.
Native source is frozen at 312 inputs, manifest
`6456879f2162baf57d6b932054bf92e19569fb8e47c89d109fbd35e11a964ec5`;
native SHA `f086e2d71c04cc0922ccd497a2cb1c63d5265bc1d3190b2e17e1156b5a7d10f4`.
Version on the actual VPS is
`1.3.0-dev.0+d3cfb94455da4a472f651eb4239abbc289e74225`; the declared base
override alone does not identify the preserved uncommitted implementation.
All old native B7 source/build/dependencies remain unchanged.

Handles **70850, 85502, 45082 and 50391 are complete/closed**. Fresh complete
Linux build passes; four focused tests and version/help pass both on the builder
and actual VPS. All component fixtures/controls receive exact-member verified
archives before only closed generated duplicates are retired. VPS component
host MemAvailable stays at least 3,328,954,368 bytes, no OOM, zero swap.
These are component results, not whole-node RSS/soak qualification.

The selected public source is `20261016T201050Z-ms-1792181450480-files-380`,
383 files / 24,114,178,822 logical bytes / 24,113,527,301 DB bytes. Manifest
`228a95506dceeff4a35b42cbd9af1709eb66906cde6346d615ed87984318ab5e`, inventory
`7a4c755fe30610281fe8059707b5e9b8d114f944faa52c9c3aa56e169b5c55c5`.
Use the preserved **promoted HTTPS metadata**, not the local unpromoted metadata:
native promotion changes only config.yml; every one of 380 DB entries matches.
The first mismatch correctly refuses before installation. Source signatures
remain absent; no trust setting is weakened. Exact published pins match the
fresh native VPS preflight.

Latest stopped B3 node is fully preserved: 434 entries; 374 existing CAS files
freshly hashed (24,062,289,819 bytes), complete 60-entry delta archive
97,764,998 bytes, plus exact four-member execution/config inputs. Handle
**44085 is complete/closed**. Controlled replacement rehashes all 434 entries,
claims the original storage gate, checks no source owners or configured node
service, saves original metadata and retires only this owned stopped test node.
Handle **59540 is complete/closed**: 24,223,096,832 available bytes recovered;
35,813,335,040 available afterward. Gate, off-host recovery bytes, unrelated
services and data remain. The no-match `systemctl list-unit-files` pattern
returns 1/empty; preserve that first refusal. The reviewed successful full
unit-file inventory/filter replaces only that diagnostic check.

Handle **47742 is live**: same final binary is running native complete direct
restore on the actual VPS, explicit experimental-8gib on every phase. Preflight
passes (peak with reserve 13,738,299,392 bytes); controlled interruption after
at least 1 GiB passes exit 130 (peak 14,862,680,064). Resume is active; activation
and mainnet observer have not yet run at this checkpoint. Follow the existing
handle/actual owned PID and `restore-progress.json`/`full-restore-live.json`;
do not duplicate a phase or replay source once validation has begun.

Broader native handle **96286 is failed/closed** only because a local archive
validator omitted its tarfile import after the first native test had passed.
Every completed archive member/hash is then verified, only its closed fixture
duplicate retired, and that same-source pass retained without repetition.
Reviewed continuation **49285 is live** for the remaining current-source tests;
four focused passes plus all remaining roster entries must total exactly 63.
The native binary hash must stay unchanged. `broader-progress.json` and the
preserved original failure/review are authoritative. No native failure is waived.

Owned evidence root remains
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/vps-8gib-goal-20261018-72a46f/`.
Prepared/installed `observer-assessment-vps8.py` and supervisor are not yet run.
They require completed same-source full restore/held activation, enforce actual
duration/planned stop and head progress, add bounded mixed historical/recent
block/receipt/resource/mempool reads, five-second SST/WAL/open-deleted attribution,
and keep the original half-second RSS/host/40-GB/OOM/swap/hold/key guards except
the explicit experimental reserve. Next: finish restore, preserve full evidence,
run smoke/restart/recovery and isolated producer tests, then a completed 24-hour
soak. Qualified smaller reserve, larger compactions and issue 9 remain open.

## Experimental 8 GiB profile: direct VPS goal active (2026-10-18)

The operator explicitly requests a new goal and tests directly on the reference
VPS. See `docs/implementation/vps-8gib-reserve-test-goal.md`. This separate
experiment preserves the default 10-GiB policy and original issue-9 contract.
It does not authorize mainnet signing/production activation. The former
component goal remains complete; its evidence is not a whole-node pass.

Fresh read-only VPS inventory confirms 2 vCPUs, 4,000,034,816 bytes RAM, no
swap, no native process, intact observer hold and no producer key in the owned
test basedir. Root available is 11,717,701,632 bytes; its 426 DB files remain
stopped. Preserve the exact latest database before any replacement.

Native `--backup-restore-reserve-profile experimental-8gib` is in development,
restricted to direct actions. A common reserve helper retains the 20% term;
preflight/stage/validation/activation share the policy and the descriptor binds
it across resume/activation. Old standard descriptors remain compatible.
Focused/default/refusal/policy-mismatch and CLI workflow checks are added but
not yet claimed passed. English operator and Koinos One handoff docs identify
the feature as uncommitted native work, not GUI or packaged functionality.

An isolated fresh 312-input Linux source export is verified; manifest SHA
`6456879f2162baf57d6b932054bf92e19569fb8e47c89d109fbd35e11a964ec5`.
The old B7 source/build/dependencies remain unchanged. Configure passes;
handle **70850** is building focused targets and the native node on the builder
under verified 2-CPU/2.5-GiB/zero-swap limits. This is preparation, not VPS runtime
qualification. My Passport remains the local evidence destination:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/vps-8gib-goal-20261018-72a46f/`.

Handle **54946** inventories the stopped VPS node for exact CAS-plus-delta
preservation; no database is retired or opened. The complete latest state,
not an earlier backup alone, must have verified off-host reconstruction bytes
before controlled replacement. One closed generated SC5 fixture archive is
losslessly re-encoded, every member and raw tar byte verified: 678,275,925
gzip bytes become 34,875,353 Zstd bytes, freeing 643,400,572 bytes without losing
evidence. The external 70-GiB floor remains unchanged. An initial local
preparation attempt fails before remote install because system Python lacks
`hashlib.file_digest`; the preserved failure is corrected with the existing
Python 3.12 interpreter. No native source change is made for that tooling issue.

## Compaction component complete; reserve remains unqualified (2026-10-18)

The requested component experiment and report review are complete. See
`docs/validation/compaction-headroom-experiment.md` and the exact unchanged
registered objective in `docs/implementation/compaction-headroom-test-goal.md`.
This does not close issue 9, change its original reserve criterion, alter native
product source/defaults, qualify the whole node/producer, or operate the VPS.

All three largest fresh archive trials pass every one of 69,120 records,
checksums/nine-CF digest and ordinary unmodified-native reopen. Each largest
automatic job outputs **1,111,250,915 bytes**; maximum observed DB-file
allocation is **2,291,363,840 bytes**. Archive CPU is 246.88–292.55 s,
largest-job duration 117.163–135.763 s, and maximum native kernel RSS
2,201,108,480 bytes. The matching state case also passes its >1-GiB job.
These pre-reserved 1-GiB atomic batches are synthetic stress, not ordinary
transaction traffic or a whole-node memory bound. R4 native executable hashes
remain those in the preceding entry. Exact libs, source, flags and link maps
are preserved externally.

Handle **80649 is complete/closed** for the repetitions. Handle **75802 is
complete/closed** for all remaining checks. Controlled interruption at a real
332,742,323-byte job's begin exits 87 and recovers all 18,432 fsynced baseline
records after synced idempotent overwrites. This is not partial-output/power-loss
or new-value interruption qualification. The diagnostic 12-MiB SST quota returns
Space limit reached, exits 88, and an ordinary reader recovers all 1,024
acknowledged records, with no visible failed batch. It does not fill the host or
change the product default. Their complete 74-/45-member archives are
356,187,267 / 40,108,538 bytes and are independently member-verified before
retiring their closed duplicate node directories.

R5 tests real selected source content in a deterministic shuffled arrival
order, with native B7 storage/options/dependencies unchanged. All **121,814
original records / 257,400,266 raw user key/value bytes** compare to the input
SST before and after native reopen. Four automatic jobs pass; largest output
58,909,656 bytes, owned allocation peak 180,867,072, native RSS 393,592,832,
CPU 136.57 s. Writer SHA
`005b4d57ae5154a8cbce90fd264451ece3ac01206320c7890dcf4135c7da6df0`, reader
`7edd630a67aca17bd68a99ccd2f118a9585e4fbd0539d459f27c372a93fbb2aa`.
The original hash-pinned SST is recovered from its verified external archive;
no original backup is altered. R5's complete source/bin/input/evidence archive
has 81 members / 93,546,770 bytes / SHA
`a279033b180d26862c727f62e9ec15198751dacd8a248698165030c30fc14958`.

Final inventory rechecks all **311 B7 inputs locally and on Linux**, unchanged
manifest `a21a8958e1757a8f2f661264b5b8665158c6db33c9c31a2fed8991d39e962a20`.
R4/R5 libraries match; ordinary-reader link role and only two optional private
hooks pass. AGENTS/CLAUDE remain identical. **All owned native processes are
closed.** No new OOM: global baseline/final remains 1; own swap zero. Across
28 r4/r5 commands, min sampled root available is 10,918,191,104 bytes and host
MemAvailable 4,459,712,512. The My Passport 70-GiB and Linux 10-GiB floors are
unchanged; final Linux headroom above its floor is about 2.337 GB. A nominal
1.5625-GiB-input fixture needs 3,556,769,792 above the floor and is declined
before generation. It is explicitly unexecuted, not a larger-job pass.

`measured-workspace-analysis.json` derives maximum additional workspace over
final archive SST content of **1,154,625,536 bytes**. Max observed WAL is
1,079,619,584 despite the 64-MiB soft trigger; flush and compaction overlap.
Open/deleted allocation is observed zero. Logs contain write-stop warnings;
exact cumulative stall duration is not instrumented. Preserve that limitation.
Adding this workspace to the **older B3** occupied peak and hypothetical 512-MiB
growth leaves 444,895,232 budget bytes with an 8-GiB held-free reserve, or
2,592,378,880 with 6 GiB. This is not a B7 whole-server result or upper bound.
Minimum safe reserve remains unestablished: keep the current default and
qualify 8 GiB first, with retained-reader/checkpoint, growth and whole-node
gates still open.

Owned evidence remains under the My Passport run's
`evidence/compaction-headroom-goal-20261018-7b585e/`; `component-acceptance.json`
and `final-native-inventory.json` are the final component ledger. Earlier r2/r3
Linux namespaces and r4 object/link-map copies are fully archived and retired;
restore those verified copies before any old command needs their former paths.
Current native build/source/dependencies and unrelated services/data remain.
Older entries below are chronological snapshots, not current live-handle state.

## Compaction: actual 1-GiB output reached; archive case live (2026-10-18)

Goal remains active. Native B7 source is unchanged. R3 ordinary 1,056-MiB
state workload verifies all 67,584 records/reopen but its largest job is only
569,313,038 bytes; input size is not a job-output pass. R3 is now fully
preserved (42-member `r4/closed-r3-complete.tar.gz`) and its closed Linux
namespace retired after fresh all-member hashes and empty process scans.
Its former paths require restoration before historical reuse. Passing
deterministic generated DB payload is retired only after complete bytewise/
checksum/reopen checks, per-file hashes and external metadata/source/trace
preservation. No original backup or operational data is removed.

Fresh diagnostic r4 compiles and both new 32-MiB pilots pass; its ordinary
reader link map excludes the observed-storage object. Writer SHA
`ca8d2b3811fc79b273d740cef96370423e45631caffc7d5e0e7bd4ce60b3cbdc`, reader
`994ef366e64d20432a9f7c0caf1d872b92dea8bb04238a04f4394c0b4046e831`.
All source/dependency/flag/link identities are in `r4/compiled-native-result.json`.

R4 `bulk-1080m-state-r1` **passes**, including an actual successful automatic
job output of **1,111,975,604 bytes**, all 69,120 expected records, nine-CF
digests/checksums and ordinary reopen. Owned DB allocation peak
2,290,585,600 bytes; native kernel RSS 2,168,426,496; CPU 17.59 s; physical
output 4,079,210,496. This deliberately pre-reserves a 1-GiB atomic batch:
synthetic soft-buffer-overshoot stress, not ordinary transaction traffic or a
whole-node memory pass. Native configuration remains unchanged. Its metadata
archive is verified before generated DB retirement.

Handle **66768 is live** at the last authoritative check, running the matching
`bulk-1080m-archive-r1`. Native result state is `running-action`; last event is
the large flush begin. Follow that same handle and actual owned process.
Do not restart on a timer. After archive integrity/reopen audit, repeat the
largest physical case until there are at least three trials. Representative
121,814-record SST input, controlled interruption/recovery and diagnostic
12-MiB SST-quota refusal are implemented privately but **not executed yet**.
The quota is only a diagnostic option in its separate case, not a product
default or physical-host ENOSPC test. Preserve failures and both storage floors.
The stopped VPS, current 10-GiB reserve and issue 9 criteria remain unchanged.

## Compaction goal execution and completed 256-MiB gate (2026-10-18)

The previous goal turn is progress: registration, exact source and live ext4
capacity evidence change the next action. This continuation preserves/reclaims
owned copies, builds the native observer/ordinary reader and executes two
passing pilots. Goal remains active; no reserve or VPS change.
See `docs/validation/compaction-headroom-experiment.md`.

All prior owned Linux evidence has a verified external recovery copy:
`compaction-headroom-goal-20261018-7b585e/previous-owned-evidence-complete.tar.gz`,
10,293 members / 385,204,372 bytes / SHA
`552fec9388b69838ac1541c04065d23be8d5d687dcb36a725040bf775077f9f9`.
Twenty closed duplicate namespaces are then retired after fresh all-member
hashes/links and empty exe/cwd/maps/cmdline/fd scans; 682,336,256 available
bytes recovered. **This includes older source-bridge/H1 reference and related
diagnostic namespaces.** Their exact paths/restore mapping are in
`reviewed-retirement-proof.json`; restore the corresponding verified archive
members before old historical commands reference those paths. Current B7
source/native build/dependencies, operational data and original full backups
remain intact. An older 678,273,616-byte SC5 gzip archive is also losslessly
re-encoded to 34,879,881 bytes, retaining every raw tar byte/member.

Compaction probe r0 handle **76527 is failed/closed** at link: its listener's
RTTI expects RocksDB Customizable typeinfo absent from the installed fno-rtti
library. Its complete 40-member failure archive is 2,156,017 bytes / SHA
`3f8007207123ef806e0df168a771f35eb339e35acb3f2ada56092e40d26c223a`.
R1 handle **65707 is failed/closed**: applying fno-rtti to the instrumented
native storage unit conflicts with Boost.Log dynamic_cast. Its 29-member
failure is fully archived before a fresh r2; no fixture executes in r0/r1.

R2 uses an opaque listener declaration in native storage, preserving its RTTI,
and fno-rtti only for the listener implementation. Source B7 stays frozen.
Handle **40694 is complete/closed**: writer/ordinary native reader compile and
both 32-MiB archive/state pilots pass. Each checks all 2,048 records bytewise,
nine-family full digest/checksums and unchanged digest after ordinary reopen.
One successful automatic compaction per pilot outputs 33,691,962 /
33,695,991 bytes; old/new SST coexistence is observed. Both link maps verify
that only the writer uses the private observer unit. `r2/pilot-audit.json`.

Handle **61542 is complete/closed**. Both 288-MiB archive/state cases pass:
18,432 records checked bytewise, all nine CF digests/checksums and ordinary
native reopen digests match. Largest successful auto-compaction outputs are
294,668,223 / 303,251,743 bytes; owned allocated peaks are 651,030,528 /
688,406,528 bytes. See `r2/scaled-audit.json`. The complete closed r2 namespace
has a verified 189-member external archive, losslessly re-encoded to
363,477,977 bytes / SHA
`2711fbfe68698ca91b95b6b96b2228daec2e9d4e5183b48149414da6bec06914`.
Re-encoding completed; the verified duplicate gzip is retired. Native result
state can still read `case-complete` from a previous action while a next
case runs; **never infer terminal status from that file alone**. Inspect the
same handle and actual process. Remote experiment namespace ends in
`compaction-headroom-r2-20261018-7b585e`; private access details stay outside
the repository. Local `r2/run-288m.py` and `scale-288m-progress.json` identify it.

Next: bound monitor memory/trace storage before large runs,
then achieve >=1-GiB actual outputs and repeat largest case three times.
Representative immutable SST input SHA
`752dd3a2a7034770889c90ee100573b7fbc5ff279ea462da7f69342ddfe5edbb`
is recoverable as `input.sst` in the verified `dictionary-screening-1.tar.gz`
(121,814 expected rows). Controlled interruption/recovery, capacity refusal
and measured 8-/6-GiB assessment remain unexecuted. Do not modify frozen r2
source while its cases run, lower the 70-GiB external/10-GiB Linux floors,
restart the VPS or claim issue 9 complete. Other B7 sanitizer/benchmark gates
from older entries remain distinct and unfinished.

## Active isolated compaction test goal (2026-10-18)

The user explicitly requests a goal for compaction testing. Goal lookup is
null before registration; the new component goal is successfully created and
freshly confirmed **active**. Exact objective:
`docs/implementation/compaction-headroom-test-goal.md`. The broader issue 9
acceptance criteria, current 10-GiB default and stopped VPS remain unchanged.
Required output includes actual 256-MiB and 1-GiB automatic leveled compaction
jobs, at least three repeats of the largest case, complete logical/checksum/
reopen checks, isolated interruption/recovery/capacity refusal, physical peaks
and CPU/RSS/I/O tradeoffs, with diagnostic 8-/6-GiB reserve assessment.
Larger scales are conditional on safe owned workspace capacity. Component
evidence must not be presented as whole-node or producer qualification.

New owned external evidence:
`evidence/compaction-headroom-goal-20261018-7b585e/` under the existing My
Passport run. `goal-registration.json`, `objective.txt`, `linux-inventory.json`
and `initial-progress.json` are saved. An initial local metadata-save wrapper
refuses before execution because its JavaScript helper lacks `btoa`; the goal
was already registered and is not recreated. Corrected quoted-heredoc saving
preserves the exact returned objective and registration.

Linux read-only inventory verifies all 311 current B7 inputs, unchanged source
manifest `a21a8958e1757a8f2f661264b5b8665158c6db33c9c31a2fed8991d39e962a20`,
correct CMake source and ext4; no owned native process is running. Available
host memory 6,112,636,928 bytes. Ext4 free above unchanged 10-GiB host floor:
1,861,754,880 bytes; external free above unchanged 70-GiB floor: 1,383,583,744.
No new fixture, compile or compaction has run. Inventory headroom cannot yet
accommodate the old-plus-new 1-GiB job with safety allowances. Prepare the
resource plan, preserve closed owned evidence before reclaiming duplicates,
and do not reduce floors or touch operational data. Smaller pilots can inform
instrumentation after budgets/ownership are verified. Existing B7 ASan r1
ledger failure and remaining full-suite/paired-measurement work stay distinct.

## Recovery reserve analysis requested by the user (2026-10-18)

The user asks for analysis to reduce the 10-GiB reserve. Analysis only: no
code/default/config/goal-criterion change, node start, DB open or compaction.
See `docs/analysis/recovery-reserve-budget.md`. The 10-GiB minimum is inherited
from the repository estimator's `max(10 GiB, DB / 5)` recommendation, repeated
in the direct estimator/enforcement. The observation harness additionally
keeps that entire reserve free during every temporary peak. Separate temporary
workspace from the persistent emergency floor before choosing a smaller policy.

Fresh read-only VPS collection preserves six OPTIONS/LOG/CURRENT files and
verifies identical DB file metadata before/after. Archive 217,799 bytes / SHA
`19985085219e0f9d1bc093cf201aa8422cc6b26ba95b11473495d04e3f78fccc`.
RocksDB 8.8.1, nine leveled CFs, one configured background job/subcompaction,
64-MiB SST target and WAL soft trigger. Current WAL allocation 66,748,416
bytes is a stopped-node sample, not a whole-run maximum. Max-compaction target
1,677,721,600 bytes is soft, with ignore-input-limit enabled.

The full 21h07 LOG has six successful compactions; largest input/output
52,701,273 / 52,471,122 bytes. The seventh starts at 22:45:30.306018 UTC,
guard minimum is 22:45:30.586684, and its final log reports `Shutdown in
progress: Database shutdown`. Do not count that partial compaction as a pass
or interpret its record-drop counters as historical record deletion. Full
per-file attribution of the global peak is unavailable.

At the unchanged observed peak, 8-GiB reserve would leave 2,136,391,680 bytes
within the 40-billion-byte budget; 6 GiB would leave 4,283,875,328. This saves
zero physical bytes and is arithmetic, not a reduced-reserve qualification.
Recommend first qualifying 8 GiB, then 6 GiB; 4 GiB remains exploratory.
No minimum safe reserve is established. The existing original-reserve goal
and the failed old B3 result remain unchanged.

Private owned evidence:
`evidence/recovery-reserve-analysis-20261018-bd2975/` under the existing
My Passport run. `reserve-assessment.json` is the final reviewed output;
the first `analysis.json` remains intact. Native B7 input and outstanding
instrumented-suite/benchmark work from the next entry are unchanged.

## Final B3 sustained-disk result and current build handoff (2026-10-18)

**Supersedes older live observer/build statements below.** The user asks what
"minimum margin" means. It is the smallest observed extra allowance within
40,000,000,000 whole-server bytes, after accounting for occupied/unavailable
whole-device space and the unchanged 10-GiB recovery reserve. The earlier
positive approximately 24-MB sample is superseded by a failed peak.

VPS handle **28096 is failed/closed**. `b3-day1-r1` stops at 22:45:32 UTC on
17 October after 76,023.649 seconds (21h07), guard reason `strict 40 decimal GB
whole-server budget exceeded`. Minimum margin **-11,091,968 bytes**, strict
accounted peak **40,011,091,968 bytes**; minimum physical root available
11,687,899,136 bytes remains above the unchanged 10,737,418,240-byte reserve.
The controller stops the native process cleanly, exit zero. Final margin
24,309,760 bytes is positive after shutdown but does not erase the failed peak.
Peak native HWM 407,150,592 bytes, host available minimum 3,033,333,760 bytes;
zero configured swap and unchanged OOM/swap counters. Head advances 24,651
blocks to 40,296,733. No completed-day, final-source or producer claim follows.

Read-only SSH verification at 23:59:48 UTC finds no owned native process,
unchanged hold/absent key and exact final report SHA
`807d96495dde09eacd15ed5cb76caf10c853cf6fdd057b7a1fec1973a5895aaa`.
No restart, reserve change or data cleanup is performed. Failure evidence:
`evidence/dictionary-policy-20261016/actual-baseline-restore-20261017/long-observer-20261017/`.
`b3-day1-r1-disk-guard-failure-preservation.json` now verifies all nine failed
namespace members, archive 5,337,957 bytes / SHA
`15234001dd889865a49b5fee0e00d9d9ab6ac7e159524b84493f93376d873460`.
Fresh post-archive inventory is identical; no remote files are retired.
The exact peak's file/compaction attribution remains to be established.

B7 normal Linux handle **47847 is complete/closed**: all 63 entries and all
86 targets pass for manifest `a21a8958e1757a8f2f661264b5b8665158c6db33c9c31a2fed8991d39e962a20`.
Native ELF `0a8c0d3c17d28c3b91363332bdf5607c8063c3d093b69b5d55fc298be4d71f0f`.
Instrumented core's detector controls, three core tests and node build/CLI pass,
then handle **43424 fails/closes on coordinator state bookkeeping** after
canonical timeout-profile restoration. This is not a native regression or a
full sanitizer pass. Original r1 result must remain failed; preserve it before
a fresh explicitly labelled continuation. The remaining 60 instrumented tests
have not run. Benchmark waiter **51114 is now verified failed/closed** because
the prerequisite pipeline fails; no measurements or benchmark compile occur.

Private archive helpers now preserve closed ELF archives in verified long-window
Zstandard packs, and re-encode selected fixture gzip archives while retaining
every raw tar byte. B7 normal SC5 falls from 678,278,228 to 34,876,322 archive
bytes; all 980 members and decoded raw tar SHA match. These are development
evidence savings, not node disk savings. The My Passport 70-GiB floor remains.
Pending: preserve failed instrumented r1 and exact core binaries, seed a fresh
r2 result from verified core records, fix only the private coordinator ledger,
rerun detector controls and remaining instrumented suite, then fresh paired
cleanup measurements. Product source must remain frozen during those checks.

Earlier goal-tool registration text below is stale: a saved receipt records
the user's precise English objective registered at 22:12 UTC, under
`evidence/objective-precision-20261017/new-goal-registration-20261018.json`.
The reusable goal and all 11 unchanged acceptance criteria remain the contract.
Current goal-tool lookup returns null; this is not evidence that any requirement
is complete. Do not mark the goal complete or recreate it to hide missing work.

## Precise objective and B7 integrated Mac checkpoint (2026-10-17)

**Supersedes the source/build status in earlier entries below.** The human
requested a more precise objective after a cybersecurity flag. No exact flag
reason is available. Avoid pretending to identify it or promising no future
alerts. Describe the existing builder as the operator-owned Linux
build-and-test machine, keeping Koinos/node/producer purpose explicit.
The reusable objective in `docs/implementation/issue-9-goal.md` now specifies
decimal RAM/disk bytes, lossless full-history requirements, the unchanged
reserve, actual target-host memory/workload evidence, exact before/after inputs
and separately scoped account/access/mainnet operations. All 11 acceptance
rows remain verbatim. GitHub issue 9 is updated and freshly read back at
21:40:33 UTC; body SHA `b7f79938da1c868e749c9c643e7517253820ad80d2333f49a9f1db771ae21f0d`.
Evidence: `evidence/objective-precision-20261017/`.

The registered goal remains active with its original text; available goal tools
cannot edit an unfinished goal's objective. A direct supported native-app
inspection is refused by the computer-use tool's Codex-app safety restriction.
Do not bypass that restriction, alter application storage, falsely complete the
goal or recreate it to hide missing work. Canonical issue/plan wording is
updated, but do not claim the registered objective text changed.

B7 combined patch source SHA remains
`3fb80a565fe46f8a8ebef607fa692ff914243db4f795639053a0d901f93ba4b1`.
First integrated Mac handle 96940 is failed/closed: the recipe's five tests pass,
but the new native regression assumes an allocation request of exactly 8193
bytes; libc++ rounds differently, so no payload failure is injected. Exact
source, failed fixture/object, runtime and build inputs are member-verified in
`protobuf-arena-integrated-b7-20261017/exact-failed-source-and-fixture.tar.gz`
(316 members, 2,906,188 bytes, SHA
`cd2fcdaa6572df0b00ab81c15f3cf81fbd697a1ab6f92f1a06f4ce1dcedb02f5`).
The old result is not relabeled. A prior archive preparation attempts a Make
link-file path in the Ninja build and refuses before opening an archive; the
corrected preserved inputs include `build.ninja`.

Native regression now sweeps ordinary allocation ordinals, positively checks
a refused request large enough for the 8-KiB payload and requires eventual
success, valid empty/default bytes on refusal, successful field reuse, complete
destruction and byte-exact protobuf roundtrip including NUL. No detector or
runtime behavior is weakened. Fresh manifest is **311 inputs / SHA
`a21a8958e1757a8f2f661264b5b8665158c6db33c9c31a2fed8991d39e962a20`**.
Exactly five build/patch/test paths change from diagnostic manifest 6dde; all
other 306 inputs match. Fresh source archive is verified member by member.

Mac handle **38698 is complete/closed**: complete build, five recipe checks,
focused ownership/pending-budget tests, version/help and **all 63 normal CTest
entries pass in 224.218 seconds**. Native ELF is
`e50d79cda78ea16ff31f415bcb717bcf5f9d4975f8d073f54c7ea496331ee98d`.
Evidence: `evidence/protobuf-arena-integrated-b7-r1-20261017/`.
Mac closed-fixture archival handle **79488 was live at the last check**, using
the proven long-window Zstandard verifier with `-M256MB`. Check its result
before repeating any archive/retirement; no fallback below 70-GiB floor.

Linux source update handle **83825 is complete/closed**: all old 311 B6 bytes
are verified first, five staged replacements are verified, then all new 311
bytes are checked. Existing export path `source-B6-20261017` now contains B7
bytes deliberately to reuse unchanged objects; its directory name is not
source identity. Frozen historical `source` and installed dependencies remain
untouched. New local/remote normal and ASan namespaces are prepared by
`protobuf-arena-integrated-b7-r1-20261017/prepare-linux.py`; **no new Linux
build/test is launched yet**, and core orchestration still needs completion.
Do not launch old B6 source guards after this source update.

Next: review the generated B7 configure/command/properties guards, create a
core/full normal-then-instrumented coordinator, retain detector controls,
assertions, two RTTI wrappers, GC main/transport and all resource limits.
The ASan core's tests need the reviewed diagnostic profile first; restore the
canonical CTest file before the existing full coordinator reapplies that
profile. Download the updated configured plan/targets/roster and canonical
files before full coordination; all 63 entries/86 targets require current
execution, not prior passes. No B7 Linux ELF or success result exists yet.
Budget evidence growth before launching the full ASan archives; prior 15
instrumented ELF archives already total 553,605,481 bytes. Preserve the 70-GiB
external floor; do not silently weaken archive or process-closure checks.

Control-workspace retirement handle **81254 is complete/closed**: both old
arena-control RAM workspaces are fully represented by verified archives and
fresh all-member hashes; all /proc exe/cwd/maps/cmdline/fd references are empty.
Only these closed copies are removed (593,309,696 allocated bytes), shared
free 5,444,440,064 to 6,037,749,760. Original native builds/objects remain.
Proof: `protobuf-arena-string-r1-finish-20261017/control-workspace-retirement-proof.json`.
Actual VPS observer **28096 remains separately live on B3**; read fresh metrics
before using old durations/margins. No mainnet producer activation, commit,
push, package or Koinos One pin change occurred. The full objective stays open.

## Arena-construction causal control and current qualification (2026-10-17)

**This checkpoint supersedes older live handles and Linux status below.**
Normal handle 59259 is complete/closed: all 63 normal Linux entries and all
86 targets for manifest `6dde3b266993faf8898f277ae8584f0c86eb659056fc604067dbbe84d2612688`.
`source-export-provenance-r1-20261017/normal-linux-audit.json` independently
verifies all 45 earlier valid and 18 fresh JUnit records; this is staged, not
one CTest invocation. Normal ELF remains `cab9f6ea08bbe67f8236a9dd21d71b8f0fb1cbeb34607c3e66ec09e68f12c2b9`.

ASan pipeline 70231 is failed/closed after fresh detector controls and 17
passing entries. The pending-record-budget test reproduces invalid arena
string destruction after payload-copy allocation failure, UBSan exit 92.
Its complete 540-member namespace is archived (305,913 bytes). Comparison
waiter 44837 is failed/closed without compiling or measuring the benchmark.
Neither failed launcher may be restarted unchanged.

Isolated arena control 21521 is failed/closed at its unchanged 3-GiB cumulative
shared-workspace limit after minimal original/corrected exits 92/0 and native
original exit 92. Exact 12-member workspace archive SHA
`4c9053a316cd9b70fbe6683dae13d60b9fdcb3fb4eb2b269258f74a62f17dfd4`
is verified; fresh hashes and all /proc references permit retirement of only
three completed duplicate ELFs (678,547,456 allocated bytes).

Fresh completion handle **97169 is complete/closed**. The corrected runtime
with the exact original native test object passes (exit 0, kernel peak
459,710,464 bytes, owned swap zero, global OOM 1 to 1). All other runtime
members and link arguments remain identical; only `arenastring.cc.o` differs.
Its 45-member namespace and exact corrected ELF are archived/verified.
Evidence is under `protobuf-arena-string-r1-finish-20261017/`.

The combined private runtime patch is now present in source, SHA
`3fb80a565fe46f8a8ebef607fa692ff914243db4f795639053a0d901f93ba4b1`.
It default-constructs a valid arena string before a throwing payload copy;
public headers/layout/version/schema remain unchanged. Build guards now check
both source preimages/postimages. Native and recipe regressions are extended.
**No fresh combined build, manifest or integrated pass yet.** Earlier B6
normal/CLI identities do not qualify this change. Source/export/build refresh
and full normal/instrumented checks remain next; no integration commit.

Actual VPS observer handle **28096 remains live** on frozen B3, PID116200,
start12049001. Fresh readback at 19:21 UTC: 63,763 seconds / 17.7 hours,
head40,292,761, native HWM407,150,592, minimum host available3,036,475,392,
no swap/OOM; minimum strict 40-decimal-GB disk margin only28,901,376 bytes.
This is unfinished B3 observer evidence. Do not change/restart its runtime,
restore target or producer-disabled configuration during the run. Seven-day,
final-source restore/sync/producer/recovery/historical gates remain open.

## Source-export diagnostics continuation (2026-10-17)

**Latest verified scope update supersedes the older live handles below.**
25955 is **failed/closed after 45 valid passes**: the P2P command/CTest return
zero after 29.70 seconds, but the sampler reads `memory.swap.current` after
systemd removes the completed cgroup and receives ENODEV. There are no residual
owned processes; global OOM remains 1. The failed test is not counted as passed.
The exact 362-member namespace is verified in a 159,172-byte archive under
`scope-retirement-race/`. Waiting handles 1262 and 88289 are likewise failed/
closed; neither starts the full instrumented suite nor the benchmark. Never
relaunch those failed scripts unchanged.

Six shared-monitor guard controls pass locally and on Linux. Three real Linux
scope retirements hold an open counter FD, reproduce ENODEV and preserve the
same 2.5-GiB / zero-swap limits with OOM 1 to 1. New private helper SHA-256:
`83569a691d1cf06742f468e523c9d69151bb7916930eabf5c4d8af756b0c102d`.
It requires a positively read scope before actual command exec. Retired counter
reads are terminal only after that proof, directory removal and no live owned
process in the scope. Active/unverified/remaining-owner/other failures still
abort, and all other resource/lifetime checks remain. This is a diagnostic
sampling fix; no native source, manifest, command flags or ceiling changes.

**59259 is live**, at
`source-export-provenance-r1-20261017/linux-normal-r5-scope/run-full-checks.py`.
It retains the 45 valid exact-source passes and reruns the failed P2P case,
which now passes with the positive gate. At the last sample it has 62 valid
entries; terminal all-63/all-86 audit is still required. Only prior native
archives are restored by verified hash; no old failure is relabeled. **70231
is live**, waiting for that exact normal proof, at
`source-export-provenance-r1-20261017/run-reviewed-normal-then-asan-scope.py`.
It will launch `linux-asan-r6-scope/run-full-checks.py`, with all 63 instrumented
entries required again and the reviewed gate. All earlier detector/RTTI/GC/
timeout/resource requirements stay. **44837 is live**, at
`evidence/cleanup-copy-overhead-r2-scope-20261017/run-after-native-checks.py`;
it waits for both suites, preserves both namespaces and runs the unchanged
paired component comparison. No benchmark compile/measurement yet.

Mac archival handle 48478 is failed/closed only because its numeric decoder
cap `-M256` is rejected before the first tar member. Original fixtures and
compressed archive remain unchanged. Corrected verifier **38165 is now
complete/closed**: explicit `-M256MB` validates all **14,561 members** of the
same **42,400,920-byte** long-window Zstandard tar. Fresh whole inventories/
hashes and empty recursive lsof scans precede retirement of both closed owned
Mac artifact/tmp roots. Retired st_blocks with inode dedup are 1,253,892,096
bytes, not APFS exclusive-extent proof. Native build, source, original corpus
and operational data remain. Proofs: `mac-closed-test-artifacts-*.json`.

Current source manifest is now **311 inputs / SHA-256
`6dde3b266993faf8898f277ae8584f0c86eb659056fc604067dbbe84d2612688`**,
under `evidence/source-export-provenance-r1-20261017/`.
Exactly `tools/backup/record_sc0_baseline.py`,
`tests/scripts/fixture_paths_test.py` and `tools/backup/README.md` differ from
the earlier C8/B6 source. All C++/headers/config/CMake/dependencies, both Linux
compile-command hashes and all three native ELF hashes are unchanged. The full
new source archive is verified against all 311 entries. Do not reuse the old
source-export guards expecting C8 after this explicit three-file update.

Previous handle **51540 is failed/closed** after 41 passes. Its SC0 harness
assumed Git metadata in the deliberately Git-free exported source and failed
before native fixture preparation. Complete failed namespace: 421 members /
304,628-byte verified archive. Both waiting handles **58993 and 51709 are also
failed/closed** after refusing their failed prerequisite; no ASan full run or
benchmark was launched by them. Preserve these states; do not relaunch.

SC0 now verifies an explicitly selected `TELENO_TEST_SOURCE_MANIFEST` when its
exact source root is not a Git checkout. Listed files must match canonical
unlinked regular paths, size and SHA-256. It records unavailable Git metadata,
the manifest digest and separately declared base/branch, with null tracked diff.
It never fabricates `.git` or a commit. Real Git checkout collection remains.
The nine meaningful artifact/provenance tests pass. A fresh full Mac run,
handle **15477 complete/closed**, passes all 63 entries in **156.437 seconds**,
plus version/help with unchanged ELF `e00e20045fcd597f931245cbd378d153a203c1054c7a817c8d37fdd2c004611d`.

Fresh exhaustive normal Linux handle **25955 is live**, at
`source-export-provenance-r1-20261017/linux-normal-r4-export/run-full-checks.py`.
All 63 tests run again from zero; old verified native objects/ELF archives are
reused, with no old test passes carried. At the last verified sample 45 entries
pass. The previously failing SC0 entry now passes actual file/HTTP restores and
both graceful negative cases. Its retained environment verifies all 311 actual
listed files against the new manifest; proof `linux-SC0-provenance-proof.json`.
Old native ELF archives are referenced without copying them again, and restored
only by exact archive/member hashes when needed. Only the node remains staged.

Fresh pipeline **1262 is live**,
`source-export-provenance-r1-20261017/run-reviewed-normal-then-asan.py`.
It waits for exact normal all-63/all-86 completion before launching
`linux-asan-r5-export/run-full-checks.py`. That full instrumented suite remains
prepared until the prerequisite passes; all 63 entries will run again, with
fresh clean/UAF/alignment controls, all 120 detector/assertion commands, the
same two RTTI wrappers, reviewed GC main/transport and diagnostic activation
300 seconds (normal 180), unchanged memory/swap/root/host/shared-space limits.
It restores the original generated CTest file on successful completion.

Fresh comparison waiter **88289 is live**, at
`evidence/cleanup-copy-overhead-r1-20261017/run-after-native-checks.py`.
It waits on pipeline 1262, preserves both completed namespaces, then runs the
same prepared 48-process comparison. No benchmark compile/measurement yet.
All earlier failed prerequisites/diagnostic inputs remain preserved. Do not
duplicate any live handle after an observation timeout. The historical fourth
range is still not restarted; wait for compilation work to finish.

Reference-VPS read-only nonlive-artifact inventory verifies the unchanged B3
observer and skips its live basedir entirely. All nonlive owned qualification
files together occupy only **156,712,960 allocated bytes**, including the live
binary's parent directory; not all of that is disposable. Runtime block storage
already selects Zstd level 15 / 64-KiB blocks / 128-KiB dictionary. At about
31,763 seconds, native peak remains 358,961,152 bytes, host available minimum
3,086,118,912 and strict whole-server margin minimum 86,089,728 bytes. No live
configuration or database changed; artifact cleanup alone cannot create broad
long-duration disk headroom. Whole-node/B6/prod/recovery gates remain open.

Closed Mac fixture archival is handled by **48478**,
`source-export-provenance-r1-20261017/archive-mac-owned.py`, using a member-
verified long-window Zstandard tar to avoid retaining repeated synthetic payload
copies. Check its terminal result/retirement proof; never infer retirement from
the compressed file alone. All source, operational data and native build stay.
Earlier continuation entries below are historical and superseded where noted.

## Latest continuation: compiler scope and preserved checkpoints (2026-10-17)

Latest readback at **10:17:47 UTC**: 51540 is still running, with **41 normal
test passes and 57 unique targets built**, no full-suite completion and builder
OOM counter unchanged at 1. Current-source check confirms all 311 native files
still match the B6 manifest; native guides are identical and both repositories'
whitespace checks pass. Pipeline 58993 still waits for normal completion;
benchmark 51709 still waits for both native suites. Do not duplicate them.
Strict Koinos One MkDocs and rendered updated CLI/scope text pass; proof:
`evidence/protobuf-runtime-candidate-r1-20261017/koinos-one-documentation-handoff-gc-success.json`.
Issue #9 readback at **10:17:33 UTC** confirms the incremental GC/CLI checkpoint
and all eleven unchanged unchecked acceptance rows. Public body SHA-256:
`03e21b77e5241780ea14124be6b3b71e64ea40107d1d761eed17ce241c7da830`.
Prepared/executed evidence is in `issue-checkpoint-gc-success/`. The issue and
goal plan retain the explicit own-software/own-VPS cost/RAM/disk purpose and
all history, producer and final qualification requirements. No promise about
automated flags or change to the registered goal's immutable objective.

Pipeline/measurement update: **58993 is live**, running
`evidence/protobuf-runtime-candidate-r1-20261017/run-reviewed-normal-then-asan.py`.
It waits for 51540's exact terminal all-63/all-86 normal proof and checks the
current 311 local source inputs before launching the prepared ASan/UBSan suite
at `linux-asan-r4-staged/run-full-instrumented.py`. At the latest readback the
normal run has **33 valid passes**, including canonical-180-second activation;
there is no full-suite pass yet. The instrumented suite is **prepared, not yet
launched**. It requires fresh three detector controls, keeps all 120 instrumented
commands/assertions and the two RTTI-aligned wrappers, extends compiler GC only
to the already reviewed main/transport pair, and applies the earlier reviewed
300-second diagnostic activation profile only to its generated test file.
Normal timeout remains 180. All memory/swap/root/host/shared-space ceilings stay.
Only the node ELF stays staged; other closed ELFs are archived/rehashed and
exactly restaged when needed. Finish restores the canonical test file.

**51709 is also live**, waiting on that complete normal+instrumented pipeline
before preserving both final diagnostic namespaces and running
`evidence/cleanup-copy-overhead-20261017/run-after-native-checks.py`.
The benchmark is **prepared, not compiled or measured**. Its fresh diagnostic
fixture compares remove/expiry over 64 records with 64-KiB signatures, 32 old
expiry markers and four views. Separate tracked/truly-unprobed objects, verified
new/delete symbols, eight excluded warmups and forty measured processes give
five alternating pairs per mode/probe. Both variants use the same corrected
B6 protobuf and link inputs; only the mempool object changes in a copy of the
preserved B4 three-member library. Ordered full wire/nonce/count/RC/membership
oracles are outside the timed phase. Phase CPU/wall/I/O/page-fault metrics and
background host counters are planned; ordinary requested bytes are not heap/
RSS and the process peak includes setup/oracles. No claimed savings yet.
Failure in either prerequisite refuses benchmark launch; no automatic retry.
Check actual processes and files before any restart, and preserve partial
workspace even if compilation stops before the final binary index is written.
Historical range-003 retry is still **not launched**; wait for compilation work.

Latest verified coordination update: handle 55578 is **failed/closed**, after
four valid test passes. Its local driver assumed CTest's configure-time JSON
always includes `command`; CTest omits that field for unbuilt executables.
The complete remote namespace is preserved (163 members, 98,512-byte archive).
Do not relaunch that driver. Reviewed continuation **51540 is live**, under
`evidence/protobuf-runtime-candidate-r1-20261017/linux-normal-r3-roster/run-full-normal.py`.
It retains the four valid passes, resolves all 63 full commands from the
generated canonical `CTestTestfile.cmake`, builds referenced executables, and
requires the fresh CTest command to match before execution. The initial
preparation correctly refused the activation test's extra node argument;
the completed preparation retains that full command. No source, test assertion,
timeout, detector or resource limit changes. Inspect actual progress/PIDs before
action. Earlier live-normal descriptions below are superseded by this update.

Reference-VPS B3 observer readback at 10:08:25 UTC is saved as
`evidence/dictionary-policy-20261016/actual-baseline-restore-20261017/long-observer-20261017/live-readback-20261017T100825Z.json`.
PID/start/exe/config and disabled producer arguments match. At 30,584 seconds,
head advances from 40,272,082 to 40,281,973; sampled/native peak is 358,756,352
bytes, minimum host available 3,086,118,912 bytes, owned swap and host OOM zero.
The strict 40-decimal-GB margin reaches a tight 86,089,728-byte minimum and is
143,409,152 bytes at the last sample. This is an unfinished older-B3 run;
neither 24-hour completion nor B6 applicability follows.

Latest verified update: handle 88021 is now **complete**, with preserved ASan/
UBSan node build, version/help and ELF SHA-256
`7ed75fef50a359e135e2186cd1177d89441ece29b12a005a115206e14f01e15e`.
The incremental continuation reuses retained objects and completes in 284.472
seconds / 276.79 CPU seconds, kernel peak RSS 2,090,115,072 bytes; owned swap
is zero and the fresh epoch's OOM counter stays 1 to 1. Do not relaunch it or
describe this as a clean combined build/full diagnostic suite.

Normal all-target/all-63 continuation is now **live under handle 55578**:
`evidence/protobuf-runtime-candidate-r1-20261017/linux-normal-r2-epoch/run-full-normal.py`.
It uses the same normal core/ELF and two earlier valid test passes, an explicit
new OOM epoch and the improved failure-capture driver. Its first kept fixture
is already built and archived; inspect progress/actual PIDs before action.
The old normal handle 8172 remains a preserved refusal, not this continuation.
Wait for compilation checks before restarting historical work, avoiding another
wasted pair if a build-scope event occurs. Then prepare the full ASan/UBSan
staged suite with its recorded diagnostic activation-timeout profile and fresh
detector controls; do not lower assertions, limits or history requirements.

Two closed component ELF duplicates are now retired after archive/member/hash
and exe/cwd/maps/cmdline/fd checks, freeing 404,930,560 RAM-filesystem bytes.
The component objects/libraries and complete external archives remain. Proof:
`evidence/protobuf-runtime-candidate-r1-20261017/closed-component-ELF-retirement-proof.json`.
Strict Koinos One MkDocs and rendered source/scope checks pass with no app/code/
package/submodule change. Issue #9 readback at 09:52:18 UTC matches the prepared
checkpoint and retains all eleven unchecked acceptance rows. Earlier snapshots
below are historical; the verified live normal handle above takes precedence.

The goal remains active and unqualified. The current native source is still the
311-input B6 manifest
`c8bad5024f0750d566145ba251d3fce72f52a7ee766414eadc57f447e466b1f2`.
All 63 normal Mac entries pass. The normal Linux `teleno_node` build,
version/help and two new tests also pass; its ELF SHA-256 is
`cab9f6ea08bbe67f8236a9dd21d71b8f0fb1cbeb34607c3e66ec09e68f12c2b9`.
The exported Linux version embeds the full base revision without a dirty
suffix; it still requires the working-tree manifest and is not an integration
commit or release. Normal core handle 52653 is complete and preserved.

ASan/UBSan's complete rebuilt protobuf runtime and both new tests pass, but
the node build does not. Kernel/unit evidence in
`linux-asan-r1/attributed-build-scope-oom.json` identifies `cc1plus`, compiling
`libp2p_transport.cpp`, killed inside the owned 2.5-GiB / zero-swap scope.
The builder global OOM counter moves 0 to 1. The original wrapper's residual-
process message was an incomplete classification; the exact failed build and
namespace are fully archived and member-verified. No diagnostic CLI exists
from that failed attempt. The target VPS is a separate host and remains B3.

The global guard consequently stops historical range-003 and refuses the first
normal full-suite build action before launch. Handles 73226, 8172 and 41917
are terminal failed; do not relaunch them. The normal core remains valid for
its exact completed interval. Range-003's source/forward/worker are closed,
original-corpus metadata is unchanged, and its entire partial state/trace is
preserved in `compiler-scope-oom-stop.tar.gz` (94,778,266 bytes). Never treat
that fourth pair as complete or running.

A reviewed private compiler profile extends the existing `main.cpp` GCC GC
parameters to `libp2p_transport.cpp`. Its diff proves exactly one command gains
two GC settings; every other command, both detectors, assertions and all
original memory/swap/root/host/shared-memory limits remain unchanged. The
first GC continuation, handle 37977, passes configuration then stops before
compilation because unchanged model files were omitted. It is preserved.

Current actual compiler continuation is handle **88021**, under
`evidence/protobuf-runtime-candidate-r1-20261017/linux-asan-r3-gc/run-core.py`.
It includes the complete unchanged 86-target / 63-test model and uses the
already verified GC profile. Re-poll that handle and verify actual descendants;
do not restart from a state file. Its new measurement epoch explicitly retains
the old OOM and uses the current counter as its baseline. Failed-case samples
and residual process details are now saved before cleanup; OOM is classified
before lifecycle checks. A successful retry still requires CLI and broad tests.

Next, once the compiler cause is resolved: prepare a reviewed fresh normal
continuation/epoch from the completed normal core with unchanged binary/source
and the two earlier test passes. `linux-normal-r1/run-full-normal.py` contains
the prepared staged all-target/all-63 flow, but its original epoch expects OOM
counter zero and is terminal failed. Do not reuse it unchanged. Its new
`miner-runner-full.py` includes a terminal audit with a fresh ELF hash. Keep
distinct temporary/artifact roots and per-case verified offload. Then resume
historical testing from range-002's verified seed in a fresh namespace after
preserving range-003; retain every field, deadline and resource floor.

Closed remote range-001/002 copies are retired after archive verification,
fresh whole-inventory hashes and empty exe/cwd/maps/cmdline/fd scans, freeing
243,687,424 root bytes. Their complete external archives and the original
corpus remain intact. The source has no further native edits after the Mac
and normal core proofs. Additional copy/merge failure paths, five paired
cleanup measurements, all full Linux suites and final producer/40-GB/whole-node
qualification remain required. The goal must not be marked complete.

## Current B6 ownership correction (2026-10-17)

The previous goal turn made progress: it preserved the B5 unknown-field failure,
updated the honest issue/plan scope and retained every acceptance gate. This
continuation isolates and corrects unknown-field publication ownership in the
same pinned Koinos protobuf revision. See
[the current report](validation/protobuf-unknown-field-ownership.md).

Two minimal original-library controls reproduce 32-byte string / 24-byte group
leaks. Twelve corrected controls pass. Relinking the identical B5 test object
and mempool library with only `unknown_field_set.cc.o` replaced eliminates the
640-byte / 20-allocation report: all 303 removal and 3,623 expiry cases pass
with original state assertions, ASan/UBSan/leak checking and resource ceilings.
The complete diagnostic namespace and exact workspace are member-verified in
`evidence/protobuf-unknown-ownership-r1-20261017/`. The first buffered-stdout
verifier failure remains preserved; no diagnostic failure is hidden.

The product build now reconstructs that runtime in its own build tree, with
archive/preimage/result/header checks and unchanged installed inputs. Current
311-input manifest is
`c8bad5024f0750d566145ba251d3fce72f52a7ee766414eadc57f447e466b1f2`.
All targets, version/help and all 63 normal Mac entries pass. Exact Mac node
SHA-256: `e00e20045fcd597f931245cbd378d153a203c1054c7a817c8d37fdd2c004611d`;
version `1.3.0-dev.0+d3cfb94455da-dirty`. No integration commit/package exists.

Linux normal/instrumented core coordinators are
`evidence/protobuf-runtime-candidate-r1-20261017/linux-normal-r1/run-core.py`
and `linux-asan-r1/run-core.py`. Current handles are 52653 / 73226; re-poll
them or verify actual descendants before action. Both configure all 120 native
commands and 63 CTest entries. The normal and instrumented full runtimes and
both new tests pass; CLI compilation and the remaining full suites continue. Initial configurations
omitted the prior static-zstd selection; complete failures/models are preserved,
and the fresh R1 restores exactly those existing dependency paths. Full 63-entry
Linux and integrated suites remain required after these core checkpoints.

Both former Linux build directories (`build` under the owned miner run and the
owned RAM run) are now retired after complete off-host archives, every-file
rehash and an empty exe/cwd/maps/fd reference scan. Do not use old runners that
expect those paths. Their archives/proofs are in
`evidence/linux-B4-build-offload-20261017/`; frozen B3 source and installed
dependencies remain intact. Current B6 source is a separate 311-file verified
copy; the actual VPS still runs B3. Closed current Mac test artifacts are fully
preserved in an 848,371,022-byte archive before retirement; no open handle or
unverified duplicate is removed.

Historical range-002 now passes all 50,060 rows and all five fields across its
60-row overlap, reaches 30,654,141 and independently matches the unchanged
legacy endpoint. Both stopped state copies reopen read-only at durable height
30,654,081 with exact trace ID/root and unchanged file bytes/metadata. Its
complete archive is verified (84 members / 97,475,687 bytes). The full interval
and native-indexer/final-source applicability remain open. The separate B3
24-hour VPS observer remains supervised; do not replace its binary mid-run.

Next: complete the current Linux core runs, stage all remaining tests without
exceeding preserved disk/RAM floors, qualify additional protobuf copy/merge
failure paths and measure five paired cleanup workloads. Fourth historical pair is live under handle 41917, targeting 30,704,141; actual
native PID/start identity was verified. Its initial preparation failure occurred
before remote installation and is retained. For subsequent ranges, consider
100,000 new blocks only if the unchanged two-hour phase and four-hour-plus-five-
minute coordinator limits remain justified by measurements; preserve every
trace/overlap field and input. This can reduce repeated seed copies.
Continue historical validation through the full required interval. Preserve all whole-node/producer/40-GB,
CPU/I/O/latency, TSan, sustained-operation and recovery gates.

## Latest cleanup decoding candidate (2026-10-17)

The current source now has a further mempool cleanup-decoding ownership change,
not yet qualified. Earlier B4 normal/benchmark/producer passes below retain
their exact frozen `93a2cc29...` source identity; they do not qualify this change.

R4 integrated diagnostics reaches 52 passes and stops in the cleanup fixture's
protobuf transaction construction, before entering the operation. The failure
namespace and exact closed build are fully archived/member-verified (747 /
1,952 members, 889,250 / 274,560,243 bytes). The test now prepares removal IDs
outside its allocation-failure window. With that correction, all cleanup
invariants pass but LeakSanitizer reports 320 bytes in ten repeated-string
allocations while the actual cleanup is faulted. That focused failure and its
exact object/executable are preserved under `evidence/cleanup-fault-boundary-20261017/`.

The native cleanup parser now returns a reference owned by a caller-scoped
protobuf arena; removal, expiry and account-resource cleanup keep partially
decoded records owned through failure, without a return-message copy. Encoded
pending values, RC/nonce behavior and failure-atomic publication remain required.
The cleanup fixture also includes pending/transaction/header unknown fields.
Current changes are in `src/koinos/mempool/mempool.cpp` and
`tests/mempool/cleanup_atomicity_test.cpp`; the earlier pending-record fixture
change remains. Source has not been installed on the VPS or publisher.

The identical preserved corrected test object now passes against the B5
library: removal 271 cases / 270 refusals, expiry 3,575 / 3,574, all original
invariants valid and no sanitizer/leak report. The added unknown-field fixture
passes the state invariants (303 / 302 removal and 3,623 / 3,622 expiry) but
LeakSanitizer reports **640 bytes in 20 allocations** in protobuf unknown-field
parsing. The runner is terminal failed, with no automatic retry. Keep both
detectors, leak checking, unknown fields and the original resource ceilings.

The current 306-input manifest is
`7e8108f2a723aaba569009d5e51f0b2f6e5a3209df67fcd69a949de8beb45500`.
`evidence/cleanup-decode-arena-20261017/unknown-field-failed-result.json`
records the failure. Its complete diagnostic namespace (54 members / 697,457
archive bytes) and exact RAM workspace (5 / 50,237,054) are preserved and every
member is verified. The preceding Mac B4 build is also fully archived and
verified (975 members / 254,506,030 bytes); it has not yet been rebuilt for B5.

Next: investigate unknown-field ownership during an interrupted decode, then
repeat the same fault sweep before broad B5 Mac/Linux/integrated regressions.
No B5 full-suite or target-host pass is claimed. Do not relaunch failed R4 or
focused coordinators. The B3 24-hour VPS observer and historical range-002 are
independent and remain supervised; the latter has finished native replay and
is running its reference half. Live PID/executable identity was checked again
for the observer; it remains the unchanged B3 binary with production disabled.

## Current candidate validation continuation (2026-10-17)

The current goal continuation makes concrete progress: the normal B4 Linux candidate
now has all 61 passing entries, all-target/CLI checks and unchanged source,
ELF and compilation commands. This is the union of 55 original passes, two R1
ext4 passes and four R3 individual passes; the original full run remains failed
in its preserved XML. `evidence/normal-linux-arena-20261017/normal-final-result.json`
records the exact scope. The undersized RAM test filesystem, subsequent ext4
headroom refusal, and owned-swap stop are preserved. Verified offload of closed
development copies frees 1,227,595,776 root bytes. R3 uses temporary child-only
2.5-GiB / zero-swap scopes and per-case verified fixture offload; host/global
settings and existing processes remain unchanged.

The mixed-RTTI control reproduces invalid-vptr, passes when aligned and retains
the wrong-type detector control. Exactly two diagnostic fault-wrapper commands
switch to RTTI; every other command and native CLI byte remains unchanged.
Both affected real tests pass. Integrated diagnostics reach 30 passes, then the
direct-activation test times out at its explicit 180-second property while
137 startup logs / 36 validation child attempts / 73 activation child attempts
show progress. All 10,731 timeout members are archived and verified. A reviewed
generated-test profile sets only this instrumented timeout to 300 seconds,
matching the already requested diagnostic budget; outer deadline stays 600.
The normal 180-second test passes. The instrumented fixture then passes in
215.608 seconds under the 300-second profile; all 11,699 fixture members are
verified in 7,149,278 bytes. The suite reaches 36 passes, then the Python
nonlocal-output negative case fails because the runner placed TMPDIR inside
the admitted artifact root. Its unchanged guard correctly treats that path as
local; the test's mocked launch fails without launching a node. All 583 failure
members are verified in 696,527 bytes. No check, detector or RAM/disk/OOM limit
is weakened. Active coordinator is
`evidence/linux-integrated-asan-r4-arena-r1-20261017/run-separate-tmp-continuation.py`;
remote runner `miner-runner-rtti-v5.py`. It restores the original temporary/
artifact root separation and preserves/retires both roots after each pass.
The unchanged Python ownership test and fixture-path test now pass, followed by
the full instrumented single-copy experiment (63.613 seconds). Its 983 fixture
members are verified in 678,333,405 bytes. Read actual handles/result before action;
do not relaunch. Complete D1/TSan/dependency/lifecycle qualification remains open.

The admission benchmark completes all 72 processes (12 warmups excluded,
60 measured) with exact bytes on all four fork views. Per-profile five-pair
unprobed B4/B3 CPU ratios are 0.9160 / 0.9875 / 0.9540. Ordinary allocation
count ratios are 0.9820 / 0.9820 / 0.9655; requested bytes rise slightly.
This is not heap/RSS or whole-node throughput proof. All 401 evidence members
and six object/ELF artifacts are independently archived/member-verified under
`evidence/admission-copy-overhead-20261017/`; raw results and paired ranges/MAD
are retained. No benchmark process remains active.

The B4 real-mempool controlled producer passes 100 blocks x 128 signed
no-operation transactions, with actual controller admission, producer and event
wiring. A fresh custom genesis funds the test payers after the preserved first
pilot correctly refuses its smaller allowance. Create/reopen wall is
160.492 / 21.874 seconds, kernel peaks 738,041,856 / 621,223,936 bytes. Reopen
from LIB 40 fully executes 60 original blocks and preserves all 12,800
transaction receipts and the exact raw archive/head/root. All 79 members are
verified in 15,084,577 bytes under
`evidence/controlled-producer-transactions-r1-20261017/`. No operational
key/data, listener or external signing/submission is used. Actual target-host
nonempty/PoB/contract/sustained/abrupt recovery remains open; B4 is not installed
on the VPS.

Historical range-001's independent endpoint and cold state-only checkpoint
30,604,081 checks pass. The third full-VM pair is now running in
`evidence/h1-eligibility-20261016/source-bridge-20261017/range-002/`, coordinator
`run-range.py`; target 30,654,141 with all five overlap fields mandatory.
The complete read-only My Passport corpus remains intact. The separate VPS
24-hour B3 observer is verified live at PID/start/executable level; an archived
near-six-hour sample independently matches legacy at 40,278,632. The latest
observed cumulative RSS/HWM is 338,604,032 bytes at 22,029.077 seconds, with
host available at least 3,093,225,472 bytes, zero swap/OOM and strict margin at
least 108,359,680 bytes. These remain provisional,
not a completed day or a final-source qualification result.

Earlier running/failure/queued descriptions below are historical checkpoints
superseded by this continuation. Preserve all exact source/evidence identities.

## Current purpose clarification (2026-10-17)

The user reaffirmed that they maintain Teleno's open-source code and own the
designated reference VPS. Continue the active issue-9 goal as application RAM
and disk optimization to reduce hosting costs for their Koinos cryptocurrency
node and producer workload. Profile allocations, cache/data lifetimes, lossless
database storage and backup/restore workspace; measure CPU, I/O and latency
tradeoffs. The [goal contract](implementation/issue-9-goal.md) and
[implementation plan](implementation/low-resource-mainnet-observer-plan.md)
retain every full-history, receipt, 40-GB whole-server, memory, recovery,
producer and authorization requirement. This clarification changes no
qualification result or production activation status.

GitHub issue #9 is updated to the same application-optimization framing and
read back exactly on 2026-10-17 at 07:03:36 UTC. All eleven open acceptance
items remain. The existing persistent goal stays active; its scope clarification
is maintained in the reusable objective and resumption contract. No matching
Teleno scheduled automation is present in the local automation catalog.

Current diagnostic correction: the normal Linux B4 candidate passes build/CLI
and 55 of 61 tests; six restore/capacity tests refuse the undersized shared-memory
test filesystem. The integrated candidate passes its first 20 sanitizer tests,
including pending-record admission, then stops on a vptr report in the
non-RTTI fault wrapper against diagnostic RTTI-enabled RocksDB. The proposed
fixture ABI cause requires a controlled check. Both failures, the exact ASan
build and both test-artifact roots are archived and member-verified. Neither
suite is qualified; use fresh owned ext4 test directories for the six capacity
reruns without changing reserves. The benchmark has run no cases because its
normal-suite prerequisite failed. Earlier running/queued descriptions below
are historical checkpoints, not current status.

The second historical full-VM pair through 30,604,141 is complete. The final
archive verifier confirms all 60 prior/native/reference overlap rows, including
LIB, with no exemption. Archive SHA is
`34a8db595146b65902cfc283fffd4c3d7bfc39b724b3355d42492858b964e59f`.
An independent endpoint check and the next range remain to be done. The
24-hour frozen B3 VPS observer remains separate from B4 candidate validation.

## Current WAL and dense-backup continuation (2026-10-17)

Latest diagnostic continuation: R1 stops at the 3-GiB shared-memory workspace
guard; R2 stops at the unchanged 2.5-GiB compiler/subtree ceiling. Both failures
and complete workspaces are preserved. R3 tunes only GCC garbage collection
for main.cpp, retaining O1/g1, assertions and both detectors; the CLI builds
at 1,770,262,528 bytes kernel peak RSS. Fifteen tests pass before LeakSanitizer
finds a 32-byte protobuf repeated-string copy leak during admission failure
injection. All 317 evidence / 4,200 workspace members are verified in
221,782-byte / 348,516,142-byte archives. Do not claim a B3 sanitizer pass.

A minimal pinned-protobuf program independently reproduces the leak; all ten
arena-owned positions pass without suppressions. The native per-admission
arena-copy correction passes the actual Linux fault/concurrency/field-roundtrip
suite and all 61 normal Mac tests with all-target/CLI checks. Candidate manifest
`93a2cc291d447139813441b245ea8575f3764e4653b9edd957053f6388bc4202`
changes only mempool.cpp and its pending-record fixture from B3. Candidate
evidence is `evidence/mempool-arena-copy-20261017/`; the closed focused RAM
workspace is fully preserved, freshly rehashed and retired. This code is not
deployed on the reference VPS and does not inherit B3 qualification.

Current **R4 candidate staged diagnostics are RUNNING** in
`evidence/linux-integrated-asan-r4-arena-r1-20261017/` on My Passport and the
corresponding owned miner namespace. Active coordinator
`run-staged-continuation.py`; read
`coordinator-result.json`, `artifact-index.json` and remote `result.json`.
Do not relaunch either coordinator. The original local runner stops after the
passed native build because its local `artifacts/` directory is absent; no
inventory command or test had started. Its failure is preserved in
`coordinator-local-directory-failure.json`. The reviewed continuation creates
only that owned directory and resumes artifact preservation without repeating
controls or build work. Remote source/flags/result remain unchanged.
The frozen B3 source/dependencies remain inputs, with exactly
two canonical candidate source overlays. The first preparation's accidental
whole-target source replacement is refused by the 119-command guard; its
117-command model/evidence are preserved before the corrected exact-source
replacement. All 119 own-source commands retain ASan/UBSan/no-recover and
assertions; 84 targets/61 tests use bounded, hash-preserved artifact batches.
All runtime/host/root/RAM ceilings remain. Full sanitizer, lifecycle/thread,
normal Linux overhead and candidate VPS gates remain open. The 24-hour B3
observer and historical jobs are independent.

The normal Linux candidate build/version/help also pass; its complete prior B3
normal build is archived and member-verified before the exact source overlays
are configured. `evidence/normal-linux-arena-20261017/run.py` is active and
waits for the sanitizer job's actual closure before its 61-test suite. Read
remote `result.json`; do not relaunch. The queued
`evidence/admission-copy-overhead-20261017/run.py` waits for normal checks to
pass, then verifies that only mempool.cpp.o differs between B3/B4 libraries.
Its controlled benchmark has 12 excluded warmup / 60 measured processes across
three signature profiles and separate true-unprobed/ordinary-new-count modes.
It retains four mutable checkpoints and checks complete bytes sorted by ID;
this is not an iteration-order or whole-node throughput proof. No result yet.
The Koinos One developer handoff is updated and strict MkDocs/rendered candidate
identity checks pass under `artifacts/manual-arena-copy-20261017`; no package,
GUI, submodule pin, commit or publication change.

Current continuation: a read-only protobuf source bridge keeps the complete
corpus on My Passport while the authorized Linux builder executes actual
native/tagged-reference VMs. Unknown-field, frame/batch-capacity, read/anchor
and method-refusal controls pass, source metadata unchanged and processes
closed. The first Linux 62-block pair also matches every deterministic full
receipt (events/resources included), computed head/root and LIB, plus nine
final fields; all 91 members are verified in an 86,086,145-byte archive.
The first 50,000-block pair through 30,554,141 completes every full receipt,
computed head/root and LIB comparison, nine final fields and an independent
canonical legacy endpoint anchor. All 77 members are verified in a
97,210,980-byte archive; complete trace SHA is
`0ff441b1549ee6585d836dbc259352423fe15e6ece330febff190a172e558949`.
Source metadata remains unchanged and owned processes close. Local evidence is
`evidence/h1-eligibility-20261016/source-bridge-20261017/range-000/` in the
selected external run; the first controller `run-range-r1.py` is complete.
The next **range-001 is RUNNING**, coordinator `range-001/run-range.py`, remote
owned `source-bridge-20261017/range-001`; do not relaunch. Cold state-only
read-only probes find matching durable checkpoint 30,554,081 and match its
prior trace row. Target is 30,604,141, with 50,000 new and 60 overlapping blocks.
All 60 native overlap rows already match every prior field, including LIB;
the earlier floor-change assumption was incorrect. Preserve original scope
text, but require the separately queued `verify-final-overlap.py` to produce
`overlap-final-full-trace.json` before accepting this pair or seeding another.
It verifies all five fields against both complete archived traces; no LIB
exemption is accepted. Its early proof and correction note are retained.
Full trace harnesses are `trace-r2`; earlier preparation failures
are preserved. Native direct-submit ELF SHA `31b1251470797b6a548d7d24d9ca4f3e8e15dbd8af904a5dd78ecfc9d799e5ef`,
tagged reference SHA `df3a5c77df2249432e3812044ca57728c0a3f36535d445776d08abe398a66da6`.
The native indexer is bypassed only in this diagnostic; its separate complete
checked-range proof remains intact. No new VPS endpoint or SSH key copy.

The one-hour B3 observer completes at 3,601.088 seconds, exit zero, advancing
34,955 blocks (40,236,956 to 40,271,911). Peak and post-900-second sampled RSS
634,728,448 bytes; minimum host available 2,837,860,352; zero swap/OOM change.
Strict whole-server runtime peak including the unchanged reserve is
39,877,636,096 bytes, margin 122,363,904. Stable near-tip head/root also matches
legacy at 40,271,440; reference head/LIB 40,271,501 / 40,271,441. Sixty native
RSS/kernel comparisons pass. One-hour evidence: nine verified archive members,
323,174 bytes. Post-catchup sample window 1,762.544 seconds uses 92.230 CPU
seconds (~2.62% of two-vCPU capacity); RSS still rises, so no plateau is claimed.

The actual-VPS three-way cache comparison completes all 45 measured processes
and functional cases, exact workloads/digests, source metadata unchanged;
296 members / 160,449-byte archive verified. Layout timings depend on host and
key profile; map ownership saves more memory but costs CPU in the 64-byte case.
Neither prototype is integrated; B3 remains frozen for qualification.
The controlled real native producer also completes on this VPS: 1,000 signed
federated blocks, durable LIB 940, clean reopen/full execution of 60 reversible
blocks, exact own head/root and all records retained. Create/reopen wall
7.253 / 0.979 seconds, kernel peaks 27,025,408 / 35,434,496 bytes. p50/p95/p99
producer call 4.444/7.343/9.292 ms. Forty members / 12,378,422-byte archive fully
verified; only this closed generated fixture is subsequently rehashed/retired.
No PoB, nonempty mempool or mainnet production qualification follows.

Long-observer evidence is under
`evidence/dictionary-policy-20261016/actual-baseline-restore-20261017/long-observer-20261017/`
in the selected external run. Its 90-second restart/telemetry smoke passes
(exit zero, 97,034,240-byte RSS peak; 18 RPC latencies, two RSS log/kernel
checks; nine members / 13,560-byte verified archive). The first `b3-day1`
attempt fails native preflight before any node launch: JSON-RPC bind without
reuse immediately after shutdown. A controlled ephemeral Linux socket test
reproduces TIME_WAIT refusal without reuse and success with the runtime's
reuse flag. The unchanged native preflight later passes. Its two preflight
files are verified in a 1,643-byte archive; no DB or check is bypassed.

The fresh **`b3-day1-r1` 24-hour observer is now RUNNING**; local coordinator
`run-day1-r1.py`, live file `b3-day1-r1-coordinator-live.json`, eventual result
`b3-day1-r1-coordinator-result.json`. Do not rerun either the original queued
`run-after-components.py` or `run-day1-r1.py`. The remote owned supervisor and
native exact PID/start-tick/executable/basedir handles live under the same dense
restore's `observer-b3-day1-r1` namespace. At 302 seconds it is ready, mainnet
identity verified, head 40,272,206, peak RSS 130,347,008, host available minimum
3,328,720,896 and strict margin minimum 187,297,792 bytes. These are provisional,
not a day or steady-window pass. Half-second resource/hold/key checks, ten-second
read RPCs, all latency histograms including final in-flight requests, one-minute
persisted samples, unchanged log/telemetry capacities and verified-identity/
heartbeat watchdog apply. Native binary/config remain frozen B3. Seven-day,
expanded mixed load, abrupt recovery, actual PoB and full qualification stay open.

The integrated ASan/UBSan pinned RocksDB rebuild with RTTI now passes, along
with native reconfiguration; all native targets are BUILDING. No native CLI
or 61-test sanitizer pass yet. Source is unchanged B3. Its 771 closed dependency
compiler-intermediate members are verified in a 155,562,867-byte archive,
SHA `f1f85b6ea247884ca40e8d335da345f39f53c8262b4f03b87811a222d72d0d99`.
The first retirement command exceeds the shell argument limit before Python
execution; `retire-preserved-dependency-r1.py` supplies the inventory on stdin
and safely retires only that archived owned build subdirectory. Installed
instrumented library SHA remains
`b7681590068474902258ec12c34490c8925bfce6ea239ae616f376cb0afc6416`;
source/install stay intact, and shared-mount free rises by 768,606,208 bytes.
Original RAM/host/root/RSS/swap guards and detectors remain. Check
`linux-integrated-asan-r1-20261017/result.json` remotely before any new build.

The cost-optimization goal remains active and unqualified. Native branch is
`codex/lr2-memory-hardening`, HEAD `d3cfb94455da4a472f651eb4239abbc289e74225`,
version `1.3.0-dev.0`. Preserve the mixed worktree. No commit, integration,
release, production activation or app packaging follows from these results.
The paragraphs below supersede older active-job descriptions in this file.

The latest actual-VPS checked historical replay finishes all 2,285,238 blocks
at height 32,789,379 with the independent legacy terminal ID/root, zero source
writes/broadcasts and unchanged source metadata. Wall 7,735.086 seconds,
CPU 4,342.260 seconds, kernel peak RSS 488,472,576 bytes, minimum concurrent
host available memory 3,004,477,440 bytes, no swap/OOM change. Full VM execution
is only a 62-block boundary; full historical differential qualification remains
open. All 172 archive members are verified in `baseline-r2-complete.tar.gz`
(96,107,946 bytes); source descriptors/mappings/groups are closed. Only its
owned remote diagnostic namespace is subsequently retired.

Four latest-source producer/VM/storage/RPC fixtures pass on the actual VPS;
the complete 32-member archive is verified before retiring that namespace.
Five actual-VPS cache triplets plus one excluded warmup pass matching program
identities/metering: a 16-MiB cache saves about 61% fixture peak RSS but costs
3.684 times CPU and 7.131 times p95 latency. No default/policy changes follow.
All 99 archive members are verified before retiring that diagnostic namespace.
Evidence is under `evidence/baseline-functional-20261017/` and
`evidence/vm-cache-mechanism-20261017/` in the selected external run.

The prior owned observer's complete rollback set is reverified off-host:
378 previously hashed unchanged base objects, 42 freshly hashed DB delta
members, four runtime/control members, the actual external observer config
and old executable. A guarded replacement rehashes all 424 source files on
the VPS and checks compatible storage lease and all source owners before
retiring only that owned test duplicate. New full restore uses the denser
native backup and current B3 binary; peak disk is accumulated at every
0.2-second observation with unchanged 10-GiB reserve and strict raw-device
accounting. Do not credit a new restore/40-GB pass until its complete ledger
and runtime evidence are available. Exact plans/controllers are under
`evidence/dictionary-policy-20261016/actual-baseline-restore-20261017/`.

The prior owned test node is now retired after all 424 source hashes match,
with the compatible storage lease and source-owner checks. Its complete
reconstruction set remains off-host. Retirement returns 24,879,448,064 bytes
in 109.648 seconds. B3 direct preflight/interruption/resume pass in 0.418 /
12.793 / 383.268 seconds; 19 completed files retain inode/size/mtime. Transfer
strict peak is 39,759,351,808 bytes including the unchanged reserve (margin
240,648,192). Full validation/held activation is still running under
`actual-baseline-restore-20261017/`; do not relaunch. The prepared one-hour B3
observer coordinator waits for every full/strict phase and verified metadata
archive before starting, with disabled producer, persistent hold, absent
producer key, strict disk, steady/peak RSS and swap/OOM guards. Do not rerun
old C073 controllers; their test basedir/executable have been retired.

Two external state-cache layouts now have 60 paired process measurements each
and verified 369-member namespace archives. All three layouts (baseline/list
owner/map owner) pass six standalone address/undefined and thread-sanitizer
commands with working clean/known-fault controls; the complete 137-member
sanitizer namespace is verified in a 3,309,882-byte archive. Metadata/key
requested bytes and unprobed RSS fall, but CPU/latency depend on the fixture;
map-owned small/null keys show a material CPU regression. Neither layout is
integrated. Native source remains B3. See
`docs/validation/state-cache-key-ownership-prototypes.md` and the three
`evidence/cache-*-20261017/` directories under the selected external run.
Latest One strict MkDocs: 0.83 seconds,
`artifacts/manual-vps-cache-checkpoint-20261017/`.

The tagged legacy Mac checked-range replay now completes 2,285,238 blocks
(19,920.7 seconds reported) and all nine common comparison fields match the
completed native Linux R2 result, including six contract-read hashes and the
single fallback. Exact tags/commits and scope remain pinned; full-range VM
execution is not inferred. Proof: `h1-eligibility-20261016/checked-full-range-native-legacy-comparison.json`.
The separate Mac native development run retains its original six-hour deadline.

The stronger external controlled producer links current B3 native libraries:
real producer/controller/shared nine-CF storage/BlockStore/EventBus, custom
non-mainnet genesis, generated test key, federated algorithm, empty mempool,
no network or key files. It creates 1,000 blocks and reopens from LIB 940 to
fully replay 60 blocks, reproducing head/root and all 1,000 raw block/receipt
records. Wall 6.100 / 0.311 seconds; kernel peaks 26,292,224 / 34,803,712 bytes.
All 53 namespace members are verified in a 12,579,427-byte archive; frozen ELF
SHA `0d3b8344c6fbdc5954fcc1d3cd6b08ff4e675403adf35f1a16354fbae5744754`.
The pre-runtime namespace compile failure is separately preserved. This is not
PoB, target-host or sustained producer qualification. Source remains B3.
See `docs/validation/controlled-native-producer.md`.

Integrated current-native ASan/UBSan build first fails linking the pinned
prebuilt RocksDB without RTTI: undefined typeinfo for DB/Iterator/WriteBatch.
No native test or CLI sanitizer pass is credited. The failed evidence and
entire owned RAM build are preserved and member-verified on My Passport
(42 / 1,770 members; 425,528 / 108,341,328 archive bytes). No data is retired.
Continuation `linux-integrated-asan-r1-20261017` now builds the same pinned
RocksDB 8.8.1 archive with RTTI, ASan and UBSan in an owned RAM subdirectory;
333 dependency compile commands verify both detectors and RTTI. It then
reconfigures the preserved native build to that isolated install and attempts
all targets, CLI and 61 tests. No detector suppression or installed dependency
change. Do not launch another copy; inspect its result and retained logs.

The current native ASan/UBSan workspace is on the authorized
miner in `/dev/shm/teleno-issue9-asan-20261017-krvvxm54`, an owned directory
on the already mounted, executable 8,266,424,320-byte tmpfs. No mount, VM,
host settings or installed service changes. Source is the existing frozen B3
copy. Root logs/controller are under the owned miner evidence namespace
`linux-integrated-asan-20261017`; local counterpart is in the selected external
run. Clean/use-after-free/alignment controls and initial configure pass.
Limits remain: one compiler job, 3-GiB
incremental shared-mount allocation, 2.5-GiB owned process peak, 2-GiB concurrent
host available, unchanged 10-GiB root floor, owned VmSwap zero and global OOM
unchanged. Other imported dependencies remain uninstrumented; inspect rebuilt
Fizzy coverage rather than assuming it. A diagnostic root `teleno_node` alias points
to the actual `build/src/teleno_node` ELF. All native 61-test/CLI diagnostics are
pending this build; no integrated D1 pass is credited. The diagnostic parent
alone has core dumps disabled. Do not operate or clear unrelated shared memory.

The complete new B3 restore now PASSES independent strict accounting:
preflight/interruption/resume/activation all exit as expected, validation checks
46,458,363 records across nine families, state revision 40,236,770 / highest
stored 40,236,830, zero WAL, same DB inode, persistent observer hold. Activation
2,478.100 seconds, CPU 2,325.660 seconds, kernel peak 53,493,760 bytes, physical
input 179,308,584,960 bytes. Whole-server peak including unchanged 10-GiB reserve
is 39,760,650,240 bytes, margin 239,349,760; minimum concurrent host available
3,429,847,040 bytes; swap/OOM unchanged. The CLI's generic capacity flag remains
false by design and this does not qualify runtime/history/whole goal. Complete
selected metadata archive: all 439 members verified, 451,875 bytes. The full DB
is already represented by unchanged complete native source objects; no second
payload is archived.

The guarded B3 observer is now RUNNING (owned remote supervisor PID 105285),
label `b3-hour1`, basedir under the dense restore namespace. It is ready and
advancing, with mainnet identity verified, producer disabled, key absent and
hold intact. At 302 seconds, head 40,242,263, sampled peak RSS 303,239,168 bytes,
minimum host available 3,191,123,968, strict margin 168,079,360 bytes. These are
provisional; steady window starts after 900 seconds, clean stop at 3,600.
Local coordinator `run-observer-b3.py` must not be relaunched. Read its live/
terminal files and exact remote process handles; all source/native identities
remain frozen B3. The separate Mac native checked replay reaches its original
six-hour deadline, exit -15, `native-ended`; this partial diagnostic is NOT a
pass. Completed native Linux and tagged Mac reference checked-range comparisons
remain valid for their exact inputs. Both terminal Mac states/logs are now
fully preserved and member-verified. A fresh second full-VM boundary from
32,789,319 through 32,789,381 also matches native/tagged-reference in all nine
common fields and the independently anchored root. Both known historical
boundary windows pass; full-range VM execution remains open. Its complete
49-member namespace is verified in a 101,696,198-byte archive, with source
metadata unchanged and no retirement.

Latest frozen native manifest:
`b3f83775d32e815d94adcbee967d2d66333e20eaed33d764daa390bcf9f80364`
(306 inputs). The seven changed paths since `b3469afb...` implement optional
`rocksdb.max-total-wal-size` and existing config/storage/CLI regressions. Zero
preserves the automatic default; explicit bytes are a soft flush trigger,
not a WAL/disk cap. Synced abrupt-exit fixtures verify all records/checksums
through read-only recovery and a default-policy reopen. No history is removed.

All targets, version/help and all 61 current-source tests pass on Mac/Linux.
Mac suite: 214.469 seconds. Linux exhaustive 59/1/1 batches: 147.790 / 1.404 /
7.213 seconds. Full fixture archives are member-verified before retirement;
all 306 native inputs are freshly reverified. Frozen executables:

- Mac: `b854856b8afcc96c97b51ab3d0057171aca8a4882199564f18df8a00b6e21dd8`.
- Linux: `b38318065a982d9a83de30f4dafd0dd85a00a2709f32aa9bec23e9c28788332c`.

The actual reference VPS passes version/help and positive/negative side-effect
free preflight with this Linux binary in its separate owned CLI namespace.
The complete held node's metadata is unchanged; no runtime replacement/start
occurs. An initial coordinator syntax failure occurs before any remote
statement and is retained. Current runtime remains the older C073 executable.
Evidence: `/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/wal-policy-native-20261016/`.
The latest One manual passes strict MkDocs into the external `artifacts/`
folder; no app/submodule/packaging changes are made.

Five WAL mechanism pairs complete on each Linux host. All 79 raw records,
checksums, synced abrupt-exit/read-only/default-reader recovery and matching
cross-case digests pass. VPS median WAL and allocated-peak ratios are 0.25025 /
0.28766, write CPU 1.00657, physical writes 1.00032, recovery CPU 0.08036 and
recovery wall 0.09308. No RSS saving is claimed. Actual peak WAL 67,616,632
exceeds the 67,108,864-byte trigger, demonstrating its soft semantics. VPS
swap/OOM counters remain zero/unchanged. A shared-builder host swap-counter
failure is retained; retries prove only owned-process VmSwap zero there.
The complete VPS namespace is preserved off-host, every archive member verified,
then retired. See `docs/validation/wal-retention-budget.md`.

The preceding scan-reuse implementation keeps both ownership locks across a
combined CLI validate/activate and reuses only a fresh private proof. All hashes
and final post-rename DB validation remain. Five warm pairs per Linux host
reduce scans from three to two; VPS median CPU/wall/logical-read ratios are
0.805 / 0.814 / 0.79985, with individual regressions and zero warm physical reads.
This is not a new full restore. Frozen source `b3469afb...` and helper identities
remain separate from current native qualification.

The preceding CA0 dictionary binary completes a native full rewrite and the
real native backup API. Backup `20261016T201050Z-ms-1792181450480-files-380`
has 383 objects/files, 24,114,178,238 payload bytes, 24,113,527,301 DB bytes and
zero WAL bytes. All object hashes/checksums and pointwise comparison of every
46,458,363 raw record across nine families pass, including all 45,076,063
block/receipt records. One allowed operational metadata value differs. All
input/object metadata remains unchanged. The DB saves 688,676,036 bytes against
the prior actual native backup; strict 40-GB restore is not yet proven.
Evidence: `evidence/dictionary-policy-20261016/` under the same external run.
All 385 old public objects are rehashed over HTTPS against the unchanged
preserved copies. Complete remote inventory and metadata/probes are preserved;
only the old generated qualification namespace is retired, reclaiming
24,806,154,240 seed available bytes. Stable public latest stays byte-identical.
Normal native promotion now completes for the new 383-file public backup in
`qualification/dictionary-policy-20261016`: 24,114,178,822 payload bytes;
metadata and all 383 HTTPS sizes pass. Target-side body hashing remains for
native staging. Seed available afterward is 46,149,042,176 bytes, with the
next full original-size publisher backup plus 10 GiB still budgeted. The
installed producer/publisher remains unchanged.

Current-source isolated runtime checks pass in the existing image: seven cases
(version/help/preflight and controlled producer/VM/storage/RPC fixtures), two
CPU quotas, 2.5-GiB cgroup limit and zero swap allowance. The owned container is
removed; all 26 existing containers retain image/running/start/restart fields.
Cgroup memory peak is 106,172,416 bytes, not node RSS. All 32 namespace members
are preserved/verified. Production environment/mount details are not retained
in the original-container comparison records. A fresh repository Dockerfile
build is not run. Report: `docs/validation/current-container-runtime.md`.

Five tracked and five probe-disabled Linux-builder VM cache triplets complete,
using 32 distinct valid Wasm programs with independently checked real code-hash
IDs. The 16-MiB policy retains seven rather than 32 modules: about 61% lower
fixture peak RSS, but median paired CPU 4.048 and p95 latency 5.684. The
128-MiB policy holds this full working set and saves no memory here; timings
vary substantially. No new cache default is chosen. The final 178-member
namespace is archived and all members verified. Existing native source is
unchanged. Report: `docs/validation/linux-vm-cache-policy.md`.
Evidence: `evidence/vm-cache-mechanism-20261017/` under the same owned run.
The earlier one-hour C073 mainnet logs separately show 60 snapshots with
11–19 cache entries and 1,813,912–3,202,382 attributed bytes; this is older
workload evidence, not a complete sizing study.

The new actual VPS restore inputs and full observer profile pass read-only
preflight. Prepared local/remote source selection records distinguish CA0
backup production from the B3 restore/runtime binary. The guarded restore
tracks exact MemTotal, ext4/root/raw device sizes, physical whole-server use
plus the unchanged 10-GiB reserve, one interruption/resume and held activation.
It has not started: first finish/preserve the actual H1 source users and
recheck/retire only the preserved old owned test node. Draft/evidence:
`evidence/dictionary-policy-20261016/actual-baseline-restore-20261017/`.
Do not blindly rerun the old C3b driver or expect its retired executable.

Four exact current test executables are frozen in a fully verified
21,740,218-byte archive for a later baseline functional check after H1 closes:
`evidence/baseline-functional-20261017/`. They are not yet run on that host.
The current old C073 executable is also preserved separately in a verified
17,606,682-byte archive; it remains present on the VPS until controlled
replacement. Neither archive receipt alone authorizes deleting operational data.

H1 starts from independently anchored revision 30,504,141. Fresh complete
2,285,238-block receipt audit through 32,789,379 has no unexplained mismatch.
Native/tagged-reference checked/full 62-block boundaries match. Mac native
(group 55741/session 81881) and tagged reference (61114/46043) full replays
remain running; inspect durable results before touching them. They progress
slowly on external storage; no completed range is inferred. The latest bounded
build/test guardian pauses owned Mac data jobs for 324.975 seconds and resumes
them, in addition to the earlier documented 766.481 and 225.068-second pauses.

VPS R0/R1 historical attempts stop at the unchanged 10-GiB root-free guard;
both complete namespaces are archived and verified before retirement. R1's
299,394,207 WAL bytes versus 46,657,290 SST bytes identify retained WAL as a
contributor. The current R2 shared-native-backend replay uses an explicit
64-MiB flush trigger with the frozen B346 helper. Both boundary modes pass;
full replay remains active beyond previous stopping durations with ordinary
WAL reclamation and no OOM/swap. Local controller session 25101;
`h1-eligibility-20261016/native-shared-state-profile/wal64/` contains the durable
controller/results. Do not replace the stopped source DB or overlap observer
runtime until this replay closes and its evidence is preserved. A first monitor
variable-collision failure is retained; its owned process group is closed.

The stopped VPS DB is fully preserved off-host: 420 files, 24,833,049,473 bytes,
378 native CAS references rehashed and a verified 42-member delta archive.
Keep the previous native CAS and delta together. A fresh four-member non-DB runtime/control archive also passes full member
hashing and source before/after metadata checks. Recheck all preserved-set
dependencies and source users before any controlled replacement. Original inputs,
full history, native backups and historical canaries remain intact. Strict
40-GB restore, current sustained operation, producer, container, aggregate
memory and complete historical/differential gates remain open.

## Current dictionary source and active full rewrite (2026-10-16)

Current frozen native manifest:
`ca0a6b4e890474528e385c3149f76250d38787d2cb41093059858a9d97290e7b`.
Only config validation and three existing fixtures differ from the preceding
RSS source. The opt-in 128-KiB dictionary retains defaults and training/buffer
bounds. Mac/Linux all targets/CLI and all 61 tests pass. Five repeated
lossless triplets and three further native SSTs quantify disk savings and
higher rewrite CPU; see [the report](validation/larger-block-dictionary.md).
Frozen binaries: Mac `76c22e8b5fca74d52fde6e9449eb8e2fe37fba71bc26fc4b37f654902ffbedb4`;
Linux `1c2981e42604b5a68db1b92a2916181313ebb8a830b3597780ad09b47925731e`.

Owned work:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/dictionary-policy-20261016/`.
Native full rewrite session `36968` is active: owned time-wrapper/native PIDs
`43575`/`43577`, using `native-rewrite-driver-r1.py`. Inspect its durable live/
result record before resuming; **do not launch another rewrite**. A first
helper `36851` fails before native startup for a missing copied input inventory;
its script/failure remain. The corrected helper uses that exact original
inventory. Input is a fresh independent COW copy of the verified native backup;
all services/network/production/schedules are disabled. The 70-GiB external
floor, original-input checks, owned RSS ceiling and six-hour deadline remain.
COW is development placement only. Full output equality, native backup and
strict-budget server restore remain open.

Current native build sessions `96901`/`6077`, Mac full CTest `85880` and Linux
full coordinator `91934` are all terminal success. Do not rerun these checks
without a new source change or unresolved concern. All Linux fixture archives
are verified before owned retirement. Completed RSS/dictionary Mac fixtures
are retained as independently verified 42-MB `.tar.zst` archives under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/development-space-preservation-20261016/`.
Only the fully compared new prototype and its immutable working hardlink
duplicate are retired there; all source/native backups, inventories, reports,
controls and 177,220,250 bytes of physical samples remain. Historical canaries,
operational data and previously retained deletion-review candidates are untouched.

The previous RSS executable's one-hour coordinator `14444` is terminal success.
Its final captured height/root 40,262,636 agrees with independently checked
legacy after reference LIB passes it. Peak kernel RSS 260,673,536 bytes;
minimum host `MemAvailable` 3,218,644,992 bytes; all 60 native RSS metrics agree
within 816 KiB. Complete observer/control archives are verified externally.
The actual VPS is stopped after its planned clean exit; preserve its healthy
DB and hold. Four unused owned native/transfer executables are externally
archived/verified and retired, with the current RSS executable, libraries,
config, keys and DB preserved. Inspect the current private retirement receipts
before installing new component fixtures. All strict disk, 24-hour/seven-day,
producer, aggregate and historical acceptance gates remain open; the goal stays active.

## Reference-server results and current RSS source (2026-10-16)

The persistent issue #9 goal stays active. Full native restore, all three
complete database validations, held activation and post-rename reopen pass
on the designated replacement VPS. Two 900-second observer runs and clean
restart pass; the second catches recent head, with captured height/root
agreement against the existing legacy reference. See the
[reference report](validation/reference-vps-observer.md). Independent kernel
peaks are 304,467,968 and 297,615,360 RSS bytes; these are short observations.

**Strict 40-decimal-GB acceptance fails.** The provider device is actually
40,961,572,864 bytes. Whole-device accounting with physical overhead, boot
partitions and the unchanged 10-GiB reserve requires up to 40,628,682,752
bytes during the observations. Earlier positive root-filesystem margins do
not qualify the physical whole server. Further lossless savings are required;
do not reduce reserve, prune history or omit receipts. Single-SST higher
compression screens are exploratory only; their full-data/CPU/I/O/latency and
restore implications remain to be measured.

The preceding RSS native manifest is
`c073ab12836d01ddd761e8146ed829792fee0f92b77c622059cf798951208c3b`,
306 inputs. It corrects Linux native RSS logs to read resident rather than
virtual pages; actual memory allocation is unchanged. Mac all targets/CLI and
61/61 CTests pass. Linux all targets/CLI and all 61 unique CTests pass across
retained attempts. The synthetic comparison initially refuses stage because
retained generated copies leave less than its recommendation. A retry
prelaunch also refuses. After externally verified owned-copy retirement, the
same single test passes. Both failed and successful fixture trees are archived
and every member hash is checked. See [RSS validation](validation/linux-rss-counter.md).

Current Linux executable SHA-256:
`52a17d127dcdb0c5f25bb87227d5f41d085c4d3a13ea920e25e160845a0cf2e5`;
Mac: `3d91e51ecde0acf86933860290fb22467ca0f681c04b2c6273ed1e8ebc2831ae`.
Build/test evidence and frozen Linux input:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/rss-counter-20261016/`.
Original Linux full coordinator `79088` is terminal failure; corrected targeted
retry `70128` is terminal success. Do not repeat the already-accounted tests.
Archive receipts record retirement of only verified failed/successful fixtures,
older owned diagnostics, a screened temporary input and completed transfer probes.
Original backups, operational data, source and measurements remain preserved.

The original full restore `17926`, corrected first observer `24019` and
read-buffer observer `43960` are all terminal success. The initial observer
monitor's undefined-variable failure is retained separately; it is not a node
pass. Full restore/observer provenance stays under
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/native-export-k3ky3m61/`.
Do not restart completed labels or clear the restored DB. Current-source
observer follow-up uses fresh `c073` scripts/plan, independent kernel guards
and bounded telemetry. Read its actual launch/result records before resuming.
Sustained/abrupt recovery, producer, complete historical and aggregate memory
qualification remain open. Production stays disabled.

## Direct restore read-buffer continuation (2026-10-16)

The preceding native manifest is
`af826d523daa1d196808b7a1e69effaadfe6e119bfa4c9be34a15af1b24d3553`.
The only changed native input from the preceding archive rewrite is the direct
HTTPS reader's 64-KiB reservation inside its existing 128-KiB wire bound.
Five alternating Linux-builder pairs verify the same 66,533,451-byte object
through real native transport and checked SHA/write/fsync. Median paired CPU
falls 47.67%; write calls fall from 129,950 to 4,062. Wall time varies.
The complete experiment archive is externally verified. Mac all targets,
four focused checks and all 60 CTests pass (154.67 s for the full suite), plus
version/help. Linux all targets, focused checks, CLI and all 60 native tests
pass in 58/1/1 batches. Every fixture archive is externally verified before
owned duplicate retirement. Strict manual/rendered checks pass. See
[the scoped report](validation/direct-restore-read-buffer.md).

Owned new source/build evidence:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/restore-read-buffer-20261016/`.
Mac driver session `37467` is terminal success; Linux native session `95127`
is terminal success. Full Linux coordinator session `17109` is terminal success;
do not rerun those tests without a new reason. Linux native SHA-256 is
`b68994f0bdf4ec2b46c92606b1d8e162306edeb648efd1ed68809d9cd4510e0b`;
Mac SHA-256 is `ca22f19e6944ce7908f6a13faa2cf8f228f7f1dc888b8b2502ec2303b85211a6`.
Five additional mechanism pairs on the designated VPS pass: median paired CPU
decreases 73.02%, wall time 71.56%, and kernel writes 96.87% for this verified
object. Their complete 48-member archive is externally verified. The actual
complete restore below deliberately retains its original `c3b1cc4e...` identity.
No goal gate or production activation follows from the component savings.

## Native archive rewrite continuation (2026-10-16)

The active issue #9 goal remains incomplete. On
`codex/lr2-memory-hardening`, frozen native input manifest
`c3b1cc4e2dad577874d5e0bb0afd8d303a89a5afe0b1d57ba11d016cd99fdb10`
adds explicit offline `--compact-rewrite` and empty host identifiers in new
SSTs. The original full public input is preserved. The actual native rewrite
and native local backup both complete; the latter contains 385 files,
24,802,854,237 total bytes and 24,802,203,337 DB bytes (16.550% smaller).
Checkpoint identity matches the original, with state revision 40,236,770 and
highest stored height 40,236,830. All 385 native object SHA-256/size checks,
both complete RocksDB checksum scans and the full nine-family raw comparison
pass. All 46,458,363 visible records are preserved, including 45,076,063
blocks/receipts. Only the two exact expected operational metadata values differ.
The block digest matches the original and binds its full canonical continuity
proof to the native backup. Historical execution and whole-node gates stay open.

Mac/Linux all-target builds and all 60 unique native tests are accounted for
across retained attempts. Two initial Mac timeouts pass with unchanged binaries
after briefly stopping only the concurrent owned rewrite; the same rewrite is
resumed and completes. Twelve current native component commands pass on the
mandatory VPS. Linux fixture/runtime/evidence archives are externally verified;
the installed observer publisher is unchanged. Koinos One documentation is
coordinated and its strict manual build passes; no GUI, package or pin changes.

Owned work is
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/native-export-k3ky3m61/`.
Native backup session `78212` is terminal success; do not restart it. The first
verification helper/session `12970` fails before creating a view because it
assumes one object shard. The corrected two-shard helper is
`native-backup-verify-driver-r1.py`, session `60476`; preserve and inspect its
live/terminal records rather than launching a duplicate. Session `60476` is
now terminal exit 0; all hashes/checksums and the complete raw comparison pass.
Do not restart its full read. Comparison takes 2,956.73 seconds; total hashing/
comparison takes 3,612.64 seconds on Mac, not Linux capacity evidence.

The isolated qualification publication is now complete: normal native promotion
and HTTPS metadata, all object sizes and reviewed config checks pass. Backup
`20261016T115052Z-ms-1792151452613-files-382` has 24,802,854,821 public payload
bytes. Stable public latest remains byte-identical. Signature policy is
unchanged; absent signatures are not claimed authenticated. See the
[current report](validation/native-archive-rewrite.md).

The native public-promotion API assumes the payload object tree is already
available; it publishes sanitized config/metadata only. The isolated object
root, actual SFTP fsync/rename/stat behavior and HTTPS content-addressed route
are now verified with a small public probe. The existing private/shared tree
is untouched. Seed planning allows both the candidate and another full
original-sized operational backup plus 10 GiB. Transfer and publication session
`17345` and its SFTP process group `86229` are complete; do not restart them.

`qualification-publication-driver.py` (session `17345`) requires the successful
terminal comparison proof, now passed. It transfers only the 384 verified
non-config objects into the isolated public
namespace, invokes existing native promotion for the reviewed observer config
and metadata, and verifies HTTPS metadata/config/all object sizes. Its terminal
result and `qualified-source-selection.json` are now present and verified.
Native target stage/resume also checks all received object bodies. Complete
whole-server disk, sustained/recovery and producer outcomes remain pending.

`baseline-restore-coordinator.py` completed in session `17926`, exit 0.
It passed the publication/source checks and installed only owned
small control scripts/selection metadata on the designated VPS, then supervised
native preflight, controlled stage interruption, resume and observer-held
activation through detached remote handles. The original prepared raw driver
is preserved as `baseline-full-restore-driver-r0.py`; current driver hashes are
in `next-stage-coordinator-inputs.json`. It stops before node startup and never
relaunches an ambiguous remote action. Inspect live/terminal records rather
than starting another controller. Native preflight passes without reducing
the 10-GiB reserve. Controlled SIGINT pauses after 18 complete files; resume
passes in 920.96 seconds and preserves all 18 inodes/sizes/mtimes. Whole-root
available space after resume is 11,312,304,128 bytes. Offline validation/held
activation then passes in 3,608.699 seconds, including three full DB scans and
post-rename reopen; the same DB inode survives. The first observer monitor
fails in its own Python startup; corrected `r1` and later `af82` runs complete
cleanly. Their complete archived results supersede this earlier pending state.
Root-space values exclude parts of physical device overhead; the corrected
strict disk finding and current-source follow-up are recorded at the top.
Complete sustained memory/CPU/I/O/latency, abrupt recovery, historical and
producer qualification remain open.

The native CLI's read-only remote listing passes using the same libssh build
and existing pinned credential. No DB/SST or seed change occurs. Its available
byte count agrees with the OpenSSH KiB capacity calculation. The native runtime
source is unchanged from the tested C3b manifest; no redundant native build/test
run is needed for these isolated transport/control steps.

Archived: 2026-09-12.
Scope: the quality, stability, memory, and disk-efficiency review and its
documentation follow-up in this conversation.

## Status to remember

The review is complete and documented. **The proposed remediation has not
been implemented.** A completed audit, a published documentation commit, and
passing existing tests must not be mistaken for completed fixes.

Authoritative documents:

- [Software review](performance/teleno-software-audit.md): evidence, findings
  TQ-01 through TQ-12, measurements, reproductions, and coverage limitations.
- [Remediation plan](implementation/teleno-audit-remediation-plan.md): WP1
  through WP7, dependencies, acceptance criteria, rollback, and validation gates.
- [Open TODO](../AGENTS.md#open-todos): the reminder remains unchecked.
  `AGENTS.md` and `CLAUDE.md` must remain byte-for-byte identical.

The review examined source revision
`9936a78d482716be1f2ca4bae81a3d786981bbe0` on 2026-09-05. This memory is a
historical summary, not a new test or production-health report.

## Decisions and boundaries

- Priorities: functional bugs, memory management, and disk/I/O efficiency.
  CPU measurements support the analysis of resource tradeoffs.
- Treat the work as repository-local software maintenance. No intrusion
  testing, external system exploration, or bypassing system protections.
- The review authorized inspection, isolated experiments, tests, and
  documentation, not production fixes or live-node operations.
- Preserve Koinos compatibility, durability, and observer-first recovery.
  Never use weaker validation, discarded chain data, or reduced persistence
  guarantees as an optimization.
- Keep all repository documentation in English. Discussion with the user may
  remain in Spanish.

## Main findings

1. State reads can turn I/O failures into apparent missing objects; state
   flushes can discard failure results.
2. A partial block-record/topology write remains inconsistent after a retry
   and reopen.
3. RPC sessions can remain usable after `stop()` returns, and idle persistent
   connections consume the configured session capacity.
4. Object-cache clear/oversize paths can hang; repeated module-cache insertion
   can break effective LRU capacity.
5. Optional index handlers show duplicate history and silent write failures.
   ABI changes from non-head/reverted uploads were observed, but the required
   fork/API contract needs confirmation before selecting a fix.
6. Cache accounting omits substantial overhead and warm hits allocate
   unnecessarily. Unpruned P2P seen-ID sets and other ownership/resource risks
   remain source-supported hypotheses where not reproduced.

Consult the report for exact evidence levels. In particular, an unattributed
standalone RPC probe SIGBUS was **not** established as a production crash.

## Recorded measurements and tests

The following results belong to the 2026-09-05 review, on an Apple M4 with
16 GiB RAM and macOS 26.5. They were not rerun when this memory was written.

- Five measured repetitions after warm-up: approximately 66 MiB cache RSS
  for 10.77 MiB of accounted keys; two million allocations for one million
  warm cache hits.
- Synthetic RocksDB workload: sampled size of 63.38 MiB before reopen versus
  8.87 MiB after reopen, largely explained by retained WAL. The same data was
  checked after reopen; this is not an implemented saving or mainnet sizing.
- Normal CTest: 20/20. Separate UBSan CTest: 20/20, with `vptr` excluded because
  the installed RocksDB library lacked the required RTTI symbols.
- Focused tests: 6/6. Benchmark-script tests: 4/4. Native build, version/help,
  and isolated observer startup/restart checks passed.
- ASan and TSan failed during runtime initialization in that environment;
  no successful coverage from those tools was claimed.

Ignored local evidence remains under `../build-audit-20260905/`. Its
`evidence-manifest.json` and `reproduction-sources.tar.gz` were found when
archiving this memory. These artifacts are not included in a normal clone.
Preserve them and use new scratch paths for future experiments.

## What remains pending

Start with the first three packages only after implementation is authorized:

1. **WP1 — Storage integrity:** propagate errors, make required persistence
   atomic, and prove retry/reopen behavior.
2. **WP2 — RPC lifecycle:** own, cancel, and drain sessions; bound idle work
   and prove safe shutdown.
3. **WP3 — Cache correctness and memory efficiency:** repair invariants,
   preserve value ownership, and reduce measured allocation overhead.

Subsequent packages address index consistency, retained IDs/memory growth,
disk and backup/recovery boundaries, and measured simplifications.

Unproven areas include full historical/differential parity, long-duration
realistic workload growth, physical write amplification, and complete lifetime
and concurrency diagnostics. Historical validation requires a verified local
full-block/receipt archive spanning both historical boundaries and matching
checkpoints. A receipt journal or small fixture test is not a substitute.

Preserve the Chain v1.5.2 replay invariants: ordinary removal semantics,
historical tombstones, fresh pending roots, the exact historical exception,
one-block lookahead, correct final-target behavior, at most one full
reexecution, and no canonical advancement or partial side effects after failed
validation. Do not confuse this pending remediation with the earlier replay
implementation described in its separate plan.

## Commit and push history

The user separately authorized committing the audit and adding the reminder,
then authorized pushing it.

- Commit: `6652556bc5cf4884f7c4a20aa4ec24d409181f3d`.
- Subject: `docs: record quality audit and remediation TODO`.
- Included files: `AGENTS.md`, `CLAUDE.md`, and the two audit/plan documents.
- Push to `origin/main` was completed and verified on 2026-09-05. This archive
  does not claim a new verification of the remote's current state.
- The user's README changes and untracked high-throughput plan were excluded
  and preserved. Large diagnostic artifacts were not committed.

## Safe resumption

At the start of this archival request, local HEAD was
`8a13a5084b6874d9f61475ea6a20860183e4b2cb`, including later unrelated
documentation commits. The audit/plan and their open TODO remain present.

The worktree already contained unrelated documentation changes, including
README changes and the high-throughput implementation plan. Other worktree
activity was observed during archival. Recheck the actual files on resumption;
do not restore an earlier snapshot or stage unrelated work.

On resumption, inspect the actual branch/worktree, read `AGENTS.md` and the
remediation plan, and revalidate the relevant baseline. Keep the TODO open
until the required fixes and proof are complete. New implementation, commit,
push, release, deployment, data cleanup, or producer activation requires the
appropriate explicit authorization; this archival request grants none of them.

## Single-copy backup launch goal (2026-10-01)

The user clarified the intended outcome: launch a new observer from an
independently stored native backup on the pictured Linux VPS with 2 vCPUs,
4 GB RAM and 40 GB local disk, without keeping a second full backup payload
on that disk. Include the OS, installed runtime, logs, metadata and operating
reserve in the whole-server budget. Host identifiers are intentionally omitted.
Screenshot resources are not verified usable capacity or deployment readiness.

Follow the [single-copy restore plan](implementation/single-copy-backup-restore-plan.md).
SC0/SC1 evidence is local/synthetic; it does not prove a deployable 40 GB node.
The current request authorizes SC2 bounded transfer/resumable staging and this
memory update, not live-server access, activation, deployment or publication.
Preserve complete data, protocol compatibility, durability and observer-first
recovery. Reject insufficient capacity instead of shrinking safety allowances.
Verify actual status from the plan and validation reports before resuming.

SC2's local library/diagnostic implementation subsequently passed 11/11 focused
tests and UBSan; broader CTest passed 25/26 with the existing SC0 fixture SIGBUS
still exposed. See the [SC2 report](validation/single-copy-restore-sc2-staging.md).
It is stage-only byte validation, not a runnable DB or a qualified 40 GB server.
Next is SC3 offline validation and crash-recoverable observer activation.

## SC3 implementation handoff (2026-10-02)

The next-milestone request authorizes the repository-local SC3 engine and isolated
verification, not VPS access/deployment, CLI integration, commit/push or release.
The internal engine now validates every existing unified CF read-only, checks
genesis/expected chain identity and checkpoint consistency, and activates by
exclusive same-filesystem rename with generation-bound observer holds. A common
POSIX startup/restore gate and durable external intent prevent ordinary startup
from adopting an interrupted target. Recovery never replays source files after
the validating transition. Config and legacy `.pre-restore/` behavior are retained.

See the [SC3 report](validation/single-copy-restore-sc3-activation.md) for actual
commands, fault tests, native genesis observer smoke and unresolved gates.
This is not full historical/differential parity or 40 GB server qualification.
The intended 2-vCPU/4-GB/40-GB backup launch remains the goal, not a proven result.
At the SC3 handoff, SC4 operational CLI integration was next (see the later
SC4 record below); SC5 must measure whole-server capacity
and Linux recovery, and audit WP1/WP6 blockers remain explicit. Do not skip those
gates merely because tiny local fixtures pass.

## SC4 implementation handoff (2026-10-02)

The follow-up authorizes standalone CLI integration and local regression, not
SC5/SC6, audit remediation, live nodes/VPS, producer activation or publication.
Direct mode now supports metadata preflight, stage-only, explicit stage resume
and separate validation/activation. Default repository behavior remains intact;
runtime admin direct mode is unsupported. Resume/activation retain the pinned
source, HTTPS/signature policy, chain and generation, reject `latest`, and
re-authenticate metadata without replaying source payload after validation.

See [SC4 evidence and blockers](validation/single-copy-restore-sc4-cli.md).
This CLI version requires original source metadata to remain available during
activation/recovery; it has no persistent offline trust-proof cache. Preserve
the sole DB/journal/intent on failure, never bypass observer holds or reserves.
Local signed-fixture success does not repair audit WP1/WP6 or prove historical
parity, Linux durability, 4 GB RAM or whole-server 40 GB capacity. SC5 is the
next independent gate, requiring explicitly scoped isolated qualification.

Final SC4 evidence: 43 signed-fixture CLI commands passed, focused backup/direct
CTest 13/13 and matching native/helper UBSan 2/2. Standard and fresh native full
CTest are 27/28, retaining the SC0 truncated-transfer SIGBUS. The dependency
bootstrap is blocked by the existing Xcode SDK path containing a space; Docker
smoke is blocked by an unavailable daemon. SC4's all-green exit gate is not met.
The English native/operator reports and Koinos One docs-only handoff are updated;
no commit, push, release, package/submodule update or server operation occurred.

## SC5 local measurement handoff (2026-10-02)

"Next step" authorizes local SC5 diagnostics, not live VPS access, audit fixes,
artifact cleanup, publication or production. The resource driver compares a
signed genesis/32-MiB optional-CF fixture over five paired runs, checks exact DB
bytes and observer-held clean/interrupted reopen, and retains owned evidence.
The tiny fixture does not qualify a representative backup, historical replay,
24-hour operation, Linux/ext4 or the 2-vCPU/4-GB/40-GB whole-server target.
The corrected 1-GiB APFS experiment refuses stage at the unchanged reserve;
40-GB image backing is blocked by insufficient host space plus reserve. Keep
existing evidence and data; do not lower allowances or delete artifacts to pass.
See [SC5 results and next prerequisites](validation/single-copy-restore-sc5-capacity.md).

## SC6 documentation handoff (2026-10-03)

The next-step request prepares the independent SC6 review checklist while SC5
remains blocked; it does not bypass qualification. See the
[SC6 handoff](validation/single-copy-restore-sc6-handoff.md) for supported behavior,
prior evidence, exact missing prerequisites and the unexecuted verification/
release sequence. The intended backup-based Linux 2-vCPU/4-GB/40-GB observer
launch remains unproven. No runtime code, data, reserves, native/app version,
submodule, live host, release or production settings change. Native identity is
still the existing dirty development build; no committed feature pin exists.
Documentation coordination and bounded local checks do not authorize publication,
audit remediation, VM/server provisioning or cleanup of retained evidence.

SC6 documentation checks: existing binary version/help passed; selected resource
unit/preflight/workflow CTest passed 3/3; Koinos One strict MkDocs passed. Full-suite
29/30 and UBSan 4/4 remain SC5's prior results, not reruns in this docs-only task.
All pre-existing code/tests/protected files are hash-preserved. The review package
is prepared but SC4/SC5, feature DoD and release-readiness gates remain open.

## Low-resource observer requirements revision (2026-10-03)

**The bounded-history direction in this entry is superseded by the full-history
clarification below.** Historical measurements and validation limits still apply.

The user asks to adapt the requirements and plan to the selected 2-vCPU,
4-GB-RAM, 40-GB Linux server, using a Koinos Foundation mainnet backup. This
follow-up authorizes planning/documentation, not runtime implementation or
server writes. See the
[low-resource observer plan](implementation/low-resource-mainnet-observer-plan.md)
for proposed LR0–LR6 and the superseding resource contract. Preserve SC gates.

Read-only source/host observations on this date found 36,146,835,456 available
root bytes and a snapshot declaring 29,396,360,941 logical bytes. The current
reserve calculation needs 40,410,218,496 free bytes. Native metadata preflight
failed; empty source chain declarations and missing independent trust material
remain blockers. These are metadata/sample observations, not payload, head,
replay or sustained capacity proof. Exact artifacts are linked in the new plan;
host/access identifiers and credentials are excluded from public documentation.

The revision permits planning a **separately qualified** workload-based reserve,
not lowering the existing 10-GiB default or bypassing checks. Budget all node
memory, OS/runtime storage and compaction/recovery peaks. For sustainable storage,
prepare a compact backup off-target with complete state and mandatory recovery/
fork tail plus bounded recent history. Disclose archival API limitations; never
discard required consensus/recovery data or compact a live DB to force a fit.
Implementation starts with LR0 measurement, then LR1 source/persistence gates
and LR2 memory work. LR3–LR6 cover disk policy, bounded history, whole-server
qualification and docs/release handoff. No LR milestone is implemented/proven,
and no indefinite 40-GB lifetime or packaged Koinos One feature is claimed.

Planning checks passed: whitespace checks in both repositories, 61 relative
Teleno file references, AGENTS/CLAUDE identity, and Koinos One strict MkDocs.
The 57 other pre-existing dirty/untracked Teleno files remained hash-identical.
No native build/test, restore, historical/differential validation or VPS write
was executed during this requirements revision.

## Teleno full-history / teleno_light separation (2026-10-03)

The user explicitly wants ordinary Teleno to retain all history from genesis
for now. The compact/bounded-history idea is a different future node, provisionally
named `teleno_light`. No pruning, receipt omission, rolling window or remote-only
archive may satisfy the active Teleno requirements or substitute for a failed
40-GB archival qualification.

Updated the [active LR plan](implementation/low-resource-mainnet-observer-plan.md)
to preserve the full canonical history and all historical block/receipt records
from the selected backup. LR4 now investigates only lossless archival storage
improvements; complete historical corpus and continuity checks are mandatory.
Keep source/trust, memory, operating reserve and full-history Linux qualification
gates. If the complete archive cannot fit safely, document the exact capacity
blocker instead of switching products or weakening checks.

The [teleno_light concept](implementation/teleno-light-concept.md) is deferred
research, not a new repo, binary, CLI mode, release or authorized implementation.
It would be full-state with limited local history, not a header-only light client.
Older bounded-history producer research is a reference for that separate idea,
not authorization for pruning/production. Documentation and matching AGENTS/
CLAUDE guidance are updated, including Koinos One's docs-only handoff. No runtime
code, existing data, VPS, packaging, submodule or release changed.

Documentation verification for this clarification passed: both worktree
whitespace checks, matching AGENTS/CLAUDE, 84 relative file references and
Koinos One strict MkDocs. The 54 other pre-existing Teleno files remained
hash-identical. Full-history Linux/resource and native test gates were not run
or completed by this documentation change.

## LR0 full-history resource baseline (2026-10-03)

The first-milestone request implemented LR0 only: versioned resource contract,
repository-local measurement driver, build/host/source manifest, owned-child
CPU/RSS and optional Linux/cgroup counters, SST/WAL/allocation categories,
explicit unavailable metrics, bounded command cleanup and 14 regression tests.
Reuses SC5's eligible signed genesis plus 32-MiB padding fixture. Complete
canonical history from genesis and all backed-up block/receipt records remain
mandatory; `teleno_light` is separate/deferred, never a capacity fallback.

See the [LR0 report](validation/low-resource-lr0-baseline.md) for exact commands,
files, versions, metrics and blockers. Final evidence is retained in ignored
`build-single-copy-evidence/sc5-run-lzi7fo5v/`: one excluded warm-up pair and
five alternating pairs, exact DB/source checks, observer holds/restarts and
owned direct interruption/reopen passed. Baseline recorded, not capacity proven.

Standard repository dependency build completed using cached local dependencies;
native build/version/help passed. Full CTest is 30/31, with the pre-existing
SC0 truncated-transfer SIGBUS still failing. LR0 unit tests pass 14/14 and SC5
measurement units 7/7; focused CTest and Koinos One strict documentation build
passed. Container smoke is blocked by the unavailable Docker daemon. An initial
collector dependency-command timeout is retained and its timeout handling was
corrected/tested; it is not a new successful dependency-flag validation.

Local LR0 exit is satisfied. LR1–LR6 remain unimplemented; next is LR1 source
eligibility/persistence, including the unexplained underlying authentication
error and truncated-transfer crash. Mainnet metadata was only reread from the
earlier pinned evidence, not refreshed/downloaded. No complete historical corpus,
reference range, matching Linux runtime or sustained WAL/compaction/growth peak
was measured. Existing reserve remains unchanged; no 40-GB launch or memory
improvement is claimed. No VPS, live data, production, runtime defaults, trust
policy, GUI/package/submodule, version, commit, push or release changed. Only
the relevant Koinos One documentation was coordinated.

## LR1 source/persistence and future backup correction (2026-10-03)

"Haz LR1" authorized local prerequisite implementation. The user additionally
requested fixing native Teleno backup creation for future snapshots. New backups
measure chain identity from the read-only checkpoint, compare genesis and
persisted state/block/receipt metadata, and label durable state revision separately
from live head/LIB. Empty diagnostic DBs and old snapshots without verified
identity cannot be promoted by the native publisher. Existing backups are not
retroactively patched or authenticated; the separate app promotion script is
unchanged.

Implemented checked state reads/flushes, atomic initial metadata and archive
record/topology, safe failed-commit ownership/session refusal, explicit close
errors/nonthrowing destructors and cache-clear accounting. Local publication
checks file/directory sync and rename and refuses corrupt object reuse. Direct
metadata is bounded/cancellable and reports safe typed source errors; SC0's
truncated-transfer Boost exception crash is fixed. New fixtures/fault tests do not
weaken normal remove, root/lookahead/fallback, history, durability, reserve or
observer-first semantics.

Fresh HTTPS and cached metadata-only preflight both identify the current source
blocker as `source_chain_identity_missing`, exit 1, no payload/target DB. A new
backup from the corrected deployed writer and independently verified identity/
trust/corpus are required. Do not invent a chain ID in old metadata or bypass
authentication to qualify the source.

See the [LR1 report](validation/low-resource-lr1-prerequisites.md) for the complete
changed-file list, commands, evidence, tests and remaining gates. Local source
verification passes CTest 33/33, focused 19/19 and UBSan 3/3, build/version/help.
The repository-local standard dependency workflow passes when run from an ARM64
shell; the initial system-Python/Rosetta Intel dependency attempt hit its owned
600-second timeout and remains recorded. Earlier intermediate test failures were
fixed and rerun, not suppressed. This is not a clean-from-empty dependency proof.

The full audit TODO remains open; only scoped WP1/WP6 and cache clear work is
implemented. Docker daemon, physical power loss, Windows publication, full
legacy-transfer bounds, complete historical/reference corpus and sustained
Linux/40-GB qualification remain pending. LR1's source exit is blocked and
LR2–LR6 are not implemented. No VPS/live node/data deletion, producer activation,
commit, push, release or GUI/package/pin change occurred. Koinos One docs-only
handoff preserves its pre-existing changes; native pin integration and writer
deployment/new public backup publication require explicit authorization.

## LR2 local memory hardening (2026-10-03)

Subsequent "Procede" authorizes only the next local accepted-ID P2P package.
The [P2P report](validation/low-resource-lr2-p2p-retention.md) records bounded
FIFO/owned-ID bytes/lazy expiry, LIB/stop cleanup, exact pending-ID duplicate
checks, tests and measurements. This is not archival pruning or a whole-node
memory bound. LR2 remains partial; LR3, VPS/live-node operation, commit/push,
release and deployment are not authorized by this request.

"Siguente" authorizes the next local LR2 work, not a VPS operation or release.
Owned asynchronous RPC sessions now expire idle/partial reads and blocked writes,
and stop drains them before dependencies are freed. Object/module cache fixes
terminate oversized inserts, preserve a single oversized value's ownership,
deduplicate module replacement and remove warm-hit list/key allocations.
Offline DB reports distinguish shared-cache usage from memtable/reader estimates.
Defaults, full-history requirements, durability and reserve are unchanged.

See [LR2 evidence](validation/low-resource-lr2-memory.md). Whole-node byte/lifetime
bounds, integrated supported lifetime/race checks, eligible mainnet source and
Linux memory/block-following evidence remain open. LR2 is partial, not a proven
4-GB/40-GB profile; LR3 must not lower reserve based on these local measurements.
The original goal remains backup-based full-history observer launch on the
selected small server. No live deployment, producer setting, commit, push,
release, packaged binary or native submodule pin changed.

### Active LR2 completion goal and external development storage

The explicit LR2 prompt starts a persistent completion goal, not merely another
planning draft. The [completion matrix](validation/low-resource-lr2-completion-matrix.md)
tracks remaining mempool, P2P, RPC, VM, state lifetime, diagnostic and real-input
gates. The first new [worker checkpoint](validation/low-resource-lr2-workers.md)
reproduces and fixes stop failing to own disconnected sync threads; local
functional proof is not supported ASan/TSan or total-node memory qualification.

User instruction: store new development builds, temporary/test databases and
evidence on **My Passport**. The mounted path verified for this execution is
`/Volumes/My Passport`; owned run root `/Volumes/My Passport/teleno-lr2-EEMOFW/`.
Use `TMPDIR`, `TELENO_TEST_ARTIFACT_ROOT` and `TELENO_TEST_BUILD_ROOT` pointing
there, with `PYTHONDONTWRITEBYTECODE=1`. Do not silently fall back to internal
storage if the volume is absent. Source/docs stay in this repository; existing
dependencies are read as inputs and existing chain/state data is not relocated
or deleted. APFS/USB diagnostics do not establish the Linux/ext4 profile.

The user explicitly prioritizes Linux VPS optimization over macOS. Keep Ubuntu/
x86_64/ext4, 2 vCPUs and 4 decimal GB physical RAM as the primary target; the
Mac is a development host, not the qualification environment. Prefer portable
fixes and Linux-based resource decisions. Mac tests cannot close the Linux
whole-node memory, catch-up, persistence or recovery gates. This clarification
does not authorize operating the VPS or creating a VM.

Current owned-worker checkpoint: 38/38 CTest and 3/3 UBSan pass; five isolated
offline genesis observer runs exit 0, without establishing full-history or
mainnet capacity. An additive M1 test distinguishes 2,001 retained synthetic
records from the 2,000 response limit and verifies logical expiry cleanup;
aggregate admission/fork byte bounds remain unimplemented. The final suite
with that test passes 38/38 (151.25 seconds), UBSan 3/3 (19.64 seconds).
Native SHA-256 is
`26ff10df621a6bd4ae3a0147222d09a5b288f02a6a034f9121ab10773cbb9b30`.
The clean dependency workflow is running with Ninja through a space-free
ignored link to new physical external storage; earlier Make/Hunter path-space
failures are retained. No success is claimed before its actual completion.
Creating a new isolated Linux VM for functioning ASan/TSan has been requested
but is not yet authorized. No VPS or unrelated existing VM is operated.

The goal remains incomplete while any applicable LR2 criterion is unproven.

Later [S1 ownership checkpoint](validation/low-resource-lr2-state-ownership.md)
confirms a held RocksDB buffer was deleted by read-caused cache eviction. State
reads now return immutable owners through consumption/iterator destruction;
map clones share immutable values and replace on write. Oversized/zero-budget
values bypass retention safely; cache payload is bounded, caller-held bytes
and metadata are not. Mac large-value clone memory improves in the synthetic
fixture, while small-value clones show time/metadata costs. Neither proves
Linux whole-node qualification. The fourth fresh external dependency workflow
passes with command-local MAKE/Ninja; earlier failed attempts remain evidence.

Later [M1 admission checkpoint](validation/low-resource-lr2-admission.md) reproduces
and corrects native insertion refusal being caught/logged before a successful
accepted notification. Admission now precedes observers; notification failures
after insertion are counted separately. Visible-head pending metrics no longer
use the zero-record request path. Current CTest 38/38 (206.74 seconds), UBSan
subset 3/3 (20.94 seconds), version/help exit 0. Current native SHA-256:
`198b251507a0532f8407539189b0e3b45416c8bd45b3b0f3afdc172418f9ef3c`.
These replace neither historical worker evidence nor Linux qualification.
Five Mac synthetic comparisons favor a nonmaterializing correct count over a
materialized correct count; the old metrics were incorrect, not expensive.
Aggregate physical/fork admission and all-fork failure proof remain open.
The third fresh dependency attempt failed at OpenSSL recursive Make; a fourth
uses command-local `MAKE=/usr/bin/make`, proven by a recursive probe, and Ninja.
It remains running, not a passed gate. All new generated data stays external.
No LR3/LR4, live server, producer, commit/push/release, backup deployment or
GUI/package/submodule integration is authorized. LR5's long soak remains separate.

Earlier pre-goal LR2 tranche (historical evidence, not current-checkout proof):
full CTest 36/36, focused cache/RPC/config/storage/
diagnostic-admission tests 5/5, UBSan 3/3 and five offline whole-binary genesis/RPC
runs pass. Native identity is `1.3.0-dev.0+cfd2eb83b808-dirty`, SHA-256
`71807aa069ee7aae046bb657a1c0dca2b09608dc44f74f2da0d8bfd6e86623bb`.
The standard cached dependency workflow/build/CLI checks pass. Integrated and
minimal ASan probes fail initialization; TSan exits without a diagnostic report.
The Docker daemon is unavailable. These checks are pending, not waived. Cache
warm-hit allocations fall from 200,000 to zero for 100,000 measured hits; process
peak RSS does not improve in that fixture. Do not mark LR2/whole-audit complete
or advance to a reduced LR3 reserve on this evidence alone.

### LR2 M1-C failure-atomic admission (2026-10-03)

[M1-C report](validation/low-resource-lr2-admission-atomicity.md) records confirmed
partial insertion after one-shot C++ allocation failure: 65 changed-state
rejections and 56 terminate paths in 455 baseline cases. Write-only anonymous
map batches now prepare all fallible work/reset ownership before mutation and
transfer existing map nodes without second payload copies. Iterators that
allocate no longer promise noexcept; post-publication debug-log failure is
best-effort, not rejection. Original per-fork nonce/resource and anonymous
root/tombstone behavior remain unchanged. Fixed sweep: 363 injected failures,
zero partial/terminate results, readmission verified. A new state-test oracle
was corrected to compare original anonymous commit, not direct puts over
tombstones; initial failed runs are retained, not called passes.

Five paired Mac processes measure 527,456 fewer C++ requested bytes and 91 fewer
allocations for one synthetic large candidate. Not retained heap or Linux/RSS
qualification. New data stays on My Passport. Global/fork/staged/chain-execution
accounting and capacity, block/LIB/prune/removal allocator paths, P2/R1/V1/S1
budgets, integrated sanitizers and real historical/Linux proof remain open.
No producer/live VPS/VM, commit/push/release, pruning, source-trust or GUI/package
integration occurred. LR2 and whole-audit TODO remain incomplete; LR5 long soak
is separate. Final native/build/test identities are in the report and ignored
preservation manifest; earlier checkpoint hashes above are historical.

Final M1-C source (including rejection of anonymous-parent batch aliases): both
native full suites pass 41/41 sequentially (112.52 / 114.42 seconds), UBSan subset
7/7 (68.80 seconds), both version/help and required dependency workflow exit 0;
final Koinos One strict docs pass in 0.42 seconds. Concurrent preflight 15-second
timeout and SC5 paused/incomplete transfer under its 2-second deadline are retained
as failed attempts; serial verification changed no timeout/retry/trust setting.
Current native identity stays `1.3.0-dev.0+cfd2eb83b808-dirty`; fresh-dependency
SHA-256 `c7ecc38a8e4f852076a1cb38d57c39ec4f1727a16191cdcdc55397388fa3e912`.
The final five paired measurements retain the 527,456 requested-byte/91-call
reduction, but timing overlap and fixture peak RSS are not Linux/node proof.

### LR2 M1-D indexed-fork retention (2026-10-03)

[M1-D evidence](validation/low-resource-lr2-retained-memory.md) adds on-demand
exclusive-lock indexed map/delta inventory: shared immutable owners counted once,
copied keys/tombstones explicit, non-map backend payload unmeasured. No hot-path
scan/RPC/CLI, aggregate capacity cap, consensus/history or durability changes.
String capacities exclude metadata/caller/transient/allocator costs and are not
heap/RSS. Five fixed synthetic processes retain 8,404,704 logical value bytes
after expiry empties the head; LIB reduces indexed values but leaves tombstones.
The test verifies clone/lifetime/root/lock/reopen/expiry/LIB/concurrent behavior
and 1,050 injected read-only scan allocation failures. Final focused 8/8; remaining
verification/build identities are in the report, not inferred from older hashes.

Generated data remains `/Volumes/My Passport/teleno-lr2-EEMOFW/{build,ubsan,tmp,evidence}`
and the separate external dependency configuration. Primary target is Linux
Ubuntu/x86_64/ext4, 2 vCPUs, 4 decimal GB RAM; no Mac-derived limit is selected.
Global reservations, consumer/lifetime bounds, current historical/reference input,
functioning ASan/TSan and representative Linux evidence remain open. No live VPS,
VM, producer, commit/push/release, LR3/LR4, trust change or GUI/package integration.
M1/LR2 and the whole-audit TODO remain incomplete; LR5 long soak stays separate.

Final M1-D validation: both external full suites 42/42 (156.73 / 170.91 seconds),
UBSan subset 8/8 (25.99 seconds), required dependency workflow/all-target builds,
both CLI checks and Koinos One strict MkDocs (0.47 seconds) exit 0. Three initial
namespace/standard-library compilation errors were corrected and retained;
no tests or deadlines were relaxed. Current cached native SHA-256
`0756811006d85807398459c45e0bb35e134111779019c2eaa0e11d48af2fc01d`, separate dependency
native `b156524f8fa4ca8a06586e927b51924d346fafd510a0eda2de634602ac862be3`, same dirty
`1.3.0-dev.0+cfd2eb83b808-dirty`. These are not release/integration pins. Docker
precondition still fails (socket unavailable); current ASan/TSan, real Linux and
historical differential gates are not executed/qualified, not silently waived.

### LR2 M1-E standalone transaction-work admission (2026-10-03)

[M1-E evidence](validation/low-resource-lr2-transaction-work.md) introduces opt-in
global standalone-submission job/input-wire reservations before controller
copying/execution, held through admission/notifications and released on failure
or return. Each axis defaults to zero/unlimited. This is not retained-fork/heap
accounting, a total-node cap or a qualified 4-GB profile. Canonical blocks/replay
are not gated. Local overload is retryable JSON-RPC `-32005` or gRPC
`RESOURCE_EXHAUSTED`, not consensus invalidity or P2P misconduct; no automatic
retry, accepted-ID cache or partial admission is introduced. The static local
exception message does not need C++ allocation.

Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB physical RAM remains the primary
optimization/qualification target. Mac fixtures only verify local behavior;
no Mac-derived policy values or APFS-specific optimization are selected. Current
source/test/build results, failures and exact native hashes belong to the report.
All generated data remains in `/Volumes/My Passport/teleno-lr2-EEMOFW/` under
the explicit TMPDIR/test/evidence/build-root environment. Existing chain/user
data is untouched. Documentation-only Koinos One handoff selects no GUI config,
binary, submodule or package change.

Remaining global retained/fork/metadata/consumer limits, integrated ASan/TSan,
eligible pinned historical/reference inputs and a functioning isolated Linux
whole-machine environment are required. No live server/VM creation, producer,
commit/push/release, LR3/LR4, trust or reserve change. M1/LR2/full-audit exits
remain incomplete; LR5 long-soak qualification stays separate.

Final M1-E validation: cached/separate dependency full suites 43/43 (169.04 /
167.23 seconds), UBSan subset 9/9 (38.51 seconds), required dependency workflow,
all-target builds/four CLI checks and Koinos One strict MkDocs exit 0. Five
synthetic functional processes respect tiny test ceilings and end with zero
credits; median regression-process peak RSS 8,536,064 bytes is not `teleno_node`
or Linux evidence. The duplicate test-local compile failure is retained; no
tests/deadlines were relaxed. Exact current dirty binary SHA-256 values and
the 29-file checkpoint inventory are in the report. Docker socket absent,
no container smoke or current integrated ASan/TSan/historical/Linux qualification.

### LR2 M1-F failure-atomic fork-head cleanup (2026-10-03)

[M1-F report](validation/low-resource-lr2-cleanup-atomicity.md) preserves the
original baseline binary and 316 removal/2,577 expiry processes: 206/2,115
rejections with changed state and 24/36 crashes. A functioning UBSan diagnostic
on the first staged fix locates incomplete stream decoding followed by a null
resource-owner read. Cleanup now checks direct protobuf decoding and prepares
all fork-head writes/removals, marker buckets and fresh resets before
allocation-free map publication. Ordinary removal/squash/durability, expiry
ordering, per-fork nonce/RC and retained ancestry/history are unchanged.

Fixed sweeps: 328 removal/3,593 expiry cases, every injected rejection unchanged
and explicitly retryable in the test, no invalid result/crash. State publication
observes zero new calls and matches original squash roots/tombstones/owners;
12 preparation faults preserve inputs. Five serialized synthetic comparisons
show fewer total requested C++ bytes, but expiry performs more small allocations.
Workspace/hash/iterator costs are not heap/RSS totals or qualified Linux gains.
Current full build/test/CLI/UBSan/docs results and hashes follow in the report;
do not reuse older checkpoint identities as current verification.

Linux Ubuntu/x86_64/ext4, 2 vCPUs/4 decimal GB remains the primary target.
Generated data stays in `/Volumes/My Passport/teleno-lr2-EEMOFW/` under explicit
temporary/test/build roots. README/high-throughput plan and existing chain/user
data are preserved. Koinos One receives documentation only, not GUI/config/pin/
binary/package changes. Aggregate retained/caller/staging budgets and emergency
cleanup headroom, block/LIB failure paths, supported ASan/TSan, pinned eligible
historical/reference input and isolated Linux whole-machine proof remain open.
No VPS/VM/live-node/producer, commit/push/release, LR3/LR4 or source-trust change.
M1/LR2/full-audit exits remain incomplete; LR5 long soak stays separate.

Final M1-F validation: cached/separate dependency full suites 44/44 (158.39 /
185.29 seconds), UBSan subset 10/10 (37.49 seconds), required dependency workflow,
all-target builds/four CLI checks and strict Koinos One docs (0.47 seconds) exit 0.
The report records failures, complete 20-file checkpoint and SHA-256 values;
cumulative goal inventory is 93 files, 134 other pre-existing files unchanged.
Current cached native SHA-256 `b99f484d078220fa474740de94ce363a42769a021982882e9169419d666f6a35`,
separate dependency `8231cfc44fe2ecbff22a263ea27ce32515fec744cb734b0ae6a0a1f31cbe025d`, same
dirty `1.3.0-dev.0+cfd2eb83b808-dirty`. No releasable/integration pin is created.
Current Docker precondition fails because the local daemon socket is absent;
ASan/TSan/Linux and full historical/differential gates remain unexecuted.

### LR2 M1-G pending-record lifetime and portable memory work (2026-10-03)

[M1-G evidence](validation/low-resource-lr2-pending-records.md) removes repeated
pending-record serialization/storage across eligible forks. Shared immutable
owners persist through clones, ordinary squash and in-memory LIB. Opt-in native
`mempool.pending-record-max-owners` / `pending-record-max-capacity-bytes` default
to zero/unlimited; reservations follow staged, retained-ancestor and caller
lifetimes. Head expiry cannot free ancestor-held credits. Refusal unwinds before
fork publication and is local/retryable, not consensus invalidity or P2P misconduct.
Canonical validation/replay remains ungated; no accepted work/history is evicted.

Five serialized before/after Mac fixture processes show 16,809,400 to 4,205,044
unique logical retained payload bytes with identical keys/nonce/RC/LIB behavior.
Other metadata, encoding/cleanup workspace and consumer memory are additional;
this is not complete aggregate capacity, a hard RSS cap or mainnet throughput.
Enabled accounting has lease/mutex cost; tiny/fewer-fork and whole-node overhead
still require Linux measurement. No Mac-derived VPS limits are selected.

**Linux remains the primary optimization/qualification target**: Ubuntu/x86_64/
ext4, 2 vCPUs and 4 decimal GB physical RAM. Mac/APFS results cannot close memory,
catch-up, durability or recovery gates. No live server or Linux VM was operated
or created. All new builds/DBs/tmp/evidence remain in the owned
`/Volumes/My Passport/teleno-lr2-EEMOFW/` run; source/docs stay in this repository.
Exact source hashes/command results are in the checkpoint report and `m1g-*`
external artifacts. Existing user changes and chain data are preserved.

Koinos One handoff extends only its two already-dirty operator/backup docs; no
GUI/config generation, binary, package, submodule, app version/changelog or
release change. Supported integrated ASan/TSan, all other consumer policies,
block/LIB allocator behavior, eligible pinned historical/reference inputs and
isolated whole-machine Linux proof remain required. M1/LR2/full-audit exits are
not satisfied; no LR3/LR4, trust/reserve reduction or commit/push/release follows.

Final M1-G gates: cached/separate dependency full CTest **45/45** (179.20 /
181.78 seconds), separate UBSan **11/11** (64.63 seconds, vptr/dependencies
excluded), required workflow/all-target builds/four CLI checks exit 0. Strict
Koinos One docs exit 0 (0.78 seconds). Docker daemon socket absent, smoke not run.
Current cached native SHA-256 `219fbfdc352db853d62c78d4caa5f0b754aa0a50723ea0df7f24503d37b14aab`,
separate dependency `c3297b0acabb41e0fc197409bec10f6752403fd054d2b77bd8eedde4a177dcf2`,
same dirty `1.3.0-dev.0+cfd2eb83b808-dirty`. No committed integration pin exists.

### LR2 V1-A VM ownership and Linux-first interpretation (2026-10-03 to 2026-10-04)

[V1-A evidence](validation/low-resource-lr2-vm-lifetime.md) reproduces an owned
parsed-module leak after successful parse when shared ownership cannot allocate.
Raw ownership is now protected; optional cache publication prepares before
eviction, and cold parsing is serialized/rechecked without blocking warm hits
or holding locks through execution/nested host calls. Each execution keeps a
fresh mutable instance and unchanged protocol metering/limits. Cache entries
remain 32, not a parsed-byte/global-instance or whole-node memory bound.

Five serialized Mac single-thread samples show unchanged tracked heap/no RSS
improvement. Controlled eight-caller same-ID lookup-strategy bursts reduce
8 parses/owners to 1; not natural latency, old production-binary performance
or Linux qualification. Diverse cold IDs serialize and require Linux profiling.
A separate pinned Fizzy context-creation allocation failure terminates the
process (isolated handler exit 77); dependency correction remains required.

**Linux Ubuntu/x86_64/ext4, 2 vCPUs and 4 decimal GB physical RAM remains the
primary target**, not Mac/APFS. All generated artifacts/builds/tmp/DBs remain
in `/Volumes/My Passport/teleno-lr2-EEMOFW/` under explicit external roots;
the pinned read-only Fizzy source is `evidence/v1a-source-I09vWF/`. Existing
user edits and chain data are preserved. No live server or VM, source-trust/
history/durability change, LR3/LR4, git publication or GUI/package integration.
Koinos One receives documentation only. Parsed/queue/instance and other consumer
policies, functioning integrated ASan/TSan, eligible historical/reference inputs
and isolated Linux whole-machine/catch-up proof remain open. V1/LR2/full-audit
exits remain incomplete; LR5 long soak is separate. Current command results and
identities belong to the V1-A report/artifacts, not older checkpoint binaries.

Final V1-A local gates: serialized cached/separate dependency CTest **46/46**
(120.20 / 162.24 seconds), separate UBSan **12/12** (45.39 seconds, vptr and
dependencies excluded), required workflow/builds/four CLI checks and strict
Koinos One docs (3.19 seconds) exit 0. First cached attempt is 45/46 with a
15-second first preflight timeout; unchanged isolated recheck passes 1/1. Its
Mac startup cause is not attributed, and no deadline/check is weakened.
Docker socket absent, smoke not run. Cached native SHA-256
`5c8925b616d7ac574304e45cd1ab076d0eceeef5bb81c187195889b0eba584ca`, separate
`9abc06f1a57a8a9934caa544dfd7b68876b8881266611a2ee21f9a7dd58ae343`, same dirty
`1.3.0-dev.0+cfd2eb83b808-dirty`; no valid new integration pin is created.

### LR2 V1-B local allocation correction (2026-10-04, local checkpoint verified)

[V1-B evidence](validation/low-resource-lr2-vm-allocation.md) corrects pinned
Fizzy context-constructor allocation termination with one hash-pinned recipe
shared by all native CMake entrypoints. Existing installs are untouched. Native
parse/instance/context exhaustion propagates locally, not as an accepted
contract reversion; P2P gossip/sync does not penalize a peer for local allocation
failure. Real-Fizzy tests cover 35 ordinary allocation boundaries with cleanup,
unchanged ticks and retry; six controlled transaction-dispatch cases emit no
receipt/change to the parent. These are scoped faults, not machine-wide OOM,
historical parity, integrated race qualification or whole-node memory bounds.

Current completed checks: focused CTest 5/5 (20.95 s), separate UBSan 13/13
(49.30 s; 12 native plus one Python test; vptr excluded, new Fizzy instrumented,
other prebuilt dependencies not), dependency workflow/all-target builds/four
CLI checks exit 0, strict Koinos One docs exit 0 (0.42 s). Five preserved context
probes exit 77 and five corrected probes exit 0. Initial cached full CTest 47/47
precedes the P2P extension; final current cached full CTest passes 47/47
(133.06 s) with explicit external roots. Separate-dependency CTest also passes
47/47 (168.39 s), exit 0. Thirty Teleno checkpoint paths and two Koinos One
documentation paths changed; 142 other starting dirty files are unchanged,
none removed. Exact source/build manifests use the `v1b-*` external prefix.
Docker daemon absent; Linux/history/ASan/TSan gates remain open. No new release,
commit, package, deployment, source-trust change or live-node operation.

One repeat omitted explicit external roots for CTest after a command-local env
collector. It was stopped (143); 17 freshly created synthetic fixture directories
(339,090,468 logical bytes) were relocated and hash-verified under
`/Volumes/My Passport/teleno-lr2-EEMOFW/evidence/v1b-interrupted-env-artifacts-VH72qy/`.
No pre-existing directory/node data was selected. The original failed/interrupted
log and exact paths/hashes remain in `v1b-environment-interruption.json`; the
separate corrected CTest invocation has explicit env. Do not erase this exception
or claim that every generated byte was always external. Linux Ubuntu/x86_64/ext4
2 vCPUs/4 decimal GB remains primary, not Mac. LR2 exit remains **not satisfied**.

### LR2 R1-B batch serialization (2026-10-04, local repeat gates verified)

The subsequent [R1-B report](validation/low-resource-lr2-rpc-batch.md) measures
and removes retained batch result trees and success-envelope result copies.
Each result serializes into the complete HTTP body then releases. Strict UTF-8
failure still executes later calls once and sends no partial output. Five paired
Mac processes reduce tracked ordinary C++ peak from 21,960,048 to 14,265,952
bytes; total requested bytes from 47,465,473 to 39,080,833. Timed ranges overlap:
no speedup claim. This is not node RSS, a final-output cap or Linux qualification.
Generated data stays in `/Volumes/My Passport/teleno-lr2-EEMOFW/`; primary target
remains Ubuntu/x86_64/ext4, 2 vCPUs/4 decimal GB. Full histories, receipts,
durability, source trust and observer-first behavior remain unchanged.

R1-B initial full CTest exits 8 (47/48); unchanged backup-service test asserts a
saved pre-join status after a one-second poll. Original isolated repeat passes,
but first-run final status was not captured, so the exact cause remains
qualified. Failure/logs remain recorded. Final cached/separate full suites pass
48/48 (122.44 / 161.64 s), UBSan 14/14 (53.67 s), required workflow/all-target
builds/four CLI checks exit 0; strict Koinos One docs exit 0 (0.34 s). Docker
daemon unavailable, smoke not run. Final manifests preserve 156 other starting
files, zero removed, protected hashes and identical guides; 19 checkpoint files.
No live server/VM, producer, LR3/LR4, commit/push/release or GUI integration.
Aggregate consumer policies, supported ASan/TSan, Linux whole-node environment
and eligible pinned full-history/reference inputs remain required; LR2 open.

### LR2 R1-A completed HTTP bodies (2026-10-04, local checkpoint verified)

[R1-A report](validation/low-resource-lr2-rpc-bodies.md) reproduces an already-sent
large response retained during idle keepalive (baseline regression exit 1).
Requests now release after synchronous dispatch; responses release only after
the async write callback. Pending writes remain owned through cancellation;
full responses, pipelining, CORS, keepalive and admitted execution are preserved.
Native C++ capacity attribution is not a new endpoint/setting or byte/RSS limit.

Five serialized instrumented processes per version, following separate warm-up,
reduce idle response capacity from 18,087,935 bytes to zero for a fake 8-MiB
result. Body-capacity high water is effectively unchanged. Fixture-process RSS
median changes from 103,120,896 to 91,979,776 bytes, including the client/JSON/
base64 equality buffers; this does not qualify isolated node RSS or Linux.
Elapsed medians are within observed spread; no speed gain claimed. Initial
lifecycle CTest 1/1 passes (7.92 s); strengthened two-pending-owner/overlap/close
test passes 1/1 (8.00 s), final UBSan 13/13 (43.05 s). Earlier broad suites
47/47 (130.73 / 159.48 s) precede that extension; final runs follow in the report.
The first overlap counter missed paired request bytes; final repeated measurement
captures both and the high water is identical before/after. Initial nonexistent
test-header build failure and a same-tree build overlap/repeat are retained.

Linux Ubuntu/x86_64/ext4 2 vCPUs/4 decimal GB remains primary. Generated data stays
in `/Volumes/My Passport/teleno-lr2-EEMOFW/` with explicit external environment;
172 starting hashes and `r1a-*` artifacts preserve prior work. No live node/VM,
source-trust/history/durability/default change, LR3/LR4, producer, git publication
or GUI/package integration. Koinos One handoff remains documentation only.
Aggregate service/JSON/protobuf/serialization/output admission, functioning
integrated ASan/TSan, eligible historical/reference inputs and isolated Linux
whole-machine/block-following proof remain open. R1/LR2 exit is not satisfied.

Final R1-A gates: cached/separate dependency CTest **47/47** (126.23 / 160.15 s),
UBSan **13/13** (43.05 s; 12 native plus recipe; vptr/other prebuilt dependencies
excluded), focused **1/1** (8.00 s), required dependency workflow/all-target
builds/four CLI checks exit 0. Strict Koinos One docs exit 0 (1.26 s); fresh Docker
info fails because the daemon socket is absent, so smoke is unexecuted. Eighteen
Teleno checkpoint files and two Koinos One docs changed; 155 other starting dirty
files remain unchanged. Protected README/high-throughput plan, original dependency
and HEAD remain unchanged; no staging/commit/publication. Native cached SHA256
`9d0196d2b11e4005361917836fbd1e69f637cad7a34e4cae67cdb2bc70b2180d`, separate
`f5383459d9af26a03e9aacf487f188c2595e36be1c80daa2bb3b6c0b030341f6`, same dirty
`1.3.0-dev.0+cfd2eb83b808-dirty`; final snapshots/manifests use `r1a-final-*`.
LR2 remains active, not completed; all required representative Linux/historical/
supported sanitizer gates and aggregate consumer policies remain open.

### LR2 V1-C parsed-cache capacity (2026-10-04, local checkpoint verified)

[V1-C evidence](validation/low-resource-lr2-vm-capacity.md) adds optional
`chain.vm-module-cache-max-capacity-bytes`, checked pinned-module/container/key
capacity attribution and native cache-only metrics. Default byte-unlimited/count
32 remains. Failure-atomic insertion/replacement, oversize bypass, held owners,
fresh/nested instances and metering are covered; no validity/history/durability
change. Cache charge excludes allocator/control/node overhead, callers, queues
and active instances, so it is not a physical heap/RSS ceiling.

Five comparable Mac processes per group retain 2,108,016 requested bytes before,
2,108,400 after default and 395,325 at experimental 512 KiB. The reduced policy
reparses cyclic IDs, raises total requests and increases median fixture time
612/605/1,026 microseconds (before/default/limited). No Linux default, throughput
or disk-saving claim follows. Ubuntu/x86_64/ext4 2 vCPUs/4 decimal GB is still
the qualification target, not Mac/APFS/USB fixture results.

Actual gates: focused 4/4 (11.89 s), cached/separate dependency full CTest 49/49
(179.73 / 172.94 s), UBSan 16/16 (55.91 s; 15 native plus recipe, vptr/other
prebuilt dependencies excluded), required workflow/all-target builds/four CLI
checks exit 0, Koinos One strict MkDocs exit 0 (0.38 s). Docker daemon absent;
container smoke unrun. Prior R1-B saved backup-status failure stays documented,
not erased by current unchanged test passes. Native identity remains dirty
`1.3.0-dev.0+cfd2eb83b808-dirty`; hashes and commands are in the report/manifests.

Generated data remains `/Volumes/My Passport/teleno-lr2-EEMOFW/` with explicit
external environment; 175 starting hashes and `v1c-*` artifacts preserve prior
work. Native source/docs stay in Teleno, Koinos One handoff is two documentation
extensions only. No live node/VM, source-trust change, pruning, LR3/LR4,
producer, commit/push/release or GUI/package/submodule integration. All-consumer
policy, functioning integrated ASan/TSan, eligible authenticated full-history
corpus/reference inputs and isolated Linux whole-machine/head-progress proof
remain required. V1/LR2 exit is NOT satisfied; the persistent goal stays active.

### LR2 V1-D / D1 host-memory progress (2026-10-04)

[Host-memory evidence](validation/low-resource-lr2-host-memory.md) reproduces
production unaligned result-count stores through direct thunks/hosts and actual
Fizzy execution. Same-site four-byte copies preserve byte addresses and alias
ordering without allocation, settings or protocol resource changes. Native and
UBSan focused suites pass 3/3 (1.26 / 1.36 s); full before/after output is
byte-identical. Required dependency workflow exits 0 (102.89 s). Broader
current-source all-target/full CTest, integrated UBSan, CLI and strict docs gates
remain pending; do not substitute prior V1-C results.

Fresh owned generated-data root:
`/Volumes/My Passport/teleno-lr2-EEMOFW/d1-diagnostics-u6pnEJ/`, with explicit
external TMPDIR/artifact/build roots and disabled Python bytecode. Starting
184 hashes match V1-C. Standalone clean ASan/TSan programs with both existing
compilers terminate before main; immediate failure is outside Teleno but the
runtime/OS cause is unattributed. No tooling waiver, live VPS/VM, pruning,
producer, source-trust, LR3/LR4, git publication or GUI integration. Linux remains
primary; all-consumer policy, functioning integrated diagnostics, eligible pinned
historical/reference corpus and representative whole-machine proof remain open.
Two Koinos One documentation extensions are added, with strict build still
pending. V1/D1/LR2 not complete; persistent goal remains active.

### User priority: efficiency plus robustness and operational security (2026-10-04)

The user clarified that the final Teleno must not merely consume less memory:
it must also be more robust and operationally secure. The active plan and LR2
matrix now make these joint acceptance requirements. Resource gains never
justify weaker validation, durability, recovery, ownership, input handling,
admin protection or source trust. Each change needs relevant failure/lifecycle
evidence and disclosed performance/resource tradeoffs; no partial state,
duplicate effects or false success may be concealed by passing memory metrics.
Linux/historical/integrated diagnostics remain required and no broad security
qualification is claimed. This does not extend live-node, external-testing,
deployment, publication or unrelated-package authority. No runtime code or
configuration changes are made by this documentation clarification.

### Dedicated memory-development branch (2026-10-06)

The user requested that the memory improvements remain on their own branch
for now. Created and selected local `codex/lr2-memory-hardening` from
`cfd2eb83b8080ca49911555108ce05f0a9068f01` without committing, pushing or merging.
`main` remains at `8a13a5084b6874d9f61475ea6a20860183e4b2cb`, and the previous
branch reference is unchanged. AGENTS/CLAUDE and the active LR2 plan/matrix
record this policy; keep memory, ownership and lifecycle work on this branch
until an explicit integration request.

All 191 pre-existing dirty/untracked files retain their exact contents across
the switch, with no index changes. Snapshots and the verification script are
on the selected external development volume at
`/Volumes/My Passport/teleno-lr2-EEMOFW/branch-isolation-zdd3KT/`.
Only the five branch-policy documentation files are changed afterwards.
README and the high-throughput plan are preserved. Earlier backup/restore
and other mixed changes remain uncommitted: branch creation alone is not
permanent isolation or a reviewed memory-only commit. No runtime changes,
tests, publication, live-node actions or LR2 qualification follow from this
branch organization.

### Contract ABI index recovery proposal (2026-10-08)

[Problem and proposed solution](implementation/contract-abi-index-recovery-plan.md)
archives the kcli KFS ABI failure and dated read-only RPC observations. The
private endpoint reported Mainnet but returned an empty metadata result; the
running deployment/configuration and database health were not inspected.
Source inspection shows no dedicated ABI reindex command, no historical
backfill on service enablement, and no ABI-handler events from fast delta
replay. Empty results can also conceal read/decoding errors. KFS protocol
rectification requires checking recovered ABI compatibility with current code.

The proposed repair reads verified historical uploads into bounded staging,
reports provenance/conflicts in a dry-run manifest and requires isolated-copy
qualification before separately authorized, exclusive metadata-only writes.
Canonical/reverted/fork API semantics must first be agreed against the reference.
During the initial investigation, no implementation, native build/tests, live
maintenance, configuration/database change, transaction or publication was
performed. This remains a WP4-related proposal, not LR2 qualification or
operational recovery.

Follow-up: the user authorized committing only the new proposal Markdown and
creating an English issue for later implementation. Local commit
`d3cfb94455da4a472f651eb4239abbc289e74225` contains only that document. The open
[implementation issue #11](https://github.com/koinos/teleno/issues/11) was created
and its title/body were read back and verified. No push was requested or run.
The documentation-index and memory links remain local uncommitted changes;
existing unrelated work was preserved. This does not authorize implementation
or live maintenance.


### Selected development-data relocation (2026-10-12)

The explicitly authorized storage task moved only the complete public historical
state-delta audit journal (52.853 GiB, 372 files) to
`/Volumes/My Passport/teleno-data-migration-20261012-gv2jtizt/state-delta-audit-journal/`. The previous ignored journal path is a symlink
there. All file hashes, sizes, modes, mtimes and flags agree, including a final
source recheck; three original/external historical windows and a file restoration
sample pass. Originals were retired only after those checks and exclusive-use
review. Source, .git, CMake trees, configurations and existing changes remain.

The [LR2 matrix](validation/low-resource-lr2-completion-matrix.md#development-data-relocation-2026-10-12)
records exact routes, hashes, free-space readings and retained cleanup candidates.
The 24 existing local Time Machine snapshots were not modified; logical offload
is not a claim of equivalent immediate physical recovery. Generated-data guards
and the 748-row candidate inventory are in the owned external run. Remaining
synthetic payloads, canaries and generated release caches require specific cleanup
approval; the release-source parent is a registered Git worktree. No general
cleanup, live node/producer/restore activation, commit, push or deployment.


### Authorized regenerable-data cleanup follow-up (2026-10-12)

The user's explicit follow-up authorized clearing reviewed Teleno regenerable
data on the Mac. Removed 54.707 GiB logically
(429,560 regular files) from unreferenced synthetic payloads and
inactive old-release caches. Source/Git/existing changes, unique logs/manifests,
referenced fixtures, canaries and operational data remain. Path-level evidence:
`/Volumes/My Passport/teleno-data-cleanup-20261012-smb0hjfh/`. Physical free space did not materially improve;
20 local Time Machine snapshots remain subject to a separately pending explicit
confirmation. Original snapshot/backup prohibition was not silently overridden.
See the [LR2 matrix](validation/low-resource-lr2-completion-matrix.md#authorized-regenerable-data-cleanup-2026-10-12).


### Issue 9 persistent goal (2026-10-15)

The user requested an active goal to implement and qualify issue #9, preserving
the minimum Hetzner CX23-class resource envelope: Ubuntu/x86_64/ext4, 2 shared
vCPUs, 4 decimal GB physical RAM and 40 decimal GB whole-server disk with full
genesis history/receipts, OS and recovery headroom. No pruning, weakened reserve
or source authentication, main integration, publication or production operation
is authorized. Remaining LR2 implementation and isolated miner builds/tests
are authorized; identify the host/resources/ownership first and preserve all
live services/data. A larger host with limited test processes is not target
whole-machine qualification.

The goal is active and its [contract](implementation/issue-9-goal.md) points to
all open matrix gates. Current branch/HEAD are `codex/lr2-memory-hardening` /
`d3cfb94455da4a472f651eb4239abbc289e74225`, with mixed dirty changes preserved.
Fresh external run: `/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/`.
Source its `environment.sh` for guarded external temporary/build/fixture and
dependency roots; Python bytecode is disabled and initial build parallelism is
two. Before-setup fingerprints cover 193 dirty/untracked paths. Live issue and
private read-only miner inventory are retained under the run's `evidence/`.
This setup records no new native fix, test pass or Linux/sanitizer qualification.

First goal execution checkpoint: isolated Linux GCC 13.3.0 clean/positive ASan
and UBSan controls pass (four standalone checks). Clean TSan compiles, then
exits -11 before main with no report; its positive control is skipped. This
reproduces a startup blocker outside Teleno without attributing the deeper
cause. Commands/source/log hashes and private remote routes are in the owned
external run's `evidence/miner-sanitizer-controls-stdout.json`. No integrated
Teleno sanitizer, full native suite or whole-machine capacity gate is closed.
My Passport remains the compilation root; no live service/data changes occurred.

Subsequent user clarification: the mandatory baseline is the specifically
selected VPS and must support the producer. Its verified 1-vCPU/2-GB-class/
40-GB-class Linux/ext4/no-swap shape supersedes the earlier 2-vCPU/4-GB observer
reference. Updated [goal contract](implementation/issue-9-goal.md) and matrix
retain full history and source/reserve protections, include producer work in
the budget, and require stricter RSS ceilings plus concurrent host headroom.
These are targets, not qualified capacity. The user additionally authorized
persistent SSH setup and necessary console/email recovery. Access by a dedicated
restricted key was verified in an independent connection after the enforced
temporary password change; no password is retained. Usable routes and private
inventory are only in the owned external evidence, not this file. Existing
node services, producer authorities and mainnet transactions remain unchanged.

Latest replacement clarification: the user retired that first VPS and supplied
a different one. **Only the replacement is active**: verified 2 vCPUs, 4 decimal
GB RAM, 40-GB-class Ubuntu/x86_64/ext4 and no swap. Its new dedicated restricted
SSH key works in an independent connection, the enforced temporary password
change is complete and no password is retained. Active access routes are solely
in private external evidence; the old reference is retired and not contacted.
The producer requirement remains, with original 4-GB RSS/headroom gates and
full-history/source/reserve protections. No production activation is implied.

Fresh current-source native baseline on My Passport: all targets build and
**51/51 CTest pass in 209.86 s**; version/help pass. Binary
`1.3.0-dev.0+d3cfb94455da-dirty`, SHA256
`29d3053bb4093278fe13ea0bed8d614433cfb644d6c78afafb2bc1f40c8c86bb`.
`evidence/b1-native-summary.json` in the owned run records exact logs/identity.
Existing dependency installs are reused inputs and corrected Fizzy was rebuilt;
this is neither a fresh dependency workflow nor Linux/producer qualification.

The current Linux baseline source export verifies 282 native inputs and retains
Git base/dirty identity; archive SHA256
`8a85c7e22cf306703a2b2cd4967a7facfc2a36458ee6b156c83440e19ed8f771`.
An isolated miner build reuses verified existing dependency installs as inputs,
with one low-priority job and 10-GiB disk / 2-GiB available-RAM stop guards for
its owned process group. Missing zstd selection caused the initial configuration
failure, which is preserved; corrected configuration passes and the build was
observed live. Current handle/private routes are in the owned external evidence.
Poll that actual handle/process; a state file alone is not a live/terminal proof.
No Linux build/test, target producer or whole-node capacity pass is claimed yet.

### Issue 9 P2-A and diagnostic follow-up (2026-10-15)

Later evidence supersedes the preceding Linux startup/build-in-progress notes.
P2-A reproduces and corrects complete-frame truncation from a single successful
`writeSome`. Request/response bytes remain unchanged and owned until terminal
completion; sequential callbacks, zero/invalid progress, error/exception and
caller-release cases pass. No aggregate transport cap, cancellation/shutdown
or fragmented-read correction is claimed. See the
[P2-A report](validation/low-resource-lr2-p2p-writes.md).

Current Mac all targets/version/help and **52/52 CTest (202.83 s)** pass;
binary `1.3.0-dev.0+d3cfb94455da-dirty`, SHA256
`187f6b39322c3edc3c3b8139c89890ce98d0d40c7f3f17afb7722a0da63d26d3`.
Separate Linux ASan/UBSan/TSan helper/codec builds/runs pass with explicit
exclusions. Five paired TSan controls restore startup only with child-local
ASLR-off; the intentional race is detected. On the replacement reference VPS,
all six clean/known-fault controls pass with privately bundled compiler runtimes,
no host installation or global setting change. This is diagnostic feasibility,
not integrated D1 or target-node readiness.

Linux all-target build passes after correcting a missing transitive static
gRPC C++ link and an explicit JSON string extraction in the synthetic restore
tool. Failed attempts remain preserved; **52/52 Linux CTest pass (122.88 s)**,
with version/help and SHA256
`aa2f9c11d829b797ade8424f7173e92bae5f00efa994bb184bff6790733fb36e`.
Nine native CLI/component/restore commands also pass on the replacement VPS,
including producer mocks and preflight; this does not qualify full-history
production. The corrected
Mac signed restore fixture roundtrip/reopen passes. Current package evidence is
`evidence/p2a/` under the same guarded My Passport run. Koinos One networking
manual and strict external MkDocs pass; no GUI/package/submodule change. Goal,
P2/D1/Q1/H1 and complete B1 remain open. Use only the replacement VPS; the first
server stays retired.


## 2026-10-15: full-backup 40 GB requirement, P2-B and publisher follow-up

The user reaffirmed that the complete backup must restore within the designated
replacement VPS's 40 GB whole-server disk envelope, including OS/runtime,
transfer scratch, metadata and unchanged recovery reserve. Fresh source/host
inspection initially modeled a 4,086,128,640-byte deficit and refused the old
public source for missing chain identity. The later publisher correction below
supersedes that source observation; the current native VPS deficit is
4,140,425,216 bytes. See
[the current assessment](validation/restore-40gb-current-baseline.md). No pruning,
reserve reduction, larger baseline or synthetic substitute is accepted.

[P2-B](validation/low-resource-lr2-p2p-reads.md) corrects incoming fragmented
frames, recursive MessagePack stack use, stale restart bookkeeping and established
connection retention on stop. A real SDK fixture reproduces the ASan leak;
explicit connection shutdown removes it without suppression. Current Mac/Linux
all targets/version/help and 54/54 CTest pass (127.37/139.29 seconds). Separate
ASan/UBSan/TSan checks of the native reader and actual transport fixture pass;
static SDK libraries, unsupported TSan atomic-fence semantics and full-node
concurrency/resource gates remain excluded. Evidence is in the guarded owned
issue-9 run's `evidence/p2b/`; current source manifests and failed attempts remain.

The user then identified their installed miner Teleno as the seed backup writer
and directed updating that observer as needed. Read-only inventory confirms
native 1.2.0 with block production disabled and an advancing mainnet head/LIB.
The old backup's 500 files are hash-verified and privately preserved. Its durable
state's archived receipt has no root, so the strict checkpoint reader refuses it.
A separate managed worktree/branch `codex/backup-publisher-identity`, based on
clean d3cfb94455da with native source matching installed b8dab4c08f99, isolates
checkpoint/publication identity plus minimal future receipt-root completion from
the mixed LR2 changes. Linux all-target/version/help, 22/22 CTest (15.08 s) and
container checks pass with the old pinned dependency inputs. The isolated
maintenance observer is now installed, binary SHA-256
`759bed551b4e6a6f13c019c37a5cc1af35c10f02dde9a99063426df75262f718`;
head/LIB advance and fresh complete receipts agree with the legacy reference.
Original image/unit/config and old complete local/public input are preserved.
The new public generation is `20261015T191906Z-ms-1792091946641-files-494`,
497 files / 29,721,611,361 bytes, checkpoint revision 40,236,770. HTTPS metadata
matches the SSH-authenticated writer; native signatures remain absent and
policy is unchanged. Current Mac and actual-VPS native metadata preflight pass
source/expected-chain checks. The VPS's stage refuses at unchanged reserve
before payload, target or attempt creation; full restore remains unproven.
See the [maintenance report](implementation/backup-publisher-identity-update-plan.md)
for exact source/image identities, publication hashes, prepared rollback and
remaining gates. Evidence collectors had corrected syntax/field-name errors;
CLI actions were inspected, not repeated to conceal those failures.
Private routes, service paths and raw operational evidence stay outside source
under `evidence/publisher/`. Do not install the LR2 development binary there.
The issue 9 goal remains active and unqualified; no main merge, push/release,
GUI integration, mainnet signing or production activation occurred.


## 2026-10-15: P2-C outgoing ownership and local receive capacity

The separately isolated publisher update and corrected public checkpoint remain
installed/published. LR2 development changes do not alter that artifact. Full
restoration within the designated 40 GB server remains required; the dated
capacity sample is short by 4,140,425,216 bytes with the reserve unchanged.

[P2-C](validation/low-resource-lr2-p2p-client.md) reproduces the prior response
wait being renewed for each fragment. One six-second deadline now covers the
complete response. Outgoing calls own open/write/read cancellation, late borrowed
buffers and restart generations. Local lifecycle/receive-capacity refusal adds
no peer score; capacity reduces a later read-only block batch before application
at the same anchor. A controlled 4-to-2 batch regression verifies progress.
Final review also reproduced retention of large ignored response regions for a
small result. The corrected owned decoder sheds that capacity while reusing
payload-dominated frames, preserving bytes and legacy decode acceptance.

Final source: base `d3cfb94455da4a472f651eb4239abbc289e74225`, dirty branch
`codex/lr2-memory-hardening`. The 290-input manifest SHA-256 is
`747a9bbacd58aa83e53c9f24b006e4796661e6ca4232fe0911fbe9b337e48211`;
archive SHA-256 is
`61ef1a7da16f883bfcb586c687fce213defe8ea916a910b1cc2939eb9678799a`.
Subsequent guidance/handoff edits affect documentation only. Native identity is
`1.3.0-dev.0+d3cfb94455da-dirty` on Mac and Linux; this is not a clean commit pin.

Final Mac focused 3/3, all targets/version/help and 55/55 CTest (144.49 s) pass,
with native SHA-256
`74509f33b763ea5a6251393823b8ce32500a25c8dc073b4151733656c08d9be9`.
Larger-Linux all targets/version/help and 55/55 CTest (130.72 s) pass, with
native SHA-256
`a5189313dc3e3d57e583bacce2541abf32389675cbb8c1568f30b6b3b9632928`.
Each separate ASan/UBSan/TSan run passes six P2P fixtures and functioning
clean/known-fault controls. All 34 native/diagnostic steps meet expected results.
The completed raw evidence archive verifies all 83 members externally, SHA-256
`c96a36aba288c33d1ac04dab85da98b1dcafbc964c5d737c72289126ee824121`.
R2's source-identical resume passed its own native and three separate diagnostic
sets; that remains intermediate evidence before the final retention correction.
The intentional first build interruption, later 900-second build timeout and
fixture logger initialization crash are retained with their actual causes.

Nine normal-ASLR CLI/synthetic component commands pass on the actual replacement
VPS, including the six P2P fixtures and FakeChain/FakeMempool producer. These are
not full-node, catch-up, sustained-producer or 40 GB restore evidence. Sampled
host MemAvailable stayed above 3,462,856,704 bytes; per-child wait4 RSS includes
launcher fork/exec lifetime and is not native-only peak RSS. The externally
verified binary bundle and raw evidence are preserved. Only the eight owned
component binaries and input archive were retired afterward (121,832,230 bytes);
logs, manifests, receipts and all earlier roots/operational data remain.

An isolated runtime image built from the retained base and final Linux native
binary passes non-root/read-only/network-disabled version/help and executable
hash checks. Image ID is
`sha256:bb4c61f44310926ea12bbbc251306f5d4544323e05b9e61ffef80d2a693303e2`.
The full repository Dockerfile/dependency workflow remains open; nothing is
deployed. Koinos One receives English documentation only, with no packaging,
GUI code or submodule change and no premature link to an unpublished report.

Five alternating decoder pairs per platform remove one approximately 8 MiB C++
allocation request for an already-owned large payload. This does not establish
an equivalent RSS saving or node limit. The final codec is hash-pinned in each
measurement. All completed intermediate/raw evidence is externally preserved;
only verified unused generated object/test/fixture/diagnostic copies are retired.
No database, backup payload, complete history or selected receipt is pruned.
Final synthetic native artifacts (10,800 files/links, 1,269,337,884 logical
bytes; archive SHA-256
`325175da2e92e59c23e5278c1165ab3e36ce89f6433233ae71913cbdbdb62e0d`)
and all raw diagnostic binaries were hash-verified externally before only that
completed artifact tree and 18 copied diagnostic executables were retired.
Original native/build executables and evidence remain; the unchanged builder
reserve is preserved. Strict final Koinos One MkDocs, both diff checks, local
links, identical root guides and source/preservation checks pass. Only two
guidance files differ from the frozen source export; runtime/test inputs match.

Owned evidence root:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/p2c/retention-final/`.
The Linux coordinator and remote live-phase are complete/exit zero; all final
step results and source identities are preserved. Do not rerun completed builds
unless a new source change or unresolved concern justifies it. All required P2/D1/B1 and M1/R1/V1/S1/Q1/H1/full
restore gates remain open. No Git commit/publication/main merge, mainnet signing
or production activation occurs. The persistent issue9 goal remains active.

Next source loop: reproduce retained candidate/known-peer/error-bookkeeping
growth and review a coherent bounded admission/retention policy that preserves
seeds, active peers and meaningful bans. The read-only
`evidence/p2c/retention-final/next-p2-source-inventory.json` pins current source
observations and distinguishes pending lifecycle hypotheses. Preserve watchdog
security evidence and complete history; no disposal-by-budget shortcut.


## 2026-10-16: P2-D peer bookkeeping continuation

Work stays uncommitted on `codex/lr2-memory-hardening`, based on `d3cfb94455da`.
[P2-D](validation/low-resource-lr2-peer-bookkeeping.md) corrects port-split peer
scores, stale admission decay, overflow, optional-candidate growth with discovery
disabled and terminating discovery allocation failure. Review additionally
reproduced/fixed repeated-query score erosion. New finite error-record/key-capacity
limits preserve nonzero records and refuse unknown bookkeeping locally; they
are not aggregate memory bounds. Protected candidate metadata can exceed the
configured candidate count, and candidate bytes remain unbounded.

External evidence is under the existing owned run's `evidence/p2d/`; generated
data remains on My Passport or isolated miner development paths. The 291-input
manifest is `10b59877afabe323399ca5a8803a33256ea5f6208803a87640de267f9878027a`.
Mac all targets/CLI and 56/56 tests pass (186.07 s), actual-clock decay passes,
and 16 vectors match the compiled Go arithmetic adaptation. Five-pair Mac
allocation/elapsed measurements are recorded with exclusions. Linux all targets/
CLI/real-clock and 56/56 exhaustive batches now pass, with
eight fixtures under each separate sanitizer, 11 normal-ASLR VPS components,
runtime-container CLI/hash and direct child CPU/I/O measurements. Do not
substitute earlier P2-C identities. The installed observer
publisher remains its isolated maintenance build. Full-history 40-GB restore,
aggregate memory and sustained target producer gates remain open.

P2-D final evidence is in `evidence/p2d/`: native/diagnostic results, paired and
CPU/I/O measurements, three verified complete native fixture archives and a
195-member raw archive. Strict Koinos One manual/rendered identity and scoped
preservation checks pass. Only 36 externally verified unused duplicate builder
copies were retired; original builds/source/caches/logs/images and live data
remain. Installed publisher service/image/configuration are freshly rechecked.
The goal remains active, with native/SDK known-peer snapshots and connecting-marker
publication rollback recorded as next P2 review candidates; they are not fixed
by P2-D. Aggregate, full diagnostic/dependency, history and mandatory whole-host
producer/40-GB qualification remain open.


## 2026-10-16: P2-E explicit dial continuation

Work remains uncommitted on `codex/lr2-memory-hardening`, based on `d3cfb94455da`.
[P2-E](validation/low-resource-lr2-p2p-dials.md) fixes reproduced stale native
connecting admission, foreign-registry cancellation and hidden seed-route omission.
Scoped count/peer-capacity credits follow queued/SDK owners across restart;
local overload retains protocol and peer-scoring semantics. The default native
32-job/1-MiB policy does not bound implicit GossipSub/RPC/DHT/inbound work,
SDK/native known peers, control queues, DNS or aggregate transport memory.

Final evidence root: the existing owned My Passport run's `evidence/p2e/final/`.
Manifest: `947958d848be5906923b8f968d6d19e01a601f8eb3e7b70bce99a6ed16d31275` (293 inputs).
Mac/Linux all targets/CLI and 57/57 tests pass (153.42 / 141.60 s).
Linux tests are exhaustive 55/1/1 batches with verified external archival before
owned fixture retirement. Nine fixtures per separate ASan/UBSan/TSan pass with
clean/known-fault controls, plus 12 actual-VPS normal-ASLR component commands and
matching runtime-container CLI/hash. The R2 guarded compiler-workspace interruption/recovery and original R1 TSan lock report, reduced
66/0/66 stack/heap/live-lock controls and fixture-only heap-owner pass remain
preserved; no detector suppression, SDK patch or global host setting changes.
Five paired common-harness Mac/Linux measurements include short-loop costs and
Linux child CPU/I/O; ordinary allocation reductions are not node RSS savings.

Final complete raw builder archive: `6acb9fbe545a2f7e0f12118bc46c8c151c9a417bb6bcc7b4f77cbbc4cdaefb24`
(448 member-verified entries). Original source/builds/evidence remain;
retire only independently verified unused owned duplicates. Strict manual,
rendered identity, final scope and installed publisher checks are recorded in
this root before completing the checkpoint. No Git publication, main merge,
release, GUI/package/submodule change, mainnet signing or production activation.
The publisher remains its isolated maintenance build. The persistent goal stays
active: aggregate consumers, full D1/B1/Q1/H1, eligible complete history and actual
producer/full-size 40-GB restore remain open. The prior 4,140,425,216-byte disk
deficit is not waived.

Next P2 review: `next-p2-source-inventory.json` records unbounded native known-peer
retention and multiple full SDK/native snapshot copies, raw host access from
the discovery worker and independent implicit dial paths. A separate stopped-transport probe reproduces 256/1,024 retained native peers
with zero pending dial jobs; the SDK/lifecycle race remains an unproven
hypothesis. Neither observation is a qualified fix. Preserve seeds/active metadata
and meaningful security records when designing optional advice limits.

P2-E closure checks now pass: strict manual/rendered identities, 192 relative
file references, identical guides and preserved HEAD/branch/index plus 364/578
other Teleno/Koinos One files. The active installed publisher, its configuration
and all 14 existing containers are unchanged. Forty unused duplicate builder
copies were retired only after verified external archival; original builds,
source, caches, logs, images and operational data remain. Normal native P2-E
libraries are separately preserved as exact before inputs for the next P2
retention/snapshot comparison. All checkpoint coordinators are terminal; no
background build remains. The full issue9 goal is still active and incomplete.

## 2026-10-16: P2-F peer advice and owned projections

Issue #9 stays active on `codex/lr2-memory-hardening`. Optional advice now has
finite count/string-capacity and failure-atomic borrowed-index/recency retention;
already-retained startup seeds, DHT bootstrap roots and active records are
protected. Move-only metadata projections own shared lifetime credits across
restart. Native queries no longer inspect external SDK repositories or `_host`,
correcting a reproduced native query/stop race (old TSan 66, final 0).

Final R1 manifest (301 inputs): `e100ef823bc7843f26f48a893a36893f4c7768119fab7991c11263769cc818d2`.
Mac/Linux all targets/CLI and 59/59 native checks pass; Linux uses exhaustive
57/1/1 batches and verified external fixture archives before owned retirement.
Eleven fixtures pass each separate ASan/UBSan/TSan with clean/known-fault controls.
R0's temporary Boost.DI scope-mutex TSan cycle remains preserved. A generated
hash-pinned TSan-only lifetime overlay passes reused-lifetime controls while
real live-cycle/destroy-held controls still fail as required. No synthetic
happens-before, suppression, original SDK edit or detector disabling is added.
Actual compile/dependency provenance verifies both native/test overlay use.

Five alternating pairs per 256/2,048-input workload preserve actual old/current
libraries, allocation/time and Linux child CPU/I/O tradeoffs. Fourteen actual-VPS
normal-ASLR synthetic/CLI commands and the matching non-root/read-only/offline
runtime container pass. No restored public-chain node or sustained producer is
implied. The full report is [P2-F](validation/low-resource-lr2-peer-snapshots.md);
final external evidence is `evidence/p2f/final/` beneath the owned My Passport run.
Active/SDK metadata, implicit work, other queues/caller aggregates and full
D1/B1/Q1/H1 remain open. The producer/40-GB restore requirement and last measured
4,140,425,216-byte disk reserve deficit remain unchanged. The installed public
backup writer remains its separate maintenance build.

P2-F closure checks pass: strict manual/rendered identities, 208 relative file
references, 359 other native-repository files and 578 other Koinos One files
preserved, with unchanged HEAD/branch/index and identical guides. The public
latest pointer and manifest/inventory pins still identify the same authenticated
writer backup. The installed publisher image is running with unchanged config
and 14 existing containers. Forty-nine archived/verified unused builder copies
are retired; original builds, source, caches, logs, images and operational data
remain. Final Linux churn user CPU rises while peak RSS is unchanged; allocation
savings are not whole-node CPU/RSS qualification.

## 2026-10-16: P2-G address-cost optimization checkpoint

The approved cost-optimization objective is now registered as the active
persistent thread goal. The exact wording is in the
[goal contract](implementation/issue-9-goal.md). Existing compatibility,
full-history/receipt, mandatory producer and 40-GB restore requirements remain.

P2-G continues the previously started SDK-compatible address repository. New
incremental victim planning eliminates the reproduced full-cache scan and large
temporary victim list. Actual DHT bootstrap/collection/restart, 96 staged
new-peer/eviction fault cases, retained-slot accounting and optional callbacks
have focused coverage. Mac/Linux all targets/CLI and 60/60 native checks pass
(187.58 / 143.23 seconds). Five paired common-harness samples per
256/1,024/4,096-input cohort record original SDK/prototype/current resource and
CPU/I/O/RSS/time tradeoffs. Linux equivalent coverage retains about 49% fewer
attributed C++ request bytes. Corrected thin-launcher runs show smaller
instrumented-process peaks at 1,024 inputs, not a whole-node RSS result.

Frozen native source manifest:
`07342ad3c065c0fe6d8bd632994c7a160b9500304c91468b42cde0b69588cb37`
(304 inputs). Native Mac/Linux executable SHA-256 values:
`724b213d371449a0d4497155f5d02e4bc6dacdfe8d664dade5b6fe0620fe6e77` /
`0a7e24f9fdbdee65b88d49b8748df8ce782e188cf83dd18759743b8c13891592`.
All twelve fixtures per separate ASan/UBSan/TSan and their controls pass, as do
fifteen baseline-server component commands and offline runtime-container CLI/hash.
The raw inherited RSS floors are explained by a controlled launcher experiment;
new thin-launcher pairs preserve the same source/binaries and request bytes. The
Koinos One developer handoff and strict MkDocs/rendered identities pass; no
application/submodule/package update follows.

Owned continuation evidence, paired programs/headers/raw samples, source export
and all verified Linux fixture archives:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/p2g/resume-20261016-rrm0ukg_/`.
Each full native batch was streamed and member/hash-verified before retiring
only its duplicate owned builder fixture output. Builder floors and existing
operational services/data are preserved. Fresh checks retain the installed
publisher service/configuration and fourteen existing containers. Complete
builder/baseline and supplemental thin-RSS archives are externally member/hash-
verified. The completed compiler-only tmpfs child is empty and retired. See the
[P2-G report](validation/low-resource-lr2-sdk-addresses.md) and
[completion matrix](validation/low-resource-lr2-completion-matrix.md).
SDK DNS/other stores/queues, aggregate consumers, full D1/B1/Q1/H1 and actual
whole-node producer/40-GB restore remain open. This is progress, not LR2 or goal
completion; subsequent evidence must retain its exact source identity.

## 2026-10-16: lossless full-archive storage investigation

Issue #9 remains active. The approved full cost objective requires isolated
lossless storage work and the complete 40-GB restore, in addition to LR2;
separate milestone exits and mainnet activation boundaries remain intact.
No native runtime/default or installed publisher change is made in this loop.

Owned run:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/disk-465t9_bg/`.
The pinned public backup is
`20261015T191906Z-ms-1792091946641-files-494`: all 497 files /
29,721,611,361 bytes pass length/SHA-256 verification in `downloaded-source/`.
The immutable original remains. Forty-two complete-table comparisons preserve
306,453 unique sampled block/receipt records. Linux builder/VPS component
commands pass 41/52; the actual VPS uses three alternating rewrite repetitions
and two-pass reads. Zstd level 9 / 64-KiB dictionaries with unchanged 64-KiB
blocks avoid the large random-read CPU/syscall-volume increase of 1-MiB blocks.
Rewrite CPU is about 85% above the matching level-3 control; warm lookup CPU is
about 1.5% higher in the sampled actual-VPS workload. This is not node RSS proof.

`full-small-block-profile/db/` contains the complete copied nine-family view,
including 45,076,063 visible block records. Measured database bytes decrease
29,720,960,254 → 24,840,258,810 (16.422%). Fresh read-only reopen, full checksum
and pointwise key/value/count/digest validation pass across all nine families
in 3,809.88 seconds. Every block contains a receipt; identity mismatches are zero.
45,075,809 receipt roots remain empty exactly as in the input. Inspect
`full-copy-result.json` and the candidate's `report.json` for terminal proof.
Original metadata is unchanged. Do not restart the copy or overwrite this
candidate. Per-phase guards retain data on failure.

Native `BlockStore` queries match all 68 blocks/receipts in six first/middle/
transition/recent ranges, including ID lookup and within-range parents. Empty
historical receipt roots remain exact; no historical repair/replay or complete
parent-chain proof follows from sampled access alone. The separate complete
audit now verifies all 45,076,063 header IDs and the original raw block digest,
then follows all 40,236,770 canonical heights/receipts to the zero genesis parent.
All 4,839,293 other selected records remain. The topology digest is
`f7fcc81a3836a7759d1402a1b10df1d03d06d1a182f85eff64ff10306ae09505`.
Its bounded external-sort/header-hash prototype passes multi-chunk/fork and three negative controls;
an unchanged-binary retry passes after an unattributed initial Mac startup
timeout. `continuity-result.json` and `continuity-audit/report.json` record the
909.97-second completed audit with unchanged candidate metadata. Read-only
public RPC resolves the selected ancestor ID and its successor-header state
root against reported height 40,253,076, with matching chain identity. The API
omits the selected receipt root; no implementation independence or historical
execution pass is inferred. Both full-data coordinators are terminal. Native
runtime source/defaults are still unchanged.

Linux evidence archives have 186/181 independently member/hash-verified files;
only then were 18/19 duplicate isolated SST fixture files retired on the
builder/VPS. Logs, programs and complete external copies remain. The original
archive, operational databases, services, legacy nodes and producer settings
are untouched. Native policy/export/recovery integration, whole-server restore,
canonical/history execution, synchronization, sustained whole-node memory and
producer workloads remain open. See the
[storage experiment](validation/lossless-archive-storage-experiment.md) and
[completion matrix](validation/low-resource-lr2-completion-matrix.md).

## 2026-10-16: persistent native block compression policy

Issue #9 remains active. The storage follow-up now implements optional
`rocksdb.blocks-zstd-level` and `rocksdb.blocks-zstd-dictionary-bytes` for normal
and bottommost block tables. Zero defaults preserve existing behavior. Invalid
scalars/combinations and unavailable required Zstd are refused; dictionary
training uses 1-MiB samples / 8-MiB buffering per active builder. No full-archive
rewrite, operational config change or producer activation occurs in this loop.

Owned run:
`/Volumes/My Passport/teleno-issue9-20261015-krvvxm54/evidence/storage-policy-28a3bhny/`.
The 304-input native manifest is
`5f89837ed3d002ece2355865d6bde342f50441581e15b8719603c89fe09b247a`, with seven
changed source/test paths from P2-G. Base HEAD/branch remain
`d3cfb94455da4a472f651eb4239abbc289e74225` / `codex/lr2-memory-hardening`.
All targets/version/help and all 60 Mac/Linux native tests pass. Mac CTest takes
196.18 seconds. The initial Linux single-copy experiment correctly refuses
after accumulated fixtures leave available space below recommendation. Its
complete failed evidence is externally preserved; additional archived owned
development duplicates are retired and the unchanged binary passes. No source,
space reserve or trust policy is weakened. The old compiled diagnostic copies
retired here can be recovered from `older-builder-evidence.tar.gz`; do not assume
those remote executable paths still exist.

The actual reference VPS passes nine native command invocations: config and
12,000-record flush/compact/default-reader storage fixtures, actual preflight,
version/help, two configured offline opens and the negative normal-startup
control plus its fresh repeat. Initial runtime command exits all match, but an
overly strict harness assertion expects no basedir. Existing normal startup
creates an empty basedir before config loading; the corrected control verifies
no DB/contents instead. Preflight remains filesystem-preserving. Both attempts
and the correction are retained. Peak component RSS is 40,828,928 bytes;
minimum sampled host `MemAvailable` is 3,404,636,160 bytes, with zero swap/OOM.
These are component checks, not a sustained node or producer run.

Native version is `1.3.0-dev.0+d3cfb94455da-dirty`. Executable SHA-256:
Mac `bf5338b88598d4be57388733f45f19931dff6256463f4ebd9053d0615dd16643`;
Linux/VPS `670798389cc53023e7f9a16b90684a4ede7ed01320171920f62a6d12e691ae7f`.
`inputs/teleno_node.before`, `inputs/teleno_node.linux.before` and
`inputs/teleno_node.after` preserve the local before/after binaries; the current
Linux executable is also in `native-component-bundle.tar.gz` and the VPS archive.
Linux/VPS terminal archives have 41/59 member-verified entries, SHA-256
`7b7243c8171f0cb4374fc7bc3b3f14200290d8276be42b17a376da6342f7abc3` /
`e42ef30effde215a7f81dcb8c5e603bcebcc04783accbd6afe20b0ac91e5a43c`.
The compiler-only tmpfs child is empty and retired. The miner publisher remains
active with unchanged config/image and fourteen running containers.

After verified retirement of isolated old baseline development duplicates and
the new binary/fixtures, root available bytes are 36,177,453,056: a modeled
311,238,656-byte margin over the compact candidate's unchanged recommendation.
This is not a measured complete restore or compaction peak. The complete pinned
source and verified compact `db/` remain in the preceding `disk-465t9_bg/` run;
reuse them. The next storage step is a supported native backup/export and
restore/recovery path, followed by actual whole-server, history execution,
synchronization, sustained RSS/headroom and producer workload gates. Do not
restore the experimental artifact parent as a basedir or re-download/rewrite
the verified corpus without a new reason. See
[native policy evidence](validation/native-block-compression-policy.md).
