"""SC5 measurement/ownership helpers; no mounts, network or native processes."""
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
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


if __name__ == "__main__":
    unittest.main()
