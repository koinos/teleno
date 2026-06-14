# Standalone build that uses pre-built Hunter packages from koinos-chain.
# Bypasses Hunter entirely — no re-downloading, no re-compilation.
#
# Usage:
#   cmake -S . -B build \
#     -DCMAKE_BUILD_TYPE=Release \
#     -DCMAKE_PREFIX_PATH="$HOME/.hunter/_Base/a20151e/caf7adb/26936b6/Install;/opt/homebrew" \
#     -DSTANDALONE_BUILD=ON
#
cmake_minimum_required(VERSION 3.19.0)
cmake_policy(VERSION 3.19.0...3.27.4)
# cmake 4.x removed the FindBoost module — use config-mode directly
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

project(koinos_node
  VERSION 0.1.0
  DESCRIPTION "Koinos monolithic node"
  LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(KOINOS_BUILD_TESTS "Build koinos-node tests" ON)
if(KOINOS_BUILD_TESTS)
  enable_testing()
endif()

# /usr/local/include has Boost 1.78 headers that shadow Hunter's Boost 1.83.
# CMake marks imported-target includes as -isystem, but Clang resolves an
# -I/-isystem conflict on the same path by keeping only -isystem, which lets
# the built-in /usr/local/include win.  Fix: use plain -I for imported targets
# so the Hunter path is searched first.
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
list(GET CMAKE_PREFIX_PATH 0 _hunter_install)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${_hunter_install}/include")

# Hunter-built Boost is static-only
set(Boost_USE_STATIC_LIBS ON)
set(Boost_NO_SYSTEM_PATHS ON)

# Find all pre-built packages (including transitive deps)
find_package(ZLIB CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(re2 CONFIG REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS system log log_setup thread date_time filesystem chrono program_options exception)
find_package(RocksDB CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
if(LIBSSH_ROOT)
  find_package(libssh CONFIG REQUIRED PATHS "${LIBSSH_ROOT}" NO_DEFAULT_PATH)
else()
  find_package(libssh CONFIG REQUIRED PATHS "/opt/homebrew" "/usr/local")
endif()
find_package(fizzy CONFIG REQUIRED)
find_package(ethash CONFIG REQUIRED)
find_package(libsecp256k1-vrf CONFIG REQUIRED)
find_package(koinos_log CONFIG REQUIRED)
find_package(koinos_util CONFIG REQUIRED)
find_package(koinos_proto CONFIG REQUIRED)
find_package(koinos_exception CONFIG REQUIRED)
find_package(koinos_crypto CONFIG REQUIRED)
find_package(koinos_state_db CONFIG REQUIRED)

# Node build version metadata.
set(KOINOS_NODE_GIT_COMMIT "unknown")
set(KOINOS_NODE_DIRTY_SUFFIX "")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${CMAKE_SOURCE_DIR}" rev-parse --short=12 HEAD
    OUTPUT_VARIABLE KOINOS_NODE_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${CMAKE_SOURCE_DIR}" status --porcelain
    OUTPUT_VARIABLE KOINOS_NODE_GIT_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(KOINOS_NODE_GIT_STATUS)
    set(KOINOS_NODE_DIRTY_SUFFIX "-dirty")
  endif()
endif()

set(KOINOS_NODE_BUILD_VERSION "${PROJECT_VERSION}+${KOINOS_NODE_GIT_COMMIT}${KOINOS_NODE_DIRTY_SUFFIX}")
file(WRITE "${CMAKE_BINARY_DIR}/generated/git_version.h"
  "#pragma once\n"
  "#define KOINOS_NODE_VERSION \"${PROJECT_VERSION}\"\n"
  "#define KOINOS_NODE_GIT_COMMIT \"${KOINOS_NODE_GIT_COMMIT}\"\n"
  "#define KOINOS_NODE_BUILD_VERSION \"${KOINOS_NODE_BUILD_VERSION}\"\n"
  "static const char* git_version = KOINOS_NODE_BUILD_VERSION;\n")

add_subdirectory(src)
