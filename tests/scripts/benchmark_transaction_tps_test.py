#!/usr/bin/env python3

from __future__ import annotations

import json
import stat
import subprocess
import sys
import tempfile
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/benchmark-transaction-tps.py"


class RpcHandler(BaseHTTPRequestHandler):
    submitted: list[str] = []
    lock = threading.Lock()

    def log_message(self, _format: str, *_args: Any) -> None:
        return

    def do_POST(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        length = int(self.headers.get("Content-Length", "0"))
        request = json.loads(self.rfile.read(length))
        method = request.get("method")
        if method == "chain.get_chain_id":
            result = {"chain_id": "test-chain"}
        elif method == "chain.get_head_info":
            with self.lock:
                height = 11 if self.submitted else 10
            result = {"head_topology": {"height": str(height), "id": "0xhead"}}
        elif method == "mempool.get_pending_transactions":
            with self.lock:
                result = {"pending_transactions": [{"transaction": {"id": tx_id}} for tx_id in self.submitted]}
        elif method == "chain.submit_transaction":
            transaction_id = request["params"]["transaction"]["id"]
            with self.lock:
                self.submitted.append(transaction_id)
            result = {"receipt": {"id": transaction_id}}
        elif method == "block_store.get_blocks_by_height":
            with self.lock:
                transactions = [{"id": tx_id} for tx_id in self.submitted]
            result = {"block_items": [{"block": {"transactions": transactions}}]}
        else:
            self.send_response(400)
            self.end_headers()
            return
        body = json.dumps({"jsonrpc": "2.0", "id": request.get("id"), "result": result}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class BenchmarkTransactionTpsTest(unittest.TestCase):
    def setUp(self) -> None:
        RpcHandler.submitted = []
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), RpcHandler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.rpc_url = f"http://127.0.0.1:{self.server.server_port}/"
        self.temp = tempfile.TemporaryDirectory()
        self.temp_path = Path(self.temp.name)

    def tearDown(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)
        self.temp.cleanup()

    def workload(self, count: int = 20, secret: bool = False) -> Path:
        path = self.temp_path / "workload.jsonl"
        records = []
        for index in range(count):
            record: dict[str, Any] = {
                "transaction_id": f"0xtx{index:04d}",
                "params": {
                    "transaction": {"id": f"0xtx{index:04d}", "header": {}, "operations": [], "signatures": []},
                    "broadcast": True,
                },
            }
            if secret:
                record["private_key"] = "must-not-be-accepted"
            records.append(json.dumps(record))
        path.write_text("\n".join(records) + "\n")
        return path

    def run_script(self, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(SCRIPT), *args],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )

    def test_dry_run_validates_without_contacting_rpc(self) -> None:
        result_dir = self.temp_path / "dry-run"
        result = self.run_script(
            "--workload", str(self.workload(5)),
            "--rpc-url", "http://127.0.0.1:1/",
            "--result-dir", str(result_dir),
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads((result_dir / "result.json").read_text())
        self.assertEqual(payload["mode"], "dry-run")
        self.assertEqual(payload["workload"]["transactions"], 5)
        self.assertEqual(stat.S_IMODE(result_dir.stat().st_mode), 0o700)
        self.assertEqual(stat.S_IMODE((result_dir / "result.json").stat().st_mode), 0o600)
        self.assertEqual(stat.S_IMODE((result_dir / "result.md").stat().st_mode), 0o600)
        self.assertEqual(RpcHandler.submitted, [])

    def test_live_simulation_measures_concurrent_accepted_tps(self) -> None:
        result_dir = self.temp_path / "submit"
        phrase = f"SUBMIT 20 TRANSACTIONS TO 127.0.0.1:{self.server.server_port} ON test-chain"
        result = self.run_script(
            "--workload", str(self.workload()),
            "--rpc-url", self.rpc_url,
            "--expected-chain-id", "test-chain",
            "--concurrency", "4",
            "--target-tps", "50",
            "--max-transactions", "20",
            "--verify-inclusion",
            "--witness-rpc-url", self.rpc_url,
            "--submit",
            "--confirm", phrase,
            "--result-dir", str(result_dir),
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads((result_dir / "result.json").read_text())
        self.assertEqual(payload["status"], "pass")
        self.assertEqual(payload["load"]["accepted"], 20)
        self.assertEqual(payload["load"]["rejected"], 0)
        self.assertGreater(payload["load"]["accepted_tps"], 0)
        self.assertEqual(payload["inclusion"]["included"], 20)
        self.assertEqual(payload["inclusion"]["missing"], [])
        self.assertGreater(payload["inclusion"]["confirmed_tps"], 0)
        self.assertGreaterEqual(
            payload["inclusion"]["end_to_end_elapsed_seconds"],
            payload["load"]["submission_elapsed_seconds"],
        )
        self.assertEqual(len(RpcHandler.submitted), 20)

    def test_rejects_secret_fields_and_duplicate_ids(self) -> None:
        secret = self.run_script("--workload", str(self.workload(1, secret=True)))
        self.assertEqual(secret.returncode, 2)
        self.assertIn("forbidden secret-like field", secret.stderr)

        duplicate_path = self.temp_path / "duplicates.jsonl"
        record = json.dumps({
            "transaction_id": "0xduplicate",
            "params": {"transaction": {"id": "0xduplicate"}},
        })
        duplicate_path.write_text(f"{record}\n{record}\n")
        duplicate = self.run_script("--workload", str(duplicate_path))
        self.assertEqual(duplicate.returncode, 2)
        self.assertIn("duplicate transaction_id", duplicate.stderr)

    def test_requires_exact_confirmation_and_chain_identity(self) -> None:
        workload = self.workload(2)
        confirmation = self.run_script(
            "--workload", str(workload),
            "--rpc-url", self.rpc_url,
            "--expected-chain-id", "test-chain",
            "--max-transactions", "2",
            "--submit",
            "--confirm", "wrong",
        )
        self.assertEqual(confirmation.returncode, 2)
        self.assertIn("confirmation mismatch", confirmation.stderr)

        chain = self.run_script(
            "--workload", str(workload),
            "--rpc-url", self.rpc_url,
            "--expected-chain-id", "wrong-chain",
            "--max-transactions", "2",
            "--submit",
            "--confirm", f"SUBMIT 2 TRANSACTIONS TO 127.0.0.1:{self.server.server_port} ON wrong-chain",
        )
        self.assertEqual(chain.returncode, 2)
        self.assertIn("chain ID mismatch", chain.stderr)
        self.assertEqual(RpcHandler.submitted, [])

        missing_cap = self.run_script(
            "--workload", str(workload),
            "--rpc-url", self.rpc_url,
            "--expected-chain-id", "test-chain",
            "--submit",
        )
        self.assertEqual(missing_cap.returncode, 2)
        self.assertIn("requires an explicit --max-transactions", missing_cap.stderr)


if __name__ == "__main__":
    unittest.main()
