include_guard(GLOBAL)
include(FetchContent)

if(NOT TELENO_NLOHMANN_JSON_INCLUDE_DIR OR NOT TARGET Koinos::exception OR NOT TARGET Koinos::crypto)
  message(FATAL_ERROR "JSON cleanup requires its staged headers and the pinned Koinos SDK targets")
endif()

# These two installed libraries contain nlohmann template instantiations.
# Rebuild their unchanged pinned sources with the same headers as the node;
# never mix the old destructor into native exception/crypto JSON paths.
set(TELENO_KOINOS_EXCEPTION_ARCHIVE
  "https://github.com/koinos/koinos-exception-cpp/archive/refs/tags/v1.0.2.tar.gz"
  CACHE STRING "Pinned Koinos exception archive URL or local archive (SHA256 enforced)")
set(TELENO_KOINOS_CRYPTO_ARCHIVE
  "https://github.com/koinos/koinos-crypto-cpp/archive/refs/tags/v1.0.2.tar.gz"
  CACHE STRING "Pinned Koinos crypto archive URL or local archive (SHA256 enforced)")
set(_json_sdk_manifest "${CMAKE_CURRENT_LIST_DIR}/patches/nlohmann-sdk-source-manifest.json")
file(READ "${_json_sdk_manifest}" _json_sdk_sources)
get_property(_json_sdk_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

function(_teleno_json_sdk_source kind index archive digest)
  string(SUBSTRING "${digest}" 0 12 _id)
  set(_content "teleno_json_sdk_${kind}_${_id}")
  string(TOUPPER "${_content}" _upper)
  if(FETCHCONTENT_SOURCE_DIR_${_upper})
    message(FATAL_ERROR "SDK source overrides bypass verification; use the pinned archive option")
  endif()
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
  FetchContent_Declare(${_content}
    URL "${archive}" URL_HASH "SHA256=${digest}"
    SOURCE_SUBDIR teleno_population_only)
  FetchContent_MakeAvailable(${_content})
  FetchContent_GetProperties(${_content} SOURCE_DIR _source)

  string(JSON _count LENGTH "${_json_sdk_sources}" ${index} files_sha256)
  math(EXPR _last "${_count} - 1")
  set(_expected_files "")
  foreach(_i RANGE 0 ${_last})
    string(JSON _file MEMBER "${_json_sdk_sources}" ${index} files_sha256 ${_i})
    list(APPEND _expected_files "${_file}")
    string(JSON _expected GET "${_json_sdk_sources}" ${index} files_sha256 "${_file}")
    file(SHA256 "${_source}/${_file}" _actual)
    if(NOT _actual STREQUAL _expected)
      message(FATAL_ERROR "Unexpected pinned SDK source: ${kind}/${_file}")
    endif()
  endforeach()
  file(GLOB_RECURSE _actual_files LIST_DIRECTORIES FALSE RELATIVE "${_source}" "${_source}/*")
  list(SORT _actual_files)
  list(SORT _expected_files)
  if(NOT _actual_files STREQUAL _expected_files)
    message(FATAL_ERROR "Unexpected files in the pinned ${kind} source tree")
  endif()
  get_target_property(_installed_includes Koinos::${kind} INTERFACE_INCLUDE_DIRECTORIES)
  set(_first_header "koinos/exception.hpp")
  if(kind STREQUAL "crypto")
    set(_first_header "koinos/crypto/multihash.hpp")
  endif()
  unset(_teleno_sdk_installed_include CACHE)
  find_path(_teleno_sdk_installed_include "${_first_header}"
    PATHS ${_installed_includes} NO_DEFAULT_PATH)
  if(NOT _teleno_sdk_installed_include)
    message(FATAL_ERROR "Cannot verify the pinned ${kind} public headers")
  endif()
  file(GLOB_RECURSE _headers RELATIVE "${_source}/include" "${_source}/include/*.hpp")
  foreach(_header IN LISTS _headers)
    file(SHA256 "${_source}/include/${_header}" _source_header)
    file(SHA256 "${_teleno_sdk_installed_include}/${_header}" _installed_header)
    if(NOT _source_header STREQUAL _installed_header)
      message(FATAL_ERROR "Pinned SDK public header mismatch: ${kind}/${_header}")
    endif()
  endforeach()
  unset(_teleno_sdk_installed_include CACHE)
  set(TELENO_JSON_${kind}_SOURCE_DIR "${_source}" PARENT_SCOPE)
endfunction()

_teleno_json_sdk_source(exception 0 "${TELENO_KOINOS_EXCEPTION_ARCHIVE}"
  "22e9593ca476be7a74a94a5f2d0ab02adc29c163e9c88d288b7ce91c1fc9b5af")
_teleno_json_sdk_source(crypto 1 "${TELENO_KOINOS_CRYPTO_ARCHIVE}"
  "45f8bf8f1d337ae18c3f8f0196d4e9da1902ec301b49f453b876d02a9bf8be18")

add_library(teleno_json_exception STATIC
  "${TELENO_JSON_exception_SOURCE_DIR}/src/koinos/exception.cpp")
target_link_libraries(teleno_json_exception PRIVATE
  Boost::exception Koinos::log Koinos::proto nlohmann_json::nlohmann_json protobuf::libprotobuf)
add_library(teleno_json_crypto STATIC
  "${TELENO_JSON_crypto_SOURCE_DIR}/src/koinos/crypto/elliptic.cpp"
  "${TELENO_JSON_crypto_SOURCE_DIR}/src/koinos/crypto/multihash.cpp"
  "${TELENO_JSON_crypto_SOURCE_DIR}/src/koinos/crypto/openssl.cpp")
target_link_libraries(teleno_json_crypto PRIVATE
  Koinos::exception Koinos::proto Koinos::util OpenSSL::SSL OpenSSL::Crypto
  ethash::ethash secp256k1-vrf::secp256k1-vrf)

foreach(_kind exception crypto)
  set(_target "teleno_json_${_kind}")
  target_include_directories(${_target} BEFORE PRIVATE
    "${TELENO_NLOHMANN_JSON_INCLUDE_DIR}"
    "${TELENO_PROTOBUF_SOURCE_DIR}/src"
    "${TELENO_KOINOS_PROTO_SOURCE_DIR}/include"
    "${TELENO_JSON_exception_SOURCE_DIR}/include"
    "${TELENO_JSON_crypto_SOURCE_DIR}/include")
  set(_directory "${CMAKE_BINARY_DIR}/_deps/teleno_json_sdk_${_json_patch_id}/lib")
  set_target_properties(${_target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${_directory}"
    OUTPUT_NAME "teleno_koinos_${_kind}")
  set(_name "${CMAKE_STATIC_LIBRARY_PREFIX}teleno_koinos_${_kind}${CMAKE_STATIC_LIBRARY_SUFFIX}")
  set(_location "${_directory}/${_name}")
  get_target_property(_configs Koinos::${_kind} IMPORTED_CONFIGURATIONS)
  if(_json_sdk_multi)
    set(_location "${_directory}/$<CONFIG>/${_name}")
    set(_configs ${CMAKE_CONFIGURATION_TYPES})
    set_property(TARGET Koinos::${_kind} PROPERTY IMPORTED_CONFIGURATIONS "${_configs}")
  endif()
  set_property(TARGET Koinos::${_kind} PROPERTY IMPORTED_LOCATION "${_location}")
  foreach(_config IN LISTS _configs)
    string(TOUPPER "${_config}" _upper)
    if(_json_sdk_multi)
      set(_location "${_directory}/${_config}/${_name}")
      set_property(TARGET Koinos::${_kind} PROPERTY MAP_IMPORTED_CONFIG_${_upper} "${_config}")
    endif()
    set_property(TARGET Koinos::${_kind} PROPERTY IMPORTED_LOCATION_${_upper} "${_location}")
  endforeach()
  add_dependencies(Koinos::${_kind} ${_target})
endforeach()
message(STATUS "Teleno Koinos exception/crypto: pinned 1.0.2 sources rebuilt with JSON cleanup headers")
