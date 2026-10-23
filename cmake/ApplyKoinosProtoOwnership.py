"""Patch only the archive-verified, build-owned generated Koinos messages."""
import hashlib
import json
from pathlib import Path
import re
import sys

root, manifest_path = map(Path, sys.argv[1:3])
manifest = json.loads(manifest_path.read_text())
pattern = re.compile(
    r"(?m)^([A-Za-z_]\w*)::\1\(const \1& from\)\n.*?"
    r"  // @@protoc_insertion_point\(copy_constructor:([^\n)]+)\)\n}", re.S)
oneof_pattern = re.compile(
    r"(?m)^([ \t]*)clear_(\w+)\(\);\n\1set_has_(\w+)\(\);\n"
    r"\1(\w+\.\w+_) = (CreateMaybeMessage<[^;\n]+);"
)
inputs = {name: (root / name).read_bytes() for name in manifest}
digest = lambda data: hashlib.sha256(data).hexdigest()
if all(digest(inputs[name]) == item["after"] for name, item in manifest.items()):
    sys.exit(0)
if not all(digest(inputs[name]) == item["before"] for name, item in manifest.items()):
    sys.exit("Unexpected generated Koinos source or mixed ownership patch state")

def replacement(match):
    name, descriptor = match.groups()
    return (f"{name}::{name}(const {name}& from)\n  : {name}() {{\n"
            "  // Complete the target first so every field is destroyed if copying throws.\n"
            "  CopyFrom(from);\n"
            f"  // @@protoc_insertion_point(copy_constructor:{descriptor})\n}}")

def oneof_replacement(match):
    indent, group, field, target, create = match.groups()
    return (f"{indent}auto* new_value = {create};\n"
            f"{indent}clear_{group}();\n"
            f"{indent}{target} = new_value;\n"
            f"{indent}set_has_{field}();")

outputs = {}
for name, item in manifest.items():
    output, count = pattern.subn(replacement, inputs[name].decode())
    output, oneof_count = oneof_pattern.subn(oneof_replacement, output)
    outputs[name] = output.encode()
    if (count != item["constructors"] or oneof_count != item.get("oneof_messages", 0)
            or digest(outputs[name]) != item["after"]):
        sys.exit(f"Generated Koinos ownership patch result mismatch: {name}")
# Verify the entire result before changing any file.
for name, data in outputs.items():
    (root / name).write_bytes(data)
