#!/usr/bin/env python3
"""SC2 signed local fixtures, fault injection and legacy-byte comparison.

Retains all owned evidence, never connects to public hosts or starts a node.
"""
import argparse
import hashlib
import http.server
import json
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.diagnostics.fixture_paths import artifact_root


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--legacy", type=Path, required=True)
    parser.add_argument("--repository", type=Path, required=True)
    args = parser.parse_args()
    repo = args.repository.resolve()
    evidence = artifact_root(repo)
    evidence.mkdir(exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="sc2-harness-", dir=evidence))
    commands, requests = [], []

    def run(argv, expected, name):
        started = time.monotonic()
        result = subprocess.run([str(x) for x in argv], cwd=repo, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        record = dict(argv=[str(x) for x in argv], exit_code=result.returncode,
                      stdout=result.stdout, stderr=result.stderr, elapsed_seconds=time.monotonic() - started)
        commands.append(record)
        (root / (name + ".json")).write_text(json.dumps(record, indent=2) + "\n")
        assert result.returncode == expected, (name, record)
        return json.loads(result.stdout) if Path(argv[0]) == args.fixture and result.stdout.strip() else None

    source = root / "source"
    run([args.legacy, "prepare", source], 0, "prepare")
    public = source / "public"
    initial = {str(p.relative_to(source)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in source.rglob("*") if p.is_file()}
    backup_id = json.loads((source / "fixture.json").read_text())["backup_id"]
    inventory = json.loads((public / "snapshots" / backup_id / "files.json").read_text())["files"]
    fingerprint = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *_args):
            pass

        def do_GET(self):
            mode, _, relative = urllib.parse.urlsplit(self.path).path.lstrip("/").partition("/")
            path = (public / relative).resolve()
            if public not in path.parents or not path.is_file():
                self.send_error(404)
                return
            requests.append(dict(mode=mode, path=relative))
            is_object = relative.startswith("objects/")
            if mode == "changing" and relative == "latest.json" and getattr(self.server, "latest_changed", False):
                self.send_error(500)  # Resume must not request latest again.
                return
            if is_object and mode == "outage":
                self.send_error(503)
                return
            if is_object and mode == "redirect-loop":
                self.send_response(302)
                self.send_header("Location", self.path)
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            length = path.stat().st_size
            self.send_response(200)
            if is_object and mode == "chunked-oversized":
                self.send_header("Transfer-Encoding", "chunked")
            else:
                self.send_header("Content-Length", str(length + (is_object and mode == "oversized")))
            self.send_header("Connection", "close")
            self.end_headers()
            if is_object and mode == "stall":
                time.sleep(2)
                self.close_connection = True
                return
            try:
                if is_object and mode == "chunked-oversized":
                    data = path.read_bytes() + b"x"
                    self.wfile.write(("{:x}\r\n".format(len(data))).encode() + data + b"\r\n0\r\n\r\n")
                elif is_object and mode == "drip":
                    for byte in path.read_bytes():
                        self.wfile.write(bytes([byte]))
                        self.wfile.flush()
                        time.sleep(0.15)
                else:
                    with path.open("rb") as stream:
                        remaining = min(7, length) if is_object and mode == "truncated" else length
                        while remaining:
                            chunk = stream.read(min(65536, remaining))
                            if is_object and mode == "bad-hash" and chunk:
                                chunk = bytes([chunk[0] ^ 1]) + chunk[1:]
                            self.wfile.write(chunk)
                            remaining -= len(chunk)
            except (BrokenPipeError, ConnectionResetError):
                pass
            self.close_connection = True

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = "http://127.0.0.1:{}/".format(server.server_port)
    target = root / "target"

    def direct(route, name, expected=0, resume=False, fault="none"):
        attempt = root / name
        result = run([args.fixture, source, target, attempt, route,
                      "resume" if resume else "new", fault], expected,
                     name + ("-resume" if resume else "-new") + "-" + fault)
        assert not target.exists(), name
        assert not (attempt / "RESTORE_STAGE_COMPLETE").exists()
        assert not (attempt / "objects").exists()
        if result:
            assert not result["ready_to_activate"] and not result["target_database_opened"]
            assert not result["node_started"] and not result["capacity_40gb_qualified"]
        return result

    try:
        results = []
        # One warm-up plus five measured small transfers, same source/host/config.
        for i in range(6):
            result = direct(url + "valid", "valid-{}".format(i))
            assert result["downloaded_files"] == len(inventory) and result["reused_files"] == 0
            for item in inventory:
                path = root / "valid-{}".format(i) / "payload" / item["path"]
                assert path.stat().st_size == item["size_bytes"] and fingerprint(path) == item["sha256"]
            if i:
                results.append(result)
        before = len(requests)
        reused = direct(url + "valid", "valid-5", resume=True)
        assert reused["downloaded_files"] == 0 and reused["reused_files"] == len(inventory)
        assert not any(r["path"].startswith("objects/") for r in requests[before:])
        corrupt = root / "valid-5/payload" / inventory[0]["path"]
        corrupt.write_bytes(corrupt.read_bytes() + b"changed")
        before = len(requests)
        repaired = direct(url + "valid", "valid-5", resume=True)
        assert repaired["downloaded_files"] == 1 and repaired["reused_files"] == len(inventory) - 1
        assert len([r for r in requests[before:] if r["path"].startswith("objects/")]) == 1

        for fault in ("kill-after-descriptor", "kill-mid-file", "kill-after-rename"):
            name = fault
            direct(url + "changing", name, 99, fault=fault)
            verified = sum((root / name / "payload" / item["path"]).is_file() and
                           fingerprint(root / name / "payload" / item["path"]) == item["sha256"]
                           for item in inventory)
            server.latest_changed = True
            before = len(requests)
            recovered = direct(url + "changing", name, resume=True)
            assert recovered["reused_files"] == verified
            assert recovered["downloaded_files"] == len(inventory) - verified
            assert not any(r["path"] == "latest.json" for r in requests[before:])
            assert not list((root / name / "partial").iterdir())
            server.latest_changed = False

        for mode in ("truncated", "oversized", "chunked-oversized", "bad-hash", "stall", "drip", "outage", "redirect-loop"):
            before = len(requests)
            failed = direct(url + mode, "negative-" + mode, 2)
            assert not failed["download_complete"] and failed["state"] == "paused"
            objects = [r for r in requests[before:] if r["path"].startswith("objects/")]
            assert len(objects) == (12 if mode == "redirect-loop" else 2), (mode, objects)
            assert len(list((root / ("negative-" + mode) / "partial").iterdir())) <= 1
        cancelled_result = direct(url + "valid", "cancelled", 2, fault="cancel")
        assert "cancelled" in cancelled_result["error"]
        idle_cancelled = direct(url + "stall", "cancel-idle", 2, fault="cancel-wait")
        assert "cancelled" in idle_cancelled["error"]
        direct(("file://" + str(public)), "no-space", 1, fault="no-space")
        assert not (root / "no-space").exists()

        # Compare exact source bytes with the legacy stage before its DB reopen.
        legacy_output = root / "legacy"
        run([args.legacy, "restore", source, legacy_output, ("file://" + str(public))], 0, "legacy-restore")
        legacy = json.loads((legacy_output / "restore.json").read_text())
        assert {(f["path"], f["sha256"], f["size_bytes"]) for f in legacy["verified_files"]} == {
            (f["path"], f["sha256"], f["size_bytes"]) for f in inventory}
        legacy_allocated = next(p for p in legacy["phases"] if p["phase"] == "after_stage")
        for item in inventory:
            obj = public / "objects/sha256" / item["sha256"][:2] / item["sha256"][2:4] / item["sha256"]
            dest = root / "valid-5/payload" / item["path"]
            assert obj.stat().st_ino != dest.stat().st_ino
        current = {str(p.relative_to(source)): fingerprint(p) for p in source.rglob("*") if p.is_file()}
        assert current == initial
        summary = dict(status="PASS", commands=len(commands), requests=requests, source_preserved=True,
                       node_started=False, measured_samples=5,
                       direct_allocated_bytes=[r["allocated_attempt_bytes"] for r in results],
                       direct_payload_bytes=[r["payload_bytes"] for r in results],
                       direct_peak_rss_bytes=[r["peak_rss_bytes"] for r in results],
                       legacy_after_stage=legacy_allocated,
                       direct_peak_rss_median_bytes=statistics.median(r["peak_rss_bytes"] for r in results))
        (root / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        print("SC2 harness passed; evidence:", root)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == "__main__":
    main()
