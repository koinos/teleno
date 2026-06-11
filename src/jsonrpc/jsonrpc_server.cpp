#include "jsonrpc_server.hpp"

#include "core/node_version.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/util/json_util.h>

#include <koinos/log.hpp>
#include <koinos/options.pb.h>
#include <koinos/util/base58.hpp>
#include <koinos/util/base64.hpp>

namespace koinos::node::jsonrpc {

namespace {

bool has_hex_prefix( const std::string& value )
{
  return value.size() >= 2 && value[ 0 ] == '0' && ( value[ 1 ] == 'x' || value[ 1 ] == 'X' );
}

uint8_t hex_nibble( char c )
{
  if( c >= '0' && c <= '9' )
    return static_cast< uint8_t >( c - '0' );
  if( c >= 'a' && c <= 'f' )
    return static_cast< uint8_t >( c - 'a' + 10 );
  if( c >= 'A' && c <= 'F' )
    return static_cast< uint8_t >( c - 'A' + 10 );
  throw std::runtime_error( "invalid hex byte string" );
}

std::string hex_to_bytes( const std::string& value )
{
  const auto offset = has_hex_prefix( value ) ? 2 : 0;
  if( ( value.size() - offset ) % 2 != 0 )
    throw std::runtime_error( "hex byte string has odd length" );

  std::string bytes;
  bytes.reserve( ( value.size() - offset ) / 2 );
  for( auto i = offset; i < value.size(); i += 2 )
  {
    const auto byte = static_cast< char >( ( hex_nibble( value[ i ] ) << 4 ) | hex_nibble( value[ i + 1 ] ) );
    bytes.push_back( byte );
  }
  return bytes;
}

std::string bytes_to_hex( const std::string& bytes )
{
  static constexpr char hex[] = "0123456789abcdef";
  std::string out = "0x";
  out.reserve( 2 + bytes.size() * 2 );
  for( unsigned char byte: bytes )
  {
    out.push_back( hex[ byte >> 4 ] );
    out.push_back( hex[ byte & 0x0f ] );
  }
  return out;
}

std::string encode_koinos_bytes_field( const google::protobuf::FieldDescriptor* field,
                                       const std::string& bytes )
{
  auto type = ::koinos::BASE64;
  if( field && field->options().HasExtension( ::koinos::btype ) )
    type = field->options().GetExtension( ::koinos::btype );

  switch( type )
  {
    case ::koinos::HEX:
    case ::koinos::BLOCK_ID:
    case ::koinos::TRANSACTION_ID:
      return bytes_to_hex( bytes );
    case ::koinos::BASE58:
    case ::koinos::CONTRACT_ID:
    case ::koinos::ADDRESS:
      return util::to_base58( bytes );
    case ::koinos::BASE64:
    default:
      return util::to_base64( bytes );
  }
}

void rewrite_koinos_json_bytes( const google::protobuf::Message& msg,
                                nlohmann::json& json )
{
  if( !json.is_object() )
    return;

  const auto* descriptor = msg.GetDescriptor();
  const auto* reflection = msg.GetReflection();
  if( !descriptor || !reflection )
    return;

  std::vector< const google::protobuf::FieldDescriptor* > fields;
  reflection->ListFields( msg, &fields );

  for( const auto* field: fields )
  {
    const auto name = field->name();
    if( !json.contains( name ) )
      continue;

    auto& value = json[ name ];
    if( field->is_repeated() )
    {
      if( field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES && value.is_array() )
      {
        for( int i = 0; i < reflection->FieldSize( msg, field ) && i < static_cast< int >( value.size() ); ++i )
          value[ i ] = encode_koinos_bytes_field( field, reflection->GetRepeatedString( msg, field, i ) );
      }
      else if( field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE && value.is_array() )
      {
        for( int i = 0; i < reflection->FieldSize( msg, field ) && i < static_cast< int >( value.size() ); ++i )
          rewrite_koinos_json_bytes( reflection->GetRepeatedMessage( msg, field, i ), value[ i ] );
      }
      continue;
    }

    if( field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES && value.is_string() )
    {
      value = encode_koinos_bytes_field( field, reflection->GetString( msg, field ) );
    }
    else if( field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE && value.is_object() )
    {
      rewrite_koinos_json_bytes( reflection->GetMessage( msg, field ), value );
    }
  }
}

void normalize_json_bytes_field( nlohmann::json& object, const char* field )
{
  if( !object.is_object() || !object.contains( field ) || !object[ field ].is_string() )
    return;

  auto value = object[ field ].get< std::string >();
  if( value.empty() )
    return;

  if( has_hex_prefix( value ) )
    object[ field ] = util::to_base64( hex_to_bytes( value ) );
}

void normalize_json_base58_field( nlohmann::json& object, const char* field )
{
  if( !object.is_object() || !object.contains( field ) || !object[ field ].is_string() )
    return;

  auto value = object[ field ].get< std::string >();
  if( value.empty() )
    return;

  try
  {
    object[ field ] = util::to_base64( util::from_base58< std::string >( value ) );
  }
  catch( const std::exception& )
  {
    // Keep already-base64 protobuf JSON values unchanged.
  }
}

void normalize_json_hash_array( nlohmann::json& object, const char* field )
{
  if( !object.is_object() || !object.contains( field ) || !object[ field ].is_array() )
    return;

  for( auto& value: object[ field ] )
  {
    if( value.is_string() )
    {
      nlohmann::json wrapper = { { "value", value } };
      normalize_json_bytes_field( wrapper, "value" );
      value = wrapper[ "value" ];
    }
  }
}

void normalize_json_base58_array( nlohmann::json& object, const char* field )
{
  if( !object.is_object() || !object.contains( field ) || !object[ field ].is_array() )
    return;

  for( auto& value: object[ field ] )
  {
    if( !value.is_string() )
      continue;

    nlohmann::json wrapper = { { "value", value } };
    normalize_json_base58_field( wrapper, "value" );
    value = wrapper[ "value" ];
  }
}

void normalize_json_operation( nlohmann::json& op )
{
  if( !op.is_object() )
    return;

  if( op.contains( "call_contract" ) )
  {
    auto& call = op[ "call_contract" ];
    normalize_json_base58_field( call, "contract_id" );
    normalize_json_bytes_field( call, "args" );
  }

  if( op.contains( "upload_contract" ) )
  {
    auto& upload = op[ "upload_contract" ];
    normalize_json_base58_field( upload, "contract_id" );
    normalize_json_bytes_field( upload, "bytecode" );
  }

  if( op.contains( "set_system_call" ) )
  {
    auto& system_call = op[ "set_system_call" ];
    if( system_call.contains( "target" ) && system_call[ "target" ].contains( "system_call_bundle" ) )
    {
      auto& bundle = system_call[ "target" ][ "system_call_bundle" ];
      normalize_json_base58_field( bundle, "contract_id" );
    }
  }

  if( op.contains( "set_system_contract" ) )
    normalize_json_base58_field( op[ "set_system_contract" ], "contract_id" );
}

void normalize_json_transaction( nlohmann::json& tx )
{
  if( !tx.is_object() )
    return;

  normalize_json_bytes_field( tx, "id" );

  if( tx.contains( "header" ) )
  {
    auto& header = tx[ "header" ];
    normalize_json_bytes_field( header, "chain_id" );
    normalize_json_bytes_field( header, "nonce" );
    normalize_json_bytes_field( header, "operation_merkle_root" );
    normalize_json_base58_field( header, "payer" );
    normalize_json_base58_field( header, "payee" );
  }

  if( tx.contains( "operations" ) && tx[ "operations" ].is_array() )
  {
    for( auto& op: tx[ "operations" ] )
      normalize_json_operation( op );
  }

  normalize_json_hash_array( tx, "signatures" );
}

void normalize_json_block( nlohmann::json& block )
{
  if( !block.is_object() )
    return;

  normalize_json_bytes_field( block, "id" );
  normalize_json_bytes_field( block, "signature" );

  if( block.contains( "header" ) )
  {
    auto& header = block[ "header" ];
    normalize_json_bytes_field( header, "previous" );
    normalize_json_bytes_field( header, "previous_state_merkle_root" );
    normalize_json_bytes_field( header, "transaction_merkle_root" );
    normalize_json_base58_field( header, "signer" );
    normalize_json_hash_array( header, "approved_proposals" );
  }

  if( block.contains( "transactions" ) && block[ "transactions" ].is_array() )
  {
    for( auto& tx: block[ "transactions" ] )
      normalize_json_transaction( tx );
  }
}

void normalize_json_event( nlohmann::json& event )
{
  if( !event.is_object() )
    return;

  normalize_json_base58_field( event, "source" );
  normalize_json_bytes_field( event, "data" );
  normalize_json_base58_array( event, "impacted" );
}

void normalize_json_state_delta_entry( nlohmann::json& entry )
{
  if( !entry.is_object() )
    return;

  if( entry.contains( "object_space" ) )
    normalize_json_bytes_field( entry[ "object_space" ], "zone" );

  normalize_json_bytes_field( entry, "key" );
  normalize_json_bytes_field( entry, "value" );
}

void normalize_json_transaction_receipt( nlohmann::json& receipt )
{
  if( !receipt.is_object() )
    return;

  normalize_json_bytes_field( receipt, "id" );
  normalize_json_base58_field( receipt, "payer" );

  if( receipt.contains( "events" ) && receipt[ "events" ].is_array() )
  {
    for( auto& event: receipt[ "events" ] )
      normalize_json_event( event );
  }

  if( receipt.contains( "state_delta_entries" ) && receipt[ "state_delta_entries" ].is_array() )
  {
    for( auto& entry: receipt[ "state_delta_entries" ] )
      normalize_json_state_delta_entry( entry );
  }
}

void normalize_json_block_receipt( nlohmann::json& receipt )
{
  if( !receipt.is_object() )
    return;

  normalize_json_bytes_field( receipt, "id" );
  normalize_json_bytes_field( receipt, "state_merkle_root" );

  if( receipt.contains( "events" ) && receipt[ "events" ].is_array() )
  {
    for( auto& event: receipt[ "events" ] )
      normalize_json_event( event );
  }

  if( receipt.contains( "transaction_receipts" ) && receipt[ "transaction_receipts" ].is_array() )
  {
    for( auto& tx_receipt: receipt[ "transaction_receipts" ] )
      normalize_json_transaction_receipt( tx_receipt );
  }

  if( receipt.contains( "state_delta_entries" ) && receipt[ "state_delta_entries" ].is_array() )
  {
    for( auto& entry: receipt[ "state_delta_entries" ] )
      normalize_json_state_delta_entry( entry );
  }
}

nlohmann::json normalize_koinos_params( const std::string& service,
                                        const std::string& method,
                                        const nlohmann::json& params )
{
  auto normalized = params;

  if( service == "chain" )
  {
    if( method == "get_account_nonce" || method == "get_account_rc" )
      normalize_json_base58_field( normalized, "account" );
    else if( method == "read_contract" )
    {
      normalize_json_base58_field( normalized, "contract_id" );
      normalize_json_bytes_field( normalized, "args" );
    }
    else if( method == "submit_transaction" )
    {
      if( normalized.contains( "transaction" ) )
        normalize_json_transaction( normalized[ "transaction" ] );
    }
    else if( method == "submit_block" || method == "propose_block" )
    {
      if( normalized.contains( "block" ) )
        normalize_json_block( normalized[ "block" ] );
    }
  }
  else if( service == "block_store" )
  {
    normalize_json_bytes_field( normalized, "head_block_id" );
    normalize_json_hash_array( normalized, "block_ids" );
    if( method == "add_block" )
    {
      if( normalized.contains( "block_to_add" ) )
        normalize_json_block( normalized[ "block_to_add" ] );
      if( normalized.contains( "receipt_to_add" ) )
        normalize_json_block_receipt( normalized[ "receipt_to_add" ] );
    }
  }
  else if( service == "transaction_store" )
  {
    normalize_json_hash_array( normalized, "transaction_ids" );
  }
  else if( service == "contract_meta_store" )
  {
    normalize_json_base58_field( normalized, "contract_id" );
  }

  return normalized;
}

nlohmann::json chain_id_to_json( const rpc::chain::get_chain_id_response& resp )
{
  return { { "chain_id", util::to_base64( resp.chain_id() ) } };
}

nlohmann::json head_info_to_json( const rpc::chain::get_head_info_response& resp )
{
  return {
    { "head_topology",
      {
        { "id", bytes_to_hex( resp.head_topology().id() ) },
        { "height", std::to_string( resp.head_topology().height() ) },
        { "previous", bytes_to_hex( resp.head_topology().previous() ) },
      } },
    { "last_irreversible_block", std::to_string( resp.last_irreversible_block() ) },
    { "head_state_merkle_root", util::to_base64( resp.head_state_merkle_root() ) },
    { "head_block_time", std::to_string( resp.head_block_time() ) },
  };
}

nlohmann::json account_nonce_to_json( const rpc::chain::get_account_nonce_response& resp )
{
  return { { "nonce", util::to_base64( resp.nonce() ) } };
}

nlohmann::json read_contract_to_json( const rpc::chain::read_contract_response& resp )
{
  nlohmann::json logs = nlohmann::json::array();
  for( const auto& log: resp.logs() )
    logs.push_back( log );

  nlohmann::json result = { { "result", util::to_base64( resp.result() ) } };
  if( !logs.empty() )
    result[ "logs" ] = std::move( logs );
  return result;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

JSONRPCServer::JSONRPCServer( IChain* chain,
                              IMempool* mempool,
                              IBlockStore* block_store,
                              contract_meta_store::ContractMetaStore* contract_meta,
                              transaction_store::TransactionStore* tx_store,
                              account_history::AccountHistory* acct_history,
                              const std::string& listen_address,
                              uint16_t port,
                              unsigned int threads,
                              koinos::node::RpcAccessPolicy access_policy )
    : _chain( chain ),
      _mempool( mempool ),
      _block_store( block_store ),
      _contract_meta( contract_meta ),
      _tx_store( tx_store ),
      _acct_history( acct_history ),
      _ioc( std::max( threads, 1u ) ),
      _acceptor( _ioc, { net::ip::make_address( listen_address ), port } ),
      _thread_count( std::max( threads, 1u ) ),
      _access_policy( std::move( access_policy ) )
{
}

JSONRPCServer::~JSONRPCServer()
{
  stop();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void JSONRPCServer::start()
{
  _running = true;
  do_accept();

  _threads.reserve( _thread_count );
  for( unsigned int i = 0; i < _thread_count; ++i )
  {
    _threads.emplace_back( [this]() { _ioc.run(); } );
  }

  LOG( info ) << "[jsonrpc] Listening on "
              << _acceptor.local_endpoint().address().to_string()
              << ":" << _acceptor.local_endpoint().port()
              << " with " << _thread_count << " threads";
}

void JSONRPCServer::stop()
{
  if( !_running.exchange( false ) )
    return;

  _ioc.stop();
  for( auto& t: _threads )
  {
    if( t.joinable() )
      t.join();
  }
  _threads.clear();
}

// ---------------------------------------------------------------------------
// Accept loop
// ---------------------------------------------------------------------------

void JSONRPCServer::do_accept()
{
  _acceptor.async_accept( [this]( beast::error_code ec, tcp::socket socket ) {
    if( !ec && _running )
    {
      bool accepted = false;
      auto active = _active_sessions.load();
      while( active < _thread_count )
      {
        if( _active_sessions.compare_exchange_weak( active, active + 1 ) )
        {
          accepted = true;
          std::thread(
            [this, s = std::move( socket )]() mutable {
              struct SessionGuard
              {
                std::atomic< unsigned int >& active_sessions;
                ~SessionGuard() { active_sessions.fetch_sub( 1 ); }
              } guard{ _active_sessions };

              handle_session( std::move( s ) );
            }
          ).detach();
          break;
        }
      }

      if( !accepted )
      {
        http::response< http::string_body > res{ http::status::service_unavailable, 11 };
        res.set( http::field::content_type, "application/json" );
        res.body() = R"({"jsonrpc":"2.0","error":{"code":-32001,"message":"JSON-RPC session limit reached"},"id":null})";
        res.prepare_payload();
        beast::error_code write_ec;
        http::write( socket, res, write_ec );
      }
    }

    if( _running )
      do_accept();
  } );
}

// ---------------------------------------------------------------------------
// HTTP session
// ---------------------------------------------------------------------------

void JSONRPCServer::handle_session( tcp::socket socket )
{
  try
  {
    beast::flat_buffer buffer;
    for( ;; )
    {
      http::request< http::string_body > req;
      http::read( socket, buffer, req );

      http::response< http::string_body > res;
      res.version( req.version() );
      res.keep_alive( req.keep_alive() );
      res.set( http::field::content_type, "application/json" );
      res.set( http::field::access_control_allow_origin, "*" );
      res.set( http::field::access_control_allow_methods, "POST, OPTIONS" );
      res.set( http::field::access_control_allow_headers, "Content-Type" );

      // Handle CORS preflight
      if( req.method() == http::verb::options )
      {
        res.result( http::status::no_content );
        res.prepare_payload();
        http::write( socket, res );
        if( res.need_eof() )
          break;
        continue;
      }

      // GET /health — simple health check for load balancers and Knodel
      if( req.method() == http::verb::get )
      {
        auto target = req.target();
        if( target == "/health" || target == "/healthz" || target == "/" )
        {
          res.result( http::status::ok );
          nlohmann::json health;
          health[ "status" ]  = "ok";
          health[ "node" ]    = ::koinos::node::node_name();
          health[ "version" ] = ::koinos::node::build_version();
          res.body()          = health.dump();
          res.prepare_payload();
          http::write( socket, res );
          if( res.need_eof() )
            break;
          continue;
        }
      }

      if( req.method() != http::verb::post )
      {
        res.result( http::status::method_not_allowed );
        res.body() = R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Only POST allowed"},"id":null})";
        res.prepare_payload();
        http::write( socket, res );
        if( res.need_eof() )
          break;
        continue;
      }

      res.result( http::status::ok );
      res.body() = process_body( req.body() );
      res.prepare_payload();
      http::write( socket, res );
      if( res.need_eof() )
        break;
    }

    beast::error_code ec;
    socket.shutdown( tcp::socket::shutdown_send, ec );
  }
  catch( const beast::system_error& se )
  {
    if( se.code() != http::error::end_of_stream )
      LOG( debug ) << "[jsonrpc] Session error: " << se.code().message();
  }
  catch( const std::exception& e )
  {
    LOG( warning ) << "[jsonrpc] Session exception: " << e.what();
  }
  catch( ... )
  {
    LOG( warning ) << "[jsonrpc] Session exception: unknown exception";
  }
}

// ---------------------------------------------------------------------------
// JSON-RPC processing
// ---------------------------------------------------------------------------

std::string JSONRPCServer::process_body( const std::string& body )
{
  nlohmann::json parsed;
  try
  {
    parsed = nlohmann::json::parse( body );
  }
  catch( ... )
  {
    return make_error( PARSE_ERROR, "Parse error" ).dump();
  }

  // Batch request (JSON array)
  if( parsed.is_array() )
  {
    nlohmann::json batch_resp = nlohmann::json::array();
    for( const auto& item: parsed )
      batch_resp.push_back( process_single_request( item ) );
    return batch_resp.dump();
  }

  // Single request
  return process_single_request( parsed ).dump();
}

nlohmann::json JSONRPCServer::process_single_request( const nlohmann::json& request )
{
  // Extract ID (default null)
  nlohmann::json id = nullptr;
  if( request.contains( "id" ) )
    id = request[ "id" ];

  // Validate jsonrpc version
  if( !request.contains( "jsonrpc" ) || request[ "jsonrpc" ] != "2.0" )
    return make_error( INVALID_REQUEST, "Invalid JSON-RPC version", id );

  // Validate method
  if( !request.contains( "method" ) || !request[ "method" ].is_string() )
    return make_error( INVALID_REQUEST, "Missing or invalid method", id );

  std::string method_str = request[ "method" ].get< std::string >();

  // Parse method → service + method
  auto [service, method] = parse_method( method_str );
  if( service.empty() || method.empty() )
    return make_error( METHOD_NOT_FOUND, "Malformed method: " + method_str, id );

  if( !_access_policy.allows( service, method ) )
    return make_error( METHOD_NOT_FOUND, "RPC method denied by access policy: " + service + "." + method, id );

  // Extract params (default empty object)
  nlohmann::json params = nlohmann::json::object();
  if( request.contains( "params" ) && !request[ "params" ].is_null() )
    params = request[ "params" ];

  // Dispatch to service
  try
  {
    auto result = dispatch( service, method, normalize_koinos_params( service, method, params ) );
    return make_result( result, id );
  }
  catch( const std::exception& e )
  {
    return make_error( INTERNAL_ERROR, e.what(), id );
  }
  catch( ... )
  {
    return make_error( INTERNAL_ERROR, "Unknown service exception", id );
  }
}

// ---------------------------------------------------------------------------
// Method parsing
// ---------------------------------------------------------------------------

std::pair< std::string, std::string > JSONRPCServer::parse_method( const std::string& method_str )
{
  // Split on '.'
  std::vector< std::string > parts;
  std::istringstream ss( method_str );
  std::string part;
  while( std::getline( ss, part, '.' ) )
    parts.push_back( part );

  if( parts.size() < 2 )
    return { "", "" };

  // "chain.get_head_info" → ("chain", "get_head_info")
  // "koinos.rpc.chain.get_head_info" → ("chain", "get_head_info")
  std::string service = parts[ parts.size() - 2 ];
  std::string method  = parts[ parts.size() - 1 ];

  return { service, method };
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

nlohmann::json JSONRPCServer::dispatch( const std::string& service,
                                         const std::string& method,
                                         const nlohmann::json& params )
{
  if( service == "chain" )
    return dispatch_chain( method, params );
  if( service == "block_store" )
    return dispatch_block_store( method, params );
  if( service == "mempool" )
    return dispatch_mempool( method, params );
  if( service == "contract_meta_store" )
    return dispatch_contract_meta_store( method, params );
  if( service == "transaction_store" )
    return dispatch_transaction_store( method, params );
  if( service == "account_history" )
    return dispatch_account_history( method, params );

  // Health / status endpoint
  if( service == "node" && method == "get_status" )
  {
    nlohmann::json status;
    status[ "node" ]    = ::koinos::node::node_name();
    status[ "version" ] = ::koinos::node::build_version();
    status[ "mode" ]    = "monolith";

    if( _chain )
    {
      try
      {
        auto head = _chain->get_head_info();
        status[ "head_height" ] = head.head_topology().height();
        status[ "last_irreversible_block" ] = head.last_irreversible_block();
      }
      catch( ... )
      {
        status[ "head_height" ] = nullptr;
      }
    }

    status[ "services" ] = nlohmann::json::object();
    status[ "services" ][ "chain" ]       = _chain != nullptr;
    status[ "services" ][ "block_store" ] = _block_store != nullptr;
    status[ "services" ][ "mempool" ]     = _mempool != nullptr;
    status[ "services" ][ "contract_meta_store" ] = _contract_meta != nullptr;
    status[ "services" ][ "transaction_store" ]   = _tx_store != nullptr;
    status[ "services" ][ "account_history" ]     = _acct_history != nullptr;

    return status;
  }

  throw std::runtime_error( "Unknown service: " + service );
}

// ---------------------------------------------------------------------------
// Chain dispatch
// ---------------------------------------------------------------------------

nlohmann::json JSONRPCServer::dispatch_chain( const std::string& method,
                                               const nlohmann::json& params )
{
  if( !_chain )
    throw std::runtime_error( "chain service not available" );

  if( method == "get_head_info" )
  {
    rpc::chain::get_head_info_request req;
    json_to_proto( params, req );
    auto resp = _chain->get_head_info( req );
    return head_info_to_json( resp );
  }
  if( method == "get_chain_id" )
  {
    rpc::chain::get_chain_id_request req;
    json_to_proto( params, req );
    auto resp = _chain->get_chain_id( req );
    return chain_id_to_json( resp );
  }
  if( method == "get_fork_heads" )
  {
    rpc::chain::get_fork_heads_request req;
    json_to_proto( params, req );
    auto resp = _chain->get_fork_heads( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }
  if( method == "get_account_nonce" )
  {
    rpc::chain::get_account_nonce_request req;
    json_to_proto( params, req );
    auto resp = _chain->get_account_nonce( req );
    return account_nonce_to_json( resp );
  }
  if( method == "get_account_rc" )
  {
    rpc::chain::get_account_rc_request req;
    json_to_proto( params, req );
    auto resp = _chain->get_account_rc( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }
  if( method == "get_resource_limits" )
  {
    rpc::chain::get_resource_limits_request req;
    json_to_proto( params, req );
    auto resp = _chain->get_resource_limits( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }
  if( method == "read_contract" )
  {
    rpc::chain::read_contract_request req;
    json_to_proto( params, req );
    auto resp = _chain->read_contract( req );
    return read_contract_to_json( resp );
  }
  if( method == "submit_block" )
  {
    rpc::chain::submit_block_request req;
    json_to_proto( params, req );
    auto resp = _chain->submit_block( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }
  if( method == "submit_transaction" )
  {
    rpc::chain::submit_transaction_request req;
    json_to_proto( params, req );
    auto resp = _chain->submit_transaction( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }
  if( method == "invoke_system_call" )
  {
    rpc::chain::invoke_system_call_request req;
    json_to_proto( params, req );
    auto resp = _chain->invoke_system_call( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }
  if( method == "propose_block" )
  {
    rpc::chain::propose_block_request req;
    json_to_proto( params, req );
    auto resp = _chain->propose_block( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }

  throw std::runtime_error( "Unknown chain method: " + method );
}

// ---------------------------------------------------------------------------
// Block store dispatch
// ---------------------------------------------------------------------------

nlohmann::json JSONRPCServer::dispatch_block_store( const std::string& method,
                                                     const nlohmann::json& params )
{
  if( !_block_store )
    throw std::runtime_error( "block_store service not available" );

  if( method == "get_blocks_by_height" )
  {
    rpc::block_store::get_blocks_by_height_request req;
    json_to_proto( params, req );
    auto resp = _block_store->get_blocks_by_height( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }
  if( method == "get_blocks_by_id" )
  {
    rpc::block_store::get_blocks_by_id_request req;
    json_to_proto( params, req );
    auto resp = _block_store->get_blocks_by_id( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }
  if( method == "get_highest_block" )
  {
    rpc::block_store::get_highest_block_request req;
    json_to_proto( params, req );
    auto resp = _block_store->get_highest_block( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }
  if( method == "add_block" )
  {
    rpc::block_store::add_block_request req;
    json_to_proto( params, req );
    auto resp = _block_store->add_block( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }

  throw std::runtime_error( "Unknown block_store method: " + method );
}

// ---------------------------------------------------------------------------
// Mempool dispatch
// ---------------------------------------------------------------------------

nlohmann::json JSONRPCServer::dispatch_mempool( const std::string& method,
                                                 const nlohmann::json& params )
{
  if( !_mempool )
    throw std::runtime_error( "mempool service not available" );

  if( method == "get_pending_transactions" )
  {
    rpc::mempool::get_pending_transactions_request req;
    json_to_proto( params, req );
    auto resp = _mempool->get_pending_transactions( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }
  if( method == "check_pending_account_resources" )
  {
    rpc::mempool::check_pending_account_resources_request req;
    json_to_proto( params, req );
    auto resp = _mempool->check_pending_account_resources( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }
  if( method == "get_reserved_account_rc" )
  {
    rpc::mempool::get_reserved_account_rc_request req;
    json_to_proto( params, req );
    auto resp = _mempool->get_reserved_account_rc( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }

  throw std::runtime_error( "Unknown mempool method: " + method );
}

// ---------------------------------------------------------------------------
// Contract meta store dispatch
// ---------------------------------------------------------------------------

nlohmann::json JSONRPCServer::dispatch_contract_meta_store( const std::string& method,
                                                             const nlohmann::json& params )
{
  if( !_contract_meta )
    throw std::runtime_error( "contract_meta_store service not available" );

  if( method == "get_contract_meta" )
  {
    rpc::contract_meta_store::get_contract_meta_request req;
    json_to_proto( params, req );
    auto resp = _contract_meta->get_contract_meta( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }

  throw std::runtime_error( "Unknown contract_meta_store method: " + method );
}

// ---------------------------------------------------------------------------
// Transaction store dispatch
// ---------------------------------------------------------------------------

nlohmann::json JSONRPCServer::dispatch_transaction_store( const std::string& method,
                                                           const nlohmann::json& params )
{
  if( !_tx_store )
    throw std::runtime_error( "transaction_store service not available" );

  if( method == "get_transactions_by_id" )
  {
    rpc::transaction_store::get_transactions_by_id_request req;
    json_to_proto( params, req );
    auto resp = _tx_store->get_transactions_by_id( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }

  throw std::runtime_error( "Unknown transaction_store method: " + method );
}

// ---------------------------------------------------------------------------
// Account history dispatch
// ---------------------------------------------------------------------------

nlohmann::json JSONRPCServer::dispatch_account_history( const std::string& method,
                                                         const nlohmann::json& params )
{
  if( !_acct_history )
    throw std::runtime_error( "account_history service not available" );

  if( method == "get_account_history" )
  {
    rpc::account_history::get_account_history_request req;
    json_to_proto( params, req );
    auto resp = _acct_history->get_account_history( req );
    return nlohmann::json::parse( proto_to_json( resp ) );
  }

  throw std::runtime_error( "Unknown account_history method: " + method );
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

nlohmann::json JSONRPCServer::make_error( int code, const std::string& message, const nlohmann::json& id )
{
  return {
    { "jsonrpc", "2.0" },
    { "error", { { "code", code }, { "message", message } } },
    { "id", id }
  };
}

nlohmann::json JSONRPCServer::make_result( const nlohmann::json& result, const nlohmann::json& id )
{
  return {
    { "jsonrpc", "2.0" },
    { "result", result },
    { "id", id }
  };
}

std::string JSONRPCServer::proto_to_json( const google::protobuf::Message& msg )
{
  std::string json_str;
  google::protobuf::util::JsonPrintOptions opts;
  opts.preserve_proto_field_names   = true;
  opts.always_print_primitive_fields = false;
  auto status = google::protobuf::util::MessageToJsonString( msg, &json_str, opts );
  if( !status.ok() )
    return "{}";

  try
  {
    auto json = nlohmann::json::parse( json_str );
    rewrite_koinos_json_bytes( msg, json );
    return json.dump();
  }
  catch( const std::exception& )
  {
    return json_str;
  }
}

bool JSONRPCServer::json_to_proto( const nlohmann::json& params, google::protobuf::Message& msg )
{
  if( params.is_null() || ( params.is_object() && params.empty() ) )
    return true; // Empty params → default-initialized message

  auto json_str = params.dump();
  auto status   = google::protobuf::util::JsonStringToMessage( json_str, &msg );
  return status.ok();
}

} // namespace koinos::node::jsonrpc
