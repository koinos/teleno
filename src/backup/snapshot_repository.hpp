#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "backup/checkpoint_manager.hpp"
#include "core/config.hpp"

namespace koinos::node::backup {

struct SnapshotFileEntry
{
  std::string path;
  std::string sha256;
  uint64_t size_bytes = 0;
  bool runtime_file = false;
};

struct RestoreSpaceEstimate
{
  uint64_t restored_database_bytes = 0;
  uint64_t runtime_files_bytes = 0;
  uint64_t object_download_bytes = 0;
  uint64_t archive_bytes = 0;
  uint64_t existing_target_bytes = 0;
  bool streaming_archive = false;
  uint64_t minimum_target_free_bytes = 0;
  uint64_t recommended_target_free_bytes = 0;
};

struct RestoreSpaceCheck
{
  bool passes_minimum = false;
  bool below_recommended = false;
  uint64_t available_bytes = 0;
  std::string target_path;
  std::string message;
};

struct RestorePreflightResult
{
  std::string backup_id;
  std::filesystem::path repository_dir;
  std::filesystem::path snapshot_dir;
  std::filesystem::path manifest_path;
  std::filesystem::path files_path;
  std::filesystem::path target_basedir;
  uint64_t file_count = 0;
  uint64_t missing_object_count = 0;
  uint64_t missing_object_bytes = 0;
  bool snapshot_complete = false;
  bool start_as_observer_first = true;
  bool ready_to_restore = false;
  RestoreSpaceEstimate restore_space;
  RestoreSpaceCheck space_check;
};

struct RestoreStageResult
{
  RestorePreflightResult preflight;
  std::filesystem::path staging_dir;
  std::filesystem::path metadata_path;
  uint64_t restored_file_count = 0;
  uint64_t restored_bytes = 0;
  std::vector< std::string > skipped_optional_runtime_files;
};

struct RestoreActivatedPath
{
  std::string relative_path;
  std::filesystem::path preserved_path;
};

struct RestoreActivationResult
{
  std::string backup_id;
  std::filesystem::path target_basedir;
  std::filesystem::path staging_dir;
  std::filesystem::path pre_restore_dir;
  std::filesystem::path marker_path;
  std::filesystem::path restore_manifest_path;
  std::vector< RestoreActivatedPath > preserved_paths;
  bool block_producer_disabled_on_first_start = true;
  bool start_as_observer_first = true;
};

struct LocalSnapshotResult
{
  std::string backup_id;
  std::filesystem::path repository_dir;
  std::filesystem::path snapshot_dir;
  std::filesystem::path manifest_path;
  std::filesystem::path files_path;
  std::filesystem::path latest_path;
  uint64_t file_count = 0;
  uint64_t object_count = 0;
  uint64_t new_object_count = 0;
  uint64_t reused_object_count = 0;
  uint64_t total_bytes = 0;
  RestoreSpaceEstimate restore_space;
};

struct BackupSnapshotSummary
{
  std::string backup_id;
  std::string created_at;
  std::string node_version;
  std::string node_id;
  std::string storage_layout;
  std::string network;
  std::string chain_id;
  std::string public_base_url;
  std::string promoted_at;
  std::string source_backup_id;
  std::string source_created_at;
  std::string source_node_version;
  std::filesystem::path repository_dir;
  std::filesystem::path snapshot_dir;
  std::filesystem::path manifest_path;
  std::filesystem::path files_path;
  uint64_t file_count = 0;
  uint64_t object_count = 0;
  uint64_t total_bytes = 0;
  uint64_t source_head_height = 0;
  uint64_t source_lib_height = 0;
  RestoreSpaceEstimate restore_space;
  bool snapshot_complete = false;
  bool latest = false;
  bool public_bootstrap = false;
};

struct BackupSnapshotListResult
{
  std::filesystem::path repository_dir;
  std::string latest_backup_id;
  std::string remote_directory;
  std::string remote_space_target_path;
  std::string remote_space_message;
  uint64_t remote_available_bytes = 0;
  bool remote_space_check_ok = false;
  std::vector< BackupSnapshotSummary > snapshots;
};

struct BackupDeleteResult
{
  std::string source;
  std::string backup_id;
  std::filesystem::path repository_dir;
  std::string remote_directory;
  std::string transport;
  bool dry_run = true;
  bool snapshot_found = false;
  bool deleted_snapshot = false;
  bool deleted_latest = false;
  std::string previous_latest_backup_id;
  std::string new_latest_backup_id;
  uint64_t snapshot_metadata_file_count = 0;
  uint64_t snapshot_metadata_bytes = 0;
  uint64_t reclaimable_object_count = 0;
  uint64_t reclaimable_object_bytes = 0;
  uint64_t deleted_object_count = 0;
  uint64_t deleted_object_bytes = 0;
};

RestoreSpaceEstimate estimate_restore_space( uint64_t restored_database_bytes,
                                             uint64_t runtime_files_bytes,
                                             uint64_t object_download_bytes,
                                             uint64_t archive_bytes = 0,
                                             bool streaming_archive = false,
                                             uint64_t existing_target_bytes = 0 );

RestoreSpaceCheck check_restore_space( const RestoreSpaceEstimate& estimate,
                                       uint64_t available_bytes,
                                       std::string target_path );

RestorePreflightResult build_local_restore_preflight( const std::filesystem::path& repository_dir,
                                                      const std::filesystem::path& target_basedir );
RestorePreflightResult build_local_restore_preflight( const std::filesystem::path& repository_dir,
                                                      const std::filesystem::path& target_basedir,
                                                      const std::string& backup_id );
RestoreStageResult stage_local_restore_snapshot( const std::filesystem::path& repository_dir,
                                                 const std::filesystem::path& target_basedir,
                                                 const std::filesystem::path& requested_staging_dir = {} );
RestoreStageResult stage_local_restore_snapshot( const std::filesystem::path& repository_dir,
                                                 const std::filesystem::path& target_basedir,
                                                 const std::string& backup_id,
                                                 const std::filesystem::path& requested_staging_dir = {} );
RestoreActivationResult activate_staged_restore_snapshot( const std::filesystem::path& staging_dir,
                                                          const std::filesystem::path& target_basedir );
BackupSnapshotListResult list_local_backup_snapshots( const std::filesystem::path& repository_dir );
BackupDeleteResult delete_local_backup_snapshot( const std::filesystem::path& repository_dir,
                                                 const std::string& backup_id,
                                                 bool dry_run );

class LocalSnapshotRepository
{
public:
  explicit LocalSnapshotRepository( std::filesystem::path repository_dir );

  LocalSnapshotResult store_checkpoint_snapshot( const CheckpointResult& checkpoint,
                                                 const NodeConfig& cfg,
                                                 const std::filesystem::path& basedir,
                                                 const std::filesystem::path& config_path );

private:
  std::filesystem::path _repository_dir;
};

std::string local_snapshot_result_to_text( const LocalSnapshotResult& result );
std::string local_snapshot_result_to_json( const LocalSnapshotResult& result );
std::string backup_snapshot_list_result_to_text( const BackupSnapshotListResult& result );
std::string backup_snapshot_list_result_to_json( const BackupSnapshotListResult& result );
std::string backup_delete_result_to_text( const BackupDeleteResult& result );
std::string backup_delete_result_to_json( const BackupDeleteResult& result );
std::string restore_preflight_result_to_text( const RestorePreflightResult& result );
std::string restore_preflight_result_to_json( const RestorePreflightResult& result );
std::string restore_stage_result_to_text( const RestoreStageResult& result );
std::string restore_stage_result_to_json( const RestoreStageResult& result );
std::string restore_activation_result_to_text( const RestoreActivationResult& result );
std::string restore_activation_result_to_json( const RestoreActivationResult& result );

} // namespace koinos::node::backup
