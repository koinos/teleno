#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "backup/snapshot_repository.hpp"
#include "core/config.hpp"

namespace koinos::node::backup {

struct SftpUploadPlan
{
  std::filesystem::path repository_dir;
  std::string backup_id;
  std::string remote_directory;
  std::vector< std::string > batch_commands;
  uint64_t file_count = 0;
  uint64_t total_bytes = 0;
};

struct SftpUploadResult
{
  std::string backup_id;
  std::filesystem::path repository_dir;
  std::string remote_directory;
  std::string transport;
  std::filesystem::path batch_file;
  bool batch_file_removed = false;
  uint64_t file_count = 0;
  uint64_t total_bytes = 0;
  uint64_t batch_file_count = 0;
  uint64_t retry_count = 0;
};

struct PublicBootstrapPublishResult
{
  std::string backup_id;
  std::filesystem::path repository_dir;
  std::string public_directory;
  std::string public_base_url;
  std::string network;
  std::string sanitized_config_sha256;
  uint64_t sanitized_config_size = 0;
  uint64_t file_count = 0;
  uint64_t object_count = 0;
  uint64_t total_bytes = 0;
  uint64_t removed_public_snapshot_count = 0;
  std::vector< std::string > removed_public_snapshot_ids;
};

struct SftpRestoreObjectDownload
{
  std::string sha256;
  std::string remote_relative_path;
  std::filesystem::path local_object_path;
  std::filesystem::path local_partial_path;
  uint64_t size_bytes = 0;
};

struct SftpRestoreObjectFetchPlan
{
  std::filesystem::path repository_dir;
  std::string backup_id;
  std::string remote_directory;
  std::vector< std::string > batch_commands;
  std::vector< SftpRestoreObjectDownload > downloads;
  uint64_t object_count = 0;
  uint64_t total_bytes = 0;
};

struct SftpRestoreFetchResult
{
  std::string backup_id;
  std::filesystem::path repository_dir;
  std::filesystem::path target_basedir;
  std::string remote_directory;
  std::string transport;
  RestorePreflightResult preflight;
  uint64_t metadata_file_count = 0;
  uint64_t object_file_count = 0;
  uint64_t object_bytes = 0;
  uint64_t repository_available_bytes = 0;
  uint64_t repository_required_bytes = 0;
  uint64_t batch_file_count = 0;
  uint64_t retry_count = 0;
  bool metadata_fetched = false;
  bool objects_fetched = false;
  bool ready_to_stage = false;
  std::string download_skipped_reason;
};

struct SftpTransferProgress
{
  std::string phase;
  std::string backup_id;
  uint64_t completed_batches = 0;
  uint64_t total_batches = 0;
  uint64_t attempt = 0;
  uint64_t file_count = 0;
  uint64_t total_bytes = 0;
};

struct SftpTransferOptions
{
  uint64_t max_attempts = 3;
  uint64_t retry_delay_seconds = 5;
  std::function< bool() > cancel_requested;
  std::function< void( const SftpTransferProgress& ) > progress;
};

SftpUploadPlan build_sftp_upload_plan( const std::filesystem::path& repository_dir,
                                       const std::string& remote_directory );
SftpRestoreObjectFetchPlan build_sftp_restore_object_fetch_plan(
  const std::filesystem::path& repository_dir,
  const std::string& remote_directory,
  const RestorePreflightResult& preflight );

SftpUploadResult upload_latest_snapshot_with_sftp( const std::filesystem::path& repository_dir,
                                                   const BackupSshConfig& ssh,
                                                   const BackupRemoteConfig& remote );
SftpUploadResult upload_latest_snapshot_with_managed_sftp( const std::filesystem::path& repository_dir,
                                                           const BackupSshConfig& ssh,
                                                           const BackupRemoteConfig& remote,
                                                           const SftpTransferOptions& options = {} );
PublicBootstrapPublishResult publish_latest_public_bootstrap_with_managed_sftp(
  const std::filesystem::path& repository_dir,
  const BackupSshConfig& ssh,
  const BackupPublicPublishConfig& public_publish,
  const SftpTransferOptions& options = {} );
BackupSnapshotListResult list_remote_backup_snapshots_with_sftp( const std::filesystem::path& repository_dir,
                                                                 const BackupSshConfig& ssh,
                                                                 const BackupRemoteConfig& remote );
BackupSnapshotListResult list_remote_backup_snapshots_with_managed_sftp(
  const std::filesystem::path& repository_dir,
  const BackupSshConfig& ssh,
  const BackupRemoteConfig& remote,
  const SftpTransferOptions& options = {} );
SftpRestoreFetchResult fetch_latest_restore_snapshot_with_sftp( const std::filesystem::path& repository_dir,
                                                                const std::filesystem::path& target_basedir,
                                                                const BackupSshConfig& ssh,
                                                                const BackupRemoteConfig& remote );
SftpRestoreFetchResult fetch_latest_restore_snapshot_with_managed_sftp( const std::filesystem::path& repository_dir,
                                                                        const std::filesystem::path& target_basedir,
                                                                        const BackupSshConfig& ssh,
                                                                        const BackupRemoteConfig& remote,
                                                                        const SftpTransferOptions& options = {} );
SftpRestoreFetchResult fetch_restore_snapshot_with_sftp( const std::filesystem::path& repository_dir,
                                                         const std::filesystem::path& target_basedir,
                                                         const BackupSshConfig& ssh,
                                                         const BackupRemoteConfig& remote,
                                                         const std::string& backup_id );
SftpRestoreFetchResult fetch_restore_snapshot_with_managed_sftp( const std::filesystem::path& repository_dir,
                                                                 const std::filesystem::path& target_basedir,
                                                                 const BackupSshConfig& ssh,
                                                                 const BackupRemoteConfig& remote,
                                                                 const std::string& backup_id,
                                                                 const SftpTransferOptions& options = {} );
BackupDeleteResult delete_remote_backup_snapshot_with_managed_sftp( const std::filesystem::path& repository_dir,
                                                                    const BackupSshConfig& ssh,
                                                                    const BackupRemoteConfig& remote,
                                                                    const std::string& backup_id,
                                                                    bool dry_run,
                                                                    const SftpTransferOptions& options = {} );

std::string sftp_upload_plan_to_text( const SftpUploadPlan& plan );
std::string sftp_upload_result_to_text( const SftpUploadResult& result );
std::string sftp_upload_result_to_json( const SftpUploadResult& result );
std::string public_bootstrap_publish_result_to_text( const PublicBootstrapPublishResult& result );
std::string public_bootstrap_publish_result_to_json( const PublicBootstrapPublishResult& result );
std::string sftp_restore_fetch_result_to_text( const SftpRestoreFetchResult& result );
std::string sftp_restore_fetch_result_to_json( const SftpRestoreFetchResult& result );

} // namespace koinos::node::backup
