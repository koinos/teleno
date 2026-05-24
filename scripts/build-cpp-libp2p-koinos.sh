#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NODE_DIR="${NODE_DIR:-$ROOT_DIR/vendor/koinos/koinos-node}"
DEPS_ROOT="${KOINOS_DEPS_ROOT:-$ROOT_DIR/.deps/koinos-node}"
HUNTER_ROOT="${HUNTER_ROOT:-$DEPS_ROOT/hunter}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

KOINOS_NODE_HUNTER_SOURCE="${KOINOS_NODE_HUNTER_SOURCE:-$DEPS_ROOT/koinos-node-hunter-src}"
KOINOS_NODE_HUNTER_BUILD="${KOINOS_NODE_HUNTER_BUILD:-$DEPS_ROOT/koinos-node-hunter-build}"
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

require_file "$NODE_DIR/CMakeLists.hunter.txt"
require_file "$CPP_LIBP2P_PATCH"

mkdir -p "$DEPS_ROOT"

echo "==> Preparing Hunter-enabled koinos-node source copy"
rm -rf "$KOINOS_NODE_HUNTER_SOURCE"
mkdir -p "$KOINOS_NODE_HUNTER_SOURCE"
cp -a "$NODE_DIR/." "$KOINOS_NODE_HUNTER_SOURCE/"
cp "$KOINOS_NODE_HUNTER_SOURCE/CMakeLists.hunter.txt" "$KOINOS_NODE_HUNTER_SOURCE/CMakeLists.txt"

echo "==> Building koinos-node Hunter dependency set"
cmake -S "$KOINOS_NODE_HUNTER_SOURCE" -B "$KOINOS_NODE_HUNTER_BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DKOINOS_BUILD_TESTS=OFF \
  -DKOINOS_ENABLE_LIBP2P=OFF
cmake --build "$KOINOS_NODE_HUNTER_BUILD" --target koinos_private_testnet_keygen --parallel "$JOBS"

KOINOS_HUNTER_INSTALL="$(find_hunter_prefix \
  "lib/cmake/koinos_proto/koinos_protoConfig.cmake" \
  "lib/cmake/koinos_proto/koinos_proto-config.cmake")"
echo "==> Koinos Hunter install: $KOINOS_HUNTER_INSTALL"

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
  -DPACKAGE_MANAGER=hunter \
  -DTESTING=OFF \
  -DEXAMPLES=OFF
cmake --build "$CPP_LIBP2P_HUNTER_BUILD" --parallel "$JOBS"

CPP_AUX_HUNTER_INSTALL="$(find_hunter_prefix \
  "lib/cmake/soralog/soralogConfig.cmake" \
  "lib/cmake/soralog/soralog-config.cmake")"
echo "==> cpp-libp2p auxiliary Hunter install: $CPP_AUX_HUNTER_INSTALL"

echo "==> Isolating cpp-libp2p third-party headers"
rm -rf "$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR"
mkdir -p "$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR"
cp -a "$CPP_AUX_HUNTER_INSTALL/include/." "$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR/"
rm -rf "$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR/google" "$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR/openssl"

echo "==> Building Koinos-compatible cpp-libp2p install"
cmake -S "$CPP_LIBP2P_SOURCE_DIR" -B "$CPP_LIBP2P_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DPACKAGE_MANAGER=vcpkg \
  -DTESTING=OFF \
  -DEXAMPLES=OFF \
  -DBoost_USE_STATIC_LIBS=ON \
  -DQTILS_INCLUDE_ROOT="$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR" \
  -DCMAKE_INSTALL_PREFIX="$CPP_LIBP2P_INSTALL_DIR" \
  -DCMAKE_PREFIX_PATH="$NODE_DIR/cmake/shims;$KOINOS_HUNTER_INSTALL;$CPP_AUX_HUNTER_INSTALL"
cmake --build "$CPP_LIBP2P_BUILD_DIR" --parallel "$JOBS"
cmake --install "$CPP_LIBP2P_BUILD_DIR"

echo "==> Configuring koinos_node with cpp-libp2p"
cmake -S "$NODE_DIR" -B "$KOINOS_NODE_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DKOINOS_BUILD_TESTS=OFF \
  -DKOINOS_ENABLE_LIBP2P=ON \
  -DCMAKE_PROJECT_INCLUDE="$NODE_DIR/cmake/cpp-libp2p-koinos-prelude.cmake" \
  -DOPENSSL_ROOT_DIR="$KOINOS_HUNTER_INSTALL" \
  -DCPP_LIBP2P_THIRDPARTY_INCLUDE_DIR="$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR" \
  -DCMAKE_CXX_FLAGS="-I$KOINOS_HUNTER_INSTALL/include -I$CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR" \
  -DCMAKE_PREFIX_PATH="$NODE_DIR/cmake/shims;$CPP_LIBP2P_INSTALL_DIR;$KOINOS_HUNTER_INSTALL;$CPP_AUX_HUNTER_INSTALL"

echo "==> Building koinos_node and private testnet keygen"
cmake --build "$KOINOS_NODE_BUILD_DIR" --target koinos_node koinos_private_testnet_keygen --parallel "$JOBS"

echo "==> Done"
echo "koinos_node: $KOINOS_NODE_BUILD_DIR/koinos_node"
echo "keygen: $KOINOS_NODE_BUILD_DIR/koinos_private_testnet_keygen"
