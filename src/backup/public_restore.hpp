#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

#include "backup/snapshot_repository.hpp"
#include "core/config.hpp"

namespace koinos::node::backup {

struct PublicRestoreFetchResult
{
  std::string backup_id;
  std::filesystem::path repository_dir;
  std::filesystem::path target_basedir;
  std::string public_base_url;
  std::string transport;
  RestorePreflightResult preflight;
  uint64_t metadata_file_count = 0;
  uint64_t object_file_count = 0;
  uint64_t object_bytes = 0;
  uint64_t repository_available_bytes = 0;
  uint64_t repository_required_bytes = 0;
  uint64_t request_count = 0;
  uint64_t retry_count = 0;
  bool metadata_fetched = false;
  bool objects_fetched = false;
  bool ready_to_stage = false;
  bool signature_required = false;
  bool signature_verified = false;
  std::string download_skipped_reason;
};

struct PublicRestoreProgress
{
  std::string phase;
  std::string backup_id;
  uint64_t completed_batches = 0;
  uint64_t total_batches = 0;
  uint64_t attempt = 0;
  uint64_t file_count = 0;
  uint64_t completed_bytes = 0;
  uint64_t total_bytes = 0;
};

struct PublicRestoreOptions
{
  std::function< bool() > cancel_requested;
  std::function< void( const PublicRestoreProgress& ) > progress;
};

BackupSnapshotListResult list_public_backup_snapshots(
  const std::filesystem::path& repository_dir,
  const BackupPublicRestoreConfig& public_restore,
  const std::string& backup_id = {},
  const PublicRestoreOptions& options = {} );

PublicRestoreFetchResult fetch_public_restore_snapshot(
  const std::filesystem::path& repository_dir,
  const std::filesystem::path& target_basedir,
  const BackupPublicRestoreConfig& public_restore,
  const std::string& backup_id = {},
  const PublicRestoreOptions& options = {} );

std::string public_restore_fetch_result_to_text( const PublicRestoreFetchResult& result );
std::string public_restore_fetch_result_to_json( const PublicRestoreFetchResult& result );

} // namespace koinos::node::backup
