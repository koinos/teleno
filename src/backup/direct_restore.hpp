#pragma once

#include "backup/public_restore.hpp"

namespace koinos::node::backup {

struct DirectStageOptions
{
  RestoreReserveProfile reserve_profile = RestoreReserveProfile::standard;
  PublicRestoreOptions transfer;
  // Diagnostic seams; production callers normally leave these unset.
  std::function< uint64_t( const std::filesystem::path& ) > available_bytes;
  std::function< void( const std::string&, uint64_t ) > checkpoint;
};

struct DirectStageResult
{
  std::string backup_id, attempt_id, state, error;
  std::filesystem::path attempt_dir, target;
  uint64_t downloaded_files = 0, reused_files = 0, transferred_bytes = 0;
  uint64_t payload_bytes = 0, allocated_attempt_bytes = 0, required_additional_bytes = 0;
  bool download_complete = false;
};

// POSIX staging only. Both paths must be disjoint siblings on one filesystem.
// An absent attempt is initialized exclusively; resume=true requires its bound
// descriptor and a freshly authenticated specification for the SAME selected ID.
// Never resolve latest for resume: read the bounded descriptor first to select ID.
// Only planned/downloading/paused/download_complete states accept source replay.
// A completed download is NOT a verified/opened DB and cannot be activated.
DirectStageResult stage_direct_restore_snapshot(
  const RestoreSpecification& specification, const BackupPublicRestoreConfig& source,
  const std::filesystem::path& target, const std::filesystem::path& attempt_dir,
  bool resume = false, const DirectStageOptions& options = {} );

// Read-only bounded identity lookup for standalone CLI resume/activation. The
// returned JSON is an untrusted selection hint; engine calls recheck it.
std::string read_direct_restore_attempt_identity( const std::filesystem::path& attempt_dir );
// Bounded, generation-checked phase hint; every mutating engine call rechecks
// the descriptor and phase under its locks. Never use a hint as authority.
std::string read_direct_restore_attempt_state( const std::filesystem::path& attempt_dir );
std::string direct_stage_result_to_json( const DirectStageResult& result );

// SC3 engine used by the standalone SC4 CLI, never by runtime direct admin.
// Failures preserve the journal and sole database; recovery never downloads
// payload files (the CLI separately reauthenticates pinned source metadata).
struct DirectActivationResult
{
  std::string attempt_id, state, validation_json;
  bool observer_hold = false;
};
DirectActivationResult validate_direct_restore_attempt(
  const RestoreSpecification&, const std::filesystem::path& target,
  const std::filesystem::path& attempt, const DirectStageOptions& = {} );
DirectActivationResult activate_direct_restore_attempt(
  const RestoreSpecification&, const std::filesystem::path& target,
  const std::filesystem::path& attempt, const DirectStageOptions& = {} );
// Complete the direct CLI workflow without releasing either ownership lock
// between validation and activation. Only a fresh proof created inside this
// call can avoid the duplicate pre-publication DB scan. All file hashes and
// the final post-rename read-only validation remain mandatory. Recovery never
// reuses a persisted record as that in-memory proof.
DirectActivationResult validate_and_activate_direct_restore_attempt(
  const RestoreSpecification&, const std::filesystem::path& target,
  const std::filesystem::path& attempt, const DirectStageOptions& = {} );

} // namespace koinos::node::backup
