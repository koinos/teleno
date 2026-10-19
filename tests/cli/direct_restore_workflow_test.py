#!/usr/bin/env python3
"""SC4 local-only CLI workflow on owned signed genesis fixtures, no public peers."""
import argparse
from collections import Counter
import fcntl
import hashlib
import http.server
import json
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.diagnostics.fixture_paths import artifact_root


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--node", type=Path, required=True)
    p.add_argument("--fixture", type=Path, required=True)
    p.add_argument("--repository", type=Path, required=True)
    args = p.parse_args()
    repo = args.repository.resolve()
    args.node = args.node.resolve()
    args.fixture = args.fixture.resolve()
    root = Path(tempfile.mkdtemp(prefix="sc4-cli-", dir=artifact_root(repo)))
    commands = []

    def save(name, argv, result):
        record = dict(argv=[str(a) for a in argv], exit_code=result.returncode,
                      stdout=result.stdout, stderr=result.stderr)
        commands.append(record)
        (root / (name + ".json")).write_text(json.dumps(record, indent=2) + "\n")
        return record

    def run(name, argv, expected=0, document=True):
        result = subprocess.run([str(a) for a in argv], cwd=repo, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=40)
        record = save(name, argv, result)
        assert result.returncode == expected, (name, record)
        return json.loads(result.stdout) if document else result

    source = root / "source"
    run("prepare", [args.fixture, "--sc4-prepare", source], document=False)
    fixture = json.loads((source / "fixture.json").read_text())
    public = source / "public"
    source_before = {str(f.relative_to(source)): hashlib.sha256(f.read_bytes()).hexdigest()
                     for f in source.rglob("*") if f.is_file()}
    config = root / "config.yml"
    config.write_text("features: {block_producer: true}\nbackup:\n  public-restore:\n"
                      "    enabled: true\n    network: testnet\n    require-https: false\n"
                      "    retries: 1\n    timeout-seconds: 2\n    signature-required: true\n"
                      "    signature-public-key-file: " + json.dumps(str(source / "fixture-signing.pub")) + "\n")

    def argv(target, attempt, action, url=None, resume=False, text=False, extra=()):
        command = [args.node, "--basedir", target, "--config", config,
                   "--backup-restore-mode", "direct", "--backup-restore-" + action,
                   "--backup-metadata-dir", root / "metadata", "--backup-public-url", url or ("file://" + str(public)),
                   "--backup-expected-chain-id", fixture["chain_id"]]
        if attempt is not None:
            command += ["--backup-output", attempt]
        if resume:
            command += ["--backup-restore-resume"]
        if not text:
            command += ["--backup-json"]
        return command + list(extra)

    target, attempt = root / "target", root / "attempt"
    preflight = run("preflight", argv(target, None, "preflight"))
    assert preflight["signature_verified"] and not preflight["payload_downloaded"] and not target.exists()
    assert preflight["recovery_reserve_profile"] == "standard" and preflight["space"]["reserve_bytes"] == 10 * 1024**3
    profile = ("--backup-restore-reserve-profile", "experimental-8gib")
    e_target, e_attempt = root / "experimental-target", root / "experimental-attempt"
    e_plan = run("experimental-preflight", argv(e_target, None, "preflight", extra=profile))
    assert e_plan["experimental_reserve"] and e_plan["space"]["reserve_bytes"] == 8 * 1024**3
    run("experimental-stage", argv(e_target, e_attempt, "stage", extra=profile))
    run("experimental-resume-missing-profile", argv(e_target, e_attempt, "stage", resume=True), 1)
    run("experimental-resume", argv(e_target, e_attempt, "stage", resume=True, extra=profile))
    run("experimental-activate-missing-profile", argv(e_target, e_attempt, "activate"), 1)
    e_activated = run("experimental-activate", argv(e_target, e_attempt, "activate", extra=profile))
    assert e_activated["observer_hold"] and e_activated["space_policy"]["reserve_bytes"] == 8 * 1024**3
    assert not e_activated["capacity_40gb_qualified"]
    rejected_target = root / "invalid-profile-target"
    run("experimental-invalid-profile", argv(rejected_target, None, "preflight",
        extra=("--backup-restore-reserve-profile", "8")), 1)
    assert not rejected_target.exists()
    target.mkdir()
    active_config = "features: {block_producer: true}\n# reviewed config, never overwrite\n"
    (target / "config.yml").write_text(active_config)
    staged = run("stage", argv(target, attempt, "stage"))
    assert staged["ok"] and staged["attempt_state"] == "download_complete"
    assert staged["source_trust"]["authenticated"] and not staged["observer_hold"]
    assert not (target / "db").exists() and not (target / ".backup-just-restored").exists()
    assert not (attempt / "objects").exists() and not (root / "metadata/objects").exists()
    for name, changed in (
        ("resume-wrong-id", ["--backup-id", "different-selected-id"]),
        ("resume-explicit-latest", ["--backup-id", "latest"]),
    ):
        run(name, argv(target, attempt, "stage", resume=True, extra=changed), 1)
    run("resume-changed-source", argv(target, attempt, "stage", "http://127.0.0.1:1/changed", resume=True), 1)
    stricter_config = root / "changed-policy.yml"
    stricter_config.write_text(config.read_text().replace("require-https: false", "require-https: true"))
    changed_policy = argv(target, attempt, "stage", resume=True)
    changed_policy[changed_policy.index(config)] = stricter_config
    policy = run("resume-changed-https-policy", changed_policy, 1)
    assert policy["failure_phase"] == "attempt_identity"
    inode = lambda path: (path.stat().st_dev, path.stat().st_ino)
    original_inode = inode(attempt / "payload/db")
    # Stage text and JSON expose the same schema/phase. Resume performs no transfer.
    text = run("resume-text", argv(target, attempt, "stage", resume=True, text=True), document=False)
    text_report = json.loads(text.stdout.split("\n", 1)[1])
    assert text_report["stage"]["transferred_bytes"] == 0 and text_report["attempt_state"] == "download_complete"
    activated = run("activate", argv(target, attempt, "activate"))
    assert activated["ok"] and activated["attempt_state"] == "activated" and activated["observer_hold"]
    assert activated["ready_to_start_observer"] and activated["validation"]["read_only"]
    assert not activated["historical_replay_validated"] and not activated["capacity_40gb_qualified"]
    assert inode(target / "db") == original_inode and not (attempt / "payload/db").exists()
    assert (target / "config.yml").read_text() == active_config and not (target / ".pre-restore").exists()
    run("activated-source-replay-refused", argv(target, attempt, "stage", resume=True), 1)
    # Current latest can disappear/change: selected-ID activation never asks for it.
    latest = (public / "latest.json").read_bytes()
    (public / "latest.json").write_text('{"backup_id":"unrelated-new-latest"}')
    run("activated-idempotent-pinned", argv(target, attempt, "activate"))
    (public / "latest.json").write_bytes(latest)

    # Abrupt interruption after the single DB rename is resumed via the CLI.
    t2, a2 = root / "pending-target", root / "pending-attempt"
    run("pending-stage", argv(t2, a2, "stage"))
    pending_inode = inode(a2 / "payload/db")
    run("activation-interrupted", [args.fixture, "--sc4-interrupt-activation", source, t2, a2], 99, False)
    refused = run("pending-startup-refused", [args.node, "--basedir", t2], 1, False)
    assert "unfinished direct restore activation" in refused.stderr
    assert inode(t2 / "db") == pending_inode and not (a2 / "payload/db").exists()
    signature = public / "snapshots" / fixture["backup_id"] / "public-bootstrap-signature.json"
    signed = signature.read_bytes()
    signature.unlink()  # Only this test's owned fixture file; restored below.
    unavailable = run("pending-metadata-unavailable", argv(t2, a2, "activate"), 1)
    assert unavailable["attempt_state"] == "activation_pending" and not unavailable["ok"]
    assert unavailable["observer_hold"] is None and not unavailable["ready_to_start_observer"]
    assert inode(t2 / "db") == pending_inode
    signature.write_bytes(signed)
    run("pending-resume-payload-refused", argv(t2, a2, "stage", resume=True), 1)
    recovered = run("pending-activation-recovered", argv(t2, a2, "activate"))
    assert recovered["observer_hold"] and inode(t2 / "db") == pending_inode

    # Same-file process gate blocks CLI writers, without creating an attempt.
    locked_target = root / "locked-target"
    with (root / ".locked-target.teleno-storage.lock").open("w") as gate:
        (root / ".locked-target.teleno-storage.lock").chmod(0o600)
        fcntl.flock(gate, fcntl.LOCK_EX | fcntl.LOCK_NB)
        run("gate-busy", argv(locked_target, root / "locked-attempt", "stage"), 1)
        assert not (root / "locked-attempt").exists() and not locked_target.exists()

    requests, fault = [], {"mode": "truncate", "objects_seen": 0}
    object_started = threading.Event()

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *_args):
            pass

        def do_GET(self):
            relative = urllib.parse.urlsplit(self.path).path.lstrip("/")
            path = (public / relative).resolve()
            if public not in path.parents or not path.is_file():
                self.send_error(404)
                return
            requests.append(relative)
            data = path.read_bytes()
            is_object = relative.startswith("objects/")
            if is_object:
                fault["objects_seen"] += 1
            mode = fault["mode"] if is_object else "ok"
            self.send_response(200)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Connection", "close")
            self.end_headers()
            try:
                if mode == "truncate" and fault["objects_seen"] >= 2 and len(data) > 32:
                    self.wfile.write(data[:-32])
                    fault["mode"] = "ok"
                elif mode == "slow":
                    self.wfile.write(data[:1])
                    self.wfile.flush()
                    object_started.set()
                    time.sleep(1.5)
                    self.wfile.write(data[1:])
                else:
                    self.wfile.write(data)
            except (BrokenPipeError, ConnectionResetError):
                pass
            self.close_connection = True

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = "http://127.0.0.1:{}".format(server.server_port)
    try:
        ht, ha = root / "http-target", root / "http-attempt"
        failed = run("http-truncated-stage", argv(ht, ha, "stage", url), 1)
        assert failed["attempt_state"] == "paused" and not ht.exists()
        latest_requests = requests.count("latest.json")
        before_resume = len(requests)
        run("resume-explicit-latest-refused", argv(ht, ha, "stage", url, resume=True, extra=["--backup-id", "latest"]), 1)
        assert len(requests) == before_resume
        (public / "latest.json").write_text('{"backup_id":"different-latest"}')
        resumed = run("http-resume", argv(ht, ha, "stage", url, resume=True))
        assert resumed["stage"]["reused_files"] > 0 and requests.count("latest.json") == latest_requests
        activated_http = run("http-activate", argv(ht, ha, "activate", url))
        assert activated_http["observer_hold"] and requests.count("latest.json") == latest_requests
        (public / "latest.json").write_bytes(latest)
        # Cooperative SIGINT during an async object wait records paused/130.
        fault["mode"] = "slow"
        ct, ca = root / "cancel-target", root / "cancel-attempt"
        command = argv(ct, ca, "stage", url)
        child = subprocess.Popen([str(a) for a in command], cwd=repo, text=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            assert object_started.wait(10), "object transfer did not begin"
            child.send_signal(signal.SIGINT)
            stdout, stderr = child.communicate(timeout=15)
        finally:
            if child.poll() is None:
                child.kill()
                child.communicate()
        result = subprocess.CompletedProcess(command, child.returncode, stdout, stderr)
        save("cancelled-stage", command, result)
        report = json.loads(stdout)
        assert child.returncode == 130 and report["attempt_state"] == "paused" and not ct.exists()
        fault["mode"] = "ok"
        run("cancelled-resume", argv(ct, ca, "stage", url, resume=True))
    finally:
        server.shutdown()
        server.server_close()
        thread.join(5)

    # Changed trusted bytes fail authentication; changed payload fails validation.
    mt, ma = root / "modified-target", root / "modified-attempt"
    run("modified-stage", argv(mt, ma, "stage"))
    current = ma / "payload/db/CURRENT"
    current.write_bytes(current.read_bytes() + b"changed")
    bad = run("modified-payload-refused", argv(mt, ma, "activate"), 1)
    assert not mt.exists() and bad["failure_phase"] == "offline_validation"
    manifest = public / "snapshots" / fixture["backup_id"] / "manifest.json"
    old_manifest = manifest.read_bytes()
    manifest.write_bytes(old_manifest + b" ")
    run("modified-signed-metadata-refused", argv(root / "tampered-target", root / "tampered-attempt", "stage"), 1)
    manifest.write_bytes(old_manifest)
    assert not (root / "tampered-attempt").exists()
    # Correctly signed, hash-valid bytes still cannot authorize an inconsistent
    # DB. Once the validator opened it, payload replay remains forbidden.
    inconsistent_source = root / "inconsistent-source"
    run("prepare-inconsistent", [args.fixture, "--sc4-prepare-inconsistent", inconsistent_source], document=False)
    it, ia = root / "inconsistent-target", root / "inconsistent-attempt"
    inconsistent_url = "file://" + str(inconsistent_source / "public")
    run("inconsistent-stage", argv(it, ia, "stage", inconsistent_url))
    failure = run("inconsistent-activation-refused", argv(it, ia, "activate", inconsistent_url), 1)
    assert failure["attempt_state"] == "validating" and failure["failure_phase"] == "offline_validation"
    assert failure["observer_hold"] is None and not failure["ready_to_start_observer"]
    assert not it.exists() and (ia / "payload/db").exists()
    run("validating-source-replay-refused", argv(it, ia, "stage", inconsistent_url, resume=True), 1)
    populated = root / "populated-target"
    (populated / "db").mkdir(parents=True)
    (populated / "db/user-file").write_text("preserve me")
    run("populated-target-refused", argv(populated, root / "populated-attempt", "stage"), 1)
    assert (populated / "db/user-file").read_text() == "preserve me"

    for name, flags in (
        ("mixed", ["--backup-restore-preflight", "--backup-restore-stage"]),
        ("public-direct", ["--backup-public-restore"]),
        ("configured-direct", ["--backup-restore"]),
        ("fetch-direct", ["--backup-public-fetch"]),
        ("resume-no-action", ["--backup-restore-resume"]),
        ("activate-resume", ["--backup-restore-activate", "--backup-restore-resume"]),
        ("enable-producer", ["--backup-restore-stage", "--enable", "block_producer"]),
        ("missing-output-chain", ["--backup-restore-stage"]),
    ):
        unused = root / ("unused-" + name)
        run(name, [args.node, "--basedir", unused, "--config", config,
                   "--backup-restore-mode", "direct", "--backup-json"] + flags, 1)
        assert not unused.exists()
    secret_url = "http://synthetic-no-secret:synthetic-no-secret@example.invalid/backups"
    sanitized = run("credential-url-refused", argv(root / "credential-target", root / "credential-attempt", "stage", secret_url), 1)
    assert "synthetic-no-secret" not in json.dumps(sanitized) and not (root / "credential-attempt").exists()
    run("repository-orphan-resume", [args.node, "--basedir", root / "orphan-resume",
                                    "--backup-restore-stage", "--backup-restore-resume"], 1, False)

    # Existing default repository mode still fetches/stages/activates via legacy
    # callers. The same signed coherent source is independent and preserved.
    lt = root / "legacy-target"
    legacy = run("repository-default", [args.node, "--basedir", lt, "--config", config,
                 "--backup-public-restore", "--backup-public-url", ("file://" + str(public)), "--backup-json"])
    assert legacy["ok"] and legacy["public_fetch"]["signature_verified"] and (lt / "db").exists()
    assert (lt / ".pre-restore").exists() and (lt / ".backup-just-restored").exists()
    # Clean native observer start from direct activation; no keys, sockets, peers.
    log = root / "observer.log"
    with log.open("w") as output:
        command = [args.node, "--basedir", target, "--disable", "p2p", "jsonrpc", "grpc", "mempool",
                   "account_history", "contract_meta_store", "transaction_store"]
        child = subprocess.Popen([str(a) for a in command], cwd=repo, stdout=output, stderr=output)
        try:
            for _ in range(200):
                if "[node] teleno_node ready" in log.read_text():
                    break
                assert child.poll() is None, log.read_text()
                time.sleep(0.05)
            else:
                raise AssertionError("observer did not become ready")
            child.send_signal(signal.SIGINT)
            assert child.wait(timeout=10) == 0
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()
    assert "Restore recovery hold active" in log.read_text() and (target / ".backup-observer-recovery").exists()
    source_after = {str(f.relative_to(source)): hashlib.sha256(f.read_bytes()).hexdigest()
                    for f in source.rglob("*") if f.is_file()}
    assert source_before == source_after, "independent source changed"
    (root / "summary.json").write_text(json.dumps(dict(status="PASS", commands=len(commands),
        fixture=fixture, source_preserved=True, request_counts=dict(Counter(requests)),
        observer_smoke=True, mainnet=False, historical_replay=False, capacity_40gb=False), indent=2) + "\n")
    print("SC4 CLI checks passed; evidence:", root)


if __name__ == "__main__":
    main()
