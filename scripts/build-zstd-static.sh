#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_ROOT="${KOINOS_DEPS_ROOT:-$ROOT_DIR/.deps/teleno-node}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

ZSTD_VERSION="${ZSTD_VERSION:-1.5.7}"
ZSTD_SOURCE_URL="${ZSTD_SOURCE_URL:-https://github.com/facebook/zstd/archive/refs/tags/v$ZSTD_VERSION.tar.gz}"
ZSTD_SOURCE_SHA256="${ZSTD_SOURCE_SHA256:-37d7284556b20954e56e1ca85b80226768902e2edabd3b649e9e72c0c9012ee3}"
ZSTD_SOURCE_TARBALL="${ZSTD_SOURCE_TARBALL:-}"
ZSTD_INSTALL_DIR="${ZSTD_INSTALL_DIR:-$DEPS_ROOT/zstd-static-$ZSTD_VERSION}"
ZSTD_BUILD_DIR="${ZSTD_BUILD_DIR:-$DEPS_ROOT/zstd-static-build}"
ZSTD_SOURCE_DIR="${ZSTD_SOURCE_DIR:-$DEPS_ROOT/zstd-static-src}"
ZSTD_DOWNLOAD_DIR="${ZSTD_DOWNLOAD_DIR:-$DEPS_ROOT/downloads}"
ZSTD_FORCE_REBUILD="${ZSTD_FORCE_REBUILD:-0}"
TELENO_MACOS_DEPLOYMENT_TARGET="${TELENO_MACOS_DEPLOYMENT_TARGET:-${CMAKE_OSX_DEPLOYMENT_TARGET:-13.3}}"

sha256_file() {
  shasum -a 256 "$1" | awk '{print $1}'
}

verify_tarball() {
  local tarball="$1"
  local actual
  [[ -f "$tarball" ]] || return 1
  actual="$(sha256_file "$tarball")"
  if [[ "$actual" != "$ZSTD_SOURCE_SHA256" ]]; then
    echo "zstd source checksum mismatch: $tarball" >&2
    echo "expected: $ZSTD_SOURCE_SHA256" >&2
    echo "actual:   $actual" >&2
    return 1
  fi
}

find_cached_tarball() {
  local candidate
  for candidate in \
    "$ZSTD_DOWNLOAD_DIR/zstd-$ZSTD_VERSION.tar.gz" \
    "$DEPS_ROOT/zstd-$ZSTD_VERSION.tar.gz"; do
    if verify_tarball "$candidate" 2>/dev/null; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  if [[ -d "${HOMEBREW_CACHE:-$HOME/Library/Caches/Homebrew}" ]]; then
    while IFS= read -r candidate; do
      if verify_tarball "$candidate" 2>/dev/null; then
        printf '%s\n' "$candidate"
        return 0
      fi
    done < <(find "${HOMEBREW_CACHE:-$HOME/Library/Caches/Homebrew}" -maxdepth 3 -type f -name "*zstd*$ZSTD_VERSION*.tar.gz" -print 2>/dev/null)
  fi

  return 1
}

resolve_source_tarball() {
  local tarball
  if [[ -n "$ZSTD_SOURCE_TARBALL" ]]; then
    verify_tarball "$ZSTD_SOURCE_TARBALL" || exit 1
    printf '%s\n' "$ZSTD_SOURCE_TARBALL"
    return
  fi

  if tarball="$(find_cached_tarball)"; then
    printf '%s\n' "$tarball"
    return
  fi

  mkdir -p "$ZSTD_DOWNLOAD_DIR"
  tarball="$ZSTD_DOWNLOAD_DIR/zstd-$ZSTD_VERSION.tar.gz"
  echo "==> Downloading zstd $ZSTD_VERSION source" >&2
  curl -L --fail --retry 3 --output "$tarball" "$ZSTD_SOURCE_URL"
  verify_tarball "$tarball" || exit 1
  printf '%s\n' "$tarball"
}

if [[ "$ZSTD_FORCE_REBUILD" != "1" \
      && -f "$ZSTD_INSTALL_DIR/include/zstd.h" \
      && -f "$ZSTD_INSTALL_DIR/lib/libzstd.a" ]]; then
  echo "==> Reusing static zstd install: $ZSTD_INSTALL_DIR"
  exit 0
fi

ZSTD_SOURCE_TARBALL="$(resolve_source_tarball)"

echo "==> Building static zstd"
echo "source: $ZSTD_SOURCE_TARBALL"
echo "install: $ZSTD_INSTALL_DIR"
echo "macos deployment target: $TELENO_MACOS_DEPLOYMENT_TARGET"

rm -rf "$ZSTD_SOURCE_DIR" "$ZSTD_BUILD_DIR" "$ZSTD_INSTALL_DIR"
mkdir -p "$ZSTD_SOURCE_DIR"
tar -xzf "$ZSTD_SOURCE_TARBALL" -C "$ZSTD_SOURCE_DIR"

ZSTD_EXTRACTED_SOURCE_DIR="$(find "$ZSTD_SOURCE_DIR" -mindepth 1 -maxdepth 1 -type d -print -quit)"
[[ -n "$ZSTD_EXTRACTED_SOURCE_DIR" ]] || {
  echo "failed to locate extracted zstd source under $ZSTD_SOURCE_DIR" >&2
  exit 1
}

OSX_ARGS=()
if [[ "$(uname -s)" == "Darwin" && -n "$TELENO_MACOS_DEPLOYMENT_TARGET" ]]; then
  OSX_ARGS+=( "-DCMAKE_OSX_DEPLOYMENT_TARGET=$TELENO_MACOS_DEPLOYMENT_TARGET" )
fi
if [[ -n "${CMAKE_OSX_ARCHITECTURES:-}" ]]; then
  OSX_ARGS+=( "-DCMAKE_OSX_ARCHITECTURES=$CMAKE_OSX_ARCHITECTURES" )
fi

cmake -S "$ZSTD_EXTRACTED_SOURCE_DIR/build/cmake" -B "$ZSTD_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$ZSTD_INSTALL_DIR" \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=OFF \
  -DZSTD_BUILD_STATIC=ON \
  -DZSTD_BUILD_SHARED=OFF \
  -DZSTD_BUILD_PROGRAMS=OFF \
  -DZSTD_BUILD_TESTS=OFF \
  -DZSTD_BUILD_CONTRIB=OFF \
  ${OSX_ARGS[@]+"${OSX_ARGS[@]}"}

cmake --build "$ZSTD_BUILD_DIR" --target install --parallel "$JOBS"

[[ -f "$ZSTD_INSTALL_DIR/include/zstd.h" ]] || {
  echo "zstd install did not produce include/zstd.h" >&2
  exit 1
}
[[ -f "$ZSTD_INSTALL_DIR/lib/libzstd.a" ]] || {
  echo "zstd install did not produce lib/libzstd.a" >&2
  exit 1
}

echo "==> static zstd install ready: $ZSTD_INSTALL_DIR"
