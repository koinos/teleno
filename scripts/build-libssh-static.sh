#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_ROOT="${KOINOS_DEPS_ROOT:-$ROOT_DIR/.deps/teleno-node}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

LIBSSH_VERSION="${LIBSSH_VERSION:-0.12.0}"
LIBSSH_SOURCE_URL="${LIBSSH_SOURCE_URL:-https://www.libssh.org/files/0.12/libssh-$LIBSSH_VERSION.tar.xz}"
LIBSSH_SOURCE_SHA256="${LIBSSH_SOURCE_SHA256:-1a6af424d8327e5eedef4e5fe7f5b924226dd617ac9f3de80f217d82a36a7121}"
LIBSSH_SOURCE_TARBALL="${LIBSSH_SOURCE_TARBALL:-}"
LIBSSH_INSTALL_DIR="${LIBSSH_INSTALL_DIR:-$DEPS_ROOT/libssh-static-$LIBSSH_VERSION}"
LIBSSH_BUILD_DIR="${LIBSSH_BUILD_DIR:-$DEPS_ROOT/libssh-static-build}"
LIBSSH_SOURCE_DIR="${LIBSSH_SOURCE_DIR:-$DEPS_ROOT/libssh-static-src}"
LIBSSH_DOWNLOAD_DIR="${LIBSSH_DOWNLOAD_DIR:-$DEPS_ROOT/downloads}"
LIBSSH_FORCE_REBUILD="${LIBSSH_FORCE_REBUILD:-0}"
TELENO_MACOS_DEPLOYMENT_TARGET="${TELENO_MACOS_DEPLOYMENT_TARGET:-${CMAKE_OSX_DEPLOYMENT_TARGET:-13.3}}"

sha256_file() {
  shasum -a 256 "$1" | awk '{print $1}'
}

verify_tarball() {
  local tarball="$1"
  local actual
  [[ -f "$tarball" ]] || return 1
  actual="$(sha256_file "$tarball")"
  if [[ "$actual" != "$LIBSSH_SOURCE_SHA256" ]]; then
    echo "libssh source checksum mismatch: $tarball" >&2
    echo "expected: $LIBSSH_SOURCE_SHA256" >&2
    echo "actual:   $actual" >&2
    return 1
  fi
}

find_cached_tarball() {
  local candidate
  for candidate in \
    "$LIBSSH_DOWNLOAD_DIR/libssh-$LIBSSH_VERSION.tar.xz" \
    "$DEPS_ROOT/libssh-$LIBSSH_VERSION.tar.xz"; do
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
    done < <(find "${HOMEBREW_CACHE:-$HOME/Library/Caches/Homebrew}" -maxdepth 3 -type f -name "*libssh*$LIBSSH_VERSION*.tar.xz" -print 2>/dev/null)
  fi

  return 1
}

resolve_source_tarball() {
  local tarball
  if [[ -n "$LIBSSH_SOURCE_TARBALL" ]]; then
    verify_tarball "$LIBSSH_SOURCE_TARBALL" || exit 1
    printf '%s\n' "$LIBSSH_SOURCE_TARBALL"
    return
  fi

  if tarball="$(find_cached_tarball)"; then
    printf '%s\n' "$tarball"
    return
  fi

  mkdir -p "$LIBSSH_DOWNLOAD_DIR"
  tarball="$LIBSSH_DOWNLOAD_DIR/libssh-$LIBSSH_VERSION.tar.xz"
  echo "==> Downloading libssh $LIBSSH_VERSION source" >&2
  curl -L --fail --retry 3 --output "$tarball" "$LIBSSH_SOURCE_URL"
  verify_tarball "$tarball" || exit 1
  printf '%s\n' "$tarball"
}

if [[ "$LIBSSH_FORCE_REBUILD" != "1" \
      && -f "$LIBSSH_INSTALL_DIR/include/libssh/libssh.h" \
      && -f "$LIBSSH_INSTALL_DIR/include/libssh/sftp.h" \
      && -f "$LIBSSH_INSTALL_DIR/lib/libssh.a" \
      && -f "$LIBSSH_INSTALL_DIR/lib/cmake/libssh/libssh-config.cmake" ]]; then
  echo "==> Reusing static libssh install: $LIBSSH_INSTALL_DIR"
  exit 0
fi

LIBSSH_SOURCE_TARBALL="$(resolve_source_tarball)"

echo "==> Building static libssh"
echo "source: $LIBSSH_SOURCE_TARBALL"
echo "install: $LIBSSH_INSTALL_DIR"
echo "macos deployment target: $TELENO_MACOS_DEPLOYMENT_TARGET"

rm -rf "$LIBSSH_SOURCE_DIR" "$LIBSSH_BUILD_DIR" "$LIBSSH_INSTALL_DIR"
mkdir -p "$LIBSSH_SOURCE_DIR"
tar -xf "$LIBSSH_SOURCE_TARBALL" -C "$LIBSSH_SOURCE_DIR"

LIBSSH_EXTRACTED_SOURCE_DIR="$(find "$LIBSSH_SOURCE_DIR" -mindepth 1 -maxdepth 1 -type d -print -quit)"
[[ -n "$LIBSSH_EXTRACTED_SOURCE_DIR" ]] || {
  echo "failed to locate extracted libssh source under $LIBSSH_SOURCE_DIR" >&2
  exit 1
}

# libssh 0.12.0 discovers ABIMap unconditionally on UNIX, and that can hang in
# Python discovery on old local pkg/python setups. Static release builds disable
# symbol versioning, so skip ABIMap unless WITH_SYMBOL_VERSIONING remains ON.
perl -0pi -e 's/# Disable symbol versioning in non UNIX platforms\nif \(UNIX\)\n    find_package\(ABIMap 0\.4\.0\)\nelse \(UNIX\)\n    set\(WITH_SYMBOL_VERSIONING OFF\)\nendif \(UNIX\)\n/# Disable symbol versioning in non UNIX platforms and skip ABIMap when disabled.\nif (UNIX AND WITH_SYMBOL_VERSIONING)\n    find_package(ABIMap 0.4.0)\nelse ()\n    set(WITH_SYMBOL_VERSIONING OFF)\nendif ()\n/' \
  "$LIBSSH_EXTRACTED_SOURCE_DIR/CMakeLists.txt"
if ! grep -q "UNIX AND WITH_SYMBOL_VERSIONING" "$LIBSSH_EXTRACTED_SOURCE_DIR/CMakeLists.txt"; then
  echo "libssh CMake ABIMap block did not match expected source" >&2
  exit 1
fi

OSX_ARGS=()
if [[ "$(uname -s)" == "Darwin" && -n "$TELENO_MACOS_DEPLOYMENT_TARGET" ]]; then
  OSX_ARGS+=( "-DCMAKE_OSX_DEPLOYMENT_TARGET=$TELENO_MACOS_DEPLOYMENT_TARGET" )
fi
if [[ -n "${CMAKE_OSX_ARCHITECTURES:-}" ]]; then
  OSX_ARGS+=( "-DCMAKE_OSX_ARCHITECTURES=$CMAKE_OSX_ARCHITECTURES" )
fi

OPENSSL_ARGS=()
if [[ -n "${OPENSSL_ROOT_DIR:-}" ]]; then
  OPENSSL_ARGS+=( "-DOPENSSL_ROOT_DIR=$OPENSSL_ROOT_DIR" )
fi
if [[ -n "${OPENSSL_INCLUDE_DIR:-}" ]]; then
  OPENSSL_ARGS+=( "-DOPENSSL_INCLUDE_DIR=$OPENSSL_INCLUDE_DIR" )
fi
if [[ -n "${OPENSSL_CRYPTO_LIBRARY:-}" ]]; then
  OPENSSL_ARGS+=( "-DOPENSSL_CRYPTO_LIBRARY=$OPENSSL_CRYPTO_LIBRARY" )
fi
if [[ -n "${OPENSSL_SSL_LIBRARY:-}" ]]; then
  OPENSSL_ARGS+=( "-DOPENSSL_SSL_LIBRARY=$OPENSSL_SSL_LIBRARY" )
fi

ZLIB_ARGS=()
if [[ -n "${ZLIB_ROOT:-}" ]]; then
  ZLIB_ARGS+=( "-DZLIB_ROOT=$ZLIB_ROOT" )
fi
if [[ -n "${ZLIB_INCLUDE_DIR:-}" ]]; then
  ZLIB_ARGS+=( "-DZLIB_INCLUDE_DIR=$ZLIB_INCLUDE_DIR" )
fi
if [[ -n "${ZLIB_LIBRARY:-}" ]]; then
  ZLIB_ARGS+=( "-DZLIB_LIBRARY=$ZLIB_LIBRARY" )
fi

cmake -S "$LIBSSH_EXTRACTED_SOURCE_DIR" -B "$LIBSSH_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$LIBSSH_INSTALL_DIR" \
  -DPKG_CONFIG_EXECUTABLE=/usr/bin/false \
  -DBUILD_SHARED_LIBS=OFF \
  -DWITH_SFTP=ON \
  -DWITH_ZLIB=ON \
  -DWITH_GSSAPI=OFF \
  -DWITH_SERVER=OFF \
  -DWITH_EXAMPLES=OFF \
  -DUNIT_TESTING=OFF \
  -DCLIENT_TESTING=OFF \
  -DSERVER_TESTING=OFF \
  -DGSSAPI_TESTING=OFF \
  -DWITH_BENCHMARKS=OFF \
  -DWITH_NACL=OFF \
  -DWITH_FIDO2=OFF \
  -DWITH_PKCS11_URI=OFF \
  -DWITH_PKCS11_PROVIDER=OFF \
  -DWITH_PCAP=OFF \
  -DWITH_EXEC=OFF \
  -DWITH_SYMBOL_VERSIONING=OFF \
  -DWITH_INTERNAL_DOC=OFF \
  ${OPENSSL_ARGS[@]+"${OPENSSL_ARGS[@]}"} \
  ${ZLIB_ARGS[@]+"${ZLIB_ARGS[@]}"} \
  ${OSX_ARGS[@]+"${OSX_ARGS[@]}"}

cmake --build "$LIBSSH_BUILD_DIR" --target install --parallel "$JOBS"

[[ -f "$LIBSSH_INSTALL_DIR/include/libssh/libssh.h" ]] || {
  echo "libssh install did not produce include/libssh/libssh.h" >&2
  exit 1
}
[[ -f "$LIBSSH_INSTALL_DIR/include/libssh/sftp.h" ]] || {
  echo "libssh install did not produce include/libssh/sftp.h" >&2
  exit 1
}
[[ -f "$LIBSSH_INSTALL_DIR/lib/libssh.a" ]] || {
  echo "libssh install did not produce lib/libssh.a" >&2
  exit 1
}
[[ -f "$LIBSSH_INSTALL_DIR/lib/cmake/libssh/libssh-config.cmake" ]] || {
  echo "libssh install did not produce lib/cmake/libssh/libssh-config.cmake" >&2
  exit 1
}

echo "==> static libssh install ready: $LIBSSH_INSTALL_DIR"
