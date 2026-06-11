/**
 * teleno_node — Monolithic Koinos blockchain node.
 *
 * Replaces 12 separate microservices + AMQP broker with a single binary.
 * Inter-service communication uses direct C++ function calls + EventBus
 * (boost::signals2) instead of serialized AMQP messages.
 *
 * Phase 0: Skeleton with EventBus, interfaces, config, lifecycle.
 * Phase 1: Chain, mempool, block_producer integrated via direct calls.
 */

#include <csignal>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>

#include <koinos/log.hpp>

#include "core/config.hpp"
#include "core/event_bus.hpp"
#include "core/node_version.hpp"
#include "core/service_registry.hpp"

#include "interfaces/i_block_store.hpp"
#include "interfaces/i_chain.hpp"
#include "interfaces/i_mempool.hpp"

// Phase 1: chain controller (internal, AMQP replaced by MonolithClient)
#include <koinos/chain/controller.hpp>
#include <koinos/chain/indexer.hpp>
#include "core/monolith_client.hpp"
#include "core/monolith_rpc_client.hpp"

// Phase 2: C++ block store
#include "block_store/block_store.hpp"

// Phase 3: JSON-RPC gateway
#include "jsonrpc/jsonrpc_server.hpp"

// Mempool
#include <koinos/mempool/mempool.hpp>
#include "mempool/mempool_adapter.hpp"

// Phase 5: P2P
#include "p2p/p2p_node.hpp"
#ifdef KOINOS_HAS_LIBP2P
#include "p2p/libp2p_transport.hpp"
#endif
#include "p2p/go_bridge_transport.hpp"

// Phase 6: gRPC + Account History
#include "grpc_server/grpc_server.hpp"
#include "account_history/account_history.hpp"

// Phase 4: Contract meta store + Transaction store
#include "contract_meta_store/contract_meta_store.hpp"
#include "transaction_store/transaction_store.hpp"

#include "block_production/block_producer.hpp"

#include <rocksdb/db.h>
#include <rocksdb/cache.h>
#include <rocksdb/convenience.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/options.h>
#include <rocksdb/slice_transform.h>
#include <rocksdb/table.h>

#if defined( __APPLE__ )
#include <mach/mach_init.h>
#include <mach/task.h>
#elif defined( __linux__ )
#include <unistd.h>
#endif

// Protobuf
#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/rpc/chain/chain_rpc.pb.h>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/rpc/mempool/mempool_rpc.pb.h>
#include <koinos/util/hex.hpp>

#include <google/protobuf/util/json_util.h>

#define HELP_OPTION    "help"
#define VERSION_OPTION "version"
#define BASEDIR_OPTION "basedir"
#define CONFIG_OPTION  "config"
#define LOG_LEVEL_OPTION "log-level"
#define ENABLE_OPTION  "enable"
#define DISABLE_OPTION "disable"
#define JOBS_OPTION    "jobs"
#define P2P_LISTEN_OPTION     "p2p-listen"
#define JSONRPC_LISTEN_OPTION "jsonrpc-listen"

namespace po = boost::program_options;
using namespace koinos;

namespace {

uint64_t current_process_rss_bytes()
{
#if defined( __APPLE__ )
  task_basic_info_data_t info;
  mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
  if( task_info( mach_task_self(), TASK_BASIC_INFO, reinterpret_cast< task_info_t >( &info ), &count ) == KERN_SUCCESS )
    return static_cast< uint64_t >( info.resident_size );
#elif defined( __linux__ )
  std::ifstream statm( "/proc/self/statm" );
  uint64_t pages = 0;
  if( statm >> pages )
  {
    const auto page_size = sysconf( _SC_PAGESIZE );
    if( page_size > 0 )
      return pages * static_cast< uint64_t >( page_size );
  }
#endif
  return 0;
}

std::pair< std::string, uint16_t > parse_jsonrpc_listen( const std::string& listen )
{
  if( listen.empty() )
    return { "0.0.0.0", 8080 };

  auto tcp_pos = listen.rfind( "/tcp/" );
  if( tcp_pos != std::string::npos )
  {
    auto host = std::string( "0.0.0.0" );
    auto ip4_pos = listen.find( "/ip4/" );
    if( ip4_pos != std::string::npos && ip4_pos < tcp_pos )
    {
      auto host_begin = ip4_pos + 5;
      host = listen.substr( host_begin, tcp_pos - host_begin );
    }

    return {
      host,
      static_cast< uint16_t >( std::stoi( listen.substr( tcp_pos + 5 ) ) )
    };
  }

  auto colon = listen.rfind( ':' );
  if( colon != std::string::npos )
  {
    return {
      listen.substr( 0, colon ),
      static_cast< uint16_t >( std::stoi( listen.substr( colon + 1 ) ) )
    };
  }

  return { "0.0.0.0", static_cast< uint16_t >( std::stoi( listen ) ) };
}

std::string lowercase( std::string value )
{
  std::transform( value.begin(), value.end(), value.begin(),
                  []( unsigned char ch ) { return static_cast< char >( std::tolower( ch ) ); } );
  return value;
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
    default:
      return std::to_string( static_cast< int >( compression ) );
  }
}

rocksdb::CompressionType select_supported_compression( const std::string& requested,
                                                       std::string& fallback_note )
{
  const auto token = lowercase( requested );
  std::vector< rocksdb::CompressionType > preferences;

  if( token == "none" || token == "no" || token == "disabled" || token == "off" )
    preferences = { rocksdb::kNoCompression };
  else if( token == "snappy" || token == "ksnappycompression" )
    preferences = { rocksdb::kSnappyCompression, rocksdb::kNoCompression };
  else
    preferences = { rocksdb::kZSTD, rocksdb::kSnappyCompression, rocksdb::kNoCompression };

  auto supported = rocksdb::GetSupportedCompressions();
  supported.push_back( rocksdb::kNoCompression );

  for( auto candidate: preferences )
  {
    if( std::find( supported.begin(), supported.end(), candidate ) != supported.end() )
    {
      if( !preferences.empty() && candidate != preferences.front() )
      {
        fallback_note = "requested " + requested + ", selected "
                        + compression_name( candidate ) + " because the requested codec is unsupported";
      }
      return candidate;
    }
  }

  fallback_note = "requested " + requested + ", selected none because no requested codec is supported";
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
                                   uint64_t max_bytes_for_level_base )
{
  cf.write_buffer_size                = static_cast< size_t >( write_buffer_size );
  cf.max_write_buffer_number          = static_cast< int >( std::max< uint64_t >( 1, max_write_buffer_number ) );
  cf.target_file_size_base            = target_file_size_base;
  cf.max_bytes_for_level_base         = max_bytes_for_level_base;
  cf.level_compaction_dynamic_level_bytes = true;
  cf.optimize_filters_for_hits        = true;
  cf.memtable_whole_key_filtering     = true;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// ChainAdapter — wraps chain::controller to implement IChain
// ---------------------------------------------------------------------------

class ChainAdapter final : public node::IChain
{
public:
  explicit ChainAdapter( chain::controller& ctrl ) : _ctrl( ctrl ) {}

  rpc::chain::submit_block_response
  submit_block( const rpc::chain::submit_block_request& req ) override
  {
    return _ctrl.submit_block( req );
  }

  rpc::chain::submit_transaction_response
  submit_transaction( const rpc::chain::submit_transaction_request& req ) override
  {
    return _ctrl.submit_transaction( req );
  }

  rpc::chain::get_head_info_response
  get_head_info( const rpc::chain::get_head_info_request& req ) override
  {
    return _ctrl.get_head_info( req );
  }

  rpc::chain::get_chain_id_response
  get_chain_id( const rpc::chain::get_chain_id_request& req ) override
  {
    return _ctrl.get_chain_id( req );
  }

  rpc::chain::get_fork_heads_response
  get_fork_heads( const rpc::chain::get_fork_heads_request& req ) override
  {
    return _ctrl.get_fork_heads( req );
  }

  rpc::chain::read_contract_response
  read_contract( const rpc::chain::read_contract_request& req ) override
  {
    return _ctrl.read_contract( req );
  }

  rpc::chain::get_account_nonce_response
  get_account_nonce( const rpc::chain::get_account_nonce_request& req ) override
  {
    return _ctrl.get_account_nonce( req );
  }

  rpc::chain::get_account_rc_response
  get_account_rc( const rpc::chain::get_account_rc_request& req ) override
  {
    return _ctrl.get_account_rc( req );
  }

  rpc::chain::get_resource_limits_response
  get_resource_limits( const rpc::chain::get_resource_limits_request& req ) override
  {
    return _ctrl.get_resource_limits( req );
  }

  rpc::chain::invoke_system_call_response
  invoke_system_call( const rpc::chain::invoke_system_call_request& req ) override
  {
    return _ctrl.invoke_system_call( req );
  }

  rpc::chain::propose_block_response
  propose_block( const rpc::chain::propose_block_request& req ) override
  {
    return _ctrl.propose_block( req );
  }

private:
  chain::controller& _ctrl;
};


// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main( int argc, char** argv )
{
  try
  {
    // ── CLI argument parsing ──
    po::options_description desc( "teleno_node options" );
    desc.add_options()
      ( HELP_OPTION ",h",    "Print help" )
      ( VERSION_OPTION ",v", "Print version" )
      ( BASEDIR_OPTION ",d", po::value< std::string >()->default_value( "" ),
        "Base data directory (default: ~/.koinos)" )
      ( CONFIG_OPTION ",c",  po::value< std::string >()->default_value( "" ),
        "Config file path (default: {basedir}/config.yml)" )
      ( LOG_LEVEL_OPTION ",l", po::value< std::string >()->default_value( "" ),
        "Log level override" )
      ( ENABLE_OPTION,  po::value< std::vector< std::string > >()->multitoken(),
        "Enable a component (repeatable)" )
      ( DISABLE_OPTION, po::value< std::vector< std::string > >()->multitoken(),
        "Disable a component (repeatable)" )
      ( JOBS_OPTION ",j", po::value< uint64_t >()->default_value( 0 ),
        "Chain worker threads" )
      ( P2P_LISTEN_OPTION,     po::value< std::string >()->default_value( "" ),
        "P2P listen address override" )
      ( JSONRPC_LISTEN_OPTION, po::value< std::string >()->default_value( "" ),
        "JSON-RPC listen address override" );

    po::variables_map vm;
    po::store( po::parse_command_line( argc, argv, desc ), vm );
    po::notify( vm );

    if( vm.count( HELP_OPTION ) )
    {
      std::cout << desc << std::endl;
      return EXIT_SUCCESS;
    }

    if( vm.count( VERSION_OPTION ) )
    {
      std::cout << node::node_name() << " " << node::build_version() << std::endl;
      return EXIT_SUCCESS;
    }

    // ── Resolve basedir ──
    std::filesystem::path basedir = vm[ BASEDIR_OPTION ].as< std::string >();
    if( basedir.empty() )
    {
      const char* home = std::getenv( "HOME" );
      basedir = home ? std::filesystem::path( home ) / ".koinos"
                     : std::filesystem::current_path() / ".koinos";
    }
    std::filesystem::create_directories( basedir );

    // ── Load config ──
    std::filesystem::path config_path = vm[ CONFIG_OPTION ].as< std::string >();
    if( config_path.empty() )
      config_path = basedir / "config.yml";

    auto enables  = vm.count( ENABLE_OPTION )  ? vm[ ENABLE_OPTION ].as< std::vector< std::string > >()  : std::vector< std::string >{};
    auto disables = vm.count( DISABLE_OPTION ) ? vm[ DISABLE_OPTION ].as< std::vector< std::string > >() : std::vector< std::string >{};

    node::NodeConfig cfg = node::load_config( config_path, enables, disables );
    cfg.basedir = basedir;

    // CLI overrides
    if( auto ll = vm[ LOG_LEVEL_OPTION ].as< std::string >(); !ll.empty() )
      cfg.log_level = ll;
    if( auto jobs = vm[ JOBS_OPTION ].as< uint64_t >(); jobs > 0 )
      cfg.chain_jobs = jobs;
    if( auto p2p = vm[ P2P_LISTEN_OPTION ].as< std::string >(); !p2p.empty() )
      cfg.p2p_listen = p2p;
    if( auto jrpc = vm[ JSONRPC_LISTEN_OPTION ].as< std::string >(); !jrpc.empty() )
      cfg.jsonrpc_listen = jrpc;

    // ── Initialize logging ──
    koinos::initialize_logging( node::node_name(), {}, cfg.log_level );
    LOG( info ) << "[node] " << node::node_name() << " v" << node::build_version() << " starting";
    LOG( info ) << "[node] basedir: " << basedir.string();
    LOG( info ) << "[node] config: " << config_path.string();

    // Log enabled features
    for( const auto& [name, enabled]: cfg.features )
    {
      if( enabled )
        LOG( info ) << "[features] " << name << ": enabled";
    }

    LOG( info ) << "[runtime] Thread topology: main_ioc=1"
                << " chain_jobs=" << std::max< uint64_t >( 1, cfg.chain_jobs )
                << " jsonrpc_session_limit=" << std::max< uint64_t >( 1, cfg.jsonrpc_jobs )
                << " grpc_pollers=" << std::max< uint64_t >( 1, cfg.grpc_jobs )
                << " p2p_requested_io_threads=" << std::max< uint64_t >( 1, cfg.p2p_jobs )
                << " p2p_effective_io_threads=1"
                << " p2p_sync_threads=per-peer"
                << " rocksdb_background_jobs=" << std::max< uint64_t >( 1, cfg.rocksdb_max_background_jobs );

    // ── Core objects ──
    node::EventBus event_bus;
    node::ServiceRegistry registry;

    // ── io_context instances (Phase 0 threading model) ──
    boost::asio::io_context main_ioc;
    boost::asio::io_context chain_ioc;

    // ── Phase 1: Chain component (requires chain library) ──

    chain::fork_resolution_algorithm fork_algo = chain::fork_resolution_algorithm::fifo;
    if( cfg.fork_algorithm == "pob" )
      fork_algo = chain::fork_resolution_algorithm::pob;
    else if( cfg.fork_algorithm == "block-time" )
      fork_algo = chain::fork_resolution_algorithm::block_time;

    chain::genesis_data genesis;
    {
      auto genesis_path = basedir / "chain" / "genesis_data.json";
      if( !std::filesystem::exists( genesis_path ) )
        genesis_path = basedir / "genesis_data.json";

      if( std::filesystem::exists( genesis_path ) )
      {
        std::ifstream ifs( genesis_path );
        std::string json_str( ( std::istreambuf_iterator< char >( ifs ) ),
                                std::istreambuf_iterator< char >() );
        google::protobuf::util::JsonStringToMessage( json_str, &genesis );
        LOG( info ) << "[chain] Loaded genesis data from " << genesis_path.string();
      }
      else
      {
        LOG( warning ) << "[chain] Genesis data not found at " << genesis_path.string();
      }
    }

    std::optional< uint64_t > pending_limit;
    if( !cfg.disable_pending_transaction_limit )
      pending_limit = cfg.pending_transaction_limit;

    chain::controller controller(
      cfg.read_compute_bandwidth_limit,
      64'000,
      pending_limit
    );

    auto state_dir = basedir / "chain" / "blockchain";
    std::filesystem::create_directories( state_dir );


    // ── Mempool ──
    koinos::mempool::mempool mempool_impl;
    node::MempoolAdapter mempool_adapter( mempool_impl );

    // ── Phase 2: RocksDB + Block Store ──
    rocksdb::DB* raw_db = nullptr;
    std::vector< rocksdb::ColumnFamilyHandle* > cf_handles;

    rocksdb::Options db_options;
    db_options.create_if_missing              = true;
    db_options.create_missing_column_families = true;
    db_options.max_background_jobs            = static_cast< int >( std::max< uint64_t >( 1, cfg.rocksdb_max_background_jobs ) );
    db_options.max_subcompactions             = static_cast< uint32_t >( std::max< uint64_t >( 1, cfg.rocksdb_max_background_jobs / 2 ) );
    db_options.bytes_per_sync                 = cfg.rocksdb_bytes_per_sync;
    db_options.db_write_buffer_size           = static_cast< size_t >( cfg.rocksdb_db_write_buffer_size );
    db_options.enable_pipelined_write         = true;

    auto db_path = basedir / "db";
    std::filesystem::create_directories( db_path );

    // ── Per-CF tuning (Phase 5/Sprint 5) ──
    auto shared_block_cache = rocksdb::NewLRUCache(
      static_cast< size_t >( cfg.rocksdb_block_cache_mb * 1024 * 1024 ),
      -1,
      false,
      0.35
    );

    std::string compression_fallback_note;
    auto blocks_compression = select_supported_compression( cfg.rocksdb_blocks_compression,
                                                            compression_fallback_note );

    // Default CF (chain state): small blocks, point lookups + iterators
    rocksdb::ColumnFamilyOptions cf_default;
    apply_point_lookup_cf_tuning( cf_default,
                                  cfg.rocksdb_write_buffer_size,
                                  cfg.rocksdb_max_write_buffer_number,
                                  cfg.rocksdb_target_file_size_base,
                                  cfg.rocksdb_max_bytes_for_level_base );
    auto default_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                  shared_block_cache,
                                                  true );
    cf_default.table_factory.reset( rocksdb::NewBlockBasedTableFactory( default_table_opts ) );

    // Blocks CF: large values (full blocks), zstd compression, bloom filters
    rocksdb::ColumnFamilyOptions cf_blocks;
    apply_point_lookup_cf_tuning( cf_blocks,
                                  cfg.rocksdb_write_buffer_size,
                                  cfg.rocksdb_max_write_buffer_number,
                                  cfg.rocksdb_target_file_size_base,
                                  cfg.rocksdb_max_bytes_for_level_base );
    auto blocks_table_opts = make_table_options( cfg.rocksdb_blocks_block_size,
                                                 shared_block_cache,
                                                 true );
    cf_blocks.table_factory.reset( rocksdb::NewBlockBasedTableFactory( blocks_table_opts ) );
    cf_blocks.compression           = blocks_compression;
    cf_blocks.bottommost_compression = blocks_compression;

    // Block meta CF: tiny (single key)
    rocksdb::ColumnFamilyOptions cf_block_meta;
    apply_point_lookup_cf_tuning( cf_block_meta,
                                  cfg.rocksdb_write_buffer_size,
                                  cfg.rocksdb_max_write_buffer_number,
                                  cfg.rocksdb_target_file_size_base,
                                  cfg.rocksdb_max_bytes_for_level_base );
    auto block_meta_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                     shared_block_cache,
                                                     true );
    cf_block_meta.table_factory.reset( rocksdb::NewBlockBasedTableFactory( block_meta_table_opts ) );

    // Contract meta CF: small values (ABI strings), bloom filters
    rocksdb::ColumnFamilyOptions cf_contract_meta;
    apply_point_lookup_cf_tuning( cf_contract_meta,
                                  cfg.rocksdb_write_buffer_size,
                                  cfg.rocksdb_max_write_buffer_number,
                                  cfg.rocksdb_target_file_size_base,
                                  cfg.rocksdb_max_bytes_for_level_base );
    auto contract_meta_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                        shared_block_cache,
                                                        true );
    cf_contract_meta.table_factory.reset( rocksdb::NewBlockBasedTableFactory( contract_meta_table_opts ) );

    // Transaction index CF: moderate, bloom filters for point lookups
    rocksdb::ColumnFamilyOptions cf_tx_index;
    apply_point_lookup_cf_tuning( cf_tx_index,
                                  cfg.rocksdb_write_buffer_size,
                                  cfg.rocksdb_max_write_buffer_number,
                                  cfg.rocksdb_target_file_size_base,
                                  cfg.rocksdb_max_bytes_for_level_base );
    auto tx_index_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                   shared_block_cache,
                                                   true );
    cf_tx_index.table_factory.reset( rocksdb::NewBlockBasedTableFactory( tx_index_table_opts ) );

    // Account history CF: prefix range scans with prefix Bloom filters
    rocksdb::ColumnFamilyOptions cf_acct_history;
    apply_point_lookup_cf_tuning( cf_acct_history,
                                  cfg.rocksdb_write_buffer_size,
                                  cfg.rocksdb_max_write_buffer_number,
                                  cfg.rocksdb_target_file_size_base,
                                  cfg.rocksdb_max_bytes_for_level_base );
    cf_acct_history.prefix_extractor.reset( rocksdb::NewFixedPrefixTransform( 34 ) ); // address length
    cf_acct_history.memtable_whole_key_filtering = false;
    auto acct_history_table_opts = make_table_options( cfg.rocksdb_default_block_size,
                                                       shared_block_cache,
                                                       true,
                                                       false );
    cf_acct_history.table_factory.reset( rocksdb::NewBlockBasedTableFactory( acct_history_table_opts ) );

    LOG( info ) << "[db] RocksDB tuning: block_cache_mb=" << cfg.rocksdb_block_cache_mb
                << " max_background_jobs=" << db_options.max_background_jobs
                << " max_subcompactions=" << db_options.max_subcompactions
                << " default_block_size=" << cfg.rocksdb_default_block_size
                << " blocks_block_size=" << cfg.rocksdb_blocks_block_size
                << " blocks_compression=" << compression_name( blocks_compression );
    if( !compression_fallback_note.empty() )
      LOG( info ) << "[db] RocksDB compression fallback: " << compression_fallback_note;

    // Column families
    std::vector< rocksdb::ColumnFamilyDescriptor > cf_descriptors = {
      { rocksdb::kDefaultColumnFamilyName, cf_default },      // 0: chain state
      { "blocks",            cf_blocks },                      // 1: block store
      { "block_meta",        cf_block_meta },                  // 2: block metadata
      { "contract_meta",     cf_contract_meta },               // 3: contract ABI
      { "transaction_index", cf_tx_index },                    // 4: tx index
      { "account_history",   cf_acct_history }                 // 5: account history
    };

    auto db_status = rocksdb::DB::Open( db_options, db_path.string(), cf_descriptors, &cf_handles, &raw_db );
    if( !db_status.ok() )
    {
      LOG( error ) << "[db] Failed to open RocksDB at " << db_path.string() << ": " << db_status.ToString();
      return EXIT_FAILURE;
    }
    std::unique_ptr< rocksdb::DB > db( raw_db );
    LOG( info ) << "[db] RocksDB opened at " << db_path.string()
                << " with " << cf_handles.size() << " column families";

    // Block store using RocksDB column families
    node::block_store::BlockStore block_store_impl( raw_db, cf_handles[ 1 ], cf_handles[ 2 ] );

    // Phase 4: Contract meta store + Transaction store
    node::contract_meta_store::ContractMetaStore contract_meta_impl( raw_db, cf_handles[ 3 ] );
    node::transaction_store::TransactionStore transaction_store_impl( raw_db, cf_handles[ 4 ] );
    node::account_history::AccountHistory account_history_impl( raw_db, cf_handles[ 5 ] );

    // Chain adapter implements IChain
    ChainAdapter chain_adapter( controller );

    // Register block store component
    if( cfg.is_enabled( "block_store" ) )
    {
      registry.add(
        "block_store",
        [&]() {
          block_store_impl.initialize();
          LOG( info ) << "[block_store] Initialized";
        },
        [&]() {
          LOG( info ) << "[block_store] Stopped";
        }
      );
    }

    // Backup restore detection — temporarily force verify-blocks
    auto backup_marker = basedir / ".backup-just-restored";
    bool force_verify  = false;
    if( std::filesystem::exists( backup_marker ) )
    {
      LOG( info ) << "[chain] Backup restore detected — enabling verify-blocks for merkle correction";
      force_verify = true;
      std::filesystem::remove( backup_marker );
    }

    bool effective_verify = cfg.verify_blocks || force_verify;

    // Register chain component
    if( cfg.is_enabled( "chain" ) )
    {
      registry.add(
        "chain",
        [&]() {
          controller.open( state_dir, genesis, fork_algo, cfg.reset );
          LOG( info ) << "[chain] State DB opened at " << state_dir.string();
        },
        [&]() {
          controller.close();
        }
      );
    }

    // Register block producer (optional)
    // The monolith assembles and signs blocks locally, then submits them
    // through chain.propose_block(), which applies the block and broadcasts
    // block acceptance through the in-process EventBus.
    std::atomic< bool > producer_running{ false };
    std::atomic< bool > producer_gossip_enabled{ false };
    std::thread producer_thread;
    std::unique_ptr< node::block_production::BlockProducer > block_producer;

    if( cfg.is_enabled( "block_producer" ) )
    {
      // Validate producer config
      std::filesystem::path key_file = cfg.block_producer_private_key_file;
      if( key_file.empty() )
      {
        key_file = basedir / "block_producer" / "private.key";
      }
      else if( key_file.is_relative() )
      {
        if( key_file.has_parent_path() )
        {
          key_file = basedir / key_file;
        }
        else
        {
          auto block_producer_key_file = basedir / "block_producer" / key_file;
          auto basedir_key_file = basedir / key_file;
          key_file = std::filesystem::exists( block_producer_key_file ) ? block_producer_key_file
                   : std::filesystem::exists( basedir_key_file ) ? basedir_key_file
                   : block_producer_key_file;
        }
      }

      node::block_production::ProducerConfig producer_cfg;
      producer_cfg.algorithm                = cfg.block_producer_algorithm;
      producer_cfg.producer_address         = cfg.block_producer_address;
      producer_cfg.resources_lower_bound    = cfg.block_producer_resources_lower_bound;
      producer_cfg.resources_upper_bound    = cfg.block_producer_resources_upper_bound;
      producer_cfg.max_inclusion_attempts   = cfg.block_producer_max_inclusion_attempts;

      for( const auto& proposal: cfg.block_producer_approved_proposals )
        producer_cfg.approved_proposals.push_back( util::from_hex< std::string >( proposal ) );

      auto signing_key = node::block_production::load_or_create_private_key_file( key_file );
      block_producer = std::make_unique< node::block_production::BlockProducer >(
        chain_adapter, mempool_adapter, std::move( signing_key ), producer_cfg );
      block_producer->write_public_key_file( basedir / "block_producer" / "public.key" );

      LOG( info ) << "[block_producer] Private key: " << key_file;
      LOG( info ) << "[block_producer] Public address: " << block_producer->public_address();
      if( !cfg.block_producer_address.empty() )
        LOG( info ) << "[block_producer] Producer address: " << cfg.block_producer_address;
      LOG( info ) << "[block_producer] Algorithm: " << cfg.block_producer_algorithm;
      LOG( info ) << "[block_producer] Resource utilization lower/upper bounds: "
                  << cfg.block_producer_resources_lower_bound << "%/"
                  << cfg.block_producer_resources_upper_bound << "%";
      LOG( info ) << "[block_producer] Max inclusion attempts: "
                  << cfg.block_producer_max_inclusion_attempts;
      LOG( info ) << "[block_producer] Gossip production gate: "
                  << ( cfg.block_producer_gossip_production ? "enabled" : "disabled" );

      registry.add(
        "block_producer",
        [&]() {
          producer_running = true;
          producer_gossip_enabled = !cfg.block_producer_gossip_production;
          producer_thread  = std::thread( [&]() {
              LOG( info ) << "[block_producer] Production loop started";
              uint64_t blocks_produced = 0;
              while( producer_running )
              {
                std::chrono::milliseconds retry_after{ 5000 };

                if( cfg.block_producer_gossip_production && !producer_gossip_enabled )
                {
                  std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
                  continue;
                }

                try
                {
                  auto result = block_producer->produce_once();
                  retry_after = result.retry_after;

                  if( result.removed_failed_transactions > 0 )
                  {
                    LOG( info ) << "[block_producer] Removed "
                                << result.removed_failed_transactions
                                << " failed transaction(s) before acceptance";
                  }

                  if( result.status == node::block_production::ProductionResult::Status::produced )
                  {
                    ++blocks_produced;
                    LOG( info ) << "[block_producer] Produced block #" << blocks_produced
                                << " at height " << result.height;
                  }
                }
                catch( const std::exception& e )
                {
                  retry_after = std::chrono::seconds( 5 );
                  LOG( warning ) << "[block_producer] " << e.what();
                }

                auto sleep_ms = std::max< int64_t >( 10, retry_after.count() );
                for( int64_t slept = 0; slept < sleep_ms && producer_running; slept += 100 )
                {
                  auto chunk = std::min< int64_t >( 100, sleep_ms - slept );
                  std::this_thread::sleep_for( std::chrono::milliseconds( chunk ) );
                }
              }
              LOG( info ) << "[block_producer] Stopped (produced " << blocks_produced << " blocks)";
            } );
          },
          [&]() {
            producer_running = false;
            if( producer_thread.joinable() )
              producer_thread.join();
        }
      );
    }

    // ── EventBus wiring ──

    // Block accepted → block store, mempool (future), p2p (future)
    event_bus.on_block_accepted.connect(
      [&block_store_impl]( const broadcast::block_accepted& ba ) {
        block_store_impl.handle_block_accepted( ba );
      }
    );

    // Contract meta store subscribes to block events
    if( cfg.is_enabled( "contract_meta_store" ) )
    {
      event_bus.on_block_accepted.connect(
        [&contract_meta_impl]( const broadcast::block_accepted& ba ) {
          contract_meta_impl.handle_block_accepted( ba );
        }
      );
    }

    // Transaction store subscribes to block events
    if( cfg.is_enabled( "transaction_store" ) )
    {
      event_bus.on_block_accepted.connect(
        [&transaction_store_impl]( const broadcast::block_accepted& ba ) {
          transaction_store_impl.handle_block_accepted( ba );
        }
      );
    }

    // Account history subscribes to block events
    if( cfg.is_enabled( "account_history" ) )
    {
      event_bus.on_block_accepted.connect(
        [&account_history_impl]( const broadcast::block_accepted& ba ) {
          account_history_impl.handle_block_accepted( ba );
        }
      );
    }

    // Mempool subscribes to accepted transactions, block events, and local expiry.
    boost::asio::steady_timer mempool_prune_timer( main_ioc );
    std::function< void( const boost::system::error_code& ) > mempool_prune_tick;
    if( cfg.is_enabled( "mempool" ) )
    {
      event_bus.on_transaction_accepted.connect(
        [&mempool_adapter]( const broadcast::transaction_accepted& ta ) {
          try
          {
            auto rc_used = mempool_adapter.add_transaction_accepted( ta );
            LOG( debug ) << "[mempool] accepted transaction id="
                         << util::to_hex( ta.transaction().id() )
                         << " reserved_rc=" << rc_used;
          }
          catch( const std::exception& e )
          {
            LOG( warning ) << "[mempool] failed to add accepted transaction: " << e.what();
          }
        }
      );
      event_bus.on_block_accepted.connect(
        [&mempool_impl]( const broadcast::block_accepted& ba ) {
          mempool_impl.handle_block( ba );
        }
      );
      event_bus.on_block_irreversible.connect(
        [&mempool_impl]( const broadcast::block_irreversible& bi ) {
          mempool_impl.handle_irreversibility( bi );
        }
      );

      mempool_prune_tick = [&]( const boost::system::error_code& ec ) {
        if( ec )
          return;

        try
        {
          auto pruned = mempool_adapter.prune( std::chrono::seconds( cfg.mempool_transaction_expiration ) );
          if( pruned )
            LOG( debug ) << "[mempool] pruned expired pending transactions count=" << pruned;
        }
        catch( const std::exception& e )
        {
          LOG( warning ) << "[mempool] prune failed: " << e.what();
        }

        mempool_prune_timer.expires_after( std::chrono::seconds( 1 ) );
        mempool_prune_timer.async_wait( mempool_prune_tick );
      };
      mempool_prune_timer.expires_after( std::chrono::seconds( 1 ) );
      mempool_prune_timer.async_wait( mempool_prune_tick );
    }

    event_bus.on_block_accepted.connect(
      [&]( const broadcast::block_accepted& ba ) {
        LOG( debug ) << "[event_bus] block_accepted height="
                     << ba.block().header().height();
      }
    );

    event_bus.on_block_irreversible.connect(
      [&]( const broadcast::block_irreversible& bi ) {
        LOG( debug ) << "[event_bus] block_irreversible height="
                     << bi.topology().height();
      }
    );

    event_bus.on_gossip_status.connect(
      [&]( bool enabled ) {
        producer_gossip_enabled = enabled;
        LOG( info ) << "[block_producer] Gossip production gate "
                    << ( enabled ? "opened" : "closed" );
      }
    );

    // ── Phase 5: P2P ──
    std::unique_ptr< node::p2p::P2PNode > p2p_node;
    if( cfg.is_enabled( "p2p" ) )
    {
#ifdef KOINOS_HAS_LIBP2P
      // cpp-libp2p transport available — create real P2P node
	      node::p2p::Libp2pTransport::Config transport_cfg;
	      transport_cfg.listen_address = cfg.p2p_listen;
	      transport_cfg.discovery_peers = cfg.p2p_seeds;
	      transport_cfg.enable_dht = cfg.p2p_peer_discovery_enabled;
	      transport_cfg.requested_io_threads = static_cast< unsigned int >( std::max< uint64_t >( 1, cfg.p2p_jobs ) );

      auto transport = std::make_unique< node::p2p::Libp2pTransport >( transport_cfg );

	      node::p2p::P2POptions p2p_opts;
	      p2p_opts.seed_reconnect_interval = std::chrono::seconds( cfg.p2p_seed_reconnect_interval_seconds );
	      p2p_opts.peer_discovery_enabled   = cfg.p2p_peer_discovery_enabled;
	      p2p_opts.target_peer_count        = static_cast< uint32_t >( cfg.p2p_target_peer_count );
	      p2p_opts.max_peer_candidates      = static_cast< uint32_t >( cfg.p2p_max_peer_candidates );
	      p2p_opts.max_candidate_dials_per_cycle =
	        static_cast< uint32_t >( cfg.p2p_max_candidate_dials_per_cycle );
	      p2p_opts.peer_acquisition_interval = std::chrono::seconds( cfg.p2p_peer_acquisition_interval_seconds );
	      p2p_opts.candidate_redial_interval = std::chrono::seconds( cfg.p2p_candidate_redial_interval_seconds );
	      p2p_opts.always_enable_gossip    = cfg.p2p_force_gossip;
	      p2p_opts.always_disable_gossip   = cfg.p2p_disable_gossip;
      for( const auto& checkpoint: cfg.p2p_checkpoints )
        p2p_opts.checkpoints.push_back( { checkpoint.block_height, checkpoint.block_id } );
      for( const auto& seed: cfg.p2p_seeds )
      {
        auto marker = seed.rfind( "/p2p/" );
        if( marker != std::string::npos )
          p2p_opts.seed_peers.push_back( { seed.substr( marker + 5 ), seed } );
      }
      p2p_node = std::make_unique< node::p2p::P2PNode >(
        p2p_opts, &chain_adapter, &block_store_impl, &event_bus, std::move( transport ) );

      registry.add(
        "p2p",
        [&]() { p2p_node->start(); },
        [&]() { p2p_node->stop(); }
      );
#else
      // Fallback: use Go P2P binary as sidecar process
      node::p2p::GoBridgeTransport::Config go_cfg;
      // Look for Go P2P binary in standard locations
      auto go_p2p_path = basedir / ".." / "koinos-p2p" / "build" / "bin" / "koinos-p2p";
      if( !std::filesystem::exists( go_p2p_path ) )
        go_p2p_path = std::filesystem::path( "/usr/local/bin/koinos-p2p" );

      go_cfg.go_p2p_binary  = go_p2p_path.string();
      go_cfg.basedir        = basedir.string();
      go_cfg.listen_address = cfg.p2p_listen;
      go_cfg.seed_peers     = cfg.p2p_seeds;

      auto transport = std::make_unique< node::p2p::GoBridgeTransport >( go_cfg );

	      node::p2p::P2POptions p2p_opts;
	      p2p_opts.seed_reconnect_interval = std::chrono::seconds( cfg.p2p_seed_reconnect_interval_seconds );
	      p2p_opts.peer_discovery_enabled   = cfg.p2p_peer_discovery_enabled;
	      p2p_opts.target_peer_count        = static_cast< uint32_t >( cfg.p2p_target_peer_count );
	      p2p_opts.max_peer_candidates      = static_cast< uint32_t >( cfg.p2p_max_peer_candidates );
	      p2p_opts.max_candidate_dials_per_cycle =
	        static_cast< uint32_t >( cfg.p2p_max_candidate_dials_per_cycle );
	      p2p_opts.peer_acquisition_interval = std::chrono::seconds( cfg.p2p_peer_acquisition_interval_seconds );
	      p2p_opts.candidate_redial_interval = std::chrono::seconds( cfg.p2p_candidate_redial_interval_seconds );
	      p2p_opts.always_enable_gossip    = cfg.p2p_force_gossip;
	      p2p_opts.always_disable_gossip   = cfg.p2p_disable_gossip;
      for( const auto& checkpoint: cfg.p2p_checkpoints )
        p2p_opts.checkpoints.push_back( { checkpoint.block_height, checkpoint.block_id } );
      for( const auto& seed: cfg.p2p_seeds )
      {
        auto marker = seed.rfind( "/p2p/" );
        if( marker != std::string::npos )
          p2p_opts.seed_peers.push_back( { seed.substr( marker + 5 ), seed } );
      }
      p2p_node = std::make_unique< node::p2p::P2PNode >(
        p2p_opts, &chain_adapter, &block_store_impl, &event_bus, std::move( transport ) );

      registry.add(
        "p2p",
        [&]() { p2p_node->start(); },
        [&]() { p2p_node->stop(); }
      );
#endif
    }

    // ── Phase 6: gRPC server ──
    std::unique_ptr< node::grpc_server::GRPCServer > grpc_srv;
    if( cfg.is_enabled( "grpc" ) )
    {
      grpc_srv = std::make_unique< node::grpc_server::GRPCServer >(
        &chain_adapter,
        &mempool_adapter,
        &block_store_impl,
        cfg.is_enabled( "contract_meta_store" ) ? &contract_meta_impl : nullptr,
        cfg.is_enabled( "transaction_store" ) ? &transaction_store_impl : nullptr,
        cfg.is_enabled( "account_history" ) ? &account_history_impl : nullptr,
        cfg.grpc_listen,
        static_cast< unsigned int >( cfg.grpc_jobs ),
        &producer_gossip_enabled );
      registry.add(
        "grpc",
        [&]() { grpc_srv->start(); },
        [&]() { grpc_srv->stop(); }
      );
    }

    // ── Phase 3: JSON-RPC server ──
    // Parse listen address: "host:port", "port", or legacy multiaddr
    // values such as "/tcp/8080" and "/ip4/0.0.0.0/tcp/8080".
    auto [jsonrpc_host, jsonrpc_port] = parse_jsonrpc_listen( cfg.jsonrpc_listen );

    // Wire MonolithClient so the chain controller routes RPC/broadcast
    // through IBlockStore + EventBus instead of AMQP
    auto monolith_client = std::make_shared< node::MonolithRpcClient >(
      &block_store_impl, &mempool_adapter, &event_bus );
    controller.set_client( monolith_client );

    // Indexer will run after registry.start_all() — see below

    std::unique_ptr< node::jsonrpc::JSONRPCServer > jsonrpc_server;
    if( cfg.is_enabled( "jsonrpc" ) )
    {
      jsonrpc_server = std::make_unique< node::jsonrpc::JSONRPCServer >(
        &chain_adapter,
        &mempool_adapter,
        &block_store_impl,
        cfg.is_enabled( "contract_meta_store" ) ? &contract_meta_impl : nullptr,
        cfg.is_enabled( "transaction_store" ) ? &transaction_store_impl : nullptr,
        cfg.is_enabled( "account_history" ) ? &account_history_impl : nullptr,
        jsonrpc_host,
        jsonrpc_port,
        static_cast< unsigned int >( cfg.jsonrpc_jobs ),
        node::RpcAccessPolicy{ cfg.rpc_blacklist, cfg.rpc_whitelist }
      );

      registry.add(
        "jsonrpc",
        [&]() { jsonrpc_server->start(); },
        [&]() { jsonrpc_server->stop(); }
      );
    }

    // ── Signal handling ──
    boost::asio::signal_set signals( main_ioc, SIGINT, SIGTERM );
    signals.async_wait( [&]( const boost::system::error_code& ec, int sig ) {
      if( !ec )
      {
        LOG( info ) << "[node] Received signal " << sig << ", shutting down...";
        registry.stop_all();
        main_ioc.stop();
      }
    } );

    // ── Start all registered components ──
    registry.start_all();

    // Run chain indexer AFTER all components are started (chain + block_store must be open)
    if( cfg.is_enabled( "chain" ) && cfg.is_enabled( "block_store" ) )
    {
      try
      {
        chain::indexer idx( chain_ioc, controller, monolith_client, effective_verify );
        auto future = idx.index();
        if( future.get() )
          LOG( info ) << "[chain] Indexing complete";
        else
          LOG( warning ) << "[chain] Indexing returned false";
      }
      catch( const std::exception& e )
      {
        LOG( warning ) << "[chain] Indexer: " << e.what();
      }
    }

    LOG( info ) << "[node] " << node::node_name() << " ready";

    // ── Periodic metrics (every 60s) ──
    boost::asio::steady_timer metrics_timer( main_ioc );
    std::function< void( const boost::system::error_code& ) > metrics_tick;
    bool have_metrics_baseline = false;
    uint64_t previous_metrics_height = 0;
    auto previous_metrics_time = std::chrono::steady_clock::now();
    metrics_tick = [&]( const boost::system::error_code& ec ) {
      if( ec )
        return;

      try
      {
        const auto now = std::chrono::steady_clock::now();
        auto head = controller.get_head_info();
        const auto height = head.head_topology().height();
        double blocks_per_sec = 0.0;
        if( have_metrics_baseline )
        {
          const auto elapsed = std::chrono::duration< double >( now - previous_metrics_time ).count();
          if( elapsed > 0.0 && height >= previous_metrics_height )
            blocks_per_sec = static_cast< double >( height - previous_metrics_height ) / elapsed;
        }

        previous_metrics_height = height;
        previous_metrics_time = now;
        have_metrics_baseline = true;

        const auto peer_count = p2p_node ? p2p_node->connected_peer_count() : 0;
        const auto rss_bytes = current_process_rss_bytes();
        const auto rss_mb = static_cast< double >( rss_bytes ) / ( 1024.0 * 1024.0 );

        LOG( info ) << "[metrics] head_height=" << head.head_topology().height()
                    << " lib=" << head.last_irreversible_block()
                    << " blocks_per_sec=" << std::fixed << std::setprecision( 3 ) << blocks_per_sec
                    << " pending_txs=" << mempool_impl.get_pending_transactions( 0 ).size()
                    << " peer_count=" << peer_count
                    << " rss_bytes=" << rss_bytes
                    << " rss_mb=" << std::fixed << std::setprecision( 3 ) << rss_mb
                    << " components=" << registry.components().size();
      }
      catch( const std::exception& e )
      {
        LOG( warning ) << "[metrics] failed to collect metrics: " << e.what();
      }
      catch( ... )
      {
        LOG( warning ) << "[metrics] failed to collect metrics: unknown exception";
      }

      metrics_timer.expires_after( std::chrono::seconds( 60 ) );
      metrics_timer.async_wait( metrics_tick );
    };
    metrics_tick( boost::system::error_code{} );

    // ── Run main event loop ──
    main_ioc.run();

    // Clean up RocksDB column family handles
    for( auto* h: cf_handles )
      delete h;

    LOG( info ) << "[node] " << node::node_name() << " shutdown complete";
    return EXIT_SUCCESS;
  }
  catch( const std::exception& e )
  {
    std::cerr << "Fatal: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  catch( ... )
  {
    std::cerr << "Fatal: unknown exception" << std::endl;
    return EXIT_FAILURE;
  }
}
