#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>

#include "backup/snapshot_repository.hpp"

namespace koinos::node::backup {

// Stable, sanitized diagnostics. Never copy transport URLs, paths or arbitrary
// exception text into the direct CLI report.
enum class RestoreSourceFailure { transport, partial_message, http_status, tls_trust, invalid_metadata,
  missing_chain_identity, conflicting_chain_identity, signature_verification };
const char* restore_source_failure_code( RestoreSourceFailure );
class RestoreSourceError : public std::runtime_error
{
public:
  explicit RestoreSourceError( RestoreSourceFailure failure, int transport_code = 0 )
    : std::runtime_error( restore_source_failure_code( failure ) ), failure( failure ), transport_code( transport_code ) {}
  const RestoreSourceFailure failure;
  const int transport_code;
};

enum class RestoreMode { repository, direct };
RestoreMode parse_restore_mode( const std::string& mode );
std::string restore_mode_name( RestoreMode mode );

struct RestoreMetadataLimits
{
  uint64_t file_bytes = 8ULL * 1024 * 1024;
  uint64_t entries = 100000;
  uint64_t object_bytes = 1ULL << 40;
  uint64_t total_bytes = 1ULL << 50;
};

// Authentication is supplied only by the acquisition layer after signature checks.
// Structural validation alone is not authentication or coherent-DB validation.
struct RestoreMetadataInput
{
  std::string backup_id, source_identity, manifest, inventory;
  std::string expected_network, expected_chain_id;
  bool signature_required = false;
  bool signature_verified = false;
  std::string authenticated_chain_id;
};

struct RestoreSpecificationData
{
  std::string backup_id, source_identity, manifest_sha256, inventory_sha256;
  std::string network, chain_id, storage_layout;
  std::string source_node_version;
  uint64_t format_version = 1;
  bool signature_required = false, signature_verified = false, expected_chain_checked = false;
  uint64_t database_bytes = 0, runtime_bytes = 0, unique_object_bytes = 0;
  std::vector< SnapshotFileEntry > files;
};

class RestoreSpecification
{
public:
  const RestoreSpecificationData& data() const { return *_data; }
private:
  explicit RestoreSpecification( RestoreSpecificationData data )
    : _data( std::make_shared< const RestoreSpecificationData >( std::move( data ) ) ) {}
  std::shared_ptr< const RestoreSpecificationData > _data;
  friend RestoreSpecification validate_restore_metadata( const RestoreMetadataInput&, const RestoreMetadataLimits& );
};

RestoreSpecification validate_restore_metadata( const RestoreMetadataInput& input,
                                                 const RestoreMetadataLimits& limits = {} );

struct RestoreBudgetPolicy
{
  RestoreReserveProfile reserve_profile = RestoreReserveProfile::standard;
  uint64_t allocation_quantum = 64 * 1024;
  uint64_t metadata_journal_bytes = 128ULL * 1024 * 1024;
  // A planning allowance, not a proven upper bound on offline recovery writes.
  uint64_t validation_allowance_bytes = 128ULL * 1024 * 1024;
  bool repository_on_target_filesystem = true;
};

struct RestoreModeBudget
{
  RestoreReserveProfile reserve_profile = RestoreReserveProfile::standard;
  uint64_t allocation_quantum = 0;
  uint64_t logical_payload_bytes = 0, allocated_payload_bytes = 0;
  uint64_t metadata_journal_bytes = 0, transfer_extra_bytes = 0;
  uint64_t validation_allowance_bytes = 0, repository_cache_bytes = 0;
  uint64_t repository_other_filesystem_bytes = 0, reserve_bytes = 0;
  uint64_t required_free_bytes = 0, recommended_free_bytes = 0;
};

RestoreModeBudget estimate_mode_restore_space( const RestoreSpecification& spec,
                                               RestoreMode mode,
                                               const RestoreBudgetPolicy& policy = {} );
RestoreSpaceCheck check_mode_restore_space( const RestoreModeBudget& budget,
                                            uint64_t available_bytes, std::string target_path );

struct DirectRestorePreflight
{
  RestoreSpecification specification;
  std::filesystem::path target, metadata_scratch;
  RestoreModeBudget budget;
  uint64_t capacity_bytes = 0, available_bytes = 0, existing_filesystem_usage_bytes = 0;
  uint64_t planned_total_with_reserve_bytes = 0;
  bool target_eligible = false, source_on_target_filesystem = false;
  bool metadata_on_target_filesystem = false, passes_minimum = false, below_recommended = false;
  std::string rejection_reason;
  bool ok() const { return target_eligible && passes_minimum; }
};

// Reject aliasing/symlinked paths before acquisition writes scratch metadata.
void validate_direct_planning_paths( const std::filesystem::path& target,
                                     const std::filesystem::path& scratch,
                                     const std::string& source_url );
DirectRestorePreflight build_direct_restore_preflight( RestoreSpecification spec,
                                                        const std::filesystem::path& target,
                                                        const std::filesystem::path& scratch,
                                                        RestoreReserveProfile reserve_profile = RestoreReserveProfile::standard );
std::string direct_restore_preflight_to_json( const DirectRestorePreflight& result );
std::string direct_restore_preflight_to_text( const DirectRestorePreflight& result );

} // namespace koinos::node::backup
