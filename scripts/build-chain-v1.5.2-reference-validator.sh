#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE_FILE="$ROOT_DIR/build/CMakeCache.txt"
REFERENCE_ROOT="${KOINOS_REFERENCE_BUILD_DIR:-$ROOT_DIR/build/reference-chain-v1.5.2}"
CHAIN_SOURCE="$REFERENCE_ROOT/source/koinos-chain"
STATE_DB_SOURCE="$REFERENCE_ROOT/source/koinos-state-db-cpp"
RUNNER_BUILD="$REFERENCE_ROOT/runner-build"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"

CHAIN_COMMIT="0ae99eced8b585c4145424e9c2a28f667796cc66"
STATE_DB_COMMIT="3a1c904e61afbff59e167f50175519e68046e090"

cache_value() {
  local key="$1"
  awk -F= -v key="$key" '$1 ~ "^" key ":" { print $2; exit }' "$CACHE_FILE"
}

require_value() {
  local name="$1"
  local value="$2"
  if [[ -z "$value" ]]; then
    echo "missing $name in clean build cache: $CACHE_FILE" >&2
    exit 1
  fi
}

if [[ ! -f "$CACHE_FILE" ]]; then
  echo "clean repository-local build cache is required first: $CACHE_FILE" >&2
  exit 1
fi

if [[ "$(cache_value CMAKE_HOME_DIRECTORY)" != "$ROOT_DIR" ]]; then
  echo "build cache belongs to another source tree; run the clean Teleno build first" >&2
  exit 1
fi

CMAKE_PREFIX_PATH_VALUE="$(cache_value CMAKE_PREFIX_PATH)"
ROCKSDB_DIR="$(cache_value RocksDB_DIR)"
ZSTD_INCLUDE_DIRS="$(cache_value zstd_INCLUDE_DIRS)"
ZSTD_LIBRARIES="$(cache_value zstd_LIBRARIES)"
require_value CMAKE_PREFIX_PATH "$CMAKE_PREFIX_PATH_VALUE"
require_value RocksDB_DIR "$ROCKSDB_DIR"
require_value zstd_INCLUDE_DIRS "$ZSTD_INCLUDE_DIRS"
require_value zstd_LIBRARIES "$ZSTD_LIBRARIES"

require_repo_local_path() {
  local name="$1"
  local value="$2"
  case "$value" in
    "$ROOT_DIR"/*) ;;
    *)
      echo "$name is not repository-local: $value" >&2
      exit 1
      ;;
  esac
}

IFS=';' read -r -a reference_prefixes <<< "$CMAKE_PREFIX_PATH_VALUE"
for prefix in "${reference_prefixes[@]}"; do
  [[ -n "$prefix" ]] && require_repo_local_path CMAKE_PREFIX_PATH "$prefix"
done
require_repo_local_path RocksDB_DIR "$ROCKSDB_DIR"
require_repo_local_path zstd_INCLUDE_DIRS "$ZSTD_INCLUDE_DIRS"
require_repo_local_path zstd_LIBRARIES "$ZSTD_LIBRARIES"

mkdir -p "$REFERENCE_ROOT/source"

prepare_reference() {
  local repository="$1"
  local commit="$2"
  local destination="$3"

  if [[ ! -d "$destination/.git" ]]; then
    git clone --filter=blob:none --no-checkout "$repository" "$destination"
  fi
  git -C "$destination" fetch --depth 1 origin "$commit"
  git -C "$destination" checkout --detach "$commit"
  if [[ -n "$(git -C "$destination" status --porcelain)" ]]; then
    echo "reference checkout is dirty: $destination" >&2
    exit 1
  fi
  if [[ "$(git -C "$destination" rev-parse HEAD)" != "$commit" ]]; then
    echo "reference checkout did not resolve exact commit: $destination" >&2
    exit 1
  fi
}

prepare_reference https://github.com/koinos/koinos-chain.git "$CHAIN_COMMIT" "$CHAIN_SOURCE"
prepare_reference https://github.com/koinos/koinos-state-db-cpp.git "$STATE_DB_COMMIT" "$STATE_DB_SOURCE"

cmake -S "$ROOT_DIR/tools/reference/chain-v1.5.2" -B "$RUNNER_BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPKG_CONFIG_EXECUTABLE=/usr/bin/false \
  -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH_VALUE" \
  -DRocksDB_DIR="$ROCKSDB_DIR" \
  -Dzstd_INCLUDE_DIRS="$ZSTD_INCLUDE_DIRS" \
  -Dzstd_LIBRARIES="$ZSTD_LIBRARIES" \
  -DREFERENCE_CHAIN_SOURCE="$CHAIN_SOURCE" \
  -DREFERENCE_STATE_DB_SOURCE="$STATE_DB_SOURCE" \
  -DTELENO_SOURCE="$ROOT_DIR"

cmake --build "$RUNNER_BUILD" --parallel "$JOBS"

echo "reference chain commit: $CHAIN_COMMIT"
echo "reference state DB commit: $STATE_DB_COMMIT"
echo "reference validator: $RUNNER_BUILD/koinos_chain_v152_reference_validator"
