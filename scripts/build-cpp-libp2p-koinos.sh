#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NODE_DIR="${NODE_DIR:-$ROOT_DIR/node/teleno-node}"
DEPS_ROOT="${KOINOS_DEPS_ROOT:-$ROOT_DIR/.deps/teleno-node}"
HUNTER_ROOT="${HUNTER_ROOT:-$DEPS_ROOT/hunter}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

KOINOS_NODE_HUNTER_SOURCE="${KOINOS_NODE_HUNTER_SOURCE:-$DEPS_ROOT/teleno-node-hunter-src}"
KOINOS_NODE_HUNTER_BUILD="${KOINOS_NODE_HUNTER_BUILD:-$DEPS_ROOT/teleno-node-hunter-build}"
CPP_LIBP2P_SOURCE_DIR="${CPP_LIBP2P_SOURCE_DIR:-$DEPS_ROOT/cpp-libp2p-src}"
CPP_LIBP2P_HUNTER_BUILD="${CPP_LIBP2P_HUNTER_BUILD:-$DEPS_ROOT/cpp-libp2p-hunter-build}"
CPP_LIBP2P_BUILD_DIR="${CPP_LIBP2P_BUILD_DIR:-$DEPS_ROOT/cpp-libp2p-koinos-build}"
CPP_LIBP2P_INSTALL_DIR="${CPP_LIBP2P_INSTALL_DIR:-$DEPS_ROOT/cpp-libp2p-koinos}"
CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR="${CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR:-$DEPS_ROOT/cpp-libp2p-thirdparty-include}"
KOINOS_NODE_BUILD_DIR="${KOINOS_NODE_BUILD_DIR:-$NODE_DIR/build}"
TELENO_ROCKSDB_WITH_ZSTD="${TELENO_ROCKSDB_WITH_ZSTD:-1}"
ROCKSDB_ZSTD_INSTALL_DIR="${ROCKSDB_ZSTD_INSTALL_DIR:-$DEPS_ROOT/rocksdb-zstd-8.8.1}"
TELENO_BUILD_LOCAL_ZSTD="${TELENO_BUILD_LOCAL_ZSTD:-1}"
ZSTD_VERSION="${ZSTD_VERSION:-1.5.7}"
ZSTD_INSTALL_DIR="${ZSTD_INSTALL_DIR:-$DEPS_ROOT/zstd-static-$ZSTD_VERSION}"
TELENO_BUILD_LOCAL_GMP="${TELENO_BUILD_LOCAL_GMP:-1}"
GMP_VERSION="${GMP_VERSION:-6.3.0}"
GMP_INSTALL_DIR="${GMP_INSTALL_DIR:-$DEPS_ROOT/gmp-static-$GMP_VERSION}"
CPP_LIBP2P_TAG="${CPP_LIBP2P_TAG:-v0.1.37}"
CPP_LIBP2P_REPO="${CPP_LIBP2P_REPO:-https://github.com/libp2p/cpp-libp2p.git}"
CPP_LIBP2P_PATCH="${CPP_LIBP2P_PATCH:-$NODE_DIR/cmake/cpp-libp2p-koinos.patch}"

export HUNTER_ROOT
export CMAKE_POLICY_VERSION_MINIMUM="${CMAKE_POLICY_VERSION_MINIMUM:-3.5}"
KOINOS_BOOTSTRAP_CXXFLAGS="${KOINOS_BOOTSTRAP_CXXFLAGS:--Wno-error=deprecated-declarations -DBOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED -DBOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT}"
export CXXFLAGS="${KOINOS_BOOTSTRAP_CXXFLAGS}${CXXFLAGS:+ $CXXFLAGS}"

require_file() {
  [[ -f "$1" ]] || {
    echo "missing required file: $1" >&2
    exit 1
  }
}

find_hunter_prefix() {
  local marker
  local config
  for marker in "$@"; do
    config="$(find "$HUNTER_ROOT/_Base" -type f -path "*/$marker" -print -quit 2>/dev/null || true)"
    if [[ -n "$config" ]]; then
      printf '%s\n' "${config%%/$marker}"
      return
    fi
  done
  echo "could not find Hunter install marker under $HUNTER_ROOT: $*" >&2
  exit 1
}

find_hunter_install_prefix() {
  local marker
  local config
  for marker in "$@"; do
    config="$(find "$HUNTER_ROOT/_Base" -type f -path "*/Install/$marker" -print -quit 2>/dev/null || true)"
    if [[ -n "$config" ]]; then
      printf '%s\n' "${config%%/Install/$marker}/Install"
      return
    fi
  done
  echo "could not find Hunter install marker under $HUNTER_ROOT: $*" >&2
  exit 1
}

cmake_cache_value() {
  local build_dir="$1"
  local key="$2"
  local cache_file="$build_dir/CMakeCache.txt"
  [[ -f "$cache_file" ]] || return 1
  awk -F= -v key="$key" '$1 ~ "^" key ":" { print $2; exit }' "$cache_file"
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

prepare_local_zstd_if_needed() {
  local explicit_paths=0
  if [[ -n "${ZSTD_INCLUDE_DIRS:-${ZSTD_INCLUDE_DIR:-}}" || -n "${ZSTD_LIBRARIES:-${ZSTD_LIBRARY:-}}" ]]; then
    explicit_paths=1
  fi

  if [[ "$TELENO_ROCKSDB_WITH_ZSTD" != "0" && "$TELENO_BUILD_LOCAL_ZSTD" != "0" && "$explicit_paths" == "0" ]]; then
    require_file "$ROOT_DIR/scripts/build-zstd-static.sh"
    KOINOS_DEPS_ROOT="$DEPS_ROOT" \
      ZSTD_VERSION="$ZSTD_VERSION" \
      ZSTD_INSTALL_DIR="$ZSTD_INSTALL_DIR" \
      JOBS="$JOBS" \
      "$ROOT_DIR/scripts/build-zstd-static.sh"
    ZSTD_INCLUDE_DIRS="$ZSTD_INSTALL_DIR/include"
    ZSTD_LIBRARIES="$ZSTD_INSTALL_DIR/lib/libzstd.a"
  fi
}

prepare_local_gmp_if_needed() {
  if [[ "$TELENO_BUILD_LOCAL_GMP" != "0" && -z "${GMP_LIBRARY:-}" ]]; then
    require_file "$ROOT_DIR/scripts/build-gmp-static.sh"
    KOINOS_DEPS_ROOT="$DEPS_ROOT" \
      GMP_VERSION="$GMP_VERSION" \
      GMP_INSTALL_DIR="$GMP_INSTALL_DIR" \
      JOBS="$JOBS" \
      "$ROOT_DIR/scripts/build-gmp-static.sh"
    GMP_LIBRARY="$GMP_INSTALL_DIR/lib/libgmp.a"
  fi

  if [[ -n "${GMP_LIBRARY:-}" && ! -f "$GMP_LIBRARY" ]]; then
    echo "GMP_LIBRARY does not exist: $GMP_LIBRARY" >&2
    exit 1
  fi
}

require_file "$NODE_DIR/CMakeLists.hunter.txt"
require_file "$CPP_LIBP2P_PATCH"
require_file "$ROOT_DIR/scripts/build-rocksdb-zstd.sh"

mkdir -p "$DEPS_ROOT"

echo "==> Preparing Hunter-enabled Teleno node source copy"
rm -rf "$KOINOS_NODE_HUNTER_SOURCE"
mkdir -p "$KOINOS_NODE_HUNTER_SOURCE"
cp -a "$NODE_DIR/." "$KOINOS_NODE_HUNTER_SOURCE/"
cp "$KOINOS_NODE_HUNTER_SOURCE/CMakeLists.hunter.txt" "$KOINOS_NODE_HUNTER_SOURCE/CMakeLists.txt"

echo "==> Building Teleno node Hunter dependency set"
cmake -S "$KOINOS_NODE_HUNTER_SOURCE" -B "$KOINOS_NODE_HUNTER_BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="$CXXFLAGS" \
  -DKOINOS_BUILD_TESTS=OFF \
  -DKOINOS_ENABLE_LIBP2P=OFF
cmake --build "$KOINOS_NODE_HUNTER_BUILD" --target koinos_private_testnet_keygen --parallel "$JOBS"

KOINOS_PROTO_DIR="$(cmake_cache_value "$KOINOS_NODE_HUNTER_BUILD" "koinos_proto_DIR" || true)"
if [[ -n "$KOINOS_PROTO_DIR" ]]; then
  KOINOS_HUNTER_INSTALL="${KOINOS_PROTO_DIR%/lib/cmake/koinos_proto}"
else
  KOINOS_HUNTER_INSTALL="$(find_hunter_prefix \
    "lib/cmake/koinos_proto/koinos_protoConfig.cmake" \
    "lib/cmake/koinos_proto/koinos_proto-config.cmake")"
fi
echo "==> Koinos Hunter install: $KOINOS_HUNTER_INSTALL"

KOINOS_HUNTER_PREFIX="$(find_hunter_install_prefix \
  "lib/cmake/koinos_proto/koinos_protoConfig.cmake" \
  "lib/cmake/koinos_proto/koinos_proto-config.cmake")"
echo "==> Koinos Hunter prefix: $KOINOS_HUNTER_PREFIX"

ROCKSDB_CMAKE_ARGS=()
if [[ "$TELENO_ROCKSDB_WITH_ZSTD" != "0" ]]; then
  prepare_local_zstd_if_needed
  detect_zstd_paths
  echo "==> Preparing zstd-enabled RocksDB override"
  KOINOS_DEPS_ROOT="$DEPS_ROOT" \
    HUNTER_ROOT="$HUNTER_ROOT" \
    ROCKSDB_ZSTD_INSTALL_DIR="$ROCKSDB_ZSTD_INSTALL_DIR" \
    TELENO_BUILD_LOCAL_ZSTD="$TELENO_BUILD_LOCAL_ZSTD" \
    ZSTD_VERSION="$ZSTD_VERSION" \
    ZSTD_INSTALL_DIR="$ZSTD_INSTALL_DIR" \
    ZSTD_INCLUDE_DIRS="$ZSTD_INCLUDE_DIRS" \
    ZSTD_LIBRARIES="$ZSTD_LIBRARIES" \
    JOBS="$JOBS" \
    "$ROOT_DIR/scripts/build-rocksdb-zstd.sh"
  ROCKSDB_CMAKE_ARGS=(
    "-DRocksDB_DIR=$ROCKSDB_ZSTD_INSTALL_DIR/lib/cmake/rocksdb"
    "-Dzstd_INCLUDE_DIRS=$ZSTD_INCLUDE_DIRS"
    "-Dzstd_LIBRARIES=$ZSTD_LIBRARIES"
  )
else
  echo "==> Using Hunter RocksDB package without zstd override"
fi

GMP_CMAKE_ARGS=()
prepare_local_gmp_if_needed
if [[ -n "${GMP_LIBRARY:-}" ]]; then
  GMP_CMAKE_ARGS=( "-DGMP_LIBRARY=$GMP_LIBRARY" )
fi

echo "==> Preparing cpp-libp2p $CPP_LIBP2P_TAG source"
if [[ ! -d "$CPP_LIBP2P_SOURCE_DIR/.git" ]]; then
  git clone --branch "$CPP_LIBP2P_TAG" --depth 1 "$CPP_LIBP2P_REPO" "$CPP_LIBP2P_SOURCE_DIR"
fi
if ! git -C "$CPP_LIBP2P_SOURCE_DIR" fetch --tags --depth 1 origin "$CPP_LIBP2P_TAG"; then
  if git -C "$CPP_LIBP2P_SOURCE_DIR" rev-parse --verify --quiet "$CPP_LIBP2P_TAG^{commit}" >/dev/null; then
    echo "cpp-libp2p fetch failed; reusing cached $CPP_LIBP2P_TAG"
  else
    echo "cpp-libp2p fetch failed and cached $CPP_LIBP2P_TAG is unavailable" >&2
    exit 1
  fi
fi
git -C "$CPP_LIBP2P_SOURCE_DIR" checkout -f "$CPP_LIBP2P_TAG"
if git -C "$CPP_LIBP2P_SOURCE_DIR" apply --check "$CPP_LIBP2P_PATCH"; then
  git -C "$CPP_LIBP2P_SOURCE_DIR" apply "$CPP_LIBP2P_PATCH"
elif git -C "$CPP_LIBP2P_SOURCE_DIR" apply --reverse --check "$CPP_LIBP2P_PATCH"; then
  echo "cpp-libp2p compatibility patch is already applied"
else
  echo "cpp-libp2p compatibility patch does not apply cleanly" >&2
  exit 1
fi

echo "==> Building cpp-libp2p auxiliary Hunter dependency set"
cmake -S "$CPP_LIBP2P_SOURCE_DIR" -B "$CPP_LIBP2P_HUNTER_BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="$CXXFLAGS" \
  -DPACKAGE_MANAGER=hunter \
  -DTESTING=OFF \
  -DEXAMPLES=OFF
cmake --build "$CPP_LIBP2P_HUNTER_BUILD" --parallel "$JOBS"

CPP_AUX_SORALOG_DIR="$(cmake_cache_value "$CPP_LIBP2P_HUNTER_BUILD" "soralog_DIR" || true)"
if [[ -n "$CPP_AUX_SORALOG_DIR" ]]; then
  CPP_AUX_HUNTER_INSTALL="${CPP_AUX_SORALOG_DIR%/lib/cmake/soralog}"
else
  CPP_AUX_HUNTER_INSTALL="$(find_hunter_prefix \
    "lib/cmake/soralog/soralogConfig.cmake" \
    "lib/cmake/soralog/soralog-config.cmake")"
fi
echo "==> cpp-libp2p auxiliary Hunter install: $CPP_AUX_HUNTER_INSTALL"

CPP_AUX_HUNTER_PREFIX="$(find_hunter_install_prefix \
  "lib/cmake/libsecp256k1/libsecp256k1-config.cmake" \
  "lib/cmake/libsecp256k1/libsecp256k1Config.cmake")"
echo "==> cpp-libp2p auxiliary Hunter prefix: $CPP_AUX_HUNTER_PREFIX"

echo "==> Isolating cpp-libp2p third-party headers"
rm -rf "$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR"
mkdir -p "$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR"
cp -a "$CPP_AUX_HUNTER_INSTALL/include/." "$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR/"
rm -rf "$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR/google" "$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR/openssl"

echo "==> Building Koinos-compatible cpp-libp2p install"
cmake -S "$CPP_LIBP2P_SOURCE_DIR" -B "$CPP_LIBP2P_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="$CXXFLAGS" \
  -DPACKAGE_MANAGER=vcpkg \
  -DTESTING=OFF \
  -DEXAMPLES=OFF \
  -DBoost_USE_STATIC_LIBS=ON \
  -DCMAKE_PROJECT_INCLUDE="$NODE_DIR/cmake/cpp-libp2p-koinos-prelude.cmake" \
  -DOPENSSL_ROOT_DIR="$KOINOS_HUNTER_PREFIX" \
  -DOPENSSL_INCLUDE_DIR="$KOINOS_HUNTER_PREFIX/include" \
  -DOPENSSL_CRYPTO_LIBRARY="$KOINOS_HUNTER_PREFIX/lib/libcrypto.a" \
  -DOPENSSL_SSL_LIBRARY="$KOINOS_HUNTER_PREFIX/lib/libssl.a" \
  -DQTILS_INCLUDE_ROOT="$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR" \
  -DCMAKE_INSTALL_PREFIX="$CPP_LIBP2P_INSTALL_DIR" \
  -DCMAKE_CXX_FLAGS="$CXXFLAGS -I$KOINOS_HUNTER_PREFIX/include" \
  -DCMAKE_PREFIX_PATH="$NODE_DIR/cmake/shims;$KOINOS_HUNTER_PREFIX;$KOINOS_HUNTER_INSTALL;$CPP_AUX_HUNTER_PREFIX;$CPP_AUX_HUNTER_INSTALL"
cmake --build "$CPP_LIBP2P_BUILD_DIR" --parallel "$JOBS"
cmake --install "$CPP_LIBP2P_BUILD_DIR"

echo "==> Configuring teleno-node with cpp-libp2p"
cmake -S "$NODE_DIR" -B "$KOINOS_NODE_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DKOINOS_BUILD_TESTS=OFF \
  -DKOINOS_ENABLE_LIBP2P=ON \
  -DCMAKE_PROJECT_INCLUDE="$NODE_DIR/cmake/cpp-libp2p-koinos-prelude.cmake" \
  -DOPENSSL_ROOT_DIR="$KOINOS_HUNTER_PREFIX" \
  -Dyaml-cpp_DIR="$KOINOS_HUNTER_PREFIX/lib/cmake/yaml-cpp" \
  -DCPP_LIBP2P_THIRDPARTY_INCLUDE_DIR="$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR" \
  -DCMAKE_CXX_FLAGS="$CXXFLAGS -I$KOINOS_HUNTER_PREFIX/include -I$KOINOS_HUNTER_INSTALL/include -I$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR" \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$KOINOS_NODE_BUILD_DIR" \
  -DCMAKE_PREFIX_PATH="$NODE_DIR/cmake/shims;$CPP_LIBP2P_INSTALL_DIR;$KOINOS_HUNTER_PREFIX;$KOINOS_HUNTER_INSTALL;$CPP_AUX_HUNTER_PREFIX;$CPP_AUX_HUNTER_INSTALL" \
  ${ROCKSDB_CMAKE_ARGS[@]+"${ROCKSDB_CMAKE_ARGS[@]}"} \
  ${GMP_CMAKE_ARGS[@]+"${GMP_CMAKE_ARGS[@]}"}

echo "==> Building teleno_node and private testnet keygen"
cmake --build "$KOINOS_NODE_BUILD_DIR" --target teleno_node koinos_private_testnet_keygen --parallel "$JOBS"

TELENO_NODE_BIN="$KOINOS_NODE_BUILD_DIR/teleno_node"
KOINOS_KEYGEN_BIN="$KOINOS_NODE_BUILD_DIR/koinos_private_testnet_keygen"
if [[ ! -x "$TELENO_NODE_BIN" && -x "$KOINOS_NODE_BUILD_DIR/src/teleno_node" ]]; then
  TELENO_NODE_BIN="$KOINOS_NODE_BUILD_DIR/src/teleno_node"
fi
if [[ ! -x "$KOINOS_KEYGEN_BIN" && -x "$KOINOS_NODE_BUILD_DIR/src/koinos_private_testnet_keygen" ]]; then
  KOINOS_KEYGEN_BIN="$KOINOS_NODE_BUILD_DIR/src/koinos_private_testnet_keygen"
fi

echo "==> Done"
echo "teleno_node: $TELENO_NODE_BIN"
echo "keygen: $KOINOS_KEYGEN_BIN"
