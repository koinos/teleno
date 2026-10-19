#!/usr/bin/env python3
"""SC1 local-only CLI regression. Retains synthetic evidence; never starts a node."""
import argparse
import hashlib
import http.server
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import signal
import urllib.parse

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.diagnostics.fixture_paths import artifact_root


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--node", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--repository", type=Path, required=True)
    args = parser.parse_args()
    repo = args.repository.resolve()
    evidence = artifact_root(repo)
    evidence.mkdir(exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="sc1-cli-", dir=evidence))
    commands = []

    def run(argv, expected, name):
        result = subprocess.run([str(arg) for arg in argv], cwd=repo, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        record = {"argv": [str(arg) for arg in argv], "exit_code": result.returncode,
                  "stdout": result.stdout, "stderr": result.stderr}
        commands.append(record)
        (root / (name + ".json")).write_text(json.dumps(record, indent=2) + "\n")
        assert result.returncode == expected, (name, record)
        return result

    source = root / "source"
    run([args.fixture, "prepare", source], 0, "prepare")
    fixture = json.loads((source / "fixture.json").read_text())
    public = source / "public"
    initial = {str(p.relative_to(source)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in source.rglob("*") if p.is_file()}
    config = root / "config.yml"
    config.write_text("features:\n  block_producer: true\nbackup:\n  public-restore:\n"
                      "    enabled: true\n    network: testnet\n    require-https: false\n"
                      "    retries: 1\n    timeout-seconds: 2\n    signature-required: true\n"
                      "    signature-public-key-file: {}\n".format(json.dumps(str(source / "fixture-signing.pub"))))
    target = root / "target"
    scratch = root / "metadata"
    base = [args.node, "--basedir", target, "--config", config, "--backup-restore-mode", "direct",
            "--backup-restore-preflight", "--backup-metadata-dir", scratch,
            "--backup-expected-chain-id", "sc0-synthetic-not-a-chain", "--backup-json"]

    def preflight(url, name, expected=0, extra=()):
        result = run(base + ["--backup-public-url", url] + list(extra), expected, name)
        if expected != 2:
            assert not (target / "db").exists()
        assert not (target / ".backup-just-restored").exists()
        if expected in (0, 2):
            report = json.loads(result.stdout)
            assert report["mode"] == "direct" and report["version"] == 1
            assert report["backup_id"] == fixture["backup_id"]
            assert report["signature_verified"] is True
            assert report["payload_downloaded"] is False and report["target_database_opened"] is False
            assert report["ready_to_restore"] is False and report["start_as_observer_first"] is True
            assert report["space"]["repository_cache_bytes"] == 0
            assert report["existing_filesystem_usage_bytes"] + report["available_bytes"] == report["capacity_bytes"]
            return report

    first = preflight(("file://" + str(public)), "file-latest")
    assert first["source_on_target_filesystem"] is True and not target.exists()
    assert not (scratch / "objects").exists() and not list(scratch.iterdir())
    pinned = preflight(("file://" + str(public)), "file-selected", extra=["--backup-id", fixture["backup_id"]])
    assert pinned["manifest_sha256"] == first["manifest_sha256"]
    assert pinned["inventory_sha256"] == first["inventory_sha256"]
    target.mkdir()
    (target / "config.yml").write_text("features: {block_producer: true}\n")
    preflight(("file://" + str(public)), "existing-config")
    (target / "db").mkdir()
    rejected = preflight(("file://" + str(public)), "existing-db", 2)
    assert rejected["target_eligible"] is False and rejected["rejection_reason"]
    assert (target / "db").is_dir() and not list((target / "db").iterdir())

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *_args):
            pass

        def do_GET(self):
            mode, _, relative = urllib.parse.urlsplit(self.path).path.lstrip("/").partition("/")
            path = (public / relative).resolve()
            if public not in path.parents or not path.is_file():
                self.send_error(404)
                return
            requests.append({"mode": mode, "path": relative})
            if mode == "stall":
                time.sleep(4)
                self.close_connection = True
                return
            if mode == "missing-signature" and relative.endswith("public-bootstrap-signature.json"):
                self.send_error(404)
                return
            if mode == "oversized":
                self.send_response(200)
                self.send_header("Content-Length", str(8 * 1024 * 1024 + 1))
                self.send_header("Connection", "close")
                self.end_headers()
                self.close_connection = True
                return
            if mode == "truncated":
                self.send_response(200)
                self.send_header("Content-Length", "1000")
                self.send_header("Connection", "close")
                self.end_headers()
                self.wfile.write(b"{")
                self.close_connection = True
                return
            data = path.read_bytes()
            if mode == "tampered" and relative.endswith("manifest.json"):
                data += b" "
            if mode == "bad-signature" and relative.endswith("public-bootstrap-signature.json"):
                envelope = json.loads(data)
                signature = envelope["signature_hex"]
                envelope["signature_hex"] = ("0" if signature[0] != "0" else "1") + signature[1:]
                data = json.dumps(envelope).encode()
            self.send_response(200)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(data)
            self.close_connection = True

    requests = []
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        # Use a second fresh target; the first rejected target is preserved.
        base[2] = root / "http-target"
        target = base[2]
        url = "http://127.0.0.1:{}".format(server.server_port)
        preflight(url + "/valid", "http-valid")
        preflight(url + "/tampered", "http-tampered", 1)
        preflight(url + "/missing-signature", "http-missing-signature", 1)
        preflight(url + "/bad-signature", "http-bad-signature", 1)
        preflight(url + "/oversized", "http-oversized", 1)
        partial = run(base + ["--backup-public-url", url + "/truncated"], 1, "http-partial-metadata")
        assert json.loads(partial.stdout)["failure_code"] == "source_partial_message"
        start = time.monotonic()
        stalled = run(base + ["--backup-public-url", url + "/stall"], 1, "http-timeout")
        assert time.monotonic() - start < 3.5
        assert json.loads(stalled.stdout)["failure_code"] == "source_transport_failure"
        assert not target.exists() and not list(scratch.iterdir())
        process = subprocess.Popen([str(x) for x in base + ["--backup-public-url", url + "/stall"]],
                                   cwd=repo, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        deadline = time.monotonic() + 1.5
        while sum(r["mode"] == "stall" for r in requests) < 2 and time.monotonic() < deadline:
            time.sleep(0.01)
        assert sum(r["mode"] == "stall" for r in requests) >= 2
        process.send_signal(signal.SIGTERM)
        stdout, stderr = process.communicate(timeout=3)
        cancellation = {"exit_code": process.returncode, "stdout": stdout, "stderr": stderr}
        (root / "http-cancel.json").write_text(json.dumps(cancellation, indent=2) + "\n")
        assert process.returncode == 130 and json.loads(stdout)["cancelled"] is True
        assert not target.exists() and not list(scratch.iterdir())
        assert not target.exists()
        assert not any(r["path"].startswith("objects/") for r in requests)
        assert sum(r["path"] == "latest.json" and r["mode"] == "valid" for r in requests) == 1
    finally:
        server.shutdown()
        server.server_close()
        thread.join(5)

    # Rejected flag combinations must fail before creating a target or scratch.
    for name, flags in (
        ("stage", ["--backup-restore-mode", "direct", "--backup-restore-stage"]),
        ("activate", ["--backup-restore-mode", "direct", "--backup-restore-activate"]),
        ("public-restore", ["--backup-restore-mode", "direct", "--backup-public-restore"]),
        ("mixed", ["--backup-restore-mode", "direct", "--backup-restore-preflight", "--backup-public-fetch"]),
        ("unknown-mode", ["--backup-restore-mode", "other", "--backup-restore-preflight"]),
        ("unknown-option", ["--backup-unrecognized"]),
        ("repository-mixed", ["--backup-restore-mode", "repository", "--backup-restore", "--backup-restore-stage"]),
        ("orphan-scratch", ["--backup-metadata-dir", scratch]),
        ("missing-scratch", ["--backup-restore-mode", "direct", "--backup-restore-preflight"]),
    ):
        unused = root / ("unused-" + name)
        run([args.node, "--basedir", unused, "--config", config] + flags, 1, name)
        assert not unused.exists()

    # Wrong expected chain must fail; HTTPS policy is not weakened by preflight.
    wrong = list(base)
    wrong[wrong.index("sc0-synthetic-not-a-chain")] = "other-chain"
    run(wrong + ["--backup-public-url", ("file://" + str(public))], 1, "wrong-chain")
    strict_config = root / "https.yml"
    strict_config.write_text("backup:\n  public-restore:\n    require-https: true\n")
    run([args.node, "--basedir", root / "https-target", "--config", strict_config,
         "--backup-restore-mode", "direct", "--backup-restore-preflight", "--backup-metadata-dir", scratch,
         "--backup-public-url", "http://127.0.0.1:1/valid"], 1, "https-required")
    current = {str(p.relative_to(source)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in source.rglob("*") if p.is_file()}
    assert current == initial, "source fixture changed"
    (root / "summary.json").write_text(json.dumps({"status": "PASS", "commands": len(commands),
        "requests": requests, "source_preserved": True, "node_started": False}, indent=2) + "\n")
    print("SC1 CLI checks passed; evidence:", root)


if __name__ == "__main__":
    main()
