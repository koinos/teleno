"""Bind the pinned synchronous gRPC call before invoking interceptor factories."""
from pathlib import Path
import hashlib
import json
import sys


def apply(source: Path, manifest: Path) -> None:
    expected = json.loads(manifest.read_text())
    if list(expected) != ["src/cpp/server/server_cc.cc"]:
        raise ValueError("Unexpected gRPC call-binding manifest")
    relative, hashes = next(iter(expected.items()))
    path = source / relative
    data = path.read_bytes()
    actual = hashlib.sha256(data).hexdigest()
    if actual == hashes["after"]:
        return
    if actual != hashes["before"]:
        raise ValueError("Pinned gRPC synchronous server source hash mismatch")
    old = b"""              ctx_.set_server_rpc_info(method_->name(), method_->method_type(),
                                       server->interceptor_creators_)),"""
    new = b"""              (ctx_.set_call(mrd->call_),
               ctx_.set_server_rpc_info(method_->name(), method_->method_type(),
                                        server->interceptor_creators_))),"""
    body = b"""          resources_(false) {
      ctx_.set_call(mrd->call_);
      ctx_.cq_ = &cq_;"""
    replacement = b"""          resources_(false) {
      ctx_.cq_ = &cq_;"""
    if data.count(old) != 1 or data.count(body) != 1:
        raise ValueError("Pinned gRPC call-binding sites changed")
    result = data.replace(old, new).replace(body, replacement)
    if hashlib.sha256(result).hexdigest() != hashes["after"]:
        raise ValueError("Patched gRPC source hash mismatch")
    path.write_bytes(result)


if __name__ == "__main__":
    try:
        if len(sys.argv) != 3:
            raise ValueError("Expected source directory and manifest")
        apply(Path(sys.argv[1]), Path(sys.argv[2]))
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(str(error))
