"""Pinned protobuf ownership-patch integrity, without network or live data."""
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
PATCH = REPO / "cmake/patches/protobuf-unknown-field-ownership.patch"
SCRIPT = REPO / "cmake/ApplyProtobufOwnershipPatch.cmake"
BEFORE = {
    "unknown_field_set.cc": "bd1d8452a66fea9f415e17b971a8c7c4ee1ba3d6c6d939f56abb46e07af0910e",
    "arenastring.cc": "4593e7d1d80677053f3bf79f2d041f56dc0fc4ee810600c69c5b5fedfef6da89",
}
AFTER = {
    "unknown_field_set.cc": "090f66ae063d27fe1ee03b5c34b6be903cf75c69ffa008336cc1ec32c8801d8a",
    "arenastring.cc": "08a52ed990b3707b65f39f342a8f0dfddfef25d9e2fe89b1af5b7be8c3faddc2",
}


class Recipe(unittest.TestCase):
    def setUp(self):
        root = os.environ.get("TELENO_TEST_ARTIFACT_ROOT") or os.environ.get("TMPDIR")
        self.assertTrue(root, "An explicit scratch root is required")
        self.tmp = tempfile.TemporaryDirectory(prefix="protobuf-recipe-", dir=root)
        self.addCleanup(self.tmp.cleanup)
        self.source = Path(self.tmp.name) / "source"
        self.files = {name: self.source / "src/google/protobuf" / name for name in AFTER}
        for name, path in self.files.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(Path(SOURCE) / "src/google/protobuf" / name, path)
        self.assertEqual(self.digest(), AFTER)

    def digest(self):
        return {name: hashlib.sha256(path.read_bytes()).hexdigest()
                for name, path in self.files.items()}

    def run_patch(self, patch=PATCH):
        return subprocess.run([CMAKE, f"-DPROTOBUF_SOURCE_DIR={self.source}",
                               f"-DPROTOBUF_PATCH={patch}", "-P", str(SCRIPT)],
                              capture_output=True, text=True, timeout=30)

    def unpatch(self):
        subprocess.run(["git", "apply", "--reverse", str(PATCH)], cwd=self.source,
                       check=True, capture_output=True, timeout=30)
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
        original = {name: path.read_bytes() for name, path in self.files.items()}
        for name in self.files:
            with self.subTest(name=name):
                for other, path in self.files.items():
                    path.write_bytes(original[other])
                self.files[name].write_bytes(b"unexpected source\n")
                before = self.digest()
                result = self.run_patch()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Unexpected protobuf", result.stderr)
                self.assertEqual(self.digest(), before)

    def test_mixed_patch_state_is_not_modified(self):
        original = {name: path.read_bytes() for name, path in self.files.items()}
        self.unpatch()
        pristine = {name: path.read_bytes() for name, path in self.files.items()}
        for name in self.files:
            with self.subTest(name=name):
                for other, path in self.files.items():
                    path.write_bytes(pristine[other])
                self.files[name].write_bytes(original[name])
                before = self.digest()
                result = self.run_patch()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("mixed patch state", result.stderr)
                self.assertEqual(self.digest(), before)

    def test_missing_patch_preserves_preimage(self):
        self.unpatch()
        result = self.run_patch(Path(self.tmp.name) / "absent.patch")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.digest(), BEFORE)

    def test_invalid_patch_preserves_both_preimages(self):
        self.unpatch()
        invalid = Path(self.tmp.name) / "invalid.patch"
        invalid.write_text(PATCH.read_text().replace(
            " #include <google/protobuf/unknown_field_set.h>",
            " #include <google/protobuf/missing_preimage.h>"))
        result = self.run_patch(invalid)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.digest(), BEFORE)


if __name__ == "__main__":
    unittest.main()
