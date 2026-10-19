#pragma once

#include <filesystem>
#include <string>

namespace koinos::node::backup {

struct DirectRestoreCliOptions
{
  std::filesystem::path target, config, metadata, attempt;
  std::string action, source_url, backup_id, expected_chain_id;
  std::string reserve_profile = "standard";
  bool resume = false, json = false, arguments_valid = true;
};
struct DirectRestoreCliResult { int exit_code; std::string output; };

// Standalone CLI only, before ordinary node initialization. Never starts
// services or production; activation is a separate explicit action.
DirectRestoreCliResult run_direct_restore_cli(const DirectRestoreCliOptions&);

} // namespace koinos::node::backup
