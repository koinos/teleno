#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

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

struct BackupOperationStatus
{
  std::string operation_id;
  BackupOperationState state = BackupOperationState::idle;
  std::string message;
  std::filesystem::path checkpoint_dir;
  uint64_t started_at_ms = 0;
  uint64_t finished_at_ms = 0;
  bool cancel_requested = false;
  bool has_snapshot = false;
  LocalSnapshotResult snapshot;
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

class BackupService
{
public:
  BackupService( NodeConfig cfg,
                 std::filesystem::path basedir,
                 std::filesystem::path config_path,
                 storage::RocksDBManager& storage_db );
  ~BackupService();

  BackupOperationStatus status() const;
  LocalSnapshotResult create_local_snapshot();
  BackupOperationStatus start_local_snapshot_async();
  BackupOperationStatus cancel_current_operation();
  void wait_for_current_operation();
  RestoreStageResult stage_restore_snapshot( const std::filesystem::path& requested_staging_dir = {} );
  RestoreActivationRequest request_restore_activation( const std::filesystem::path& requested_staging_dir = {} );

private:
  void validate_local_snapshot_request() const;
  std::string next_operation_id() const;
  void begin_operation( const std::string& operation_id,
                        const std::filesystem::path& checkpoint_dir );
  LocalSnapshotResult execute_local_snapshot_operation( const std::string& operation_id,
                                                        const std::filesystem::path& checkpoint_dir,
                                                        bool rethrow_on_failure );
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
