# yaml-cpp Hunter target is "yaml-cpp", not "yaml-cpp::yaml-cpp"
if(TARGET yaml-cpp AND NOT TARGET yaml-cpp::yaml-cpp)
  add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
endif()

# ── Core library (config, event_bus, service_registry) ──
add_library(koinos_node_core
  core/config.cpp
  core/config.hpp
  core/event_bus.hpp
  core/node_version.hpp
  core/rpc_access_policy.hpp
  core/service_registry.hpp)

target_include_directories(
  koinos_node_core
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
      $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>)

target_link_libraries(
  koinos_node_core
    PUBLIC
      Koinos::log
      Koinos::proto
      yaml-cpp::yaml-cpp
      Boost::system
      Boost::log)

# ── Backup planning library (native backup/restore foundation) ──
add_library(koinos_backup_lib
  backup/backup_plan.cpp
  backup/backup_plan.hpp)

target_include_directories(
  koinos_backup_lib
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(
  koinos_backup_lib
    PUBLIC
      koinos_node_core)

# ── Interface headers (header-only) ──
add_library(koinos_node_interfaces INTERFACE)
target_include_directories(
  koinos_node_interfaces
    INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(
  koinos_node_interfaces
    INTERFACE
      Koinos::proto)

# ── Local State DB library (adapted from koinos-state-db-cpp) ──
add_library(koinos_state_db_local
  koinos/state_db/state_db.cpp
  koinos/state_db/state_delta.cpp
  koinos/state_db/merge_iterator.cpp
  koinos/state_db/backends/backend.cpp
  koinos/state_db/backends/iterator.cpp
  koinos/state_db/backends/map/map_backend.cpp
  koinos/state_db/backends/map/map_iterator.cpp
  koinos/state_db/backends/rocksdb/rocksdb_backend.cpp
  koinos/state_db/backends/rocksdb/rocksdb_iterator.cpp
  koinos/state_db/backends/rocksdb/object_cache.cpp
  koinos/state_db/state_db.hpp
  koinos/state_db/state_delta.hpp
  koinos/state_db/merge_iterator.hpp
  koinos/state_db/state_db_types.hpp
  koinos/state_db/backends/backend.hpp
  koinos/state_db/backends/exceptions.hpp
  koinos/state_db/backends/iterator.hpp
  koinos/state_db/backends/types.hpp
  koinos/state_db/backends/map/map_backend.hpp
  koinos/state_db/backends/map/map_iterator.hpp
  koinos/state_db/backends/rocksdb/exceptions.hpp
  koinos/state_db/backends/rocksdb/object_cache.hpp
  koinos/state_db/backends/rocksdb/rocksdb_backend.hpp
  koinos/state_db/backends/rocksdb/rocksdb_iterator.hpp)

target_include_directories(
  koinos_state_db_local
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(
  koinos_state_db_local
    PUBLIC
      Koinos::exception
      Koinos::proto
      Koinos::crypto
      RocksDB::rocksdb)

# ── Storage library (shared RocksDB layout and metadata) ──
add_library(koinos_storage_lib
  storage/chain_migration.cpp
  storage/chain_migration.hpp
  storage/rocksdb_manager.cpp
  storage/rocksdb_manager.hpp)

target_include_directories(
  koinos_storage_lib
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(
  koinos_storage_lib
    PUBLIC
      koinos_node_core
      Koinos::log
      RocksDB::rocksdb
      OpenSSL::Crypto)

# ── Backup checkpoint library (RocksDB checkpoint primitive) ──
add_library(koinos_backup_checkpoint_lib
  backup/checkpoint_manager.cpp
  backup/checkpoint_manager.hpp)

target_include_directories(
  koinos_backup_checkpoint_lib
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(
  koinos_backup_checkpoint_lib
    PUBLIC
      koinos_storage_lib
      RocksDB::rocksdb)

# ── Backup snapshot repository (local object store + restore preflight data) ──
add_library(koinos_backup_snapshot_lib
  backup/restore_activation_supervisor.cpp
  backup/restore_activation_supervisor.hpp
  backup/snapshot_repository.cpp
  backup/snapshot_repository.hpp)

target_include_directories(
  koinos_backup_snapshot_lib
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(
  koinos_backup_snapshot_lib
    PUBLIC
      koinos_backup_checkpoint_lib
      koinos_node_core
      OpenSSL::Crypto
    PRIVATE
      nlohmann_json::nlohmann_json)

# ── Backup SFTP upload validation backend ──
add_library(koinos_backup_sftp_lib
  backup/sftp_uploader.cpp
  backup/sftp_uploader.hpp)

target_include_directories(
  koinos_backup_sftp_lib
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(
  koinos_backup_sftp_lib
    PUBLIC
      koinos_node_core
      koinos_backup_snapshot_lib
      OpenSSL::Crypto
      ZLIB::ZLIB
      ssh
    PRIVATE
      nlohmann_json::nlohmann_json)

# ── Public read-only bootstrap restore backend ──
add_library(koinos_backup_public_restore_lib
  backup/public_restore.cpp
  backup/public_restore.hpp)

target_include_directories(
  koinos_backup_public_restore_lib
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(
  koinos_backup_public_restore_lib
    PUBLIC
      koinos_node_core
      koinos_backup_snapshot_lib
      Boost::system
      OpenSSL::SSL
      OpenSSL::Crypto
    PRIVATE
      nlohmann_json::nlohmann_json)

# ── Runtime backup service (in-process hot backup primitive) ──
add_library(koinos_backup_service_lib
  backup/backup_scheduler.cpp
  backup/backup_scheduler.hpp
  backup/backup_service.cpp
  backup/backup_service.hpp)

target_include_directories(
  koinos_backup_service_lib
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(
  koinos_backup_service_lib
    PUBLIC
      koinos_backup_snapshot_lib
      koinos_backup_sftp_lib
      koinos_backup_public_restore_lib
      koinos_storage_lib
      Koinos::log)

# ── Local-only backup admin HTTP server ──
add_library(koinos_backup_admin_lib
  backup/backup_admin_server.cpp
  backup/backup_admin_server.hpp)

target_include_directories(
  koinos_backup_admin_lib
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(
  koinos_backup_admin_lib
    PUBLIC
      koinos_backup_service_lib
      Boost::system
    PRIVATE
      nlohmann_json::nlohmann_json)

# ── Block Store library (Phase 2: C++ port of Go block_store) ──
add_library(koinos_block_store_lib
  block_store/block_store.cpp
  block_store/block_store.hpp
  block_store/skip_list.hpp)

target_include_directories(
  koinos_block_store_lib
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(
  koinos_block_store_lib
    PUBLIC
      koinos_node_interfaces
      Koinos::log
      Koinos::proto
      Koinos::exception
      RocksDB::rocksdb)

# ── VM Manager library (from koinos-chain, no AMQP deps) ──
add_library(vm_manager
  koinos/vm_manager/host_api.cpp
  koinos/vm_manager/vm_backend.cpp
  koinos/vm_manager/fizzy/fizzy_vm_backend.cpp
  koinos/vm_manager/fizzy/module_cache.cpp
  koinos/vm_manager/host_api.hpp
  koinos/vm_manager/vm_backend.hpp
  koinos/vm_manager/fizzy/exceptions.hpp
  koinos/vm_manager/fizzy/fizzy_vm_backend.hpp
  koinos/vm_manager/fizzy/module_cache.hpp)

target_include_directories(vm_manager PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(vm_manager PUBLIC
  Koinos::exception Koinos::log Koinos::util fizzy::fizzy)

# ── Mempool library (from koinos-mempool, no AMQP deps) ──
add_library(mempool_lib
  koinos/mempool/mempool.cpp
  koinos/mempool/block_applicator.cpp
  koinos/mempool/state.cpp
  koinos/mempool/mempool.hpp
  koinos/mempool/block_applicator.hpp
  koinos/mempool/state.hpp)

target_include_directories(mempool_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(mempool_lib PUBLIC
  koinos_state_db_local Koinos::exception Koinos::crypto Koinos::log Koinos::proto)

# ── Chain library (from koinos-chain, with MonolithClient replacing AMQP) ──
add_library(chain
  koinos/chain/chronicler.cpp
  koinos/chain/controller.cpp
  koinos/chain/execution_context.cpp
  koinos/chain/host_api.cpp
  koinos/chain/indexer.cpp
  koinos/chain/proto_utils.cpp
  koinos/chain/rectify.cpp
  koinos/chain/resource_meter.cpp
  koinos/chain/session.cpp
  koinos/chain/state.cpp
  koinos/chain/system_calls.cpp
  koinos/chain/thunk_dispatcher.cpp)

target_include_directories(chain PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(chain PUBLIC
  koinos_node_core koinos_node_interfaces
  koinos_state_db_local Koinos::exception Koinos::crypto Koinos::log
  Koinos::util Koinos::proto vm_manager)

# ── Block Producer library (monolith-native producer loop) ──
add_library(koinos_block_producer_lib
  block_production/block_producer.cpp
  block_production/block_producer.hpp)

target_include_directories(koinos_block_producer_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(koinos_block_producer_lib PUBLIC
  koinos_node_interfaces
  Koinos::crypto
  Koinos::exception
  Koinos::log
  Koinos::proto
  Koinos::util)

# ── Account History library (Phase 6) ──
add_library(koinos_account_history_lib
  account_history/account_history.cpp
  account_history/account_history.hpp)

target_include_directories(koinos_account_history_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(koinos_account_history_lib PUBLIC
  Koinos::log Koinos::proto RocksDB::rocksdb)

# ── gRPC server library (Phase 6) ──
add_library(koinos_grpc_server_lib
  grpc_server/grpc_server.cpp
  grpc_server/grpc_server.hpp)

target_include_directories(koinos_grpc_server_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(koinos_grpc_server_lib PUBLIC
  koinos_node_interfaces
  Koinos::log Koinos::proto
  gRPC::grpc++ protobuf::libprotobuf)

# ── P2P library (Phase 5: sync, gossip, error scoring — transport abstracted) ──
set(P2P_SOURCES
  p2p/gorpc_codec.cpp
  p2p/gorpc_codec.hpp
  p2p/p2p_node.cpp
  p2p/p2p_node.hpp
  p2p/transport.hpp
  p2p/error_handler.hpp
  p2p/fork_watchdog.hpp
  p2p/gossip_toggle.hpp
  p2p/types.hpp
  p2p/libp2p_transport.hpp
  p2p/libp2p_transport.cpp
  p2p/go_bridge_transport.hpp
  p2p/go_bridge_transport.cpp)

add_library(koinos_p2p_lib ${P2P_SOURCES})

target_include_directories(koinos_p2p_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(koinos_p2p_lib PUBLIC
  koinos_node_core koinos_node_interfaces
  Koinos::log Koinos::proto Boost::thread)

# cpp-libp2p integration (optional — enable with -DKOINOS_ENABLE_LIBP2P=ON)
option(KOINOS_ENABLE_LIBP2P "Enable cpp-libp2p transport for P2P networking" OFF)
if(KOINOS_ENABLE_LIBP2P)
  # cpp-libp2p must be built separately and installed, then found via find_package.
  #
  # Build cpp-libp2p separately:
  #   git clone https://github.com/libp2p/cpp-libp2p.git -b v0.1.37
  #   cd cpp-libp2p
  #   CMAKE_POLICY_VERSION_MINIMUM=3.5 cmake -B build -DTESTING=OFF -DEXAMPLES=OFF \
  #     -DHUNTER_ROOT=/Volumes/external/.hunter -DCMAKE_INSTALL_PREFIX=./install
  #   cmake --build build --parallel && cmake --install build
  #
  # Then pass: -DCMAKE_PREFIX_PATH="<install-path>;..."
  # libp2p's cmake config omits some transitive find_dependency calls
  find_package(libsecp256k1 CONFIG REQUIRED)
  find_package(libp2p REQUIRED)

  set(CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR "" CACHE PATH
    "Isolated include directory for cpp-libp2p dependencies")
  if(NOT CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR)
    set(_cpp_libp2p_default_thirdparty_include
      "${CMAKE_CURRENT_SOURCE_DIR}/../.deps/cpp-libp2p-thirdparty-include")
    if(IS_DIRECTORY "${_cpp_libp2p_default_thirdparty_include}")
      set(CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR
        "${_cpp_libp2p_default_thirdparty_include}")
    endif()
  endif()

  if(CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR)
    set(_cpp_libp2p_thirdparty_targets
      Boost::Boost.DI
      fmt::fmt
      fmt::fmt-header-only
      libsecp256k1::secp256k1
      qtils::qtils
      soralog::configurator
      soralog::configurator_yaml
      soralog::fallback_configurator
      soralog::group
      soralog::logger
      soralog::logging_system
      soralog::multisink
      soralog::sink
      soralog::sink_to_console
      soralog::sink_to_file
      soralog::sink_to_nowhere
      soralog::sink_to_syslog
      soralog::soralog
      tsl::tsl_hat_trie)
    foreach(_cpp_libp2p_thirdparty_target IN LISTS _cpp_libp2p_thirdparty_targets)
      if(TARGET "${_cpp_libp2p_thirdparty_target}")
        set_property(TARGET "${_cpp_libp2p_thirdparty_target}" PROPERTY
          INTERFACE_INCLUDE_DIRECTORIES "${CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR}")
      endif()
    endforeach()
  endif()

  if(TARGET libsecp256k1::secp256k1 AND TARGET secp256k1-vrf::secp256k1-vrf)
    get_target_property(_cpp_libp2p_vrf_configs
      secp256k1-vrf::secp256k1-vrf IMPORTED_CONFIGURATIONS)
    if(_cpp_libp2p_vrf_configs)
      set_property(TARGET libsecp256k1::secp256k1 PROPERTY
        IMPORTED_CONFIGURATIONS "${_cpp_libp2p_vrf_configs}")
      foreach(_cpp_libp2p_vrf_config IN LISTS _cpp_libp2p_vrf_configs)
        get_target_property(_cpp_libp2p_vrf_location
          secp256k1-vrf::secp256k1-vrf
          "IMPORTED_LOCATION_${_cpp_libp2p_vrf_config}")
        if(_cpp_libp2p_vrf_location)
          set_property(TARGET libsecp256k1::secp256k1 PROPERTY
            "IMPORTED_LOCATION_${_cpp_libp2p_vrf_config}"
            "${_cpp_libp2p_vrf_location}")
        endif()
      endforeach()
    endif()
  endif()

  set(GMP_LIBRARY "" CACHE FILEPATH "GMP library used by cpp-libp2p VRF dependencies")
  if(NOT GMP_LIBRARY)
    find_library(GMP_LIBRARY NAMES libgmp.a gmp)
  endif()
  if(GMP_LIBRARY)
    message(STATUS "teleno_node: GMP library: ${GMP_LIBRARY}")
    if(TARGET secp256k1-vrf::secp256k1-vrf)
      set_property(TARGET secp256k1-vrf::secp256k1-vrf PROPERTY
        INTERFACE_LINK_LIBRARIES "${GMP_LIBRARY}")
    endif()
    target_link_libraries(koinos_p2p_lib PUBLIC "${GMP_LIBRARY}")
  endif()

  target_link_libraries(koinos_p2p_lib PUBLIC
    p2p::p2p_default_host
    p2p::p2p_default_network
    p2p::p2p_gossip
    p2p::p2p_kademlia
    p2p::p2p_noise
    p2p::p2p_yamux
    p2p::p2p_peer_id
    p2p::p2p_multiaddress
    p2p::p2p_identify)
  target_compile_definitions(koinos_p2p_lib PUBLIC KOINOS_HAS_LIBP2P)
  message(STATUS "teleno_node: cpp-libp2p enabled")
else()
  message(STATUS "teleno_node: cpp-libp2p disabled (use -DKOINOS_ENABLE_LIBP2P=ON to enable)")
endif()

if(KOINOS_BUILD_TESTS)
  add_executable(koinos_config_test
    ../tests/core/config_test.cpp)

  target_link_libraries(koinos_config_test PRIVATE
    koinos_node_core)

  add_test(NAME koinos_config_test COMMAND koinos_config_test)

  add_executable(koinos_service_registry_test
    ../tests/core/service_registry_test.cpp)

  target_link_libraries(koinos_service_registry_test PRIVATE
    koinos_node_core)

  add_test(NAME koinos_service_registry_test COMMAND koinos_service_registry_test)

  add_executable(koinos_controller_delta_test
    ../tests/chain/controller_delta_test.cpp)

  target_link_libraries(koinos_controller_delta_test PRIVATE
    chain)

  add_test(NAME koinos_controller_delta_test COMMAND koinos_controller_delta_test)

  add_executable(koinos_rocksdb_manager_test
    ../tests/storage/rocksdb_manager_test.cpp)

  target_link_libraries(koinos_rocksdb_manager_test PRIVATE
    koinos_storage_lib)

  add_test(NAME koinos_rocksdb_manager_test COMMAND koinos_rocksdb_manager_test)

  add_executable(koinos_backup_plan_test
    ../tests/backup/backup_plan_test.cpp)

  target_link_libraries(koinos_backup_plan_test PRIVATE
    koinos_backup_lib)

  add_test(NAME koinos_backup_plan_test COMMAND koinos_backup_plan_test)

  add_executable(koinos_backup_checkpoint_test
    ../tests/backup/backup_checkpoint_test.cpp)

  target_link_libraries(koinos_backup_checkpoint_test PRIVATE
    koinos_backup_checkpoint_lib)

  add_test(NAME koinos_backup_checkpoint_test COMMAND koinos_backup_checkpoint_test)

  add_executable(koinos_backup_snapshot_test
    ../tests/backup/backup_snapshot_test.cpp)

  target_link_libraries(koinos_backup_snapshot_test PRIVATE
    koinos_backup_snapshot_lib)

  add_test(NAME koinos_backup_snapshot_test COMMAND koinos_backup_snapshot_test)

  add_executable(koinos_backup_sftp_test
    ../tests/backup/backup_sftp_test.cpp)

  target_link_libraries(koinos_backup_sftp_test PRIVATE
    koinos_backup_sftp_lib)

  add_test(NAME koinos_backup_sftp_test COMMAND koinos_backup_sftp_test)

  add_executable(koinos_backup_public_restore_test
    ../tests/backup/backup_public_restore_test.cpp)

  target_link_libraries(koinos_backup_public_restore_test PRIVATE
    koinos_backup_public_restore_lib)

  add_test(NAME koinos_backup_public_restore_test COMMAND koinos_backup_public_restore_test)

  add_executable(koinos_backup_service_test
    ../tests/backup/backup_service_test.cpp)

  target_link_libraries(koinos_backup_service_test PRIVATE
    koinos_backup_service_lib)

  add_test(NAME koinos_backup_service_test COMMAND koinos_backup_service_test)

  add_executable(koinos_backup_admin_server_test
    ../tests/backup/backup_admin_server_test.cpp)

  target_link_libraries(koinos_backup_admin_server_test PRIVATE
    koinos_backup_admin_lib)

  add_test(NAME koinos_backup_admin_server_test COMMAND koinos_backup_admin_server_test)

  add_executable(koinos_gorpc_codec_test
    ../tests/p2p/gorpc_codec_test.cpp
    p2p/gorpc_codec.cpp
    p2p/gorpc_codec.hpp)

  target_include_directories(koinos_gorpc_codec_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

  add_test(NAME koinos_gorpc_codec_test COMMAND koinos_gorpc_codec_test)

  add_executable(koinos_p2p_one_peer_sync_test
    ../tests/p2p/p2p_one_peer_sync_test.cpp)

  target_link_libraries(koinos_p2p_one_peer_sync_test PRIVATE
    koinos_p2p_lib)

  add_test(NAME koinos_p2p_one_peer_sync_test COMMAND koinos_p2p_one_peer_sync_test)

  add_executable(koinos_block_producer_test
    ../tests/block_production/block_producer_test.cpp)

  target_link_libraries(koinos_block_producer_test PRIVATE
    koinos_block_producer_lib)

  add_test(NAME koinos_block_producer_test COMMAND koinos_block_producer_test)

  add_executable(koinos_mempool_adapter_test
    ../tests/mempool/mempool_adapter_test.cpp)

  target_link_libraries(koinos_mempool_adapter_test PRIVATE
    koinos_node_core
    koinos_node_interfaces
    mempool_lib)

  add_test(NAME koinos_mempool_adapter_test COMMAND koinos_mempool_adapter_test)

  add_executable(koinos_grpc_server_test
    ../tests/grpc/grpc_server_test.cpp)

  target_link_libraries(koinos_grpc_server_test PRIVATE
    koinos_grpc_server_lib
    koinos_contract_meta_store_lib
    koinos_transaction_store_lib
    koinos_account_history_lib)

  add_test(NAME koinos_grpc_server_test COMMAND koinos_grpc_server_test)

  if(KOINOS_ENABLE_LIBP2P)
    add_executable(koinos_libp2p_peer_rpc_live_test
      ../tests/p2p/libp2p_peer_rpc_live_test.cpp)

    target_link_libraries(koinos_libp2p_peer_rpc_live_test PRIVATE
      koinos_p2p_lib)

    add_executable(koinos_libp2p_one_peer_sync_live_test
      ../tests/p2p/libp2p_one_peer_sync_live_test.cpp)

    target_link_libraries(koinos_libp2p_one_peer_sync_live_test PRIVATE
      koinos_p2p_lib)

    add_executable(koinos_libp2p_gossip_live_test
      ../tests/p2p/libp2p_gossip_live_test.cpp)

    target_link_libraries(koinos_libp2p_gossip_live_test PRIVATE
      koinos_p2p_lib)
  endif()
endif()

# ── Contract Meta Store library (Phase 4) ──
add_library(koinos_contract_meta_store_lib
  contract_meta_store/contract_meta_store.cpp
  contract_meta_store/contract_meta_store.hpp)

target_include_directories(koinos_contract_meta_store_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(koinos_contract_meta_store_lib PUBLIC
  Koinos::log Koinos::proto RocksDB::rocksdb)

# ── Transaction Store library (Phase 4) ──
add_library(koinos_transaction_store_lib
  transaction_store/transaction_store.cpp
  transaction_store/transaction_store.hpp)

target_include_directories(koinos_transaction_store_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(koinos_transaction_store_lib PUBLIC
  Koinos::log Koinos::proto RocksDB::rocksdb)

# ── JSON-RPC server library (Phase 3: C++ port of Go jsonrpc) ──
add_library(koinos_jsonrpc_lib
  jsonrpc/jsonrpc_server.cpp
  jsonrpc/jsonrpc_server.hpp)

target_include_directories(
  koinos_jsonrpc_lib
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(
  koinos_jsonrpc_lib
    PUBLIC
      koinos_node_core
      koinos_node_interfaces
      koinos_contract_meta_store_lib
      koinos_transaction_store_lib
      koinos_account_history_lib
      Koinos::log
      Koinos::proto
      Boost::system
      Boost::thread
      nlohmann_json::nlohmann_json
      protobuf::libprotobuf)

# ── Main executable ──
#
# Phase 1: Links the existing chain library directly.
# The chain library is built from the koinos-chain submodule but used
# as a library target (no AMQP main). The monolith provides its own main().
#
# NOTE: For this to build, the koinos-chain library target must be available.
# In development, add koinos-chain as a subdirectory or link the pre-built lib.

add_executable(teleno_node main.cpp)

target_link_libraries(
  teleno_node
    PRIVATE
      koinos_node_core
      koinos_node_interfaces
      koinos_backup_lib
      koinos_backup_checkpoint_lib
      koinos_backup_snapshot_lib
      koinos_backup_sftp_lib
      koinos_backup_public_restore_lib
      koinos_backup_service_lib
      koinos_backup_admin_lib
      koinos_storage_lib
      koinos_block_store_lib
      mempool_lib
      koinos_p2p_lib
      koinos_block_producer_lib
      koinos_account_history_lib
      koinos_grpc_server_lib
      koinos_contract_meta_store_lib
      koinos_transaction_store_lib
      koinos_jsonrpc_lib
      Koinos::log
      Koinos::proto
      Koinos::util
      Koinos::exception
      Koinos::crypto
      koinos_state_db_local
      RocksDB::rocksdb
      Boost::program_options
      Boost::thread
      Boost::filesystem
      yaml-cpp::yaml-cpp)

# Chain library is now built internally — always available
target_link_libraries(teleno_node PRIVATE chain)
target_compile_definitions(teleno_node PRIVATE KOINOS_HAS_CHAIN)

# Compatibility target for old automation that still invokes --target koinos_node.
add_custom_target(koinos_node DEPENDS teleno_node)

add_executable(koinos_private_testnet_keygen
  ../tools/private_testnet_keygen.cpp)

target_link_libraries(
  koinos_private_testnet_keygen
    PRIVATE
      Koinos::crypto
      Koinos::util)

add_executable(koinos_grpc_parity_probe
  ../tools/grpc_parity_probe.cpp)

target_link_libraries(
  koinos_grpc_parity_probe
    PRIVATE
      Koinos::proto
      gRPC::grpc++
      protobuf::libprotobuf
      nlohmann_json::nlohmann_json)

if(COMMAND koinos_install)
  koinos_install(TARGETS teleno_node)
else()
  install(TARGETS teleno_node RUNTIME DESTINATION bin)
endif()
