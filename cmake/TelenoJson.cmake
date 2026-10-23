include_guard(GLOBAL)

if(NOT TARGET nlohmann_json::nlohmann_json)
  message(FATAL_ERROR "Teleno requires the pinned nlohmann JSON package")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_target_property(_json_includes nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
unset(_teleno_json_installed_include CACHE)
find_path(_teleno_json_installed_include nlohmann/json.hpp
  PATHS ${_json_includes} NO_DEFAULT_PATH)
if(NOT _teleno_json_installed_include)
  message(FATAL_ERROR "Cannot locate the pinned nlohmann JSON public headers")
endif()
set(_json_patch "${CMAKE_CURRENT_LIST_DIR}/patches/nlohmann-json-allocation-free-destroy.patch")
set(_json_manifest "${CMAKE_CURRENT_LIST_DIR}/patches/nlohmann-json-cleanup-manifest.json")
file(SHA256 "${_json_patch}" _json_patch_hash)
string(SUBSTRING "${_json_patch_hash}" 0 12 _json_patch_id)
set(TELENO_NLOHMANN_JSON_ROOT "${CMAKE_BINARY_DIR}/_deps/teleno_json_${_json_patch_id}")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_LIST_DIR}/ApplyNlohmannCleanup.py"
    --installed-include "${_teleno_json_installed_include}"
    --output-root "${TELENO_NLOHMANN_JSON_ROOT}"
    --manifest "${_json_manifest}" --patch "${_json_patch}"
  RESULT_VARIABLE _json_staged)
if(NOT _json_staged EQUAL 0)
  message(FATAL_ERROR "Pinned build-owned JSON cleanup staging failed")
endif()
set(TELENO_NLOHMANN_JSON_INCLUDE_DIR "${TELENO_NLOHMANN_JSON_ROOT}/include")
set_property(TARGET nlohmann_json::nlohmann_json PROPERTY
  INTERFACE_INCLUDE_DIRECTORIES "${TELENO_NLOHMANN_JSON_INCLUDE_DIR}")
# Other SDK targets share their installed include prefix, so changing only the
# JSON imported target is insufficient to choose the corrected header first.
include_directories(BEFORE "${TELENO_NLOHMANN_JSON_INCLUDE_DIR}")
unset(_teleno_json_installed_include CACHE)
message(STATUS "Teleno nlohmann JSON: 3.11.2 + cleanup patch ${_json_patch_hash}")
