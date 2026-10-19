"""LR0 collector tests: no mounts, network, existing nodes or production settings."""
import json
import os
from pathlib import Path
import sys
import subprocess
import tempfile
import types
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/backup"))
import measure_low_resource_baseline as lr0


class BaselineTest(unittest.TestCase):
    def scratch(self):
        lr0.sc5.EVIDENCE.mkdir(exist_ok=True)
        self.assertFalse(lr0.sc5.EVIDENCE.is_symlink())
        return tempfile.TemporaryDirectory(prefix="lr0-unit-", dir=lr0.sc5.EVIDENCE)

    def test_contract_preserves_full_archive(self):
        self.assertEqual(lr0.CONTRACT["history"], "complete_from_genesis")
        for field in ("pruning_allowed", "receipt_omission_allowed", "light_fallback_allowed"):
            self.assertFalse(lr0.CONTRACT[field])
        self.assertEqual(lr0.CONTRACT["whole_server"]["disk_bytes"], 40_000_000_000)

    def test_unknown_is_not_zero(self):
        self.assertEqual(lr0.metric()["status"], "unavailable")
        self.assertIsNone(lr0.metric()["value"])
        self.assertEqual(lr0.metric(0)["status"], "observed")
        self.assertEqual(lr0.metric(0)["value"], 0)

    def test_dependency_tarball_does_not_inherit_parent_revision(self):
        path = lr0.sc5.REPO / ".deps/archive-source"
        with patch.object(lr0, "inspect", return_value={"exit_code": 0, "output": str(lr0.sc5.REPO)}) as read:
            result = lr0.dependency_identity(path)
            self.assertIsNone(result["revision"])
            self.assertEqual(result["status"], "unavailable")
            self.assertEqual(read.call_count, 1)
        with patch.object(lr0, "inspect", side_effect=[
                {"exit_code": 0, "output": str(path)},
                {"exit_code": 0, "output": "a" * 40},
                {"exit_code": 0, "output": " M patched.cpp\n"}]):
            result = lr0.dependency_identity(path)
            self.assertEqual(result["status"], "observed")
            self.assertIn("patched.cpp", result["worktree"]["output"])

    def test_counter_units_and_missing_fields(self):
        values = lr0.parse_counters("MemTotal: 4096 kB\nMemAvailable: 1 kB\nbad: x\n", True)
        self.assertEqual(values, {"MemTotal": 4096 * 1024, "MemAvailable": 1024})
        self.assertIsNone(lr0.counter_metric(values, "SwapTotal", "fixture")["value"])

    def test_linux_memory_and_process_samples(self):
        with self.scratch() as d:
            root = Path(d)
            (root / "meminfo").write_text("MemTotal: 8 kB\nMemAvailable: 4 kB\nSwapTotal: 0 kB\n")
            (root / "123").mkdir()
            (root / "123/status").write_text("VmRSS: 3 kB\nVmHWM: 5 kB\n")
            (root / "123/io").write_text("write_bytes: 0\nread_bytes: 4096\n")
            self.assertEqual(lr0.linux_memory(root)["available_bytes"]["value"], 4096)
            process = lr0.process_metrics(123, root, "Linux")
            self.assertEqual(process["rss_bytes"]["value"], 3072)
            self.assertEqual(process["rss_high_water_bytes"]["value"], 5120)
            self.assertEqual(process["io"]["write_bytes"]["value"], 0)
            self.assertIsNone(process["io"]["wchar"]["value"])
            self.assertIsNone(lr0.process_metrics(None, root, "Linux")["rss_bytes"]["value"])
            self.assertIsNone(lr0.process_metrics(123, root, "Darwin")["rss_bytes"]["value"])

    def test_cgroup_unlimited_and_shared_scope(self):
        with self.scratch() as d:
            root = Path(d)
            (root / "proc/123").mkdir(parents=True)
            (root / "proc/123/cgroup").write_text("0::/owned\n")
            group = root / "cgroup/owned"
            group.mkdir(parents=True)
            (group / "memory.current").write_text("400\n")
            (group / "memory.max").write_text("max\n")
            (group / "memory.events").write_text("oom_kill 0\n")
            result = lr0.cgroup_v2(123, root / "proc", root / "cgroup")
            self.assertEqual(result["current_bytes"]["value"], 400)
            self.assertIsNone(result["peak_bytes"]["value"])
            self.assertEqual(result["limit_bytes"]["reason"], "unlimited")
            self.assertEqual(result["oom_kill_count"]["value"], 0)
            self.assertIn("shared", result["scope"])
            (root / "proc/123/cgroup").write_text("0::/../outside\n")
            self.assertIsNone(lr0.cgroup_v2(123, root / "proc", root / "cgroup")["current_bytes"]["value"])

    def test_storage_groups_and_hardlinks(self):
        with self.scratch() as d:
            root = Path(d)
            for name in ("0001.sst", "0002.log", "MANIFEST-1", "LOG", "out.stdout", "config.yml"):
                (root / name).write_bytes(b"x" * 4096)
            os.link(root / "0001.sst", root / "0003.sst")
            (root / "objects/sha256").mkdir(parents=True)
            (root / "objects/sha256/a").write_bytes(b"y")
            groups = lr0.storage_groups(root)
            self.assertEqual(groups["sst"]["files"], 2)
            self.assertEqual(groups["sst"]["logical_bytes"], 8192)
            self.assertEqual(groups["sst"]["allocated_bytes"], (root / "0001.sst").stat().st_blocks * 512)
            self.assertEqual(groups["wal"]["files"], 1)
            self.assertEqual(groups["logs"]["files"], 2)
            self.assertEqual(groups["db_metadata"]["files"], 1)
            self.assertEqual(groups["objects"]["files"], 1)

    def test_symlink_sampling_refused(self):
        with self.scratch() as d:
            root = Path(d)
            (root / "link").symlink_to("absent")
            with self.assertRaisesRegex(ValueError, "symlink"):
                lr0.storage_groups(root)

    def metadata(self, root, path="db/0001.sst", size=1, chain=""):
        entry = {"path": path, "size_bytes": size, "sha256": "a" * 64}
        (root / "files.json").write_text(json.dumps({"backup_id": "fixture", "files": [entry]}))
        (root / "manifest.json").write_text(json.dumps({
            "backup_id": "fixture", "source": {"chain_id": chain},
            "public_bootstrap": {"chain_id": chain, "network": "mainnet"},
            "snapshot": {"object_count": 1, "total_bytes": size}}))

    def test_metadata_pinned_not_authenticated(self):
        with self.scratch() as d:
            root = Path(d)
            self.metadata(root)
            review = lr0.metadata_review(root)
            self.assertFalse(review["authenticated"])
            self.assertFalse(review["full_history_verified"])
            self.assertIsNone(review["native_eligible"])
            self.assertEqual(review["declared_database_bytes"], 1)
            self.assertEqual(review["manifest_sha256"], lr0.sc5.digest(root / "manifest.json"))
            self.assertTrue(any("missing declared" in b for b in review["blockers"]))
            self.metadata(root, chain="known")
            self.assertFalse(lr0.metadata_review(root)["authenticated"])

    def test_metadata_limits_paths_sizes_and_overflow(self):
        with self.scratch() as d:
            root = Path(d)
            for path, size in (("../db/a", 1), ("/db/a", 1), ("db//a", 1),
                               ("db/a", -1), ("db/a", True), ("db/a", 2**64)):
                self.metadata(root, path, size)
                with self.assertRaises(ValueError):
                    lr0.metadata_review(root)
            (root / "huge.json").write_bytes(b"x" * (lr0.MAX_METADATA_BYTES + 1))
            with self.assertRaisesRegex(ValueError, "size limit"):
                lr0.bounded_json(root / "huge.json")
            (root / "link.json").symlink_to(root / "huge.json")
            with self.assertRaises(ValueError):
                lr0.bounded_json(root / "link.json")

    def test_collector_does_not_certify_target_or_failed_tests(self):
        experiment = types.SimpleNamespace(summary={"status": "SYNTHETIC_PASS"}, commands=[])
        report = lr0.baseline_report(experiment, {}, None, [{"exit_code": 8}])
        self.assertEqual(report["status"], "LOCAL_BASELINE_RECORDED")
        self.assertFalse(report["all_baseline_checks_passed"])
        for field in ("full_history_verified", "linux_qualified", "whole_server_40gb_qualified", "reserve_calibrated"):
            self.assertFalse(report[field])
        self.assertIsNone(report["fixture_resource_samples"]["direct"]["sampled_storage_peaks"]["wal"]["files"])
        experiment.summary = {"status": "BLOCKED"}
        self.assertEqual(lr0.baseline_report(experiment, {}, None, [])["status"], "BLOCKED")

    def test_warmup_samples_excluded(self):
        resources = {"storage": {name: {"files": 1, "logical_bytes": 10, "allocated_bytes": 10}
                     for name in ("sst", "wal", "db_metadata", "logs", "objects", "other")},
                     "host_memory": {"available_bytes": lr0.metric(0)}}
        commands = [{"stdout": "direct--1-stage.stdout", "samples": [{"resources": resources}]},
                    {"stdout": "direct-0-stage.stdout", "samples": [{"resources": resources}]}]
        aggregated = lr0.aggregate_resources(commands)
        self.assertEqual(aggregated["direct"]["samples"], 1)
        self.assertEqual(aggregated["direct"]["min_host_available_bytes"]["value"], 0)
        self.assertIsNone(aggregated["repository"]["min_host_available_bytes"]["value"])

    def experiment(self, root, sampler=None):
        experiment = lr0.sc5.Experiment.__new__(lr0.sc5.Experiment)
        experiment.root = experiment.destination = root
        experiment.commands = []
        experiment.measurement_sampler = sampler
        return experiment

    def test_sampler_hook_and_child_rss_are_recorded(self):
        with self.scratch() as d:
            root = Path(d)
            sampler = lambda case, pid: {"owned_case": str(case), "pid": pid}
            experiment = self.experiment(root, sampler)
            _, record = experiment.command("sample", [sys.executable, "-c", "print('fixture')"], root)
            self.assertEqual(record["state"], "finished")
            self.assertGreater(record["child_peak_rss_bytes"], 0)
            self.assertEqual(record["samples"][-1]["resources"]["owned_case"], str(root))
            self.assertIsNone(record["samples"][-1]["resources"]["pid"])
            self.assertGreaterEqual(record["child_user_cpu_seconds"], 0)

    def test_timeout_records_abort_and_stops_owned_child_group(self):
        with self.scratch() as d:
            root = Path(d)
            experiment = self.experiment(root)
            command = ("import subprocess,sys,time; "
                       "p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(30)']); "
                       "print(p.pid,flush=True); time.sleep(30)")
            with self.assertRaisesRegex(ValueError, "timeout"):
                experiment.command("timeout", [sys.executable, "-c", command], timeout_seconds=0.5)
            record = json.loads((root / "commands.json").read_text())[0]
            self.assertEqual(record["state"], "aborted")
            child = int((root / "timeout.stdout").read_text().strip())
            state = subprocess.run(["ps", "-p", str(child), "-o", "stat="], text=True,
                                   stdout=subprocess.PIPE, timeout=5).stdout.strip()
            self.assertTrue(not state or state.startswith("Z"), "owned descendant still running")


if __name__ == "__main__":
    unittest.main()
