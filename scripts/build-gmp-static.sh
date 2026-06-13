#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_ROOT="${KOINOS_DEPS_ROOT:-$ROOT_DIR/.deps/teleno-node}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

GMP_VERSION="${GMP_VERSION:-6.3.0}"
GMP_SOURCE_URL="${GMP_SOURCE_URL:-https://ftpmirror.gnu.org/gnu/gmp/gmp-$GMP_VERSION.tar.xz}"
GMP_SOURCE_SHA256="${GMP_SOURCE_SHA256:-a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898}"
GMP_SOURCE_TARBALL="${GMP_SOURCE_TARBALL:-}"
GMP_INSTALL_DIR="${GMP_INSTALL_DIR:-$DEPS_ROOT/gmp-static-$GMP_VERSION}"
GMP_BUILD_DIR="${GMP_BUILD_DIR:-$DEPS_ROOT/gmp-static-build}"
GMP_SOURCE_DIR="${GMP_SOURCE_DIR:-$DEPS_ROOT/gmp-static-src}"
GMP_DOWNLOAD_DIR="${GMP_DOWNLOAD_DIR:-$DEPS_ROOT/downloads}"
GMP_FORCE_REBUILD="${GMP_FORCE_REBUILD:-0}"
TELENO_MACOS_DEPLOYMENT_TARGET="${TELENO_MACOS_DEPLOYMENT_TARGET:-${CMAKE_OSX_DEPLOYMENT_TARGET:-13.3}}"

sha256_file() {
  shasum -a 256 "$1" | awk '{print $1}'
}

verify_tarball() {
  local tarball="$1"
  local actual
  [[ -f "$tarball" ]] || return 1
  actual="$(sha256_file "$tarball")"
  if [[ "$actual" != "$GMP_SOURCE_SHA256" ]]; then
    echo "GMP source checksum mismatch: $tarball" >&2
    echo "expected: $GMP_SOURCE_SHA256" >&2
    echo "actual:   $actual" >&2
    return 1
  fi
}

find_cached_tarball() {
  local candidate
  for candidate in \
    "$GMP_DOWNLOAD_DIR/gmp-$GMP_VERSION.tar.xz" \
    "$DEPS_ROOT/gmp-$GMP_VERSION.tar.xz"; do
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
    done < <(find "${HOMEBREW_CACHE:-$HOME/Library/Caches/Homebrew}" -maxdepth 3 -type f -name "*gmp*$GMP_VERSION*.tar.xz" -print 2>/dev/null)
  fi

  return 1
}

resolve_source_tarball() {
  local tarball
  if [[ -n "$GMP_SOURCE_TARBALL" ]]; then
    verify_tarball "$GMP_SOURCE_TARBALL" || exit 1
    printf '%s\n' "$GMP_SOURCE_TARBALL"
    return
  fi

  if tarball="$(find_cached_tarball)"; then
    printf '%s\n' "$tarball"
    return
  fi

  mkdir -p "$GMP_DOWNLOAD_DIR"
  tarball="$GMP_DOWNLOAD_DIR/gmp-$GMP_VERSION.tar.xz"
  echo "==> Downloading GMP $GMP_VERSION source" >&2
  curl -L --fail --retry 3 --output "$tarball" "$GMP_SOURCE_URL"
  verify_tarball "$tarball" || exit 1
  printf '%s\n' "$tarball"
}

if [[ "$GMP_FORCE_REBUILD" != "1" \
      && -f "$GMP_INSTALL_DIR/include/gmp.h" \
      && -f "$GMP_INSTALL_DIR/lib/libgmp.a" ]]; then
  echo "==> Reusing static GMP install: $GMP_INSTALL_DIR"
  exit 0
fi

GMP_SOURCE_TARBALL="$(resolve_source_tarball)"

echo "==> Building static GMP"
echo "source: $GMP_SOURCE_TARBALL"
echo "install: $GMP_INSTALL_DIR"
echo "macos deployment target: $TELENO_MACOS_DEPLOYMENT_TARGET"

rm -rf "$GMP_SOURCE_DIR" "$GMP_BUILD_DIR" "$GMP_INSTALL_DIR"
mkdir -p "$GMP_SOURCE_DIR" "$GMP_BUILD_DIR"
tar -xf "$GMP_SOURCE_TARBALL" -C "$GMP_SOURCE_DIR"

GMP_EXTRACTED_SOURCE_DIR="$(find "$GMP_SOURCE_DIR" -mindepth 1 -maxdepth 1 -type d -print -quit)"
[[ -n "$GMP_EXTRACTED_SOURCE_DIR" ]] || {
  echo "failed to locate extracted GMP source under $GMP_SOURCE_DIR" >&2
  exit 1
}

CONFIGURE_ENV=()
if [[ "$(uname -s)" == "Darwin" && -n "$TELENO_MACOS_DEPLOYMENT_TARGET" ]]; then
  CONFIGURE_ENV+=( "MACOSX_DEPLOYMENT_TARGET=$TELENO_MACOS_DEPLOYMENT_TARGET" )
  CONFIGURE_ENV+=( "CFLAGS=-O3 -mmacosx-version-min=$TELENO_MACOS_DEPLOYMENT_TARGET" )
  CONFIGURE_ENV+=( "CXXFLAGS=-O3 -mmacosx-version-min=$TELENO_MACOS_DEPLOYMENT_TARGET" )
else
  CONFIGURE_ENV+=( "CFLAGS=-O3" )
  CONFIGURE_ENV+=( "CXXFLAGS=-O3" )
fi

(
  cd "$GMP_BUILD_DIR"
  env "${CONFIGURE_ENV[@]}" "$GMP_EXTRACTED_SOURCE_DIR/configure" \
    --prefix="$GMP_INSTALL_DIR" \
    --disable-shared \
    --enable-static \
    --with-pic
  make -j"$JOBS"
  make install
)

[[ -f "$GMP_INSTALL_DIR/include/gmp.h" ]] || {
  echo "GMP install did not produce include/gmp.h" >&2
  exit 1
}
[[ -f "$GMP_INSTALL_DIR/lib/libgmp.a" ]] || {
  echo "GMP install did not produce lib/libgmp.a" >&2
  exit 1
}

echo "==> static GMP install ready: $GMP_INSTALL_DIR"
