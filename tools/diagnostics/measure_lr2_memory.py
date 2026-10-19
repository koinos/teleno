#!/usr/bin/env python3
"""Bounded offline genesis/RPC measurements, never mainnet capacity evidence."""
import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import socket
import subprocess
import sys
import time

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO))
from tools.diagnostics.fixture_paths import artifact_root, build_is_authorized


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def run(binary, output, repetitions):
    require(1 <= repetitions <= 5, "repetitions must be 1..5")
    binary = binary.resolve()
    require(binary.is_file(), "binary must exist")
    if not binary.is_relative_to(REPO):
        require(build_is_authorized(binary.parent, REPO), "binary must belong to selected build")
        cache = binary.parent / "CMakeCache.txt"
        require(cache.is_file() and ("CMAKE_HOME_DIRECTORY:INTERNAL=" + str(REPO)) in cache.read_text().splitlines(),
                "external build must name this repository as CMake source")
    output = output.resolve()
    evidence = artifact_root(REPO)
    require(output != evidence and output.is_relative_to(evidence), "output must be a child of selected evidence")
    require(not output.exists(), "output must be fresh")
    output.mkdir(parents=True)
    results = []
    for index in range(repetitions):
        root = output / str(index)
        root.mkdir()
        shutil.copyfile(REPO / "config/example/harbinger/genesis_data.json", root / "genesis_data.json")
        config = root / "config.yml"
        config.write_text("jsonrpc:\n  listen: 127.0.0.1:0\n  jobs: 2\n  io-timeout-seconds: 1\n"
                          "features:\n  block_producer: false\n  p2p: false\n  grpc: false\n")
        stdout, stderr = root / "stdout.log", root / "stderr.log"
        samples = []
        connections = []
        child = None
        try:
            with stdout.open("wb") as out, stderr.open("wb") as err:
                argv = [str(binary), "--basedir", str(root), "--config", str(config),
                        "--disable", "block_producer", "p2p", "grpc"]
                child = subprocess.Popen(argv, cwd=REPO, stdout=out, stderr=err, start_new_session=True)
                deadline = time.monotonic() + 15
                while True:
                    require(time.monotonic() < deadline, "startup deadline")
                    require(child.poll() is None, "node exited before ready")
                    text = stdout.read_text(errors="replace") + stderr.read_text(errors="replace")
                    require(len(text) < 2*1024*1024, "log limit")
                    listening = re.search(r"\[jsonrpc\] Listening on 127\.0\.0\.1:(\d+)", text)
                    if listening and "[node] teleno_node ready" in text:
                        break
                    time.sleep(.02)
                port = int(listening.group(1))
                connection = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
                connections.append(connection)
                connection.request("GET", "/health")
                response = connection.getresponse()
                require(response.status == 200, "health failed")
                identity = json.loads(response.read())["version"]
                # Owned process samples only. RSS is not total host/cgroup memory.
                for _ in range(10):
                    measured = subprocess.check_output(["ps", "-o", "rss=", "-p", str(child.pid)], text=True, timeout=2)
                    samples.append(int(measured.strip()) * 1024)
                    time.sleep(.12)
                require(connection.sock.recv(1) == b"", "idle session did not expire")
                partial = socket.create_connection(("127.0.0.1", port), timeout=2)
                connections.append(partial)
                partial.sendall(b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\n{")
                time.sleep(.05)
                started = time.monotonic()
                child.send_signal(signal.SIGTERM)
                deadline = started + 5
                while True:
                    pid, status, usage = os.wait4(child.pid, os.WNOHANG)
                    if pid:
                        child.returncode = os.waitstatus_to_exitcode(status)
                        break
                    require(time.monotonic() < deadline, "shutdown deadline")
                    time.sleep(.02)
                require(child.returncode == 0, "node did not shut down cleanly")
                require(partial.recv(1) == b"", "partial session survived shutdown")
                results.append({"argv": argv, "identity": identity, "exit_code": child.returncode,
                                "shutdown_seconds": time.monotonic()-started, "rss_samples_bytes": samples,
                                "peak_rss_bytes": usage.ru_maxrss * (1 if platform.system()=="Darwin" else 1024),
                                "host_mem_available_bytes": None, "cgroup_memory_bytes": None,
                                "workload": "offline Harbinger genesis, ordinary RPC, idle expiry, partial-read shutdown"})
        finally:
            for connection in connections:
                connection.close()
            if child is not None and child.returncode is None:
                os.killpg(child.pid, signal.SIGTERM)
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(child.pid, signal.SIGKILL)
                    child.wait()
            (output / "results.json").write_text(json.dumps({"runs": results,
                "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                "mainnet_qualified": False, "linux_qualified": False,
                "full_history_capacity_qualified": False}, indent=2))
    print(json.dumps({"output": str(output), "runs": len(results), "mainnet_qualified": False}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=REPO / "build/teleno_node")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, default=5)
    args = parser.parse_args()
    require(platform.system() in ("Darwin", "Linux"), "POSIX wait4 required")
    run(args.binary.resolve(), args.output, args.repetitions)
