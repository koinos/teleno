cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19...3.27)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

project(koinos_chain_v152_reference_runner LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
list(GET CMAKE_PREFIX_PATH 0 _hunter_install)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${_hunter_install}/include")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_NO_SYSTEM_PATHS ON)

if(NOT REFERENCE_CHAIN_SOURCE OR NOT REFERENCE_STATE_DB_SOURCE OR NOT TELENO_SOURCE)
  message(FATAL_ERROR "REFERENCE_CHAIN_SOURCE, REFERENCE_STATE_DB_SOURCE, and TELENO_SOURCE are required")
endif()

find_package(ZLIB CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(re2 CONFIG REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS system log log_setup thread date_time filesystem chrono exception program_options)
find_package(RocksDB CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(fizzy CONFIG REQUIRED)
find_package(ethash CONFIG REQUIRED)
find_package(libsecp256k1-vrf CONFIG REQUIRED)
find_package(koinos_log CONFIG REQUIRED)
find_package(koinos_util CONFIG REQUIRED)
find_package(koinos_proto CONFIG REQUIRED)
find_package(koinos_exception CONFIG REQUIRED)
find_package(koinos_crypto CONFIG REQUIRED)

add_library(reference_state_db
  ${REFERENCE_STATE_DB_SOURCE}/src/koinos/state_db/state_db.cpp
  ${REFERENCE_STATE_DB_SOURCE}/src/koinos/state_db/state_delta.cpp
  ${REFERENCE_STATE_DB_SOURCE}/src/koinos/state_db/merge_iterator.cpp
  ${REFERENCE_STATE_DB_SOURCE}/src/koinos/state_db/backends/backend.cpp
  ${REFERENCE_STATE_DB_SOURCE}/src/koinos/state_db/backends/iterator.cpp
  ${REFERENCE_STATE_DB_SOURCE}/src/koinos/state_db/backends/map/map_backend.cpp
  ${REFERENCE_STATE_DB_SOURCE}/src/koinos/state_db/backends/map/map_iterator.cpp
  ${REFERENCE_STATE_DB_SOURCE}/src/koinos/state_db/backends/rocksdb/rocksdb_backend.cpp
  ${REFERENCE_STATE_DB_SOURCE}/src/koinos/state_db/backends/rocksdb/rocksdb_iterator.cpp
  ${REFERENCE_STATE_DB_SOURCE}/src/koinos/state_db/backends/rocksdb/object_cache.cpp)
target_include_directories(reference_state_db PUBLIC
  ${REFERENCE_STATE_DB_SOURCE}/include
  PRIVATE ${REFERENCE_STATE_DB_SOURCE}/src)
target_link_libraries(reference_state_db PUBLIC
  Koinos::exception Koinos::proto Koinos::crypto RocksDB::rocksdb)

add_library(reference_vm_manager
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/vm_manager/host_api.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/vm_manager/vm_backend.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/vm_manager/fizzy/fizzy_vm_backend.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/vm_manager/fizzy/module_cache.cpp)
target_include_directories(reference_vm_manager PUBLIC ${REFERENCE_CHAIN_SOURCE}/src)
target_link_libraries(reference_vm_manager PUBLIC
  Koinos::exception Koinos::log Koinos::util fizzy::fizzy)

add_library(reference_chain
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/chain/chronicler.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/chain/controller.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/chain/execution_context.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/chain/host_api.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/chain/proto_utils.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/chain/rectify.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/chain/resource_meter.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/chain/session.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/chain/state.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/chain/system_calls.cpp
  ${REFERENCE_CHAIN_SOURCE}/src/koinos/chain/thunk_dispatcher.cpp)
target_include_directories(reference_chain PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${REFERENCE_CHAIN_SOURCE}/src)
target_link_libraries(reference_chain PUBLIC
  reference_state_db reference_vm_manager Koinos::exception Koinos::crypto
  Koinos::log Koinos::util Koinos::proto Boost::system Boost::thread)

add_library(reference_block_store
  ${TELENO_SOURCE}/src/block_store/block_store.cpp)
target_include_directories(reference_block_store PUBLIC ${TELENO_SOURCE}/src)
target_link_libraries(reference_block_store PUBLIC
  Koinos::log Koinos::proto Koinos::exception RocksDB::rocksdb)

add_executable(koinos_chain_v152_reference_validator reference_validator.cpp)
target_include_directories(koinos_chain_v152_reference_validator PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${REFERENCE_CHAIN_SOURCE}/src
  ${TELENO_SOURCE}/src)
target_link_libraries(koinos_chain_v152_reference_validator PRIVATE
  reference_chain reference_block_store Boost::log_setup Boost::filesystem
  OpenSSL::Crypto)
