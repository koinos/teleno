"""Replace one object in a build-owned SDK archive; never alter the installed SDK."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def repack(archive: Path, replacement: Path, output: Path, ar: str, ranlib: str,
           expected_sha256=None) -> dict:
    if output.is_symlink() or archive.resolve() == output.resolve():
        raise ValueError("Patched archive must be a separate owned file")
    if output.name != archive.name or archive.parent.resolve() == output.parent.resolve():
        raise ValueError("Patched archive must keep its name in a separate build directory")
    members = subprocess.check_output([ar, "t", str(archive)], text=True).splitlines()
    matches = [name for name in members if name in ("server_cc.cc.o", "server_cc.cc.obj")]
    if len(matches) != 1 or len(members) != len(set(members)):
        raise ValueError("Pinned gRPC archive has missing or ambiguous object members")
    member = matches[0]
    if any(Path(name).name != name or "\\" in name for name in members):
        raise ValueError("Unsupported archive member path")
    original_hash = digest(archive)
    if expected_sha256 is not None and original_hash != expected_sha256:
        raise ValueError("Installed gRPC archive changed after configuration")
    object_hash = digest(replacement)
    original_members = {
        name: hashlib.sha256(subprocess.check_output([ar, "p", str(archive), name])).hexdigest()
        for name in members
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="grpc-call-binding-", dir=output.parent) as temporary:
        root = Path(temporary)
        owned = root / archive.name
        obj = root / member
        shutil.copyfile(archive, owned)
        shutil.copyfile(replacement, obj)
        subprocess.run([ar, "r", str(owned), str(obj)], check=True)
        subprocess.run([ranlib, str(owned)], check=True)
        if subprocess.check_output([ar, "t", str(owned)], text=True).splitlines() != members:
            raise ValueError("Patched gRPC archive member list changed")
        changed = []
        for name in members:
            actual = hashlib.sha256(subprocess.check_output([ar, "p", str(owned), name])).hexdigest()
            if actual != original_members[name]:
                changed.append(name)
            if name == member and actual != object_hash:
                raise ValueError("Replacement SDK object differs after archiving")
        if changed != [member] or digest(archive) != original_hash or digest(replacement) != object_hash:
            raise ValueError("SDK archive replacement changed unexpected inputs or members")
        identity = {
            "original_archive": str(archive.resolve()),
            "original_sha256": original_hash,
            "replacement_sha256": object_hash,
            "patched_sha256": digest(owned),
            "changed_members": changed,
            "unchanged_member_count": len(members) - 1,
        }
        metadata = root / "identity.json"
        metadata.write_text(json.dumps(identity, indent=2, sort_keys=True) + "\n")
        os.replace(owned, output)
        os.replace(metadata, output.with_suffix(output.suffix + ".identity.json"))
    return identity


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--object", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ar", required=True)
    parser.add_argument("--ranlib", required=True)
    parser.add_argument("--expected-sha256", required=True)
    args = parser.parse_args()
    try:
        result = repack(args.archive, args.object, args.output, args.ar, args.ranlib,
                        args.expected_sha256)
        print(json.dumps(result, sort_keys=True))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
