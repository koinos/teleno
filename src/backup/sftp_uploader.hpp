#pragma once

#include <cstdint>
#include <filesystem>
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
  std::filesystem::path batch_file;
  bool batch_file_removed = false;
  uint64_t file_count = 0;
  uint64_t total_bytes = 0;
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
  RestorePreflightResult preflight;
  uint64_t metadata_file_count = 0;
  uint64_t object_file_count = 0;
  uint64_t object_bytes = 0;
  uint64_t repository_available_bytes = 0;
  uint64_t repository_required_bytes = 0;
  uint64_t batch_file_count = 0;
  bool metadata_fetched = false;
  bool objects_fetched = false;
  bool ready_to_stage = false;
  std::string download_skipped_reason;
};

SftpUploadPlan build_open_ssh_sftp_upload_plan( const std::filesystem::path& repository_dir,
                                                const std::string& remote_directory );
SftpRestoreObjectFetchPlan build_open_ssh_sftp_restore_object_fetch_plan(
  const std::filesystem::path& repository_dir,
  const std::string& remote_directory,
  const RestorePreflightResult& preflight );

SftpUploadResult upload_latest_snapshot_with_open_ssh_sftp( const std::filesystem::path& repository_dir,
                                                            const BackupSshConfig& ssh,
                                                            const BackupRemoteConfig& remote );
SftpRestoreFetchResult fetch_latest_restore_snapshot_with_open_ssh_sftp( const std::filesystem::path& repository_dir,
                                                                         const std::filesystem::path& target_basedir,
                                                                         const BackupSshConfig& ssh,
                                                                         const BackupRemoteConfig& remote );

std::string sftp_upload_plan_to_text( const SftpUploadPlan& plan );
std::string sftp_upload_result_to_text( const SftpUploadResult& result );
std::string sftp_upload_result_to_json( const SftpUploadResult& result );
std::string sftp_restore_fetch_result_to_text( const SftpRestoreFetchResult& result );
std::string sftp_restore_fetch_result_to_json( const SftpRestoreFetchResult& result );

} // namespace koinos::node::backup
