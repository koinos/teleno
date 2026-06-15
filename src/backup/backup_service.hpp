#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "backup/sftp_uploader.hpp"
#include "backup/snapshot_repository.hpp"
#include "core/config.hpp"
#include "storage/rocksdb_manager.hpp"

namespace koinos::node::backup {

enum class BackupOperationState
{
  idle,
  running,
  succeeded,
  failed
};

struct RestoreActivationRequest
{
  std::filesystem::path target_basedir;
  std::filesystem::path staging_dir;
  std::filesystem::path intent_path;
  bool requires_node_stop = true;
  bool ready_to_activate = false;
  std::string message;
};

struct BackupOperationStatus
{
  std::string operation_id;
  std::string operation_kind;
  BackupOperationState state = BackupOperationState::idle;
  std::string phase;
  std::string message;
  std::filesystem::path checkpoint_dir;
  uint64_t started_at_ms = 0;
  uint64_t finished_at_ms = 0;
  bool cancel_requested = false;
  uint64_t completed_batches = 0;
  uint64_t total_batches = 0;
  uint64_t attempt = 0;
  uint64_t progress_file_count = 0;
  uint64_t progress_total_bytes = 0;
  bool has_snapshot = false;
  LocalSnapshotResult snapshot;
  bool has_remote_upload = false;
  SftpUploadResult remote_upload;
  bool has_restore_fetch = false;
  SftpRestoreFetchResult restore_fetch;
  bool has_restore_preflight = false;
  RestorePreflightResult restore_preflight;
  bool has_restore_stage = false;
  RestoreStageResult restore_stage;
  bool has_activation_request = false;
  RestoreActivationRequest activation_request;
  std::vector< BackupDeleteResult > delete_results;
};

class BackupService
{
public:
  BackupService( NodeConfig cfg,
                 std::filesystem::path basedir,
                 std::filesystem::path config_path,
                 storage::RocksDBManager& storage_db );
  ~BackupService();

  BackupOperationStatus status() const;
  std::string config_summary_json() const;
  BackupSnapshotListResult list_local_snapshots() const;
  BackupSnapshotListResult list_remote_snapshots();
  RestorePreflightResult restore_preflight( const std::string& backup_id = {} ) const;
  LocalSnapshotResult create_local_snapshot();
  BackupOperationStatus start_configured_backup_async( bool upload_remote );
  BackupOperationStatus start_local_snapshot_async();
  BackupOperationStatus start_upload_latest_async();
  BackupOperationStatus start_delete_async( const std::string& scope,
                                            const std::string& backup_id,
                                            const std::string& confirm );
  BackupOperationStatus start_restore_fetch_async( const std::string& backup_id );
  BackupOperationStatus cancel_current_operation();
  void wait_for_current_operation();
  RestoreStageResult stage_restore_snapshot( const std::string& backup_id = {},
                                             const std::filesystem::path& requested_staging_dir = {} );
  RestoreActivationRequest request_restore_activation( const std::string& backup_id = {},
                                                       const std::filesystem::path& requested_staging_dir = {} );

private:
  void validate_local_snapshot_request() const;
  void validate_local_repository_request( const std::string& operation_name ) const;
  void validate_remote_repository_request( const std::string& operation_name ) const;
  std::string next_operation_id() const;
  std::string next_operation_id( const std::string& kind ) const;
  void begin_operation( const std::string& operation_id,
                        const std::string& operation_kind,
                        const std::string& phase,
                        const std::string& message,
                        const std::filesystem::path& checkpoint_dir );
  void update_operation_progress( const std::string& phase,
                                  const std::string& message,
                                  const SftpTransferProgress* progress = nullptr );
  void finish_operation_success( const std::string& message );
  void finish_operation_failure( const std::string& message );
  LocalSnapshotResult create_local_snapshot_body( const std::filesystem::path& checkpoint_dir );
  LocalSnapshotResult execute_local_snapshot_operation( const std::string& operation_id,
                                                        const std::filesystem::path& checkpoint_dir,
                                                        bool rethrow_on_failure );
  void execute_configured_backup_operation( const std::string& operation_id,
                                            const std::filesystem::path& checkpoint_dir,
                                            bool upload_remote );
  void execute_upload_latest_operation();
  void execute_delete_operation( const std::string& scope,
                                 const std::string& backup_id,
                                 bool dry_run );
  void execute_restore_fetch_operation( const std::string& backup_id );
  SftpTransferOptions operation_sftp_transfer_options();
  void throw_if_cancel_requested() const;
  void join_finished_worker();

  NodeConfig _cfg;
  std::filesystem::path _basedir;
  std::filesystem::path _config_path;
  storage::RocksDBManager& _storage_db;

  mutable std::mutex _mutex;
  BackupOperationStatus _status;
  bool _running = false;
  bool _worker_active = false;
  std::thread _worker;
};

const char* backup_operation_state_name( BackupOperationState state );
std::string backup_operation_status_to_text( const BackupOperationStatus& status );
std::string backup_operation_status_to_json( const BackupOperationStatus& status );
std::string restore_activation_request_to_json( const RestoreActivationRequest& request );

} // namespace koinos::node::backup
