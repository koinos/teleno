"""SC5 measurement/ownership helpers; no mounts, network or native processes."""
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import sys

sys.dont_write_bytecode = True

REPO = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("sc5", REPO / "tools/backup/validate_single_copy_restore.py")
sc5 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sc5)


class MeasurementTest(unittest.TestCase):
    def test_distribution(self):
        self.assertEqual(sc5.distribution([1, 2, 3, 4, 100]),
                         dict(count=5, median=3, min=1, max=100, mad=1))
        with self.assertRaises(ValueError):
            sc5.distribution([])

    def test_native_rss_units(self):
        self.assertEqual(sc5.rss_bytes(100, "Darwin"), 100)
        self.assertEqual(sc5.rss_bytes(100, "Linux"), 102400)

    def test_measured_capacity_not_label(self):
        sc5.validate_capacity(dict(total_bytes=999, available_bytes=900), 1000)
        for observed in (dict(total_bytes=1001, available_bytes=900),
                         dict(total_bytes=999, available_bytes=1000),
                         dict(total_bytes=0, available_bytes=0)):
            with self.assertRaises(ValueError):
                sc5.validate_capacity(observed, 1000)

    def test_inode_allocation(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            file = root / "a"
            file.write_bytes(b"x" * 8192)
            os.link(file, root / "b")
            result = sc5.allocation(root)
            self.assertEqual(result["logical_bytes"], 16384)
            self.assertEqual(result["files"], 2)
            self.assertEqual(result["allocated_bytes"], file.stat().st_blocks * 512)

    def test_symlink_refused(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root / "link").symlink_to("absent")
            with self.assertRaises(ValueError):
                sc5.allocation(root)
            with self.assertRaises(ValueError):
                sc5.allocation(root / "link")

    def test_missing_tree(self):
        self.assertEqual(sc5.allocation(Path("/nonexistent-sc5-fixture")),
                         dict(logical_bytes=0, allocated_bytes=0, files=0))

    def test_foreign_cache_and_disabled_tests(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            cache = root / "CMakeCache.txt"
            cache.write_text("CMAKE_HOME_DIRECTORY:INTERNAL=/wrong\nKOINOS_BUILD_TESTS:BOOL=ON\n")
            with self.assertRaisesRegex(ValueError, "foreign"):
                sc5.check_build(root)
            cache.write_text("CMAKE_HOME_DIRECTORY:INTERNAL=" + str(REPO) + "\nKOINOS_BUILD_TESTS:BOOL=OFF\n")
            with self.assertRaisesRegex(ValueError, "test-enabled"):
                sc5.check_build(root)


class PairRetirementTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.destination = self.root / "destination"
        self.destination.mkdir()
        self.source = self.root / "source"
        self.source.mkdir()
        (self.source / "original").write_bytes(b"independent fixture")
        self.owners, self.pair = {}, {}
        for mode in ("direct", "repository"):
            case = self.destination / (mode + "-0")
            case.mkdir()
            (case / "target").mkdir()
            (case / "target" / "fixture").write_bytes(mode.encode())
            identity = case.stat()
            self.owners[case] = identity.st_dev, identity.st_ino
            self.pair[mode] = dict(mode=mode, number=0, verification=dict(root="same"))

    def retire(self):
        return sc5.retire_owned_pair(self.destination, 0, self.pair, self.owners, self.root)

    def assert_cases_preserved(self):
        for case in self.owners:
            self.assertTrue((case / "target" / "fixture").is_file())

    def test_verified_pair_retirement_preserves_source_and_other_cases(self):
        other = self.destination / "direct-99"
        other.mkdir()
        record = self.retire()
        self.assertEqual(record["status"], "retired")
        self.assertEqual(record["retired"], ["direct", "repository"])
        for mode in self.pair:
            self.assertFalse((self.destination / (mode + "-0")).exists())
            self.assertEqual(record["cases"][mode]["files"]["target/fixture"],
                             sc5.hashlib.sha256(mode.encode()).hexdigest())
        self.assertEqual((self.source / "original").read_bytes(), b"independent fixture")
        self.assertTrue(other.is_dir())
        self.assertTrue((self.root / "retired-pair-0.json").is_file())

    def test_failed_pair_preserves_both_cases(self):
        self.pair["repository"]["verification"] = dict(root="different")
        with self.assertRaisesRegex(ValueError, "unverified"):
            self.retire()
        self.assert_cases_preserved()

    def test_python39_compatible_retirement(self):
        with mock.patch.object(sc5.sys, "version_info", (3, 9, 0)):
            record = self.retire()
        self.assertEqual(record["status"], "retired")
        self.assert_cases_absent()

    def assert_cases_absent(self):
        for case in self.owners:
            self.assertFalse(case.exists())

    def test_wrong_case_result_preserves_both_cases(self):
        self.pair["repository"]["number"] = 1
        with self.assertRaisesRegex(ValueError, "foreign synthetic pair"):
            self.retire()
        self.assert_cases_preserved()

    def test_replaced_case_identity_is_refused(self):
        case = self.destination / "repository-0"
        saved = self.destination / "saved-repository"
        case.rename(saved)
        case.mkdir()
        with self.assertRaisesRegex(ValueError, "identity changed"):
            self.retire()
        self.assertTrue((self.destination / "direct-0/target/fixture").is_file())
        self.assertTrue((saved / "target/fixture").is_file())
        self.assertTrue(case.is_dir())

    def test_symlink_in_either_case_preserves_all_data(self):
        (self.destination / "repository-0/link").symlink_to(self.source)
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.retire()
        self.assert_cases_preserved()
        self.assertEqual((self.source / "original").read_bytes(), b"independent fixture")

    def test_evidence_write_failure_preserves_both_cases(self):
        with mock.patch.object(sc5, "save", side_effect=OSError("injected evidence failure")):
            with self.assertRaisesRegex(OSError, "injected evidence"):
                self.retire()
        self.assert_cases_preserved()

    def test_foreign_destination_is_refused(self):
        with self.assertRaisesRegex(ValueError, "foreign synthetic destination"):
            sc5.retire_owned_pair(self.destination, 0, self.pair, self.owners, self.source)
        self.assert_cases_preserved()

    def test_existing_retirement_evidence_is_not_overwritten(self):
        journal = self.root / "retired-pair-0.json"
        journal.write_text("previous evidence")
        with self.assertRaisesRegex(ValueError, "already recorded"):
            self.retire()
        self.assert_cases_preserved()
        self.assertEqual(journal.read_text(), "previous evidence")


if __name__ == "__main__":
    unittest.main()
