include_guard(GLOBAL)
include(FetchContent)
include(ExternalProject)

# Keep the Koinos protobuf revision, wire format and installed public headers.
# Rebuild the runtime in this build tree with exception-safe unknown-field
# publication and arena string copying. Existing Hunter installations and
# protoc remain read-only inputs.
if(NOT TARGET protobuf::libprotobuf OR NOT Protobuf_VERSION VERSION_EQUAL "3.17.3.0")
  message(FATAL_ERROR "Teleno requires the pinned Koinos protobuf 3.17.3.0 package")
endif()
get_target_property(_protobuf_type protobuf::libprotobuf TYPE)
if(NOT _protobuf_type STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "Teleno requires the pinned static protobuf runtime")
endif()
set(TELENO_PROTOBUF_ARCHIVE
  "https://github.com/koinos/protobuf/archive/e1b1477875a8b022903b548eb144f2c7bf4d9561.tar.gz"
  CACHE STRING "Pinned protobuf archive URL or local archive (SHA256 always enforced)")
set(TELENO_PROTOBUF_BUILD_JOBS 2 CACHE STRING "Parallel jobs for the isolated protobuf runtime build")
if(NOT TELENO_PROTOBUF_BUILD_JOBS MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "TELENO_PROTOBUF_BUILD_JOBS must be a positive integer")
endif()
set(_patch "${CMAKE_CURRENT_LIST_DIR}/patches/protobuf-unknown-field-ownership.patch")
file(SHA256 "${_patch}" _patch_hash)
string(SUBSTRING "${_patch_hash}" 0 12 _patch_id)
set(_content "teleno_protobuf_${_patch_id}")
string(TOUPPER "${_content}" _content_upper)
if(FETCHCONTENT_SOURCE_DIR_${_content_upper})
  message(FATAL_ERROR "Protobuf source overrides bypass archive verification; use TELENO_PROTOBUF_ARCHIVE")
endif()
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(${_content}
  URL "${TELENO_PROTOBUF_ARCHIVE}"
  URL_HASH SHA256=824eca40d5a9b7e1090961e84efe7b90fba818156c32d92e447f7daf61522318
  SOURCE_SUBDIR teleno_population_only
  PATCH_COMMAND "${CMAKE_COMMAND}" "-DPROTOBUF_SOURCE_DIR=<SOURCE_DIR>"
    "-DPROTOBUF_PATCH=${_patch}" -P "${CMAKE_CURRENT_LIST_DIR}/ApplyProtobufOwnershipPatch.cmake")
FetchContent_MakeAvailable(${_content})
FetchContent_GetProperties(${_content} SOURCE_DIR _source BINARY_DIR _binary)
file(SHA256 "${_source}/src/google/protobuf/unknown_field_set.cc" _source_hash)
if(NOT _source_hash STREQUAL "090f66ae063d27fe1ee03b5c34b6be903cf75c69ffa008336cc1ec32c8801d8a")
  message(FATAL_ERROR "Protobuf ownership patch is absent or changed")
endif()
file(SHA256 "${_source}/src/google/protobuf/arenastring.cc" _source_hash)
if(NOT _source_hash STREQUAL "08a52ed990b3707b65f39f342a8f0dfddfef25d9e2fe89b1af5b7be8c3faddc2")
  message(FATAL_ERROR "Protobuf arena string patch is absent or changed")
endif()

# A mismatched header/runtime pair is not an acceptable fallback.
get_target_property(_includes protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
unset(_teleno_protobuf_include CACHE)
find_path(_teleno_protobuf_include google/protobuf/stubs/common.h
  PATHS ${_includes} NO_DEFAULT_PATH)
if(NOT _teleno_protobuf_include)
  message(FATAL_ERROR "Cannot verify the pinned protobuf public headers")
endif()
foreach(_header stubs/common.h unknown_field_set.h arena.h arenastring.h)
  file(SHA256 "${_source}/src/google/protobuf/${_header}" _expected)
  file(SHA256 "${_teleno_protobuf_include}/google/protobuf/${_header}" _installed)
  if(NOT _installed STREQUAL _expected)
    message(FATAL_ERROR "Pinned protobuf public header does not match: ${_header}")
  endif()
endforeach()
unset(_teleno_protobuf_include CACHE)

set(_args -DHUNTER_ENABLED=OFF -Dprotobuf_BUILD_TESTS=OFF
  -Dprotobuf_BUILD_CONFORMANCE=OFF -Dprotobuf_BUILD_EXAMPLES=OFF
  -Dprotobuf_BUILD_PROTOC_BINARIES=OFF -Dprotobuf_BUILD_LIBPROTOC=OFF
  -Dprotobuf_BUILD_SHARED_LIBS=OFF -Dprotobuf_DEBUG_POSTFIX=
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  "-DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITION_INDEPENDENT_CODE}")
foreach(_var CMAKE_BUILD_TYPE CMAKE_TOOLCHAIN_FILE CMAKE_C_COMPILER CMAKE_CXX_COMPILER
    CMAKE_C_COMPILER_TARGET CMAKE_CXX_COMPILER_TARGET CMAKE_SYSROOT
    CMAKE_OSX_ARCHITECTURES CMAKE_OSX_SYSROOT CMAKE_OSX_DEPLOYMENT_TARGET
    CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS
    CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE
    CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO
    CMAKE_C_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_MINSIZEREL)
  if(DEFINED ${_var})
    string(REPLACE ";" "|" _value "${${_var}}")
    list(APPEND _args "-D${_var}:STRING=${_value}")
  endif()
endforeach()
if(MSVC)
  set(_library_name "libprotobuf${CMAKE_STATIC_LIBRARY_SUFFIX}")
else()
  set(_library_name "${CMAKE_STATIC_LIBRARY_PREFIX}protobuf${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()
set(_library "${_binary}/${_library_name}")
get_property(_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set(_byproducts "${_library}")
if(_multi)
  set(_library "${_binary}/$<CONFIG>/${_library_name}")
  set(_byproducts "")
  foreach(_config IN LISTS CMAKE_CONFIGURATION_TYPES)
    list(APPEND _byproducts
      "${_binary}/${_config}/${_library_name}")
  endforeach()
endif()
ExternalProject_Add(teleno_protobuf_build
  PREFIX "${CMAKE_BINARY_DIR}/_deps/${_content}-external"
  SOURCE_DIR "${_source}/cmake" BINARY_DIR "${_binary}"
  DOWNLOAD_COMMAND "" UPDATE_COMMAND "" PATCH_COMMAND ""
  CMAKE_ARGS ${_args} LIST_SEPARATOR "|"
  BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config $<CONFIG>
    --target libprotobuf --parallel "${TELENO_PROTOBUF_BUILD_JOBS}"
  BUILD_BYPRODUCTS ${_byproducts} BUILD_ALWAYS TRUE INSTALL_COMMAND "")
set_target_properties(protobuf::libprotobuf PROPERTIES IMPORTED_LOCATION "${_library}")
get_target_property(_configs protobuf::libprotobuf IMPORTED_CONFIGURATIONS)
if(_multi)
  set(_configs ${CMAKE_CONFIGURATION_TYPES})
  set_property(TARGET protobuf::libprotobuf PROPERTY IMPORTED_CONFIGURATIONS "${_configs}")
endif()
foreach(_config IN LISTS _configs)
  string(TOUPPER "${_config}" _config_upper)
  if(_multi)
    set(_location "${_binary}/${_config}/${_library_name}")
    set_property(TARGET protobuf::libprotobuf PROPERTY MAP_IMPORTED_CONFIG_${_config_upper} "${_config}")
  else()
    set(_location "${_library}")
  endif()
  set_property(TARGET protobuf::libprotobuf PROPERTY IMPORTED_LOCATION_${_config_upper} "${_location}")
endforeach()
add_dependencies(protobuf::libprotobuf teleno_protobuf_build)
set(TELENO_PROTOBUF_SOURCE_DIR "${_source}")
message(STATUS "Teleno protobuf: e1b1477875a8b022903b548eb144f2c7bf4d9561 + ownership patch ${_patch_hash}")
