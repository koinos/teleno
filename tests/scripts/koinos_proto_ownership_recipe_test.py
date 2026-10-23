"""Verify exact generated-source ownership repair without network access."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
SOURCE = Path(sys.argv.pop(1))
SCRIPT = REPO / "cmake/ApplyKoinosProtoOwnership.py"
MANIFEST = REPO / "cmake/patches/koinos-proto-copy-ownership-manifest.json"
ITEMS = json.loads(MANIFEST.read_text())


class Recipe(unittest.TestCase):
    def setUp(self):
        root = os.environ.get("TELENO_TEST_ARTIFACT_ROOT") or os.environ.get("TMPDIR")
        self.assertTrue(root, "Explicit scratch root required")
        self.tmp = tempfile.TemporaryDirectory(prefix="koinos-proto-recipe-", dir=root)
        self.addCleanup(self.tmp.cleanup)
        self.source = Path(self.tmp.name) / "source"
        for name in ITEMS:
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(SOURCE / name, path)

    def digest(self):
        return {name: hashlib.sha256((self.source / name).read_bytes()).hexdigest()
                for name in ITEMS}

    def run_patch(self, manifest=MANIFEST):
        return subprocess.run([sys.executable, "-B", str(SCRIPT), str(self.source), str(manifest)],
                              capture_output=True, text=True, timeout=30)

    def test_patched_idempotence_and_inventory(self):
        expected = {name: item["after"] for name, item in ITEMS.items()}
        self.assertEqual(self.digest(), expected)
        self.assertEqual(sum(item["constructors"] for item in ITEMS.values()), 481)
        self.assertEqual(sum(item["oneof_messages"] for item in ITEMS.values()), 94)
        result = self.run_patch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.digest(), expected)

    def test_unknown_input_preserves_all_files(self):
        name = next(iter(ITEMS))
        (self.source / name).write_bytes(b"unexpected generated source\n")
        before = self.digest()
        result = self.run_patch()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unexpected generated Koinos source", result.stderr)
        self.assertEqual(self.digest(), before)

    def test_wrong_output_pin_preserves_all_files(self):
        # An after-state with an altered pin cannot be mistaken for a pristine
        # archive or partially rewritten by the repair script.
        changed = json.loads(MANIFEST.read_text())
        changed[next(iter(changed))]["after"] = "0" * 64
        manifest = Path(self.tmp.name) / "altered-manifest.json"
        manifest.write_text(json.dumps(changed))
        before = self.digest()
        result = self.run_patch(manifest)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.digest(), before)

    def test_mixed_header_patch_state_preserves_all_files(self):
        name = next(name for name, item in ITEMS.items()
                    if name.startswith("include/") and item["oneof_messages"])
        path = self.source / name
        pattern = re.compile(
            r"(?m)^([ \t]*)auto\* new_value = (CreateMaybeMessage<[^;\n]+);\n"
            r"\1clear_(\w+)\(\);\n\1(\w+\.\w+_) = new_value;\n"
            r"\1set_has_(\w+)\(\);")

        def reverse(match):
            indent, create, group, target, field = match.groups()
            return (f"{indent}clear_{group}();\n{indent}set_has_{field}();\n"
                    f"{indent}{target} = {create};")

        original, count = pattern.subn(reverse, path.read_text())
        self.assertEqual(count, ITEMS[name]["oneof_messages"])
        self.assertEqual(hashlib.sha256(original.encode()).hexdigest(), ITEMS[name]["before"])
        path.write_text(original)
        before = self.digest()
        result = self.run_patch()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("mixed ownership patch state", result.stderr)
        self.assertEqual(self.digest(), before)


if __name__ == "__main__":
    unittest.main()
