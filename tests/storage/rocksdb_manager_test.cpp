#include <storage/rocksdb_manager.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <rocksdb/convenience.h>
#include <rocksdb/db.h>

using koinos::node::NodeConfig;
using koinos::node::storage::ColumnFamily;
using koinos::node::storage::RocksDBManager;
using koinos::node::storage::column_family_from_name;

namespace
{

std::filesystem::path unique_temp_dir( const std::string& name )
{
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() / ( name + "-" + std::to_string( now ) );
}

NodeConfig test_config()
{
  NodeConfig cfg;
  cfg.rocksdb_block_cache_mb = 8;
  cfg.rocksdb_compression = "none";
  cfg.rocksdb_blocks_compression = "none";
  return cfg;
}

bool has_supported_compression( const RocksDBManager& manager, const std::string& name )
{
  const auto& supported = manager.compression_status().supported_compressions;
  return std::find( supported.begin(), supported.end(), name ) != supported.end();
}

void test_open_initializes_metadata()
{
  const auto dir = unique_temp_dir( "teleno-rocksdb-manager-open" );
  std::filesystem::remove_all( dir );

  {
    RocksDBManager manager;
    manager.open( dir, test_config() );

    assert( manager.db() != nullptr );
    assert( manager.path() == dir / "db" );
    assert( manager.column_family_count() == 9 );
    assert( manager.handle( ColumnFamily::blocks ) != nullptr );
    assert( manager.read_metadata( "layout.version" ) == "1" );
    assert( manager.read_metadata( "layout.chain_storage" ) == "legacy" );
    assert( manager.read_metadata( "layout.network" ) == "unknown" );
    assert( !manager.read_metadata( "layout.created_at" ).empty() );
    assert( !manager.read_metadata( "layout.created_by" ).empty() );
    assert( manager.read_metadata( "layout.basedir" ) == dir.string() );
    assert( manager.column_family_stats().size() == 9 );
    assert( manager.compression_status().requested_default == "none" );
    assert( manager.compression_status().selected_default == "none" );
    assert( manager.compression_status().requested_blocks == "none" );
    assert( manager.compression_status().selected_blocks == "none" );
    assert( has_supported_compression( manager, "none" ) );

    manager.write_metadata( "test.marker", "ok" );
    assert( manager.read_metadata( "test.marker" ) == "ok" );
  }

  {
    RocksDBManager manager;
    manager.open( dir, test_config() );

    assert( manager.column_family_count() == 9 );
    assert( manager.read_metadata( "layout.chain_storage" ) == "legacy" );
    assert( manager.read_metadata( "test.marker" ) == "ok" );
  }

  std::filesystem::remove_all( dir );
}

void test_column_family_name_parser()
{
  assert( column_family_from_name( "blocks" ) == ColumnFamily::blocks );
  assert( column_family_from_name( "chain_state" ) == ColumnFamily::chain_state );

  bool threw = false;
  try
  {
    column_family_from_name( "missing" );
  }
  catch( const std::runtime_error& )
  {
    threw = true;
  }
  assert( threw );
}

void test_require_compression_gate()
{
  const auto dir = unique_temp_dir( "teleno-rocksdb-manager-compression-gate" );
  std::filesystem::remove_all( dir );

  NodeConfig cfg = test_config();
  cfg.rocksdb_compression = "zstd";
  cfg.rocksdb_blocks_compression.clear();
  cfg.rocksdb_require_compression = true;

  const auto supported = rocksdb::GetSupportedCompressions();
  const bool zstd_supported = std::find( supported.begin(), supported.end(), rocksdb::kZSTD ) != supported.end();

  bool threw = false;
  try
  {
    RocksDBManager manager;
    manager.open( dir, cfg );
    assert( zstd_supported );
    assert( manager.compression_status().selected_default == "zstd" );
    assert( manager.compression_status().selected_blocks == "zstd" );
  }
  catch( const std::runtime_error& e )
  {
    threw = std::string( e.what() ).find( "compression requirement failed" ) != std::string::npos;
  }

  assert( threw != zstd_supported );
  std::filesystem::remove_all( dir );
}

void test_compacts_column_family()
{
  const auto dir = unique_temp_dir( "teleno-rocksdb-manager-compact" );
  std::filesystem::remove_all( dir );

  {
    RocksDBManager manager;
    manager.open( dir, test_config() );
    for( int i = 0; i < 100; ++i )
    {
      auto key = "key-" + std::to_string( i );
      auto value = std::string( 1024, static_cast< char >( 'a' + ( i % 26 ) ) );
      auto status = manager.db()->Put( rocksdb::WriteOptions(), manager.handle( ColumnFamily::blocks ), key, value );
      assert( status.ok() );
    }
    manager.compact_column_family( ColumnFamily::blocks );
    manager.compact_all_column_families();
  }

  std::filesystem::remove_all( dir );
}

void test_refuses_interrupted_migration()
{
  const auto dir = unique_temp_dir( "teleno-rocksdb-manager-migration" );
  std::filesystem::remove_all( dir );

  {
    RocksDBManager manager;
    manager.open( dir, test_config() );
    manager.write_metadata( "layout.chain_storage", "migration-in-progress" );
  }

  bool threw = false;
  try
  {
    RocksDBManager manager;
    manager.open( dir, test_config() );
  }
  catch( const std::runtime_error& e )
  {
    threw = std::string( e.what() ).find( "migration is marked in progress" ) != std::string::npos;
  }

  assert( threw );
  std::filesystem::remove_all( dir );
}

void test_refuses_unknown_layout()
{
  const auto dir = unique_temp_dir( "teleno-rocksdb-manager-unknown" );
  std::filesystem::remove_all( dir );

  {
    RocksDBManager manager;
    manager.open( dir, test_config() );
    manager.write_metadata( "layout.chain_storage", "future-layout" );
  }

  bool threw = false;
  try
  {
    RocksDBManager manager;
    manager.open( dir, test_config() );
  }
  catch( const std::runtime_error& e )
  {
    threw = std::string( e.what() ).find( "unknown chain storage layout" ) != std::string::npos;
  }

  assert( threw );
  std::filesystem::remove_all( dir );
}

} // namespace

int main()
{
  test_open_initializes_metadata();
  test_column_family_name_parser();
  test_require_compression_gate();
  test_compacts_column_family();
  test_refuses_interrupted_migration();
  test_refuses_unknown_layout();
  return 0;
}
