# Chain v1.5.2 Fast Replay Release Checklist

Status: 1.2.0 release preparation authorized; corrected observer canary passed on 2026-08-31; publication not executed

This checklist covers actions outside the repository after the repository-side
implementation and validation gates pass. The 1.2.0 native release was
authorized on 2026-08-29, and an isolated observer canary was authorized on
2026-08-30. Neither request activates block production or authorizes changes
to Koinos One.

## Required Inputs

Set these placeholders only after the release reviewer approves the exact
commit, version, canary host, and observer basedir:

```bash
export TELENO_COMMIT="<VALIDATED_TELENO_COMMIT>"
export TELENO_VERSION="<VALIDATED_VERSION>"
export CANARY_BASEDIR="<ISOLATED_OBSERVER_BASEDIR>"
export CANARY_CONFIG="<OBSERVER_CONFIG_PATH>"
```

The canary basedir must be an isolated copy or restore that starts before block
30,504,202. It must not contain producer keys, and no other process may open it.

## Repository And Artifact Gate

From the exact reviewed Teleno commit:

```bash
test "$(git rev-parse HEAD)" = "$TELENO_COMMIT"
test "$(cat VERSION)" = "$TELENO_VERSION"
test -z "$(git status --porcelain)"

KOINOS_BUILD_TESTS=ON ./scripts/build-cpp-libp2p-koinos.sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/teleno_node --version
./build/teleno_node --help

docker build \
  --build-arg VCS_REF="$TELENO_COMMIT" \
  --build-arg BUILD_DATE="$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
  -t "teleno-node:$TELENO_VERSION" .
docker run --rm "teleno-node:$TELENO_VERSION" --version
docker run --rm "teleno-node:$TELENO_VERSION" --help
```

Verify that `VERSION`, `CHANGELOG.md`, the native `--version` output, the
container labels, and the validation report all identify the same source.

## Observer Canary

Start the corrected binary as an observer. The explicit disable is required
even if the supplied config already disables production:

```bash
./build/teleno_node \
  --basedir "$CANARY_BASEDIR" \
  --config "$CANARY_CONFIG" \
  --disable block_producer
```

In a second terminal, verify the head repeatedly and retain the logs:

```bash
curl -fsS http://127.0.0.1:18122/ \
  -H 'content-type: application/json' \
  --data '{"jsonrpc":"2.0","id":1,"method":"chain.get_head_info","params":{}}'

rg -n \
  'delta_replay_fallback|Delta replay re-execution fallbacks|state merkle mismatch|teleno_node ready' \
  "<CANARY_LOG_PATH>"
```

The canary must cross both historical boundaries, report exactly the documented
fallback behavior, reach and follow the current canonical head, and show no
unexplained state-Merkle mismatch. Stop it cleanly, restart with the same
observer-only command, and repeat head, root, peer, and log checks. Do not pass
`--enable block_producer` during this canary.

The authorized corrected canary has crossed both historical boundaries and
completed checked replay through archived height 37,019,561. It applied
6,515,420 blocks in 48,125.3 seconds, reported exactly 70,731 explained
fallbacks, kept P2P and JSON-RPC stopped until indexing completed, and then
reached ready state. The archive-head log SHA-256 is
`7ed12bac6c35a2e367ef46520cfbe88fccbc19499a17b0462d76743eb5f3f3d6`.
During live catch-up, an intermediate SIGINT stop exited zero and the same
observer-only container reopened the same durable basedir. The recovery gate
validated a 60-block block-store backlog before P2P and JSON-RPC started, then
the node returned to ready state with block production and gRPC still
disabled.

The canary subsequently reached and followed current canonical head. Twelve
ten-second samples before the final restart and twelve after it matched the
independent native observer and legacy microservice reference on block ID,
state root, and LIB whenever sampled at the same height; transient one- or
two-block sequential-polling differences resolved on the next sample. The
final clean SIGINT stop exited zero without OOM, and restart of the same image,
arguments, and basedir validated a 60-block backlog before starting P2P and
JSON-RPC. The final exact three-way comparison was at height 38,973,360, with
no unexplained consensus mismatch or producer activity. The corrected
observer canary therefore passed. Its synthetic image revision is validation
evidence only and must not be published as the release artifact.

## Native And Container Publication

The observer canary has passed. Run these commands only after the exact release
commit is reviewed, the repository and artifact gate above is repeated from
that clean commit, and all identities agree. The 1.2.0 release request does
not waive those final source and artifact checks:

```bash
git tag -a "teleno-node-v$TELENO_VERSION" "$TELENO_COMMIT" \
  -m "Teleno node $TELENO_VERSION"
git push origin "teleno-node-v$TELENO_VERSION"

docker tag \
  "teleno-node:$TELENO_VERSION" \
  "ghcr.io/koinos/teleno:$TELENO_VERSION"
docker push "ghcr.io/koinos/teleno:$TELENO_VERSION"
```

Use the repository's approved registry and release workflow if it differs from
the placeholder image name above. Never publish from an unclean tree or from a
binary that was not built from `TELENO_COMMIT`.

## Koinos One Handoff

After the native release is published, provide the validated Teleno commit and
binary identity to the Koinos One maintainer. In the Koinos One repository,
and only after separate authorization, the maintainer can stage the deliberate
submodule update with:

```bash
git -C node/teleno-node fetch origin "$TELENO_COMMIT"
git -C node/teleno-node checkout "$TELENO_COMMIT"
git add node/teleno-node
git diff --cached --submodule=log -- node/teleno-node
```

Koinos One must then rebuild or stage its native binary, run its package and
application checks, and update its own changelog/manual as needed. This Teleno
task does not execute those consumer-repository steps.

## Producer Rollout Gate

Block-producing deployments remain out of scope until observer canaries pass.
Each producer upgrade starts with production disabled. Re-enable production
only under a separate approved runbook after database health, canonical head
progress, peers, producer address, VHP, and signer checks all pass.
