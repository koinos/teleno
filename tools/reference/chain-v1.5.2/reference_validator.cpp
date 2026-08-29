#include "block_store/block_store.hpp"

#include <google/protobuf/util/json_util.h>

#include <koinos/chain/controller.hpp>
#include <koinos/chain/system_call_ids.pb.h>
#include <koinos/contracts/name_service/name_service.pb.h>
#include <koinos/crypto/multihash.hpp>
#include <koinos/log.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>

#include <rocksdb/db.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Args
{
  std::filesystem::path source_db;
  std::filesystem::path scratch_state_dir;
  std::filesystem::path genesis_file;
  uint64_t target_height = 0;
  bool verify_blocks     = false;
  bool help              = false;
};

void usage( const char* argv0 )
{
  std::cout << "Usage: " << argv0
            << " --source-db PATH --scratch-state-dir PATH --genesis PATH"
               " --target-height N [--verify-blocks] [--json]\n";
}

uint64_t parse_u64( const std::string& name, const std::string& value )
{
  std::size_t parsed = 0;
  const auto result  = std::stoull( value, &parsed );
  if( parsed != value.size() )
    throw std::runtime_error( "invalid " + name + " value: " + value );
  return result;
}

Args parse_args( int argc, char** argv )
{
  Args args;
  for( int i = 1; i < argc; ++i )
  {
    const std::string arg = argv[ i ];
    auto value = [&]() -> std::string
    {
      if( i + 1 >= argc )
        throw std::runtime_error( arg + " requires a value" );
      return argv[ ++i ];
    };
    if( arg == "--source-db" )
      args.source_db = value();
    else if( arg == "--scratch-state-dir" )
      args.scratch_state_dir = value();
    else if( arg == "--genesis" )
      args.genesis_file = value();
    else if( arg == "--target-height" )
      args.target_height = parse_u64( arg, value() );
    else if( arg == "--verify-blocks" )
      args.verify_blocks = true;
    else if( arg == "--json" )
    {
      // JSON is the only result format. Retain the option for command parity
      // with the Teleno historical validator.
    }
    else if( arg == "--help" || arg == "-h" )
      args.help = true;
    else
      throw std::runtime_error( "unknown argument: " + arg );
  }
  if( args.help )
    return args;
  if( args.source_db.empty() || args.scratch_state_dir.empty()
      || args.genesis_file.empty() || !args.target_height )
    throw std::runtime_error( "all path and target arguments are required" );

  args.source_db         = std::filesystem::absolute( args.source_db ).lexically_normal();
  args.scratch_state_dir = std::filesystem::absolute( args.scratch_state_dir ).lexically_normal();
  args.genesis_file      = std::filesystem::absolute( args.genesis_file ).lexically_normal();
  if( args.source_db == args.scratch_state_dir )
    throw std::runtime_error( "source and scratch paths must differ" );
  if( !std::filesystem::exists( args.source_db / "CURRENT" ) )
    throw std::runtime_error( "source unified RocksDB does not exist" );
  if( !std::filesystem::exists( args.scratch_state_dir / "CURRENT" ) )
    throw std::runtime_error( "prepared scratch state RocksDB does not exist" );
  if( !std::filesystem::is_regular_file( args.genesis_file ) )
    throw std::runtime_error( "genesis data does not exist" );
  return args;
}

koinos::chain::genesis_data load_genesis( const std::filesystem::path& path )
{
  std::ifstream input( path );
  if( !input )
    throw std::runtime_error( "could not open genesis data" );
  const std::string json( ( std::istreambuf_iterator< char >( input ) ), {} );
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
    static const std::vector< std::string > names = {
      rocksdb::kDefaultColumnFamilyName, "blocks", "block_meta", "contract_meta",
      "transaction_index", "account_history", "chain_state", "chain_metadata",
      "storage_metadata"
    };
    std::vector< rocksdb::ColumnFamilyDescriptor > descriptors;
    for( const auto& name: names )
      descriptors.emplace_back( name, rocksdb::ColumnFamilyOptions() );
    rocksdb::DB* raw = nullptr;
    rocksdb::Options options;
    const auto status = rocksdb::DB::OpenForReadOnly(
      options, path.string(), descriptors, &_handles, &raw );
    if( !status.ok() )
      throw std::runtime_error( "could not open source DB read-only: " + status.ToString() );
    _db.reset( raw );
  }

  rocksdb::DB* db() const { return _db.get(); }
  rocksdb::ColumnFamilyHandle* blocks() const { return _handles.at( 1 ); }
  rocksdb::ColumnFamilyHandle* block_meta() const { return _handles.at( 2 ); }

private:
  std::unique_ptr< rocksdb::DB > _db;
  std::vector< rocksdb::ColumnFamilyHandle* > _handles;
};

koinos::block_topology target_topology( koinos::node::block_store::BlockStore& store,
                                        uint64_t height )
{
  const auto highest = store.get_highest_block( {} );
  if( !highest.has_topology() || highest.topology().height() < height )
    throw std::runtime_error( "source block store does not reach target" );
  koinos::rpc::block_store::get_blocks_by_height_request request;
  request.set_head_block_id( highest.topology().id() );
  request.set_ancestor_start_height( height );
  request.set_num_blocks( 1 );
  request.set_return_block( true );
  const auto response = store.get_blocks_by_height( request );
  if( response.block_items_size() != 1 )
    throw std::runtime_error( "could not resolve target block" );
  const auto& block = response.block_items( 0 ).block();
  koinos::block_topology result;
  result.set_id( block.id() );
  result.set_height( block.header().height() );
  result.set_previous( block.header().previous() );
  return result;
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
                                const std::string& name,
                                uint32_t entry_point )
{
  koinos::rpc::chain::read_contract_request request;
  request.set_contract_id( get_contract_address( controller, name ) );
  request.set_entry_point( entry_point );
  return koinos::util::to_hex( koinos::crypto::hash(
    koinos::crypto::multicodec::sha2_256,
    controller.read_contract( request ).result() ) );
}

void print_string( const std::string& value )
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

    koinos::initialize_logging( "chain_v152_reference_validator", {}, "info" );
    ReadOnlyUnifiedDB source;
    source.open( args.source_db );
    koinos::node::block_store::BlockStore store(
      source.db(), source.blocks(), source.block_meta() );
    const auto target = target_topology( store, args.target_height );

    // Match the shipped koinos-chain v1.5.2 service defaults. In particular,
    // zero is not an unlimited read-compute budget in this release.
    koinos::chain::controller controller( 10'000'000, 64'000, uint64_t( 10 ) );
    controller.open( args.scratch_state_dir, load_genesis( args.genesis_file ),
                     koinos::chain::fork_resolution_algorithm::pob, false );
    const auto start = controller.get_head_info();
    if( start.head_topology().height() > args.target_height )
      throw std::runtime_error( "scratch state is beyond target" );

    const auto started = std::chrono::steady_clock::now();
    uint64_t next_height = start.head_topology().height() + 1;
    uint64_t fallbacks   = 0;
    std::optional< koinos::block_store::block_item > pending;
    while( next_height <= args.target_height )
    {
      koinos::rpc::block_store::get_blocks_by_height_request request;
      request.set_head_block_id( target.id() );
      request.set_ancestor_start_height( next_height );
      request.set_num_blocks( static_cast< uint32_t >(
        std::min< uint64_t >( 1000, args.target_height - next_height + 1 ) ) );
      request.set_return_block( true );
      request.set_return_receipt( true );
      auto response = store.get_blocks_by_height( request );
      if( response.block_items_size() == 0 )
        throw std::runtime_error( "source returned an empty block batch" );
      for( auto& item: *response.mutable_block_items() )
      {
        if( args.verify_blocks )
        {
          koinos::rpc::chain::submit_block_request submit;
          *submit.mutable_block() = item.block();
          controller.submit_block( submit, args.target_height );
        }
        else
        {
          if( pending
              && controller.apply_block_delta_checked(
                pending->block(), pending->receipt(),
                item.block().header().previous_state_merkle_root(), args.target_height ) )
            ++fallbacks;
          pending = std::move( item );
        }
        ++next_height;
      }
    }
    if( pending )
      controller.apply_block_delta( pending->block(), pending->receipt(), args.target_height );

    const auto duration = std::chrono::duration< double >(
      std::chrono::steady_clock::now() - started ).count();
    const auto final_head = controller.get_head_info();
    const auto chain_id   = controller.get_chain_id().chain_id();
    const std::vector< std::pair< std::string, std::string > > reads = {
      { "koin.symbol", contract_read_hash( controller, "koin", 0xb76a7ca1 ) },
      { "koin.decimals", contract_read_hash( controller, "koin", 0xee80fd2f ) },
      { "vhp.symbol", contract_read_hash( controller, "vhp", 0xb76a7ca1 ) },
      { "vhp.decimals", contract_read_hash( controller, "vhp", 0xee80fd2f ) },
      { "pob.metadata", contract_read_hash( controller, "pob", 0xfcf7a68f ) },
      { "pob.consensus_parameters", contract_read_hash( controller, "pob", 0x5fd7ac0f ) }
    };
    controller.close();

    std::cout << "{\n  \"ok\": true,\n  \"reference_chain_commit\": ";
    print_string( "0ae99eced8b585c4145424e9c2a28f667796cc66" );
    std::cout << ",\n  \"reference_state_db_commit\": ";
    print_string( "3a1c904e61afbff59e167f50175519e68046e090" );
    std::cout << ",\n  \"mode\": ";
    print_string( args.verify_blocks ? "full" : "checked-fast" );
    std::cout << ",\n  \"start_height\": " << start.head_topology().height()
              << ",\n  \"target_height\": " << args.target_height
              << ",\n  \"final_height\": " << final_head.head_topology().height()
              << ",\n  \"final_block_id\": ";
    print_string( koinos::util::to_hex( final_head.head_topology().id() ) );
    std::cout << ",\n  \"final_state_merkle_root\": ";
    print_string( koinos::util::to_hex( final_head.head_state_merkle_root() ) );
    std::cout << ",\n  \"chain_id\": ";
    print_string( koinos::util::to_hex( chain_id ) );
    std::cout << ",\n  \"contract_read_sha256\": {";
    for( std::size_t i = 0; i < reads.size(); ++i )
    {
      std::cout << ( i ? ",\n    " : "\n    " );
      print_string( reads[ i ].first );
      std::cout << ": ";
      print_string( reads[ i ].second );
    }
    std::cout << "\n  },\n  \"fallback_count\": " << fallbacks
              << ",\n  \"duration_seconds\": " << duration << "\n}\n";
    return 0;
  }
  catch( const std::exception& e )
  {
    std::cerr << "reference validation failed: " << e.what() << '\n';
    usage( argv[ 0 ] );
    return 1;
  }
}
