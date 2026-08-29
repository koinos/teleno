#include "block_store/block_store.hpp"
#include "core/monolith_client.hpp"
#include "koinos/chain/controller.hpp"
#include "koinos/chain/indexer.hpp"
#include "storage/rocksdb_manager.hpp"

#include <google/protobuf/util/json_util.h>

#include <koinos/chain/system_call_ids.pb.h>
#include <koinos/contracts/name_service/name_service.pb.h>
#include <koinos/crypto/multihash.hpp>
#include <koinos/log.hpp>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/util/hex.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/services.hpp>

#include <rocksdb/db.h>

#include <boost/asio/io_context.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using koinos::node::storage::ColumnFamily;

struct Args
{
  std::filesystem::path source_db;
  std::filesystem::path scratch_state_dir;
  std::filesystem::path genesis_file;
  uint64_t target_height = 0;
  std::optional< uint64_t > dump_block_json;
  uint32_t threads       = std::max( 1u, std::thread::hardware_concurrency() );
  bool verify_blocks     = false;
  bool json              = false;
  bool help              = false;
};

void usage( const char* argv0 )
{
  std::cout
    << "Usage:\n"
    << "  " << argv0
    << " --source-db PATH --scratch-state-dir PATH --genesis PATH --target-height N [options]\n"
    << "  " << argv0 << " --source-db PATH --dump-block-json N\n\n"
    << "Options:\n"
    << "  --verify-blocks       Fully execute every block instead of checked receipt replay.\n"
    << "  --dump-block-json N   Print canonical protobuf JSON for block N and exit.\n"
    << "  --threads N           io_context worker count. Default: hardware concurrency.\n"
    << "  --json                Print the final result as JSON.\n"
    << "  --help                Show this help.\n\n"
    << "The source unified RocksDB is opened read-only. Only the explicit scratch\n"
    << "state directory is mutated. Prepare that directory at a canonical checkpoint\n"
    << "with koinos_state_delta_replay_audit --state-db-replay before running this tool.\n";
}

uint64_t parse_u64( const std::string& name, const std::string& value )
{
  std::size_t parsed = 0;
  uint64_t result    = 0;
  try
  {
    result = std::stoull( value, &parsed );
  }
  catch( const std::exception& e )
  {
    throw std::runtime_error( "invalid " + name + " value '" + value + "': " + e.what() );
  }
  if( parsed != value.size() )
    throw std::runtime_error( "invalid " + name + " value '" + value + "'" );
  return result;
}

Args parse_args( int argc, char** argv )
{
  Args args;
  for( int i = 1; i < argc; ++i )
  {
    const std::string arg = argv[ i ];
    auto require_value = [&]( const std::string& name ) -> std::string
    {
      if( i + 1 >= argc )
        throw std::runtime_error( name + " requires a value" );
      return argv[ ++i ];
    };

    if( arg == "--source-db" )
      args.source_db = require_value( arg );
    else if( arg == "--scratch-state-dir" )
      args.scratch_state_dir = require_value( arg );
    else if( arg == "--genesis" )
      args.genesis_file = require_value( arg );
    else if( arg == "--target-height" )
      args.target_height = parse_u64( arg, require_value( arg ) );
    else if( arg == "--dump-block-json" )
      args.dump_block_json = parse_u64( arg, require_value( arg ) );
    else if( arg == "--threads" )
    {
      const auto threads = parse_u64( arg, require_value( arg ) );
      if( threads == 0 || threads > std::numeric_limits< uint32_t >::max() )
        throw std::runtime_error( "--threads is out of range" );
      args.threads = static_cast< uint32_t >( threads );
    }
    else if( arg == "--verify-blocks" )
      args.verify_blocks = true;
    else if( arg == "--json" )
      args.json = true;
    else if( arg == "--help" || arg == "-h" )
      args.help = true;
    else
      throw std::runtime_error( "unknown argument: " + arg );
  }

  if( args.help )
    return args;
  if( args.source_db.empty() )
    throw std::runtime_error( "--source-db is required" );
  if( !args.dump_block_json
      && ( args.scratch_state_dir.empty() || args.genesis_file.empty() || args.target_height == 0 ) )
  {
    throw std::runtime_error(
      "--source-db, --scratch-state-dir, --genesis, and --target-height are required" );
  }

  args.source_db        = std::filesystem::absolute( args.source_db ).lexically_normal();
  if( !std::filesystem::exists( args.source_db / "CURRENT" ) )
    throw std::runtime_error( "source unified RocksDB does not exist" );
  if( !args.dump_block_json )
  {
    args.scratch_state_dir = std::filesystem::absolute( args.scratch_state_dir ).lexically_normal();
    args.genesis_file      = std::filesystem::absolute( args.genesis_file ).lexically_normal();
    if( args.source_db == args.scratch_state_dir )
      throw std::runtime_error( "source and scratch directories must differ" );
    if( !std::filesystem::exists( args.scratch_state_dir / "CURRENT" ) )
      throw std::runtime_error( "prepared scratch state RocksDB does not exist" );
    if( !std::filesystem::is_regular_file( args.genesis_file ) )
      throw std::runtime_error( "genesis file does not exist" );
  }
  return args;
}

koinos::chain::genesis_data load_genesis( const std::filesystem::path& path )
{
  std::ifstream input( path );
  if( !input )
    throw std::runtime_error( "could not open genesis data" );
  const std::string json( ( std::istreambuf_iterator< char >( input ) ),
                          std::istreambuf_iterator< char >() );
  koinos::chain::genesis_data genesis;
  const auto status = google::protobuf::util::JsonStringToMessage( json, &genesis );
  if( !status.ok() )
    throw std::runtime_error( "could not parse genesis data: " + status.ToString() );
  return genesis;
}

class ReadOnlyUnifiedDB
{
public:
  ~ReadOnlyUnifiedDB()
  {
    for( auto* handle: _handles )
      delete handle;
  }

  void open( const std::filesystem::path& path )
  {
    rocksdb::Options options;
    options.create_if_missing = false;
    std::vector< rocksdb::ColumnFamilyDescriptor > descriptors;
    for( std::size_t i = 0; i <= static_cast< std::size_t >( ColumnFamily::storage_metadata ); ++i )
    {
      descriptors.emplace_back(
        koinos::node::storage::column_family_name( static_cast< ColumnFamily >( i ) ),
        rocksdb::ColumnFamilyOptions() );
    }

    rocksdb::DB* raw = nullptr;
    const auto status = rocksdb::DB::OpenForReadOnly(
      options, path.string(), descriptors, &_handles, &raw );
    if( !status.ok() )
      throw std::runtime_error( "failed to open source RocksDB read-only: " + status.ToString() );
    _db.reset( raw );
  }

  rocksdb::DB* db() const { return _db.get(); }

  rocksdb::ColumnFamilyHandle* handle( ColumnFamily family ) const
  {
    const auto index = static_cast< std::size_t >( family );
    if( index >= _handles.size() )
      throw std::out_of_range( "column family handle index out of range" );
    return _handles[ index ];
  }

private:
  std::unique_ptr< rocksdb::DB > _db;
  std::vector< rocksdb::ColumnFamilyHandle* > _handles;
};

class ReadOnlyBlockStoreClient final: public koinos::node::IRpcClient
{
public:
  ReadOnlyBlockStoreClient( koinos::node::block_store::BlockStore& block_store,
                            uint64_t target_height ):
      _block_store( block_store )
  {
    const auto highest = _block_store.get_highest_block(
      koinos::rpc::block_store::get_highest_block_request{} );
    if( !highest.has_topology() || highest.topology().height() < target_height )
      throw std::runtime_error( "source block store does not reach target height" );

    koinos::rpc::block_store::get_blocks_by_height_request request;
    request.set_head_block_id( highest.topology().id() );
    request.set_ancestor_start_height( target_height );
    request.set_num_blocks( 1 );
    request.set_return_block( true );
    const auto response = _block_store.get_blocks_by_height( request );
    if( response.block_items_size() != 1 || !response.block_items( 0 ).has_block() )
      throw std::runtime_error( "could not resolve canonical target block" );

    const auto& block = response.block_items( 0 ).block();
    _target.set_id( block.id() );
    _target.set_height( block.header().height() );
    _target.set_previous( block.header().previous() );
  }

  std::shared_future< std::string >
  rpc( const std::string& service,
       const std::string& payload,
       std::chrono::milliseconds,
       retry_policy ) override
  {
    if( service != koinos::util::service::block_store )
      throw std::runtime_error( "historical validator permits only block-store RPC" );

    koinos::rpc::block_store::block_store_request request;
    if( !request.ParseFromString( payload ) )
      throw std::runtime_error( "could not parse block-store request" );

    koinos::rpc::block_store::block_store_response response;
    if( request.has_get_highest_block() )
    {
      *response.mutable_get_highest_block()->mutable_topology() = _target;
    }
    else if( request.has_get_blocks_by_height() )
    {
      *response.mutable_get_blocks_by_height() =
        _block_store.get_blocks_by_height( request.get_blocks_by_height() );
    }
    else if( request.has_get_blocks_by_id() )
    {
      *response.mutable_get_blocks_by_id() =
        _block_store.get_blocks_by_id( request.get_blocks_by_id() );
    }
    else
    {
      ++_write_attempts;
      throw std::runtime_error( "historical validator rejected a block-store write attempt" );
    }

    std::promise< std::string > promise;
    promise.set_value( response.SerializeAsString() );
    return promise.get_future().share();
  }

  void broadcast( const std::string&, const std::string& ) override
  {
    ++_broadcast_attempts;
    throw std::runtime_error( "historical validator rejected a broadcast attempt" );
  }

  uint64_t write_attempts() const { return _write_attempts; }
  uint64_t broadcast_attempts() const { return _broadcast_attempts; }

private:
  koinos::node::block_store::BlockStore& _block_store;
  koinos::block_topology _target;
  std::atomic< uint64_t > _write_attempts{ 0 };
  std::atomic< uint64_t > _broadcast_attempts{ 0 };
};

void print_json_string( const std::string& value )
{
  std::cout << '"';
  for( const auto ch: value )
  {
    if( ch == '\\' || ch == '"' )
      std::cout << '\\';
    std::cout << ch;
  }
  std::cout << '"';
}

std::string get_contract_address( koinos::chain::controller& controller,
                                  const std::string& name )
{
  koinos::contracts::name_service::get_address_arguments arguments;
  arguments.set_name( name );
  koinos::rpc::chain::invoke_system_call_request request;
  request.set_id( koinos::chain::get_contract_address );
  request.set_args( arguments.SerializeAsString() );

  koinos::contracts::name_service::get_address_result result;
  if( !result.ParseFromString( controller.invoke_system_call( request ).value() ) )
    throw std::runtime_error( "could not resolve contract address for " + name );
  return result.value().address();
}

std::string contract_read_hash( koinos::chain::controller& controller,
                                const std::string& contract_name,
                                uint32_t entry_point )
{
  koinos::rpc::chain::read_contract_request request;
  request.set_contract_id( get_contract_address( controller, contract_name ) );
  request.set_entry_point( entry_point );
  const auto result = controller.read_contract( request ).result();
  return koinos::util::to_hex(
    koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, result ) );
}

} // namespace

int main( int argc, char** argv )
{
  try
  {
    const auto args = parse_args( argc, argv );
    if( args.help )
    {
      usage( argv[ 0 ] );
      return 0;
    }

    koinos::initialize_logging( "historical_indexing_validator", {}, "info" );
    ReadOnlyUnifiedDB source;
    source.open( args.source_db );
    koinos::node::block_store::BlockStore block_store(
      source.db(),
      source.handle( ColumnFamily::blocks ),
      source.handle( ColumnFamily::block_meta ) );

    if( args.dump_block_json )
    {
      const auto highest = block_store.get_highest_block(
        koinos::rpc::block_store::get_highest_block_request{} );
      if( !highest.has_topology() || highest.topology().height() < *args.dump_block_json )
        throw std::runtime_error( "source block store does not reach requested dump height" );

      koinos::rpc::block_store::get_blocks_by_height_request request;
      request.set_head_block_id( highest.topology().id() );
      request.set_ancestor_start_height( *args.dump_block_json );
      request.set_num_blocks( 1 );
      request.set_return_block( true );
      const auto response = block_store.get_blocks_by_height( request );
      if( response.block_items_size() != 1 || !response.block_items( 0 ).has_block() )
        throw std::runtime_error( "could not resolve requested canonical block" );

      google::protobuf::util::JsonPrintOptions options;
      options.add_whitespace             = true;
      options.preserve_proto_field_names = true;
      std::string json;
      const auto status = google::protobuf::util::MessageToJsonString(
        response.block_items( 0 ).block(), &json, options );
      if( !status.ok() )
        throw std::runtime_error( "could not serialize canonical block: " + status.ToString() );
      std::cout << json;
      if( json.empty() || json.back() != '\n' )
        std::cout << '\n';
      return 0;
    }

    auto client = std::make_shared< ReadOnlyBlockStoreClient >( block_store, args.target_height );

    // Match the shipped node defaults so the selected post-index contract
    // reads exercise the same bounded read-only execution surface.
    koinos::chain::controller controller( 10'000'000, 64'000, uint64_t( 10 ) );
    controller.open(
      args.scratch_state_dir,
      load_genesis( args.genesis_file ),
      koinos::chain::fork_resolution_algorithm::pob,
      false );
    const auto start_head = controller.get_head_info();
    if( start_head.head_topology().height() > args.target_height )
      throw std::runtime_error( "scratch state is already beyond target height" );

    boost::asio::io_context ioc;
    koinos::chain::indexer indexer( ioc, controller, client, args.verify_blocks );
    auto complete      = indexer.index();
    const auto started = std::chrono::steady_clock::now();
    std::vector< std::thread > workers;
    workers.reserve( args.threads );
    for( uint32_t i = 0; i < args.threads; ++i )
      workers.emplace_back( [ &ioc ]() { ioc.run(); } );

    bool indexed = false;
    try
    {
      indexed = complete.get();
    }
    catch( ... )
    {
      ioc.stop();
      for( auto& worker: workers )
        worker.join();
      throw;
    }
    ioc.stop();
    for( auto& worker: workers )
      worker.join();
    if( !indexed )
      throw std::runtime_error( "indexer stopped before reaching target" );

    const auto duration = std::chrono::duration< double >(
      std::chrono::steady_clock::now() - started ).count();
    const auto final_head = controller.get_head_info();
    const auto chain_id   = controller.get_chain_id().chain_id();
    const std::vector< std::pair< std::string, std::string > > contract_reads = {
      { "koin.symbol", contract_read_hash( controller, "koin", 0xb76a7ca1 ) },
      { "koin.decimals", contract_read_hash( controller, "koin", 0xee80fd2f ) },
      { "vhp.symbol", contract_read_hash( controller, "vhp", 0xb76a7ca1 ) },
      { "vhp.decimals", contract_read_hash( controller, "vhp", 0xee80fd2f ) },
      { "pob.metadata", contract_read_hash( controller, "pob", 0xfcf7a68f ) },
      { "pob.consensus_parameters", contract_read_hash( controller, "pob", 0x5fd7ac0f ) }
    };
    controller.close();

    if( args.json )
    {
      std::cout << "{\n  \"ok\": true,\n  \"mode\": \""
                << ( args.verify_blocks ? "full" : "checked-fast" )
                << "\",\n  \"start_height\": " << start_head.head_topology().height()
                << ",\n  \"target_height\": " << args.target_height
                << ",\n  \"final_height\": " << final_head.head_topology().height()
                << ",\n  \"final_block_id\": ";
      print_json_string( koinos::util::to_hex( final_head.head_topology().id() ) );
      std::cout << ",\n  \"final_state_merkle_root\": ";
      print_json_string( koinos::util::to_hex( final_head.head_state_merkle_root() ) );
      std::cout << ",\n  \"chain_id\": ";
      print_json_string( koinos::util::to_hex( chain_id ) );
      std::cout << ",\n  \"contract_read_sha256\": {";
      for( std::size_t i = 0; i < contract_reads.size(); ++i )
      {
        std::cout << ( i ? ",\n    " : "\n    " );
        print_json_string( contract_reads[ i ].first );
        std::cout << ": ";
        print_json_string( contract_reads[ i ].second );
      }
      std::cout << "\n  }";
      std::cout << ",\n  \"fallback_count\": " << indexer.fallback_count()
                << ",\n  \"source_write_attempts\": " << client->write_attempts()
                << ",\n  \"broadcast_attempts\": " << client->broadcast_attempts()
                << ",\n  \"duration_seconds\": " << duration << "\n}\n";
    }
    else
    {
      std::cout << "historical indexing validation: ok\n"
                << "mode: " << ( args.verify_blocks ? "full" : "checked-fast" ) << '\n'
                << "start_height: " << start_head.head_topology().height() << '\n'
                << "target_height: " << args.target_height << '\n'
                << "final_height: " << final_head.head_topology().height() << '\n'
                << "final_block_id: " << koinos::util::to_hex( final_head.head_topology().id() ) << '\n'
                << "final_state_merkle_root: "
                << koinos::util::to_hex( final_head.head_state_merkle_root() ) << '\n'
                << "chain_id: " << koinos::util::to_hex( chain_id ) << '\n'
                << "contract_read_sha256:\n";
      for( const auto& [ name, hash ]: contract_reads )
        std::cout << "  " << name << ": " << hash << '\n';
      std::cout
                << "fallback_count: " << indexer.fallback_count() << '\n'
                << "source_write_attempts: " << client->write_attempts() << '\n'
                << "broadcast_attempts: " << client->broadcast_attempts() << '\n'
                << "duration_seconds: " << duration << '\n';
    }
    return 0;
  }
  catch( const std::exception& e )
  {
    std::cerr << "historical indexing validation failed: " << e.what() << '\n';
    usage( argv[ 0 ] );
    return 1;
  }
}
