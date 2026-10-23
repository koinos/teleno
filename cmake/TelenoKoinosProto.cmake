include_guard(GLOBAL)
include(FetchContent)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Keep the installed 2.6.0 message layout, descriptors and wire format. Rebuild
# the same generated sources with fully constructed copy targets and corrected
# protobuf inline ownership. Never modify the installed SDK or run protoc.
set(TELENO_KOINOS_PROTO_ARCHIVE
  "https://github.com/koinos/koinos-proto-cpp/archive/a24bb5ff91b2f245863bddb02496cc4831ad96c2.tar.gz"
  CACHE STRING "Pinned generated Koinos protobuf archive (SHA256 enforced)")
set(_manifest "${CMAKE_CURRENT_LIST_DIR}/patches/koinos-proto-copy-ownership-manifest.json")
set(_script "${CMAKE_CURRENT_LIST_DIR}/ApplyKoinosProtoOwnership.py")
file(SHA256 "${_manifest}" _manifest_hash)
file(SHA256 "${_script}" _script_hash)
string(SHA256 _patch_hash "${_manifest_hash}${_script_hash}")
string(SUBSTRING "${_patch_hash}" 0 12 _patch_id)
set(_content "teleno_koinos_proto_${_patch_id}")
string(TOUPPER "${_content}" _upper)
if(FETCHCONTENT_SOURCE_DIR_${_upper})
  message(FATAL_ERROR "Koinos protobuf source overrides bypass archive verification")
endif()
FetchContent_Declare(${_content}
  URL "${TELENO_KOINOS_PROTO_ARCHIVE}"
  URL_HASH SHA256=7152e153ff9a5af0add6a485faa83a1ce93803cd2c6bf9cf7b5fda5542c8832a
  SOURCE_SUBDIR teleno_population_only
  PATCH_COMMAND "${Python3_EXECUTABLE}" -B "${_script}" <SOURCE_DIR> "${_manifest}")
FetchContent_MakeAvailable(${_content})
FetchContent_GetProperties(${_content} SOURCE_DIR _source)

get_target_property(_includes Koinos::proto INTERFACE_INCLUDE_DIRECTORIES)
unset(_teleno_koinos_proto_install CACHE)
find_path(_teleno_koinos_proto_install koinos/serialization.hpp PATHS ${_includes} NO_DEFAULT_PATH)
if(NOT _teleno_koinos_proto_install)
  message(FATAL_ERROR "Cannot verify installed Koinos protobuf headers")
endif()
file(GLOB_RECURSE _headers RELATIVE "${_source}/include" "${_source}/include/*.h" "${_source}/include/*.hpp")
file(READ "${_manifest}" _ownership_manifest)
foreach(_header IN LISTS _headers)
  string(JSON _expected ERROR_VARIABLE _not_patched GET "${_ownership_manifest}" "include/${_header}" before)
  if(_not_patched)
    file(SHA256 "${_source}/include/${_header}" _expected)
  endif()
  file(SHA256 "${_teleno_koinos_proto_install}/${_header}" _actual)
  if(NOT _actual STREQUAL _expected)
    message(FATAL_ERROR "Installed Koinos protobuf header mismatch: ${_header}")
  endif()
endforeach()
execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${_script}" "${_source}" "${_manifest}"
  RESULT_VARIABLE _verified)
if(NOT _verified EQUAL 0)
  message(FATAL_ERROR "Generated Koinos protobuf ownership verification failed")
endif()
file(GLOB_RECURSE _generated "${_source}/src/*.pb.cc")
add_library(teleno_koinos_proto STATIC ${_generated} "${_source}/src/koinos/serialization.cpp")
target_include_directories(teleno_koinos_proto PUBLIC "${_source}/include" "${TELENO_PROTOBUF_SOURCE_DIR}/src")
target_link_libraries(teleno_koinos_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)
set_target_properties(teleno_koinos_proto PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/teleno-proto")
set(_library "${CMAKE_BINARY_DIR}/teleno-proto/${CMAKE_STATIC_LIBRARY_PREFIX}teleno_koinos_proto${CMAKE_STATIC_LIBRARY_SUFFIX}")
get_property(_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
get_target_property(_configs Koinos::proto IMPORTED_CONFIGURATIONS)
if(_multi)
  set(_configs ${CMAKE_CONFIGURATION_TYPES})
endif()
foreach(_config IN LISTS _configs)
  string(TOUPPER "${_config}" _config_upper)
  if(_multi)
    set(_location "${CMAKE_BINARY_DIR}/teleno-proto/${_config}/${CMAKE_STATIC_LIBRARY_PREFIX}teleno_koinos_proto${CMAKE_STATIC_LIBRARY_SUFFIX}")
  else()
    set(_location "${_library}")
  endif()
  set_property(TARGET Koinos::proto PROPERTY IMPORTED_LOCATION_${_config_upper} "${_location}")
endforeach()
set_property(TARGET Koinos::proto PROPERTY IMPORTED_LOCATION "${_library}")
add_dependencies(Koinos::proto teleno_koinos_proto)
# The installed SDK also exports these header paths. Directory-level BEFORE
# keeps both checked build-owned trees ahead of transitive imported includes
# for every native target, including test and generated-source translation
# units. A CMAKE_CXX_FLAGS -I arrives after target includes and is insufficient.
include_directories(BEFORE "${_source}/include" "${TELENO_PROTOBUF_SOURCE_DIR}/src")
set(TELENO_KOINOS_PROTO_SOURCE_DIR "${_source}")
message(STATUS "Teleno generated protobuf: a24bb5ff91b2f245863bddb02496cc4831ad96c2 + ownership ${_patch_hash}")
