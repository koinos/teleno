include_guard(GLOBAL)
cmake_policy(PUSH)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
include(FetchContent)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)

# The pinned sync SDK constructs interceptor factories before binding ctx.call.
# Rebuild that one source file, then replace exactly its archive member in an
# owned copy. Keep the installed SDK and all other archive members unchanged.
set(TELENO_GRPC_ARCHIVE
  "https://github.com/cpp-pm/grpc/archive/v1.31.0-p0.tar.gz"
  CACHE STRING "Pinned gRPC call-binding source archive (SHA256 enforced)")
# Migrate only the formerly shipped missing default. Preserve operator-supplied
# local archives and other locations; the archive hash is unchanged.
if(TELENO_GRPC_ARCHIVE STREQUAL "https://github.com/hunter-packages/grpc/archive/v1.31.0-p0.tar.gz")
  set(TELENO_GRPC_ARCHIVE "https://github.com/cpp-pm/grpc/archive/v1.31.0-p0.tar.gz"
    CACHE STRING "Pinned gRPC call-binding source archive (SHA256 enforced)" FORCE)
endif()
set(TELENO_GRPC_BASE_ARCHIVE "" CACHE FILEPATH "Optional identified instrumented gRPC C++ archive")
set(TELENO_GRPC_BASE_SHA256 "" CACHE STRING "Required SHA256 for an explicit gRPC base archive")
set(_patch "${CMAKE_CURRENT_LIST_DIR}/ApplyGrpcCallBinding.py")
set(_manifest "${CMAKE_CURRENT_LIST_DIR}/patches/grpc-call-binding-manifest.json")
set(_repack "${CMAKE_CURRENT_LIST_DIR}/RepackGrpcCallBinding.py")
file(SHA256 "${_patch}" _patch_hash)
file(SHA256 "${_manifest}" _manifest_hash)
string(SHA256 _identity "${_patch_hash}${_manifest_hash}")
string(SUBSTRING "${_identity}" 0 12 _patch_id)
set(_content "teleno_grpc_call_binding_${_patch_id}")
string(TOUPPER "${_content}" _upper)
if(FETCHCONTENT_SOURCE_DIR_${_upper})
  message(FATAL_ERROR "gRPC source overrides bypass pinned archive verification")
endif()
FetchContent_Declare(${_content}
  URL "${TELENO_GRPC_ARCHIVE}"
  URL_HASH SHA256=48f52c566a03b3bea26e8440798d2ff4ce181221935e5983fb9495e13618dd55
  SOURCE_SUBDIR teleno_population_only
  PATCH_COMMAND "${Python3_EXECUTABLE}" -B "${_patch}" <SOURCE_DIR> "${_manifest}")
FetchContent_MakeAvailable(${_content})
FetchContent_GetProperties(${_content} SOURCE_DIR _source)
execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${_patch}" "${_source}" "${_manifest}"
  RESULT_VARIABLE _verified)
if(NOT _verified EQUAL 0)
  message(FATAL_ERROR "gRPC call-binding source verification failed")
endif()

get_target_property(_includes gRPC::grpc++ INTERFACE_INCLUDE_DIRECTORIES)
unset(_teleno_grpc_install CACHE)
find_path(_teleno_grpc_install grpcpp/grpcpp.h PATHS ${_includes} NO_DEFAULT_PATH)
if(NOT _teleno_grpc_install)
  message(FATAL_ERROR "Cannot verify installed gRPC public headers")
endif()
file(GLOB_RECURSE _headers RELATIVE "${_source}/include" "${_source}/include/*.h")
# This pinned SDK does not install its test/optional feature headers or these
# three source implementation entry points. They remain archive-hash pinned;
# every installed matching public header must agree byte for byte.
set(_not_installed_headers
  grpc/grpc_cronet.h grpcpp/resource_quota_impl.h grpcpp/server_builder_impl.h
  grpcpp/create_channel_posix_impl.h grpcpp/opencensus.h
  grpc++/test/server_context_test_spouse.h grpc++/test/mock_stream.h
  grpcpp/security/cronet_credentials.h grpcpp/security/cronet_credentials_impl.h
  grpcpp/test/default_reactor_test_peer.h grpcpp/test/server_context_test_spouse.h
  grpcpp/test/mock_stream.h grpcpp/test/channel_test_peer.h grpcpp/ext/server_load_reporting.h)
set(_verified_headers 0)
foreach(_header IN LISTS _headers)
  file(SHA256 "${_source}/include/${_header}" _expected)
  if(NOT EXISTS "${_teleno_grpc_install}/${_header}")
    if(_header IN_LIST _not_installed_headers)
      continue()
    endif()
    message(FATAL_ERROR "Missing installed pinned gRPC header: ${_header}")
  endif()
  file(SHA256 "${_teleno_grpc_install}/${_header}" _actual)
  if(NOT _actual STREQUAL _expected)
    message(FATAL_ERROR "Installed gRPC header mismatch: ${_header}")
  endif()
  math(EXPR _verified_headers "${_verified_headers}+1")
endforeach()

function(_teleno_grpc_base_for_config config output)
  if(TELENO_GRPC_BASE_ARCHIVE)
    if(NOT TELENO_GRPC_BASE_SHA256)
      message(FATAL_ERROR "An explicit gRPC archive requires TELENO_GRPC_BASE_SHA256")
    endif()
    set(_base "${TELENO_GRPC_BASE_ARCHIVE}")
  else()
    string(TOUPPER "${config}" _requested)
    get_target_property(_base gRPC::grpc++ IMPORTED_LOCATION_${_requested})
    if(NOT _base OR _base MATCHES "-NOTFOUND$")
      get_target_property(_mapped gRPC::grpc++ MAP_IMPORTED_CONFIG_${_requested})
      foreach(_mapping IN LISTS _mapped)
        string(TOUPPER "${_mapping}" _mapping_upper)
        get_target_property(_base gRPC::grpc++ IMPORTED_LOCATION_${_mapping_upper})
        if(_base AND NOT _base MATCHES "-NOTFOUND$")
          break()
        endif()
      endforeach()
    endif()
    if(NOT _base OR _base MATCHES "-NOTFOUND$")
      get_target_property(_base gRPC::grpc++ IMPORTED_LOCATION)
    endif()
    if(NOT _base OR _base MATCHES "-NOTFOUND$")
      get_target_property(_available gRPC::grpc++ IMPORTED_CONFIGURATIONS)
      foreach(_config IN LISTS _available)
        string(TOUPPER "${_config}" _upper_config)
        get_target_property(_base gRPC::grpc++ IMPORTED_LOCATION_${_upper_config})
        if(_base AND NOT _base MATCHES "-NOTFOUND$")
          break()
        endif()
      endforeach()
    endif()
  endif()
  if(NOT EXISTS "${_base}" OR NOT _base MATCHES "\\.a$")
    message(FATAL_ERROR "The pinned gRPC repair requires its identified static archive")
  endif()
  file(SHA256 "${_base}" _hash)
  if(TELENO_GRPC_BASE_ARCHIVE AND NOT _hash STREQUAL TELENO_GRPC_BASE_SHA256)
    message(FATAL_ERROR "Explicit gRPC base archive hash mismatch")
  endif()
  set(${output} "${_base}" PARENT_SCOPE)
endfunction()

add_library(teleno_grpc_call_binding OBJECT "${_source}/src/cpp/server/server_cc.cc")
set_target_properties(teleno_grpc_call_binding PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(teleno_grpc_call_binding PRIVATE cxx_std_11)
target_compile_definitions(teleno_grpc_call_binding PRIVATE CARES_STATICLIB)
target_include_directories(teleno_grpc_call_binding PRIVATE
  "${TELENO_PROTOBUF_SOURCE_DIR}/src" "${_source}/include" "${_source}"
  "${_source}/third_party/address_sorting/include" "${_source}/src/core/ext/upb-generated"
  "${_source}/third_party/upb" "${_teleno_grpc_install}")
target_link_libraries(teleno_grpc_call_binding PRIVATE protobuf::libprotobuf Threads::Threads)

get_property(_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_multi)
  if(CMAKE_VERSION VERSION_LESS 3.20)
    message(FATAL_ERROR "Multi-config gRPC call-binding builds require CMake 3.20 or newer")
  endif()
  set(_configs ${CMAKE_CONFIGURATION_TYPES})
  # Collect every original location before replacing any imported property.
  foreach(_config IN LISTS _configs)
    _teleno_grpc_base_for_config("${_config}" _base_${_config})
  endforeach()
else()
  get_target_property(_configs gRPC::grpc++ IMPORTED_CONFIGURATIONS)
  _teleno_grpc_base_for_config("${CMAKE_BUILD_TYPE}" _original)
endif()

function(_teleno_grpc_prepare_archive original directory output)
  get_filename_component(_name "${original}" NAME)
  set(_owned "${directory}/${_name}")
  file(SHA256 "${original}" _hash)
  add_custom_command(OUTPUT "${_owned}" "${_owned}.identity.json"
    COMMAND "${Python3_EXECUTABLE}" -B "${_repack}"
      --archive "${original}" --object $<TARGET_OBJECTS:teleno_grpc_call_binding>
      --output "${_owned}" --ar "${CMAKE_AR}" --ranlib "${CMAKE_RANLIB}"
      --expected-sha256 "${_hash}"
    DEPENDS teleno_grpc_call_binding $<TARGET_OBJECTS:teleno_grpc_call_binding> "${original}" "${_repack}"
    COMMAND_EXPAND_LISTS VERBATIM)
  set(${output} "${_owned}" PARENT_SCOPE)
  message(STATUS "Teleno gRPC call binding: ${_identity}; base ${_hash}; ${_verified_headers} installed headers verified")
endfunction()

set(_outputs "")
if(_multi)
  foreach(_config IN LISTS _configs)
    _teleno_grpc_prepare_archive("${_base_${_config}}" "${CMAKE_BINARY_DIR}/teleno-grpc/${_config}" _location)
    string(TOUPPER "${_config}" _upper_config)
    set_property(TARGET gRPC::grpc++ PROPERTY IMPORTED_LOCATION_${_upper_config} "${_location}")
    list(APPEND _outputs "$<$<CONFIG:${_config}>:${_location}>" "$<$<CONFIG:${_config}>:${_location}.identity.json>")
  endforeach()
else()
  _teleno_grpc_prepare_archive("${_original}" "${CMAKE_BINARY_DIR}/teleno-grpc" _output)
  set(_outputs "${_output}" "${_output}.identity.json")
  foreach(_config IN LISTS _configs)
    string(TOUPPER "${_config}" _upper_config)
    set_property(TARGET gRPC::grpc++ PROPERTY IMPORTED_LOCATION_${_upper_config} "${_output}")
  endforeach()
  set_property(TARGET gRPC::grpc++ PROPERTY IMPORTED_LOCATION "${_output}")
endif()
add_custom_target(teleno_grpc_patched_archive DEPENDS ${_outputs})
add_dependencies(gRPC::grpc++ teleno_grpc_patched_archive)
set(TELENO_GRPC_CALL_BINDING_SOURCE_DIR "${_source}")
cmake_policy(POP)
