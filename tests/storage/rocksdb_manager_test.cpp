#include <storage/rocksdb_manager.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <rocksdb/convenience.h>
#include <rocksdb/db.h>
#include <rocksdb/metadata.h>
#include <rocksdb/sst_file_writer.h>
#include <rocksdb/write_batch.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#endif

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
    assert( manager.db()->GetDBOptions().max_total_wal_size == 0 );
    assert( manager.path() == dir / "db" );
    assert( manager.column_family_count() == 9 );
    assert( manager.handle( ColumnFamily::blocks ) != nullptr );
    assert( manager.read_metadata( "layout.version" ) == "1" );
    assert( manager.read_metadata( "layout.chain_storage" ) == "unified" );
    assert( manager.read_metadata( "layout.network" ) == "unknown" );
    assert( !manager.read_metadata( "layout.created_at" ).empty() );
    assert( !manager.read_metadata( "layout.created_by" ).empty() );
    assert( manager.read_metadata( "layout.basedir" ) == dir.string() );
    assert( manager.column_family_stats().size() == 9 );
    assert( manager.compression_status().requested_default == "none" );
    const auto memory = manager.memory_stats();
    assert(memory.block_cache_capacity == 8*1024*1024);
    assert(memory.block_cache_pinned_usage <= memory.block_cache_usage);
    assert(memory.aggregate_memtable_flush_threshold == 256*1024*1024);
    assert(memory.all_memtables && memory.table_readers);
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
    assert( manager.read_metadata( "layout.chain_storage" ) == "unified" );
    assert( manager.read_metadata( "test.marker" ) == "ok" );
  }

  std::filesystem::remove_all( dir );
}

#if defined(__unix__) || defined(__APPLE__)
void wal_crash_writer( const std::filesystem::path& dir )
{
  if( !dir.is_absolute() || std::filesystem::exists( dir ) )
    throw std::invalid_argument( "WAL fixture requires a fresh absolute test directory" );
  auto cfg = test_config();
  cfg.rocksdb_write_buffer_size = 64 * 1024;
  cfg.rocksdb_db_write_buffer_size = 128 * 1024;
  cfg.rocksdb_max_write_buffer_number = 2;
  cfg.rocksdb_max_background_jobs = 1;
  cfg.rocksdb_max_total_wal_size = 64 * 1024;
  RocksDBManager manager;
  manager.open( dir, cfg );
  assert( manager.db()->GetDBOptions().max_total_wal_size == cfg.rocksdb_max_total_wal_size );

  rocksdb::WriteOptions writes;
  writes.sync = true;
  rocksdb::WriteBatch cold;
  for( unsigned i = 0; i <= static_cast< unsigned >( ColumnFamily::storage_metadata ); ++i )
    assert( cold.Put( manager.handle( static_cast< ColumnFamily >( i ) ),
                     "wal.cold." + std::to_string( i ), "preserved" ).ok() );
  assert( manager.db()->Write( writes, &cold ).ok() );
  for( unsigned i = 0; i < 128; ++i )
    assert( manager.db()->Put( writes, manager.handle( ColumnFamily::chain_state ),
                              "wal.hot." + std::to_string( i % 8 ),
                              std::string( 4096, static_cast< char >( i ) ) ).ok() );
  assert( manager.db()->FlushWAL( true ).ok() );
  // Exercise recovery of synchronized WAL without DB destructors. This is
  // process-exit recovery, not a power-loss simulation or a hard WAL bound.
  ::_exit( 0 );
}

void test_wal_policy_process_exit_recovery( const std::string& executable )
{
  const auto dir = unique_temp_dir( "teleno-rocksdb-wal-recovery" );
  assert( !std::filesystem::exists( dir ) );
  const auto child = ::fork();
  assert( child >= 0 );
  if( child == 0 )
  {
    // Exec avoids inheriting the parent's RocksDB background-thread state.
    ::execl( executable.c_str(), executable.c_str(), "--wal-crash-writer", dir.c_str(), nullptr );
    ::_exit( 127 );
  }
  int status = 0;
  const auto waited = ::waitpid( child, &status, 0 );
  assert( waited == child && WIFEXITED( status ) && WEXITSTATUS( status ) == 0 );
  {
    RocksDBManager reader;
    reader.open( dir, test_config() );
    assert( reader.db()->GetDBOptions().max_total_wal_size == 0 );
    assert( reader.db()->VerifyChecksum( rocksdb::ReadOptions{} ).ok() );
    for( unsigned i = 0; i <= static_cast< unsigned >( ColumnFamily::storage_metadata ); ++i )
    {
      std::string value;
      assert( reader.db()->Get( {}, reader.handle( static_cast< ColumnFamily >( i ) ),
                               "wal.cold." + std::to_string( i ), &value ).ok() );
      assert( value == "preserved" );
    }
    for( unsigned i = 0; i < 8; ++i )
    {
      std::string value;
      assert( reader.db()->Get( {}, reader.handle( ColumnFamily::chain_state ),
                               "wal.hot." + std::to_string( i ), &value ).ok() );
      assert( value == std::string( 4096, static_cast< char >( 120 + i ) ) );
    }
  }
  std::filesystem::remove_all( dir );
}
#endif

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

std::string profile_key( uint32_t index )
{
  std::string key( 5, '\0' );
  key[ 0 ] = '\x12';
  for( unsigned i = 0; i < 4; ++i )
    key[ i + 1 ] = static_cast< char >( index >> ( 8 * ( 3 - i ) ) );
  return key;
}

std::string profile_value( uint32_t index )
{
  std::string value( 2048, '\0' );
  uint32_t state = 0x6a09e667U + index % 512;
  for( auto& byte: value )
  {
    state = state * 1664525U + 1013904223U;
    byte = static_cast< char >( state >> 24 );
  }
  value[ 0 ] = '\0';
  value[ 1 ] = '\xff';
  return value;
}

void test_blocks_zstd_profile()
{
  for( uint64_t invalid: { uint64_t( 20 ), uint64_t( UINT64_MAX ) } )
  {
    auto dir = unique_temp_dir( "teleno-invalid-zstd-level" );
    NodeConfig cfg;
    cfg.rocksdb_blocks_zstd_level = invalid;
    bool threw = false;
    try { RocksDBManager manager; manager.open( dir, cfg ); }
    catch( const std::invalid_argument& ) { threw = true; }
    assert( threw && !std::filesystem::exists( dir ) );
  }
  for( uint64_t invalid: { uint64_t( 1 ), uint64_t( 65537 ), uint64_t( UINT64_MAX ) } )
  {
    auto dir = unique_temp_dir( "teleno-invalid-zstd-dictionary" );
    NodeConfig cfg;
    cfg.rocksdb_blocks_zstd_dictionary_bytes = invalid;
    bool threw = false;
    try { RocksDBManager manager; manager.open( dir, cfg ); }
    catch( const std::invalid_argument& ) { threw = true; }
    assert( threw && !std::filesystem::exists( dir ) );
  }
  {
    auto dir = unique_temp_dir( "teleno-non-zstd-profile" );
    auto cfg = test_config();
    cfg.rocksdb_blocks_zstd_level = 9;
    bool threw = false;
    try { RocksDBManager manager; manager.open( dir, cfg ); }
    catch( const std::invalid_argument& ) { threw = true; }
    assert( threw && !std::filesystem::exists( dir ) );
  }

  const auto codecs = rocksdb::GetSupportedCompressions();
  const bool supported = std::find( codecs.begin(), codecs.end(), rocksdb::kZSTD ) != codecs.end();
  auto cfg = test_config();
  cfg.rocksdb_blocks_compression = "zstd";
  cfg.rocksdb_blocks_zstd_level = 9;
  cfg.rocksdb_blocks_zstd_dictionary_bytes = 65536;
  cfg.rocksdb_write_buffer_size = 2 * 1024 * 1024;
  cfg.rocksdb_max_background_jobs = 1;
  const auto dir = unique_temp_dir( "teleno-blocks-zstd-profile" );
  if( !supported )
  {
    bool threw = false;
    try { RocksDBManager manager; manager.open( dir, cfg ); }
    catch( const std::runtime_error& e )
    {
      threw = std::string( e.what() ).find( "linked Zstd support" ) != std::string::npos;
    }
    assert( threw && !std::filesystem::exists( dir ) );
    return;
  }
  for( bool dictionary_only: { false, true } )
  {
    auto variant = cfg;
    variant.rocksdb_blocks_zstd_level = dictionary_only ? 0 : 9;
    variant.rocksdb_blocks_zstd_dictionary_bytes = dictionary_only ? 32768 : 0;
    const auto variant_dir = unique_temp_dir( "teleno-zstd-independent-options" );
    {
      RocksDBManager manager;
      manager.open( variant_dir, variant );
      const auto options = manager.db()->GetOptions( manager.handle( ColumnFamily::blocks ) );
      assert( options.compression_opts.level == ( dictionary_only
                ? rocksdb::CompressionOptions::kDefaultCompressionLevel : 9 ) );
      assert( options.compression_opts.max_dict_bytes == ( dictionary_only ? 32768 : 0 ) );
      assert( options.compression_opts.max_dict_buffer_bytes == ( dictionary_only ? 8 * 1024 * 1024 : 0 ) );
      assert( options.bottommost_compression_opts.enabled );
    }
    std::filesystem::remove_all( variant_dir );
  }
  constexpr uint32_t count = 12000;
  {
    RocksDBManager manager;
    manager.open( dir, cfg );
    const auto options = manager.db()->GetOptions( manager.handle( ColumnFamily::blocks ) );
    assert( options.compression_opts.level == 9 );
    assert( options.compression_opts.max_dict_bytes == 65536 );
    assert( options.compression_opts.zstd_max_train_bytes == 1024 * 1024 );
    assert( options.compression_opts.max_dict_buffer_bytes == 8 * 1024 * 1024 );
    assert( options.bottommost_compression_opts.enabled );
    assert( options.bottommost_compression_opts.level == 9 );
    assert( options.bottommost_compression_opts.max_dict_bytes == 65536 );
    const auto state = manager.db()->GetOptions( manager.handle( ColumnFamily::chain_state ) );
    assert( state.compression == rocksdb::kNoCompression );
    assert( state.compression_opts.level == rocksdb::CompressionOptions::kDefaultCompressionLevel );
    assert( state.compression_opts.max_dict_bytes == 0 );
    for( uint32_t i = 0; i < count; ++i )
      assert( manager.db()->Put( rocksdb::WriteOptions(), manager.handle( ColumnFamily::blocks ),
                                profile_key( i ), profile_value( i ) ).ok() );
    rocksdb::FlushOptions flush;
    flush.wait = true;
    assert( manager.db()->Flush( flush, manager.handle( ColumnFamily::blocks ) ).ok() );
    manager.compact_column_family( ColumnFamily::blocks );
    rocksdb::TablePropertiesCollection properties;
    assert( manager.db()->GetPropertiesOfAllTables( manager.handle( ColumnFamily::blocks ), &properties ).ok() );
    assert( !properties.empty() );
    for( const auto& [path, table]: properties )
    {
      assert( table->compression_name == "ZSTD" );
      assert( table->compression_options.find( "level=9;" ) != std::string::npos );
      assert( table->compression_options.find( "max_dict_bytes=65536;" ) != std::string::npos );
      assert( table->db_host_id.empty() );
    }
  }
  {
    // Reopen old tables with the larger opt-in policy, then deliberately
    // rewrite them. The ordinary default reader below must retain every byte.
    auto larger = cfg;
    larger.rocksdb_blocks_zstd_level = 15;
    larger.rocksdb_blocks_zstd_dictionary_bytes = 131072;
    RocksDBManager manager;
    manager.open( dir, larger );
    const auto options = manager.db()->GetOptions( manager.handle( ColumnFamily::blocks ) );
    assert( options.compression_opts.max_dict_bytes == 131072 );
    assert( options.bottommost_compression_opts.max_dict_bytes == 131072 );
    assert( options.compression_opts.zstd_max_train_bytes == 1024 * 1024 );
    assert( options.compression_opts.max_dict_buffer_bytes == 8 * 1024 * 1024 );
    rocksdb::TablePropertiesCollection before;
    assert( manager.db()->GetPropertiesOfAllTables( manager.handle( ColumnFamily::blocks ), &before ).ok() );
    assert( !before.empty() );
    for( const auto& [path, table]: before )
      assert( table->compression_options.find( "max_dict_bytes=65536;" ) != std::string::npos );
    manager.compact_column_family( ColumnFamily::blocks, true );
    assert( manager.db()->VerifyChecksum( rocksdb::ReadOptions{} ).ok() );
    rocksdb::TablePropertiesCollection after;
    assert( manager.db()->GetPropertiesOfAllTables( manager.handle( ColumnFamily::blocks ), &after ).ok() );
    assert( !after.empty() );
    for( const auto& [path, table]: after )
    {
      assert( table->compression_name == "ZSTD" );
      assert( table->compression_options.find( "level=15;" ) != std::string::npos );
      assert( table->compression_options.find( "max_dict_bytes=131072;" ) != std::string::npos );
      assert( table->db_host_id.empty() );
    }
  }
  {
    NodeConfig defaults;
    defaults.rocksdb_block_cache_mb = 8;
    RocksDBManager manager;
    manager.open( dir, defaults );
    const auto options = manager.db()->GetOptions( manager.handle( ColumnFamily::blocks ) );
    assert( options.compression_opts.level == rocksdb::CompressionOptions::kDefaultCompressionLevel );
    assert( options.compression_opts.max_dict_bytes == 0 );
    assert( !options.bottommost_compression_opts.enabled );
    std::unique_ptr< rocksdb::Iterator > iterator( manager.db()->NewIterator( {}, manager.handle( ColumnFamily::blocks ) ) );
    uint32_t i = 0;
    for( iterator->SeekToFirst(); iterator->Valid(); iterator->Next(), ++i )
    {
      assert( i < count && iterator->key() == rocksdb::Slice( profile_key( i ) ) );
      assert( iterator->value() == rocksdb::Slice( profile_value( i ) ) );
    }
    assert( iterator->status().ok() && i == count );
  }
  std::filesystem::remove_all( dir );
}

void test_explicit_bottommost_rewrite()
{
  const auto codecs = rocksdb::GetSupportedCompressions();
  if( std::find( codecs.begin(), codecs.end(), rocksdb::kZSTD ) == codecs.end() )
    return;

  const auto root = unique_temp_dir( "teleno-explicit-rewrite" );
  const auto basedir = root / "node";
  auto cfg = test_config();
  cfg.rocksdb_blocks_compression = "zstd";
  cfg.rocksdb_blocks_zstd_level = 9;
  cfg.rocksdb_blocks_zstd_dictionary_bytes = 65536;
  cfg.rocksdb_write_buffer_size = 2 * 1024 * 1024;
  cfg.rocksdb_max_background_jobs = 1;
  constexpr uint32_t count = 4096;
  {
    RocksDBManager manager;
    manager.open( basedir, cfg );
    auto* handle = manager.handle( ColumnFamily::blocks );
    rocksdb::Options old( manager.db()->GetDBOptions(), manager.db()->GetOptions( handle ) );
    old.db_host_id = "synthetic-fixture-host";
    old.compression_opts.level = 3;
    old.compression_opts.max_dict_bytes = 0;
    old.compression_opts.zstd_max_train_bytes = 0;
    old.compression_opts.max_dict_buffer_bytes = 0;
    old.bottommost_compression_opts = old.compression_opts;
    const auto input = root / "old-policy.sst";
    {
      rocksdb::SstFileWriter writer( rocksdb::EnvOptions(), old, handle );
      assert( writer.Open( input.string() ).ok() );
      for( uint32_t i = 0; i < count; ++i )
        assert( writer.Put( profile_key( i ), profile_value( i ) ).ok() );
      assert( writer.Finish().ok() );
    }
    rocksdb::IngestExternalFileOptions ingest;
    ingest.move_files = true;
    ingest.failed_move_fall_back_to_copy = false;
    ingest.snapshot_consistency = false;
    ingest.allow_global_seqno = false;
    ingest.write_global_seqno = false;
    ingest.verify_checksums_before_ingest = true;
    assert( manager.db()->IngestExternalFile( handle, { input.string() }, ingest ).ok() );
    std::vector< rocksdb::LiveFileMetaData > files;
    manager.db()->GetLiveFilesMetaData( &files );
    auto table = std::find_if( files.begin(), files.end(), []( const auto& file ) {
      return file.column_family_name == "blocks";
    } );
    assert( table != files.end() && table->level == old.num_levels - 1 );

    rocksdb::TablePropertiesCollection before, ordinary, rewritten;
    assert( manager.db()->GetPropertiesOfAllTables( handle, &before ).ok() && before.size() == 1 );
    assert( before.begin()->second->db_host_id == "synthetic-fixture-host" );
    assert( before.begin()->second->compression_options.find( "level=3;" ) != std::string::npos );
    manager.compact_column_family( ColumnFamily::blocks );
    assert( manager.db()->GetPropertiesOfAllTables( handle, &ordinary ).ok() && ordinary.size() == 1 );
    assert( ordinary.begin()->first == before.begin()->first );
    assert( ordinary.begin()->second->db_host_id == "synthetic-fixture-host" );

    manager.compact_column_family( ColumnFamily::blocks, true );
    assert( manager.db()->GetPropertiesOfAllTables( handle, &rewritten ).ok() && !rewritten.empty() );
    for( const auto& [name, properties]: rewritten )
    {
      assert( before.find( name ) == before.end() );
      assert( properties->db_host_id.empty() );
      assert( properties->compression_name == "ZSTD" );
      assert( properties->compression_options.find( "level=9;" ) != std::string::npos );
      assert( properties->compression_options.find( "max_dict_bytes=65536;" ) != std::string::npos );
    }
  }
  {
    RocksDBManager reader;
    reader.open( basedir, test_config() );
    std::unique_ptr< rocksdb::Iterator > it( reader.db()->NewIterator( {}, reader.handle( ColumnFamily::blocks ) ) );
    uint32_t i = 0;
    for( it->SeekToFirst(); it->Valid(); it->Next(), ++i )
      assert( i < count && it->key() == rocksdb::Slice( profile_key( i ) )
              && it->value() == rocksdb::Slice( profile_value( i ) ) );
    assert( it->status().ok() && i == count );
  }
  std::filesystem::remove_all( root );
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

int main( int argc, char** argv )
{
#if defined(__unix__) || defined(__APPLE__)
  if( argc == 3 && std::string( argv[1] ) == "--wal-crash-writer" )
  {
    wal_crash_writer( argv[2] );
    return 0;
  }
#endif
  if( argc != 1 ) return 2;
  test_open_initializes_metadata();
#if defined(__unix__) || defined(__APPLE__)
  test_wal_policy_process_exit_recovery( std::filesystem::absolute( argv[0] ).string() );
#endif
  {
    RocksDBManager closed;
    bool threw=false;
    try { closed.memory_stats(); } catch(const std::exception&) { threw=true; }
    assert(threw);
    NodeConfig overflow; overflow.rocksdb_block_cache_mb=UINT64_MAX;
    threw=false;
    try { closed.open(unique_temp_dir("lr2-overflow"),overflow); } catch(const std::exception&) { threw=true; }
    assert(threw);
  }
  test_column_family_name_parser();
  test_require_compression_gate();
  test_compacts_column_family();
  test_blocks_zstd_profile();
  test_explicit_bottommost_rewrite();
  test_refuses_interrupted_migration();
  test_refuses_unknown_layout();
  return 0;
}
