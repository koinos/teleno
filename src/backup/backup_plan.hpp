#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/config.hpp"

namespace koinos::node::backup {

struct BackupPlanIssue
{
  std::string severity;
  std::string message;
};

struct BackupDryRunPlan
{
  bool enabled = false;
  std::string node_id;
  std::string basedir;
  std::string config_path;
  std::string workspace;

  bool schedule_enabled = false;
  std::string schedule_interval;
  bool schedule_run_on_startup_if_missed = true;
  uint64_t schedule_jitter_seconds = 0;
  uint64_t schedule_minimum_head_progress = 0;
  bool schedule_skip_if_syncing_from_genesis = true;
  uint64_t schedule_max_concurrent_backups = 1;

  bool local_enabled = false;
  std::string local_directory;
  uint64_t local_retention_count = 0;

  bool remote_enabled = false;
  std::string remote_directory;
  uint64_t remote_retention_count = 0;
  uint64_t remote_retention_days = 0;
  std::string remote_upload_temp_suffix;

  bool public_publish_enabled = false;
  std::string public_publish_directory;
  std::string public_publish_base_url;
  std::string public_publish_network;
  std::string public_publish_observer_config_file;
  uint64_t public_publish_retention_count = 0;
  std::string public_publish_upload_temp_suffix;

  bool ssh_enabled = false;
  std::string ssh_transport;
  std::string ssh_host;
  uint64_t ssh_port = 0;
  std::string ssh_user;
  std::string ssh_auth;
  std::string ssh_password_file;
  std::string ssh_private_key_file;
  std::string ssh_passphrase_file;
  std::string ssh_known_hosts_file;
  bool ssh_strict_host_key_checking = true;
  uint64_t ssh_connect_timeout_seconds = 0;

  bool source_db_exists = false;
  bool source_unified_chain = false;

  std::vector< BackupPlanIssue > issues;

  bool has_errors() const;
};

BackupDryRunPlan build_backup_dry_run_plan( const NodeConfig& cfg,
                                            const std::filesystem::path& basedir,
                                            const std::filesystem::path& config_path );

std::string backup_dry_run_plan_to_text( const BackupDryRunPlan& plan );
std::string backup_dry_run_plan_to_json( const BackupDryRunPlan& plan );

} // namespace koinos::node::backup
