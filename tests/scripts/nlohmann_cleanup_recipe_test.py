"""Exercise real pinned header staging without touching a dependency installation."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
CMAKE, PATCHED_INCLUDE = sys.argv[1:3]
del sys.argv[1:3]
SCRIPT = REPO / "cmake/ApplyNlohmannCleanup.py"
PATCH = REPO / "cmake/patches/nlohmann-json-allocation-free-destroy.patch"
MANIFEST = REPO / "cmake/patches/nlohmann-json-cleanup-manifest.json"
EXPECTED = json.loads(MANIFEST.read_text())


def files(root):
    return {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(root.rglob("*")) if path.is_file()}


def git_environment():
    return {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}


class Recipe(unittest.TestCase):
    def setUp(self):
        root = os.environ.get("TELENO_TEST_ARTIFACT_ROOT") or os.environ.get("TMPDIR")
        self.assertTrue(root, "An explicit owned scratch root is required")
        self.tmp = tempfile.TemporaryDirectory(prefix="json-recipe-", dir=root)
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.input = self.root / "installed/include"
        shutil.copytree(Path(PATCHED_INCLUDE) / "nlohmann", self.input / "nlohmann")
        after = dict(EXPECTED["original_headers"])
        after.update(EXPECTED["patched_headers"])
        self.assertEqual(files(self.input / "nlohmann"), after)
        subprocess.run(["git", "apply", "--reverse", str(PATCH)],
                       cwd=self.input.parent, env=git_environment(), check=True, capture_output=True)
        self.assertEqual(files(self.input / "nlohmann"), EXPECTED["original_headers"])
        self.output = self.root / "build-owned"
        self.before = files(self.input)

    def stage(self, patch=PATCH, environment=None):
        return subprocess.run([sys.executable, "-B", str(SCRIPT),
                               "--installed-include", str(self.input),
                               "--output-root", str(self.output),
                               "--manifest", str(MANIFEST), "--patch", str(patch)],
                              env=environment, capture_output=True, text=True)

    def verify_output(self):
        expected = dict(EXPECTED["original_headers"])
        expected.update(EXPECTED["patched_headers"])
        self.assertEqual(files(self.output / "include/nlohmann"), expected)
        self.assertEqual(files(self.input), self.before)

    def test_clean_and_idempotent(self):
        self.assertEqual(self.stage().returncode, 0)
        self.verify_output()
        before = files(self.output)
        self.assertEqual(self.stage().returncode, 0)
        self.assertEqual(files(self.output), before)

    def test_reject_changed_installed_header(self):
        path = self.input / "nlohmann/json_fwd.hpp"
        path.write_bytes(path.read_bytes() + b"\n// changed input\n")
        before = files(self.input)
        result = self.stage()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unexpected installed", result.stderr)
        self.assertEqual(files(self.input), before)
        self.assertFalse(self.output.exists())

    def test_reject_changed_patch(self):
        patch = self.root / "changed.patch"
        patch.write_bytes(PATCH.read_bytes() + b"\n")
        self.assertNotEqual(self.stage(patch).returncode, 0)
        self.assertEqual(files(self.input), self.before)
        self.assertFalse(self.output.exists())

    def test_preserve_mixed_previous_output(self):
        self.assertEqual(self.stage().returncode, 0)
        path = self.output / "include/nlohmann/json.hpp"
        path.write_bytes(path.read_bytes() + b"\n// changed output\n")
        before = files(self.output)
        result = self.stage()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("mixed existing", result.stderr)
        self.assertEqual(files(self.output), before)
        self.assertEqual(files(self.input), self.before)

    def test_reject_output_inside_installation(self):
        self.output = self.input / "another-output"
        self.assertNotEqual(self.stage().returncode, 0)
        self.assertEqual(files(self.input), self.before)

    def test_enclosing_and_inherited_git_context(self):
        checkout = self.root / "unrelated-checkout"
        checkout.mkdir()
        subprocess.run(["git", "init", "--quiet", str(checkout)], check=True, env=git_environment())
        witness = checkout / "witness"
        witness.write_text("unrelated bytes\n")
        self.output = checkout / "deep/owned-output"
        environment = dict(os.environ, GIT_DIR=str(checkout / ".git"),
                           GIT_WORK_TREE=str(checkout), GIT_INDEX_FILE=str(checkout / "private-index"))
        self.assertEqual(self.stage(environment=environment).returncode, 0)
        self.verify_output()
        self.assertEqual(witness.read_text(), "unrelated bytes\n")
        self.assertFalse((checkout / "private-index").exists())

    def test_cmake_selects_owned_headers(self):
        source = self.root / "cmake-project"
        source.mkdir()
        module = (REPO / "cmake/TelenoJson.cmake").as_posix()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.19)\nproject(json_staging LANGUAGES NONE)\n"
            "add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)\n"
            f'set_property(TARGET nlohmann_json::nlohmann_json PROPERTY INTERFACE_INCLUDE_DIRECTORIES "{self.input.as_posix()}")\n'
            f'include("{module}")\n'
            "get_target_property(_headers nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)\n"
            'file(WRITE "${CMAKE_BINARY_DIR}/selected-headers" "${_headers}")\n')
        build = self.root / "cmake-build"
        result = subprocess.run([CMAKE, "-S", str(source), "-B", str(build)],
                                capture_output=True, text=True, env=git_environment())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        selected = Path((build / "selected-headers").read_text())
        self.assertTrue(selected.is_relative_to(build))
        expected = dict(EXPECTED["original_headers"])
        expected.update(EXPECTED["patched_headers"])
        self.assertEqual(files(selected / "nlohmann"), expected)
        self.assertEqual(files(self.input), self.before)


if __name__ == "__main__":
    unittest.main()
