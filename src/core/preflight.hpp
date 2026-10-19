#pragma once

#include "config.hpp"
#include <nlohmann/json.hpp>

namespace koinos::node {

// Contains observations only; never certifies runtime or producer readiness.
struct PreflightReport
{
  nlohmann::json checks = nlohmann::json::array();
  void add( const std::string& id, const std::string& status,
            const std::string& observed, const std::string& action = {} );
  int exit_code() const; // 0: no known blocker, 1: failed check, 2: incomplete/error
  std::string json() const;
  std::string text() const;
};

PreflightReport run_preflight( const NodeConfig& cfg );

} // namespace koinos::node
