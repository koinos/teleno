#include "backup/snapshot_repository.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#if !defined( _WIN32 )
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "core/node_version.hpp"

namespace koinos::node::backup {
namespace {

constexpr uint64_t metadata_overhead_bytes = 128ULL * 1024ULL * 1024ULL;
constexpr uint64_t recommended_min_margin_bytes = 10ULL * 1024ULL * 1024ULL * 1024ULL;

std::string utc_timestamp()
{
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t( now );
  std::tm tm{};
#if defined( _WIN32 )
  gmtime_s( &tm, &time );
#else
  gmtime_r( &time, &tm );
#endif

  std::ostringstream out;
  out << std::put_time( &tm, "%Y%m%dT%H%M%SZ" );
  return out.str();
}

uint64_t epoch_milliseconds()
{
  const auto now = std::chrono::system_clock::now();
  return static_cast< uint64_t >(
    std::chrono::duration_cast< std::chrono::milliseconds >( now.time_since_epoch() ).count() );
}

std::string make_backup_id( const CheckpointResult& checkpoint )
{
  std::ostringstream out;
  out << utc_timestamp()
      << "-ms-" << epoch_milliseconds()
      << "-files-" << checkpoint.file_count;
  return out.str();
}

std::string json_escape( const std::string& value )
{
  std::ostringstream out;
  for( char ch: value )
  {
    switch( ch )
    {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\b':
        out << "\\b";
        break;
      case '\f':
        out << "\\f";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
      {
        const auto byte = static_cast< unsigned char >( ch );
        if( byte < 0x20 )
          out << "\\u00" << "0123456789abcdef"[ ( byte >> 4 ) & 0xf ]
              << "0123456789abcdef"[ byte & 0xf ];
        else
          out << ch;
      }
    }
  }
  return out.str();
}

std::string json_bool( bool value )
{
  return value ? "true" : "false";
}

std::string bytes_to_hex( const unsigned char* data, unsigned int size )
{
  std::ostringstream out;
  out << std::hex << std::setfill( '0' );
  for( unsigned int i = 0; i < size; ++i )
    out << std::setw( 2 ) << static_cast< unsigned int >( data[ i ] );
  return out.str();
}

std::string sha256_file( const std::filesystem::path& path )
{
  std::ifstream input( path, std::ios::binary );
  if( !input )
    throw std::runtime_error( "failed to open file for SHA-256: " + path.string() );

  EVP_MD_CTX* raw_ctx = EVP_MD_CTX_new();
  if( !raw_ctx )
    throw std::runtime_error( "failed to allocate SHA-256 context" );
  std::unique_ptr< EVP_MD_CTX, decltype( &EVP_MD_CTX_free ) > ctx( raw_ctx, EVP_MD_CTX_free );

  if( EVP_DigestInit_ex( ctx.get(), EVP_sha256(), nullptr ) != 1 )
    throw std::runtime_error( "failed to initialize SHA-256 context" );

  std::vector< char > buffer( 1024 * 1024 );
  while( input )
  {
    input.read( buffer.data(), static_cast< std::streamsize >( buffer.size() ) );
    const auto count = input.gcount();
    if( count > 0
        && EVP_DigestUpdate( ctx.get(), buffer.data(), static_cast< std::size_t >( count ) ) != 1 )
      throw std::runtime_error( "failed to update SHA-256 for file: " + path.string() );
  }

  unsigned char digest[ EVP_MAX_MD_SIZE ];
  unsigned int digest_size = 0;
  if( EVP_DigestFinal_ex( ctx.get(), digest, &digest_size ) != 1 )
    throw std::runtime_error( "failed to finalize SHA-256 for file: " + path.string() );

  return bytes_to_hex( digest, digest_size );
}

uint64_t file_size_or_zero( const std::filesystem::path& path )
{
  std::error_code ec;
  if( !std::filesystem::is_regular_file( path, ec ) )
    return 0;
  return std::filesystem::file_size( path, ec );
}

std::string read_text_file( const std::filesystem::path& path )
{
  std::ifstream input( path, std::ios::binary );
  if( !input )
    throw std::runtime_error( "failed to read file: " + path.string() );
  return std::string( ( std::istreambuf_iterator< char >( input ) ),
                      std::istreambuf_iterator< char >() );
}

uint64_t directory_size_or_zero( const std::filesystem::path& path )
{
  std::error_code ec;
  if( !std::filesystem::exists( path, ec ) )
    return 0;

  uint64_t total = 0;
  for( const auto& entry: std::filesystem::recursive_directory_iterator( path, ec ) )
  {
    if( ec )
      break;
    if( !entry.is_regular_file( ec ) )
      continue;
    const auto size = entry.file_size( ec );
    if( !ec )
      total += size;
    ec.clear();
  }
  return total;
}

uint64_t existing_restore_target_bytes( const std::filesystem::path& target_basedir )
{
  return directory_size_or_zero( target_basedir / "db" )
       + directory_size_or_zero( target_basedir / "chain" / "blockchain" );
}

uint64_t available_space_bytes( std::filesystem::path path )
{
  std::error_code ec;
  while( !path.empty() && !std::filesystem::exists( path, ec ) )
  {
    ec.clear();
    path = path.parent_path();
  }
  if( path.empty() )
    path = std::filesystem::current_path();

  const auto info = std::filesystem::space( path, ec );
  if( ec )
    throw std::runtime_error( "failed to read available space at " + path.string() + ": " + ec.message() );
  return info.available;
}

void add_file_entry( std::vector< SnapshotFileEntry >& entries,
                     const std::filesystem::path& source,
                     const std::string& relative_path,
                     bool runtime_file )
{
  std::error_code ec;
  if( !std::filesystem::is_regular_file( source, ec ) )
    return;

  SnapshotFileEntry entry;
  entry.path = relative_path;
  entry.sha256 = sha256_file( source );
  entry.size_bytes = std::filesystem::file_size( source );
  entry.runtime_file = runtime_file;
  entries.push_back( std::move( entry ) );
}

std::vector< SnapshotFileEntry > build_file_inventory( const CheckpointResult& checkpoint,
                                                       const std::filesystem::path& basedir,
                                                       const std::filesystem::path& config_path )
{
  std::vector< SnapshotFileEntry > entries;
  std::error_code ec;

  for( const auto& item: std::filesystem::recursive_directory_iterator( checkpoint.checkpoint_dir, ec ) )
  {
    if( ec )
      throw std::runtime_error( "failed while scanning checkpoint directory: " + ec.message() );
    if( !item.is_regular_file( ec ) )
      continue;

    auto relative = std::filesystem::relative( item.path(), checkpoint.checkpoint_dir, ec );
    if( ec )
      throw std::runtime_error( "failed to compute checkpoint relative path: " + ec.message() );
    add_file_entry( entries, item.path(), relative.generic_string(), false );
  }

  add_file_entry( entries, config_path, "config.yml", true );
  if( std::filesystem::exists( basedir / "chain" / "genesis_data.json" ) )
    add_file_entry( entries, basedir / "chain" / "genesis_data.json", "chain/genesis_data.json", true );
  else
    add_file_entry( entries, basedir / "genesis_data.json", "chain/genesis_data.json", true );
  add_file_entry( entries,
                  basedir / "jsonrpc" / "descriptors" / "koinos_descriptors.pb",
                  "jsonrpc/descriptors/koinos_descriptors.pb",
                  true );

  std::map< std::string, SnapshotFileEntry > unique_by_path;
  for( auto& entry: entries )
    unique_by_path[ entry.path ] = std::move( entry );

  entries.clear();
  entries.reserve( unique_by_path.size() );
  for( auto& [_, entry]: unique_by_path )
    entries.push_back( std::move( entry ) );

  std::sort( entries.begin(), entries.end(), []( const auto& lhs, const auto& rhs ) {
    return lhs.path < rhs.path;
  } );

  return entries;
}

std::filesystem::path object_path( const std::filesystem::path& repository_dir, const std::string& sha256 )
{
  if( sha256.size() != 64 )
    throw std::runtime_error( "invalid SHA-256 length for object path" );
  return repository_dir / "objects" / "sha256" / sha256.substr( 0, 2 ) / sha256.substr( 2, 2 ) / sha256;
}

std::vector< std::filesystem::path > completed_snapshot_dirs( const std::filesystem::path& repository_dir )
{
  std::vector< std::filesystem::path > snapshots;
  const auto snapshots_dir = repository_dir / "snapshots";
  if( !std::filesystem::exists( snapshots_dir ) )
    return snapshots;

  for( const auto& entry: std::filesystem::directory_iterator( snapshots_dir ) )
  {
    if( !entry.is_directory() )
      continue;
    const auto name = entry.path().filename().string();
    if( name.size() >= 8 && name.substr( name.size() - 8 ) == ".partial" )
      continue;
    if( std::filesystem::exists( entry.path() / "COMPLETE" ) )
      snapshots.push_back( entry.path() );
  }

  std::sort( snapshots.begin(), snapshots.end() );
  return snapshots;
}

void collect_snapshot_object_hashes( const std::filesystem::path& snapshot_dir,
                                     std::set< std::string >& referenced_hashes )
{
  const auto files_path = snapshot_dir / "files.json";
  if( !std::filesystem::is_regular_file( files_path ) )
    return;

  const auto files = nlohmann::json::parse( read_text_file( files_path ) );
  for( const auto& file: files.at( "files" ) )
    referenced_hashes.insert( file.at( "sha256" ).get< std::string >() );
}

std::map< std::string, uint64_t > read_snapshot_object_sizes( const std::filesystem::path& snapshot_dir )
{
  std::map< std::string, uint64_t > objects;
  const auto files_path = snapshot_dir / "files.json";
  if( !std::filesystem::is_regular_file( files_path ) )
    return objects;

  const auto files = nlohmann::json::parse( read_text_file( files_path ) );
  for( const auto& file: files.at( "files" ) )
  {
    const auto sha256 = file.at( "sha256" ).get< std::string >();
    if( sha256.size() != 64 )
      throw std::runtime_error( "invalid SHA-256 in backup files manifest: " + sha256 );
    objects.emplace( sha256, file.value( "size_bytes", 0ULL ) );
  }
  return objects;
}

std::pair< uint64_t, uint64_t > directory_file_stats( const std::filesystem::path& directory )
{
  uint64_t file_count = 0;
  uint64_t bytes = 0;
  if( !std::filesystem::exists( directory ) )
    return { file_count, bytes };

  for( const auto& entry: std::filesystem::recursive_directory_iterator( directory ) )
  {
    if( !entry.is_regular_file() )
      continue;
    ++file_count;
    std::error_code ec;
    const auto size = entry.file_size( ec );
    if( !ec )
      bytes += size;
  }
  return { file_count, bytes };
}

std::string latest_json_for_backup_id( const std::string& backup_id )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"format\": \"teleno-native-latest-snapshot\",\n";
  out << "  \"version\": 1,\n";
  out << "  \"backup_id\": \"" << json_escape( backup_id ) << "\",\n";
  out << "  \"snapshot_dir\": \"" << json_escape( backup_id ) << "\",\n";
  out << "  \"manifest\": \"snapshots/" << json_escape( backup_id ) << "/manifest.json\",\n";
  out << "  \"files\": \"snapshots/" << json_escape( backup_id ) << "/files.json\"\n";
  out << "}\n";
  return out.str();
}

std::string newest_completed_backup_id( const std::filesystem::path& repository_dir,
                                        const std::string& excluded_backup_id )
{
  std::string newest;
  for( const auto& snapshot_dir: completed_snapshot_dirs( repository_dir ) )
  {
    const auto backup_id = snapshot_dir.filename().string();
    if( backup_id == excluded_backup_id )
      continue;
    if( newest.empty() || backup_id > newest )
      newest = backup_id;
  }
  return newest;
}

void prune_empty_object_directories( const std::filesystem::path& objects_root )
{
  if( !std::filesystem::exists( objects_root ) )
    return;

  std::vector< std::filesystem::path > directories;
  for( const auto& entry: std::filesystem::recursive_directory_iterator( objects_root ) )
  {
    if( entry.is_directory() )
      directories.push_back( entry.path() );
  }

  std::sort( directories.rbegin(), directories.rend() );
  for( const auto& directory: directories )
  {
    std::error_code ec;
    std::filesystem::remove( directory, ec );
  }
}

void prune_local_snapshot_repository( const std::filesystem::path& repository_dir,
                                      uint64_t retention_count )
{
  if( retention_count == 0 )
    return;

  auto snapshots = completed_snapshot_dirs( repository_dir );
  if( snapshots.size() <= retention_count )
    return;

  const auto remove_count = snapshots.size() - static_cast< std::size_t >( retention_count );
  for( std::size_t i = 0; i < remove_count; ++i )
  {
    std::error_code ec;
    std::filesystem::remove_all( snapshots[ i ], ec );
  }

  snapshots = completed_snapshot_dirs( repository_dir );
  std::set< std::string > referenced_hashes;
  for( const auto& snapshot: snapshots )
    collect_snapshot_object_hashes( snapshot, referenced_hashes );

  const auto objects_root = repository_dir / "objects" / "sha256";
  if( std::filesystem::exists( objects_root ) )
  {
    for( const auto& entry: std::filesystem::recursive_directory_iterator( objects_root ) )
    {
      if( !entry.is_regular_file() )
        continue;
      const auto object_hash = entry.path().filename().string();
      if( object_hash.size() == 64 && referenced_hashes.find( object_hash ) == referenced_hashes.end() )
      {
        std::error_code ec;
        std::filesystem::remove( entry.path(), ec );
      }
    }
  }
  prune_empty_object_directories( objects_root );
}

void validate_restore_relative_path( const std::string& relative_path )
{
  if( relative_path.empty() )
    throw std::runtime_error( "restore manifest contains an empty file path" );
  std::filesystem::path path( relative_path );
  if( path.is_absolute() )
    throw std::runtime_error( "restore manifest contains an absolute file path: " + relative_path );
  for( const auto& part: path )
  {
    const auto value = part.string();
    if( value == "." || value == ".." )
      throw std::runtime_error( "restore manifest contains an unsafe file path: " + relative_path );
  }
}

void copy_or_link_object( const std::filesystem::path& source, const std::filesystem::path& destination )
{
  std::filesystem::create_directories( destination.parent_path() );
  if( std::filesystem::exists( destination ) )
  {
    const auto existing_hash = sha256_file( destination );
    if( existing_hash == destination.filename().string() )
      return;

    std::error_code remove_ec;
    std::filesystem::remove( destination, remove_ec );
    if( remove_ec )
      throw std::runtime_error( "failed to replace corrupt backup object " + destination.string()
                                + ": " + remove_ec.message() );
  }

  const auto tmp = destination.parent_path() / ( destination.filename().string() + ".tmp" );
  std::error_code ec;
  std::filesystem::remove( tmp, ec );
  ec.clear();
  std::filesystem::copy_file( source, tmp, std::filesystem::copy_options::overwrite_existing, ec );
  if( ec )
    throw std::runtime_error( "failed to copy backup object " + source.string()
                              + " to " + tmp.string() + ": " + ec.message() );

  const auto copied_hash = sha256_file( tmp );
  if( copied_hash != destination.filename().string() )
  {
    std::filesystem::remove( tmp, ec );
    throw std::runtime_error( "backup object checksum changed while copying " + source.string() );
  }

  std::filesystem::rename( tmp, destination );
}

std::string inventory_json( const std::string& backup_id,
                            const std::vector< SnapshotFileEntry >& entries )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"format\": \"teleno-native-snapshot-files\",\n";
  out << "  \"version\": 1,\n";
  out << "  \"backup_id\": \"" << json_escape( backup_id ) << "\",\n";
  out << "  \"files\": [\n";
  for( std::size_t i = 0; i < entries.size(); ++i )
  {
    const auto& entry = entries[ i ];
    out << "    { \"path\": \"" << json_escape( entry.path )
        << "\", \"sha256\": \"" << entry.sha256
        << "\", \"size_bytes\": " << entry.size_bytes
        << ", \"runtime_file\": " << json_bool( entry.runtime_file ) << " }";
    if( i + 1 != entries.size() )
      out << ",";
    out << "\n";
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

std::string manifest_json( const std::string& backup_id,
                           const NodeConfig& cfg,
                           const std::filesystem::path& basedir,
                           const std::filesystem::path& repository_dir,
                           const RestoreSpaceEstimate& restore_space,
                           uint64_t file_count,
                           uint64_t object_count,
                           uint64_t total_bytes )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"format\": \"teleno-native-rocksdb-snapshot\",\n";
  out << "  \"version\": 1,\n";
  out << "  \"backup_id\": \"" << json_escape( backup_id ) << "\",\n";
  out << "  \"created_at\": \"" << utc_timestamp() << "\",\n";
  out << "  \"node\": {\n";
  out << "    \"name\": \"" << node_name() << "\",\n";
  out << "    \"version\": \"" << json_escape( build_version() ) << "\"\n";
  out << "  },\n";
  out << "  \"source\": {\n";
  out << "    \"basedir\": \"" << json_escape( basedir.string() ) << "\",\n";
  out << "    \"node_id\": \"" << json_escape( cfg.backup.node_id ) << "\",\n";
  out << "    \"storage_layout\": \"unified\"\n";
  out << "  },\n";
  out << "  \"repository\": {\n";
  out << "    \"type\": \"local-object-store\",\n";
  out << "    \"path\": \"" << json_escape( repository_dir.string() ) << "\"\n";
  out << "  },\n";
  out << "  \"snapshot\": {\n";
  out << "    \"file_count\": " << file_count << ",\n";
  out << "    \"object_count\": " << object_count << ",\n";
  out << "    \"total_bytes\": " << total_bytes << "\n";
  out << "  },\n";
  out << "  \"sizes\": {\n";
  out << "    \"restored_database_bytes\": " << restore_space.restored_database_bytes << ",\n";
  out << "    \"runtime_files_bytes\": " << restore_space.runtime_files_bytes << ",\n";
  out << "    \"object_download_bytes\": " << restore_space.object_download_bytes << ",\n";
  out << "    \"archive_bytes\": " << restore_space.archive_bytes << ",\n";
  out << "    \"minimum_target_free_bytes\": " << restore_space.minimum_target_free_bytes << ",\n";
  out << "    \"recommended_target_free_bytes\": " << restore_space.recommended_target_free_bytes << "\n";
  out << "  },\n";
  out << "  \"restore\": {\n";
  out << "    \"requires_node_stop\": true,\n";
  out << "    \"start_as_observer_first\": true,\n";
  out << "    \"force_block_producer_disabled_on_first_start\": true\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

std::string latest_json( const LocalSnapshotResult& result )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"format\": \"teleno-native-latest-snapshot\",\n";
  out << "  \"version\": 1,\n";
  out << "  \"backup_id\": \"" << json_escape( result.backup_id ) << "\",\n";
  out << "  \"snapshot_dir\": \"" << json_escape( result.snapshot_dir.filename().string() ) << "\",\n";
  out << "  \"manifest\": \"snapshots/" << json_escape( result.snapshot_dir.filename().string() ) << "/manifest.json\",\n";
  out << "  \"files\": \"snapshots/" << json_escape( result.snapshot_dir.filename().string() ) << "/files.json\"\n";
  out << "}\n";
  return out.str();
}

std::string read_latest_backup_id( const std::filesystem::path& repository_dir )
{
  const auto latest_path = repository_dir / "latest.json";
  if( !std::filesystem::is_regular_file( latest_path ) )
    return {};

  const auto latest = nlohmann::json::parse( read_text_file( latest_path ) );
  return latest.value( "backup_id", "" );
}

void validate_backup_id_path_component( const std::string& backup_id )
{
  if( backup_id.empty() )
    throw std::runtime_error( "backup ID cannot be empty" );
  std::filesystem::path id_path( backup_id );
  if( id_path.is_absolute() || id_path.filename().string() != backup_id )
    throw std::runtime_error( "backup ID must be a snapshot directory name, not a path: " + backup_id );
  if( backup_id == "." || backup_id == ".."
      || ( backup_id.size() >= 8 && backup_id.substr( backup_id.size() - 8 ) == ".partial" ) )
    throw std::runtime_error( "invalid backup ID: " + backup_id );
}

std::filesystem::path resolve_snapshot_dir( const std::filesystem::path& repository_dir,
                                            const std::string& backup_id )
{
  validate_backup_id_path_component( backup_id );
  const auto snapshot_dir = repository_dir / "snapshots" / backup_id;
  if( !std::filesystem::is_directory( snapshot_dir ) )
    throw std::runtime_error( "backup snapshot not found: " + backup_id );
  return snapshot_dir;
}

BackupSnapshotSummary read_snapshot_summary( const std::filesystem::path& repository_dir,
                                             const std::filesystem::path& snapshot_dir,
                                             const std::string& latest_backup_id )
{
  const auto manifest_path = snapshot_dir / "manifest.json";
  const auto files_path = snapshot_dir / "files.json";
  const auto manifest = nlohmann::json::parse( read_text_file( manifest_path ) );
  const auto files = nlohmann::json::parse( read_text_file( files_path ) );
  const auto sizes = manifest.value( "sizes", nlohmann::json::object() );
  const auto snapshot = manifest.value( "snapshot", nlohmann::json::object() );
  const auto source = manifest.value( "source", nlohmann::json::object() );
  const auto node = manifest.value( "node", nlohmann::json::object() );

  uint64_t inventory_file_count = 0;
  for( const auto& _: files.at( "files" ) )
  {
    (void)_;
    ++inventory_file_count;
  }

  BackupSnapshotSummary summary;
  summary.repository_dir = repository_dir;
  summary.snapshot_dir = snapshot_dir;
  summary.manifest_path = manifest_path;
  summary.files_path = files_path;
  summary.backup_id = manifest.value( "backup_id", snapshot_dir.filename().string() );
  summary.created_at = manifest.value( "created_at", "" );
  summary.node_version = node.value( "version", "" );
  summary.node_id = source.value( "node_id", "" );
  summary.storage_layout = source.value( "storage_layout", "" );
  summary.file_count = snapshot.value( "file_count", inventory_file_count );
  summary.object_count = snapshot.value( "object_count", 0ULL );
  summary.total_bytes = snapshot.value( "total_bytes", 0ULL );
  summary.restore_space = estimate_restore_space(
    sizes.value( "restored_database_bytes", 0ULL ),
    sizes.value( "runtime_files_bytes", 0ULL ),
    sizes.value( "object_download_bytes", 0ULL ),
    sizes.value( "archive_bytes", 0ULL ),
    false,
    0 );
  summary.snapshot_complete = std::filesystem::exists( snapshot_dir / "COMPLETE" );
  summary.latest = !latest_backup_id.empty() && summary.backup_id == latest_backup_id;
  return summary;
}

void write_file_atomic( const std::filesystem::path& path, const std::string& content )
{
  std::filesystem::create_directories( path.parent_path() );
  const auto tmp = path.parent_path() / ( path.filename().string() + ".tmp" );
  {
    std::ofstream out( tmp, std::ios::binary | std::ios::trunc );
    if( !out )
      throw std::runtime_error( "failed to write temporary file: " + tmp.string() );
    out << content;
  }
  std::filesystem::rename( tmp, path );
}

void write_complete_marker( const std::filesystem::path& snapshot_dir )
{
  write_file_atomic( snapshot_dir / "COMPLETE", "complete\n" );
}

std::string safe_path_fragment( const std::string& value )
{
  std::string out;
  out.reserve( value.size() );
  for( char ch: value )
  {
    if( ( ch >= 'a' && ch <= 'z' )
        || ( ch >= 'A' && ch <= 'Z' )
        || ( ch >= '0' && ch <= '9' )
        || ch == '-' || ch == '_' || ch == '.' )
      out.push_back( ch );
    else
      out.push_back( '-' );
  }
  return out.empty() ? "restore" : out;
}

bool rocksdb_lock_is_held( const std::filesystem::path& db_dir )
{
#if defined( _WIN32 )
  (void)db_dir;
  return false;
#else
  const auto lock_path = db_dir / "LOCK";
  if( !std::filesystem::exists( lock_path ) )
    return false;

  const int fd = ::open( lock_path.c_str(), O_RDWR );
  if( fd < 0 )
    return false;

  struct flock lock{};
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;

  const auto rc = ::fcntl( fd, F_SETLK, &lock );
  const auto saved_errno = errno;
  if( rc == 0 )
  {
    lock.l_type = F_UNLCK;
    (void)::fcntl( fd, F_SETLK, &lock );
  }
  (void)::close( fd );

  return rc != 0 && ( saved_errno == EACCES || saved_errno == EAGAIN );
#endif
}

void preserve_existing_path( const std::filesystem::path& target_basedir,
                             const std::filesystem::path& pre_restore_dir,
                             const std::string& relative_path,
                             std::vector< RestoreActivatedPath >& preserved_paths )
{
  validate_restore_relative_path( relative_path );
  const auto source = target_basedir / std::filesystem::path( relative_path );
  if( !std::filesystem::exists( source ) )
    return;

  const auto preserved = pre_restore_dir / std::filesystem::path( relative_path );
  std::filesystem::create_directories( preserved.parent_path() );
  std::filesystem::rename( source, preserved );

  RestoreActivatedPath moved;
  moved.relative_path = relative_path;
  moved.preserved_path = preserved;
  preserved_paths.push_back( std::move( moved ) );
}

void copy_staged_runtime_file( const std::filesystem::path& staging_dir,
                               const std::filesystem::path& target_basedir,
                               const std::string& relative_path )
{
  validate_restore_relative_path( relative_path );
  const auto source = staging_dir / std::filesystem::path( relative_path );
  if( !std::filesystem::is_regular_file( source ) )
    return;

  const auto destination = target_basedir / std::filesystem::path( relative_path );
  std::filesystem::create_directories( destination.parent_path() );
  std::filesystem::copy_file( source, destination, std::filesystem::copy_options::overwrite_existing );
}

void move_path_with_copy_fallback( const std::filesystem::path& source,
                                   const std::filesystem::path& destination )
{
  std::filesystem::create_directories( destination.parent_path() );

  std::error_code ec;
  std::filesystem::rename( source, destination, ec );
  if( !ec )
    return;

  ec.clear();
  if( std::filesystem::is_directory( source ) )
  {
    std::filesystem::create_directories( destination, ec );
    if( ec )
      throw std::runtime_error( "failed to create restore destination " + destination.string()
                                + ": " + ec.message() );
    std::filesystem::copy( source,
                           destination,
                           std::filesystem::copy_options::recursive
                             | std::filesystem::copy_options::overwrite_existing,
                           ec );
  }
  else
  {
    std::filesystem::copy_file( source, destination, std::filesystem::copy_options::overwrite_existing, ec );
  }
  if( ec )
    throw std::runtime_error( "failed to move restore path " + source.string()
                              + " to " + destination.string() + ": " + ec.message() );

  std::filesystem::remove_all( source, ec );
  if( ec )
    throw std::runtime_error( "failed to remove restore source after copy fallback "
                              + source.string() + ": " + ec.message() );
}

} // anonymous namespace

RestoreSpaceEstimate estimate_restore_space( uint64_t restored_database_bytes,
                                             uint64_t runtime_files_bytes,
                                             uint64_t object_download_bytes,
                                             uint64_t archive_bytes,
                                             bool streaming_archive,
                                             uint64_t existing_target_bytes )
{
  RestoreSpaceEstimate estimate;
  estimate.restored_database_bytes = restored_database_bytes;
  estimate.runtime_files_bytes = runtime_files_bytes;
  estimate.object_download_bytes = object_download_bytes;
  estimate.archive_bytes = archive_bytes;
  estimate.streaming_archive = streaming_archive;
  estimate.existing_target_bytes = existing_target_bytes;

  const auto archive_requirement = streaming_archive ? 0 : archive_bytes;
  estimate.minimum_target_free_bytes = restored_database_bytes
                                     + runtime_files_bytes
                                     + archive_requirement
                                     + existing_target_bytes
                                     + metadata_overhead_bytes;

  const auto recommended_margin = std::max( recommended_min_margin_bytes, restored_database_bytes / 5 );
  estimate.recommended_target_free_bytes = estimate.minimum_target_free_bytes + recommended_margin;
  return estimate;
}

RestoreSpaceCheck check_restore_space( const RestoreSpaceEstimate& estimate,
                                       uint64_t available_bytes,
                                       std::string target_path )
{
  RestoreSpaceCheck check;
  check.available_bytes = available_bytes;
  check.target_path = std::move( target_path );
  check.passes_minimum = available_bytes >= estimate.minimum_target_free_bytes;
  check.below_recommended = check.passes_minimum && available_bytes < estimate.recommended_target_free_bytes;

  std::ostringstream message;
  if( !check.passes_minimum )
  {
    message << "Backup restore requires at least " << estimate.minimum_target_free_bytes
            << " bytes free at " << check.target_path
            << "; available bytes: " << available_bytes;
  }
  else if( check.below_recommended )
  {
    message << "Backup restore has enough minimum space at " << check.target_path
            << " but is below recommended free bytes " << estimate.recommended_target_free_bytes
            << "; available bytes: " << available_bytes;
  }
  else
  {
    message << "Backup restore disk-space preflight passed for " << check.target_path
            << "; available bytes: " << available_bytes;
  }
  check.message = message.str();
  return check;
}

RestorePreflightResult build_local_restore_preflight( const std::filesystem::path& repository_dir,
                                                      const std::filesystem::path& target_basedir )
{
  return build_local_restore_preflight( repository_dir, target_basedir, {} );
}

RestorePreflightResult build_local_restore_preflight( const std::filesystem::path& repository_dir,
                                                      const std::filesystem::path& target_basedir,
                                                      const std::string& requested_backup_id )
{
  if( repository_dir.empty() )
    throw std::runtime_error( "local backup repository directory is required for restore preflight" );
  if( target_basedir.empty() )
    throw std::runtime_error( "target basedir is required for restore preflight" );

  std::string backup_id = requested_backup_id;
  std::filesystem::path snapshot_dir;
  std::filesystem::path manifest_path;
  std::filesystem::path files_path;
  if( backup_id.empty() )
  {
    const auto latest_path = repository_dir / "latest.json";
    const auto latest = nlohmann::json::parse( read_text_file( latest_path ) );
    backup_id = latest.at( "backup_id" ).get< std::string >();
    const auto snapshot_rel = latest.value( "snapshot_dir", backup_id );
    snapshot_dir = repository_dir / "snapshots" / snapshot_rel;
    manifest_path = repository_dir / latest.value( "manifest", "snapshots/" + snapshot_rel + "/manifest.json" );
    files_path = repository_dir / latest.value( "files", "snapshots/" + snapshot_rel + "/files.json" );
  }
  else
  {
    snapshot_dir = resolve_snapshot_dir( repository_dir, backup_id );
    manifest_path = snapshot_dir / "manifest.json";
    files_path = snapshot_dir / "files.json";
  }

  const auto manifest = nlohmann::json::parse( read_text_file( manifest_path ) );
  const auto files = nlohmann::json::parse( read_text_file( files_path ) );
  const auto sizes = manifest.at( "sizes" );

  uint64_t missing_object_count = 0;
  uint64_t missing_object_bytes = 0;
  uint64_t file_count = 0;
  for( const auto& file: files.at( "files" ) )
  {
    ++file_count;
    const auto sha256 = file.at( "sha256" ).get< std::string >();
    const auto size = file.value( "size_bytes", 0ULL );
    if( !std::filesystem::exists( object_path( repository_dir, sha256 ) ) )
    {
      ++missing_object_count;
      missing_object_bytes += size;
    }
  }

  const auto existing_bytes = existing_restore_target_bytes( target_basedir );
  auto estimate = estimate_restore_space(
    sizes.value( "restored_database_bytes", 0ULL ),
    sizes.value( "runtime_files_bytes", 0ULL ),
    sizes.value( "object_download_bytes", 0ULL ),
    sizes.value( "archive_bytes", 0ULL ),
    false,
    existing_bytes );
  const auto available = available_space_bytes( target_basedir );
  auto space_check = check_restore_space( estimate, available, target_basedir.string() );

  RestorePreflightResult result;
  result.backup_id = backup_id;
  result.repository_dir = repository_dir;
  result.snapshot_dir = snapshot_dir;
  result.manifest_path = manifest_path;
  result.files_path = files_path;
  result.target_basedir = target_basedir;
  result.file_count = file_count;
  result.missing_object_count = missing_object_count;
  result.missing_object_bytes = missing_object_bytes;
  result.snapshot_complete = std::filesystem::exists( snapshot_dir / "COMPLETE" );
  result.start_as_observer_first = manifest.value( "restore", nlohmann::json::object() )
                                     .value( "start_as_observer_first", true );
  result.restore_space = estimate;
  result.space_check = std::move( space_check );
  result.ready_to_restore = result.snapshot_complete
                          && result.missing_object_count == 0
                          && result.space_check.passes_minimum;
  return result;
}

RestoreStageResult stage_local_restore_snapshot( const std::filesystem::path& repository_dir,
                                                 const std::filesystem::path& target_basedir,
                                                 const std::filesystem::path& requested_staging_dir )
{
  return stage_local_restore_snapshot( repository_dir, target_basedir, {}, requested_staging_dir );
}

RestoreStageResult stage_local_restore_snapshot( const std::filesystem::path& repository_dir,
                                                 const std::filesystem::path& target_basedir,
                                                 const std::string& backup_id,
                                                 const std::filesystem::path& requested_staging_dir )
{
  auto preflight = build_local_restore_preflight( repository_dir, target_basedir, backup_id );
  if( !preflight.ready_to_restore )
    throw std::runtime_error( "backup restore preflight did not pass: " + preflight.space_check.message );

  const auto staging_dir = requested_staging_dir.empty()
    ? target_basedir / ".teleno-restore-staging" / preflight.backup_id
    : requested_staging_dir;
  const auto partial_dir = staging_dir.parent_path() / ( staging_dir.filename().string() + ".partial" );
  const auto metadata_path = staging_dir / ".teleno-restore-stage.json";

  std::error_code ec;
  if( std::filesystem::exists( staging_dir, ec ) )
  {
    ec.clear();
    if( !std::filesystem::is_directory( staging_dir, ec ) || !std::filesystem::is_empty( staging_dir, ec ) )
      throw std::runtime_error( "restore staging directory must be empty or missing: " + staging_dir.string() );
  }
  if( std::filesystem::exists( partial_dir, ec ) )
    throw std::runtime_error( "restore partial staging directory already exists: " + partial_dir.string() );

  std::filesystem::create_directories( partial_dir );

  RestoreStageResult result;
  result.preflight = std::move( preflight );
  result.staging_dir = staging_dir;
  result.metadata_path = metadata_path;

  try
  {
    const auto files = nlohmann::json::parse( read_text_file( result.preflight.files_path ) );
    for( const auto& file: files.at( "files" ) )
    {
      const auto relative_path = file.at( "path" ).get< std::string >();
      validate_restore_relative_path( relative_path );

      const auto sha256 = file.at( "sha256" ).get< std::string >();
      const auto size = file.value( "size_bytes", 0ULL );
      const auto source = object_path( repository_dir, sha256 );
      const auto optional_restored_config = relative_path == "config.yml";
      if( !std::filesystem::is_regular_file( source ) )
      {
        if( optional_restored_config )
        {
          result.skipped_optional_runtime_files.push_back( relative_path );
          continue;
        }
        throw std::runtime_error( "restore object is missing: " + source.string() );
      }

      const auto destination = partial_dir / std::filesystem::path( relative_path );
      std::filesystem::create_directories( destination.parent_path() );
      std::filesystem::copy_file( source, destination, std::filesystem::copy_options::overwrite_existing );

      const auto restored_sha256 = sha256_file( destination );
      if( restored_sha256 != sha256 )
      {
        if( optional_restored_config )
        {
          std::filesystem::remove( destination );
          result.skipped_optional_runtime_files.push_back( relative_path );
          continue;
        }
        throw std::runtime_error( "restore checksum mismatch for " + relative_path );
      }

      ++result.restored_file_count;
      result.restored_bytes += size;
    }

    std::ostringstream metadata;
    metadata << "{\n";
    metadata << "  \"format\": \"teleno-native-restore-stage\",\n";
    metadata << "  \"version\": 1,\n";
    metadata << "  \"backup_id\": \"" << json_escape( result.preflight.backup_id ) << "\",\n";
    metadata << "  \"repository_dir\": \"" << json_escape( repository_dir.string() ) << "\",\n";
    metadata << "  \"target_basedir\": \"" << json_escape( target_basedir.string() ) << "\",\n";
    metadata << "  \"staging_dir\": \"" << json_escape( staging_dir.string() ) << "\",\n";
    metadata << "  \"restored_file_count\": " << result.restored_file_count << ",\n";
    metadata << "  \"restored_bytes\": " << result.restored_bytes << ",\n";
    metadata << "  \"start_as_observer_first\": " << json_bool( result.preflight.start_as_observer_first ) << ",\n";
    metadata << "  \"skipped_optional_runtime_files\": [";
    for( std::size_t i = 0; i < result.skipped_optional_runtime_files.size(); ++i )
    {
      if( i > 0 )
        metadata << ", ";
      metadata << "\"" << json_escape( result.skipped_optional_runtime_files[ i ] ) << "\"";
    }
    metadata << "]\n";
    metadata << "}\n";
    write_file_atomic( partial_dir / ".teleno-restore-stage.json", metadata.str() );
    write_file_atomic( partial_dir / "RESTORE_STAGE_COMPLETE", "complete\n" );
    std::filesystem::rename( partial_dir, staging_dir );
    return result;
  }
  catch( ... )
  {
    std::error_code cleanup_ec;
    std::filesystem::remove_all( partial_dir, cleanup_ec );
    throw;
  }
}

BackupSnapshotListResult list_local_backup_snapshots( const std::filesystem::path& repository_dir )
{
  if( repository_dir.empty() )
    throw std::runtime_error( "local backup repository directory is required for backup listing" );

  BackupSnapshotListResult result;
  result.repository_dir = repository_dir;
  result.latest_backup_id = read_latest_backup_id( repository_dir );

  auto snapshots = completed_snapshot_dirs( repository_dir );
  for( const auto& snapshot_dir: snapshots )
    result.snapshots.push_back( read_snapshot_summary( repository_dir, snapshot_dir, result.latest_backup_id ) );

  std::sort( result.snapshots.begin(), result.snapshots.end(), []( const auto& lhs, const auto& rhs ) {
    return lhs.backup_id < rhs.backup_id;
  } );
  return result;
}

BackupDeleteResult delete_local_backup_snapshot( const std::filesystem::path& repository_dir,
                                                 const std::string& backup_id,
                                                 bool dry_run )
{
  if( repository_dir.empty() )
    throw std::runtime_error( "local backup repository directory is required for backup deletion" );
  if( backup_id == "latest" )
    throw std::runtime_error( "backup deletion requires an exact backup ID; 'latest' is not accepted" );
  validate_backup_id_path_component( backup_id );

  const auto snapshot_dir = resolve_snapshot_dir( repository_dir, backup_id );
  if( !std::filesystem::exists( snapshot_dir / "COMPLETE" ) )
    throw std::runtime_error( "backup snapshot is not complete: " + backup_id );

  BackupDeleteResult result;
  result.source = "local";
  result.backup_id = backup_id;
  result.repository_dir = repository_dir;
  result.dry_run = dry_run;
  result.snapshot_found = true;
  result.previous_latest_backup_id = read_latest_backup_id( repository_dir );
  result.deleted_latest = result.previous_latest_backup_id == backup_id;
  result.new_latest_backup_id = result.deleted_latest
    ? newest_completed_backup_id( repository_dir, backup_id )
    : result.previous_latest_backup_id;

  const auto metadata_stats = directory_file_stats( snapshot_dir );
  result.snapshot_metadata_file_count = metadata_stats.first;
  result.snapshot_metadata_bytes = metadata_stats.second;

  const auto deleted_snapshot_objects = read_snapshot_object_sizes( snapshot_dir );
  std::set< std::string > remaining_object_hashes;
  for( const auto& remaining_snapshot_dir: completed_snapshot_dirs( repository_dir ) )
  {
    if( remaining_snapshot_dir.filename().string() == backup_id )
      continue;
    collect_snapshot_object_hashes( remaining_snapshot_dir, remaining_object_hashes );
  }

  for( const auto& [sha256, size_bytes]: deleted_snapshot_objects )
  {
    if( remaining_object_hashes.find( sha256 ) != remaining_object_hashes.end() )
      continue;
    const auto path = object_path( repository_dir, sha256 );
    if( !std::filesystem::is_regular_file( path ) )
      continue;
    ++result.reclaimable_object_count;
    std::error_code ec;
    const auto actual_size = std::filesystem::file_size( path, ec );
    result.reclaimable_object_bytes += ec ? size_bytes : actual_size;
  }

  if( dry_run )
    return result;

  std::error_code ec;
  std::filesystem::remove_all( snapshot_dir, ec );
  if( ec )
    throw std::runtime_error( "failed to remove local backup snapshot " + snapshot_dir.string()
                              + ": " + ec.message() );
  result.deleted_snapshot = true;

  for( const auto& [sha256, size_bytes]: deleted_snapshot_objects )
  {
    if( remaining_object_hashes.find( sha256 ) != remaining_object_hashes.end() )
      continue;
    const auto path = object_path( repository_dir, sha256 );
    if( !std::filesystem::is_regular_file( path ) )
      continue;
    uint64_t removed_size = size_bytes;
    std::error_code size_ec;
    const auto actual_size = std::filesystem::file_size( path, size_ec );
    if( !size_ec )
      removed_size = actual_size;

    std::error_code remove_ec;
    if( std::filesystem::remove( path, remove_ec ) )
    {
      ++result.deleted_object_count;
      result.deleted_object_bytes += removed_size;
    }
    else if( remove_ec )
    {
      throw std::runtime_error( "failed to remove unreferenced backup object " + path.string()
                                + ": " + remove_ec.message() );
    }
  }
  prune_empty_object_directories( repository_dir / "objects" / "sha256" );

  const auto latest_path = repository_dir / "latest.json";
  if( result.deleted_latest )
  {
    if( result.new_latest_backup_id.empty() )
    {
      std::error_code remove_latest_ec;
      std::filesystem::remove( latest_path, remove_latest_ec );
      if( remove_latest_ec )
        throw std::runtime_error( "failed to remove local latest.json: " + remove_latest_ec.message() );
    }
    else
    {
      write_file_atomic( latest_path, latest_json_for_backup_id( result.new_latest_backup_id ) );
    }
  }

  return result;
}

RestoreActivationResult activate_staged_restore_snapshot( const std::filesystem::path& staging_dir,
                                                          const std::filesystem::path& target_basedir )
{
  if( staging_dir.empty() )
    throw std::runtime_error( "restore staging directory is required for activation" );
  if( target_basedir.empty() )
    throw std::runtime_error( "target basedir is required for restore activation" );
  if( !std::filesystem::exists( staging_dir / "RESTORE_STAGE_COMPLETE" ) )
    throw std::runtime_error( "restore staging directory is not complete: " + staging_dir.string() );
  if( !std::filesystem::is_directory( staging_dir / "db" ) )
    throw std::runtime_error( "restore staging directory does not contain a db directory: " + staging_dir.string() );

  const auto stage_metadata_path = staging_dir / ".teleno-restore-stage.json";
  const auto stage_metadata = nlohmann::json::parse( read_text_file( stage_metadata_path ) );
  const auto backup_id = stage_metadata.at( "backup_id" ).get< std::string >();

  const auto target_db = target_basedir / "db";
  if( rocksdb_lock_is_held( target_db ) )
    throw std::runtime_error( "target RocksDB appears to be locked by a running node: " + target_db.string() );

  const auto pre_restore_dir = target_basedir
                               / ".pre-restore"
                               / ( utc_timestamp() + "-" + safe_path_fragment( backup_id ) );
  if( std::filesystem::exists( pre_restore_dir ) )
    throw std::runtime_error( "restore preservation directory already exists: " + pre_restore_dir.string() );

  std::filesystem::create_directories( target_basedir );
  std::filesystem::create_directories( pre_restore_dir );

  RestoreActivationResult result;
  result.backup_id = backup_id;
  result.target_basedir = target_basedir;
  result.staging_dir = staging_dir;
  result.pre_restore_dir = pre_restore_dir;
  result.marker_path = target_basedir / ".backup-just-restored";
  result.restore_manifest_path = target_basedir / ".teleno-restore-manifest.json";
  result.start_as_observer_first = stage_metadata.value( "start_as_observer_first", true );

  preserve_existing_path( target_basedir, pre_restore_dir, "db", result.preserved_paths );
  preserve_existing_path( target_basedir, pre_restore_dir, "chain/blockchain", result.preserved_paths );
  preserve_existing_path( target_basedir, pre_restore_dir, "chain/genesis_data.json", result.preserved_paths );
  preserve_existing_path( target_basedir,
                          pre_restore_dir,
                          "jsonrpc/descriptors/koinos_descriptors.pb",
                          result.preserved_paths );
  preserve_existing_path( target_basedir, pre_restore_dir, ".teleno-restored-config.yml", result.preserved_paths );

  move_path_with_copy_fallback( staging_dir / "db", target_db );
  copy_staged_runtime_file( staging_dir, target_basedir, "chain/genesis_data.json" );
  copy_staged_runtime_file( staging_dir,
                            target_basedir,
                            "jsonrpc/descriptors/koinos_descriptors.pb" );
  if( std::filesystem::is_regular_file( staging_dir / "config.yml" ) )
  {
    std::filesystem::copy_file( staging_dir / "config.yml",
                                target_basedir / ".teleno-restored-config.yml",
                                std::filesystem::copy_options::overwrite_existing );
  }

  std::ostringstream manifest;
  manifest << "{\n";
  manifest << "  \"format\": \"teleno-native-restore-activation\",\n";
  manifest << "  \"version\": 1,\n";
  manifest << "  \"backup_id\": \"" << json_escape( result.backup_id ) << "\",\n";
  manifest << "  \"target_basedir\": \"" << json_escape( target_basedir.string() ) << "\",\n";
  manifest << "  \"staging_dir\": \"" << json_escape( staging_dir.string() ) << "\",\n";
  manifest << "  \"pre_restore_dir\": \"" << json_escape( pre_restore_dir.string() ) << "\",\n";
  manifest << "  \"block_producer_disabled_on_first_start\": true,\n";
  manifest << "  \"start_as_observer_first\": " << json_bool( result.start_as_observer_first ) << ",\n";
  manifest << "  \"active_config_overwritten\": false,\n";
  manifest << "  \"restored_config_copy\": \"" << json_escape( ( target_basedir / ".teleno-restored-config.yml" ).string() ) << "\",\n";
  manifest << "  \"preserved_paths\": [\n";
  for( std::size_t i = 0; i < result.preserved_paths.size(); ++i )
  {
    const auto& preserved = result.preserved_paths[ i ];
    manifest << "    { \"relative_path\": \"" << json_escape( preserved.relative_path )
             << "\", \"preserved_path\": \"" << json_escape( preserved.preserved_path.string() )
             << "\" }";
    if( i + 1 != result.preserved_paths.size() )
      manifest << ",";
    manifest << "\n";
  }
  manifest << "  ]\n";
  manifest << "}\n";

  write_file_atomic( result.restore_manifest_path, manifest.str() );
  write_file_atomic( result.marker_path,
                     "backup_id=" + result.backup_id
                       + "\nblock_producer_disabled_on_first_start=true\n"
                       + "start_as_observer_first=true\n" );
  return result;
}

LocalSnapshotRepository::LocalSnapshotRepository( std::filesystem::path repository_dir )
  : _repository_dir( std::move( repository_dir ) )
{}

LocalSnapshotResult LocalSnapshotRepository::store_checkpoint_snapshot( const CheckpointResult& checkpoint,
                                                                        const NodeConfig& cfg,
                                                                        const std::filesystem::path& basedir,
                                                                        const std::filesystem::path& config_path )
{
  if( _repository_dir.empty() )
    throw std::runtime_error( "local backup repository directory is required" );

  const auto backup_id = make_backup_id( checkpoint );
  const auto snapshots_dir = _repository_dir / "snapshots";
  const auto snapshot_tmp = snapshots_dir / ( backup_id + ".partial" );
  const auto snapshot_dir = snapshots_dir / backup_id;
  if( std::filesystem::exists( snapshot_tmp ) || std::filesystem::exists( snapshot_dir ) )
    throw std::runtime_error( "snapshot already exists: " + backup_id );

  std::filesystem::create_directories( snapshot_tmp );

  try
  {
    auto entries = build_file_inventory( checkpoint, basedir, config_path );
    if( entries.empty() )
      throw std::runtime_error( "backup snapshot inventory is empty" );

    uint64_t total_bytes = 0;
    uint64_t db_bytes = 0;
    uint64_t runtime_bytes = 0;
    uint64_t unique_object_bytes = 0;
    std::map< std::string, uint64_t > unique_object_sizes;
    std::set< std::string > unique_objects;
    uint64_t new_objects = 0;
    uint64_t reused_objects = 0;

    for( const auto& entry: entries )
    {
      total_bytes += entry.size_bytes;
      if( entry.runtime_file )
        runtime_bytes += entry.size_bytes;
      else
        db_bytes += entry.size_bytes;

      if( !unique_objects.insert( entry.sha256 ).second )
        continue;
      unique_object_sizes[ entry.sha256 ] = entry.size_bytes;

      const auto destination = object_path( _repository_dir, entry.sha256 );
      if( std::filesystem::exists( destination ) )
      {
        ++reused_objects;
        continue;
      }

      const auto source = entry.runtime_file
        ? [&]() {
            if( entry.path == "config.yml" )
              return config_path;
            if( entry.path == "chain/genesis_data.json" && std::filesystem::exists( basedir / "chain" / "genesis_data.json" ) )
              return basedir / "chain" / "genesis_data.json";
            if( entry.path == "chain/genesis_data.json" )
              return basedir / "genesis_data.json";
            return basedir / entry.path;
          }()
        : checkpoint.checkpoint_dir / std::filesystem::path( entry.path );

      copy_or_link_object( source, destination );
      ++new_objects;
    }

    for( const auto& [_, size]: unique_object_sizes )
      unique_object_bytes += size;

    LocalSnapshotResult result;
    result.backup_id = backup_id;
    result.repository_dir = _repository_dir;
    result.snapshot_dir = snapshot_dir;
    result.manifest_path = snapshot_dir / "manifest.json";
    result.files_path = snapshot_dir / "files.json";
    result.latest_path = _repository_dir / "latest.json";
    result.file_count = static_cast< uint64_t >( entries.size() );
    result.object_count = static_cast< uint64_t >( unique_objects.size() );
    result.new_object_count = new_objects;
    result.reused_object_count = reused_objects;
    result.total_bytes = total_bytes;
    result.restore_space = estimate_restore_space( db_bytes, runtime_bytes, unique_object_bytes );

    write_file_atomic( snapshot_tmp / "files.json", inventory_json( backup_id, entries ) );
    write_file_atomic( snapshot_tmp / "manifest.json",
                       manifest_json( backup_id,
                                      cfg,
                                      basedir,
                                      _repository_dir,
                                      result.restore_space,
                                      result.file_count,
                                      result.object_count,
                                      result.total_bytes ) );
    write_complete_marker( snapshot_tmp );
    std::filesystem::rename( snapshot_tmp, snapshot_dir );
    write_file_atomic( result.latest_path, latest_json( result ) );
    prune_local_snapshot_repository( _repository_dir, cfg.backup.local.retention_count );

    return result;
  }
  catch( ... )
  {
    std::error_code ec;
    std::filesystem::remove_all( snapshot_tmp, ec );
    throw;
  }
}

std::string local_snapshot_result_to_text( const LocalSnapshotResult& result )
{
  std::ostringstream out;
  out << "Created local backup snapshot\n";
  out << "backup_id: " << result.backup_id << "\n";
  out << "repository_dir: " << result.repository_dir.string() << "\n";
  out << "snapshot_dir: " << result.snapshot_dir.string() << "\n";
  out << "manifest: " << result.manifest_path.string() << "\n";
  out << "files: " << result.files_path.string() << "\n";
  out << "latest: " << result.latest_path.string() << "\n";
  out << "file_count: " << result.file_count << "\n";
  out << "object_count: " << result.object_count << "\n";
  out << "new_object_count: " << result.new_object_count << "\n";
  out << "reused_object_count: " << result.reused_object_count << "\n";
  out << "total_bytes: " << result.total_bytes << "\n";
  out << "minimum_target_free_bytes: " << result.restore_space.minimum_target_free_bytes << "\n";
  out << "recommended_target_free_bytes: " << result.restore_space.recommended_target_free_bytes << "\n";
  return out.str();
}

std::string local_snapshot_result_to_json( const LocalSnapshotResult& result )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"backup_id\": \"" << json_escape( result.backup_id ) << "\",\n";
  out << "  \"repository_dir\": \"" << json_escape( result.repository_dir.string() ) << "\",\n";
  out << "  \"snapshot_dir\": \"" << json_escape( result.snapshot_dir.string() ) << "\",\n";
  out << "  \"manifest\": \"" << json_escape( result.manifest_path.string() ) << "\",\n";
  out << "  \"files\": \"" << json_escape( result.files_path.string() ) << "\",\n";
  out << "  \"latest\": \"" << json_escape( result.latest_path.string() ) << "\",\n";
  out << "  \"file_count\": " << result.file_count << ",\n";
  out << "  \"object_count\": " << result.object_count << ",\n";
  out << "  \"new_object_count\": " << result.new_object_count << ",\n";
  out << "  \"reused_object_count\": " << result.reused_object_count << ",\n";
  out << "  \"total_bytes\": " << result.total_bytes << ",\n";
  out << "  \"restore_space\": {\n";
  out << "    \"restored_database_bytes\": " << result.restore_space.restored_database_bytes << ",\n";
  out << "    \"runtime_files_bytes\": " << result.restore_space.runtime_files_bytes << ",\n";
  out << "    \"object_download_bytes\": " << result.restore_space.object_download_bytes << ",\n";
  out << "    \"minimum_target_free_bytes\": " << result.restore_space.minimum_target_free_bytes << ",\n";
  out << "    \"recommended_target_free_bytes\": " << result.restore_space.recommended_target_free_bytes << "\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

std::string backup_snapshot_list_result_to_text( const BackupSnapshotListResult& result )
{
  std::ostringstream out;
  out << "Native backup snapshots\n";
  out << "repository_dir: " << result.repository_dir.string() << "\n";
  out << "latest_backup_id: " << result.latest_backup_id << "\n";
  out << "snapshot_count: " << result.snapshots.size() << "\n";
  for( const auto& snapshot: result.snapshots )
  {
    out << "- backup_id: " << snapshot.backup_id << "\n";
    out << "  created_at: " << snapshot.created_at << "\n";
    out << "  latest: " << json_bool( snapshot.latest ) << "\n";
    out << "  complete: " << json_bool( snapshot.snapshot_complete ) << "\n";
    out << "  node_id: " << snapshot.node_id << "\n";
    out << "  node_version: " << snapshot.node_version << "\n";
    out << "  storage_layout: " << snapshot.storage_layout << "\n";
    out << "  file_count: " << snapshot.file_count << "\n";
    out << "  object_count: " << snapshot.object_count << "\n";
    out << "  total_bytes: " << snapshot.total_bytes << "\n";
    out << "  minimum_target_free_bytes: " << snapshot.restore_space.minimum_target_free_bytes << "\n";
    out << "  recommended_target_free_bytes: " << snapshot.restore_space.recommended_target_free_bytes << "\n";
  }
  return out.str();
}

std::string backup_snapshot_list_result_to_json( const BackupSnapshotListResult& result )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"repository_dir\": \"" << json_escape( result.repository_dir.string() ) << "\",\n";
  out << "  \"latest_backup_id\": \"" << json_escape( result.latest_backup_id ) << "\",\n";
  out << "  \"snapshot_count\": " << result.snapshots.size() << ",\n";
  out << "  \"snapshots\": [\n";
  for( std::size_t i = 0; i < result.snapshots.size(); ++i )
  {
    const auto& snapshot = result.snapshots[ i ];
    out << "    {\n";
    out << "      \"backup_id\": \"" << json_escape( snapshot.backup_id ) << "\",\n";
    out << "      \"created_at\": \"" << json_escape( snapshot.created_at ) << "\",\n";
    out << "      \"latest\": " << json_bool( snapshot.latest ) << ",\n";
    out << "      \"complete\": " << json_bool( snapshot.snapshot_complete ) << ",\n";
    out << "      \"node_id\": \"" << json_escape( snapshot.node_id ) << "\",\n";
    out << "      \"node_version\": \"" << json_escape( snapshot.node_version ) << "\",\n";
    out << "      \"storage_layout\": \"" << json_escape( snapshot.storage_layout ) << "\",\n";
    out << "      \"repository_dir\": \"" << json_escape( snapshot.repository_dir.string() ) << "\",\n";
    out << "      \"snapshot_dir\": \"" << json_escape( snapshot.snapshot_dir.string() ) << "\",\n";
    out << "      \"manifest\": \"" << json_escape( snapshot.manifest_path.string() ) << "\",\n";
    out << "      \"files\": \"" << json_escape( snapshot.files_path.string() ) << "\",\n";
    out << "      \"file_count\": " << snapshot.file_count << ",\n";
    out << "      \"object_count\": " << snapshot.object_count << ",\n";
    out << "      \"total_bytes\": " << snapshot.total_bytes << ",\n";
    out << "      \"restore_space\": {\n";
    out << "        \"restored_database_bytes\": " << snapshot.restore_space.restored_database_bytes << ",\n";
    out << "        \"runtime_files_bytes\": " << snapshot.restore_space.runtime_files_bytes << ",\n";
    out << "        \"object_download_bytes\": " << snapshot.restore_space.object_download_bytes << ",\n";
    out << "        \"minimum_target_free_bytes\": " << snapshot.restore_space.minimum_target_free_bytes << ",\n";
    out << "        \"recommended_target_free_bytes\": " << snapshot.restore_space.recommended_target_free_bytes << "\n";
    out << "      }\n";
    out << "    }";
    if( i + 1 != result.snapshots.size() )
      out << ",";
    out << "\n";
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

std::string backup_delete_result_to_text( const BackupDeleteResult& result )
{
  std::ostringstream out;
  out << "Native backup delete\n";
  out << "source: " << result.source << "\n";
  out << "backup_id: " << result.backup_id << "\n";
  out << "dry_run: " << json_bool( result.dry_run ) << "\n";
  out << "repository_dir: " << result.repository_dir.string() << "\n";
  if( !result.remote_directory.empty() )
    out << "remote_directory: " << result.remote_directory << "\n";
  if( !result.transport.empty() )
    out << "transport: " << result.transport << "\n";
  out << "snapshot_found: " << json_bool( result.snapshot_found ) << "\n";
  out << "deleted_snapshot: " << json_bool( result.deleted_snapshot ) << "\n";
  out << "deleted_latest: " << json_bool( result.deleted_latest ) << "\n";
  out << "previous_latest_backup_id: " << result.previous_latest_backup_id << "\n";
  out << "new_latest_backup_id: " << result.new_latest_backup_id << "\n";
  out << "snapshot_metadata_file_count: " << result.snapshot_metadata_file_count << "\n";
  out << "snapshot_metadata_bytes: " << result.snapshot_metadata_bytes << "\n";
  out << "reclaimable_object_count: " << result.reclaimable_object_count << "\n";
  out << "reclaimable_object_bytes: " << result.reclaimable_object_bytes << "\n";
  out << "deleted_object_count: " << result.deleted_object_count << "\n";
  out << "deleted_object_bytes: " << result.deleted_object_bytes << "\n";
  return out.str();
}

std::string backup_delete_result_to_json( const BackupDeleteResult& result )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"source\": \"" << json_escape( result.source ) << "\",\n";
  out << "  \"backup_id\": \"" << json_escape( result.backup_id ) << "\",\n";
  out << "  \"dry_run\": " << json_bool( result.dry_run ) << ",\n";
  out << "  \"repository_dir\": \"" << json_escape( result.repository_dir.string() ) << "\",\n";
  out << "  \"remote_directory\": \"" << json_escape( result.remote_directory ) << "\",\n";
  out << "  \"transport\": \"" << json_escape( result.transport ) << "\",\n";
  out << "  \"snapshot_found\": " << json_bool( result.snapshot_found ) << ",\n";
  out << "  \"deleted_snapshot\": " << json_bool( result.deleted_snapshot ) << ",\n";
  out << "  \"deleted_latest\": " << json_bool( result.deleted_latest ) << ",\n";
  out << "  \"previous_latest_backup_id\": \"" << json_escape( result.previous_latest_backup_id ) << "\",\n";
  out << "  \"new_latest_backup_id\": \"" << json_escape( result.new_latest_backup_id ) << "\",\n";
  out << "  \"snapshot_metadata_file_count\": " << result.snapshot_metadata_file_count << ",\n";
  out << "  \"snapshot_metadata_bytes\": " << result.snapshot_metadata_bytes << ",\n";
  out << "  \"reclaimable_object_count\": " << result.reclaimable_object_count << ",\n";
  out << "  \"reclaimable_object_bytes\": " << result.reclaimable_object_bytes << ",\n";
  out << "  \"deleted_object_count\": " << result.deleted_object_count << ",\n";
  out << "  \"deleted_object_bytes\": " << result.deleted_object_bytes << "\n";
  out << "}\n";
  return out.str();
}

std::string restore_preflight_result_to_text( const RestorePreflightResult& result )
{
  std::ostringstream out;
  out << "Backup restore preflight\n";
  out << "backup_id: " << result.backup_id << "\n";
  out << "repository_dir: " << result.repository_dir.string() << "\n";
  out << "snapshot_dir: " << result.snapshot_dir.string() << "\n";
  out << "manifest: " << result.manifest_path.string() << "\n";
  out << "files: " << result.files_path.string() << "\n";
  out << "target_basedir: " << result.target_basedir.string() << "\n";
  out << "snapshot_complete: " << json_bool( result.snapshot_complete ) << "\n";
  out << "file_count: " << result.file_count << "\n";
  out << "missing_object_count: " << result.missing_object_count << "\n";
  out << "missing_object_bytes: " << result.missing_object_bytes << "\n";
  out << "existing_target_bytes: " << result.restore_space.existing_target_bytes << "\n";
  out << "minimum_target_free_bytes: " << result.restore_space.minimum_target_free_bytes << "\n";
  out << "recommended_target_free_bytes: " << result.restore_space.recommended_target_free_bytes << "\n";
  out << "available_bytes: " << result.space_check.available_bytes << "\n";
  out << "space_check: " << result.space_check.message << "\n";
  out << "start_as_observer_first: " << json_bool( result.start_as_observer_first ) << "\n";
  out << "ready_to_restore: " << json_bool( result.ready_to_restore ) << "\n";
  return out.str();
}

std::string restore_preflight_result_to_json( const RestorePreflightResult& result )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"backup_id\": \"" << json_escape( result.backup_id ) << "\",\n";
  out << "  \"repository_dir\": \"" << json_escape( result.repository_dir.string() ) << "\",\n";
  out << "  \"snapshot_dir\": \"" << json_escape( result.snapshot_dir.string() ) << "\",\n";
  out << "  \"manifest\": \"" << json_escape( result.manifest_path.string() ) << "\",\n";
  out << "  \"files\": \"" << json_escape( result.files_path.string() ) << "\",\n";
  out << "  \"target_basedir\": \"" << json_escape( result.target_basedir.string() ) << "\",\n";
  out << "  \"snapshot_complete\": " << json_bool( result.snapshot_complete ) << ",\n";
  out << "  \"file_count\": " << result.file_count << ",\n";
  out << "  \"missing_object_count\": " << result.missing_object_count << ",\n";
  out << "  \"missing_object_bytes\": " << result.missing_object_bytes << ",\n";
  out << "  \"ready_to_restore\": " << json_bool( result.ready_to_restore ) << ",\n";
  out << "  \"start_as_observer_first\": " << json_bool( result.start_as_observer_first ) << ",\n";
  out << "  \"restore_space\": {\n";
  out << "    \"restored_database_bytes\": " << result.restore_space.restored_database_bytes << ",\n";
  out << "    \"runtime_files_bytes\": " << result.restore_space.runtime_files_bytes << ",\n";
  out << "    \"object_download_bytes\": " << result.restore_space.object_download_bytes << ",\n";
  out << "    \"existing_target_bytes\": " << result.restore_space.existing_target_bytes << ",\n";
  out << "    \"minimum_target_free_bytes\": " << result.restore_space.minimum_target_free_bytes << ",\n";
  out << "    \"recommended_target_free_bytes\": " << result.restore_space.recommended_target_free_bytes << "\n";
  out << "  },\n";
  out << "  \"space_check\": {\n";
  out << "    \"passes_minimum\": " << json_bool( result.space_check.passes_minimum ) << ",\n";
  out << "    \"below_recommended\": " << json_bool( result.space_check.below_recommended ) << ",\n";
  out << "    \"available_bytes\": " << result.space_check.available_bytes << ",\n";
  out << "    \"target_path\": \"" << json_escape( result.space_check.target_path ) << "\",\n";
  out << "    \"message\": \"" << json_escape( result.space_check.message ) << "\"\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

std::string restore_stage_result_to_text( const RestoreStageResult& result )
{
  std::ostringstream out;
  out << "Staged backup restore\n";
  out << "backup_id: " << result.preflight.backup_id << "\n";
  out << "repository_dir: " << result.preflight.repository_dir.string() << "\n";
  out << "target_basedir: " << result.preflight.target_basedir.string() << "\n";
  out << "staging_dir: " << result.staging_dir.string() << "\n";
  out << "metadata: " << result.metadata_path.string() << "\n";
  out << "restored_file_count: " << result.restored_file_count << "\n";
  out << "restored_bytes: " << result.restored_bytes << "\n";
  out << "skipped_optional_runtime_file_count: " << result.skipped_optional_runtime_files.size() << "\n";
  for( const auto& path: result.skipped_optional_runtime_files )
    out << "skipped_optional_runtime_file: " << path << "\n";
  out << "start_as_observer_first: " << json_bool( result.preflight.start_as_observer_first ) << "\n";
  return out.str();
}

std::string restore_stage_result_to_json( const RestoreStageResult& result )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"backup_id\": \"" << json_escape( result.preflight.backup_id ) << "\",\n";
  out << "  \"repository_dir\": \"" << json_escape( result.preflight.repository_dir.string() ) << "\",\n";
  out << "  \"target_basedir\": \"" << json_escape( result.preflight.target_basedir.string() ) << "\",\n";
  out << "  \"staging_dir\": \"" << json_escape( result.staging_dir.string() ) << "\",\n";
  out << "  \"metadata\": \"" << json_escape( result.metadata_path.string() ) << "\",\n";
  out << "  \"restored_file_count\": " << result.restored_file_count << ",\n";
  out << "  \"restored_bytes\": " << result.restored_bytes << ",\n";
  out << "  \"start_as_observer_first\": " << json_bool( result.preflight.start_as_observer_first ) << ",\n";
  out << "  \"skipped_optional_runtime_files\": [";
  for( std::size_t i = 0; i < result.skipped_optional_runtime_files.size(); ++i )
  {
    if( i > 0 )
      out << ", ";
    out << "\"" << json_escape( result.skipped_optional_runtime_files[ i ] ) << "\"";
  }
  out << "]\n";
  out << "}\n";
  return out.str();
}

std::string restore_activation_result_to_text( const RestoreActivationResult& result )
{
  std::ostringstream out;
  out << "Activated staged backup restore\n";
  out << "backup_id: " << result.backup_id << "\n";
  out << "target_basedir: " << result.target_basedir.string() << "\n";
  out << "staging_dir: " << result.staging_dir.string() << "\n";
  out << "pre_restore_dir: " << result.pre_restore_dir.string() << "\n";
  out << "marker: " << result.marker_path.string() << "\n";
  out << "restore_manifest: " << result.restore_manifest_path.string() << "\n";
  out << "preserved_path_count: " << result.preserved_paths.size() << "\n";
  out << "block_producer_disabled_on_first_start: "
      << json_bool( result.block_producer_disabled_on_first_start ) << "\n";
  out << "start_as_observer_first: " << json_bool( result.start_as_observer_first ) << "\n";
  return out.str();
}

std::string restore_activation_result_to_json( const RestoreActivationResult& result )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"backup_id\": \"" << json_escape( result.backup_id ) << "\",\n";
  out << "  \"target_basedir\": \"" << json_escape( result.target_basedir.string() ) << "\",\n";
  out << "  \"staging_dir\": \"" << json_escape( result.staging_dir.string() ) << "\",\n";
  out << "  \"pre_restore_dir\": \"" << json_escape( result.pre_restore_dir.string() ) << "\",\n";
  out << "  \"marker\": \"" << json_escape( result.marker_path.string() ) << "\",\n";
  out << "  \"restore_manifest\": \"" << json_escape( result.restore_manifest_path.string() ) << "\",\n";
  out << "  \"block_producer_disabled_on_first_start\": "
      << json_bool( result.block_producer_disabled_on_first_start ) << ",\n";
  out << "  \"start_as_observer_first\": " << json_bool( result.start_as_observer_first ) << ",\n";
  out << "  \"preserved_paths\": [\n";
  for( std::size_t i = 0; i < result.preserved_paths.size(); ++i )
  {
    const auto& preserved = result.preserved_paths[ i ];
    out << "    { \"relative_path\": \"" << json_escape( preserved.relative_path )
        << "\", \"preserved_path\": \"" << json_escape( preserved.preserved_path.string() )
        << "\" }";
    if( i + 1 != result.preserved_paths.size() )
      out << ",";
    out << "\n";
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

} // namespace koinos::node::backup
