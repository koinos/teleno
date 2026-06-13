#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_ROOT="${KOINOS_DEPS_ROOT:-$ROOT_DIR/.deps/teleno-node}"
HUNTER_ROOT="${HUNTER_ROOT:-$DEPS_ROOT/hunter}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

ROCKSDB_ZSTD_INSTALL_DIR="${ROCKSDB_ZSTD_INSTALL_DIR:-$DEPS_ROOT/rocksdb-zstd-8.8.1}"
ROCKSDB_ZSTD_BUILD_DIR="${ROCKSDB_ZSTD_BUILD_DIR:-$DEPS_ROOT/rocksdb-zstd-build}"
ROCKSDB_ZSTD_SOURCE_DIR="${ROCKSDB_ZSTD_SOURCE_DIR:-$DEPS_ROOT/rocksdb-zstd-src}"
ROCKSDB_ZSTD_FORCE_REBUILD="${ROCKSDB_ZSTD_FORCE_REBUILD:-0}"
ROCKSDB_SOURCE_TARBALL="${ROCKSDB_SOURCE_TARBALL:-}"
ROCKSDB_ZSTD_DEPENDENCY_MARKER="$ROCKSDB_ZSTD_INSTALL_DIR/.teleno-zstd-dependency"
TELENO_BUILD_LOCAL_ZSTD="${TELENO_BUILD_LOCAL_ZSTD:-1}"
ZSTD_VERSION="${ZSTD_VERSION:-1.5.7}"
ZSTD_INSTALL_DIR="${ZSTD_INSTALL_DIR:-$DEPS_ROOT/zstd-static-$ZSTD_VERSION}"

require_file() {
  [[ -f "$1" ]] || {
    echo "missing required file: $1" >&2
    exit 1
  }
}

find_rocksdb_tarball() {
  local tarball
  if [[ -n "$ROCKSDB_SOURCE_TARBALL" ]]; then
    [[ -f "$ROCKSDB_SOURCE_TARBALL" ]] || {
      echo "ROCKSDB_SOURCE_TARBALL does not exist: $ROCKSDB_SOURCE_TARBALL" >&2
      exit 1
    }
    printf '%s\n' "$ROCKSDB_SOURCE_TARBALL"
    return
  fi

  tarball="$(find "$HUNTER_ROOT/_Base/Download/rocksdb" -maxdepth 5 -type f -name '*.tar.gz' -print -quit 2>/dev/null || true)"
  if [[ -n "$tarball" ]]; then
    printf '%s\n' "$tarball"
    return
  fi

  echo "could not find cached RocksDB source tarball under $HUNTER_ROOT/_Base/Download/rocksdb" >&2
  echo "run the Hunter dependency bootstrap first or pass ROCKSDB_SOURCE_TARBALL=/path/to/rocksdb.tar.gz" >&2
  exit 1
}

detect_zstd_paths() {
  ZSTD_INCLUDE_DIRS="${ZSTD_INCLUDE_DIRS:-${ZSTD_INCLUDE_DIR:-}}"
  ZSTD_LIBRARIES="${ZSTD_LIBRARIES:-${ZSTD_LIBRARY:-}}"

  if [[ -n "$ZSTD_INCLUDE_DIRS" && -n "$ZSTD_LIBRARIES" ]]; then
    [[ -f "$ZSTD_INCLUDE_DIRS/zstd.h" ]] || {
      echo "zstd.h not found under ZSTD_INCLUDE_DIRS=$ZSTD_INCLUDE_DIRS" >&2
      exit 1
    }
    [[ -f "$ZSTD_LIBRARIES" ]] || {
      echo "zstd library not found: $ZSTD_LIBRARIES" >&2
      exit 1
    }
    return
  fi

  local root
  for root in "${ZSTD_ROOT:-}" /opt/homebrew /usr/local; do
    [[ -n "$root" ]] || continue
    if [[ -f "$root/include/zstd.h" ]]; then
      ZSTD_INCLUDE_DIRS="$root/include"
      if [[ -f "$root/lib/libzstd.a" ]]; then
        ZSTD_LIBRARIES="$root/lib/libzstd.a"
      elif [[ -f "$root/lib/libzstd.dylib" ]]; then
        ZSTD_LIBRARIES="$root/lib/libzstd.dylib"
      fi
      if [[ -n "$ZSTD_LIBRARIES" ]]; then
        return
      fi
    fi
  done

  echo "could not find zstd headers/library; set ZSTD_INCLUDE_DIRS and ZSTD_LIBRARIES" >&2
  exit 1
}

zstd_library_sha256() {
  shasum -a 256 "$ZSTD_LIBRARIES" | awk '{print $1}'
}

write_zstd_dependency_marker() {
  {
    echo "zstd_include=$ZSTD_INCLUDE_DIRS"
    echo "zstd_library=$ZSTD_LIBRARIES"
    echo "zstd_sha256=$(zstd_library_sha256)"
  } > "$ROCKSDB_ZSTD_DEPENDENCY_MARKER"
}

zstd_dependency_marker_matches() {
  [[ -f "$ROCKSDB_ZSTD_DEPENDENCY_MARKER" ]] || return 1
  grep -Fxq "zstd_include=$ZSTD_INCLUDE_DIRS" "$ROCKSDB_ZSTD_DEPENDENCY_MARKER" || return 1
  grep -Fxq "zstd_library=$ZSTD_LIBRARIES" "$ROCKSDB_ZSTD_DEPENDENCY_MARKER" || return 1
  grep -Fxq "zstd_sha256=$(zstd_library_sha256)" "$ROCKSDB_ZSTD_DEPENDENCY_MARKER" || return 1
}

prepare_zstd_dependency() {
  local explicit_paths=0
  if [[ -n "${ZSTD_INCLUDE_DIRS:-${ZSTD_INCLUDE_DIR:-}}" || -n "${ZSTD_LIBRARIES:-${ZSTD_LIBRARY:-}}" ]]; then
    explicit_paths=1
  fi

  if [[ "$TELENO_BUILD_LOCAL_ZSTD" != "0" && "$explicit_paths" == "0" ]]; then
    require_file "$ROOT_DIR/scripts/build-zstd-static.sh"
    KOINOS_DEPS_ROOT="$DEPS_ROOT" \
      ZSTD_VERSION="$ZSTD_VERSION" \
      ZSTD_INSTALL_DIR="$ZSTD_INSTALL_DIR" \
      JOBS="$JOBS" \
      "$ROOT_DIR/scripts/build-zstd-static.sh"
    ZSTD_INCLUDE_DIRS="$ZSTD_INSTALL_DIR/include"
    ZSTD_LIBRARIES="$ZSTD_INSTALL_DIR/lib/libzstd.a"
  fi

  detect_zstd_paths
}

prepare_zstd_dependency

if [[ "$ROCKSDB_ZSTD_FORCE_REBUILD" != "1" \
      && -f "$ROCKSDB_ZSTD_INSTALL_DIR/lib/cmake/rocksdb/RocksDBConfig.cmake" \
      && -f "$ROCKSDB_ZSTD_INSTALL_DIR/lib/librocksdb.a" \
      && zstd_dependency_marker_matches ]]; then
  echo "==> Reusing zstd-enabled RocksDB install: $ROCKSDB_ZSTD_INSTALL_DIR"
  echo "zstd include: $ZSTD_INCLUDE_DIRS"
  echo "zstd library: $ZSTD_LIBRARIES"
  exit 0
fi

ROCKSDB_SOURCE_TARBALL="$(find_rocksdb_tarball)"

echo "==> Building zstd-enabled RocksDB"
echo "source: $ROCKSDB_SOURCE_TARBALL"
echo "install: $ROCKSDB_ZSTD_INSTALL_DIR"
echo "zstd include: $ZSTD_INCLUDE_DIRS"
echo "zstd library: $ZSTD_LIBRARIES"

rm -rf "$ROCKSDB_ZSTD_SOURCE_DIR" "$ROCKSDB_ZSTD_BUILD_DIR" "$ROCKSDB_ZSTD_INSTALL_DIR"
mkdir -p "$ROCKSDB_ZSTD_SOURCE_DIR"
tar -xzf "$ROCKSDB_SOURCE_TARBALL" -C "$ROCKSDB_ZSTD_SOURCE_DIR"

ROCKSDB_SOURCE_DIR="$(find "$ROCKSDB_ZSTD_SOURCE_DIR" -mindepth 1 -maxdepth 1 -type d -print -quit)"
[[ -n "$ROCKSDB_SOURCE_DIR" ]] || {
  echo "failed to locate extracted RocksDB source under $ROCKSDB_ZSTD_SOURCE_DIR" >&2
  exit 1
}

OSX_ARGS=()
if [[ -n "${CMAKE_OSX_DEPLOYMENT_TARGET:-}" ]]; then
  OSX_ARGS+=( "-DCMAKE_OSX_DEPLOYMENT_TARGET=$CMAKE_OSX_DEPLOYMENT_TARGET" )
fi
if [[ -n "${CMAKE_OSX_ARCHITECTURES:-}" ]]; then
  OSX_ARGS+=( "-DCMAKE_OSX_ARCHITECTURES=$CMAKE_OSX_ARCHITECTURES" )
fi

cmake -S "$ROCKSDB_SOURCE_DIR" -B "$ROCKSDB_ZSTD_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$ROCKSDB_ZSTD_INSTALL_DIR" \
  -DCMAKE_PREFIX_PATH="${ZSTD_ROOT:-${ZSTD_INCLUDE_DIRS%/include}}" \
  -DWITH_ZSTD=ON \
  -Dzstd_LIBRARIES="$ZSTD_LIBRARIES" \
  -Dzstd_INCLUDE_DIRS="$ZSTD_INCLUDE_DIRS" \
  -DWITH_TESTS=OFF \
  -DWITH_GFLAGS=OFF \
  -DWITH_BENCHMARK_TOOLS=OFF \
  -DWITH_CORE_TOOLS=OFF \
  -DWITH_TOOLS=OFF \
  -DPORTABLE=ON \
  -DFAIL_ON_WARNINGS=OFF \
  -DROCKSDB_BUILD_SHARED=OFF \
  ${OSX_ARGS[@]+"${OSX_ARGS[@]}"}

cmake --build "$ROCKSDB_ZSTD_BUILD_DIR" --target install --parallel "$JOBS"
write_zstd_dependency_marker

echo "==> zstd-enabled RocksDB install ready: $ROCKSDB_ZSTD_INSTALL_DIR"
