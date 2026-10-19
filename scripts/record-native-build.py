#!/usr/bin/env python3
"""Record an exact clean native release build without accessing runtime data."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess


def sha(path):
    with path.open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    binary = args.binary.resolve(strict=True)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    status = subprocess.check_output(["git", "status", "--porcelain"], cwd=source, text=True)
    if status:
        raise RuntimeError("Release provenance requires a clean source checkout")
    version = (source / "VERSION").read_text().strip()
    reported = subprocess.check_output([str(binary), "--version"], text=True).strip()
    if reported not in {f"teleno_node {version}+{commit}", f"teleno_node {version}+{commit[:12]}"}:
        raise RuntimeError("Native version/commit does not match the clean source")
    link_file = binary.parent / "src/CMakeFiles/teleno_node.dir/link.txt"
    archives = {}
    for token in shlex.split(link_file.read_text()):
        if not token.endswith(".a"):
            continue
        path = Path(token)
        path = path if path.is_absolute() else link_file.parents[2] / path
        path = path.resolve(strict=True)
        archives[str(path)] = {"bytes": path.stat().st_size, "sha256": sha(path)}
    if not archives:
        raise RuntimeError("No actual native link archives were recorded")
    source_files = {}
    names = subprocess.check_output(["git", "ls-files", "-z"], cwd=source).decode().split("\0")
    for name in filter(None, names):
        path = source / name
        if path.is_symlink() or not path.is_file():
            raise RuntimeError("Source manifest requires regular committed files: " + name)
        source_files[name] = {"bytes": path.stat().st_size, "sha256": sha(path)}
    result = {
        "format": "teleno-native-release-build", "version": 1,
        "native_version": version, "native_build_version": reported,
        "release_tag": "teleno-node-v" + version,
        "source_commit": commit, "source_clean": True,
        "binary": {"bytes": binary.stat().st_size, "sha256": sha(binary)},
        "compiler": subprocess.check_output(["c++", "--version"], text=True).splitlines()[0],
        "actual_link_archives": archives, "committed_source_files": source_files,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"native_build_version": reported, "source_commit": commit,
                      "binary_sha256": result["binary"]["sha256"],
                      "actual_link_archives": len(archives), "source_files": len(source_files)}))


if __name__ == "__main__":
    main()
