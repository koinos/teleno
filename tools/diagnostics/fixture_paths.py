"""Explicit test-only artifact roots; source and runtime restore policy are unchanged."""
import os
from pathlib import Path


def artifact_root(repository):
    selected = os.environ.get("TELENO_TEST_ARTIFACT_ROOT")
    root = Path(selected) if selected is not None else repository / "build-single-copy-evidence"
    if not root.is_absolute() or ".." in root.parts or root.resolve() == Path(root.anchor) or root.is_symlink():
        raise RuntimeError("test artifact root must be absolute, non-root and not a symlink")
    if selected is not None and not root.is_dir():
        raise RuntimeError("explicit test artifact root must already exist")
    return root.resolve()


def build_is_authorized(build, repository):
    build = build.resolve()
    if build == Path(build.anchor):
        return False
    if build.is_relative_to(repository.resolve()):
        return True
    selected = os.environ.get("TELENO_TEST_BUILD_ROOT")
    # External builds require both explicit selections; never infer permission
    # from a binary's location or relax CMake source correspondence checks.
    return (os.environ.get("TELENO_TEST_ARTIFACT_ROOT") is not None and selected is not None
            and Path(selected).is_absolute() and Path(selected).resolve() == build
            and artifact_root(repository).is_dir())
