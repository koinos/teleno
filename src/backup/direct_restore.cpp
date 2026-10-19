#include "backup/direct_restore.hpp"
#include "core/storage_lease.hpp"
#include "backup/offline_restore_validation.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <iomanip>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <thread>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <stdio.h>
#elif defined(__linux__)
#include <sys/syscall.h>
#endif
#endif

namespace koinos::node::backup {
namespace {
namespace fs = std::filesystem;
using json = nlohmann::json;
constexpr uint64_t metadata_limit = 64 * 1024;
void require( bool ok, const char* message ) { if( !ok ) throw std::runtime_error( message ); }
uint64_t sum( uint64_t a, uint64_t b )
{ require( b <= UINT64_MAX - a, "direct restore size overflow" ); return a + b; }
uint64_t rounded( uint64_t value, uint64_t quantum ) { return sum( value, quantum - 1 ) / quantum * quantum; }
void checkpoint( const DirectStageOptions& options, const std::string& name, uint64_t file = 0 )
{ if( options.checkpoint ) options.checkpoint( name, file ); }
void cancelled( const DirectStageOptions& options )
{ if( options.transfer.cancel_requested && options.transfer.cancel_requested() ) throw std::runtime_error( "direct transfer cancelled" ); }

#if defined(__unix__) || defined(__APPLE__)
thread_local const DirectStageOptions* sync_options = nullptr;
struct SyncCheckpoints
{
  const DirectStageOptions* previous;
  explicit SyncCheckpoints(const DirectStageOptions& options): previous(sync_options) { sync_options = &options; }
  ~SyncCheckpoints() { sync_options = previous; }
};
struct FD
{
  int value;
  explicit FD( int fd ): value( fd )
  { require( value >= 0, errno == ENOSPC ? "direct restore disk full on open" : "direct restore open failed" ); }
  ~FD() { if( value >= 0 ) ::close( value ); }
  FD( const FD& ) = delete; FD& operator=( const FD& ) = delete;
  void close_checked() { const auto fd = value; value = -1; require( ::close( fd ) == 0, "direct restore close failed" ); }
};
void regular( const fs::path& path )
{
  struct stat status{};
  require( ::lstat( path.c_str(), &status ) == 0 && S_ISREG( status.st_mode ) && status.st_nlink == 1,
           "direct attempt file is not an unshared regular file" );
}
void directory( const fs::path& path )
{
  struct stat status{};
  require( ::lstat( path.c_str(), &status ) == 0 && S_ISDIR( status.st_mode ), "direct attempt directory is invalid" );
}
void sync_directory( const fs::path& path )
{
  FD dir( ::open( path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC ) );
  if(sync_options) checkpoint(*sync_options,"before_directory_sync");
  require( ::fsync( dir.value ) == 0, "direct restore directory sync failed" );
  if(sync_options) checkpoint(*sync_options,"after_directory_sync");
  dir.close_checked();
}
void write_all( FD& file, const char* bytes, size_t count )
{
  while( count )
  {
    const auto n = ::write( file.value, bytes, count );
    if( n < 0 && errno == EINTR ) continue;
    require( n > 0, errno == ENOSPC ? "direct restore disk full" : "direct restore write failed" );
    bytes += n; count -= size_t( n );
  }
}
void make_directory( const fs::path& path )
{
  if( !fs::exists( path ) )
  { require( ::mkdir( path.c_str(), 0700 ) == 0, "direct restore exclusive mkdir failed" ); sync_directory( path.parent_path() ); }
  directory( path );
}
void owned_remove( const fs::path& path )
{
  if( fs::exists( fs::symlink_status( path ) ) )
  { regular( path ); require( ::unlink( path.c_str() ) == 0, "direct restore partial cleanup failed" ); sync_directory( path.parent_path() ); }
}
void persist( const fs::path& path, const json& value, const DirectStageOptions& options, uint64_t file = 0 )
{
  const auto bytes = value.dump() + "\n";
  require( bytes.size() <= metadata_limit, "direct journal record exceeds limit" );
  auto tmp = path; tmp += ".tmp";
  owned_remove( tmp );
  if( fs::exists( path ) ) regular( path );
  FD out( ::open( tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600 ) );
  checkpoint( options, "journal_write", file ); write_all( out, bytes.data(), bytes.size() );
  checkpoint( options, "after_journal_write", file );
  checkpoint( options, "journal_sync", file ); require( ::fsync( out.value ) == 0, "direct journal sync failed" );
  checkpoint( options, "after_journal_sync", file ); out.close_checked();
  checkpoint( options, "after_journal_close", file );
  checkpoint( options, "journal_rename", file ); require( ::rename( tmp.c_str(), path.c_str() ) == 0, "direct journal rename failed" );
  checkpoint( options, "after_journal_rename", file );
  sync_directory( path.parent_path() );
}
std::string bounded_read( const fs::path& path )
{
  regular( path ); require( fs::file_size( path ) <= metadata_limit, "direct journal record exceeds limit" );
  FD file( ::open( path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC ) );
  std::string value; std::array< char, 4096 > buffer;
  for( ;; )
  {
    const auto count = ::read( file.value, buffer.data(), buffer.size() );
    if( count < 0 && errno == EINTR ) continue;
    require( count >= 0, "direct journal read failed" ); if( !count ) break;
    require( size_t( count ) <= metadata_limit - value.size(), "direct journal record grew beyond limit" );
    value.append( buffer.data(), size_t( count ) );
  }
  file.close_checked(); return value;
}
json read_record( const fs::path& path )
{
  return json::parse( bounded_read( path ), []( int depth, json::parse_event_t, json& ) {
    require( depth <= 16, "direct journal depth limit" ); return true;
  } );
}
class Digest
{
  std::unique_ptr< EVP_MD_CTX, decltype( &EVP_MD_CTX_free ) > ctx{ EVP_MD_CTX_new(), EVP_MD_CTX_free };
public:
  Digest() { require( ctx && EVP_DigestInit_ex( ctx.get(), EVP_sha256(), nullptr ) == 1, "direct SHA init failed" ); }
  void update( const char* bytes, size_t count ) { require( EVP_DigestUpdate( ctx.get(), bytes, count ) == 1, "direct SHA update failed" ); }
  std::string finish()
  {
    unsigned char bytes[ EVP_MAX_MD_SIZE ]; unsigned length = 0;
    require( EVP_DigestFinal_ex( ctx.get(), bytes, &length ) == 1, "direct SHA finish failed" );
    std::ostringstream out; out << std::hex << std::setfill( '0' );
    for( unsigned i = 0; i < length; ++i ) out << std::setw( 2 ) << unsigned( bytes[ i ] ); return out.str();
  }
};
bool matches( const fs::path& path, const SnapshotFileEntry& expected, const DirectStageOptions& options )
{
  if( !fs::exists( fs::symlink_status( path ) ) ) return false;
  regular( path );
  if( fs::file_size( path ) != expected.size_bytes ) return false;
  FD file( ::open( path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC ) );
  Digest hash; std::array< char, 64 * 1024 > buffer; uint64_t size = 0;
  for( ;; )
  {
    cancelled( options ); const auto count = ::read( file.value, buffer.data(), buffer.size() );
    if( count < 0 && errno == EINTR ) continue;
    require( count >= 0, "direct staged file read failed" ); if( !count ) break;
    hash.update( buffer.data(), size_t( count ) ); size = sum( size, uint64_t( count ) );
    if( size > expected.size_bytes ) return false;
  }
  file.close_checked(); return size == expected.size_bytes && hash.finish() == expected.sha256;
}
void validate_tree( const fs::path& root, const RestoreSpecification& spec )
{
  struct stat root_status{}; require( ::lstat( root.c_str(), &root_status ) == 0, "cannot inspect direct attempt filesystem" );
  std::set< std::string > files{ "ATTEMPT.json", "ATTEMPT.json.tmp", "state.json", "state.json.tmp", "lock", "partial/current.part",
    "validation.json", "validation.json.tmp", "activation-intent.json", "activation-intent.json.tmp" };
  std::set< std::string > directories{ "payload", "progress", "partial" };
  for( size_t i = 0; i < spec.data().files.size(); ++i )
  {
    const auto payload = "payload/" + spec.data().files[ i ].path;
    files.insert( payload ); files.insert( "progress/" + std::to_string( i ) + ".json" ); files.insert( "progress/" + std::to_string( i ) + ".json.tmp" );
    for( auto p = fs::path( payload ).parent_path(); !p.empty(); p = p.parent_path() ) directories.insert( p.generic_string() );
  }
  for( const auto& entry: fs::recursive_directory_iterator( root ) )
  {
    const auto name = entry.path().lexically_relative( root ).generic_string();
    struct stat entry_status{};
    require( ::lstat( entry.path().c_str(), &entry_status ) == 0 && entry_status.st_dev == root_status.st_dev,
             "cross-filesystem entry in direct attempt" );
    require( !entry.is_symlink(), "symlink in direct attempt" );
    if( entry.is_directory() ) require( directories.count( name ), "unexpected directory in direct attempt" );
    else
    {
      require( files.count( name ), "unexpected file in direct attempt" ); regular( entry.path() );
      if( name.rfind( "payload/", 0 ) != 0 && name != "partial/current.part" )
        require( fs::file_size( entry.path() ) <= metadata_limit, "direct attempt metadata exceeds limit" );
    }
  }
}
uint64_t allocated( const fs::path& root )
{
  uint64_t total = 0;
  for( const auto& entry: fs::recursive_directory_iterator( root ) )
  { struct stat s{}; require( ::lstat( entry.path().c_str(), &s ) == 0, "direct allocation stat failed" ); total = sum( total, uint64_t( s.st_blocks ) * 512 ); }
  return total;
}
#endif
} // namespace

std::string read_direct_restore_attempt_identity( const fs::path& attempt )
{
#if defined(__unix__) || defined(__APPLE__)
  // Validate all ancestors without writing anything; a dummy disjoint sibling
  // invokes the same path checks as preflight.
  validate_direct_planning_paths( attempt, fs::absolute( attempt ).parent_path() / ( attempt.filename().string() + ".identity-check" ), "" );
  return bounded_read( attempt / "ATTEMPT.json" );
#else
  throw std::runtime_error( "direct staging is supported only on POSIX platforms" );
#endif
}

std::string read_direct_restore_attempt_state( const fs::path& attempt )
{
#if defined(__unix__) || defined(__APPLE__)
  const auto id = json::parse(read_direct_restore_attempt_identity(attempt));
  const auto state = read_record(attempt / "state.json");
  require(state.at("format") == "teleno-direct-restore-state" && state.at("version") == 1
    && state.at("attempt") == id.at("attempt_dir") && state.at("attempt_id") == id.at("attempt_id"),
    "direct phase hint generation mismatch");
  const auto phase = state.at("state").get<std::string>();
  require(phase == "planned" || phase == "downloading" || phase == "paused" || phase == "download_complete"
    || phase == "validating" || phase == "verified" || phase == "activation_pending" || phase == "activated", "unknown direct attempt phase");
  return phase;
#else
  (void)attempt; throw std::runtime_error("direct attempt state requires POSIX");
#endif
}

DirectStageResult stage_direct_restore_snapshot( const RestoreSpecification& spec, const BackupPublicRestoreConfig& source,
  const fs::path& requested_target, const fs::path& requested_attempt, bool resume, const DirectStageOptions& options )
{
#if defined(__unix__) || defined(__APPLE__)
  const auto& data = spec.data();
  validate_direct_planning_paths( requested_target, requested_attempt, data.source_identity );
  const auto target = fs::absolute( requested_target ).lexically_normal(), attempt = fs::absolute( requested_attempt ).lexically_normal();
  require( target.parent_path() == attempt.parent_path(), "direct stage must be a disjoint sibling of the target" );
  directory( attempt.parent_path() );
  StorageLease storage_lease( target );
  require( !fs::exists( fs::symlink_status( storage_lease.intent_path() ) ),
           "unfinished activation forbids source replay" );
  auto source_identity = source.base_url; while( source_identity.size() > 1 && source_identity.back() == '/' ) source_identity.pop_back();
  require( source_identity.size() <= 4096 && ( source_identity.rfind( "file://", 0 ) == 0
    || source_identity.rfind( "http://", 0 ) == 0 || source_identity.rfind( "https://", 0 ) == 0 ),
    "invalid direct source URL" );
  require( !source.require_https || source_identity.rfind( "https://", 0 ) == 0 || source_identity.rfind( "file://", 0 ) == 0,
           "direct source violates HTTPS policy" );
  require( source_identity == data.source_identity && source.network == data.network,
           "direct source differs from the pinned specification" );
  require( source.retries <= 10 && source.timeout_seconds >= 1 && source.timeout_seconds <= 300,
           "invalid direct retry/deadline policy" );
  require( !source.signature_required || ( data.signature_required && data.signature_verified ), "direct signature policy not satisfied" );
  json identity = { { "format", "teleno-direct-restore-attempt" }, { "version", 1 },
    { "target", target.string() }, { "attempt_dir", attempt.string() }, { "backup_id", data.backup_id },
    { "source_identity", data.source_identity }, { "manifest_sha256", data.manifest_sha256 }, { "inventory_sha256", data.inventory_sha256 },
    { "network", data.network }, { "chain_id", data.chain_id }, { "storage_layout", data.storage_layout },
    { "input_format_version", data.format_version }, { "source_node_version", data.source_node_version },
    { "signature_required", data.signature_required }, { "signature_verified", data.signature_verified },
    { "expected_chain_checked", data.expected_chain_checked }, { "file_count", data.files.size() },
    { "https_required", source.require_https } };
  if( options.reserve_profile != RestoreReserveProfile::standard )
    identity[ "recovery_reserve_profile" ] = restore_reserve_profile_name( options.reserve_profile );
  require( identity.dump().size() <= metadata_limit - 128, "direct attempt identity exceeds limit" );
  const auto planning = build_direct_restore_preflight( spec, target, attempt, options.reserve_profile );
  require( planning.target_eligible, "direct target is not fresh" );
  require( planning.metadata_on_target_filesystem, "direct stage and target must share one filesystem" );
  const auto budget = planning.budget;
  if( resume )
  {
    directory( attempt );
    struct stat status{};
    require( ::lstat( attempt.c_str(), &status ) == 0 && status.st_uid == ::geteuid() && ( status.st_mode & 0077 ) == 0,
             "direct attempt is not private to the current user" );
    const auto pinned = read_record( attempt / "ATTEMPT.json" );
    const auto generation = pinned.at( "attempt_id" ).get< std::string >();
    require( generation.size() == 32 && generation.find_first_not_of( "0123456789abcdef" ) == std::string::npos,
             "invalid direct attempt generation" );
    identity[ "attempt_id" ] = generation;
    require( pinned == identity, "direct attempt identity/source/metadata mismatch" );
  }
  else
  {
    require( !fs::exists( fs::symlink_status( attempt ) ), "direct attempt already exists; explicit resume required" );
    // Preserve reserve before creating anything. A below-recommendation SC1
    // report is not authority to spend the operating reserve during transfer.
    const auto available = options.available_bytes ? options.available_bytes( attempt.parent_path() ) : fs::space( attempt.parent_path() ).available;
    require( available >= budget.recommended_free_bytes, "insufficient direct staging space including reserve" );
    std::random_device random; std::ostringstream generation; generation << std::hex << std::setfill( '0' );
    for( unsigned i = 0; i < 4; ++i ) generation << std::setw( 8 ) << uint32_t( random() );
    identity[ "attempt_id" ] = generation.str();
    require( ::mkdir( attempt.c_str(), 0700 ) == 0, "cannot exclusively create direct attempt" ); sync_directory( attempt.parent_path() );
    persist( attempt / "ATTEMPT.json", identity, options );
    checkpoint( options, "after_descriptor" );
  }
  // This per-attempt lock supplements the common node-startup/restore lease.
  if( fs::exists( attempt / "lock" ) ) regular( attempt / "lock" );
  FD lock( ::open( ( attempt / "lock" ).c_str(), O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600 ) );
  require( ::flock( lock.value, LOCK_EX | LOCK_NB ) == 0, "direct attempt is already in use" );
  sync_directory( attempt );
  validate_tree( attempt, spec );
  if( resume )
  {
    if( fs::exists( attempt / "state.json" ) )
    {
      const auto prior = read_record( attempt / "state.json" );
      require( prior.at( "format" ) == "teleno-direct-restore-state" && prior.at( "version" ) == 1,
               "unsupported direct state format/version" );
      require( prior.at( "attempt" ) == identity.at( "attempt_dir" ), "direct state belongs to another attempt" );
      require( prior.at( "attempt_id" ) == identity.at( "attempt_id" ), "direct state belongs to another generation" );
      const auto phase = prior.at( "state" ).get< std::string >();
      require( phase == "planned" || phase == "downloading" || phase == "paused" || phase == "download_complete",
               "direct attempt phase forbids source replay; retain for validation recovery" );
    }
    else
    {
      // Recover a crash after descriptor publication, before the first state.
      // Missing state with any payload/progress is ambiguous and fails closed.
      for( const auto& dir: { "payload", "partial", "progress" } )
        require( !fs::exists( attempt / dir ) || fs::is_empty( attempt / dir ),
                 "missing direct state with existing payload/progress" );
    }
  }
  for( const auto& dir: { "payload", "partial", "progress" } ) make_directory( attempt / dir );
  DirectStageResult result;
  result.backup_id = data.backup_id; result.target = target; result.attempt_dir = attempt;
  result.attempt_id = identity.at( "attempt_id" ).get< std::string >();
  const auto save_state = [&]( const char* state, const std::string& reason = {} ) {
    persist( attempt / "state.json", json{ { "format", "teleno-direct-restore-state" }, { "version", 1 },
      { "attempt", attempt.string() }, { "attempt_id", result.attempt_id }, { "state", state }, { "reason", reason } }, options ); result.state = state;
  };
  if( !resume ) save_state( "planned" );
  uint64_t verified_allocation = 0;
  const auto space_check = [&]( uint64_t current = 0 ) {
    const auto allocated_current = rounded( current, budget.allocation_quantum );
    const auto covered = sum( verified_allocation, allocated_current );
    require( covered <= budget.allocated_payload_bytes, "direct space accounting exceeds inventory" );
    result.required_additional_bytes = sum( budget.recommended_free_bytes - budget.allocated_payload_bytes,
                                           budget.allocated_payload_bytes - covered );
    const auto available = options.available_bytes ? options.available_bytes( attempt ) : fs::space( attempt ).available;
    require( available >= result.required_additional_bytes, "insufficient direct staging space including reserve" );
  };
  try
  {
    save_state( "downloading" );
    std::vector< bool > reusable; reusable.reserve( data.files.size() );
    // Rehash all existing files before subtracting bytes or contacting the source.
    for( const auto& file: data.files )
    {
      const bool valid = matches( attempt / "payload" / file.path, file, options ); reusable.push_back( valid );
      if( valid ) verified_allocation = sum( verified_allocation, rounded( file.size_bytes, budget.allocation_quantum ) );
    }
    // One owned partial only; never create failed generations or a hash cache.
    owned_remove( attempt / "partial/current.part" );
    // Discard only invalid files of this authenticated, locked, pre-validation
    // attempt before checking incremental demand. Otherwise a corrupt complete
    // file could be counted as both occupied space and a full new allocation.
    for( size_t i = 0; i < data.files.size(); ++i )
      if( !reusable[ i ] ) owned_remove( attempt / "payload" / data.files[ i ].path );
    space_check();
    for( size_t i = 0; i < data.files.size(); ++i )
    {
      cancelled( options ); const auto& file = data.files[ i ]; const auto destination = attempt / "payload" / file.path;
      if( reusable[ i ] )
      {
        // A crash after rename but before progress publication is recovered by
        // the rehash, not by trusting a missing or stale completion record.
        persist( attempt / "progress" / ( std::to_string( i ) + ".json" ),
          json{ { "attempt_id", result.attempt_id }, { "path", file.path }, { "sha256", file.sha256 }, { "size_bytes", file.size_bytes }, { "complete", true } }, options, i );
        ++result.reused_files; result.payload_bytes = sum( result.payload_bytes, file.size_bytes ); continue;
      }
      auto parent = attempt / "payload";
      for( const auto& part: fs::path( file.path ).parent_path() ) { parent /= part; make_directory( parent ); }
      owned_remove( destination ); space_check();
      const auto partial = attempt / "partial/current.part";
      for( uint64_t retry = 0; ; ++retry )
      {
        owned_remove( partial ); cancelled( options );
        try
        {
          FD out( ::open( partial.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600 ) );
          Digest hash; uint64_t written = 0, next_check = 0;
          stream_public_restore_object( source, file, [&]( const char* bytes, size_t count ) {
            cancelled( options ); checkpoint( options, "chunk_write", i );
            require( count <= file.size_bytes - written, "direct write exceeds inventory size" );
            if( written >= next_check ) { space_check( written ); next_check = sum( written, 8 * 1024 * 1024 ); }
            write_all( out, bytes, count ); hash.update( bytes, count ); written += count;
            result.transferred_bytes = sum( result.transferred_bytes, count );
          }, options.transfer );
          if( written != file.size_bytes || hash.finish() != file.sha256 ) throw DirectTransferError( "direct object checksum/size mismatch" );
          checkpoint( options, "file_sync", i ); require( ::fsync( out.value ) == 0, "direct staged file sync failed" ); out.close_checked();
          checkpoint( options, "file_rename", i );
          require( ::rename( partial.c_str(), destination.c_str() ) == 0, "direct staged file rename failed" );
          sync_directory( destination.parent_path() ); sync_directory( partial.parent_path() );
          checkpoint( options, "after_file_rename", i );
          persist( attempt / "progress" / ( std::to_string( i ) + ".json" ),
            json{ { "attempt_id", result.attempt_id }, { "path", file.path }, { "sha256", file.sha256 }, { "size_bytes", file.size_bytes }, { "complete", true } }, options, i );
          verified_allocation = sum( verified_allocation, rounded( file.size_bytes, budget.allocation_quantum ) );
          ++result.downloaded_files; result.payload_bytes = sum( result.payload_bytes, file.size_bytes ); space_check(); break;
        }
        catch( const DirectTransferError& )
        {
          if( retry + 1 >= std::max< uint64_t >( 1, source.retries ) ) throw;
          checkpoint( options, "retry", i );
          for( unsigned tick = 0; tick < 5; ++tick ) { cancelled( options ); std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) ); }
        }
      }
    }
    // Detect changes after reuse/download before recording completion. No DB open.
    validate_tree( attempt, spec );
    for( const auto& file: data.files ) require( matches( attempt / "payload" / file.path, file, options ), "direct payload changed before completion" );
    require( build_direct_restore_preflight( spec, target, attempt, options.reserve_profile ).target_eligible, "direct target changed while downloading" );
    space_check(); checkpoint( options, "download_complete", data.files.size() ); save_state( "download_complete" );
    result.download_complete = true;
  }
  catch( const std::exception& error )
  {
    result.error = error.what(); result.download_complete = false;
    // If persisting failure itself fails, propagate the error. Never report success.
    save_state( "paused", "transfer incomplete; explicit same-input resume required" );
  }
  result.allocated_attempt_bytes = allocated( attempt ); return result;
#else
  (void)spec; (void)source; (void)requested_target; (void)requested_attempt; (void)resume; (void)options;
  throw std::runtime_error( "direct staging is supported only on POSIX platforms" );
#endif
}

namespace {
#if defined(__unix__) || defined(__APPLE__)
json bind_attempt( const RestoreSpecification& spec, const fs::path& target, const fs::path& attempt,
  RestoreReserveProfile reserve_profile )
{
  validate_direct_planning_paths( target, attempt, spec.data().source_identity );
  require( target.parent_path() == attempt.parent_path(), "direct activation requires sibling paths" );
  directory( attempt );
  struct stat st{};
  require( ::lstat( attempt.c_str(), &st ) == 0 && st.st_uid == ::geteuid() && (st.st_mode & 077) == 0,
    "activation attempt is not private" );
  const auto id = read_record( attempt / "ATTEMPT.json" ); const auto& d = spec.data();
  const auto generation = id.at("attempt_id").get<std::string>();
  require( generation.size() == 32 && generation.find_first_not_of("0123456789abcdef") == std::string::npos,
    "invalid activation generation" );
  json expected = { {"format", "teleno-direct-restore-attempt"}, {"version", 1},
    {"target", target.string()}, {"attempt_dir", attempt.string()}, {"backup_id", d.backup_id},
    {"source_identity", d.source_identity}, {"manifest_sha256", d.manifest_sha256}, {"inventory_sha256", d.inventory_sha256},
    {"network", d.network}, {"chain_id", d.chain_id}, {"storage_layout", d.storage_layout},
    {"input_format_version", d.format_version}, {"source_node_version", d.source_node_version},
    {"signature_required", d.signature_required}, {"signature_verified", d.signature_verified},
    {"expected_chain_checked", d.expected_chain_checked}, {"file_count", d.files.size()},
    {"https_required", id.at("https_required").get<bool>()}, {"attempt_id", generation} };
  if( reserve_profile != RestoreReserveProfile::standard )
    expected[ "recovery_reserve_profile" ] = restore_reserve_profile_name( reserve_profile );
  require( id == expected, "activation identity/specification mismatch" );
  validate_tree( attempt, spec );
  return id;
}
json state_record( const fs::path& attempt, const json& id )
{
  const auto state = read_record( attempt / "state.json" );
  require( state.at("format") == "teleno-direct-restore-state" && state.at("version") == 1
    && state.at("attempt") == attempt.string() && state.at("attempt_id") == id.at("attempt_id"),
    "activation state generation mismatch" );
  return state;
}
void phase( const fs::path& attempt, const json& id, const std::string& state, const DirectStageOptions& options )
{
  checkpoint( options, "before_" + state );
  persist( attempt / "state.json", { {"format", "teleno-direct-restore-state"}, {"version", 1},
    {"attempt", attempt.string()}, {"attempt_id", id.at("attempt_id")}, {"state", state}, {"reason", ""} }, options, 1000 );
  checkpoint( options, "after_" + state );
}
void rehash_payload( const RestoreSpecification& spec, const fs::path& payload, const DirectStageOptions& options )
{
  for( const auto& file: spec.data().files )
    require( matches( payload / file.path, file, options ), "offline payload changed or incomplete; preserve attempt, do not replay source" );
}
void validation_space( const RestoreSpecification& spec, const fs::path& attempt, const DirectStageOptions& options )
{
  RestoreBudgetPolicy policy; policy.reserve_profile = options.reserve_profile;
  const auto budget = estimate_mode_restore_space( spec, RestoreMode::direct, policy );
  const auto free = options.available_bytes ? options.available_bytes(attempt.parent_path()) : fs::space(attempt.parent_path()).available;
  require( free >= sum( budget.reserve_bytes, sum(budget.validation_allowance_bytes, budget.metadata_journal_bytes) ),
    "insufficient offline validation/activation space including reserve" );
}
json validation_record( const RestoreSpecification& spec, const json& id, const fs::path& payload,
  const DirectStageOptions& options )
{
  checkpoint(options,"before_offline_database_validation");
  const auto validation = validate_offline_restore_database(spec,payload,[&]{ cancelled(options); });
  checkpoint(options,"after_offline_database_validation");
  return { {"format", "teleno-direct-restore-verified"}, {"version", 1}, {"attempt_id", id.at("attempt_id")},
    {"manifest_sha256", spec.data().manifest_sha256}, {"inventory_sha256", spec.data().inventory_sha256},
    {"validation", json::parse(validation)} };
}
void exclusive_rename( const fs::path& from, const fs::path& to )
{
#if defined(__APPLE__)
  const auto result = ::renamex_np( from.c_str(), to.c_str(), RENAME_EXCL );
#elif defined(__linux__)
  const auto result = ::syscall( SYS_renameat2, AT_FDCWD, from.c_str(), AT_FDCWD, to.c_str(), 1 /* RENAME_NOREPLACE */ );
#else
  const auto result = -1; // Never emulate exclusivity with exists()+rename().
#endif
  require( result == 0, "exclusive same-filesystem restore rename failed; no copy/overwrite fallback" );
  sync_directory(from.parent_path()); sync_directory(to.parent_path());
}
std::pair<uint64_t,uint64_t> inode( const fs::path& path )
{
  directory(path); struct stat st{};
  require(::lstat(path.c_str(), &st) == 0, "cannot inspect restore database identity");
  return {uint64_t(st.st_dev), uint64_t(st.st_ino)};
}
json hold_record( const json& id )
{
  return { {"format", "teleno-direct-restore-observer-hold"}, {"version", 1},
    {"attempt_id", id.at("attempt_id")}, {"target", id.at("target")},
    {"manifest_sha256", id.at("manifest_sha256")}, {"inventory_sha256", id.at("inventory_sha256")} };
}
// A fresh target may contain reviewed inputs, then only this generation's
// markers and the exact renamed DB. Config is never overwritten or imported.
void activation_target( const RestoreSpecification& spec, const fs::path& target, const json& id,
  const json& intent, const DirectStageOptions& options )
{
  if(!fs::exists(fs::symlink_status(target))) return;
  directory(target);
  require(inode(target).first == inode(fs::path(id.at("attempt_dir").get<std::string>())).first,
    "activation target filesystem changed; cross-device activation is forbidden");
  const auto hold = hold_record(id);
  const bool moved = fs::exists(fs::symlink_status(target / "db"));
  if(moved)
  {
    const auto identity = inode(target / "db");
    require(identity.first == intent.at("db_device") && identity.second == intent.at("db_inode"),
      "target DB belongs to a different generation" );
  }
  for(const auto& entry: fs::recursive_directory_iterator(target))
  {
    const auto relative = entry.path().lexically_relative(target).generic_string();
    require(!entry.is_symlink(), "symlink in activation target");
    if(entry.is_directory())
    {
      require(relative == "db" || relative.rfind("db/",0) == 0 || relative == "chain"
        || relative == "jsonrpc" || relative == "jsonrpc/descriptors", "populated activation target");
      continue;
    }
    regular(entry.path());
    if(relative == "config.yml") continue;
    if(relative == ".backup-just-restored" || relative == ".backup-observer-recovery"
      || relative == ".teleno-direct-restore.json")
    { require(read_record(entry.path()) == hold, "foreign restore/recovery marker"); continue; }
    if(relative == ".backup-just-restored.tmp" || relative == ".backup-observer-recovery.tmp"
      || relative == ".teleno-direct-restore.json.tmp")
    { require(fs::file_size(entry.path()) <= metadata_limit, "oversized owned marker temporary"); continue; }
    const auto found = std::find_if(spec.data().files.begin(), spec.data().files.end(),
      [&](const auto& file){return file.path == relative;});
    require(found != spec.data().files.end() && matches(entry.path(), *found, options),
      "unknown or modified activation target file");
  }
}
#endif
}

#if defined(__unix__) || defined(__APPLE__)
namespace {
// These helpers require the caller to retain both the target lease and the
// attempt lock. A fresh proof is private to one synchronous completion call.
DirectActivationResult validate_direct_restore_locked( const RestoreSpecification& spec,
  const fs::path& target, const fs::path& attempt, const json& id,
  const DirectStageOptions& options, json* fresh_proof = nullptr )
{
  const auto old = state_record(attempt,id).at("state").get<std::string>();
  require(old == "download_complete" || old == "validating" || old == "verified", "offline validation requires complete downloaded payload");
  const auto plan = build_direct_restore_preflight(spec,target,attempt,options.reserve_profile);
  require(plan.target_eligible && plan.metadata_on_target_filesystem, "offline target must be fresh and on the same filesystem");
  rehash_payload(spec,attempt / "payload",options); validation_space(spec,attempt,options);
  // Persist the phase before any DB open. Only read-only WAL replay is supported;
  // writable RocksDB recovery is deliberately refused rather than redownloaded.
  phase(attempt,id,"validating",options);
  checkpoint(options,"before_database_validation");
  const auto verified = validation_record(spec,id,attempt / "payload",options);
  checkpoint(options,"after_database_validation");
  rehash_payload(spec,attempt / "payload",options); validation_space(spec,attempt,options);
  persist(attempt / "validation.json",verified,options,1001);
  checkpoint(options,"after_validation_record");
  phase(attempt,id,"verified",options);
  if(fresh_proof) *fresh_proof = verified;
  return {id.at("attempt_id"), "verified", verified.at("validation").dump(), false};
}

DirectActivationResult activate_direct_restore_locked( const RestoreSpecification& spec,
  const fs::path& target, const fs::path& attempt, const StorageLease& lease,
  const json& id, const DirectStageOptions& options, const json* fresh_proof = nullptr )
{
  const auto old = state_record(attempt,id).at("state").get<std::string>();
  require(old == "verified" || old == "activation_pending" || old == "activated", "attempt is not verified for activation");
  const auto verified = read_record(attempt / "validation.json");
  require(verified.at("format") == "teleno-direct-restore-verified" && verified.at("version") == 1
    && verified.at("attempt_id") == id.at("attempt_id") && verified.at("manifest_sha256") == id.at("manifest_sha256")
    && verified.at("inventory_sha256") == id.at("inventory_sha256"), "verified generation mismatch");
  require(!fresh_proof || *fresh_proof == verified, "fresh database verification record changed before activation");
  if(old == "activated" && !fs::exists(fs::symlink_status(lease.intent_path())))
  {
    require(read_record(target / ".teleno-direct-restore.json") == hold_record(id), "activated receipt mismatch");
    return {id.at("attempt_id"),"activated",verified.at("validation").dump(), fs::exists(target / ".backup-observer-recovery")};
  }
  json intent;
  if(fs::exists(fs::symlink_status(lease.intent_path())))
  {
    intent = read_record(lease.intent_path());
    require(intent.at("format") == "teleno-direct-restore-activation" && intent.at("version") == 1
      && intent.at("identity") == id && intent.at("verified") == verified, "foreign/conflicting activation intent");
  }
  else
  {
    require(old == "verified", "activation intent missing; preserve sole database for investigation");
    const auto plan = build_direct_restore_preflight(spec,target,attempt,options.reserve_profile);
    require(plan.target_eligible && plan.metadata_on_target_filesystem, "activation target is not fresh or same-filesystem");
    rehash_payload(spec,attempt / "payload",options);
    // The complete payload is rehashed even when this same locked call just
    // validated it. Identical authenticated bytes may reuse that in-memory
    // proof; persisted records and recovered attempts always scan afresh.
    if(!fresh_proof)
      require(validation_record(spec,id,attempt / "payload",options) == verified, "database verification changed before activation");
    const auto db = inode(attempt / "payload/db");
    intent = { {"format","teleno-direct-restore-activation"}, {"version",1}, {"identity",id},
      {"verified",verified}, {"db_device",db.first}, {"db_inode",db.second} };
    validation_space(spec,attempt,options);
    checkpoint(options,"before_activation_intent");
    // Build the intent inside the private attempt, then publish without
    // replacing any external sidecar. A crash before publication leaves only
    // an owned temporary, never an external file that must be guessed/deleted.
    persist(attempt / "activation-intent.json",intent,options,1002);
    checkpoint(options,"before_intent_publish");
    exclusive_rename(attempt / "activation-intent.json",lease.intent_path());
    checkpoint(options,"after_activation_intent");
  }
  phase(attempt,id,"activation_pending",options);
  activation_target(spec,target,id,intent,options);
  make_directory(target);
  const auto hold = hold_record(id);
  for(const auto& name: {".backup-observer-recovery", ".backup-just-restored", ".teleno-direct-restore.json"})
  {
    checkpoint(options,std::string("before_marker:") + name);
    persist(target / name,hold,options,1003);
    checkpoint(options,std::string("after_marker:") + name);
  }
  for(const auto& file: spec.data().files)
  {
    if(!file.runtime_file || file.path == "config.yml") continue;
    const auto destination = target / file.path;
    if(fs::exists(fs::symlink_status(destination)))
      require(matches(destination,file,options),"runtime input changed during activation");
    else
    {
      auto parent = destination.parent_path();
      if(parent.parent_path() != target && parent != target) make_directory(parent.parent_path());
      make_directory(parent);
      checkpoint(options,"before_runtime_rename", &file - spec.data().files.data());
      exclusive_rename(attempt / "payload" / file.path,destination);
      checkpoint(options,"after_runtime_rename", &file - spec.data().files.data());
    }
  }
  const auto from = attempt / "payload/db", to = target / "db";
  const bool staged = fs::exists(fs::symlink_status(from)), moved = fs::exists(fs::symlink_status(to));
  require(staged != moved,"activation requires exactly one owned database");
  const auto db = inode(staged ? from : to);
  require(db.first == intent.at("db_device") && db.second == intent.at("db_inode"),"activation DB inode mismatch");
  activation_target(spec,target,id,intent,options);
  // Rehash every DB file immediately before/after the rename. No copying and
  // no writable validation; an interrupted attempt's only DB is never replaced.
  for(const auto& file: spec.data().files)
    if(!file.runtime_file) require(matches((staged ? attempt / "payload" : target) / file.path,file,options),"activation DB bytes changed");
  validation_space(spec,attempt,options); cancelled(options);
  if(staged)
  {
    checkpoint(options,"before_database_rename");
    activation_target(spec,target,id,intent,options);
    exclusive_rename(from,to);
    checkpoint(options,"after_database_rename");
  }
  require(validation_record(spec,id,target,options) == verified,"activated database failed offline reopen validation");
  checkpoint(options,"after_activated_reopen");
  phase(attempt,id,"activated",options);
  checkpoint(options,"before_intent_remove"); owned_remove(lease.intent_path());
  checkpoint(options,"after_intent_remove");
  return {id.at("attempt_id"),"activated",verified.at("validation").dump(),true};
}
} // namespace
#endif

DirectActivationResult validate_direct_restore_attempt( const RestoreSpecification& spec,
  const fs::path& requested_target, const fs::path& requested_attempt, const DirectStageOptions& options )
{
#if defined(__unix__) || defined(__APPLE__)
  validate_direct_planning_paths(requested_target,requested_attempt,spec.data().source_identity);
  const auto target = fs::absolute(requested_target).lexically_normal(), attempt = fs::absolute(requested_attempt).lexically_normal();
  StorageLease lease(target);
  SyncCheckpoints sync_checkpoints(options);
  require(!fs::exists(fs::symlink_status(lease.intent_path())), "pending activation requires activation recovery, not validation/source replay");
  const auto id = bind_attempt(spec,target,attempt,options.reserve_profile);
  FD lock(::open((attempt / "lock").c_str(),O_RDWR | O_NOFOLLOW | O_CLOEXEC));
  require(::flock(lock.value,LOCK_EX | LOCK_NB) == 0,"direct attempt busy");
  return validate_direct_restore_locked(spec,target,attempt,id,options);
#else
  (void)spec; (void)requested_target; (void)requested_attempt; (void)options;
  throw std::runtime_error("direct offline validation requires POSIX");
#endif
}

DirectActivationResult activate_direct_restore_attempt( const RestoreSpecification& spec,
  const fs::path& requested_target, const fs::path& requested_attempt, const DirectStageOptions& options )
{
#if defined(__unix__) || defined(__APPLE__)
  validate_direct_planning_paths(requested_target,requested_attempt,spec.data().source_identity);
  const auto target = fs::absolute(requested_target).lexically_normal(), attempt = fs::absolute(requested_attempt).lexically_normal();
  StorageLease lease(target);
  SyncCheckpoints sync_checkpoints(options);
  const auto id = bind_attempt(spec,target,attempt,options.reserve_profile);
  FD lock(::open((attempt / "lock").c_str(),O_RDWR | O_NOFOLLOW | O_CLOEXEC));
  require(::flock(lock.value,LOCK_EX | LOCK_NB) == 0,"direct attempt busy");
  return activate_direct_restore_locked(spec,target,attempt,lease,id,options);
#else
  (void)spec; (void)requested_target; (void)requested_attempt; (void)options;
  throw std::runtime_error("direct activation requires POSIX");
#endif
}

DirectActivationResult validate_and_activate_direct_restore_attempt( const RestoreSpecification& spec,
  const fs::path& requested_target, const fs::path& requested_attempt, const DirectStageOptions& options )
{
#if defined(__unix__) || defined(__APPLE__)
  validate_direct_planning_paths(requested_target,requested_attempt,spec.data().source_identity);
  const auto target = fs::absolute(requested_target).lexically_normal(), attempt = fs::absolute(requested_attempt).lexically_normal();
  StorageLease lease(target);
  SyncCheckpoints sync_checkpoints(options);
  const auto id = bind_attempt(spec,target,attempt,options.reserve_profile);
  FD lock(::open((attempt / "lock").c_str(),O_RDWR | O_NOFOLLOW | O_CLOEXEC));
  require(::flock(lock.value,LOCK_EX | LOCK_NB) == 0,"direct attempt busy");
  const auto old = state_record(attempt,id).at("state").get<std::string>();
  json fresh_proof;
  if(old == "download_complete" || old == "validating" || old == "verified")
  {
    require(!fs::exists(fs::symlink_status(lease.intent_path())), "pending activation requires activation recovery, not validation/source replay");
    validate_direct_restore_locked(spec,target,attempt,id,options,&fresh_proof);
  }
  else require(old == "activation_pending" || old == "activated", "attempt is not ready for validation or activation");
  checkpoint(options,"before_activation");
  validate_direct_planning_paths(target,attempt,spec.data().source_identity);
  require(bind_attempt(spec,target,attempt,options.reserve_profile) == id, "direct attempt identity changed before activation");
  return activate_direct_restore_locked(spec,target,attempt,lease,id,options,fresh_proof.is_null() ? nullptr : &fresh_proof);
#else
  (void)spec; (void)requested_target; (void)requested_attempt; (void)options;
  throw std::runtime_error("direct validation and activation require POSIX");
#endif
}

std::string direct_stage_result_to_json( const DirectStageResult& r )
{
  return json{ { "format", "teleno-direct-restore-stage" }, { "version", 1 }, { "backup_id", r.backup_id },
    { "attempt_id", r.attempt_id },
    { "attempt_dir", r.attempt_dir.string() }, { "target", r.target.string() }, { "state", r.state }, { "error", r.error },
    { "download_complete", r.download_complete }, { "downloaded_files", r.downloaded_files }, { "reused_files", r.reused_files },
    { "transferred_bytes", r.transferred_bytes }, { "payload_bytes", r.payload_bytes },
    { "allocated_attempt_bytes", r.allocated_attempt_bytes }, { "required_additional_bytes", r.required_additional_bytes },
    { "target_database_opened", false }, { "ready_to_activate", false }, { "observer_first_required", true },
    { "capacity_40gb_qualified", false } }.dump( 2 ) + "\n";
}
} // namespace koinos::node::backup
