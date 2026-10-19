"""Pinned build-owned Fizzy patch checks, with no network or live data access."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
CMAKE, SOURCE = sys.argv[1:3]
del sys.argv[1:3]
PATCH = REPO / "cmake/patches/fizzy-context-allocation.patch"
SCRIPT = REPO / "cmake/ApplyFizzyContextPatch.cmake"
BEFORE = "e4fe84df817ad13dda075138ccf0dd97868c307ed5815a81868a73b74a737774"
AFTER = "ff5034e6de9dd84d1ffbf961ee19f8063dc39ed811ecbba3651f37e5e20c8e8b"


class Recipe(unittest.TestCase):
    def setUp(self):
        root = os.environ.get("TELENO_TEST_ARTIFACT_ROOT") or os.environ.get("TMPDIR")
        self.assertTrue(root, "An explicit scratch root is required")
        self.tmp = tempfile.TemporaryDirectory(prefix="fizzy-recipe-", dir=root)
        self.addCleanup(self.tmp.cleanup)
        self.source = Path(self.tmp.name) / "source"
        self.file = self.source / "lib/fizzy/capi.cpp"
        self.file.parent.mkdir(parents=True)
        shutil.copyfile(Path(SOURCE) / "lib/fizzy/capi.cpp", self.file)
        self.assertEqual(self.digest(), AFTER)

    def digest(self):
        return hashlib.sha256(self.file.read_bytes()).hexdigest()

    def run_patch(self, patch=PATCH):
        return subprocess.run([CMAKE, f"-DFIZZY_SOURCE_DIR={self.source}",
                               f"-DFIZZY_PATCH={patch}", "-P", str(SCRIPT)],
                              capture_output=True, text=True, timeout=30)

    def unpatch(self):
        subprocess.run(["git", "apply", "--reverse", str(PATCH)],
                       cwd=self.source, check=True, capture_output=True, timeout=30)
        self.assertEqual(self.digest(), BEFORE)

    def test_pinned_preimage_and_idempotence(self):
        self.unpatch()
        result = self.run_patch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.digest(), AFTER)
        result = self.run_patch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.digest(), AFTER)

    def test_unexpected_input_is_not_modified(self):
        self.file.write_bytes(b"unexpected source\n")
        before = self.file.read_bytes()
        result = self.run_patch()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unexpected Fizzy", result.stderr)
        self.assertEqual(self.file.read_bytes(), before)

    def test_missing_patch_preserves_preimage(self):
        self.unpatch()
        result = self.run_patch(Path(self.tmp.name) / "absent.patch")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.digest(), BEFORE)

    def test_all_native_entrypoints_use_corrected_recipe(self):
        for name in ("CMakeLists.txt", "CMakeLists.hunter.txt", "CMakeLists.standalone.txt"):
            text = (REPO / name).read_text()
            self.assertIn("include(cmake/TelenoFizzy.cmake)", text)
            self.assertNotIn("find_package(fizzy", text)
            self.assertNotIn("koinos_add_package(fizzy", text)


if __name__ == "__main__":
    unittest.main()
