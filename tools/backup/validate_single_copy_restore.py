#!/usr/bin/env python3
"""SC5 bounded local experiment, not a production restore or capacity guarantee."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import signal
import shutil
import stat
import statistics
import subprocess
import sys
import tempfile
import time

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO))
from tools.diagnostics.fixture_paths import artifact_root, build_is_authorized
EVIDENCE = artifact_root(REPO)
HOST_RESERVE = 10 * 1024**3


def require(condition, message):
    if not condition:
        raise ValueError(message)


def save(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def hashes(root):
    allocation(root)  # Refuse symlinks before hashing any owned fixture tree.
    return {str(p.relative_to(root)): digest(p) for p in root.rglob("*") if p.is_file()}


def allocation(root):
    logical = allocated = files = 0
    inodes = set()
    require(not root.is_symlink(), "symlink measurement root")
    if not root.exists():
        return dict(logical_bytes=0, allocated_bytes=0, files=0)
    for parent, dirs, names in os.walk(root, followlinks=False):
        for name in dirs + names:
            try:
                s = (Path(parent) / name).lstat()
            except FileNotFoundError:
                continue  # Owned temporary files can disappear during sampling.
            require(not stat.S_ISLNK(s.st_mode), "symlink in measured destination")
            if stat.S_ISREG(s.st_mode):
                logical += s.st_size
                files += 1
            key = s.st_dev, s.st_ino
            if key not in inodes:
                allocated += s.st_blocks * 512
                inodes.add(key)
    return dict(logical_bytes=logical, allocated_bytes=allocated, files=files)


def filesystem(path):
    s = os.statvfs(path)
    return dict(total_bytes=s.f_blocks * s.f_frsize, available_bytes=s.f_bavail * s.f_frsize,
                fragment_bytes=s.f_frsize, device=path.stat().st_dev)


def rss_bytes(value, system=None):
    return int(value) * (1 if (system or platform.system()) == "Darwin" else 1024)


def validate_capacity(observed, declared):
    require(0 < observed["total_bytes"] <= declared and
            0 <= observed["available_bytes"] <= observed["total_bytes"],
            "mounted capacity exceeds declared budget")


def distribution(values):
    require(bool(values), "empty measurement set")
    median = statistics.median(values)
    return dict(count=len(values), median=median, min=min(values), max=max(values),
                mad=statistics.median(abs(v - median) for v in values))


def check_build(build):
    values = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line and not line.startswith(("#", "//")) and ":" in line and "=" in line:
            k, v = line.split("=", 1)
            values[k.split(":", 1)[0]] = v
    require(values.get("CMAKE_HOME_DIRECTORY") == str(REPO), "foreign build tree")
    require(values.get("KOINOS_BUILD_TESTS") == "ON", "test-enabled build required")
    require(build_is_authorized(build, REPO), "build must be repository-local or explicitly selected")
    return values


def retire_owned_pair(destination, number, pair, owners, evidence):
    """Retire only this run's verified synthetic cases, after recording hashes."""
    started, before = time.monotonic(), resource.getrusage(resource.RUSAGE_SELF)
    require(destination.parent == evidence and destination.name == "destination" and
            not evidence.is_symlink() and not destination.is_symlink(),
            "foreign synthetic destination")
    require(set(pair) == {"direct", "repository"} and
            pair["direct"]["verification"] == pair["repository"]["verification"],
            "unverified synthetic pair")
    require(shutil.rmtree.avoids_symlink_attacks, "safe fixture retirement is unavailable")
    prepared = {}
    for mode in ("direct", "repository"):
        row = pair[mode]
        require(row["mode"] == mode and row["number"] == number,
                "foreign synthetic pair result")
        case = destination / (mode + "-" + str(number))
        require(case in owners and not case.is_symlink(), "unowned synthetic case")
        identity = case.stat()
        require((identity.st_dev, identity.st_ino) == owners[case],
                "synthetic case identity changed")
        prepared[mode] = dict(case=case.name, identity=list(owners[case]),
                              files=hashes(case), allocation=allocation(case))
    journal = evidence / ("retired-pair-" + str(number) + ".json")
    require(not journal.exists() and not journal.is_symlink(),
            "synthetic pair retirement already recorded")
    record = dict(number=number, status="prepared", cases=prepared, retired=[])
    save(journal, record)  # A failed evidence write must preserve both cases.
    parent = os.open(destination, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        for mode in ("direct", "repository"):
            case = destination / prepared[mode]["case"]
            identity = os.stat(case.name, dir_fd=parent, follow_symlinks=False)
            require(stat.S_ISDIR(identity.st_mode) and
                    (identity.st_dev, identity.st_ino) == owners[case],
                    "synthetic case identity changed before retirement")
            if sys.version_info >= (3, 11):
                shutil.rmtree(case.name, dir_fd=parent)
            else:
                # Python 3.9/3.10 lack rmtree's parent-fd argument. Their
                # required fd-based safe implementation still protects the
                # freshly owned case tree; its identity was checked above.
                shutil.rmtree(case)
            record["retired"].append(mode)
            save(evidence / ("retired-pair-" + str(number) + "-" + mode + ".json"), record)
    finally:
        os.close(parent)
    after = resource.getrusage(resource.RUSAGE_SELF)
    record.update(status="retired", retirement_elapsed_seconds=time.monotonic() - started,
                  retirement_user_cpu_seconds=after.ru_utime - before.ru_utime,
                  retirement_system_cpu_seconds=after.ru_stime - before.ru_stime,
                  retirement_input_blocks=after.ru_inblock - before.ru_inblock,
                  retirement_output_blocks=after.ru_oublock - before.ru_oublock)
    save(evidence / ("retired-pair-" + str(number) + "-result.json"), record)
    return record


class Experiment:
    def __init__(self, build, repetitions, image_bytes=None, measurement_sampler=None,
                 retire_completed_pairs=False):
        self.build = build
        self.cache = check_build(build)
        require(5 <= repetitions <= 10, "use five to ten measured pairs")
        require(image_bytes is None or 1024**3 <= image_bytes <= 40_000_000_000,
                "image must be between 1 GiB and 40 decimal GB")
        require(image_bytes is None or image_bytes % 512 == 0, "image size must be whole 512-byte sectors")
        EVIDENCE.mkdir(exist_ok=True)
        require(not EVIDENCE.is_symlink(), "evidence must not be a symlink")
        self.root = Path(tempfile.mkdtemp(prefix="sc5-run-", dir=EVIDENCE))
        self.destination = self.root / "destination"
        self.destination.mkdir()
        self.repetitions, self.image_bytes = repetitions, image_bytes
        self.measurement_sampler = measurement_sampler
        self.retire_completed_pairs = retire_completed_pairs
        self.case_owners, self.retirements = {}, []
        self.mounted = False
        self.commands, self.rows = [], []
        self.node = build / "teleno_node"
        self.helper = build / "koinos_backup_direct_activation_test"
        if not self.node.exists():
            self.node = build / "src/teleno_node"
        if not self.helper.exists():
            self.helper = build / "src/koinos_backup_direct_activation_test"
        require(self.node.resolve().is_relative_to(build) and self.helper.resolve().is_relative_to(build),
                "native tools must belong to selected build")
        self.summary = dict(format="teleno-sc5-experiment", version=1, status="BLOCKED",
                            image_label_bytes=image_bytes,
                            capacity_40gb_qualified=False, whole_server_qualified=False,
                            historical_replay_validated=False, linux_qualified=False,
                            fixture_retention="retire-verified-pairs" if retire_completed_pairs else "retain")

    def command(self, name, argv, case=None, expected=0, observer_signal=None, timeout_seconds=120):
        require(0 < timeout_seconds <= 600, "invalid owned command timeout")
        log, error = self.root / (name + ".stdout"), self.root / (name + ".stderr")
        record = dict(argv=[str(a) for a in argv], state="running", stdout=str(log), stderr=str(error))
        self.commands.append(record)
        save(self.root / "commands.json", self.commands)
        samples = []
        started, signaled = time.monotonic(), False
        with log.open("wb") as out, error.open("wb") as err:
            child = subprocess.Popen([str(a) for a in argv], cwd=REPO, stdout=out, stderr=err,
                                     start_new_session=True)
            try:
                while True:
                    pid, status, usage = os.wait4(child.pid, os.WNOHANG)
                    if pid:
                        child.returncode = os.waitstatus_to_exitcode(status)
                        break
                    elapsed = time.monotonic() - started
                    require(elapsed < timeout_seconds, "owned command timeout")
                    require(log.stat().st_size + error.stat().st_size < 16 * 1024**2, "diagnostic log limit")
                    require(filesystem(self.root)["available_bytes"] >= HOST_RESERVE, "host reserve depleted")
                    if case:
                        sample = dict(seconds=elapsed, allocation=allocation(case),
                                      filesystem=filesystem(self.destination))
                        if self.measurement_sampler:
                            sample["resources"] = self.measurement_sampler(case, child.pid)
                        samples.append(sample)
                    if observer_signal and not signaled and "[node] teleno_node ready" in (log.read_text() + error.read_text()):
                        child.send_signal(observer_signal)
                        signaled = True
                    time.sleep(0.02)
            except BaseException as error:
                # Terminate only this newly owned process group, never a live node.
                try:
                    os.killpg(child.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(child.pid, signal.SIGKILL)
                    child.wait()
                record.update(state="aborted", exit_code=child.returncode, samples=samples,
                              elapsed_seconds=time.monotonic() - started, blocker=str(error))
                save(self.root / "commands.json", self.commands)
                raise
        if case:
            sample = dict(seconds=time.monotonic() - started, allocation=allocation(case),
                          filesystem=filesystem(self.destination))
            if self.measurement_sampler:
                sample["resources"] = self.measurement_sampler(case, None)
            samples.append(sample)
        record.update(state="finished", exit_code=child.returncode,
                      elapsed_seconds=time.monotonic() - started,
                      child_user_cpu_seconds=usage.ru_utime,
                      child_system_cpu_seconds=usage.ru_stime,
                      child_peak_rss_bytes=rss_bytes(usage.ru_maxrss), samples=samples,
                      stdout=str(log), stderr=str(error), observer_signaled=signaled)
        save(self.root / "commands.json", self.commands)
        expected_codes = expected if isinstance(expected, tuple) else (expected,)
        require(child.returncode in expected_codes, "unexpected exit in " + name + "; see retained logs")
        require(not observer_signal or signaled, "observer did not become ready")
        return log.read_text(), record

    def mount(self):
        if self.image_bytes is None:
            return
        require(platform.system() == "Darwin", "APFS image experiment requires macOS")
        require(filesystem(self.root)["available_bytes"] >= self.image_bytes + HOST_RESERVE,
                "host cannot back image's full capacity while preserving reserve")
        image = self.root / "capacity.sparseimage"
        # hdiutil's b suffix means blocks, not bytes. Use explicit sectors and
        # independently verify the mounted filesystem rather than trust a label.
        self.command("image-create", ["hdiutil", "create", "-sectors", str(self.image_bytes // 512),
                     "-type", "SPARSE", "-fs", "APFS", "-volname", "TelenoSC5", "-nospotlight", image])
        try:
            self.command("image-attach", ["hdiutil", "attach", image, "-mountpoint", self.destination,
                                         "-noautoopen", "-plist"])
        finally:
            self.mounted = os.path.ismount(self.destination)
        require(self.mounted and self.destination.stat().st_dev != self.root.stat().st_dev,
                "image did not mount as a separate filesystem")
        observed = filesystem(self.destination)
        self.summary["mounted_geometry"] = observed
        validate_capacity(observed, self.image_bytes)

    def restore(self, mode, number):
        case = self.destination / (mode + "-" + str(number))
        case.mkdir(mode=0o700)
        identity = case.stat()
        self.case_owners[case] = identity.st_dev, identity.st_ino
        target, attempt, config = case / "target", case / "attempt", case / "config.yml"
        config.write_text("features: {block_producer: false, p2p: false, jsonrpc: false, grpc: false, "
                          "mempool: false, account_history: false, contract_meta_store: false, transaction_store: false}\n"
                          "backup:\n  local:\n    enabled: true\n    directory: " + json.dumps(str(case / "repository")) +
                          "\n  public-restore:\n    enabled: true\n    network: testnet\n    require-https: false\n"
                          "    retries: 1\n    timeout-seconds: 2\n    signature-required: true\n"
                          "    signature-public-key-file: " + json.dumps(str(self.source / "fixture-signing.pub")) + "\n")
        base = [self.node, "--basedir", target, "--config", config, "--backup-json"]
        public = ["--backup-public-url", "file://" + str(self.source / "public"), "--backup-id", self.fixture["backup_id"]]
        prefix, before = mode + "-" + str(number), len(self.commands)
        if mode == "direct":
            args = base + public + ["--backup-restore-mode", "direct", "--backup-output", attempt,
                   "--backup-metadata-dir", case / "metadata", "--backup-expected-chain-id", self.fixture["chain_id"]]
            staged = json.loads(self.command(prefix + "-stage", args + ["--backup-restore-stage"], case)[0])
            require(staged["attempt_state"] == "download_complete", "stage incomplete")
            db = attempt / "payload/db"
        else:
            self.command(prefix + "-fetch", base + public + ["--backup-public-fetch"], case)
            self.command(prefix + "-stage", base + ["--backup-id", self.fixture["backup_id"],
                         "--backup-output", attempt, "--backup-restore-stage"], case)
            args, db = base + ["--backup-output", attempt], attempt / "db"
        require(hashes(db) == self.database_hashes, "stage differs from same snapshot")
        inode_before = db.stat().st_dev, db.stat().st_ino
        self.command(prefix + "-activate", args + ["--backup-restore-activate"], case)
        require(((target / "db").stat().st_dev, (target / "db").stat().st_ino) == inode_before,
                "activation copied database")
        require(hashes(target / "db") == self.database_hashes, "activated snapshot bytes differ")
        if mode == "direct":
            require(not (case / "repository").exists() and not (attempt / "payload/db").exists(), "second direct payload")
        text, _ = self.command(prefix + "-inspect", [self.helper, "--sc5-inspect", self.source,
                               target, case / "inspect-metadata"], case)
        verification, phases = json.loads(text), self.commands[before:]
        row = dict(mode=mode, number=number, warmup=number < 0, verification=verification,
                   elapsed_seconds=sum(p["elapsed_seconds"] for p in phases),
                   peak_child_rss_bytes=max(p["child_peak_rss_bytes"] for p in phases),
                   sampled_peak_allocated_bytes=max(s["allocation"]["allocated_bytes"] for p in phases for s in p["samples"]),
                   min_available_bytes=min(s["filesystem"]["available_bytes"] for p in phases for s in p["samples"]),
                   final_allocation=allocation(case), same_inode_activation=True)
        # Last pair: two clean restarts, and direct abrupt exit/recovery.
        # No blocks are processed; all network/optional services are disabled.
        if number == self.repetitions - 1:
            for cycle in range(2 + (2 if mode == "direct" else 0)):
                abrupt = mode == "direct" and cycle == 2
                self.command(prefix + "-observer-" + str(cycle), base[:5] + ["--disable", "block_producer",
                             "p2p", "jsonrpc", "grpc", "mempool", "account_history", "contract_meta_store", "transaction_store"],
                             case, expected=-signal.SIGKILL if abrupt else 0,
                             observer_signal=signal.SIGKILL if abrupt else signal.SIGINT)
                require((target / ".backup-observer-recovery").exists(), "observer hold missing")
                text, _ = self.command(prefix + "-reopen-" + str(cycle), [self.helper, "--sc5-inspect", self.source,
                                       target, case / "inspect-metadata"], case)
                require(json.loads(text) == verification, "checkpoint changed on reopen")
            row.update(observer_restarts=2, abrupt_observer_recovery=mode == "direct")
            observer_phases = self.commands[before + len(phases):]
            row["observer_peak_rss_bytes"] = max(p["child_peak_rss_bytes"] for p in observer_phases)
            row["observer_sampled_peak_allocated_bytes"] = max(
                s["allocation"]["allocated_bytes"] for p in observer_phases for s in p["samples"])
        self.rows.append(row)
        save(self.root / "runs.json", self.rows)
        return row

    def run(self):
        try:
            save(self.root / "environment.json", dict(system=platform.platform(), python=platform.python_version(),
                 recorded_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                 native_cache=self.cache, native_sha256=digest(self.node), helper_sha256=digest(self.helper),
                 host_filesystem=filesystem(self.root), cold_physical_cache=False))
            self.command("native-version", [self.node, "--version"])
            self.mount()
            self.source = self.root / "source"
            self.command("prepare", [self.helper, "--sc5-prepare", self.source])
            self.fixture = json.loads((self.source / "fixture.json").read_text())
            self.source_hashes = hashes(self.source)
            self.database_hashes = hashes(self.source / "checkpoint/db")
            save(self.root / "source-before-sha256.json", self.source_hashes)
            save(self.root / "database-input-sha256.json", self.database_hashes)
            self.summary.update(fixture=self.fixture, source_allocation=allocation(self.source),
                                destination_filesystem=filesystem(self.destination), image_label_bytes=self.image_bytes,
                                claim="synthetic data volume only; OS/runtime/build/source excluded")
            probe = self.destination / "preflight"
            probe.mkdir()
            config = probe / "config.yml"
            config.write_text("backup:\n  public-restore:\n    enabled: true\n    network: testnet\n"
                              "    require-https: false\n    signature-required: true\n"
                              "    signature-public-key-file: " + json.dumps(str(self.source / "fixture-signing.pub")) + "\n")
            cli = [self.node, "--basedir", probe / "target",
                    "--config", config, "--backup-restore-mode", "direct", "--backup-restore-preflight",
                    "--backup-metadata-dir", probe / "metadata", "--backup-public-url", "file://" + str(self.source / "public"),
                    "--backup-expected-chain-id", self.fixture["chain_id"], "--backup-json"]
            text, command = self.command("capacity-preflight", cli, probe, expected=(0, 2))
            self.summary["preflight"] = json.loads(text)
            if command["exit_code"] == 2:
                self.summary.update(status="CAPACITY_REJECTED", failure_phase="preflight",
                                    source_preserved=hashes(self.source) == self.source_hashes)
                return 2
            if self.summary["preflight"]["below_recommended"]:
                stage_cli = ["--backup-restore-stage" if a == "--backup-restore-preflight" else a for a in cli]
                stage, _ = self.command("capacity-stage-refused", stage_cli + ["--backup-output", probe / "attempt"],
                                        probe, expected=1)
                require(not (probe / "attempt").exists() and not (probe / "target").exists(), "reserve refusal wrote payload")
                self.summary.update(status="CAPACITY_REJECTED", failure_phase=json.loads(stage)["failure_phase"],
                                    source_preserved=hashes(self.source) == self.source_hashes)
                return 2
            for number in range(-1, self.repetitions):
                modes = ("repository", "direct") if number % 2 else ("direct", "repository")
                pair = {mode: self.restore(mode, number) for mode in modes}
                require(pair["direct"]["verification"] == pair["repository"]["verification"], "checkpoint parity mismatch")
                if self.retire_completed_pairs:
                    self.retirements.append(retire_owned_pair(
                        self.destination, number, pair, self.case_owners, self.root))
                    self.summary["fixture_retirements"] = self.retirements
            require(hashes(self.source) == self.source_hashes, "independent source changed")
            measured = [r for r in self.rows if not r["warmup"]]
            keys = ("elapsed_seconds", "peak_child_rss_bytes", "sampled_peak_allocated_bytes", "min_available_bytes")
            self.summary.update(status="SYNTHETIC_PASS", source_preserved=True, measurements={
                mode: {key: distribution([r[key] for r in measured if r["mode"] == mode]) for key in keys}
                for mode in ("direct", "repository")}, paired_peak_savings_bytes=distribution([
                    next(r["sampled_peak_allocated_bytes"] for r in measured if r["mode"] == "repository" and r["number"] == n) -
                    next(r["sampled_peak_allocated_bytes"] for r in measured if r["mode"] == "direct" and r["number"] == n)
                    for n in range(self.repetitions)]), gaps=["representative snapshot/successor range unavailable",
                    "no 24-hour workload, catch-up, WAL rotation or compaction qualification",
                    "no Linux/ext4 or 2-vCPU/4-GB/40-GB whole-server test", "SC4/audit gates still blocked"])
            return 0
        except Exception as error:
            self.summary.update(status="BLOCKED", blocker=str(error))
            return 2
        finally:
            if self.mounted:
                try:
                    self.command("image-detach", ["hdiutil", "detach", self.destination])
                    self.summary["image_detached"] = True
                except Exception:
                    self.summary.update(status="BLOCKED", image_detached=False,
                                        detach_blocker="owned image remains mounted; inspect logs")
            self.summary["driver_peak_rss_bytes"] = rss_bytes(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
            save(self.root / "summary.json", self.summary)
            print("SC5 evidence:", self.root)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=REPO / "build")
    parser.add_argument("--repetitions", type=int, default=5)
    parser.add_argument("--apfs-capacity-bytes", type=int)
    parser.add_argument("--retire-completed-pairs", action="store_true",
                        help="retire this run's verified synthetic case trees after each pair; "
                             "retain source, logs, measurements and file hashes")
    args = parser.parse_args()
    experiment = Experiment(args.build_dir.resolve(), args.repetitions, args.apfs_capacity_bytes,
                            retire_completed_pairs=args.retire_completed_pairs)
    code = experiment.run()
    return 2 if experiment.summary["status"] == "BLOCKED" else code


if __name__ == "__main__":
    raise SystemExit(main())
