"""External test-storage selection must not waive source/ownership checks."""
import hashlib
import json
import os
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO))
from tools.diagnostics.fixture_paths import artifact_root, build_is_authorized
from tools.backup.record_sc0_baseline import worktree


class FixturePathsTest(unittest.TestCase):
    def test_default(self):
        with patch.dict(os.environ, {}, clear=True):
            self.assertEqual(artifact_root(REPO), (REPO / "build-single-copy-evidence").resolve())
            self.assertTrue(build_is_authorized(REPO / "build", REPO))

    def test_explicit_root_and_build(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            evidence = root / "evidence"; evidence.mkdir()
            build = root / "build"; build.mkdir()
            with patch.dict(os.environ, {"TELENO_TEST_ARTIFACT_ROOT": str(evidence), "TELENO_TEST_BUILD_ROOT": str(build)}, clear=True):
                self.assertEqual(artifact_root(REPO), evidence)
                self.assertTrue(build_is_authorized(build, REPO))
                self.assertFalse(build_is_authorized(build / "another", REPO))
                self.assertFalse(build_is_authorized(root / "other", REPO))
            with patch.dict(os.environ, {"TELENO_TEST_ARTIFACT_ROOT": str(evidence), "TELENO_TEST_BUILD_ROOT": "/"}, clear=True):
                self.assertFalse(build_is_authorized(Path("/"), REPO))
            with patch.dict(os.environ, {"TELENO_TEST_BUILD_ROOT": str(build)}, clear=True):
                self.assertFalse(build_is_authorized(build, REPO))

    def test_refuses_invalid_roots(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            link = root / "link"; link.symlink_to(root, target_is_directory=True)
            for selected in ("", "relative", "/", "/tmp/..", str(root / "absent"), str(link)):
                with self.subTest(selected=selected), patch.dict(os.environ, {"TELENO_TEST_ARTIFACT_ROOT": selected}, clear=True):
                    with self.assertRaises(RuntimeError):
                        artifact_root(REPO)


class SourceExportProvenanceTest(unittest.TestCase):
    def manifest(self, root):
        source = root / "source"; source.mkdir()
        data = b"retained source bytes\n"
        (source / "input.cpp").write_bytes(data)
        manifest = {"format": "teleno-native-input-manifest", "version": 1,
                    "git_head": "declared-base-only", "branch": "declared-branch",
                    "native_files": {"input.cpp": {"size": len(data),
                        "sha256": hashlib.sha256(data).hexdigest()}}}
        selected = root / "manifest.json"
        selected.write_text(json.dumps(manifest))
        return source, selected, manifest

    def test_export_records_verified_bytes_without_a_git_claim(self):
        with tempfile.TemporaryDirectory() as tmp:
            source, selected, manifest = self.manifest(Path(tmp).resolve())
            with patch.dict(os.environ, {"TELENO_TEST_SOURCE_MANIFEST": str(selected)}, clear=True):
                result = worktree(source)
            self.assertEqual(result["mode"], "verified_source_export")
            self.assertFalse(result["git_available"])
            self.assertIsNone(result["tracked_diff_sha256"])
            self.assertEqual(result["manifest_base_git_head"], "declared-base-only")
            self.assertEqual(result["source_manifest_sha256"], hashlib.sha256(selected.read_bytes()).hexdigest())
            self.assertEqual(result["file_sha256"], {"input.cpp": manifest["native_files"]["input.cpp"]["sha256"]})

    def test_export_requires_explicit_manifest(self):
        with tempfile.TemporaryDirectory() as tmp, patch.dict(os.environ, {}, clear=True):
            with self.assertRaisesRegex(RuntimeError, "explicit TELENO_TEST_SOURCE_MANIFEST"):
                worktree(Path(tmp))

    def test_export_rejects_changed_or_missing_source(self):
        for mode in ("changed", "missing", "wrong_size", "empty"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as tmp:
                source, selected, manifest = self.manifest(Path(tmp).resolve())
                if mode == "changed": (source / "input.cpp").write_bytes(b"different bytes\n")
                elif mode == "missing": (source / "input.cpp").unlink()
                elif mode == "wrong_size": manifest["native_files"]["input.cpp"]["size"] += 1
                else: manifest["native_files"].clear()
                selected.write_text(json.dumps(manifest))
                with patch.dict(os.environ, {"TELENO_TEST_SOURCE_MANIFEST": str(selected)}, clear=True):
                    with self.assertRaises(RuntimeError): worktree(source)

    def test_export_rejects_path_escape_and_linked_inputs(self):
        for name in ("../outside.cpp", "/outside.cpp", "./input.cpp", "linked.cpp"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp).resolve(); source, selected, manifest = self.manifest(root)
                outside = root / "outside.cpp"; outside.write_bytes((source / "input.cpp").read_bytes())
                if name == "linked.cpp": (source / name).symlink_to(outside)
                manifest["native_files"] = {name: manifest["native_files"]["input.cpp"]}
                selected.write_text(json.dumps(manifest))
                with patch.dict(os.environ, {"TELENO_TEST_SOURCE_MANIFEST": str(selected)}, clear=True):
                    with self.assertRaises(RuntimeError): worktree(source)
                self.assertEqual(outside.read_bytes(), b"retained source bytes\n")

    def test_git_checkout_keeps_real_status_and_hashes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            subprocess.run(["git", "init", str(root)], check=True, capture_output=True)
            (root / "untracked.cpp").write_bytes(b"actual Git input\n")
            with patch.dict(os.environ, {}, clear=True): result = worktree(root)
            self.assertEqual(result["mode"], "git_worktree")
            self.assertTrue(result["git_available"])
            self.assertEqual(result["status"]["exit_code"], 0)
            self.assertEqual(result["file_sha256"]["untracked.cpp"], hashlib.sha256(b"actual Git input\n").hexdigest())

    def test_parent_git_repository_is_not_the_export(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve()
            subprocess.run(["git", "init", str(root)], check=True, capture_output=True)
            child = root / "export"; child.mkdir()
            with patch.dict(os.environ, {}, clear=True):
                with self.assertRaisesRegex(RuntimeError, "explicit TELENO_TEST_SOURCE_MANIFEST"):
                    worktree(child)


if __name__ == "__main__":
    unittest.main()
