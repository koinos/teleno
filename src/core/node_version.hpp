#pragma once

#include "git_version.h"

namespace koinos::node {

inline constexpr const char* node_name()
{
  return "teleno_node";
}

inline constexpr const char* semantic_version()
{
  return KOINOS_NODE_VERSION;
}

inline constexpr const char* release_tag()
{
  return KOINOS_NODE_RELEASE_TAG;
}

inline constexpr const char* build_version()
{
  return KOINOS_NODE_BUILD_VERSION;
}

} // namespace koinos::node
