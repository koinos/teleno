#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "backup/snapshot_repository.hpp"

namespace koinos::node::backup {

struct RestoreActivationIntent
{
  std::filesystem::path target_basedir;
  std::filesystem::path staging_dir;
  std::filesystem::path intent_path;
  bool requires_node_stop = true;
};

std::filesystem::path restore_activation_intent_path( const std::filesystem::path& basedir );
std::optional< RestoreActivationIntent > read_pending_restore_activation_request(
  const std::filesystem::path& basedir );
bool has_pending_restore_activation_request( const std::filesystem::path& basedir );
RestoreActivationResult activate_pending_restore_activation_request(
  const std::filesystem::path& basedir );
std::string restore_activation_intent_to_json( const RestoreActivationIntent& intent );

} // namespace koinos::node::backup
