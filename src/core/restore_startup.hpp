#pragma once

#include "core/config.hpp"

#include <filesystem>

namespace koinos::node {

struct RestoreStartupPolicyResult
{
  bool restore_marker_found                = false;
  bool producer_recovery_hold_active       = false;
  bool producer_recovery_hold_released     = false;
  bool block_producer_was_enabled          = false;
  bool verify_blocks                       = false;
};

/**
 * Consume the restore marker and force observer-first startup without changing
 * the explicitly configured block-verification mode.
 */
RestoreStartupPolicyResult apply_restore_startup_policy(
  NodeConfig& config,
  const std::filesystem::path& basedir,
  bool explicit_producer_activation = false );

} // namespace koinos::node
