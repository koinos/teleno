#include "backup/public_restore.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <openssl/sha.h>

using namespace koinos::node::backup;
using koinos::node::BackupPublicRestoreConfig;

namespace {

std::filesystem::path unique_temp_dir( const std::string& prefix )
{
  auto path = std::filesystem::temp_directory_path()
              / ( prefix + "-" + std::to_string( std::rand() ) );
  std::filesystem::remove_all( path );
  std::filesystem::create_directories( path );
  return path;
}

void write_file( const std::filesystem::path& path, const std::string& content )
{
  std::filesystem::create_directories( path.parent_path() );
  std::ofstream out( path, std::ios::binary | std::ios::trunc );
  out << content;
}

std::string bytes_to_hex( const unsigned char* data, unsigned int size )
{
  static const char* hex = "0123456789abcdef";
  std::string out;
  out.reserve( size * 2 );
  for( unsigned int i = 0; i < size; ++i )
  {
    out.push_back( hex[ ( data[ i ] >> 4 ) & 0xf ] );
    out.push_back( hex[ data[ i ] & 0xf ] );
  }
  return out;
}

std::string sha256_string( const std::string& value )
{
  unsigned char digest[ SHA256_DIGEST_LENGTH ];
  SHA256( reinterpret_cast< const unsigned char* >( value.data() ), value.size(), digest );
  return bytes_to_hex( digest, SHA256_DIGEST_LENGTH );
}

std::filesystem::path object_path( const std::filesystem::path& repo, const std::string& sha256 )
{
  return repo / "objects" / "sha256" / sha256.substr( 0, 2 ) / sha256.substr( 2, 2 ) / sha256;
}

struct FileSpec
{
  std::string path;
  std::string content;
  bool runtime_file = false;
};

void write_public_snapshot( const std::filesystem::path& repo,
                            const std::string& backup_id,
                            const std::vector< FileSpec >& files )
{
  uint64_t db_bytes = 0;
  uint64_t runtime_bytes = 0;
  uint64_t total_bytes = 0;

  std::string files_json =
    "{\n"
    "  \"format\": \"teleno-native-snapshot-files\",\n"
    "  \"version\": 1,\n"
    "  \"backup_id\": \"" + backup_id + "\",\n"
    "  \"files\": [\n";

  for( std::size_t i = 0; i < files.size(); ++i )
  {
    const auto& file = files[ i ];
    const auto sha = sha256_string( file.content );
    write_file( object_path( repo, sha ), file.content );
    total_bytes += file.content.size();
    if( file.runtime_file )
      runtime_bytes += file.content.size();
    else
      db_bytes += file.content.size();

    files_json += "    { \"path\": \"" + file.path
               + "\", \"sha256\": \"" + sha
               + "\", \"size_bytes\": " + std::to_string( file.content.size() )
               + ", \"runtime_file\": " + ( file.runtime_file ? "true" : "false" ) + " }";
    if( i + 1 != files.size() )
      files_json += ",";
    files_json += "\n";
  }
  files_json += "  ]\n}\n";

  const auto snapshot_dir = repo / "snapshots" / backup_id;
  write_file( snapshot_dir / "files.json", files_json );
  write_file( snapshot_dir / "manifest.json",
              "{\n"
              "  \"format\": \"teleno-native-rocksdb-snapshot\",\n"
              "  \"version\": 1,\n"
              "  \"backup_id\": \"" + backup_id + "\",\n"
              "  \"created_at\": \"20260619T000000Z\",\n"
              "  \"node\": { \"name\": \"teleno_node\", \"version\": \"test\" },\n"
              "  \"source\": { \"basedir\": \"/tmp/source\", \"node_id\": \"public-test\", \"storage_layout\": \"unified\" },\n"
              "  \"snapshot\": { \"file_count\": " + std::to_string( files.size() )
                + ", \"object_count\": " + std::to_string( files.size() )
                + ", \"total_bytes\": " + std::to_string( total_bytes ) + " },\n"
              "  \"sizes\": {\n"
              "    \"restored_database_bytes\": " + std::to_string( db_bytes ) + ",\n"
              "    \"runtime_files_bytes\": " + std::to_string( runtime_bytes ) + ",\n"
              "    \"object_download_bytes\": " + std::to_string( total_bytes ) + ",\n"
              "    \"archive_bytes\": 0\n"
              "  },\n"
              "  \"restore\": { \"start_as_observer_first\": true, \"force_block_producer_disabled_on_first_start\": true }\n"
              "}\n" );
  write_file( snapshot_dir / "COMPLETE", "complete\n" );
  write_file( repo / "latest.json",
              "{\n"
              "  \"format\": \"teleno-native-latest-snapshot\",\n"
              "  \"version\": 1,\n"
              "  \"backup_id\": \"" + backup_id + "\",\n"
              "  \"snapshot_dir\": \"" + backup_id + "\",\n"
              "  \"manifest\": \"snapshots/" + backup_id + "/manifest.json\",\n"
              "  \"files\": \"snapshots/" + backup_id + "/files.json\"\n"
              "}\n" );
}

} // namespace

int main()
{
  std::string phase = "start";
  try
  {
  {
    phase = "fixture";
    auto root = unique_temp_dir( "teleno-public-restore" );
    auto public_repo = root / "public";
    auto local_repo = root / "local-repo";
    auto target = root / "target";
    const std::string backup_id = "20260619T000000Z-ms-1-files-4";
    write_public_snapshot(
      public_repo,
      backup_id,
      {
        { "db/CURRENT", "CURRENT\n", false },
        { "config.yml", "features:\n  block_producer: true\n", true },
        { "chain/genesis_data.json", "{\"genesis\":true}\n", true },
        { "jsonrpc/descriptors/koinos_descriptors.pb", "descriptor", true }
      } );

    BackupPublicRestoreConfig cfg;
    cfg.enabled = true;
    cfg.base_url = "file://" + public_repo.string();
    cfg.require_https = true;
    cfg.retries = 1;

    phase = "list";
    auto list = list_public_backup_snapshots( local_repo, cfg );
    assert( list.latest_backup_id == backup_id );
    assert( list.snapshots.size() == 1 );
    assert( list.snapshots[ 0 ].backup_id == backup_id );
    assert( list.snapshots[ 0 ].latest );
    assert( list.snapshots[ 0 ].file_count == 4 );
    assert( !std::filesystem::exists( local_repo / "objects" ) );

    auto exact_local_repo = root / "exact-local-repo";
    auto exact_list = list_public_backup_snapshots( exact_local_repo, cfg, backup_id );
    assert( exact_list.snapshots.size() == 1 );
    assert( exact_list.snapshots[ 0 ].backup_id == backup_id );
    assert( !exact_list.snapshots[ 0 ].latest );

    phase = "fetch";
    auto fetch = fetch_public_restore_snapshot( local_repo, target, cfg );
    assert( fetch.backup_id == backup_id );
    assert( fetch.metadata_fetched );
    assert( fetch.objects_fetched );
    assert( fetch.object_file_count == 4 );
    assert( fetch.ready_to_stage );
    assert( fetch.preflight.missing_object_count == 0 );
    assert( public_restore_fetch_result_to_text( fetch ).find( "Fetched public backup restore data" ) != std::string::npos );
    assert( public_restore_fetch_result_to_json( fetch ).find( "\"transport\": \"file\"" ) != std::string::npos );

    phase = "selected";
    auto selected = fetch_public_restore_snapshot( local_repo, target, cfg, backup_id );
    assert( selected.backup_id == backup_id );
    assert( selected.ready_to_stage );
    assert( selected.object_file_count == 0 );

    std::filesystem::remove_all( root );
  }

  {
    phase = "require-https";
    BackupPublicRestoreConfig cfg;
    cfg.enabled = true;
    cfg.base_url = "http://example.invalid/backups";
    cfg.require_https = true;

    bool threw = false;
    try
    {
      (void)list_public_backup_snapshots( "/tmp/teleno-public-restore-invalid", cfg );
    }
    catch( const std::runtime_error& e )
    {
      threw = std::string( e.what() ).find( "requires an https base-url" ) != std::string::npos;
    }
    assert( threw );
  }

  return 0;
  }
  catch( const std::exception& e )
  {
    std::cerr << "phase: " << phase << " error: " << e.what() << "\n";
    throw;
  }
}
