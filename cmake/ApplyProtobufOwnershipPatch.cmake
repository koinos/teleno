cmake_minimum_required(VERSION 3.19)

# Patch only the build-owned pinned source, never a dependency installation.
if(NOT IS_DIRECTORY "${PROTOBUF_SOURCE_DIR}" OR NOT PROTOBUF_PATCH)
  message(FATAL_ERROR "Missing build-owned protobuf source or ownership patch")
endif()
set(_files unknown_field_set.cc arenastring.cc repeated_field.h util/internal/type_info.cc io/coded_stream.cc util/internal/proto_writer.cc)
set(_before
  "bd1d8452a66fea9f415e17b971a8c7c4ee1ba3d6c6d939f56abb46e07af0910e"
  "4593e7d1d80677053f3bf79f2d041f56dc0fc4ee810600c69c5b5fedfef6da89"
  "f66762384d1c7d778e5c13ab1c33b25f161758241bb408acc61c3c377feb507f"
  "bf15e8ee929fbf1639cda37487c45a7e9a08a94dcf4053ed44338f6394702f01"
  "1b020e3f9e2e5d04207cb714e97697d2612da84a427649510b9b26f961e0bd3e"
  "c68af956b2215f3d214b6dac17f7ed71dd959b5ba599cc55c19e6802de4e7da8")
set(_after
  "ad7ca08c6b2e32706e55cb0acc4e12611ef17703fba3a418e564653506ac1030"
  "08a52ed990b3707b65f39f342a8f0dfddfef25d9e2fe89b1af5b7be8c3faddc2"
  "004633591ac91eed57adf6ea139dfca59cbcec9ea98e7d7962ad3d978f869c20"
  "1b71c6fd10a861d91a60d667b3dc67e1e421c944b4d8fd263fbaffc571bbd8f9"
  "6522a5409787995173ed75c7c3082aab427fcafafa7282ddffb338c1c0b1f710"
  "9aa70909639ec766086c4045b3ba6bbd13bf0e4e4f46861ff012acdcee69611e")
set(_all_before TRUE)
set(_all_after TRUE)
foreach(_index RANGE 0 5)
  list(GET _files ${_index} _name)
  list(GET _before ${_index} _expected_before)
  list(GET _after ${_index} _expected_after)
  file(SHA256 "${PROTOBUF_SOURCE_DIR}/src/google/protobuf/${_name}" _hash)
  if(NOT _hash STREQUAL _expected_before)
    set(_all_before FALSE)
  endif()
  if(NOT _hash STREQUAL _expected_after)
    set(_all_after FALSE)
  endif()
endforeach()
if(_all_after)
  return()
endif()
if(NOT _all_before)
  message(FATAL_ERROR "Unexpected protobuf source or mixed patch state: refusing ownership patch")
endif()
find_package(Git REQUIRED)
get_filename_component(_patch_root "${PROTOBUF_SOURCE_DIR}" REALPATH)
get_filename_component(_git_ceiling "${_patch_root}" DIRECTORY)
set(_patch_git "${CMAKE_COMMAND}" -E env --unset=GIT_DIR --unset=GIT_WORK_TREE
  "GIT_CEILING_DIRECTORIES=${_git_ceiling}" "${GIT_EXECUTABLE}")
execute_process(COMMAND ${_patch_git} apply --check "${PROTOBUF_PATCH}"
  WORKING_DIRECTORY "${PROTOBUF_SOURCE_DIR}" RESULT_VARIABLE _check)
if(NOT _check EQUAL 0)
  message(FATAL_ERROR "Pinned protobuf ownership patch preflight failed")
endif()
execute_process(COMMAND ${_patch_git} apply "${PROTOBUF_PATCH}"
  WORKING_DIRECTORY "${PROTOBUF_SOURCE_DIR}" RESULT_VARIABLE _applied)
if(NOT _applied EQUAL 0)
  message(FATAL_ERROR "Pinned protobuf ownership patch result did not match")
endif()
foreach(_index RANGE 0 5)
  list(GET _files ${_index} _name)
  list(GET _after ${_index} _expected_after)
  file(SHA256 "${PROTOBUF_SOURCE_DIR}/src/google/protobuf/${_name}" _hash)
  if(NOT _hash STREQUAL _expected_after)
    message(FATAL_ERROR "Pinned protobuf ownership patch result did not match: ${_name}")
  endif()
endforeach()
