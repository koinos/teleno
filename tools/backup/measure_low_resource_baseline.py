#!/usr/bin/env python3
"""LR0 local full-history resource baseline; never a mainnet capacity proof."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import stat
import subprocess
import sys

sys.dont_write_bytecode = True
import validate_single_copy_restore as sc5

MAX_METADATA_BYTES = 8 * 1024**2
MAX_COUNTER = 2**64 - 1
CONTRACT = {
    "version": 1, "network": "mainnet", "role": "observer",
    "history": "complete_from_genesis", "pruning_allowed": False,
    "receipt_omission_allowed": False, "light_fallback_allowed": False,
    "whole_server": {"cpus": 2, "ram_bytes": 4_000_000_000,
                     "disk_bytes": 40_000_000_000},
    "experimental_targets": {"steady_node_rss_bytes": 2 * 1024**3,
                             "peak_node_rss_bytes": 5 * 1024**3 // 2,
                             "min_host_available_memory_bytes": 768 * 1024**2,
                             "qualification_horizon_seconds": 7 * 24 * 3600},
    "standard_reserve": "max(10 GiB, database_bytes / 5); unchanged",
}
DISABLED_FIXTURE_FEATURES = ["block_producer", "p2p", "jsonrpc", "grpc", "mempool",
                             "account_history", "contract_meta_store", "transaction_store"]


def metric(value=None, reason=None, source=None):
    result = {"value": value, "status": "observed" if value is not None else "unavailable"}
    if reason:
        result["reason"] = reason
    if source:
        result["source"] = source
    return result


def parse_counters(text, kib=False):
    """Read integer counters; an absent field is never converted to zero."""
    values = {}
    for line in text.splitlines():
        match = re.fullmatch(r"([A-Za-z_]+):?\s+(\d+)(?:\s+(kB))?", line.strip())
        if match:
            values[match[1]] = int(match[2]) * (1024 if kib and match[3] else 1)
    return values


def counters(path, kib=False):
    try:
        return parse_counters(path.read_text(), kib)
    except (OSError, UnicodeError):
        return {}


def counter_metric(values, name, source):
    return metric(values.get(name), None if name in values else "counter not exposed", source)


def linux_memory(proc=Path("/proc")):
    values = counters(proc / "meminfo", True)
    return {key: counter_metric(values, field, "proc/meminfo") for key, field in
            (("physical_bytes", "MemTotal"), ("available_bytes", "MemAvailable"),
             ("swap_total_bytes", "SwapTotal"), ("swap_free_bytes", "SwapFree"))}


def cgroup_v2(pid, proc=Path("/proc"), mount=Path("/sys/fs/cgroup")):
    """Observe only the owned child's cgroup. It may include other processes."""
    missing = {key: metric(reason="owned child cgroup-v2 accounting unavailable") for key in
               ("current_bytes", "peak_bytes", "limit_bytes", "oom_kill_count")}
    if pid is None:
        return missing
    try:
        entries = (proc / str(pid) / "cgroup").read_text().splitlines()
        entry = next(line[3:] for line in entries if line.startswith("0::"))
        relative = PurePosixPath(entry)
        if not entry.startswith("/") or ".." in relative.parts:
            return missing
        directory = (mount / entry.lstrip("/")).resolve()
        if not directory.is_relative_to(mount.resolve()):
            return missing
        result = {}
        for key, name in (("current_bytes", "memory.current"), ("peak_bytes", "memory.peak"),
                          ("limit_bytes", "memory.max")):
            try:
                raw = (directory / name).read_text().strip()
                result[key] = metric(int(raw), source="cgroup-v2/" + name) if raw.isdigit() else metric(
                    reason="unlimited" if raw == "max" else "counter unavailable")
            except OSError:
                result[key] = missing[key]
        events = counters(directory / "memory.events")
        result["oom_kill_count"] = counter_metric(events, "oom_kill", "cgroup-v2/memory.events")
        result["scope"] = "shared cgroup, not attributable solely to the child"
        return result
    except (OSError, StopIteration):
        return missing


def process_metrics(pid, proc=Path("/proc"), system=None):
    system = system or platform.system()
    names = {"rss_bytes": "VmRSS", "rss_high_water_bytes": "VmHWM"}
    values = counters(proc / str(pid) / "status", True) if system == "Linux" and pid else {}
    result = {key: counter_metric(values, name, "owned-child/proc/status")
              for key, name in names.items()}
    io = counters(proc / str(pid) / "io") if system == "Linux" and pid else {}
    result["io"] = {key: counter_metric(io, key, "owned-child/proc/io") for key in
                    ("read_bytes", "write_bytes", "rchar", "wchar")}
    if system != "Linux":
        result["sample_limit"] = "proc counters unavailable; per-command wait4 RSS is recorded separately"
    return result


def storage_groups(root):
    """Names/allocation only; never open RocksDB or parse operator logs/configs."""
    sc5.require(not root.is_symlink(), "symlink sampling root")
    groups = {name: {"files": 0, "logical_bytes": 0, "allocated_bytes": 0} for name in
              ("sst", "wal", "db_metadata", "logs", "objects", "other")}
    seen = {key: set() for key in groups}
    for parent, dirs, files in os.walk(root, followlinks=False):
        for name in dirs + files:
            path = Path(parent) / name
            try:
                info = path.lstat()
            except FileNotFoundError:
                continue
            sc5.require(not stat.S_ISLNK(info.st_mode), "symlink in sampled fixture")
            if not stat.S_ISREG(info.st_mode):
                continue
            relative = path.relative_to(root)
            if "objects" in relative.parts:
                group = "objects"
            elif path.suffix == ".sst":
                group = "sst"
            elif path.suffix == ".log" and name[:-4].isdigit():
                group = "wal"
            elif name.startswith(("MANIFEST-", "OPTIONS-")) or name in ("CURRENT", "IDENTITY", "LOCK"):
                group = "db_metadata"
            elif name.startswith("LOG") or path.suffix in (".stdout", ".stderr", ".log"):
                group = "logs"
            else:
                group = "other"
            groups[group]["files"] += 1
            groups[group]["logical_bytes"] += info.st_size
            inode = info.st_dev, info.st_ino
            if inode not in seen[group]:
                groups[group]["allocated_bytes"] += info.st_blocks * 512
                seen[group].add(inode)
    return groups


def sample_resources(case, pid):
    linux = platform.system() == "Linux"
    return {
        "storage": storage_groups(case), "process": process_metrics(pid),
        "host_memory": linux_memory() if linux else {
            "available_bytes": metric(reason="Linux MemAvailable not exposed on this platform")},
        "cgroup": cgroup_v2(pid) if linux else {
            "current_bytes": metric(reason="cgroup-v2 unavailable on this platform")},
        "rocksdb_pending_compaction_bytes": metric(reason="no live DB-property collector in LR0"),
        "rocksdb_write_stall_seconds": metric(reason="no live DB-property collector in LR0"),
        "canonical_head": metric(reason="socket-free genesis fixture processes no blocks"),
    }


def bounded_json(path):
    sc5.require(path.is_file() and not path.is_symlink(), "metadata must be a regular nonsymlink file")
    with path.open("rb") as stream:
        data = stream.read(MAX_METADATA_BYTES + 1)
    sc5.require(len(data) <= MAX_METADATA_BYTES, "metadata size limit exceeded")
    return json.loads(data), hashlib.sha256(data).hexdigest()


def metadata_review(root):
    """Informational, read-only pinned metadata. Not the native trust validator."""
    sc5.require(not root.is_symlink(), "symlink metadata root")
    manifest, mh = bounded_json(root / "manifest.json")
    inventory, ih = bounded_json(root / "files.json")
    sc5.require(isinstance(manifest, dict) and isinstance(inventory, dict), "metadata object required")
    entries = inventory.get("files")
    sc5.require(isinstance(entries, list) and len(entries) <= 100_000, "invalid file inventory")
    total = database = 0
    seen = set()
    for entry in entries:
        sc5.require(isinstance(entry, dict), "invalid inventory entry")
        name, size, sha = entry.get("path"), entry.get("size_bytes"), entry.get("sha256")
        sc5.require(isinstance(name, str) and 0 < len(name) <= 1024 and "\\" not in name,
                    "invalid inventory path")
        parts = name.split("/")
        sc5.require(all(p not in ("", ".", "..") for p in parts) and name not in seen,
                    "unsafe or duplicate inventory path")
        seen.add(name)
        sc5.require(type(size) is int and 0 <= size <= MAX_COUNTER, "invalid inventory size")
        sc5.require(isinstance(sha, str) and re.fullmatch(r"[0-9a-f]{64}", sha), "invalid inventory hash")
        total += size
        sc5.require(total <= MAX_COUNTER, "inventory total overflow")
        database += size if name.startswith("db/") else 0
    source = manifest.get("source", {})
    public = manifest.get("public_bootstrap", {})
    snapshot = manifest.get("snapshot", {})
    sc5.require(all(isinstance(v, dict) for v in (source, public, snapshot)), "invalid manifest section")
    problems = ["payload not fetched or verified", "independent trust material not established",
                "actual DB identity/head and full historical coverage not verified"]
    if not source.get("chain_id") or not public.get("chain_id"):
        problems.append("missing declared chain identity; native source eligibility blocked")
    if manifest.get("backup_id") != inventory.get("backup_id"):
        problems.append("manifest/inventory backup ID mismatch")
    if snapshot.get("object_count") != len(entries) or snapshot.get("total_bytes") != total:
        problems.append("manifest/inventory count or byte mismatch")
    return {"observed_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "evidence_level": "previously_acquired_metadata_only; not refreshed from publisher",
            "backup_id": manifest.get("backup_id"), "network": public.get("network"),
            "manifest_sha256": mh, "inventory_sha256": ih,
            "declared_logical_bytes": total, "declared_database_bytes": database,
            "files": len(entries), "native_eligible": None, "authenticated": False,
            "full_history_verified": False, "blockers": problems}


def inspect(argv):
    try:
        result = subprocess.run([str(a) for a in argv], cwd=sc5.REPO, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=15)
        return {"exit_code": result.returncode, "output": result.stdout[:65536]}
    except (OSError, subprocess.TimeoutExpired):
        return {"exit_code": None, "unavailable": "tool absent or timed out"}


def dependency_identity(path):
    # A tarball under this repository inherits the parent's git HEAD. That is
    # not the dependency revision, so first require an independent checkout.
    root = inspect(["git", "-C", path, "rev-parse", "--show-toplevel"])
    if root.get("exit_code") != 0 or Path(root.get("output", "").strip()).resolve() != path.resolve():
        return {"revision": None, "status": "unavailable",
                "reason": "not an independent git checkout; installed header digests recorded separately"}
    return {"status": "observed", "revision": inspect(["git", "-C", path, "rev-parse", "HEAD"]),
            "worktree": inspect(["git", "-C", path, "status", "--porcelain"])}


def build_manifest(experiment):
    cache = experiment.cache
    keys = ("CMAKE_HOME_DIRECTORY", "CMAKE_BUILD_TYPE", "CMAKE_CXX_COMPILER",
            "CMAKE_CXX_FLAGS", "CMAKE_CXX_FLAGS_RELEASE", "CMAKE_OSX_ARCHITECTURES",
            "KOINOS_BUILD_TESTS", "TELENO_ROCKSDB_ROOT")
    changed = subprocess.check_output(["git", "ls-files", "-m", "-o", "--exclude-standard", "-z"],
                                      cwd=sc5.REPO).split(b"\0")
    file_hashes = {}
    for raw in changed:
        if raw:
            name = os.fsdecode(raw)
            path = sc5.REPO / name
            if path.is_file() and not path.is_symlink():
                file_hashes[name] = sc5.digest(path)
    dependencies = {}
    for name in ("cpp-libp2p-src", "rocksdb-zstd-src/rocksdb-8.8.1"):
        path = sc5.REPO / ".deps" / name
        dependencies[name] = dependency_identity(path)
    physical = linux_memory()["physical_bytes"] if platform.system() == "Linux" else metric(
        reason="not collected on this platform")
    if platform.system() == "Darwin":
        memory = inspect(["sysctl", "-n", "hw.memsize"])
        if memory.get("exit_code") == 0 and memory["output"].strip().isdigit():
            physical = metric(int(memory["output"].strip()), source="sysctl/hw.memsize")
    headers = {}
    for name in ("rocksdb-zstd-8.8.1/include/rocksdb/version.h",
                 "zstd-static-1.5.7/include/zstd.h", "libssh-static-0.12.0/include/libssh/libssh_version.h"):
        path = sc5.REPO / ".deps" / name
        headers[name] = sc5.digest(path) if path.is_file() else None
    return {"recorded_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "git_commit": inspect(["git", "rev-parse", "HEAD"]),
            "git_branch": inspect(["git", "branch", "--show-current"]),
            "changed_file_sha256": file_hashes, "dependencies": dependencies,
            "dependency_header_sha256": headers,
            "cmake_cache_sha256": sc5.digest(experiment.build / "CMakeCache.txt"),
            "cmake": {key: cache.get(key) for key in keys},
            "native_sha256": sc5.digest(experiment.node), "helper_sha256": sc5.digest(experiment.helper),
            "compiler": inspect([cache["CMAKE_CXX_COMPILER"], "--version"]),
            "cmake_version": inspect(["cmake", "--version"]), "python": platform.python_version(),
            "native_file_identity": inspect(["file", experiment.node]),
            "host": {"os": platform.system(), "release": platform.release(),
                     "os_version": inspect(["sw_vers"]) if platform.system() == "Darwin" else None,
                     "driver_architecture": platform.machine(), "cpu_count": os.cpu_count(),
                     "physical_memory_bytes": physical, "filesystem": sc5.filesystem(experiment.root)},
            "planned_mainnet_template_sha256": sc5.digest(
                sc5.REPO / "config/mainnet-public-bootstrap-observer.yml"),
            "fixture_requested_disabled_features": DISABLED_FIXTURE_FEATURES,
            "fixture_workload": "signed Harbinger genesis + 32 MiB deterministic optional-CF padding",
            "cold_physical_cache": False, "runtime_services_footprint": metric(
                reason="no deployed target runtime; local machine is not the selected VPS")}


def aggregate_resources(commands):
    by_mode = {}
    for mode in ("direct", "repository"):
        samples = [s["resources"] for command in commands
                   if Path(command["stdout"]).name.startswith(mode + "-")
                   and not Path(command["stdout"]).name.startswith(mode + "--1-")
                   for s in command["samples"] if "resources" in s]
        by_mode[mode] = {"samples": len(samples), "sampled_storage_peaks": {
            group: {key: max((s["storage"][group][key] for s in samples), default=None)
                    for key in ("logical_bytes", "allocated_bytes", "files")}
            for group in ("sst", "wal", "db_metadata", "logs", "objects", "other")}}
        for key in ("available_bytes",):
            values = [s["host_memory"][key]["value"] for s in samples
                      if s["host_memory"][key]["value"] is not None]
            by_mode[mode]["min_host_" + key] = metric(min(values) if values else None,
                                                    None if values else "no host counter samples")
    return by_mode


def baseline_report(experiment, manifest, source, checks):
    return {"format": "teleno-lr0-baseline", "version": 1,
            "status": "LOCAL_BASELINE_RECORDED" if experiment.summary["status"] == "SYNTHETIC_PASS" else "BLOCKED",
            "contract": CONTRACT, "build_and_host": manifest, "selected_mainnet_source": source,
            "fixture_result": experiment.summary, "fixture_resource_samples": aggregate_resources(experiment.commands),
            "baseline_checks": checks, "all_baseline_checks_passed": bool(checks) and all(
                check["exit_code"] == 0 for check in checks),
            "full_history_verified": False, "linux_qualified": False,
            "whole_server_40gb_qualified": False, "reserve_calibrated": False,
            "measurement_limits": ["sampled peaks can miss transient allocations; wait4 records child RSS high water",
                                   "allocation is st_blocks, not exclusive APFS extents; category totals may share inodes",
                                   "local host free space includes unrelated activity",
                                   "no steady-state RSS or advancing head from a socket-free genesis fixture",
                                   "SST/WAL file sizes do not identify compaction or write-stall events"],
            "unavailable": {
                "representative_archival_workload": "eligible trusted source and successor range unavailable (LR1)",
                "linux_ext4_whole_machine": "isolated matching Linux build/environment not available in this local run",
                "target_os_service_runtime_footprint": "previous read-only sample, not measured installed feature runtime",
                "combined_wal_compaction_recovery_peak": "representative sustained collector/workload required for LR3",
                "historical_corpus_and_differential_parity": "complete payload and reference ranges not executed"}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=sc5.REPO / "build")
    parser.add_argument("--repetitions", type=int, default=5)
    parser.add_argument("--source-metadata-dir", type=Path)
    parser.add_argument("--baseline-checks", action="store_true",
                        help="record incremental native build, full CTest, version and help; retain failing baseline results")
    parser.add_argument("--dependency-build", action="store_true",
                        help="also record the standard repository dependency build (requires --baseline-checks)")
    args = parser.parse_args()
    experiment = None
    try:
        sc5.require(not args.dependency_build or args.baseline_checks,
                    "--dependency-build requires --baseline-checks")
        source = None
        if args.source_metadata_dir:
            path = args.source_metadata_dir
            sc5.require(not path.is_symlink() and path.resolve().is_relative_to(sc5.EVIDENCE.resolve()),
                        "metadata must be inside ignored repository evidence")
            source = metadata_review(path)
        experiment = sc5.Experiment(args.build_dir.resolve(), args.repetitions,
                                    measurement_sampler=sample_resources)
        checks = []
        if args.baseline_checks:
            commands = []
            if args.dependency_build:
                commands.append(("dependencies", ["env", "KOINOS_BUILD_TESTS=ON",
                                                  "KOINOS_NODE_BUILD_DIR=" + str(experiment.build),
                                                  sc5.REPO / "scripts/build-cpp-libp2p-koinos.sh"]))
            commands.extend((("build", ["cmake", "--build", experiment.build, "--parallel", "4"]),
                             ("ctest", ["ctest", "--test-dir", experiment.build, "--output-on-failure"]),
                             ("version", [experiment.node, "--version"]),
                             ("help", [experiment.node, "--help"])))
            for name, argv in commands:
                _, record = experiment.command("lr0-baseline-" + name, argv,
                                               expected=tuple(range(-64, 256)), timeout_seconds=600)
                checks.append({"name": name, "exit_code": record["exit_code"],
                               "stdout": record["stdout"], "stderr": record["stderr"]})
        # Record the actual artifacts/config after builds, before measured commands.
        experiment.cache = sc5.check_build(experiment.build)
        manifest = build_manifest(experiment)
        sc5.save(experiment.root / "lr0-environment.json", manifest)
        code = experiment.run()
        report = baseline_report(experiment, manifest, source, checks)
        sc5.save(experiment.root / "lr0-baseline.json", report)
        sc5.save(experiment.root / "lr0-artifact-sha256.json", {
            str(path.relative_to(experiment.root)): sc5.digest(path)
            for path in experiment.root.rglob("*") if path.is_file() and path.suffix != ".sst"})
        print("LR0 baseline:", experiment.root / "lr0-baseline.json")
        print("Local fixture:", report["status"], "; full-history/40 GB qualification: UNVERIFIED")
        return code
    except (ValueError, OSError, KeyError, json.JSONDecodeError) as error:
        if experiment is not None:
            sc5.save(experiment.root / "lr0-baseline.json", {
                "format": "teleno-lr0-baseline", "version": 1, "status": "BLOCKED",
                "blocker": str(error), "whole_server_40gb_qualified": False,
                "full_history_verified": False, "reserve_calibrated": False})
            print("LR0 incomplete evidence:", experiment.root)
        print("LR0 blocked:", str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
