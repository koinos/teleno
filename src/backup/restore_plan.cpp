#include "backup/restore_plan.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <sys/statvfs.h>
#endif

namespace koinos::node::backup {
namespace {
namespace fs = std::filesystem;
using json = nlohmann::json;
void require( bool ok, const std::string& error ) { if( !ok ) throw std::runtime_error( error ); }
uint64_t add( uint64_t a, uint64_t b )
{
  require( b <= std::numeric_limits< uint64_t >::max() - a, "restore space arithmetic overflow" );
  return a + b;
}
uint64_t number( const json& object, const char* key )
{
  const auto& value = object.at( key );
  require( value.is_number_unsigned(), std::string( "invalid unsigned metadata field: " ) + key );
  return value.get< uint64_t >();
}
std::string digest( const std::string& bytes )
{
  unsigned char output[ EVP_MAX_MD_SIZE ]; unsigned length = 0;
  require( EVP_Digest( bytes.data(), bytes.size(), output, &length, EVP_sha256(), nullptr ) == 1,
           "restore metadata digest failed" );
  std::ostringstream out; out << std::hex << std::setfill( '0' );
  for( unsigned i = 0; i < length; ++i ) out << std::setw( 2 ) << unsigned( output[ i ] );
  return out.str();
}
void identifier( const std::string& value )
{
  require( !value.empty() && value.size() <= 255 && value != "." && value != ".."
    && value.find_first_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_ ." ) == std::string::npos
    && value.find( ' ' ) == std::string::npos, "invalid restore backup identity" );
}
std::string fold_path( const std::string& path )
{
  require( !path.empty() && path.size() <= 1024 && path.front() != '/' && path.back() != '/',
           "invalid restore destination path" );
  std::string folded;
  for( unsigned char ch: path )
  {
    require( ch >= 32 && ch < 127 && ch != '\\' && ch != ':' && ch != '?' && ch != '*' && ch != '"'
      && ch != '<' && ch != '>' && ch != '|', "non-portable restore destination path" );
    folded += ch >= 'A' && ch <= 'Z' ? char( ch + 32 ) : char( ch );
  }
  std::istringstream parts( path ); std::string part;
  while( std::getline( parts, part, '/' ) )
    require( !part.empty() && part != "." && part != ".." && part.back() != '.' && part.back() != ' ',
             "unsafe restore destination path" );
  return folded;
}
fs::path checked_path( const fs::path& requested )
{
  require( !requested.empty(), "restore planning path is required" );
  for( const auto& part: requested ) require( part != "..", "parent traversal in restore planning path" );
  auto path = fs::absolute( requested ).lexically_normal();
  fs::path current;
  for( const auto& part: path )
  {
    current /= part;
    std::error_code ec;
    const auto status = fs::symlink_status( current, ec );
    require( !ec || ec == std::errc::no_such_file_or_directory, "cannot inspect restore planning path" );
    require( !fs::is_symlink( status ), "symlinked restore planning path is not allowed" );
  }
  return path;
}
bool contains( const fs::path& parent, const fs::path& child )
{
  const auto relative = child.lexically_relative( parent );
  return relative.empty() || ( !relative.is_absolute() && *relative.begin() != ".." );
}
fs::path existing_parent( fs::path path )
{
  while( !fs::exists( path ) ) path = path.parent_path();
  require( fs::is_directory( path ), "restore planning ancestor is not a directory" );
  return path;
}
bool same_filesystem( const fs::path& a, const fs::path& b )
{
#if defined(__unix__) || defined(__APPLE__)
  struct stat left{}, right{};
  require( ::stat( existing_parent( a ).c_str(), &left ) == 0
    && ::stat( existing_parent( b ).c_str(), &right ) == 0, "filesystem identity inspection failed" );
  return left.st_dev == right.st_dev;
#else
  throw std::runtime_error( "direct restore filesystem identity is not supported on this platform" );
#endif
}
std::string eligibility( const RestoreSpecification& spec, const fs::path& target )
{
  if( !fs::exists( target ) ) return {};
  if( !fs::is_directory( target ) ) return "target is not a directory";
  // Only explicitly reviewed runtime inputs may already exist. Do not traverse DBs.
  for( const auto& entry: fs::recursive_directory_iterator( target ) )
  {
    const auto relative = entry.path().lexically_relative( target ).generic_string();
    if( entry.is_symlink() ) return "target contains a symlink";
    if( entry.is_directory() && ( relative == "chain" || relative == "jsonrpc" || relative == "jsonrpc/descriptors" ) ) continue;
    if( !entry.is_regular_file() ) return "target contains existing managed data or a special file";
    if( relative == "config.yml" ) continue; // Never overwrite the active config.
    if( relative != "chain/genesis_data.json" && relative != "jsonrpc/descriptors/koinos_descriptors.pb" )
      return "target contains existing managed data or an unowned restore attempt";
    auto file = std::find_if( spec.data().files.begin(), spec.data().files.end(),
      [&]( const auto& item ) { return item.path == relative; } );
    if( file == spec.data().files.end() || fs::file_size( entry.path() ) != file->size_bytes )
      return "existing runtime input does not match the selected snapshot";
    // Runtime inputs are bounded by the same object policy, but hash them streaming.
    std::ifstream in( entry.path(), std::ios::binary );
    std::unique_ptr< EVP_MD_CTX, decltype( &EVP_MD_CTX_free ) > ctx( EVP_MD_CTX_new(), EVP_MD_CTX_free );
    require( in && ctx && EVP_DigestInit_ex( ctx.get(), EVP_sha256(), nullptr ) == 1, "runtime input hash initialization failed" );
    char buffer[ 65536 ];
    while( in ) { in.read( buffer, sizeof( buffer ) ); require( EVP_DigestUpdate( ctx.get(), buffer, size_t( in.gcount() ) ) == 1, "runtime input hash failed" ); }
    require( !in.bad(), "runtime input read failed" );
    unsigned char output[ EVP_MAX_MD_SIZE ]; unsigned length = 0;
    require( EVP_DigestFinal_ex( ctx.get(), output, &length ) == 1, "runtime input hash failed" );
    std::ostringstream hash; hash << std::hex << std::setfill( '0' );
    for( unsigned i = 0; i < length; ++i ) hash << std::setw( 2 ) << unsigned( output[ i ] );
    if( hash.str() != file->sha256 ) return "existing runtime input does not match the selected snapshot";
  }
  return {};
}
} // namespace

RestoreMode parse_restore_mode( const std::string& mode )
{
  if( mode == "repository" ) return RestoreMode::repository;
  if( mode == "direct" ) return RestoreMode::direct;
  throw std::invalid_argument( "--backup-restore-mode must be repository or direct" );
}
std::string restore_mode_name( RestoreMode mode )
{ return mode == RestoreMode::direct ? "direct" : "repository"; }

const char* restore_source_failure_code( RestoreSourceFailure failure )
{
  switch( failure )
  {
    case RestoreSourceFailure::transport: return "source_transport_failure";
    case RestoreSourceFailure::partial_message: return "source_partial_message";
    case RestoreSourceFailure::http_status: return "source_http_status";
    case RestoreSourceFailure::tls_trust: return "source_tls_trust_unavailable";
    case RestoreSourceFailure::missing_chain_identity: return "source_chain_identity_missing";
    case RestoreSourceFailure::conflicting_chain_identity: return "source_chain_identity_conflict";
    case RestoreSourceFailure::signature_verification: return "source_signature_verification_failed";
    default: return "source_metadata_invalid";
  }
}

RestoreSpecification validate_restore_metadata( const RestoreMetadataInput& input, const RestoreMetadataLimits& limits )
{
  require( input.manifest.size() <= limits.file_bytes && input.inventory.size() <= limits.file_bytes,
           "restore metadata exceeds byte limit" );
  identifier( input.backup_id );
  const auto bounded_parse = []( const std::string& bytes ) {
    return json::parse( bytes, []( int depth, json::parse_event_t, json& ) {
      require( depth <= 32, "restore metadata nesting exceeds limit" ); return true;
    } );
  };
  const auto manifest = bounded_parse( input.manifest ); const auto inventory = bounded_parse( input.inventory );
  require( manifest.at( "format" ) == "teleno-native-rocksdb-snapshot" && number( manifest, "version" ) == 1
    && inventory.at( "format" ) == "teleno-native-snapshot-files" && number( inventory, "version" ) == 1,
    "unsupported restore metadata format/version" );
  require( manifest.at( "backup_id" ) == input.backup_id && inventory.at( "backup_id" ) == input.backup_id,
           "restore metadata backup identity mismatch" );
  RestoreSpecificationData data;
  data.backup_id = input.backup_id; data.source_identity = input.source_identity;
  data.manifest_sha256 = digest( input.manifest ); data.inventory_sha256 = digest( input.inventory );
  data.storage_layout = manifest.at( "source" ).at( "storage_layout" ).get< std::string >();
  data.source_node_version = manifest.value( "node", json::object() ).value( "version", std::string{} );
  require( data.storage_layout == "unified", "direct planning requires a unified snapshot layout" );
  const auto& bootstrap = manifest.at( "public_bootstrap" );
  data.network = bootstrap.at( "network" ).get< std::string >(); data.chain_id = bootstrap.at( "chain_id" ).get< std::string >();
  require( !data.network.empty(), "restore network identity is required" );
  if( data.chain_id.empty() ) throw RestoreSourceError( RestoreSourceFailure::missing_chain_identity );
  const auto& source = manifest.at( "source" );
  if( source.contains( "chain_id" ) && source.at( "chain_id" ) != data.chain_id )
    throw RestoreSourceError( RestoreSourceFailure::conflicting_chain_identity );
  require( !source.contains( "network" ) || source.at( "network" ) == data.network, "contradictory source network" );
  if( bootstrap.contains( "source" ) )
  {
    const auto& published_source = bootstrap.at( "source" );
    require( !published_source.contains( "chain_id" ) || published_source.at( "chain_id" ) == data.chain_id,
             "contradictory public source chain identity" );
    require( !published_source.contains( "storage_layout" ) || published_source.at( "storage_layout" ) == data.storage_layout,
             "contradictory public source storage layout" );
  }
  require( input.expected_network.empty() || input.expected_network == data.network, "restore network mismatch" );
  if( !input.expected_chain_id.empty() && input.expected_chain_id != data.chain_id )
    throw RestoreSourceError( RestoreSourceFailure::conflicting_chain_identity );
  data.expected_chain_checked = !input.expected_chain_id.empty();
  data.signature_required = input.signature_required; data.signature_verified = input.signature_verified;
  require( !input.signature_required || input.signature_verified, "required restore signature was not verified" );
  require( !input.signature_verified || input.authenticated_chain_id == data.chain_id, "authenticated restore chain identity mismatch" );
  const auto& entries = inventory.at( "files" );
  require( entries.is_array() && !entries.empty() && entries.size() <= limits.entries, "restore inventory entry limit" );
  std::set< std::string > paths; std::map< std::string, uint64_t > objects;
  std::map< std::string, std::string > directories;
  bool current = false, genesis = false;
  for( const auto& entry: entries )
  {
    SnapshotFileEntry file;
    file.path = entry.at( "path" ).get< std::string >(); const auto folded = fold_path( file.path );
    require( paths.insert( folded ).second, "duplicate or case-colliding restore destination" );
    for( auto slash = folded.find( '/' ); slash != std::string::npos; slash = folded.find( '/', slash + 1 ) )
    {
      auto [dir, inserted] = directories.emplace( folded.substr( 0, slash ), file.path.substr( 0, slash ) );
      require( inserted || dir->second == file.path.substr( 0, slash ), "case-colliding restore directory" );
    }
    file.sha256 = entry.at( "sha256" ).get< std::string >();
    require( file.sha256.size() == 64 && file.sha256.find_first_not_of( "0123456789abcdef" ) == std::string::npos, "invalid restore SHA-256" );
    file.size_bytes = number( entry, "size_bytes" ); require( file.size_bytes <= limits.object_bytes, "restore object exceeds size limit" );
    require( entry.at( "runtime_file" ).is_boolean(), "invalid restore runtime classification" );
    file.runtime_file = entry.at( "runtime_file" ).get< bool >();
    const bool runtime = file.path == "config.yml" || file.path == "chain/genesis_data.json"
      || file.path == "jsonrpc/descriptors/koinos_descriptors.pb";
    require( ( runtime && file.runtime_file ) || ( file.path.rfind( "db/", 0 ) == 0 && !file.runtime_file ),
             "unsupported restore inventory destination/classification" );
    current |= file.path == "db/CURRENT"; genesis |= file.path == "chain/genesis_data.json";
    auto [object, inserted] = objects.emplace( file.sha256, file.size_bytes );
    require( inserted || object->second == file.size_bytes, "contradictory repeated restore object size" );
    if( inserted ) data.unique_object_bytes = add( data.unique_object_bytes, file.size_bytes );
    if( runtime ) data.runtime_bytes = add( data.runtime_bytes, file.size_bytes );
    else data.database_bytes = add( data.database_bytes, file.size_bytes );
    data.files.push_back( std::move( file ) );
  }
  // Reject file/directory collisions, including case-only ancestor conflicts.
  for( const auto& path: paths )
    for( auto slash = path.find( '/' ); slash != std::string::npos; slash = path.find( '/', slash + 1 ) )
      require( !paths.count( path.substr( 0, slash ) ), "restore file/directory path collision" );
  require( current && genesis, "restore inventory lacks CURRENT or genesis input" );
  const auto total = add( data.database_bytes, data.runtime_bytes );
  require( total <= limits.total_bytes, "restore snapshot exceeds size limit" );
  const auto& snapshot = manifest.at( "snapshot" ); const auto& sizes = manifest.at( "sizes" );
  require( number( snapshot, "file_count" ) == data.files.size() && number( snapshot, "object_count" ) == objects.size()
    && number( snapshot, "total_bytes" ) == total && number( sizes, "restored_database_bytes" ) == data.database_bytes
    && number( sizes, "runtime_files_bytes" ) == data.runtime_bytes
    // Native local snapshots declare unique bytes; the native public publisher
    // declares per-path bytes. Budget from the inventory, not this ambiguous field.
    && ( number( sizes, "object_download_bytes" ) == data.unique_object_bytes || number( sizes, "object_download_bytes" ) == total )
    && number( sizes, "archive_bytes" ) == 0, "contradictory restore inventory totals" );
  return RestoreSpecification( std::move( data ) );
}

RestoreModeBudget estimate_mode_restore_space( const RestoreSpecification& spec, RestoreMode mode, const RestoreBudgetPolicy& policy )
{
  require( policy.allocation_quantum > 0, "invalid restore allocation quantum" );
  require( mode == RestoreMode::repository || mode == RestoreMode::direct, "invalid restore mode" );
  RestoreModeBudget b; std::map< std::string, uint64_t > objects; std::set< std::string > directories;
  b.allocation_quantum = policy.allocation_quantum;
  for( const auto& file: spec.data().files )
  {
    auto rounded = add( file.size_bytes, policy.allocation_quantum - 1 ) / policy.allocation_quantum;
    require( rounded <= std::numeric_limits< uint64_t >::max() / policy.allocation_quantum, "restore allocation overflow" );
    rounded *= policy.allocation_quantum;
    b.logical_payload_bytes = add( b.logical_payload_bytes, file.size_bytes );
    b.allocated_payload_bytes = add( b.allocated_payload_bytes, rounded );
    objects.emplace( file.sha256, rounded );
    for( auto parent = fs::path( file.path ).parent_path(); !parent.empty(); parent = parent.parent_path() )
      directories.insert( parent.generic_string() );
  }
  if( mode == RestoreMode::repository ) for( const auto& [_, size]: objects )
  {
    if( policy.repository_on_target_filesystem ) b.repository_cache_bytes = add( b.repository_cache_bytes, size );
    else b.repository_other_filesystem_bytes = add( b.repository_other_filesystem_bytes, size );
  }
  require( directories.size() <= std::numeric_limits< uint64_t >::max() / policy.allocation_quantum,
           "restore directory allocation overflow" );
  const auto directory_bytes = directories.size() * policy.allocation_quantum;
  require( spec.data().files.size() <= std::numeric_limits< uint64_t >::max() / policy.allocation_quantum,
           "restore journal allocation overflow" );
  // SC2 persists a small record per destination. Budget allocated files, not
  // just their logical JSON length (important for many small inventory entries).
  const auto metadata_records = add( 16ULL * 1024 * 1024, spec.data().files.size() * policy.allocation_quantum );
  b.metadata_journal_bytes = std::max( policy.metadata_journal_bytes, add( directory_bytes, metadata_records ) );
  b.validation_allowance_bytes = policy.validation_allowance_bytes;
  b.reserve_profile = policy.reserve_profile;
  b.reserve_bytes = restore_recovery_reserve_bytes( spec.data().database_bytes, policy.reserve_profile );
  b.required_free_bytes = add( add( add( b.allocated_payload_bytes, b.metadata_journal_bytes ),
    b.validation_allowance_bytes ), b.repository_cache_bytes );
  b.recommended_free_bytes = add( b.required_free_bytes, b.reserve_bytes );
  return b;
}

RestoreSpaceCheck check_mode_restore_space( const RestoreModeBudget& budget, uint64_t available, std::string target )
{
  RestoreSpaceEstimate estimate;
  estimate.minimum_target_free_bytes = budget.required_free_bytes;
  estimate.recommended_target_free_bytes = budget.recommended_free_bytes;
  return check_restore_space( estimate, available, std::move( target ) );
}

void validate_direct_planning_paths( const fs::path& target, const fs::path& scratch, const std::string& source )
{
  const auto t = checked_path( target ), s = checked_path( scratch );
  require( !contains( t, s ) && !contains( s, t ), "metadata scratch and target must not overlap" );
  require( source.find_first_of( "@?#\\" ) == std::string::npos, "direct source credentials/query/fragment are unsupported" );
  if( source.rfind( "file://", 0 ) == 0 )
  {
    const auto local = checked_path( source.substr( 7 ) );
    require( !contains( local, t ) && !contains( t, local ) && !contains( local, s ) && !contains( s, local ),
             "local source, target and metadata scratch must not overlap" );
  }
}

DirectRestorePreflight build_direct_restore_preflight( RestoreSpecification spec, const fs::path& target, const fs::path& scratch,
  RestoreReserveProfile reserve_profile )
{
  validate_direct_planning_paths( target, scratch, spec.data().source_identity );
  DirectRestorePreflight r{ std::move( spec ), checked_path( target ), checked_path( scratch ), {} };
  r.rejection_reason = eligibility( r.specification, r.target ); r.target_eligible = r.rejection_reason.empty();
  const auto space = fs::space( existing_parent( r.target ) );
  r.capacity_bytes = space.capacity; r.available_bytes = space.available;
  require( space.capacity >= space.available, "inconsistent filesystem space observation" );
  r.existing_filesystem_usage_bytes = space.capacity - space.available;
  r.metadata_on_target_filesystem = same_filesystem( r.target, r.metadata_scratch );
  if( r.specification.data().source_identity.rfind( "file://", 0 ) == 0 )
    r.source_on_target_filesystem = same_filesystem( r.target, r.specification.data().source_identity.substr( 7 ) );
  RestoreBudgetPolicy policy;
  policy.reserve_profile = reserve_profile;
#if defined(__unix__) || defined(__APPLE__)
  struct statvfs filesystem{};
  require( ::statvfs( existing_parent( r.target ).c_str(), &filesystem ) == 0, "filesystem allocation unit inspection failed" );
  policy.allocation_quantum = std::max< uint64_t >( policy.allocation_quantum, filesystem.f_frsize );
#endif
  r.budget = estimate_mode_restore_space( r.specification, RestoreMode::direct, policy );
  const auto check = check_mode_restore_space( r.budget, r.available_bytes, r.target.string() );
  r.passes_minimum = check.passes_minimum; r.below_recommended = check.below_recommended;
  r.planned_total_with_reserve_bytes = add( r.existing_filesystem_usage_bytes, r.budget.recommended_free_bytes );
  if( r.rejection_reason.empty() && !r.passes_minimum ) r.rejection_reason = "insufficient available bytes for declared restore planning budget";
  return r;
}

std::string direct_restore_preflight_to_json( const DirectRestorePreflight& r )
{
  const auto& d = r.specification.data(); const auto& b = r.budget;
  json value = { { "format", "teleno-direct-restore-preflight" }, { "version", 1 }, { "mode", "direct" },
    { "recovery_reserve_profile", restore_reserve_profile_name( b.reserve_profile ) },
    { "experimental_reserve", b.reserve_profile != RestoreReserveProfile::standard },
    { "preflight_ok", r.ok() }, { "backup_id", d.backup_id }, { "source_identity", d.source_identity },
    { "network", d.network }, { "chain_id", d.chain_id }, { "storage_layout", d.storage_layout },
    { "input_format_version", d.format_version }, { "source_node_version", d.source_node_version },
    { "manifest_sha256", d.manifest_sha256 }, { "inventory_sha256", d.inventory_sha256 },
    { "signature_required", d.signature_required }, { "signature_verified", d.signature_verified },
    { "expected_chain_checked", d.expected_chain_checked }, { "file_count", d.files.size() },
    { "target", r.target.string() }, { "target_eligible", r.target_eligible }, { "rejection_reason", r.rejection_reason },
    { "metadata_scratch", r.metadata_scratch.string() }, { "metadata_scratch_writes", true },
    { "maximum_metadata_file_bytes", 8ULL * 1024 * 1024 }, { "maximum_metadata_payload_bytes", 48ULL * 1024 * 1024 },
    { "metadata_on_target_filesystem", r.metadata_on_target_filesystem }, { "source_on_target_filesystem", r.source_on_target_filesystem },
    { "capacity_bytes", r.capacity_bytes }, { "available_bytes", r.available_bytes },
    { "existing_filesystem_usage_bytes", r.existing_filesystem_usage_bytes },
    { "planned_total_with_reserve_bytes", r.planned_total_with_reserve_bytes },
    { "passes_minimum", r.passes_minimum }, { "below_recommended", r.below_recommended },
    { "space", { { "logical_payload_bytes", b.logical_payload_bytes }, { "allocated_payload_bytes", b.allocated_payload_bytes },
      { "allocation_quantum_bytes", b.allocation_quantum }, { "metadata_journal_bytes", b.metadata_journal_bytes },
      { "transfer_extra_bytes", b.transfer_extra_bytes }, { "validation_allowance_bytes", b.validation_allowance_bytes },
      { "repository_cache_bytes", b.repository_cache_bytes }, { "reserve_bytes", b.reserve_bytes },
      { "required_free_bytes", b.required_free_bytes }, { "recommended_free_bytes", b.recommended_free_bytes } } },
    { "payload_downloaded", false }, { "target_database_opened", false }, { "attempt_initialized", false },
    { "ready_to_restore", false }, { "start_as_observer_first", true }, { "capacity_40gb_qualified", false },
    { "unverified", { "offline database/genesis validation", "validation allowance upper bound",
      "concurrent startup/restore exclusion", "filesystem allocation/compaction peaks", "40 GB operation" } } };
  return value.dump( 2 ) + "\n";
}
std::string direct_restore_preflight_to_text( const DirectRestorePreflight& r )
{
  // Identical field names and values in a labeled human-readable JSON document.
  return "Direct restore metadata-only planning (not ready to restore)\n" + direct_restore_preflight_to_json( r );
}
} // namespace koinos::node::backup
