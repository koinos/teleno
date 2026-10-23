# yaml-cpp Hunter target is "yaml-cpp", not "yaml-cpp::yaml-cpp"
add_library(koinos_owned_allocation OBJECT core/owned_allocation.cpp)
target_include_directories(koinos_owned_allocation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(koinos_owned_allocation_link INTERFACE)
target_sources(koinos_owned_allocation_link INTERFACE $<TARGET_OBJECTS:koinos_owned_allocation>)
target_include_directories(koinos_owned_allocation_link INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
# Include the replacement object once in native executables. Weak replacement
# symbols allow fault fixtures to wrap the same primitive without bypassing it.
link_libraries(koinos_owned_allocation_link)

if(TARGET yaml-cpp AND NOT TARGET yaml-cpp::yaml-cpp)
  add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
endif()

# The pinned SDK exports only the C gRPC runtime, but its generated service
# stubs also reference the C++ runtime. Propagate it after Koinos::proto so
# static ELF links resolve those symbols without depending on another consumer.
set_property(TARGET Koinos::proto APPEND PROPERTY INTERFACE_LINK_LIBRARIES gRPC::grpc++)

# ── Core library (config, event_bus, service_registry) ──
add_library(koinos_node_core
  core/config.cpp
  core/config.hpp
  core/preflight.cpp
  core/preflight.hpp
  core/event_bus.hpp
  core/transaction_work_budget.hpp
  core/transaction_response_preparer.hpp
  core/owned_memory_budget.hpp
  core/node_version.hpp
  core/restore_startup.cpp
  core/restore_startup.hpp
  core/storage_lease.cpp
  core/storage_lease.hpp
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
      nlohmann_json::nlohmann_json
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
  backup/checkpoint_identity.cpp
  backup/checkpoint_identity.hpp
  backup/durable_publication.cpp
  backup/durable_publication.hpp
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
      Koinos::crypto
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
  backup/public_restore.hpp
  backup/restore_plan.cpp
  backup/restore_plan.hpp
  backup/direct_restore.cpp
  backup/direct_restore.hpp
  backup/direct_restore_cli.cpp
  backup/direct_restore_cli.hpp
  backup/offline_restore_validation.cpp
  backup/offline_restore_validation.hpp)

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
      Koinos::crypto
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
  koinos/vm_manager/fizzy/module_memory.cpp
  koinos/vm_manager/host_api.hpp
  koinos/vm_manager/vm_backend.hpp
  koinos/vm_manager/fizzy/exceptions.hpp
  koinos/vm_manager/fizzy/fizzy_vm_backend.hpp
  koinos/vm_manager/fizzy/module_cache.hpp)

target_include_directories(vm_manager PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_include_directories(vm_manager PRIVATE "${TELENO_FIZZY_SOURCE_DIR}/lib")

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
  grpc_server/grpc_server.hpp
  grpc_server/mempool_output_state.hpp)

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
  p2p/seen_cache.hpp
  p2p/transport.hpp
  p2p/error_handler.hpp
  p2p/fork_watchdog.hpp
  p2p/gossip_toggle.hpp
  p2p/types.hpp
  p2p/libp2p_transport.hpp
  p2p/libp2p_transport.cpp
  p2p/peer_rpc_write.hpp
  p2p/peer_rpc_read.hpp
  p2p/go_bridge_transport.hpp
  p2p/go_bridge_transport.cpp)

add_library(koinos_p2p_lib ${P2P_SOURCES})

target_include_directories(koinos_p2p_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(koinos_p2p_lib PUBLIC
  koinos_node_core koinos_node_interfaces
  Koinos::log Koinos::proto Boost::thread)
if(WIN32)
  target_link_libraries(koinos_p2p_lib PUBLIC ws2_32)
endif()

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

  string(TOUPPER "${CMAKE_BUILD_TYPE}" _teleno_tsan_build_type)
  set(_teleno_tsan_compile_flags "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_teleno_tsan_build_type}}")
  if(_teleno_tsan_compile_flags MATCHES "-fsanitize=[^ ]*thread")
    include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/TelenoTsanScopes.cmake")
    teleno_make_tsan_scope_overlay(
      "${CPP_LIBP2P_THIRDPARTY_INCLUDE_DIR}/boost/di/extension/scopes/shared.hpp"
      TELENO_TSAN_SCOPE_OVERLAY)
    target_include_directories(koinos_p2p_lib BEFORE PUBLIC "${TELENO_TSAN_SCOPE_OVERLAY}")
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
    # The libsecp256k1 import location is redirected to the VRF build above,
    # which requires GMP; propagate it so static link order stays correct for
    # every consumer (e.g. the libp2p live test executables on Linux).
    if(TARGET libsecp256k1::secp256k1)
      set_property(TARGET libsecp256k1::secp256k1 APPEND PROPERTY
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
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME koinos_fizzy_dependency_recipe_test
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/fizzy_dependency_recipe_test.py
        ${CMAKE_COMMAND} ${TELENO_FIZZY_SOURCE_DIR})
    set_tests_properties(koinos_fizzy_dependency_recipe_test PROPERTIES TIMEOUT 90)
    add_test(NAME koinos_protobuf_dependency_recipe_test
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/protobuf_dependency_recipe_test.py
        ${CMAKE_COMMAND} ${TELENO_PROTOBUF_SOURCE_DIR})
    set_tests_properties(koinos_protobuf_dependency_recipe_test PROPERTIES TIMEOUT 90)
    add_test(NAME koinos_generated_proto_ownership_recipe_test
      COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/koinos_proto_ownership_recipe_test.py
        ${TELENO_KOINOS_PROTO_SOURCE_DIR})
    set_tests_properties(koinos_generated_proto_ownership_recipe_test PROPERTIES TIMEOUT 90)
    add_test(NAME koinos_grpc_call_binding_recipe_test
      COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/grpc_call_binding_recipe_test.py
        ${TELENO_GRPC_CALL_BINDING_SOURCE_DIR} ${CMAKE_AR} ${CMAKE_RANLIB})
    set_tests_properties(koinos_grpc_call_binding_recipe_test PROPERTIES TIMEOUT 90)
    add_test(NAME koinos_nlohmann_cleanup_recipe_test
      COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/nlohmann_cleanup_recipe_test.py
        ${CMAKE_COMMAND} ${TELENO_NLOHMANN_JSON_INCLUDE_DIR})
    add_test(NAME teleno_preflight_cli_test
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/cli/preflight_test.py $<TARGET_FILE:teleno_node>)
    set_tests_properties(teleno_preflight_cli_test PROPERTIES TIMEOUT 90)
    add_executable(teleno_compatibility_fixture
      ../tests/integration/compatibility_fixture.cpp)
    target_link_libraries(teleno_compatibility_fixture PRIVATE chain koinos_storage_lib mempool_lib)
    add_test(NAME koinos_compatibility_node_test
      COMMAND ${Python3_EXECUTABLE} -B
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/integration/compatibility_node_test.py
        --node $<TARGET_FILE:teleno_node>
        --fixture $<TARGET_FILE:teleno_compatibility_fixture>)
    set_tests_properties(koinos_compatibility_node_test PROPERTIES TIMEOUT 100)
  endif()
  add_executable(koinos_config_test
    ../tests/core/config_test.cpp)

  target_link_libraries(koinos_config_test PRIVATE
    koinos_node_core)

  add_test(NAME koinos_config_test COMMAND koinos_config_test)

  add_executable(koinos_process_memory_test ../tests/core/process_memory_test.cpp)
  target_link_libraries(koinos_process_memory_test PRIVATE koinos_node_core)
  add_test(NAME koinos_process_memory_test COMMAND koinos_process_memory_test)
  set_tests_properties(koinos_process_memory_test PROPERTIES TIMEOUT 30)

  add_executable(koinos_transaction_work_budget_test ../tests/core/transaction_work_budget_test.cpp)
  target_link_libraries(koinos_transaction_work_budget_test PRIVATE koinos_node_core koinos_node_interfaces)
  add_test(NAME koinos_transaction_work_budget_test COMMAND koinos_transaction_work_budget_test)
  set_tests_properties(koinos_transaction_work_budget_test PROPERTIES TIMEOUT 30)

  add_executable(koinos_owned_memory_budget_test ../tests/core/owned_memory_budget_test.cpp)
  target_link_libraries(koinos_owned_memory_budget_test PRIVATE koinos_node_core)
  add_test(NAME koinos_owned_memory_budget_test COMMAND koinos_owned_memory_budget_test)
  set_tests_properties(koinos_owned_memory_budget_test PROPERTIES TIMEOUT 60)

  add_executable(koinos_protobuf_metadata_budget_test ../tests/core/protobuf_metadata_budget_test.cpp)
  target_link_libraries(koinos_protobuf_metadata_budget_test PRIVATE koinos_node_core Koinos::proto protobuf::libprotobuf)
  add_test(NAME koinos_protobuf_metadata_budget_test COMMAND koinos_protobuf_metadata_budget_test)
  add_test(NAME koinos_protobuf_metadata_concurrent_budget_test COMMAND koinos_protobuf_metadata_budget_test --concurrent)

  add_executable(koinos_owned_allocation_test ../tests/core/owned_allocation_test.cpp)
  target_link_libraries(koinos_owned_allocation_test PRIVATE Threads::Threads)
  add_test(NAME koinos_owned_allocation_test COMMAND koinos_owned_allocation_test)
  set_tests_properties(koinos_owned_allocation_test PROPERTIES TIMEOUT 60)

  add_executable(koinos_mempool_aggregate_memory_test ../tests/mempool/aggregate_memory_test.cpp)
  target_link_libraries(koinos_mempool_aggregate_memory_test PRIVATE mempool_lib)
  add_test(NAME koinos_mempool_aggregate_memory_test COMMAND koinos_mempool_aggregate_memory_test)
  set_tests_properties(koinos_mempool_aggregate_memory_test PROPERTIES TIMEOUT 600)
  add_test(NAME koinos_mempool_shorter_predecessor_test COMMAND koinos_mempool_aggregate_memory_test --short-predecessor)
  set_tests_properties(koinos_mempool_shorter_predecessor_test PROPERTIES TIMEOUT 30)

  add_executable(koinos_service_registry_test
    ../tests/core/service_registry_test.cpp)

  target_link_libraries(koinos_service_registry_test PRIVATE
    koinos_node_core)

  add_test(NAME koinos_service_registry_test COMMAND koinos_service_registry_test)

  add_executable(koinos_controller_delta_test
    ../tests/chain/controller_delta_test.cpp)

  target_link_libraries(koinos_controller_delta_test PRIVATE
    chain mempool_lib)

  add_test(NAME koinos_controller_delta_test COMMAND koinos_controller_delta_test)

  add_executable(koinos_host_memory_test ../tests/vm/host_memory_test.cpp)
  target_link_libraries(koinos_host_memory_test PRIVATE chain)
  add_test(NAME koinos_host_memory_test COMMAND koinos_host_memory_test)
  set_tests_properties(koinos_host_memory_test PROPERTIES TIMEOUT 30)

  add_executable(koinos_indexer_lookahead_test
    ../tests/chain/indexer_lookahead_test.cpp)

  target_link_libraries(koinos_indexer_lookahead_test PRIVATE
    chain)

  add_test(NAME koinos_indexer_lookahead_test COMMAND koinos_indexer_lookahead_test)

  add_executable(koinos_historical_fast_replay_fixture_test
    ../tests/chain/historical_fast_replay_fixture_test.cpp)

  target_compile_definitions(koinos_historical_fast_replay_fixture_test PRIVATE
    TELENO_FAST_REPLAY_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/chain-v1.5.2-fast-replay")

  target_link_libraries(koinos_historical_fast_replay_fixture_test PRIVATE
    chain
    nlohmann_json::nlohmann_json)

  add_test(NAME koinos_historical_fast_replay_fixture_test
    COMMAND koinos_historical_fast_replay_fixture_test)

  add_executable(koinos_rocksdb_manager_test
    ../tests/storage/rocksdb_manager_test.cpp)

  target_link_libraries(koinos_rocksdb_manager_test PRIVATE
    koinos_storage_lib)

  add_test(NAME koinos_rocksdb_manager_test COMMAND koinos_rocksdb_manager_test)

  add_executable(koinos_cache_invariants_test ../tests/storage/cache_invariants_test.cpp)
  target_link_libraries(koinos_cache_invariants_test PRIVATE koinos_state_db_local vm_manager)
  add_test(NAME koinos_cache_invariants_test COMMAND koinos_cache_invariants_test)
  set_tests_properties(koinos_cache_invariants_test PROPERTIES TIMEOUT 30)

  add_executable(koinos_state_value_lifetime_test ../tests/storage/state_value_lifetime_test.cpp)
  target_link_libraries(koinos_state_value_lifetime_test PRIVATE koinos_state_db_local)
  add_test(NAME koinos_state_value_lifetime_test COMMAND koinos_state_value_lifetime_test)
  set_tests_properties(koinos_state_value_lifetime_test PROPERTIES TIMEOUT 60)

  add_executable(koinos_atomic_write_batch_test ../tests/storage/atomic_write_batch_test.cpp)
  target_link_libraries(koinos_atomic_write_batch_test PRIVATE koinos_state_db_local)
  add_test(NAME koinos_atomic_write_batch_test COMMAND koinos_atomic_write_batch_test)

  add_executable(koinos_retained_memory_test ../tests/storage/retained_memory_test.cpp)
  target_compile_definitions(koinos_retained_memory_test PRIVATE
    TELENO_LR2_REPOSITORY_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
  target_link_libraries(koinos_retained_memory_test PRIVATE koinos_state_db_local mempool_lib)
  add_test(NAME koinos_retained_memory_test COMMAND koinos_retained_memory_test)
  set_tests_properties(koinos_retained_memory_test PROPERTIES TIMEOUT 60)

  add_executable(koinos_mempool_pending_record_budget_test ../tests/mempool/pending_record_budget_test.cpp)
  add_executable(koinos_protobuf_unknown_field_ownership_test ../tests/core/protobuf_unknown_field_ownership_test.cpp)
  target_link_libraries(koinos_protobuf_unknown_field_ownership_test PRIVATE protobuf::libprotobuf)
  add_test(NAME koinos_protobuf_unknown_field_ownership_test COMMAND koinos_protobuf_unknown_field_ownership_test)
  set_tests_properties(koinos_protobuf_unknown_field_ownership_test PROPERTIES TIMEOUT 30)
  target_compile_definitions(koinos_mempool_pending_record_budget_test PRIVATE
    TELENO_LR2_REPOSITORY_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
  target_link_libraries(koinos_mempool_pending_record_budget_test PRIVATE koinos_state_db_local mempool_lib)
  add_test(NAME koinos_mempool_pending_record_budget_test COMMAND koinos_mempool_pending_record_budget_test)
  set_tests_properties(koinos_mempool_pending_record_budget_test PROPERTIES TIMEOUT 120)

  add_executable(koinos_fizzy_lifetime_test ../tests/vm/fizzy_lifetime_test.cpp)
  target_link_libraries(koinos_fizzy_lifetime_test PRIVATE vm_manager)
  add_test(NAME koinos_fizzy_lifetime_test COMMAND koinos_fizzy_lifetime_test)
  set_tests_properties(koinos_fizzy_lifetime_test PROPERTIES TIMEOUT 90)
  add_executable(koinos_module_capacity_test ../tests/vm/module_capacity_test.cpp)
  target_include_directories(koinos_module_capacity_test PRIVATE "${TELENO_FIZZY_SOURCE_DIR}/lib")
  target_link_libraries(koinos_module_capacity_test PRIVATE vm_manager chain)
  add_test(NAME koinos_module_capacity_test COMMAND koinos_module_capacity_test)
  set_tests_properties(koinos_module_capacity_test PROPERTIES TIMEOUT 60)

  add_executable(koinos_state_db_durability_test
    ../tests/storage/state_db_durability_test.cpp)

  target_link_libraries(koinos_state_db_durability_test PRIVATE
    koinos_state_db_local)

  add_test(NAME koinos_state_db_durability_test COMMAND koinos_state_db_durability_test)

  add_executable(koinos_state_db_pending_root_test
    ../tests/storage/state_db_pending_root_test.cpp)

  target_link_libraries(koinos_state_db_pending_root_test PRIVATE
    koinos_state_db_local)

  add_test(NAME koinos_state_db_pending_root_test COMMAND koinos_state_db_pending_root_test)

  add_executable(koinos_persistence_failure_test ../tests/storage/persistence_failure_test.cpp
    ../tests/storage/fault_db.cpp)
  target_compile_definitions(koinos_persistence_failure_test PRIVATE
    TELENO_LR1_REPOSITORY_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
  target_link_libraries(koinos_persistence_failure_test PRIVATE
    koinos_state_db_local koinos_storage_lib koinos_block_store_lib)
  set_source_files_properties(../tests/storage/fault_db.cpp PROPERTIES
    COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-fno-rtti>")
  add_test(NAME koinos_persistence_failure_test COMMAND koinos_persistence_failure_test)
  set_tests_properties(koinos_persistence_failure_test PROPERTIES TIMEOUT 30)

  add_executable(koinos_block_receipt_completion_test
    ../tests/storage/block_receipt_completion_test.cpp ../tests/storage/fault_db.cpp)
  target_compile_definitions(koinos_block_receipt_completion_test PRIVATE
    TELENO_LR1_REPOSITORY_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
  target_link_libraries(koinos_block_receipt_completion_test PRIVATE koinos_block_store_lib)
  add_test(NAME koinos_block_receipt_completion_test COMMAND koinos_block_receipt_completion_test)
  set_tests_properties(koinos_block_receipt_completion_test PROPERTIES TIMEOUT 30)

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

  if(UNIX)
    add_executable(koinos_durable_publication_test ../tests/backup/durable_publication_test.cpp)
    target_compile_definitions(koinos_durable_publication_test PRIVATE TELENO_LR1_REPOSITORY_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(koinos_durable_publication_test PRIVATE koinos_backup_snapshot_lib)
    add_test(NAME koinos_durable_publication_test COMMAND koinos_durable_publication_test)
    set_tests_properties(koinos_durable_publication_test PROPERTIES TIMEOUT 30)
  endif()

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

  add_executable(koinos_backup_restore_plan_test ../tests/backup/restore_plan_test.cpp)
  target_link_libraries(koinos_backup_restore_plan_test PRIVATE koinos_backup_public_restore_lib nlohmann_json::nlohmann_json)
  target_compile_definitions(koinos_backup_restore_plan_test PRIVATE TELENO_SC1_REPOSITORY_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
  add_test(NAME koinos_backup_restore_plan_test COMMAND koinos_backup_restore_plan_test)
  if(UNIX)
    add_executable(koinos_direct_restore_fixture ../tools/backup/direct_restore_fixture.cpp)
    target_link_libraries(koinos_direct_restore_fixture PRIVATE koinos_backup_public_restore_lib nlohmann_json::nlohmann_json)
    target_compile_definitions(koinos_direct_restore_fixture PRIVATE TELENO_SC2_REPOSITORY_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_executable(koinos_backup_direct_restore_test ../tests/backup/direct_restore_test.cpp)
    target_link_libraries(koinos_backup_direct_restore_test PRIVATE koinos_backup_public_restore_lib nlohmann_json::nlohmann_json)
    target_compile_definitions(koinos_backup_direct_restore_test PRIVATE TELENO_SC2_REPOSITORY_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_test(NAME koinos_backup_direct_restore_test COMMAND koinos_backup_direct_restore_test)
    set_tests_properties(koinos_backup_direct_restore_test PROPERTIES TIMEOUT 120)
    add_executable(koinos_backup_direct_activation_test ../tests/backup/direct_activation_test.cpp)
    target_link_libraries(koinos_backup_direct_activation_test PRIVATE koinos_backup_public_restore_lib nlohmann_json::nlohmann_json)
    target_compile_definitions(koinos_backup_direct_activation_test PRIVATE TELENO_SC3_REPOSITORY_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_test(NAME koinos_backup_direct_activation_test COMMAND koinos_backup_direct_activation_test $<TARGET_FILE:teleno_node>)
    set_tests_properties(koinos_backup_direct_activation_test PROPERTIES TIMEOUT 180)
  endif()
  if(UNIX)
    if(NOT Python3_Interpreter_FOUND)
      find_package(Python3 COMPONENTS Interpreter QUIET)
    endif()
  endif()
  if(UNIX AND Python3_Interpreter_FOUND)
    add_test(NAME teleno_direct_restore_harness_test
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/backup/direct_restore_harness_test.py
        --fixture $<TARGET_FILE:koinos_direct_restore_fixture> --legacy $<TARGET_FILE:koinos_restore_baseline_fixture>
        --repository ${CMAKE_CURRENT_SOURCE_DIR}/..)
    set_tests_properties(teleno_direct_restore_harness_test PROPERTIES TIMEOUT 120)
    add_test(NAME teleno_direct_restore_preflight_cli_test
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/cli/direct_restore_preflight_test.py
        --node $<TARGET_FILE:teleno_node> --fixture $<TARGET_FILE:koinos_restore_baseline_fixture>
        --repository ${CMAKE_CURRENT_SOURCE_DIR}/..)
    set_tests_properties(teleno_direct_restore_preflight_cli_test PROPERTIES TIMEOUT 90)
    add_test(NAME teleno_direct_restore_workflow_cli_test
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/cli/direct_restore_workflow_test.py
        --node $<TARGET_FILE:teleno_node> --fixture $<TARGET_FILE:koinos_backup_direct_activation_test>
        --repository ${CMAKE_CURRENT_SOURCE_DIR}/..)
    set_tests_properties(teleno_direct_restore_workflow_cli_test PROPERTIES TIMEOUT 180)
    add_test(NAME teleno_single_copy_measurement_test
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/validate_single_copy_restore_test.py)
    add_test(NAME teleno_low_resource_baseline_test
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/low_resource_baseline_test.py)
    set_tests_properties(teleno_low_resource_baseline_test PROPERTIES TIMEOUT 30)
    add_test(NAME teleno_lr2_memory_admission_test
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/lr2_memory_test.py)
    set_tests_properties(teleno_lr2_memory_admission_test PROPERTIES TIMEOUT 30)
    add_test(NAME teleno_fixture_paths_test
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/scripts/fixture_paths_test.py)
    set_tests_properties(teleno_fixture_paths_test PROPERTIES TIMEOUT 30)
    add_test(NAME teleno_single_copy_experiment_test
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/backup/validate_single_copy_restore.py
        --build-dir ${CMAKE_BINARY_DIR} --repetitions 5 --retire-completed-pairs)
    set_tests_properties(teleno_single_copy_experiment_test PROPERTIES TIMEOUT 240)
  endif()

  # Diagnostic-only SC0 fixture; uses the existing restore APIs and owned scratch data.
  if(UNIX)
    add_executable(koinos_restore_baseline_fixture
      ../tools/backup/restore_baseline_fixture.cpp)
    target_link_libraries(koinos_restore_baseline_fixture PRIVATE
      koinos_backup_public_restore_lib)
    target_compile_definitions(koinos_restore_baseline_fixture PRIVATE
      TELENO_SC0_REPOSITORY_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    if(NOT Python3_Interpreter_FOUND)
      find_package(Python3 COMPONENTS Interpreter QUIET)
    endif()
    if(Python3_Interpreter_FOUND)
      add_test(NAME teleno_restore_baseline_harness_test
        COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/../tools/backup/record_sc0_baseline.py
          --build-dir ${CMAKE_BINARY_DIR} --repetitions 1)
      set_tests_properties(teleno_restore_baseline_harness_test PROPERTIES TIMEOUT 90)
    endif()
  endif()

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

  add_executable(koinos_peer_dial_budget_test ../tests/p2p/peer_dial_budget_test.cpp)
  target_link_libraries(koinos_peer_dial_budget_test PRIVATE koinos_p2p_lib)
  target_compile_options(koinos_peer_dial_budget_test PRIVATE -UNDEBUG)
  add_test(NAME koinos_peer_dial_budget_test COMMAND koinos_peer_dial_budget_test)
  set_tests_properties(koinos_peer_dial_budget_test PROPERTIES TIMEOUT 30)

  foreach(_peer_component peer_advice_store peer_snapshot_budget)
    add_executable(koinos_${_peer_component}_test ../tests/p2p/${_peer_component}_test.cpp)
    target_link_libraries(koinos_${_peer_component}_test PRIVATE koinos_p2p_lib)
    target_compile_options(koinos_${_peer_component}_test PRIVATE -UNDEBUG)
    add_test(NAME koinos_${_peer_component}_test COMMAND koinos_${_peer_component}_test)
    set_tests_properties(koinos_${_peer_component}_test PROPERTIES TIMEOUT 30)
  endforeach()

  add_executable(koinos_peer_error_handler_test ../tests/p2p/peer_error_handler_test.cpp)
  target_link_libraries(koinos_peer_error_handler_test PRIVATE koinos_p2p_lib)
  add_test(NAME koinos_peer_error_handler_test COMMAND koinos_peer_error_handler_test)
  set_tests_properties(koinos_peer_error_handler_test PROPERTIES TIMEOUT 30)

  add_executable(koinos_p2p_seen_cache_test ../tests/p2p/seen_cache_test.cpp)
  target_include_directories(koinos_p2p_seen_cache_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(koinos_p2p_seen_cache_test PRIVATE Threads::Threads)
  add_test(NAME koinos_p2p_seen_cache_test COMMAND koinos_p2p_seen_cache_test)

  add_executable(koinos_block_producer_test
    ../tests/block_production/block_producer_test.cpp)

  target_link_libraries(koinos_block_producer_test PRIVATE
    koinos_block_producer_lib
    mempool_lib)

  add_test(NAME koinos_block_producer_test COMMAND koinos_block_producer_test)

  add_executable(koinos_mempool_adapter_test
    ../tests/mempool/mempool_adapter_test.cpp)

  target_link_libraries(koinos_mempool_adapter_test PRIVATE
    koinos_node_core
    koinos_node_interfaces
    mempool_lib)

  add_test(NAME koinos_mempool_adapter_test COMMAND koinos_mempool_adapter_test)

  add_executable(koinos_mempool_admission_atomicity_test
    ../tests/mempool/admission_atomicity_test.cpp)
  target_link_libraries(koinos_mempool_admission_atomicity_test PRIVATE mempool_lib)
  add_test(NAME koinos_mempool_admission_atomicity_test COMMAND koinos_mempool_admission_atomicity_test)
  set_tests_properties(koinos_mempool_admission_atomicity_test PROPERTIES TIMEOUT 60)

  add_executable(koinos_mempool_cleanup_atomicity_test ../tests/mempool/cleanup_atomicity_test.cpp)
  target_link_libraries(koinos_mempool_cleanup_atomicity_test PRIVATE mempool_lib)
  add_test(NAME koinos_mempool_cleanup_atomicity_test COMMAND koinos_mempool_cleanup_atomicity_test)
  set_tests_properties(koinos_mempool_cleanup_atomicity_test PROPERTIES TIMEOUT 120)

  add_executable(koinos_grpc_server_test
    ../tests/grpc/grpc_server_test.cpp)

  target_link_libraries(koinos_grpc_server_test PRIVATE
    koinos_grpc_server_lib
    koinos_contract_meta_store_lib
    koinos_transaction_store_lib
    koinos_account_history_lib)

  add_test(NAME koinos_grpc_server_test COMMAND koinos_grpc_server_test)
  add_test(NAME koinos_grpc_control_ownership_test COMMAND koinos_grpc_server_test --m1-control-owners-owned)
  add_test(NAME koinos_grpc_control_concurrency_test COMMAND koinos_grpc_server_test --m1-control-concurrency)
  add_test(NAME koinos_grpc_refusal_control_capacity_test COMMAND koinos_grpc_server_test --m1-refusal-control-capacity)

  add_executable(koinos_grpc_status_memory_test
    ../tests/grpc/status_memory_test.cpp)
  target_link_libraries(koinos_grpc_status_memory_test PRIVATE koinos_grpc_server_lib)
  add_test(NAME koinos_grpc_status_refusal_memory_test
    COMMAND koinos_grpc_status_memory_test refusal)
  add_test(NAME koinos_grpc_status_encoding_memory_test
    COMMAND koinos_grpc_status_memory_test encoding)
  add_test(NAME koinos_grpc_status_copy_failure_test
    COMMAND koinos_grpc_status_memory_test copy-failure)
  add_test(NAME koinos_grpc_status_argument_failure_test
    COMMAND koinos_grpc_status_memory_test argument-failure)
  add_test(NAME koinos_grpc_status_bounded_refusal_test
    COMMAND koinos_grpc_status_memory_test bounded-refusal)

  if(KOINOS_ENABLE_LIBP2P)
    add_executable(koinos_sdk_address_repository_test ../tests/p2p/sdk_address_repository_test.cpp)
    target_link_libraries(koinos_sdk_address_repository_test PRIVATE koinos_p2p_lib)
    target_compile_options(koinos_sdk_address_repository_test PRIVATE -UNDEBUG)
    add_test(NAME koinos_sdk_address_repository_test COMMAND koinos_sdk_address_repository_test)
    set_tests_properties(koinos_sdk_address_repository_test PROPERTIES TIMEOUT 30)
    if(TELENO_TSAN_SCOPE_OVERLAY)
      add_executable(koinos_tsan_scope_lifetime_test ../tests/p2p/tsan_scope_lifetime_test.cpp)
      target_include_directories(koinos_tsan_scope_lifetime_test PRIVATE "${TELENO_TSAN_SCOPE_OVERLAY}")
      target_link_libraries(koinos_tsan_scope_lifetime_test PRIVATE Threads::Threads)
      add_test(NAME koinos_tsan_scope_lifetime_test COMMAND koinos_tsan_scope_lifetime_test)
      set_tests_properties(koinos_tsan_scope_lifetime_test PROPERTIES TIMEOUT 30)
    endif()
    add_executable(koinos_peer_rpc_client_test ../tests/p2p/peer_rpc_client_test.cpp)
    target_link_libraries(koinos_peer_rpc_client_test PRIVATE koinos_p2p_lib)
    add_test(NAME koinos_peer_rpc_client_test COMMAND koinos_peer_rpc_client_test)
    set_tests_properties(koinos_peer_rpc_client_test PROPERTIES TIMEOUT 30)

    add_executable(koinos_peer_rpc_read_test ../tests/p2p/peer_rpc_read_test.cpp)
    target_link_libraries(koinos_peer_rpc_read_test PRIVATE koinos_p2p_lib)
    add_test(NAME koinos_peer_rpc_read_test COMMAND koinos_peer_rpc_read_test)
    set_tests_properties(koinos_peer_rpc_read_test PROPERTIES TIMEOUT 30)

    add_executable(koinos_peer_rpc_transport_test ../tests/p2p/peer_rpc_transport_test.cpp)
    target_link_libraries(koinos_peer_rpc_transport_test PRIVATE koinos_p2p_lib)
    add_test(NAME koinos_peer_rpc_transport_test COMMAND koinos_peer_rpc_transport_test)
    set_tests_properties(koinos_peer_rpc_transport_test PROPERTIES TIMEOUT 60)

    add_executable(koinos_peer_rpc_write_test ../tests/p2p/peer_rpc_write_test.cpp)
    target_link_libraries(koinos_peer_rpc_write_test PRIVATE koinos_p2p_lib)
    add_test(NAME koinos_peer_rpc_write_test COMMAND koinos_peer_rpc_write_test)
    set_tests_properties(koinos_peer_rpc_write_test PROPERTIES TIMEOUT 30)

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

  # The repository's native tests use assert() for their executable checks.
  # Release configurations define NDEBUG globally, so explicitly re-enable
  # assertions for test binaries only; production targets retain NDEBUG.
  set(_teleno_asserting_test_targets
    koinos_config_test
    koinos_service_registry_test
    koinos_controller_delta_test
    koinos_indexer_lookahead_test
    koinos_historical_fast_replay_fixture_test
    koinos_rocksdb_manager_test
    koinos_state_db_durability_test
    koinos_state_db_pending_root_test
    koinos_backup_plan_test
    koinos_backup_checkpoint_test
    koinos_backup_snapshot_test
    koinos_backup_sftp_test
    koinos_backup_public_restore_test
    koinos_backup_service_test
    koinos_backup_admin_server_test
    koinos_gorpc_codec_test
    koinos_p2p_one_peer_sync_test
    koinos_p2p_seen_cache_test
    koinos_peer_error_handler_test
    koinos_block_producer_test
    koinos_mempool_adapter_test
    koinos_grpc_server_test)
  if(KOINOS_ENABLE_LIBP2P)
    list(APPEND _teleno_asserting_test_targets
      koinos_peer_rpc_read_test
      koinos_peer_rpc_client_test
      koinos_peer_rpc_transport_test
      koinos_peer_rpc_write_test
      koinos_libp2p_peer_rpc_live_test
      koinos_libp2p_one_peer_sync_live_test
      koinos_libp2p_gossip_live_test)
  endif()
  foreach(_teleno_test_target IN LISTS _teleno_asserting_test_targets)
    if(MSVC)
      target_compile_options(${_teleno_test_target} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${_teleno_test_target} PRIVATE -UNDEBUG)
    endif()
  endforeach()
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
  jsonrpc/jsonrpc_server.hpp
  jsonrpc/json_cleanup.hpp)

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

if(KOINOS_BUILD_TESTS)
  add_executable(koinos_json_cleanup_test ../tests/jsonrpc/json_cleanup_test.cpp)
  target_include_directories(koinos_json_cleanup_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(koinos_json_cleanup_test PRIVATE nlohmann_json::nlohmann_json)
  add_test(NAME koinos_json_cleanup_test COMMAND koinos_json_cleanup_test)
  add_executable(koinos_json_default_cleanup_test ../tests/jsonrpc/json_default_cleanup_test.cpp)
  target_include_directories(koinos_json_default_cleanup_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(koinos_json_default_cleanup_test PRIVATE nlohmann_json::nlohmann_json Threads::Threads)
  add_test(NAME koinos_json_default_cleanup_test COMMAND koinos_json_default_cleanup_test)
  add_executable(koinos_json_cleanup_memory_profile ../tools/diagnostics/json_cleanup_memory_profile.cpp)
  target_include_directories(koinos_json_cleanup_memory_profile PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(koinos_json_cleanup_memory_profile PRIVATE nlohmann_json::nlohmann_json)
  add_executable(koinos_jsonrpc_session_lifecycle_test ../tests/jsonrpc/session_lifecycle_test.cpp)
  target_link_libraries(koinos_jsonrpc_session_lifecycle_test PRIVATE koinos_jsonrpc_lib)
  add_test(NAME koinos_jsonrpc_session_lifecycle_test COMMAND koinos_jsonrpc_session_lifecycle_test)
  set_tests_properties(koinos_jsonrpc_session_lifecycle_test PROPERTIES TIMEOUT 30)
  add_test(NAME koinos_jsonrpc_transaction_input_lifetime_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-transaction-input-lifetime)
  add_test(NAME koinos_jsonrpc_transaction_normalization_refusal_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-transaction-normalization-refusal)
  add_test(NAME koinos_jsonrpc_mempool_output_ownership_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-output-owned)
  set_tests_properties(koinos_jsonrpc_mempool_output_ownership_test PROPERTIES TIMEOUT 30)
  add_test(NAME koinos_jsonrpc_mempool_mixed_output_ownership_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-mixed-batch-owned)
  set_tests_properties(koinos_jsonrpc_mempool_mixed_output_ownership_test PROPERTIES TIMEOUT 30)
  add_test(NAME koinos_jsonrpc_mempool_mixed_capacity_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-mixed-batch-capacity)
  set_tests_properties(koinos_jsonrpc_mempool_mixed_capacity_test PROPERTIES TIMEOUT 30)
  add_test(NAME koinos_jsonrpc_mempool_output_roundtrip_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-output-roundtrip)
  set_tests_properties(koinos_jsonrpc_mempool_output_roundtrip_test PROPERTIES TIMEOUT 30)
  add_test(NAME koinos_jsonrpc_mempool_output_concurrent_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-output-concurrent)
  set_tests_properties(koinos_jsonrpc_mempool_output_concurrent_test PROPERTIES TIMEOUT 30)
  add_test(NAME koinos_jsonrpc_refusal_output_ownership_test COMMAND koinos_jsonrpc_session_lifecycle_test --http-refusal-owner-owned)
  set_tests_properties(koinos_jsonrpc_refusal_output_ownership_test PROPERTIES TIMEOUT 30)
  add_test(NAME koinos_jsonrpc_refusal_shapes_test COMMAND koinos_jsonrpc_session_lifecycle_test --http-refusal-shapes)
  add_test(NAME koinos_jsonrpc_refusal_gate_test COMMAND koinos_jsonrpc_session_lifecycle_test --http-refusal-gate)
  set_tests_properties(koinos_jsonrpc_refusal_shapes_test koinos_jsonrpc_refusal_gate_test PROPERTIES TIMEOUT 30)
  add_test(NAME koinos_jsonrpc_refusal_fault_cleanup_test COMMAND koinos_jsonrpc_session_lifecycle_test --http-refusal-faults)
  add_test(NAME koinos_jsonrpc_conversion_fault_cleanup_test COMMAND koinos_jsonrpc_session_lifecycle_test --generic-json-faults)
  set_tests_properties(koinos_jsonrpc_conversion_fault_cleanup_test PROPERTIES TIMEOUT 600)
  add_test(NAME koinos_jsonrpc_input_conversion_fault_cleanup_test COMMAND koinos_jsonrpc_session_lifecycle_test --generic-json-input-faults)
  set_tests_properties(koinos_jsonrpc_input_conversion_fault_cleanup_test PROPERTIES TIMEOUT 600)
  add_test(NAME koinos_jsonrpc_method_format_compatibility_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-method-format)
  set_tests_properties(koinos_jsonrpc_refusal_fault_cleanup_test PROPERTIES TIMEOUT 120)
  add_test(NAME koinos_jsonrpc_transaction_output_ownership_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-transaction-output-owned)
  set_tests_properties(koinos_jsonrpc_transaction_output_ownership_test PROPERTIES TIMEOUT 45)
  add_test(NAME koinos_jsonrpc_transaction_output_concurrent_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-transaction-output-concurrent)
  set_tests_properties(koinos_jsonrpc_transaction_output_concurrent_test PROPERTIES TIMEOUT 30)
  add_test(NAME koinos_jsonrpc_transaction_capacity_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-transaction-capacity)
  add_test(NAME koinos_jsonrpc_transaction_error_output_ownership_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-transaction-error-output-owned)
  set_tests_properties(koinos_jsonrpc_transaction_error_output_ownership_test PROPERTIES TIMEOUT 45)
  add_test(NAME koinos_jsonrpc_transaction_error_capacity_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-transaction-error-capacity)
  set_tests_properties(koinos_jsonrpc_transaction_capacity_test PROPERTIES TIMEOUT 30)
  add_test(NAME koinos_jsonrpc_transaction_output_roundtrip_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-transaction-roundtrip)
  set_tests_properties(koinos_jsonrpc_transaction_output_roundtrip_test PROPERTIES TIMEOUT 30)
  add_test(NAME koinos_jsonrpc_transaction_post_acceptance_failure_test COMMAND koinos_jsonrpc_session_lifecycle_test --m1-transaction-post-acceptance-failure)
  set_tests_properties(koinos_jsonrpc_transaction_post_acceptance_failure_test PROPERTIES TIMEOUT 30)
  add_executable(koinos_jsonrpc_batch_memory_test ../tests/jsonrpc/batch_memory_test.cpp)
  target_link_libraries(koinos_jsonrpc_batch_memory_test PRIVATE koinos_jsonrpc_lib)
  add_test(NAME koinos_jsonrpc_batch_memory_test COMMAND koinos_jsonrpc_batch_memory_test)
  set_tests_properties(koinos_jsonrpc_batch_memory_test PROPERTIES TIMEOUT 30)
endif()
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

add_executable(koinos_state_delta_replay_audit
  ../tools/state_delta_replay_audit.cpp)

target_link_libraries(
  koinos_state_delta_replay_audit
    PRIVATE
      koinos_block_store_lib
      koinos_storage_lib
      koinos_state_db_local
      chain
      Koinos::log
      Koinos::proto
      Koinos::crypto
      Koinos::util
      Koinos::exception
      RocksDB::rocksdb
      protobuf::libprotobuf)

add_executable(koinos_historical_indexing_validator
  ../tools/historical_indexing_validator.cpp)

target_link_libraries(
  koinos_historical_indexing_validator
    PRIVATE
      chain
      koinos_block_store_lib
      koinos_storage_lib
      Koinos::log
      Koinos::proto
      Koinos::util
      RocksDB::rocksdb
      protobuf::libprotobuf)

add_executable(koinos_import_block_store_stream
  ../tools/import_block_store_stream.cpp)

target_link_libraries(
  koinos_import_block_store_stream
    PRIVATE
      koinos_storage_lib
      RocksDB::rocksdb
      OpenSSL::Crypto)

if(COMMAND koinos_install)
  koinos_install(TARGETS teleno_node)
else()
  install(TARGETS teleno_node RUNTIME DESTINATION bin)
endif()
