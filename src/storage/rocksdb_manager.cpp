#include "storage/rocksdb_manager.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>

#include <rocksdb/cache.h>
#include <rocksdb/convenience.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/options.h>
#include <rocksdb/slice_transform.h>
#include <rocksdb/table.h>

#include <koinos/log.hpp>

#include "core/node_version.hpp"

namespace koinos::node::storage {
namespace {

std::string lowercase( std::string value )
{
  std::transform( value.begin(), value.end(), value.begin(),
                  []( unsigned char ch ) { return static_cast< char >( std::tolower( ch ) ); } );
  return value;
}

std::vector< rocksdb::CompressionType > supported_compressions()
{
  auto supported = rocksdb::GetSupportedCompressions();
  if( std::find( supported.begin(), supported.end(), rocksdb::kNoCompression ) == supported.end() )
    supported.push_back( rocksdb::kNoCompression );
  return supported;
}

std::vector< std::string > supported_compression_names()
{
  std::vector< std::string > names;
  for( auto compression: supported_compressions() )
  {
    auto name = compression_name( compression );
    if( std::find( names.begin(), names.end(), name ) == names.end() )
      names.push_back( std::move( name ) );
  }
  std::sort( names.begin(), names.end() );
  return names;
}

std::optional< rocksdb::CompressionType > compression_from_token( const std::string& requested )
{
  const auto token = lowercase( requested );

  if( token == "none" || token == "no" || token == "disabled" || token == "off" )
    return rocksdb::kNoCompression;
  if( token == "snappy" || token == "ksnappycompression" )
    return rocksdb::kSnappyCompression;
  if( token == "zstd" || token == "kzstd" || token == "kzstdcompression" )
    return rocksdb::kZSTD;
  return std::nullopt;
}

rocksdb::CompressionType select_supported_compression( const std::string& requested,
                                                       const std::string& label,
                                                       bool require_exact,
                                                       std::string& fallback_note )
{
  const auto normalized = requested.empty() ? std::string( "zstd" ) : requested;
  const auto requested_type = compression_from_token( normalized );
  if( !requested_type )
    throw std::runtime_error( "unknown RocksDB compression for " + label + ": " + requested );

  std::vector< rocksdb::CompressionType > preferences = { *requested_type };
  if( *requested_type == rocksdb::kZSTD )
    preferences.push_back( rocksdb::kSnappyCompression );
  if( *requested_type != rocksdb::kNoCompression )
    preferences.push_back( rocksdb::kNoCompression );

  auto supported = supported_compressions();

  for( auto candidate: preferences )
  {
    if( std::find( supported.begin(), supported.end(), candidate ) != supported.end() )
    {
      if( !preferences.empty() && candidate != preferences.front() )
      {
        const auto note = label + ": requested " + normalized + ", selected "
                          + compression_name( candidate ) + " because the requested codec is unsupported";
        if( !fallback_note.empty() )
          fallback_note += "; ";
        fallback_note += note;
        if( require_exact )
          throw std::runtime_error( "RocksDB compression requirement failed: " + note );
      }
      return candidate;
    }
  }

  const auto note = label + ": requested " + normalized + ", selected none because no requested codec is supported";
  if( !fallback_note.empty() )
    fallback_note += "; ";
  fallback_note += note;
  if( require_exact )
    throw std::runtime_error( "RocksDB compression requirement failed: " + note );
  return rocksdb::kNoCompression;
}

rocksdb::BlockBasedTableOptions make_table_options( uint64_t block_size,
                                                    const std::shared_ptr< rocksdb::Cache >& block_cache,
                                                    bool bloom_filter,
                                                    bool whole_key_filtering = true )
{
  rocksdb::BlockBasedTableOptions opts;
  opts.block_size                                  = static_cast< size_t >( block_size );
  opts.block_cache                                 = block_cache;
  opts.cache_index_and_filter_blocks               = true;
  opts.cache_index_and_filter_blocks_with_high_priority = true;
  opts.pin_l0_filter_and_index_blocks_in_cache     = true;
  opts.whole_key_filtering                         = whole_key_filtering;

  if( bloom_filter )
    opts.filter_policy.reset( rocksdb::NewBloomFilterPolicy( 10 ) );

  return opts;
}

void apply_point_lookup_cf_tuning( rocksdb::ColumnFamilyOptions& cf,
                                   uint64_t write_buffer_size,
                                   uint64_t max_write_buffer_number,
                                   uint64_t target_file_size_base,
                                   uint64_t max_bytes_for_level_base,
                                   rocksdb::CompressionType compression )
{
  cf.write_buffer_size                = static_cast< size_t >( write_buffer_size );
  cf.max_write_buffer_number          = static_cast< int >( std::max< uint64_t >( 1, max_write_buffer_number ) );
  cf.target_file_size_base            = target_file_size_base;
  cf.max_bytes_for_level_base         = max_bytes_for_level_base;
  cf.level_compaction_dynamic_level_bytes = true;
  cf.optimize_filters_for_hits        = true;
  cf.memtable_whole_key_filtering     = true;
  cf.compression                      = compression;
  cf.bottommost_compression           = compression;
}

std::string iso8601_now()
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
  out << std::put_time( &tm, "%Y-%m-%dT%H:%M:%SZ" );
  return out.str();
}

uint64_t get_int_property_or_zero( rocksdb::DB& db,
                                   rocksdb::ColumnFamilyHandle* handle,
                                   const std::string& property )
{
  uint64_t value = 0;
  if( !db.GetIntProperty( handle, property, &value ) )
    return 0;
  return value;
}

} // anonymous namespace

const char* column_family_name( ColumnFamily cf )
{
  switch( cf )
  {
    case ColumnFamily::default_state:
      return rocksdb::kDefaultColumnFamilyName.c_str();
    case ColumnFamily::blocks:
      return "blocks";
    case ColumnFamily::block_meta:
      return "block_meta";
    case ColumnFamily::contract_meta:
      return "contract_meta";
    case ColumnFamily::transaction_index:
      return "transaction_index";
    case ColumnFamily::account_history:
      return "account_history";
    case ColumnFamily::chain_state:
      return "chain_state";
    case ColumnFamily::chain_metadata:
      return "chain_metadata";
    case ColumnFamily::storage_metadata:
      return "storage_metadata";
    default:
      return "unknown";
  }
}

ColumnFamily column_family_from_name( const std::string& name )
{
  for( std::size_t i = 0; i <= static_cast< std::size_t >( ColumnFamily::storage_metadata ); ++i )
  {
    auto cf = static_cast< ColumnFamily >( i );
    if( name == column_family_name( cf ) )
      return cf;
  }
  throw std::runtime_error( "unknown RocksDB column family: " + name );
}

std::string compression_name( rocksdb::CompressionType compression )
{
  switch( compression )
  {
    case rocksdb::kNoCompression:
      return "none";
    case rocksdb::kSnappyCompression:
      return "snappy";
    case rocksdb::kZSTD:
      return "zstd";
    case rocksdb::kZSTDNotFinalCompression:
      return "zstd-not-final";
    default:
      return std::to_string( static_cast< int >( compression ) );
  }
}

RocksDBManager::~RocksDBManager()
{
  close();
}

void RocksDBManager::open( const std::filesystem::path& basedir, const NodeConfig& cfg )
{
  close();

  rocksdb::Options db_options;
  db_options.create_if_missing              = true;
  db_options.create_missing_column_families = true;
  db_options.max_background_jobs            = static_cast< int >( std::max< uint64_t >( 1, cfg.rocksdb_max_background_jobs ) );
  db_options.max_subcompactions             = static_cast< uint32_t >( std::max< uint64_t >( 1, cfg.rocksdb_max_background_jobs / 2 ) );
  db_options.bytes_per_sync                 = cfg.rocksdb_bytes_per_sync;
  db_options.db_write_buffer_size           = static_cast< size_t >( cfg.rocksdb_db_write_buffer_size );
  db_options.enable_pipelined_write         = true;

  _path = basedir / "db";
  std::filesystem::create_directories( _path );

  auto shared_block_cache = rocksdb::NewLRUCache(
    static_cast< size_t >( cfg.rocksdb_block_cache_mb * 1024 * 1024 ),
    -1,
    false,
    0.35
  );

  std::string compression_fallback_note;
  const auto default_requested = cfg.rocksdb_compression.empty() ? std::string( "zstd" ) : cfg.rocksdb_compression;
  const auto blocks_requested = cfg.rocksdb_blocks_compression.empty()
                              ? default_requested
                              : cfg.rocksdb_blocks_compression;
  auto default_compression = select_supported_compression( default_requested,
                                                           "default",
                                                           cfg.rocksdb_require_compression,
                                                           compression_fallback_note );
  auto blocks_compression = select_supported_compression( blocks_requested,
                                                          "blocks",
                                                          cfg.rocksdb_require_compression,
                                                          compression_fallback_note );
  _compression_status.requested_default = default_requested;
  _compression_status.selected_default = compression_name( default_compression );
  _compression_status.requested_blocks = blocks_requested;
  _compression_status.selected_blocks = compression_name( blocks_compression );
  _compression_status.fallback_note = compression_fallback_note;
  _compression_status.supported_compressions = supported_compression_names();

  rocksdb::ColumnFamilyOptions cf_default;
  apply_point_lookup_cf_tuning( cf_default,
                                cfg.rocksdb_write_buffer_size,
                                cfg.rocksdb_max_write_buffer_number,
                                cfg.rocksdb_target_file_size_base,
                                cfg.rocksdb_max_bytes_for_level_base,
                                default_compression );
  auto default_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                shared_block_cache,
                                                true );
  cf_default.table_factory.reset( rocksdb::NewBlockBasedTableFactory( default_table_opts ) );

  rocksdb::ColumnFamilyOptions cf_blocks;
  apply_point_lookup_cf_tuning( cf_blocks,
                                cfg.rocksdb_write_buffer_size,
                                cfg.rocksdb_max_write_buffer_number,
                                cfg.rocksdb_target_file_size_base,
                                cfg.rocksdb_max_bytes_for_level_base,
                                blocks_compression );
  auto blocks_table_opts = make_table_options( cfg.rocksdb_blocks_block_size,
                                               shared_block_cache,
                                               true );
  cf_blocks.table_factory.reset( rocksdb::NewBlockBasedTableFactory( blocks_table_opts ) );
  rocksdb::ColumnFamilyOptions cf_block_meta;
  apply_point_lookup_cf_tuning( cf_block_meta,
                                cfg.rocksdb_write_buffer_size,
                                cfg.rocksdb_max_write_buffer_number,
                                cfg.rocksdb_target_file_size_base,
                                cfg.rocksdb_max_bytes_for_level_base,
                                default_compression );
  auto block_meta_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                   shared_block_cache,
                                                   true );
  cf_block_meta.table_factory.reset( rocksdb::NewBlockBasedTableFactory( block_meta_table_opts ) );

  rocksdb::ColumnFamilyOptions cf_contract_meta;
  apply_point_lookup_cf_tuning( cf_contract_meta,
                                cfg.rocksdb_write_buffer_size,
                                cfg.rocksdb_max_write_buffer_number,
                                cfg.rocksdb_target_file_size_base,
                                cfg.rocksdb_max_bytes_for_level_base,
                                default_compression );
  auto contract_meta_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                      shared_block_cache,
                                                      true );
  cf_contract_meta.table_factory.reset( rocksdb::NewBlockBasedTableFactory( contract_meta_table_opts ) );

  rocksdb::ColumnFamilyOptions cf_tx_index;
  apply_point_lookup_cf_tuning( cf_tx_index,
                                cfg.rocksdb_write_buffer_size,
                                cfg.rocksdb_max_write_buffer_number,
                                cfg.rocksdb_target_file_size_base,
                                cfg.rocksdb_max_bytes_for_level_base,
                                default_compression );
  auto tx_index_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                 shared_block_cache,
                                                 true );
  cf_tx_index.table_factory.reset( rocksdb::NewBlockBasedTableFactory( tx_index_table_opts ) );

  rocksdb::ColumnFamilyOptions cf_acct_history;
  apply_point_lookup_cf_tuning( cf_acct_history,
                                cfg.rocksdb_write_buffer_size,
                                cfg.rocksdb_max_write_buffer_number,
                                cfg.rocksdb_target_file_size_base,
                                cfg.rocksdb_max_bytes_for_level_base,
                                default_compression );
  cf_acct_history.prefix_extractor.reset( rocksdb::NewFixedPrefixTransform( 34 ) );
  cf_acct_history.memtable_whole_key_filtering = false;
  auto acct_history_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                     shared_block_cache,
                                                     true,
                                                     false );
  cf_acct_history.table_factory.reset( rocksdb::NewBlockBasedTableFactory( acct_history_table_opts ) );

  rocksdb::ColumnFamilyOptions cf_storage_metadata;
  apply_point_lookup_cf_tuning( cf_storage_metadata,
                                cfg.rocksdb_write_buffer_size,
                                cfg.rocksdb_max_write_buffer_number,
                                cfg.rocksdb_target_file_size_base,
                                cfg.rocksdb_max_bytes_for_level_base,
                                default_compression );
  auto storage_metadata_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                         shared_block_cache,
                                                         true );
  cf_storage_metadata.table_factory.reset( rocksdb::NewBlockBasedTableFactory( storage_metadata_table_opts ) );

  rocksdb::ColumnFamilyOptions cf_chain_state;
  apply_point_lookup_cf_tuning( cf_chain_state,
                                cfg.rocksdb_write_buffer_size,
                                cfg.rocksdb_max_write_buffer_number,
                                cfg.rocksdb_target_file_size_base,
                                cfg.rocksdb_max_bytes_for_level_base,
                                default_compression );
  auto chain_state_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                    shared_block_cache,
                                                    true );
  cf_chain_state.table_factory.reset( rocksdb::NewBlockBasedTableFactory( chain_state_table_opts ) );

  rocksdb::ColumnFamilyOptions cf_chain_metadata;
  apply_point_lookup_cf_tuning( cf_chain_metadata,
                                cfg.rocksdb_write_buffer_size,
                                cfg.rocksdb_max_write_buffer_number,
                                cfg.rocksdb_target_file_size_base,
                                cfg.rocksdb_max_bytes_for_level_base,
                                default_compression );
  auto chain_metadata_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                       shared_block_cache,
                                                       true );
  cf_chain_metadata.table_factory.reset( rocksdb::NewBlockBasedTableFactory( chain_metadata_table_opts ) );

  LOG( info ) << "[db] RocksDB tuning: block_cache_mb=" << cfg.rocksdb_block_cache_mb
              << " max_background_jobs=" << db_options.max_background_jobs
              << " max_subcompactions=" << db_options.max_subcompactions
              << " default_block_size=" << cfg.rocksdb_default_block_size
              << " blocks_block_size=" << cfg.rocksdb_blocks_block_size
              << " compression=" << compression_name( default_compression )
              << " blocks_compression=" << compression_name( blocks_compression )
              << " supported_compressions=" << [&]() {
                   std::ostringstream out;
                   for( std::size_t i = 0; i < _compression_status.supported_compressions.size(); ++i )
                   {
                     if( i )
                       out << ",";
                     out << _compression_status.supported_compressions[ i ];
                   }
                   return out.str();
                 }();
  if( !compression_fallback_note.empty() )
    LOG( info ) << "[db] RocksDB compression fallback: " << compression_fallback_note;

  std::vector< rocksdb::ColumnFamilyDescriptor > cf_descriptors = {
    { column_family_name( ColumnFamily::default_state ),     cf_default },
    { column_family_name( ColumnFamily::blocks ),            cf_blocks },
    { column_family_name( ColumnFamily::block_meta ),        cf_block_meta },
    { column_family_name( ColumnFamily::contract_meta ),     cf_contract_meta },
    { column_family_name( ColumnFamily::transaction_index ), cf_tx_index },
    { column_family_name( ColumnFamily::account_history ),   cf_acct_history },
    { column_family_name( ColumnFamily::chain_state ),       cf_chain_state },
    { column_family_name( ColumnFamily::chain_metadata ),    cf_chain_metadata },
    { column_family_name( ColumnFamily::storage_metadata ),  cf_storage_metadata }
  };

  rocksdb::DB* raw_db = nullptr;
  auto db_status = rocksdb::DB::Open( db_options, _path.string(), cf_descriptors, &_handles, &raw_db );
  if( !db_status.ok() )
  {
    LOG( error ) << "[db] Failed to open RocksDB at " << _path.string() << ": " << db_status.ToString();
    throw std::runtime_error( "failed to open RocksDB at " + _path.string() + ": " + db_status.ToString() );
  }

  _db.reset( raw_db );
  initialize_storage_metadata( basedir );

  LOG( info ) << "[db] RocksDB opened at " << _path.string()
              << " with " << _handles.size() << " column families";
}

void RocksDBManager::close()
{
  for( auto* handle: _handles )
    delete handle;
  _handles.clear();
  _db.reset();
  _path.clear();
}

rocksdb::DB* RocksDBManager::db() const
{
  return _db.get();
}

rocksdb::ColumnFamilyHandle* RocksDBManager::handle( ColumnFamily cf ) const
{
  const auto index = static_cast< std::size_t >( cf );
  if( index >= _handles.size() )
    throw std::out_of_range( "RocksDB column family handle index out of range" );
  return _handles[ index ];
}

const std::filesystem::path& RocksDBManager::path() const
{
  return _path;
}

std::size_t RocksDBManager::column_family_count() const
{
  return _handles.size();
}

std::vector< ColumnFamilyStats > RocksDBManager::column_family_stats() const
{
  if( !_db )
    throw std::runtime_error( "RocksDB is not open" );

  std::vector< ColumnFamilyStats > stats;
  stats.reserve( _handles.size() );
  for( std::size_t i = 0; i < _handles.size(); ++i )
  {
    auto cf = static_cast< ColumnFamily >( i );
    ColumnFamilyStats row;
    row.name = column_family_name( cf );
    row.estimated_keys = get_int_property_or_zero( *_db, _handles[ i ], "rocksdb.estimate-num-keys" );
    row.total_sst_file_size = get_int_property_or_zero( *_db, _handles[ i ], "rocksdb.total-sst-files-size" );
    row.estimated_live_data_size = get_int_property_or_zero( *_db, _handles[ i ], "rocksdb.estimate-live-data-size" );
    stats.push_back( std::move( row ) );
  }
  return stats;
}

const CompressionStatus& RocksDBManager::compression_status() const
{
  return _compression_status;
}

std::string RocksDBManager::read_metadata( const std::string& key ) const
{
  if( !_db )
    throw std::runtime_error( "RocksDB is not open" );

  std::string value;
  auto status = _db->Get( rocksdb::ReadOptions(), handle( ColumnFamily::storage_metadata ), key, &value );
  if( status.IsNotFound() )
    return {};
  if( !status.ok() )
    throw std::runtime_error( "failed to read storage metadata " + key + ": " + status.ToString() );
  return value;
}

void RocksDBManager::write_metadata( const std::string& key, const std::string& value )
{
  if( !_db )
    throw std::runtime_error( "RocksDB is not open" );

  auto status = _db->Put( rocksdb::WriteOptions(), handle( ColumnFamily::storage_metadata ), key, value );
  if( !status.ok() )
    throw std::runtime_error( "failed to write storage metadata " + key + ": " + status.ToString() );
}

void RocksDBManager::compact_column_family( ColumnFamily cf )
{
  if( !_db )
    throw std::runtime_error( "RocksDB is not open" );

  auto* cf_handle = handle( cf );
  rocksdb::FlushOptions flush_options;
  auto status = _db->Flush( flush_options, cf_handle );
  if( !status.ok() )
    throw std::runtime_error( "failed to flush column family " + std::string( column_family_name( cf ) )
                              + " before compaction: " + status.ToString() );

  rocksdb::CompactRangeOptions compact_options;
  compact_options.change_level = false;
  status = _db->CompactRange( compact_options, cf_handle, nullptr, nullptr );
  if( !status.ok() )
    throw std::runtime_error( "failed to compact column family " + std::string( column_family_name( cf ) )
                              + ": " + status.ToString() );
}

void RocksDBManager::compact_all_column_families()
{
  for( std::size_t i = 0; i < _handles.size(); ++i )
    compact_column_family( static_cast< ColumnFamily >( i ) );
}

void RocksDBManager::initialize_storage_metadata( const std::filesystem::path& basedir )
{
  auto chain_storage = read_metadata( "layout.chain_storage" );
  const auto legacy_chain_db_path = basedir / "chain" / "blockchain";
  std::error_code legacy_chain_ec;
  const auto has_legacy_chain_db =
    std::filesystem::exists( legacy_chain_db_path / "CURRENT", legacy_chain_ec );
  legacy_chain_ec.clear();

  if( chain_storage == "migration-in-progress" )
    throw std::runtime_error( "storage migration is marked in progress; refusing to start until migration is repaired or rolled back" );
  if( !chain_storage.empty() && chain_storage != "legacy" && chain_storage != "unified" )
    throw std::runtime_error( "unknown chain storage layout: " + chain_storage );

  if( read_metadata( "layout.version" ).empty() )
    write_metadata( "layout.version", "1" );
  if( chain_storage.empty() )
  {
    chain_storage = has_legacy_chain_db ? "legacy" : "unified";
    write_metadata( "layout.chain_storage", chain_storage );
  }
  else if( chain_storage == "legacy" && !has_legacy_chain_db )
  {
    chain_storage = "unified";
    write_metadata( "layout.chain_storage", chain_storage );
  }
  if( read_metadata( "layout.created_at" ).empty() )
    write_metadata( "layout.created_at", iso8601_now() );
  if( read_metadata( "layout.network" ).empty() )
    write_metadata( "layout.network", "unknown" );

  write_metadata( "layout.created_by", std::string( node_name() ) + " " + build_version() );
  write_metadata( "layout.basedir", basedir.string() );
}

} // namespace koinos::node::storage
