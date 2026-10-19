cmake_minimum_required(VERSION 3.19)

# Only the build-owned, pinned archive is patched. Never patch an installation.
if(NOT IS_DIRECTORY "${FIZZY_SOURCE_DIR}" OR NOT FIZZY_PATCH)
  message(FATAL_ERROR "Missing build-owned Fizzy source or patch")
endif()
set(_file "${FIZZY_SOURCE_DIR}/lib/fizzy/capi.cpp")
set(_before "e4fe84df817ad13dda075138ccf0dd97868c307ed5815a81868a73b74a737774")
set(_after "ff5034e6de9dd84d1ffbf961ee19f8063dc39ed811ecbba3651f37e5e20c8e8b")
file(SHA256 "${_file}" _hash)
if(_hash STREQUAL _after)
  return()
endif()
if(NOT _hash STREQUAL _before)
  message(FATAL_ERROR "Unexpected Fizzy capi.cpp: refusing context patch")
endif()
find_package(Git REQUIRED)
# The extracted archive has no repository. An enclosing checkout would make
# Git silently skip extended-diff paths relative to that enclosing repository.
get_filename_component(_patch_root "${FIZZY_SOURCE_DIR}" REALPATH)
get_filename_component(_git_ceiling "${_patch_root}" DIRECTORY)
set(_patch_git "${CMAKE_COMMAND}" -E env --unset=GIT_DIR --unset=GIT_WORK_TREE
  "GIT_CEILING_DIRECTORIES=${_git_ceiling}" "${GIT_EXECUTABLE}")
execute_process(COMMAND ${_patch_git} apply --check "${FIZZY_PATCH}"
  WORKING_DIRECTORY "${FIZZY_SOURCE_DIR}" RESULT_VARIABLE _check)
if(NOT _check EQUAL 0)
  message(FATAL_ERROR "Pinned Fizzy patch preflight failed")
endif()
execute_process(COMMAND ${_patch_git} apply "${FIZZY_PATCH}"
  WORKING_DIRECTORY "${FIZZY_SOURCE_DIR}" RESULT_VARIABLE _applied)
file(SHA256 "${_file}" _hash)
if(NOT _applied EQUAL 0 OR NOT _hash STREQUAL _after)
  message(FATAL_ERROR "Pinned Fizzy patch result did not match")
endif()
