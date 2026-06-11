#include <koinos/rpc/services.grpc.pb.h>

#include <google/protobuf/util/json_util.h>
#include <grpcpp/grpcpp.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

struct Args
{
  std::string legacy;
  std::string monolith;
  std::string legacy_input;
  std::string monolith_input;
  std::string output;
  int timeout_ms          = 5000;
  bool fail_on_mismatch   = false;
  bool strict_payload     = false;
};

struct ProbeCase
{
  std::string name;
  bool stable_payload;
  std::function< json( koinos::services::koinos::Stub& stub, int timeout_ms ) > run;
};

std::string bytes_to_hex( const std::string& bytes )
{
  static constexpr char hex[] = "0123456789abcdef";
  std::string out;
  out.reserve( bytes.size() * 2 );
  for( unsigned char c: bytes )
  {
    out.push_back( hex[ c >> 4 ] );
    out.push_back( hex[ c & 0x0f ] );
  }
  return out;
}

std::string proto_json( const google::protobuf::Message& message )
{
  std::string out;
  google::protobuf::util::JsonPrintOptions options;
  options.always_print_primitive_fields = true;
  options.preserve_proto_field_names    = true;
  auto status = google::protobuf::util::MessageToJsonString( message, &out, options );
  if( !status.ok() )
    return "{}";
  return out;
}

template< typename Request, typename Response, typename Call >
json unary_call( const std::string& name, int timeout_ms, const Request& request, Call&& call )
{
  grpc::ClientContext ctx;
  ctx.set_deadline( std::chrono::system_clock::now() + std::chrono::milliseconds( timeout_ms ) );

  Response response;
  grpc::Status status = call( &ctx, request, &response );

  json result;
  result[ "name" ]           = name;
  result[ "status_code" ]    = status.error_code();
  result[ "status_message" ] = status.error_message();
  result[ "ok" ]             = status.ok();
  if( status.ok() )
  {
    const auto bytes = response.SerializeAsString();
    result[ "response_hex" ]  = bytes_to_hex( bytes );
    result[ "response_json" ] = json::parse( proto_json( response ), nullptr, false );
    if( result[ "response_json" ].is_discarded() )
      result[ "response_json" ] = proto_json( response );
  }
  return result;
}

std::vector< ProbeCase > build_cases()
{
  return {
    {
      "get_chain_id",
      true,
      []( auto& stub, int timeout_ms ) {
        return unary_call< koinos::rpc::chain::get_chain_id_request,
                           koinos::rpc::chain::get_chain_id_response >(
          "get_chain_id",
          timeout_ms,
          {},
          [&]( auto* ctx, const auto& req, auto* resp ) { return stub.get_chain_id( ctx, req, resp ); } );
      },
    },
    {
      "get_head_info",
      false,
      []( auto& stub, int timeout_ms ) {
        return unary_call< koinos::rpc::chain::get_head_info_request,
                           koinos::rpc::chain::get_head_info_response >(
          "get_head_info",
          timeout_ms,
          {},
          [&]( auto* ctx, const auto& req, auto* resp ) { return stub.get_head_info( ctx, req, resp ); } );
      },
    },
    {
      "get_highest_block",
      false,
      []( auto& stub, int timeout_ms ) {
        return unary_call< koinos::rpc::block_store::get_highest_block_request,
                           koinos::rpc::block_store::get_highest_block_response >(
          "get_highest_block",
          timeout_ms,
          {},
          [&]( auto* ctx, const auto& req, auto* resp ) { return stub.get_highest_block( ctx, req, resp ); } );
      },
    },
    {
      "get_pending_transactions",
      false,
      []( auto& stub, int timeout_ms ) {
        koinos::rpc::mempool::get_pending_transactions_request req;
        req.set_limit( 10 );
        return unary_call< koinos::rpc::mempool::get_pending_transactions_request,
                           koinos::rpc::mempool::get_pending_transactions_response >(
          "get_pending_transactions",
          timeout_ms,
          req,
          [&]( auto* ctx, const auto& r, auto* resp ) { return stub.get_pending_transactions( ctx, r, resp ); } );
      },
    },
    {
      "check_pending_account_resources.empty",
      true,
      []( auto& stub, int timeout_ms ) {
        return unary_call< koinos::rpc::mempool::check_pending_account_resources_request,
                           koinos::rpc::mempool::check_pending_account_resources_response >(
          "check_pending_account_resources.empty",
          timeout_ms,
          {},
          [&]( auto* ctx, const auto& req, auto* resp ) {
            return stub.check_pending_account_resources( ctx, req, resp );
          } );
      },
    },
    {
      "get_contract_meta.missing_contract_id",
      true,
      []( auto& stub, int timeout_ms ) {
        return unary_call< koinos::rpc::contract_meta_store::get_contract_meta_request,
                           koinos::rpc::contract_meta_store::get_contract_meta_response >(
          "get_contract_meta.missing_contract_id",
          timeout_ms,
          {},
          [&]( auto* ctx, const auto& req, auto* resp ) { return stub.get_contract_meta( ctx, req, resp ); } );
      },
    },
    {
      "get_contract_meta.db_miss",
      true,
      []( auto& stub, int timeout_ms ) {
        koinos::rpc::contract_meta_store::get_contract_meta_request req;
        req.set_contract_id( std::string( 1, '\0' ) );
        return unary_call< koinos::rpc::contract_meta_store::get_contract_meta_request,
                           koinos::rpc::contract_meta_store::get_contract_meta_response >(
          "get_contract_meta.db_miss",
          timeout_ms,
          req,
          [&]( auto* ctx, const auto& r, auto* resp ) { return stub.get_contract_meta( ctx, r, resp ); } );
      },
    },
    {
      "get_transactions_by_id.missing_ids",
      true,
      []( auto& stub, int timeout_ms ) {
        return unary_call< koinos::rpc::transaction_store::get_transactions_by_id_request,
                           koinos::rpc::transaction_store::get_transactions_by_id_response >(
          "get_transactions_by_id.missing_ids",
          timeout_ms,
          {},
          [&]( auto* ctx, const auto& req, auto* resp ) { return stub.get_transactions_by_id( ctx, req, resp ); } );
      },
    },
    {
      "get_transactions_by_id.db_miss",
      true,
      []( auto& stub, int timeout_ms ) {
        koinos::rpc::transaction_store::get_transactions_by_id_request req;
        req.add_transaction_ids( std::string( 1, '\0' ) );
        return unary_call< koinos::rpc::transaction_store::get_transactions_by_id_request,
                           koinos::rpc::transaction_store::get_transactions_by_id_response >(
          "get_transactions_by_id.db_miss",
          timeout_ms,
          req,
          [&]( auto* ctx, const auto& r, auto* resp ) { return stub.get_transactions_by_id( ctx, r, resp ); } );
      },
    },
    {
      "get_gossip_status",
      true,
      []( auto& stub, int timeout_ms ) {
        return unary_call< koinos::rpc::p2p::get_gossip_status_request,
                           koinos::rpc::p2p::get_gossip_status_response >(
          "get_gossip_status",
          timeout_ms,
          {},
          [&]( auto* ctx, const auto& req, auto* resp ) { return stub.get_gossip_status( ctx, req, resp ); } );
      },
    },
  };
}

json probe_endpoint( const std::string& endpoint, int timeout_ms )
{
  auto channel = grpc::CreateChannel( endpoint, grpc::InsecureChannelCredentials() );
  auto ready   = channel->WaitForConnected(
    std::chrono::system_clock::now() + std::chrono::milliseconds( timeout_ms ) );

  json result;
  result[ "endpoint" ] = endpoint;
  result[ "ready" ]    = ready;
  result[ "cases" ]    = json::array();

  if( !ready )
    return result;

  auto stub = koinos::services::koinos::NewStub( channel );
  for( const auto& test_case: build_cases() )
  {
    auto row                 = test_case.run( *stub, timeout_ms );
    row[ "stable_payload" ]  = test_case.stable_payload;
    result[ "cases" ].push_back( std::move( row ) );
  }
  return result;
}

json compare_results( const json& legacy, const json& monolith, bool strict_payload )
{
  json comparisons = json::array();
  bool ok          = true;

  std::map< std::string, json > legacy_by_name;
  for( const auto& row: legacy.value( "cases", json::array() ) )
    legacy_by_name[ row.value( "name", "" ) ] = row;

  for( const auto& monolith_row: monolith.value( "cases", json::array() ) )
  {
    const auto name = monolith_row.value( "name", "" );
    json cmp;
    cmp[ "name" ] = name;

    auto it = legacy_by_name.find( name );
    if( it == legacy_by_name.end() )
    {
      cmp[ "status" ] = "missing_legacy_case";
      ok              = false;
      comparisons.push_back( std::move( cmp ) );
      continue;
    }

    const auto& legacy_row = it->second;
    const bool status_match =
      legacy_row.value( "status_code", -1 ) == monolith_row.value( "status_code", -2 );
    const bool should_compare_payload =
      strict_payload || monolith_row.value( "stable_payload", false );
    const bool payload_match =
      !should_compare_payload || !legacy_row.value( "ok", false ) || !monolith_row.value( "ok", false )
      || legacy_row.value( "response_hex", "" ) == monolith_row.value( "response_hex", "" );

    cmp[ "status_match" ]  = status_match;
    cmp[ "payload_match" ] = payload_match;
    cmp[ "status" ]        = status_match && payload_match ? "pass" : "fail";
    if( cmp[ "status" ] == "fail" )
      ok = false;
    comparisons.push_back( std::move( cmp ) );
  }

  json result;
  result[ "ok" ]          = ok;
  result[ "comparisons" ] = comparisons;
  return result;
}

json read_json_file( const std::string& path )
{
  std::ifstream file( path );
  if( !file )
    throw std::runtime_error( "could not open input: " + path );

  json data;
  file >> data;
  return data;
}

json extract_endpoint_result( const json& data, const std::string& preferred_key )
{
  if( data.contains( preferred_key ) )
    return data.at( preferred_key );
  if( data.contains( "monolith" ) )
    return data.at( "monolith" );
  if( data.contains( "legacy" ) )
    return data.at( "legacy" );
  if( data.contains( "cases" ) )
    return data;
  throw std::runtime_error( "input does not contain a gRPC probe result" );
}

void usage( const char* program )
{
  std::cerr << "usage: " << program
            << " [--legacy host:port] [--monolith host:port]"
            << " [--legacy-input path --monolith-input path] [--output path]"
            << " [--timeout-ms n] [--strict-payload] [--fail-on-mismatch]\n";
}

Args parse_args( int argc, char** argv )
{
  Args args;
  for( int i = 1; i < argc; ++i )
  {
    std::string arg = argv[ i ];
    auto next = [&]() -> std::string {
      if( i + 1 >= argc )
        throw std::runtime_error( "missing value for " + arg );
      return argv[ ++i ];
    };

    if( arg == "--legacy" )
      args.legacy = next();
    else if( arg == "--monolith" )
      args.monolith = next();
    else if( arg == "--legacy-input" )
      args.legacy_input = next();
    else if( arg == "--monolith-input" )
      args.monolith_input = next();
    else if( arg == "--output" )
      args.output = next();
    else if( arg == "--timeout-ms" )
      args.timeout_ms = std::stoi( next() );
    else if( arg == "--strict-payload" )
      args.strict_payload = true;
    else if( arg == "--fail-on-mismatch" )
      args.fail_on_mismatch = true;
    else if( arg == "--help" || arg == "-h" )
    {
      usage( argv[ 0 ] );
      std::exit( 0 );
    }
    else
      throw std::runtime_error( "unknown argument: " + arg );
  }

  const bool has_endpoint = !args.legacy.empty() || !args.monolith.empty();
  const bool has_inputs   = !args.legacy_input.empty() || !args.monolith_input.empty();
  if( !has_endpoint && !has_inputs )
    throw std::runtime_error( "at least one endpoint or input file is required" );
  if( has_inputs && ( args.legacy_input.empty() || args.monolith_input.empty() ) )
    throw std::runtime_error( "--legacy-input and --monolith-input must be provided together" );

  return args;
}

} // anonymous namespace

int main( int argc, char** argv )
{
  try
  {
    const auto args = parse_args( argc, argv );

    json result;
    result[ "kind" ]             = "koinos-grpc-parity-probe";
    result[ "strict_payload" ]   = args.strict_payload;
    result[ "fail_on_mismatch" ] = args.fail_on_mismatch;

    if( !args.monolith_input.empty() )
      result[ "monolith" ] = extract_endpoint_result( read_json_file( args.monolith_input ), "monolith" );
    else if( !args.monolith.empty() )
      result[ "monolith" ] = probe_endpoint( args.monolith, args.timeout_ms );

    if( !args.legacy_input.empty() )
      result[ "legacy" ] = extract_endpoint_result( read_json_file( args.legacy_input ), "legacy" );
    else if( !args.legacy.empty() )
      result[ "legacy" ] = probe_endpoint( args.legacy, args.timeout_ms );

    if( result.contains( "legacy" ) && result.contains( "monolith" ) )
      result[ "comparison" ] = compare_results( result[ "legacy" ], result[ "monolith" ], args.strict_payload );

    const auto output = result.dump( 2 ) + "\n";
    if( args.output.empty() )
      std::cout << output;
    else
    {
      std::ofstream file( args.output );
      if( !file )
        throw std::runtime_error( "could not open output: " + args.output );
      file << output;
      std::cout << "wrote gRPC parity probe result to " << args.output << "\n";
    }

    if( args.fail_on_mismatch && result.contains( "comparison" ) && !result[ "comparison" ].value( "ok", false ) )
      return 1;
    return 0;
  }
  catch( const std::exception& e )
  {
    std::cerr << "error: " << e.what() << "\n";
    usage( argv[ 0 ] );
    return 2;
  }
}
