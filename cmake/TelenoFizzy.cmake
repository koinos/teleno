include_guard(GLOBAL)
include(FetchContent)
include(ExternalProject)

# Preserve the Koinos fork/revision and its upstream compiler/platform checks.
# Existing Hunter installations are inputs only; the corrected static library
# belongs to this build tree. All native entrypoints use this recipe.
if(TARGET fizzy::fizzy)
  message(FATAL_ERROR "Teleno requires its pinned, corrected Fizzy build")
endif()
set(TELENO_FIZZY_ARCHIVE
  "https://github.com/koinos/fizzy/archive/b9bf7feaa8009a3d4f4bdd49245a5cd55d122055.tar.gz"
  CACHE STRING "Pinned Fizzy archive URL or local archive (SHA256 always enforced)")
set(TELENO_FIZZY_BUILD_JOBS 2 CACHE STRING "Parallel jobs for the isolated Fizzy build")
if(NOT TELENO_FIZZY_BUILD_JOBS MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "TELENO_FIZZY_BUILD_JOBS must be a positive integer")
endif()
set(_patch "${CMAKE_CURRENT_LIST_DIR}/patches/fizzy-context-allocation.patch")
file(SHA256 "${_patch}" _patch_hash)
string(SUBSTRING "${_patch_hash}" 0 12 _patch_id)
set(_content "teleno_fizzy_${_patch_id}")
string(TOUPPER "${_content}" _content_upper)
if(FETCHCONTENT_SOURCE_DIR_${_content_upper})
  message(FATAL_ERROR "Fizzy source overrides bypass archive verification; use TELENO_FIZZY_ARCHIVE")
endif()
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(${_content}
  URL "${TELENO_FIZZY_ARCHIVE}"
  URL_HASH SHA256=40975339c2e8a0f7aa1693866945becb40a65e3c0cdee862cc1ee760b99b7533
  SOURCE_SUBDIR teleno_population_only
  PATCH_COMMAND "${CMAKE_COMMAND}" "-DFIZZY_SOURCE_DIR=<SOURCE_DIR>"
    "-DFIZZY_PATCH=${_patch}" -P "${CMAKE_CURRENT_LIST_DIR}/ApplyFizzyContextPatch.cmake")
FetchContent_MakeAvailable(${_content})
FetchContent_GetProperties(${_content} SOURCE_DIR _source BINARY_DIR _binary)
file(SHA256 "${_source}/lib/fizzy/capi.cpp" _capi_hash)
if(NOT _capi_hash STREQUAL "ff5034e6de9dd84d1ffbf961ee19f8063dc39ed811ecbba3651f37e5e20c8e8b")
  message(FATAL_ERROR "Fizzy context patch is absent or changed")
endif()

set(_args -DFIZZY_TESTING=OFF -DFIZZY_WASI=OFF -DHUNTER_ENABLED=OFF
  -DBUILD_SHARED_LIBS=OFF "-DCMAKE_POSITION_INDEPENDENT_CODE=${CMAKE_POSITION_INDEPENDENT_CODE}")
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
get_property(_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set(_library "${_binary}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}fizzy${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(_byproducts "${_library}")
if(_multi)
  set(_library "${_binary}/lib/$<CONFIG>/${CMAKE_STATIC_LIBRARY_PREFIX}fizzy${CMAKE_STATIC_LIBRARY_SUFFIX}")
  # Explicit paths keep BUILD_BYPRODUCTS compatible with CMake 3.19.
  set(_byproducts "")
  foreach(_config IN LISTS CMAKE_CONFIGURATION_TYPES)
    list(APPEND _byproducts "${_binary}/lib/${_config}/${CMAKE_STATIC_LIBRARY_PREFIX}fizzy${CMAKE_STATIC_LIBRARY_SUFFIX}")
  endforeach()
endif()
ExternalProject_Add(teleno_fizzy_build
  PREFIX "${CMAKE_BINARY_DIR}/_deps/${_content}-external"
  SOURCE_DIR "${_source}" BINARY_DIR "${_binary}"
  DOWNLOAD_COMMAND "" UPDATE_COMMAND "" PATCH_COMMAND ""
  CMAKE_ARGS ${_args} LIST_SEPARATOR "|"
  BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config $<CONFIG>
    --target fizzy --parallel "${TELENO_FIZZY_BUILD_JOBS}"
  BUILD_BYPRODUCTS ${_byproducts} BUILD_ALWAYS TRUE INSTALL_COMMAND "")
add_library(fizzy::fizzy STATIC IMPORTED GLOBAL)
set_target_properties(fizzy::fizzy PROPERTIES
  IMPORTED_LOCATION "${_library}" INTERFACE_INCLUDE_DIRECTORIES "${_source}/include"
  INTERFACE_COMPILE_FEATURES cxx_std_17)
if(_multi)
  foreach(_config IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${_config}" _config_upper)
    set_property(TARGET fizzy::fizzy PROPERTY IMPORTED_LOCATION_${_config_upper}
      "${_binary}/lib/${_config}/${CMAKE_STATIC_LIBRARY_PREFIX}fizzy${CMAKE_STATIC_LIBRARY_SUFFIX}")
  endforeach()
endif()
add_dependencies(fizzy::fizzy teleno_fizzy_build)
set(TELENO_FIZZY_SOURCE_DIR "${_source}")
message(STATUS "Teleno Fizzy: b9bf7feaa8009a3d4f4bdd49245a5cd55d122055 + context patch ${_patch_hash}")
