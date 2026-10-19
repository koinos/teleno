#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>

#include "backup/snapshot_repository.hpp"
#include "backup/restore_plan.hpp"
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

// Opt-in SC2 object transport: one attempt, 64 KiB chunks, exact size enforced
// while receiving, no destination/object cache. Caller owns checked writes/hash.
// Network steps use asynchronous cancellation/deadlines; legacy routes unchanged.
class DirectTransferError : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
  DirectTransferError(const std::string& message,int code,bool partial):std::runtime_error(message),transport_code(code),partial_message(partial) {}
  int transport_code = 0;
  bool partial_message = false;
};
void stream_public_restore_object(
  const BackupPublicRestoreConfig& source, const SnapshotFileEntry& file,
  const std::function< void( const char*, size_t ) >& consume,
  const PublicRestoreOptions& options = {} );

BackupSnapshotListResult list_public_backup_snapshots(
  const std::filesystem::path& repository_dir,
  const BackupPublicRestoreConfig& public_restore,
  const std::string& backup_id = {},
  const PublicRestoreOptions& options = {} );

// Bounded temporary metadata acquisition only. Does not publish repository
// metadata, fetch objects, create a target, open a DB, or initialize an attempt.
RestoreSpecification acquire_public_restore_specification(
  const std::filesystem::path& metadata_scratch,
  const BackupPublicRestoreConfig& public_restore,
  const std::string& backup_id = {},
  const std::string& expected_chain_id = {},
  const PublicRestoreOptions& options = {},
  const RestoreMetadataLimits& limits = {} );

PublicRestoreFetchResult fetch_public_restore_snapshot(
  const std::filesystem::path& repository_dir,
  const std::filesystem::path& target_basedir,
  const BackupPublicRestoreConfig& public_restore,
  const std::string& backup_id = {},
  const PublicRestoreOptions& options = {} );

std::string public_restore_fetch_result_to_text( const PublicRestoreFetchResult& result );
std::string public_restore_fetch_result_to_json( const PublicRestoreFetchResult& result );

} // namespace koinos::node::backup
