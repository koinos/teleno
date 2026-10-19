"""Exercise the actual CLI with disposable data; never start a node."""
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())


class PreflightTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="teleno-preflight-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.base = self.root / "absent" / "node"
        self.config = self.root / "config.yml"
        self.config.write_text("features:\n  p2p: false\n  jsonrpc: false\n")

    def snapshot(self):
        return {str(p.relative_to(self.root)): (p.lstat().st_mode, p.lstat().st_mtime_ns,
                    hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else None)
                for p in self.root.rglob("*")}

    def run_check(self, *args, expected=0):
        before = self.snapshot()
        result = subprocess.run([BINARY, "--basedir", str(self.base), "--config", str(self.config),
                                 "--preflight", "--preflight-json", *args],
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["exit_code"], expected)
        self.assertFalse(report["runtime_readiness_verified"])
        self.assertEqual(before, self.snapshot(), "Preflight changed the filesystem")
        return {c["id"]: c for c in report["checks"]}

    def test_absent_basedir_is_not_created(self):
        checks = self.run_check()
        self.assertEqual(checks["config"]["status"], "pass")
        self.assertFalse(self.base.exists())

    def test_wal_policy_is_reported_without_opening_storage(self):
        for value in [0, 67108864]:
            with self.subTest(value=value):
                self.config.write_text("features:\n  p2p: false\n  jsonrpc: false\n"
                                       f"rocksdb:\n  max-total-wal-size: {value}\n")
                checks = self.run_check()
                self.assertEqual(checks["storage.wal"]["status"], "not_checked")
                self.assertIn(str(value) if value else "Automatic", checks["storage.wal"]["observed"])
                self.assertFalse(self.base.exists())

    def test_invalid_wal_policy_is_side_effect_free(self):
        for value in ["-1", "-0", "true", "[]", "null", "1.5", "18446744073709551616"]:
            with self.subTest(value=value):
                self.config.write_text(f"rocksdb:\n  max-total-wal-size: {value}\n")
                checks = self.run_check(expected=1)
                self.assertEqual(checks["config"]["status"], "fail")
                self.assertFalse(self.base.exists())

    def test_blocks_zstd_profile_requires_runtime_codec_check(self):
        for options in ["  blocks-zstd-level: 9\n",
                        "  blocks-zstd-dictionary-bytes: 65536\n",
                        "  blocks-zstd-level: 15\n  blocks-zstd-dictionary-bytes: 131072\n"]:
            with self.subTest(options=options):
                self.config.write_text("features:\n  p2p: false\n  jsonrpc: false\n"
                                       "rocksdb:\n  blocks-compression: zstd\n" + options)
                checks = self.run_check()
                self.assertEqual(checks["config"]["status"], "pass")
                self.assertEqual(checks["storage.compression"]["status"], "not_checked")
                self.assertFalse(self.base.exists())

    def test_invalid_blocks_zstd_profile_is_side_effect_free(self):
        for options in ["  blocks-zstd-level: 20\n",
                        "  blocks-zstd-dictionary-bytes: 65537\n",
                        "  blocks-zstd-dictionary-bytes: 131073\n",
                        "  blocks-zstd-dictionary-bytes: 262144\n",
                        "  blocks-zstd-level: -1\n",
                        "  blocks-zstd-dictionary-bytes: true\n",
                        "  blocks-compression: none\n  blocks-zstd-level: 9\n"]:
            with self.subTest(options=options):
                self.config.write_text("rocksdb:\n" + options)
                checks = self.run_check(expected=1)
                self.assertEqual(checks["config"]["status"], "fail")
                self.assertFalse(self.base.exists())

    def test_existing_database_and_recovery_markers_unchanged(self):
        (self.base / "db").mkdir(parents=True)
        for name in ["db/CURRENT", "db/LOCK", ".backup-observer-recovery"]:
            (self.base / name).write_text("sentinel: not a database")
        self.run_check()

    def test_missing_config(self):
        self.config.unlink()
        self.run_check(expected=1)

    def test_invalid_configs_and_no_secret_output(self):
        for content in ["secret-value: [", "chain:\n  jobs: secret-value\n", "chain:\n  jobs: -1\n",
                        "chain: false\n", "features:\n  invented: true\n", "features:\n  p2p: secret-value\n",
                        "backup:\n  admin: secret-value\n", "p2p:\n  peer: {secret-value: true}\n"]:
            with self.subTest(content=content):
                self.config.write_text(content)
                checks = self.run_check(expected=1)
                self.assertNotIn("secret-value", json.dumps(checks))

    def test_busy_port_and_cli_override(self):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            endpoint = f"127.0.0.1:{listener.getsockname()[1]}"
            checks = self.run_check("--enable", "jsonrpc", "--jsonrpc-listen", endpoint, expected=1)
            self.assertEqual(checks["ports.jsonrpc"]["status"], "fail")
            self.run_check("--jsonrpc-listen", endpoint)

    def test_dynamic_port_and_text(self):
        checks = self.run_check("--enable", "jsonrpc", "--jsonrpc-listen", "127.0.0.1:0")
        self.assertEqual(checks["ports.jsonrpc"]["status"], "pass")
        result = subprocess.run([BINARY, "--preflight", "--config", str(self.config),
                                 "--basedir", str(self.base)], capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0)
        self.assertIn("runtime readiness is NOT verified", result.stdout)

    def test_maintenance_modes_rejected(self):
        for option in ["--compact-db", "--compact-rewrite", "--backup-restore", "--backup-create", "--migrate-chain-db"]:
            self.run_check(option, expected=2)

    def test_compact_rewrite_arguments_do_not_create_a_database(self):
        for options in [["--compact-rewrite"],
                        ["--compact-rewrite", "--compact-db"],
                        ["--compact-rewrite", "--compact-db", "--all", "--compact-cf", "blocks"],
                        ["--compact-rewrite", "--compact-db", "--compact-cf", "invented"],
                        ["--compact-rewrite", "--compact-db", "--all", "--backup-create-local"],
                        ["--compact-rewrite", "--compact-db", "--all"]]:
            with self.subTest(options=options):
                before = self.snapshot()
                result = subprocess.run([BINARY, "--basedir", str(self.base), "--config", str(self.config),
                                         *options], capture_output=True, text=True, timeout=15)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(before, self.snapshot())

    def test_json_requires_preflight(self):
        result = subprocess.run([BINARY, "--preflight-json", "--basedir", str(self.base)],
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 2)
        json.loads(result.stdout)
        self.assertFalse(self.base.exists())

    def test_producer_never_creates_key(self):
        checks = self.run_check("--enable", "block_producer", expected=1)
        self.assertEqual(checks["producer.key"]["status"], "fail")
        self.assertFalse(self.base.exists())

    def test_reset_and_unknown_cli_feature(self):
        self.config.write_text("global:\n  reset: true\nfeatures:\n  p2p: false\n  jsonrpc: false\n")
        self.run_check(expected=1)
        self.run_check("--enable", "invented", expected=1)

    def test_configured_port_collision(self):
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        self.config.write_text(f"features:\n  p2p: true\n  jsonrpc: true\np2p:\n  listen: /ip4/127.0.0.1/tcp/{port}\njsonrpc:\n  listen: 127.0.0.1:{port}\n")
        checks = self.run_check(expected=1)
        self.assertEqual(checks["ports.jsonrpc"]["status"], "fail")
        with socket.socket() as released:
            released.bind(("127.0.0.1", port))

    def test_bad_storage_path_and_broken_symlink(self):
        self.base.parent.mkdir()
        self.base.write_text("not a directory")
        self.run_check(expected=1)
        self.base.unlink()
        self.base.symlink_to(self.root / "missing-target")
        self.run_check(expected=1)

    def test_numeric_port_range_and_p2p_family(self):
        self.run_check("--enable", "jsonrpc", "--jsonrpc-listen", "127.0.0.1:65536", expected=1)
        self.run_check("--enable", "p2p", "--p2p-listen", "/ip4/::1/tcp/0", expected=1)


if __name__ == "__main__":
    unittest.main()
