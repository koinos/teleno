#!/usr/bin/env python3
"""Measure sustained Koinos transaction submission and inclusion throughput.

The benchmark consumes unique, pre-signed chain.submit_transaction requests
from a JSON Lines workload. It does not load wallet material or generate
transactions. Dry-run validation is the default; live submission requires an
explicit flag, expected chain ID, transaction cap, and exact confirmation.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import math
import os
import re
import statistics
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from collections import Counter
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable


DEFAULT_RESULT_ROOT = Path("/private/tmp/teleno-transaction-tps")
SENSITIVE_KEY = re.compile(r"(?i)(private.?key|password|secret|seed.?phrase|mnemonic|wif|bearer|api.?token)")


class BenchmarkError(RuntimeError):
    pass


@dataclass(frozen=True)
class WorkloadEntry:
    transaction_id: str
    params: dict[str, Any]


@dataclass
class SubmissionResult:
    transaction_id: str
    index: int
    scheduled_offset_ms: float
    started_offset_ms: float
    schedule_lag_ms: float
    latency_ms: float
    completed_offset_ms: float
    accepted: bool
    http_status: int | None
    rpc_error_code: int | str | None
    error: str | None


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def safe_timestamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def round_number(value: float, places: int = 3) -> float:
    return round(value, places)


def percentile(values: list[float], percentage: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    rank = (len(ordered) - 1) * percentage / 100
    lower = math.floor(rank)
    upper = math.ceil(rank)
    if lower == upper:
        return ordered[lower]
    fraction = rank - lower
    return ordered[lower] * (1 - fraction) + ordered[upper] * fraction


def summarize(values: Iterable[float]) -> dict[str, Any]:
    samples = list(values)
    if not samples:
        return {"count": 0}
    return {
        "count": len(samples),
        "min": round_number(min(samples)),
        "mean": round_number(statistics.fmean(samples)),
        "p50": round_number(percentile(samples, 50) or 0),
        "p95": round_number(percentile(samples, 95) or 0),
        "p99": round_number(percentile(samples, 99) or 0),
        "max": round_number(max(samples)),
    }


def sanitized_url(url: str) -> str:
    parsed = urllib.parse.urlsplit(url)
    host = parsed.hostname or ""
    if parsed.port is not None:
        host = f"{host}:{parsed.port}"
    return urllib.parse.urlunsplit((parsed.scheme, host, parsed.path, "", ""))


def confirmation_phrase(transaction_count: int, rpc_url: str, chain_id: str) -> str:
    parsed = urllib.parse.urlsplit(rpc_url)
    endpoint = parsed.hostname or "unknown-host"
    if parsed.port is not None:
        endpoint = f"{endpoint}:{parsed.port}"
    return f"SUBMIT {transaction_count} TRANSACTIONS TO {endpoint} ON {chain_id}"


def contains_sensitive_key(value: Any) -> bool:
    if isinstance(value, dict):
        return any(SENSITIVE_KEY.search(str(key)) or contains_sensitive_key(item) for key, item in value.items())
    if isinstance(value, list):
        return any(contains_sensitive_key(item) for item in value)
    return False


def load_workload(path: Path, max_transactions: int) -> list[WorkloadEntry]:
    if not path.is_file():
        raise BenchmarkError(f"workload is not a regular file: {path}")
    entries: list[WorkloadEntry] = []
    seen_ids: set[str] = set()
    with path.open(encoding="utf-8") as handle:
        for line_number, raw_line in enumerate(handle, start=1):
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as exc:
                raise BenchmarkError(f"invalid JSON on workload line {line_number}: {exc}") from exc
            if not isinstance(record, dict):
                raise BenchmarkError(f"workload line {line_number} must be an object")
            if contains_sensitive_key(record):
                raise BenchmarkError(f"workload line {line_number} contains a forbidden secret-like field")
            transaction_id = record.get("transaction_id")
            params = record.get("params")
            if not isinstance(transaction_id, str) or not transaction_id:
                raise BenchmarkError(f"workload line {line_number} requires a non-empty transaction_id")
            if transaction_id in seen_ids:
                raise BenchmarkError(f"duplicate transaction_id on workload line {line_number}: {transaction_id}")
            if not isinstance(params, dict) or not isinstance(params.get("transaction"), dict):
                raise BenchmarkError(f"workload line {line_number} requires params.transaction")
            seen_ids.add(transaction_id)
            entries.append(WorkloadEntry(transaction_id=transaction_id, params=params))
            if len(entries) > max_transactions:
                raise BenchmarkError(
                    f"workload contains more than --max-transactions {max_transactions}; "
                    "raise the explicit safety cap to continue"
                )
    if not entries:
        raise BenchmarkError("workload contains no transactions")
    return entries


def rpc_call(
    url: str,
    method: str,
    params: dict[str, Any],
    timeout: float,
    request_id: int = 1,
) -> tuple[dict[str, Any], int]:
    body = json.dumps({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params}).encode()
    request = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"}, method="POST")
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            payload = json.loads(response.read().decode())
            return payload, response.status
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode(errors="replace")
        try:
            payload = json.loads(raw)
        except json.JSONDecodeError:
            payload = {"error": {"code": f"HTTP_{exc.code}", "message": raw[:500]}}
        return payload, exc.code


def extract_chain_id(payload: dict[str, Any]) -> str:
    result = payload.get("result", {})
    if isinstance(result, dict):
        chain_id = result.get("chain_id") or result.get("chainId")
        if isinstance(chain_id, str):
            return chain_id
    raise BenchmarkError(f"chain.get_chain_id returned no chain ID: {payload}")


def pending_count(rpc_url: str, timeout: float) -> int | None:
    try:
        payload, _ = rpc_call(rpc_url, "mempool.get_pending_transactions", {}, timeout)
        result = payload.get("result", {})
        pending = result.get("pending_transactions", []) if isinstance(result, dict) else []
        return len(pending) if isinstance(pending, list) else None
    except (OSError, ValueError, BenchmarkError):
        return None


def submit_one(
    entry: WorkloadEntry,
    index: int,
    rpc_url: str,
    timeout: float,
    benchmark_started: float,
    scheduled_at: float,
) -> SubmissionResult:
    delay = scheduled_at - time.perf_counter()
    if delay > 0:
        time.sleep(delay)
    started = time.perf_counter()
    started_offset_ms = (started - benchmark_started) * 1000
    try:
        payload, http_status = rpc_call(rpc_url, "chain.submit_transaction", entry.params, timeout, index + 1)
        finished = time.perf_counter()
        error = payload.get("error")
        accepted = error is None and 200 <= http_status < 300
        rpc_error_code = error.get("code") if isinstance(error, dict) else None
        error_message = error.get("message") if isinstance(error, dict) else None
        return SubmissionResult(
            transaction_id=entry.transaction_id,
            index=index,
            scheduled_offset_ms=round_number((scheduled_at - benchmark_started) * 1000),
            started_offset_ms=round_number(started_offset_ms),
            schedule_lag_ms=round_number(max(0, (started - scheduled_at) * 1000)),
            latency_ms=round_number((finished - started) * 1000),
            completed_offset_ms=round_number((finished - benchmark_started) * 1000),
            accepted=accepted,
            http_status=http_status,
            rpc_error_code=rpc_error_code,
            error=str(error_message)[:500] if error_message is not None else None,
        )
    except Exception as exc:  # noqa: BLE001 - benchmark records transport failures
        finished = time.perf_counter()
        return SubmissionResult(
            transaction_id=entry.transaction_id,
            index=index,
            scheduled_offset_ms=round_number((scheduled_at - benchmark_started) * 1000),
            started_offset_ms=round_number(started_offset_ms),
            schedule_lag_ms=round_number(max(0, (started - scheduled_at) * 1000)),
            latency_ms=round_number((finished - started) * 1000),
            completed_offset_ms=round_number((finished - benchmark_started) * 1000),
            accepted=False,
            http_status=None,
            rpc_error_code=None,
            error=f"{type(exc).__name__}: {exc}"[:500],
        )


def submit_workload(entries: list[WorkloadEntry], args: argparse.Namespace) -> tuple[list[SubmissionResult], float]:
    benchmark_started = time.perf_counter()
    interval = 0 if args.target_tps <= 0 else 1 / args.target_tps
    futures: list[concurrent.futures.Future[SubmissionResult]] = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.concurrency, thread_name_prefix="tps") as executor:
        for index, entry in enumerate(entries):
            scheduled_at = benchmark_started + index * interval
            futures.append(executor.submit(
                submit_one,
                entry,
                index,
                args.rpc_url,
                args.rpc_timeout,
                benchmark_started,
                scheduled_at,
            ))
        results = [future.result() for future in futures]
    elapsed = time.perf_counter() - benchmark_started
    return sorted(results, key=lambda row: row.index), elapsed


def per_second_buckets(results: list[SubmissionResult]) -> list[dict[str, Any]]:
    buckets: dict[int, Counter[str]] = {}
    for result in results:
        second = max(0, int(result.completed_offset_ms // 1000))
        bucket = buckets.setdefault(second, Counter())
        bucket["completed"] += 1
        bucket["accepted" if result.accepted else "rejected"] += 1
    return [
        {
            "second": second,
            "completed": counts["completed"],
            "accepted": counts["accepted"],
            "rejected": counts["rejected"],
        }
        for second, counts in sorted(buckets.items())
    ]


def get_head(rpc_url: str, timeout: float) -> tuple[int, str]:
    payload, _ = rpc_call(rpc_url, "chain.get_head_info", {}, timeout)
    result = payload.get("result", {})
    topology = result.get("head_topology", {}) if isinstance(result, dict) else {}
    try:
        return int(topology.get("height", 0)), str(topology.get("id", ""))
    except (TypeError, ValueError) as exc:
        raise BenchmarkError(f"invalid head topology: {topology}") from exc


def block_transaction_ids(block: Any) -> set[str]:
    if not isinstance(block, dict):
        return set()
    transactions = block.get("transactions", [])
    if not isinstance(transactions, list):
        return set()
    return {
        str(transaction.get("id"))
        for transaction in transactions
        if isinstance(transaction, dict) and transaction.get("id")
    }


def verify_inclusion(
    transaction_ids: list[str],
    rpc_url: str,
    start_height: int,
    submission_elapsed: float,
    timeout: float,
    poll_interval: float,
    rpc_timeout: float,
) -> dict[str, Any]:
    pending = set(transaction_ids)
    included: dict[str, dict[str, Any]] = {}
    next_height = start_height
    started = time.perf_counter()
    checked_blocks = 0
    while pending and time.perf_counter() - started <= timeout:
        head_height, head_id = get_head(rpc_url, rpc_timeout)
        if next_height <= head_height:
            count = min(100, head_height - next_height + 1)
            payload, _ = rpc_call(
                rpc_url,
                "block_store.get_blocks_by_height",
                {
                    "head_block_id": head_id,
                    "ancestor_start_height": str(next_height),
                    "num_blocks": count,
                    "return_block": True,
                    "return_receipt": False,
                },
                rpc_timeout,
            )
            result = payload.get("result", {})
            items = result.get("block_items", []) if isinstance(result, dict) else []
            for offset, item in enumerate(items if isinstance(items, list) else []):
                height = next_height + offset
                ids = block_transaction_ids(item.get("block", {}) if isinstance(item, dict) else {})
                for transaction_id in pending.intersection(ids):
                    included[transaction_id] = {
                        "height": height,
                        "detected_after_ms": round_number((time.perf_counter() - started) * 1000),
                    }
                pending.difference_update(ids)
                checked_blocks += 1
            item_count = len(items) if isinstance(items, list) else 0
            if item_count > 0:
                next_height += item_count
        if pending:
            time.sleep(poll_interval)
    elapsed = time.perf_counter() - started
    end_to_end_elapsed = submission_elapsed + elapsed
    return {
        "requested": len(transaction_ids),
        "included": len(included),
        "missing": sorted(pending),
        "included_transactions": included,
        "checked_blocks": checked_blocks,
        "polling_elapsed_seconds": round_number(elapsed),
        "end_to_end_elapsed_seconds": round_number(end_to_end_elapsed),
        "confirmed_tps": round_number(len(included) / end_to_end_elapsed) if end_to_end_elapsed > 0 else None,
    }


def result_to_dict(result: SubmissionResult) -> dict[str, Any]:
    return {
        "transaction_id": result.transaction_id,
        "index": result.index,
        "scheduled_offset_ms": result.scheduled_offset_ms,
        "started_offset_ms": result.started_offset_ms,
        "schedule_lag_ms": result.schedule_lag_ms,
        "latency_ms": result.latency_ms,
        "completed_offset_ms": result.completed_offset_ms,
        "accepted": result.accepted,
        "http_status": result.http_status,
        "rpc_error_code": result.rpc_error_code,
        "error": result.error,
    }


def build_result(
    args: argparse.Namespace,
    entries: list[WorkloadEntry],
    submissions: list[SubmissionResult],
    submission_elapsed: float,
    chain_id: str | None,
    pending_before: int | None,
    pending_after: int | None,
    inclusion: dict[str, Any] | None,
    started_at: str,
) -> dict[str, Any]:
    accepted = [row for row in submissions if row.accepted]
    rejected = [row for row in submissions if not row.accepted]
    status = "warn" if not args.submit else "pass"
    if args.submit and (rejected or (inclusion is not None and inclusion["missing"])):
        status = "fail"
    return {
        "kind": "teleno-transaction-tps-benchmark",
        "schema_version": 1,
        "status": status,
        "mode": "submit" if args.submit else "dry-run",
        "started_at": started_at,
        "finished_at": utc_now(),
        "rpc_url": sanitized_url(args.rpc_url),
        "witness_rpc_url": sanitized_url(args.witness_rpc_url) if args.witness_rpc_url else None,
        "chain_id": chain_id,
        "workload": {
            "path": str(args.workload),
            "transactions": len(entries),
            "max_transactions": args.max_transactions,
        },
        "load": {
            "concurrency": args.concurrency,
            "target_tps": args.target_tps if args.target_tps > 0 else None,
            "submission_elapsed_seconds": round_number(submission_elapsed),
            "offered_tps": round_number(len(submissions) / submission_elapsed) if submission_elapsed > 0 else None,
            "accepted_tps": round_number(len(accepted) / submission_elapsed) if submission_elapsed > 0 else None,
            "completed": len(submissions),
            "accepted": len(accepted),
            "rejected": len(rejected),
        },
        "latency_ms": {
            "all": summarize(row.latency_ms for row in submissions),
            "accepted": summarize(row.latency_ms for row in accepted),
            "schedule_lag": summarize(row.schedule_lag_ms for row in submissions),
        },
        "mempool": {"pending_before": pending_before, "pending_after_submission": pending_after},
        "per_second": per_second_buckets(submissions),
        "inclusion": inclusion,
        "submissions": [result_to_dict(row) for row in submissions],
    }


def write_outputs(result: dict[str, Any], result_dir: Path) -> None:
    result_dir.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(result_dir, 0o700)
    json_path = result_dir / "result.json"
    markdown_path = result_dir / "result.md"
    json_path.write_text(json.dumps(result, indent=2) + "\n")
    os.chmod(json_path, 0o600)
    load = result["load"]
    latency = result["latency_ms"]["accepted"]
    inclusion = result.get("inclusion")
    lines = [
        "# Transaction TPS Benchmark",
        "",
        f"- Status: `{result['status']}`",
        f"- Mode: `{result['mode']}`",
        f"- Started: `{result['started_at']}`",
        f"- Finished: `{result['finished_at']}`",
        f"- RPC: `{result['rpc_url']}`",
        f"- Chain ID: `{result['chain_id'] or 'not queried in dry-run'}`",
        f"- Transactions: `{result['workload']['transactions']}`",
        f"- Concurrency: `{load['concurrency']}`",
        f"- Target TPS: `{load['target_tps'] or 'unlimited'}`",
        "",
        "## Throughput",
        "",
        f"- Offered TPS: `{load['offered_tps']}`",
        f"- Accepted TPS: `{load['accepted_tps']}`",
        f"- Accepted: `{load['accepted']}`",
        f"- Rejected: `{load['rejected']}`",
        f"- Submission window: `{load['submission_elapsed_seconds']} seconds`",
        "",
        "## Accepted Submission Latency",
        "",
        f"- Mean: `{latency.get('mean')} ms`",
        f"- p50: `{latency.get('p50')} ms`",
        f"- p95: `{latency.get('p95')} ms`",
        f"- p99: `{latency.get('p99')} ms`",
        f"- Max: `{latency.get('max')} ms`",
        "",
        "## Inclusion",
        "",
    ]
    if inclusion is None:
        lines.append("- Inclusion verification: `not requested`")
    else:
        lines.extend([
            f"- Included: `{inclusion['included']}`",
            f"- Missing: `{len(inclusion['missing'])}`",
            f"- Confirmed TPS: `{inclusion['confirmed_tps']}`",
            f"- End-to-end inclusion window: `{inclusion['end_to_end_elapsed_seconds']} seconds`",
            f"- Post-submission polling: `{inclusion['polling_elapsed_seconds']} seconds`",
        ])
    lines.extend([
        "",
        "## Interpretation",
        "",
        "- Accepted TPS measures JSON-RPC admission over the submission window.",
        "- Confirmed TPS is reported only when block inclusion verification is enabled.",
        "- Neither metric should be published without the workload, hardware, "
        "node config, block limits, and transaction mix.",
        "- The workload contains signed public transaction data only; wallet secrets are forbidden.",
        "",
        f"JSON result: `{json_path}`",
        "",
    ])
    markdown_path.write_text("\n".join(lines))
    os.chmod(markdown_path, 0o600)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workload", type=Path, required=True, help="JSONL file of unique signed transactions")
    parser.add_argument("--rpc-url", default="http://127.0.0.1:18122/", help="local JSON-RPC endpoint")
    parser.add_argument("--witness-rpc-url", default="", help="RPC endpoint used for optional inclusion verification")
    parser.add_argument("--expected-chain-id", default="", help="required expected chain ID for live submission")
    parser.add_argument("--concurrency", type=int, default=4, help="concurrent HTTP submissions (1-256)")
    parser.add_argument("--target-tps", type=float, default=0, help="offered rate; 0 means unlimited")
    parser.add_argument("--max-transactions", type=int, default=None, help="explicit workload safety cap")
    parser.add_argument("--rpc-timeout", type=float, default=10, help="per-request timeout seconds")
    parser.add_argument(
        "--verify-inclusion",
        action="store_true",
        help="scan witness blocks for accepted transaction IDs",
    )
    parser.add_argument("--inclusion-timeout", type=float, default=180, help="inclusion verification timeout seconds")
    parser.add_argument("--poll-interval", type=float, default=1, help="inclusion poll interval seconds")
    parser.add_argument("--result-dir", type=Path, default=None, help="report directory")
    parser.add_argument("--submit", action="store_true", help="perform live submission; default is dry-run")
    parser.add_argument("--confirm", default="", help="exact live-submission confirmation phrase")
    args = parser.parse_args(argv)
    if not 1 <= args.concurrency <= 256:
        parser.error("--concurrency must be between 1 and 256")
    if args.target_tps < 0:
        parser.error("--target-tps must be non-negative")
    if args.max_transactions is not None and args.max_transactions <= 0:
        parser.error("--max-transactions must be positive")
    if args.rpc_timeout <= 0 or args.inclusion_timeout <= 0 or args.poll_interval <= 0:
        parser.error("timeouts and poll interval must be positive")
    if args.verify_inclusion and not args.witness_rpc_url:
        parser.error("--verify-inclusion requires --witness-rpc-url")
    if args.submit and args.max_transactions is None:
        parser.error("--submit requires an explicit --max-transactions safety cap")
    if args.max_transactions is None:
        args.max_transactions = 1000
    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    started_at = utc_now()
    result_dir = args.result_dir or DEFAULT_RESULT_ROOT / safe_timestamp()
    entries = load_workload(args.workload, args.max_transactions)

    if not args.submit:
        result = build_result(args, entries, [], 0, None, None, None, None, started_at)
        write_outputs(result, result_dir)
        print(json.dumps({
            "status": "warn",
            "mode": "dry-run",
            "transactions": len(entries),
            "required_confirmation": confirmation_phrase(
                len(entries), args.rpc_url, args.expected_chain_id or "<CHAIN_ID>"
            ),
            "result_dir": str(result_dir),
        }, indent=2))
        return 0

    if not args.expected_chain_id:
        raise BenchmarkError("--submit requires --expected-chain-id")
    expected_confirmation = confirmation_phrase(len(entries), args.rpc_url, args.expected_chain_id)
    if args.confirm != expected_confirmation:
        raise BenchmarkError(f"confirmation mismatch; expected exactly: {expected_confirmation}")

    chain_payload, _ = rpc_call(args.rpc_url, "chain.get_chain_id", {}, args.rpc_timeout)
    chain_id = extract_chain_id(chain_payload)
    if chain_id != args.expected_chain_id:
        raise BenchmarkError(f"chain ID mismatch: expected {args.expected_chain_id}, observed {chain_id}")

    if args.verify_inclusion:
        witness_payload, _ = rpc_call(args.witness_rpc_url, "chain.get_chain_id", {}, args.rpc_timeout)
        witness_chain_id = extract_chain_id(witness_payload)
        if witness_chain_id != args.expected_chain_id:
            raise BenchmarkError(
                f"witness chain ID mismatch: expected {args.expected_chain_id}, observed {witness_chain_id}"
            )

    start_height = (
        get_head(args.witness_rpc_url or args.rpc_url, args.rpc_timeout)[0] + 1
        if args.verify_inclusion
        else 0
    )
    pending_before = pending_count(args.rpc_url, args.rpc_timeout)
    submissions, submission_elapsed = submit_workload(entries, args)
    pending_after = pending_count(args.rpc_url, args.rpc_timeout)
    accepted_ids = [row.transaction_id for row in submissions if row.accepted]
    inclusion = None
    if args.verify_inclusion and accepted_ids:
        inclusion = verify_inclusion(
            accepted_ids,
            args.witness_rpc_url,
            start_height,
            submission_elapsed,
            args.inclusion_timeout,
            args.poll_interval,
            args.rpc_timeout,
        )
    result = build_result(
        args,
        entries,
        submissions,
        submission_elapsed,
        chain_id,
        pending_before,
        pending_after,
        inclusion,
        started_at,
    )
    write_outputs(result, result_dir)
    print(json.dumps({
        "status": result["status"],
        "accepted_tps": result["load"]["accepted_tps"],
        "accepted": result["load"]["accepted"],
        "rejected": result["load"]["rejected"],
        "confirmed_tps": inclusion["confirmed_tps"] if inclusion else None,
        "result_dir": str(result_dir),
    }, indent=2))
    return 0 if result["status"] != "fail" else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except BenchmarkError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2) from exc
