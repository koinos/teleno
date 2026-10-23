#!/usr/bin/env python3
"""Verify installed inputs and stage the cleanup backport in an owned build tree."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def within(child, parent):
    try:
        child.relative_to(parent)
        return True
    except ValueError:
        return False


def headers(root):
    return {
        path.relative_to(root).as_posix(): digest(path)
        for path in sorted(root.rglob("*"))
        if path.is_file()
    }


def stage(installed_include, output_root, manifest_path, patch_path):
    input_root = installed_include.resolve(strict=True)
    installed = (input_root / "nlohmann").resolve(strict=True)
    output = output_root.resolve()
    if within(output, input_root) or within(input_root, output):
        raise ValueError("Build output and installed JSON headers must be separate")
    manifest = json.loads(manifest_path.read_text())
    before = manifest["original_headers"]
    if headers(installed) != before:
        raise ValueError("Unexpected installed nlohmann headers; refusing cleanup backport")
    if digest(patch_path) != manifest["patch_sha256"]:
        raise ValueError("Unexpected JSON cleanup patch")
    after = dict(before)
    after.update(manifest["patched_headers"])
    if output.exists():
        if headers(output / "include/nlohmann") != after:
            raise ValueError("Unexpected or mixed existing build-owned JSON headers")
        return

    staging = output.with_name(output.name + ".staging")
    if staging.exists():
        raise ValueError("Prior JSON staging exists; preserve it and use a new owned output")
    staging.mkdir(parents=True)
    destination = staging / "include/nlohmann"
    for name in before:
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(installed / name, target)

    # Do not let an enclosing checkout or inherited Git worktree/index redirect
    # a dependency patch. Every installed byte remains a read-only input.
    environment = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}
    environment["GIT_CEILING_DIRECTORIES"] = str(staging.parent)
    patch = str(patch_path.resolve(strict=True))
    for arguments in (["--check", patch], [patch]):
        subprocess.run(["git", "apply"] + arguments, cwd=staging, env=environment, check=True)
    if headers(destination) != after:
        raise ValueError("JSON cleanup backport does not match the pinned result")
    (staging / "provenance.json").write_text(json.dumps({
        "version": manifest["version"],
        "manifest_sha256": digest(manifest_path),
        "patch_sha256": manifest["patch_sha256"],
        "headers": after,
    }, indent=2, sort_keys=True) + "\n")
    staging.rename(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--installed-include", required=True, type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--patch", required=True, type=Path)
    arguments = parser.parse_args()
    stage(arguments.installed_include, arguments.output_root, arguments.manifest, arguments.patch)


if __name__ == "__main__":
    main()
