#!/usr/bin/env python3
"""Measure existing restore APIs using owned, repository-local synthetic fixtures."""
import argparse
import datetime
import hashlib
import http.server
import json
import os
from pathlib import Path
import platform
import re
import signal
import statistics
import subprocess
import tempfile
import threading
import time
import urllib.parse

REPOSITORY = Path(__file__).resolve().parents[2]
import sys
sys.path.insert(0, str(REPOSITORY))
from tools.diagnostics.fixture_paths import artifact_root, build_is_authorized
EVIDENCE = artifact_root(REPOSITORY)


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def inspect_command(argv, repository=REPOSITORY):
    result = subprocess.run(argv, cwd=repository, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=30)
    return {"argv": argv, "exit_code": result.returncode, "output": result.stdout.strip()}


def cache_values(build):
    values = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line and not line.startswith(("#", "//")) and ":" in line and "=" in line:
            name, value = line.split("=", 1)
            values[name.split(":", 1)[0]] = value
    if values.get("CMAKE_HOME_DIRECTORY") != str(REPOSITORY):
        raise ValueError("native build belongs to another workspace")
    if values.get("KOINOS_BUILD_TESTS") != "ON":
        raise ValueError("native baseline must have tests enabled")
    return values


def worktree(repository=REPOSITORY):
    repository = repository.resolve()
    try:
        git_root = inspect_command(["git", "rev-parse", "--show-toplevel"], repository)
    except (OSError, subprocess.TimeoutExpired) as error:
        git_root = {"unavailable": str(error)}
    if (git_root.get("exit_code") != 0
            or Path(git_root["output"]).resolve() != repository):
        selected = os.environ.get("TELENO_TEST_SOURCE_MANIFEST")
        if not selected:
            raise RuntimeError("source export requires explicit TELENO_TEST_SOURCE_MANIFEST")
        manifest_path = Path(selected)
        if not manifest_path.is_absolute() or not manifest_path.is_file():
            raise RuntimeError("source manifest must be an existing absolute file")
        manifest_bytes = manifest_path.read_bytes()
        manifest = json.loads(manifest_bytes)
        if (manifest.get("format") != "teleno-native-input-manifest"
                or manifest.get("version") != 1
                or not isinstance(manifest.get("native_files"), dict)
                or not manifest["native_files"]):
            raise RuntimeError("unsupported or empty source manifest")
        hashes = {}
        for relative, row in manifest["native_files"].items():
            name = Path(relative)
            if (not relative or name.is_absolute() or ".." in name.parts
                    or name.as_posix() != relative or relative == "."):
                raise RuntimeError("source manifest path must be canonical and relative")
            path = repository / name
            if path.resolve() != path or not path.is_file():
                raise RuntimeError("source manifest file is missing or linked: " + relative)
            if (not isinstance(row, dict) or type(row.get("size")) is not int
                    or row["size"] < 0 or not isinstance(row.get("sha256"), str)
                    or re.fullmatch(r"[0-9a-f]{64}", row["sha256"]) is None):
                raise RuntimeError("invalid source manifest record: " + relative)
            actual = digest(path)
            if path.stat().st_size != row["size"] or actual != row["sha256"]:
                raise RuntimeError("source manifest mismatch: " + relative)
            hashes[relative] = actual
        return {"mode": "verified_source_export", "git_available": False,
                "git_root_probe": git_root, "file_sha256": hashes,
                "source_manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest(),
                "manifest_base_git_head": manifest.get("git_head"),
                "manifest_branch": manifest.get("branch"), "tracked_diff_sha256": None}
    status = inspect_command(["git", "status", "--short", "--branch"], repository)
    if status["exit_code"] != 0:
        raise RuntimeError("cannot collect source worktree status")
    changed = subprocess.check_output(["git", "diff", "--name-only", "-z"], cwd=repository).split(b"\0")
    untracked = subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard", "-z"], cwd=repository).split(b"\0")
    hashes = {}
    for name in changed + untracked:
        if name:
            relative = os.fsdecode(name)
            path = repository / relative
            if path.is_file():
                hashes[relative] = digest(path)
    return {"mode": "git_worktree", "git_available": True, "status": status, "file_sha256": hashes,
            "tracked_diff_sha256": hashlib.sha256(subprocess.check_output(
                ["git", "diff", "--binary"], cwd=repository)).hexdigest()}


def environment(build):
    commands = [["git", "rev-parse", "HEAD"], ["git", "branch", "--show-current"],
                ["sw_vers"], ["uname", "-sm"], ["sysctl", "-n", "hw.memsize", "hw.logicalcpu", "hw.model"],
                ["df", "-k", str(REPOSITORY)], ["cmake", "--version"], ["ctest", "--version"],
                ["ninja", "--version"], ["c++", "--version"], ["xcode-select", "-p"],
                ["sysctl", "-n", "hw.optional.arm64"], ["sysctl", "-n", "sysctl.proc_translated"],
                ["file", str(build / "teleno_node")]]
    results = []
    for command in commands:
        try:
            results.append(inspect_command(command))
        except (OSError, subprocess.TimeoutExpired) as error:
            results.append({"argv": command, "unavailable": str(error)})
    dependency_revisions = {}
    for name in ("cpp-libp2p-src", "sc0-teleno-node-hunter-build/_deps/koinos_cmake-src"):
        directory = REPOSITORY / ".deps" / name
        if (directory / ".git").exists():
            dependency_revisions[name] = inspect_command(["git", "-C", str(directory), "rev-parse", "HEAD"])
    st = os.statvfs(REPOSITORY)
    compiler = {}
    for compiler_file in (build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"):
        compiler = {"path": str(compiler_file), "sha256": digest(compiler_file),
                    "recorded_variables": dict(re.findall(
                        r'set\((CMAKE_CXX_COMPILER(?:_ID|_VERSION|_ARCHITECTURE_ID)?) "([^"\n]*)"\)',
                        compiler_file.read_text()))}
    return {"recorded_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "commands": results, "dependency_revisions": dependency_revisions,
            "python_version": platform.python_version(), "native_cache": cache_values(build),
            "native_cache_sha256": digest(build / "CMakeCache.txt"), "native_compiler": compiler,
            "driver_architecture": platform.machine(),
            "filesystem": {"fragment_bytes": st.f_frsize, "available_bytes": st.f_bavail * st.f_frsize,
                           "total_bytes": st.f_blocks * st.f_frsize}, "worktree": worktree()}


def allocation(root):
    logical = allocated = count = 0
    seen = set()
    if root.exists():
        for parent, _, files in os.walk(root, followlinks=False):
            for name in files:
                path = Path(parent) / name
                try:
                    st = os.stat(path, follow_symlinks=False)
                except FileNotFoundError:
                    continue  # A temporary file can be renamed during a sample.
                if path.is_symlink() or not path.is_file():
                    continue
                logical += st.st_size
                count += 1
                identity = (st.st_dev, st.st_ino)
                if identity not in seen:
                    seen.add(identity)
                    allocated += st.st_blocks * 512
    return {"logical_bytes": logical, "allocated_bytes": allocated, "files": count}


def run_logged(argv, log, destination=None, expected_success=True):
    samples = []
    started = time.monotonic()
    with log.open("wb") as stream:
        process = subprocess.Popen(argv, cwd=REPOSITORY, stdout=stream, stderr=subprocess.STDOUT)
        try:
            while process.poll() is None:
                if time.monotonic() - started > 60:
                    raise TimeoutError("owned fixture command exceeded 60 seconds")
                if destination is not None:
                    samples.append({"elapsed_seconds": time.monotonic() - started,
                                    "destination": allocation(destination),
                                    "available_bytes": os.statvfs(destination.parent).f_bavail * os.statvfs(destination.parent).f_frsize})
                time.sleep(0.02)
            code = process.wait()
        except BaseException:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            raise
    result = {"argv": [str(arg) for arg in argv], "exit_code": code,
              "elapsed_seconds": time.monotonic() - started, "expected_success": expected_success,
              "log": str(log), "samples": samples}
    write_json(log.with_suffix(".command.json"), result)
    if (code == 0) != expected_success:
        raise RuntimeError("unexpected fixture outcome: {} (see {})".format(code, log))
    return result


class FixtureHandler(http.server.BaseHTTPRequestHandler):
    """Loopback-only source with finite valid, tampered, and truncated responses."""
    def log_message(self, *_args):
        pass

    def do_GET(self):
        parts = urllib.parse.urlsplit(self.path).path.strip("/").split("/", 1)
        if len(parts) != 2 or parts[0] not in ("valid", "tampered", "truncated"):
            self.send_error(404)
            return
        mode, relative = parts
        path = (self.server.fixture_root / relative).resolve()
        if self.server.fixture_root not in path.parents or not path.is_file():
            self.send_error(404)
            return
        if path.stat().st_size > 8 * 1024 * 1024:
            self.send_error(413)
            return
        self.server.requests.append({"mode": mode, "path": relative})
        tampered = mode == "tampered" and relative.endswith("manifest.json")
        truncated = mode == "truncated" and relative.startswith("objects/")
        length = path.stat().st_size + int(tampered)
        self.send_response(200)
        self.send_header("Content-Length", str(length))
        self.send_header("Connection", "close")
        self.end_headers()
        with path.open("rb") as stream:
            remaining = min(length, 7) if truncated else length
            while remaining:
                data = stream.read(min(65536, remaining))
                if not data:
                    break
                self.wfile.write(data)
                remaining -= len(data)
        if tampered:
            self.wfile.write(b" ")
        self.close_connection = True


def summary(reports):
    result = {}
    for transport in ("file", "public-http"):
        group = [report for report in reports if report["transport"] == transport]
        result[transport] = {"sample_count": len(group),
                             "elapsed_median_seconds": statistics.median(r["elapsed_seconds"] for r in group),
                             "elapsed_min_seconds": min(r["elapsed_seconds"] for r in group),
                             "elapsed_max_seconds": max(r["elapsed_seconds"] for r in group),
                             "peak_rss_median_bytes": statistics.median(r["peak_rss_bytes"] for r in group),
                             "duplicated_payload_bytes": sorted(set(r["duplicated_payload_bytes"] for r in group)),
                             "after_stage_allocated_bytes": [next(p for p in r["phases"] if p["phase"] == "after_stage") for r in group]}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=REPOSITORY / "build-sc0-baseline")
    parser.add_argument("--output-parent", type=Path, default=EVIDENCE)
    parser.add_argument("--repetitions", type=int, default=5)
    parser.add_argument("--capture-only", type=Path, help="write environment evidence without executing fixtures")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    parent = args.output_parent.resolve()
    if not build_is_authorized(build, REPOSITORY):
        parser.error("build directory must be repository-local or explicitly selected")
    if parent != EVIDENCE and EVIDENCE not in parent.parents:
        parser.error("output parent must be within build-single-copy-evidence")
    if not 1 <= args.repetitions <= 10:
        parser.error("repetitions must be between 1 and 10")
    if args.capture_only:
        output = args.capture_only.resolve()
        if EVIDENCE not in output.parents or output.exists():
            parser.error("capture must be a new evidence file")
        write_json(output, environment(build))
        return
    executable = build / "koinos_restore_baseline_fixture"
    if not executable.is_file():
        parser.error("build koinos_restore_baseline_fixture first")
    parent.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="sc0-run-", dir=parent))
    write_json(root / "environment.json", environment(build))
    commands = []
    fixture = root / "source"
    commands.append(run_logged([str(executable), "prepare", str(fixture)], root / "prepare.log"))
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), FixtureHandler)
    server.fixture_root = fixture / "public"
    server.requests = []
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    reports = []
    blockers = []
    negative_outcomes = []
    try:
        http_url = "http://127.0.0.1:{}/valid".format(server.server_port)
        for label, url in (("file", "file://" + str(fixture / "public")), ("http", http_url)):
            for index in range(args.repetitions + 1):
                name = "{}-{}".format(label, "warmup" if index == 0 else index)
                destination = root / name
                command = run_logged([str(executable), "restore", str(fixture), str(destination), url],
                                     root / (name + ".log"), destination)
                commands.append(command)
                report = json.loads((destination / "restore.json").read_text())
                if index:
                    reports.append(report)
        for mode, expected_message in (("tampered", "manifest hash mismatch"), ("truncated", "source_partial_message")):
            destination = root / mode
            url = "http://127.0.0.1:{}/{}".format(server.server_port, mode)
            commands.append(run_logged([str(executable), "restore", str(fixture), str(destination), url],
                                       root / (mode + ".log"), destination, expected_success=False))
            error = (root / (mode + ".log")).read_text()
            code = commands[-1]["exit_code"]
            negative_outcomes.append({"case": mode, "exit_code": code,
                                      "expected_diagnostic_present": expected_message in error,
                                      "log": commands[-1]["log"]})
            if code < 0:
                blockers.append({"case": mode, "kind": "native_signal", "signal": signal.Signals(-code).name,
                                 "exit_code": code, "log": commands[-1]["log"]})
            elif expected_message not in error:
                blockers.append({"case": mode, "kind": "unexpected_error_diagnostic", "exit_code": code,
                                 "log": commands[-1]["log"]})
            if (destination / "target/db").exists() or (destination / "target/.backup-just-restored").exists():
                raise RuntimeError("rejected transfer activated a database")

        # Confirm any signal using the existing node CLI fetch path, without opening a DB.
        if any(b["kind"] == "native_signal" for b in blockers):
            cli = root / "cli-truncated"
            cli.mkdir()
            target = cli / "target"
            config = cli / "config.yml"
            config.write_text("features:\n  block_producer: false\nbackup:\n  enabled: true\n"
                              "  local:\n    enabled: true\n    directory: {}\n"
                              "  public-restore:\n    network: testnet\n    require-https: false\n"
                              "    retries: 1\n    signature-required: true\n    signature-public-key-file: {}\n".format(
                                  json.dumps(str(cli / "repository")), json.dumps(str(fixture / "fixture-signing.pub"))))
            commands.append(run_logged([str(build / "teleno_node"), "--basedir", str(target), "--config", str(config),
                                        "--backup-public-fetch", "--backup-public-url",
                                        "http://127.0.0.1:{}/truncated".format(server.server_port), "--backup-id",
                                        json.loads((fixture / "fixture.json").read_text())["backup_id"], "--backup-json"],
                                       root / "cli-truncated.log", cli, expected_success=False))
            code = commands[-1]["exit_code"]
            cli_error = (root / "cli-truncated.log").read_text()
            negative_outcomes.append({"case": "cli-truncated", "exit_code": code,
                                      "expected_diagnostic_present": "source_partial_message" in cli_error,
                                      "log": commands[-1]["log"]})
            if code < 0:
                blockers.append({"case": "cli-truncated", "kind": "native_signal", "signal": signal.Signals(-code).name,
                                 "exit_code": code, "log": commands[-1]["log"]})
            elif "source_partial_message" not in cli_error:
                blockers.append({"case": "cli-truncated", "kind": "unexpected_error_diagnostic",
                                 "exit_code": code, "log": commands[-1]["log"]})
            if (target / "db").exists() or (target / ".backup-just-restored").exists():
                raise RuntimeError("rejected CLI fetch activated a database")
        write_json(root / "commands.json", commands)
        write_json(root / "http-requests.json", server.requests)
        write_json(root / "summary.json", {"status": "BLOCKED" if blockers else "PASS",
                                           "fixture": json.loads((fixture / "fixture.json").read_text()),
                                           "measurements": summary(reports), "negative_cases": ["tampered", "truncated"],
                                           "negative_outcomes": negative_outcomes,
                                           "blockers": blockers, "warmups_per_transport": 1, "node_started": False})
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
        write_json(root / "commands.json", commands)
    inventory = {str(path.relative_to(root)): digest(path) for path in sorted(root.rglob("*"))
                 if path.is_file() and not path.is_symlink()}
    write_json(root / "artifact-sha256.json", inventory)
    print("SC0 evidence:", root)
    print("SC0 fixture baseline:", "BLOCKED" if blockers else "PASS")
    for blocker in blockers:
        print("Baseline blocker:", json.dumps(blocker, sort_keys=True))
    if blockers:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
