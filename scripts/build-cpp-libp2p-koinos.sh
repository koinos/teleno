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

require_file "$NODE_DIR/CMakeLists.hunter.txt"
require_file "$CPP_LIBP2P_PATCH"

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

echo "==> Preparing cpp-libp2p $CPP_LIBP2P_TAG source"
if [[ ! -d "$CPP_LIBP2P_SOURCE_DIR/.git" ]]; then
  git clone --branch "$CPP_LIBP2P_TAG" --depth 1 "$CPP_LIBP2P_REPO" "$CPP_LIBP2P_SOURCE_DIR"
fi
git -C "$CPP_LIBP2P_SOURCE_DIR" fetch --tags --depth 1 origin "$CPP_LIBP2P_TAG"
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
  -DCMAKE_PREFIX_PATH="$NODE_DIR/cmake/shims;$CPP_LIBP2P_INSTALL_DIR;$KOINOS_HUNTER_PREFIX;$KOINOS_HUNTER_INSTALL;$CPP_AUX_HUNTER_PREFIX;$CPP_AUX_HUNTER_INSTALL"

echo "==> Building koinos_node and private testnet keygen"
cmake --build "$KOINOS_NODE_BUILD_DIR" --target koinos_node koinos_private_testnet_keygen --parallel "$JOBS"

KOINOS_NODE_BIN="$KOINOS_NODE_BUILD_DIR/koinos_node"
KOINOS_KEYGEN_BIN="$KOINOS_NODE_BUILD_DIR/koinos_private_testnet_keygen"
if [[ ! -x "$KOINOS_NODE_BIN" && -x "$KOINOS_NODE_BUILD_DIR/src/koinos_node" ]]; then
  KOINOS_NODE_BIN="$KOINOS_NODE_BUILD_DIR/src/koinos_node"
fi
if [[ ! -x "$KOINOS_KEYGEN_BIN" && -x "$KOINOS_NODE_BUILD_DIR/src/koinos_private_testnet_keygen" ]]; then
  KOINOS_KEYGEN_BIN="$KOINOS_NODE_BUILD_DIR/src/koinos_private_testnet_keygen"
fi

echo "==> Done"
echo "koinos_node: $KOINOS_NODE_BIN"
echo "keygen: $KOINOS_KEYGEN_BIN"
