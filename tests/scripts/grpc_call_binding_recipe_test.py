"""Pinned SDK source and single-member archive repair, with no network access."""
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
SOURCE = Path(sys.argv.pop(1))
AR = sys.argv.pop(1)
RANLIB = sys.argv.pop(1)
PATCH = REPO / "cmake/ApplyGrpcCallBinding.py"
REPACK = REPO / "cmake/RepackGrpcCallBinding.py"
MANIFEST = REPO / "cmake/patches/grpc-call-binding-manifest.json"
RELATIVE = "src/cpp/server/server_cc.cc"
HASHES = json.loads(MANIFEST.read_text())[RELATIVE]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Recipe(unittest.TestCase):
    def setUp(self):
        root = os.environ.get("TELENO_TEST_ARTIFACT_ROOT") or os.environ.get("TMPDIR")
        self.assertTrue(root, "Explicit scratch root required")
        self.tmp = tempfile.TemporaryDirectory(prefix="grpc-call-binding-recipe-", dir=root)
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source = self.root / "source"
        self.path = self.source / RELATIVE
        self.path.parent.mkdir(parents=True)
        shutil.copyfile(SOURCE / RELATIVE, self.path)

    def pristine(self):
        data = self.path.read_bytes()
        data = data.replace(
            b"""              (ctx_.set_call(mrd->call_),
               ctx_.set_server_rpc_info(method_->name(), method_->method_type(),
                                        server->interceptor_creators_))),""",
            b"""              ctx_.set_server_rpc_info(method_->name(), method_->method_type(),
                                       server->interceptor_creators_)),""")
        data = data.replace(b"""          resources_(false) {
      ctx_.cq_ = &cq_;""", b"""          resources_(false) {
      ctx_.set_call(mrd->call_);
      ctx_.cq_ = &cq_;""")
        self.assertEqual(hashlib.sha256(data).hexdigest(), HASHES["before"])
        self.path.write_bytes(data)

    def patch(self, manifest=MANIFEST):
        return subprocess.run([sys.executable, "-B", str(PATCH), str(self.source), str(manifest)],
                              capture_output=True, text=True, timeout=30)

    def test_patched_idempotence(self):
        self.assertEqual(digest(self.path), HASHES["after"])
        self.assertEqual(self.patch().returncode, 0)
        self.assertEqual(digest(self.path), HASHES["after"])

    def test_pristine_to_exact_patch(self):
        self.pristine()
        result = self.patch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(digest(self.path), HASHES["after"])

    def test_unknown_source_preserved(self):
        self.path.write_bytes(b"unexpected source")
        before = self.path.read_bytes()
        self.assertNotEqual(self.patch().returncode, 0)
        self.assertEqual(self.path.read_bytes(), before)

    def test_wrong_output_pin_preserves_source(self):
        self.pristine()
        before = self.path.read_bytes()
        data = json.loads(MANIFEST.read_text())
        data[RELATIVE]["after"] = "0" * 64
        manifest = self.root / "wrong.json"
        manifest.write_text(json.dumps(data))
        self.assertNotEqual(self.patch(manifest).returncode, 0)
        self.assertEqual(self.path.read_bytes(), before)

    def archive(self, member="server_cc.cc.o", duplicate=False):
        installed = self.root / "installed"
        installed.mkdir()
        old = installed / member
        old.write_bytes(b"old object")
        other = installed / "unchanged.o"
        other.write_bytes(b"unrelated object")
        archive = installed / "libgrpc++.a"
        subprocess.run([AR, "rc", str(archive), str(old), str(other)], check=True,
                       capture_output=True)
        if duplicate:
            subprocess.run([AR, "q", str(archive), str(old)], check=True, capture_output=True)
        replacement = self.root / "replacement.o"
        replacement.write_bytes(b"new call-binding object")
        return archive, replacement, self.root / "owned/libgrpc++.a"

    def repack(self, archive, replacement, output, expected=None):
        return subprocess.run([sys.executable, "-B", str(REPACK), "--archive", str(archive),
                               "--object", str(replacement), "--output", str(output),
                               "--ar", AR, "--ranlib", RANLIB,
                               "--expected-sha256", expected or digest(archive)],
                              capture_output=True, text=True, timeout=30)

    def test_one_member_only_and_installed_preserved(self):
        archive, replacement, output = self.archive()
        before = digest(archive)
        result = self.repack(archive, replacement, output)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(digest(archive), before)
        identity = json.loads(output.with_suffix(".a.identity.json").read_text())
        self.assertEqual(identity["changed_members"], ["server_cc.cc.o"])
        self.assertEqual(identity["unchanged_member_count"], 1)
        self.assertEqual(identity["original_sha256"], before)
        self.assertEqual(identity["replacement_sha256"], digest(replacement))
        self.assertEqual(subprocess.check_output([AR, "p", str(output), "unchanged.o"]),
                         b"unrelated object")

    def test_changed_installed_archive_refused(self):
        archive, replacement, output = self.archive()
        before = digest(archive)
        result = self.repack(archive, replacement, output, "0" * 64)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(output.exists())
        self.assertEqual(digest(archive), before)

    def test_missing_member_refused(self):
        archive, replacement, output = self.archive("different.o")
        self.assertNotEqual(self.repack(archive, replacement, output).returncode, 0)
        self.assertFalse(output.exists())

    def test_duplicate_member_refused(self):
        archive, replacement, output = self.archive(duplicate=True)
        self.assertNotEqual(self.repack(archive, replacement, output).returncode, 0)
        self.assertFalse(output.exists())

    def test_installed_output_alias_refused(self):
        archive, replacement, _ = self.archive()
        before = digest(archive)
        self.assertNotEqual(self.repack(archive, replacement, archive).returncode, 0)
        self.assertEqual(digest(archive), before)

    def test_unchanged_object_refused(self):
        archive, replacement, output = self.archive()
        replacement.write_bytes(b"old object")
        self.assertNotEqual(self.repack(archive, replacement, output).returncode, 0)
        self.assertFalse(output.exists())

    def selector(self, suffix):
        # Execute the actual selector in isolation from SDK downloads. The
        # independent Debug/Release fixtures reproduce the configuration bug.
        module = (REPO / "cmake/TelenoGrpcCallBinding.cmake").read_text()
        start = module.index("function(_teleno_grpc_base_for_config ")
        end = module.index("\nendfunction()", start) + len("\nendfunction()")
        directory = self.root / "selector"
        directory.mkdir(exist_ok=True)
        debug = directory / "libgrpc++d.a"
        release = directory / "libgrpc++.a"
        debug.write_bytes(b"debug SDK")
        release.write_bytes(b"release SDK")
        project = ("cmake_minimum_required(VERSION 3.20)\nproject(Selector NONE)\n"
                   "add_library(gRPC::grpc++ STATIC IMPORTED)\n"
                   "set_target_properties(gRPC::grpc++ PROPERTIES IMPORTED_CONFIGURATIONS DEBUG\\;RELEASE\n"
                   f"  IMPORTED_LOCATION_DEBUG [=[{debug}]=]\n"
                   f"  IMPORTED_LOCATION_RELEASE [=[{release}]=])\n"
                   + module[start:end] + "\n" + suffix)
        (directory / "CMakeLists.txt").write_text(project)
        cmake = shutil.which("cmake")
        self.assertTrue(cmake, "CMake is required for SDK configuration checks")
        result = subprocess.run([cmake, "-S", str(directory), "-B", str(directory / "build")],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(debug.read_bytes(), b"debug SDK")
        self.assertEqual(release.read_bytes(), b"release SDK")
        return result, directory, debug, release

    def test_release_does_not_select_first_debug_import(self):
        result, directory, debug, release = self.selector(
            '_teleno_grpc_base_for_config(Release selected_release)\n'
            '_teleno_grpc_base_for_config(Debug selected_debug)\n'
            'file(WRITE "${CMAKE_SOURCE_DIR}/selected" "${selected_release}\\n${selected_debug}")\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((directory / "selected").read_text().splitlines(), [str(release), str(debug)])

    def test_imported_configuration_mapping(self):
        result, directory, _, release = self.selector(
            'set_property(TARGET gRPC::grpc++ PROPERTY MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)\n'
            '_teleno_grpc_base_for_config(RelWithDebInfo selected)\n'
            'file(WRITE "${CMAKE_SOURCE_DIR}/selected" "${selected}")\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((directory / "selected").read_text(), str(release))

    def test_explicit_archive_requires_matching_pin(self):
        result, _, _, _ = self.selector(
            'set(TELENO_GRPC_BASE_ARCHIVE "${CMAKE_SOURCE_DIR}/libgrpc++d.a")\n'
            'set(TELENO_GRPC_BASE_SHA256 "wrong")\n'
            '_teleno_grpc_base_for_config(Release selected)\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Explicit gRPC base archive hash mismatch", result.stderr)


if __name__ == "__main__":
    unittest.main()
