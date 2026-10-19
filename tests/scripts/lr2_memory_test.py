"""Diagnostic admission checks must fail before spawning or writing node data."""
import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO))
from tools.diagnostics.fixture_paths import artifact_root
spec = importlib.util.spec_from_file_location("lr2", REPO / "tools/diagnostics/measure_lr2_memory.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class AdmissionTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="lr2-admission-", dir=artifact_root(REPO))
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        # Existence/ownership is checked before execution; this is not executable.
        self.binary = Path(os.environ.get("TELENO_TEST_BUILD_ROOT", REPO / "build")) / "teleno_node"
        self.assertTrue(self.binary.is_file())

    def rejects(self, binary, output, repetitions):
        before = set(self.root.rglob("*"))
        with patch.object(module.subprocess, "Popen", side_effect=AssertionError("unexpected spawn")):
            with self.assertRaises(RuntimeError):
                module.run(binary, output, repetitions)
        self.assertEqual(before, set(self.root.rglob("*")))

    def test_repetitions(self):
        for count in (0, 6):
            self.rejects(self.binary, self.root / "new", count)

    def test_nonlocal_binary(self):
        self.rejects(Path(sys.executable).resolve(), self.root / "new", 1)

    def test_nonlocal_output(self):
        self.rejects(self.binary, Path(tempfile.gettempdir()) / "lr2-unowned-output", 1)

    def test_existing_output(self):
        self.rejects(self.binary, self.root, 1)


if __name__ == "__main__":
    unittest.main()
