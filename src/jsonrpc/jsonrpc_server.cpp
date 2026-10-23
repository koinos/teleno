#include "jsonrpc_server.hpp"
#include "json_cleanup.hpp"

#include "core/node_version.hpp"
#include "core/transaction_work_budget.hpp"

#include <algorithm>
#include <exception>
#include <optional>
#include <sstream>
#include <streambuf>
#include <utility>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/util/json_util.h>

#include <koinos/log.hpp>
#include <koinos/options.pb.h>
#include <koinos/util/base58.hpp>
#include <koinos/util/base64.hpp>

namespace koinos::node::jsonrpc {

namespace {

// Serialize into the final HTTP body without a second per-item string. This
// is not network streaming: no bytes are sent until the complete body exists.
class StringAppendBuffer final : public std::streambuf
{
public:
  explicit StringAppendBuffer( std::string& output ) : _output( output ) {}

protected:
  std::streamsize xsputn( const char* data, std::streamsize size ) override
  {
    _output.append( data, static_cast< std::size_t >( size ) );
    return size;
  }

  int_type overflow( int_type value ) override
  {
    if( !traits_type::eq_int_type( value, traits_type::eof() ) )
      _output.push_back( traits_type::to_char_type( value ) );
    return traits_type::not_eof( value );
  }

private:
  std::string& _output;
};

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

  const auto& value = object[ field ].get_ref< const std::string& >();
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
  catch( const LocalCapacityExceeded& )
  {
    throw;
  }
  catch( const std::bad_alloc& )
  {
    throw;
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
      JsonCleanup wrapper_cleanup( wrapper );
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
    JsonCleanup wrapper_cleanup( wrapper );
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
  JsonCleanup normalized_cleanup( normalized );

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

  return std::move( normalized );
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
  JsonCleanup logs_cleanup( logs );
  for( const auto& log: resp.logs() )
    logs.push_back( log );

  nlohmann::json result = { { "result", util::to_base64( resp.result() ) } };
  JsonCleanup result_cleanup( result );
  if( !logs.empty() )
    result[ "logs" ] = std::move( logs );
  return std::move( result );
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

struct JSONRPCServer::Session: std::enable_shared_from_this< Session >
{
  using session_executor = net::strand< net::io_context::executor_type >;
  using session_socket = net::basic_stream_socket< tcp, session_executor >;
  using session_stream = beast::basic_stream< tcp, session_executor >;
  JSONRPCServer& server;
  session_stream stream;
  beast::flat_buffer buffer;
  std::optional< http::request_parser< http::string_body > > parser;
  OwnedMemoryBudget::Lease refusal_workspace;
  MempoolOutputLeases mempool_output_leases;
  std::optional< http::response< http::string_body > > response;
  bool closed = false;
  std::size_t attributed_request_capacity = 0;
  std::size_t attributed_response_capacity = 0;

  Session( JSONRPCServer& s, session_socket socket )
    : server( s ), stream( std::move( socket ) ),
      mempool_output_leases( OwnedMemoryBudget::Allocator< OwnedMemoryBudget::Lease >(
        s._mempool ? s._mempool->memory_budget() : OwnedMemoryBudget{} ) ) {}
  ~Session()
  {
    server.replace_body_capacities( attributed_request_capacity,
                                   attributed_response_capacity, 0, 0 );
  }
  void account_bodies( bool completed_request = true )
  {
    const auto request_capacity = parser && completed_request ? parser->get().body().capacity() : 0;
    const auto response_capacity = response ? response->body().capacity() : 0;
    server.replace_body_capacities( attributed_request_capacity, attributed_response_capacity,
                                   request_capacity, response_capacity );
    attributed_request_capacity = request_capacity;
    attributed_response_capacity = response_capacity;
  }
  void start() { net::dispatch( stream.get_executor(), [self=shared_from_this()] { self->read(); } ); }
  void cancel() { net::post( stream.get_executor(), [self=shared_from_this()] { self->close(); } ); }
  void close()
  {
    if( closed ) return;
    closed = true;
    beast::error_code ec;
    stream.socket().cancel( ec );
    stream.socket().shutdown( tcp::socket::shutdown_both, ec );
    stream.socket().close( ec );
    server.release_session( this );
  }
  void read()
  {
    if( closed || !server._running ) { close(); return; }
    parser.emplace();
    account_bodies( false );
    // Preserve Beast's previous defaults. Deadline covers the complete read,
    // including idle keepalive and incomplete headers/body, not each byte.
    parser->body_limit( 1024 * 1024 );
    parser->header_limit( 8192 );
    stream.expires_after( server._io_timeout );
    http::async_read( stream, buffer, *parser,
      [self=shared_from_this()]( beast::error_code ec, std::size_t ) {
        if( ec || self->closed || !self->server._running ) { self->close(); return; }
        try { self->account_bodies(); self->respond(); }
        catch( const std::exception& e ) {
          // Logging must not throw a second allocation failure past io.run().
          try { LOG( warning ) << "[jsonrpc] Session exception: " << e.what(); }
          catch( ... ) {}
          self->close();
        }
        catch( ... ) { self->close(); }
      } );
  }
  void respond()
  {
    const auto& req = parser->get();
    response.emplace();
    response->version( req.version() );
    response->keep_alive( req.keep_alive() );
    response->set( http::field::content_type, "application/json" );
    response->set( http::field::access_control_allow_origin, "*" );
    response->set( http::field::access_control_allow_methods, "POST, OPTIONS" );
    response->set( http::field::access_control_allow_headers, "Content-Type" );
    if( req.method() == http::verb::options )
      response->result( http::status::no_content );
    else if( req.method() == http::verb::get &&
             (req.target() == "/health" || req.target() == "/healthz" || req.target() == "/") )
    {
      response->result( http::status::ok );
      response->body() = nlohmann::json( {
        { "status", "ok" }, { "node", ::koinos::node::node_name() },
        { "version", ::koinos::node::build_version() } } ).dump();
    }
    else if( req.method() != http::verb::post )
    {
      response->result( http::status::method_not_allowed );
      response->body() = R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Only POST allowed"},"id":null})";
    }
    else
    {
      response->result( http::status::ok );
      // Already-admitted service calls run to completion, including mutations.
      // Disconnect/stop cannot imply rollback; stop joins dispatch before the
      // registry releases dependencies. Service execution has no forced timeout.
      response->body() = server.process_body( req.body(), mempool_output_leases, refusal_workspace );
    }
    // Observe both completed bodies before releasing the request, so the
    // transition high water does not hide this real ownership overlap.
    account_bodies();
    // Dispatch has finished using the request. Destroy its string rather than
    // clear/empty-assign: those operations may retain capacity on Linux.
    // Preserve the flat_buffer's unread pipelined bytes for the next read.
    parser.reset();
    account_bodies();
    if( !server._running ) { close(); return; }
    response->prepare_payload();
    stream.expires_after( server._io_timeout );
    http::async_write( stream, *response,
      [self=shared_from_this()]( beast::error_code ec, std::size_t ) {
        // async_write no longer borrows the response after this callback.
        // Cancellation must not destroy it earlier, while writes still use it.
        const bool close_connection = ec || self->closed || self->response->need_eof();
        self->response.reset();
        // clear() alone leaves the vector's capacity charged across keepalive.
        {
          MempoolOutputLeases empty( self->mempool_output_leases.get_allocator() );
          self->mempool_output_leases.swap( empty );
        }
        self->refusal_workspace.reset();
        self->account_bodies();
        if( close_connection ) { self->close(); return; }
        self->read();
      } );
  }
};

JSONRPCServer::JSONRPCServer( IChain* chain,
                              IMempool* mempool,
                              IBlockStore* block_store,
                              contract_meta_store::ContractMetaStore* contract_meta,
                              transaction_store::TransactionStore* tx_store,
                              account_history::AccountHistory* acct_history,
                              const std::string& listen_address,
                              uint16_t port,
                              unsigned int threads,
                              koinos::node::RpcAccessPolicy access_policy,
                              std::chrono::milliseconds io_timeout )
    : _chain( chain ),
      _mempool( mempool ),
      _block_store( block_store ),
      _contract_meta( contract_meta ),
      _tx_store( tx_store ),
      _acct_history( acct_history ),
      _ioc( std::max( threads, 1u ) ),
      _acceptor( _ioc, { net::ip::make_address( listen_address ), port } ),
      _endpoint( _acceptor.local_endpoint() ),
      _accept_strand( net::make_strand( _ioc ) ),
      _thread_count( std::max( threads, 1u ) ),
      _access_policy( std::move( access_policy ) ),
      _io_timeout( io_timeout )
{
  if( threads > 64 || io_timeout.count() <= 0 || io_timeout > std::chrono::seconds( 300 ) )
    throw std::invalid_argument( "JSON-RPC jobs must be at most 64 and I/O timeout must be positive and at most 300 seconds" );
  if( _mempool && _mempool->memory_budget().enabled() )
  {
    OwnedMemoryBudget::Scope scope( _mempool->memory_budget(), true );
    _refusal_workspace = OwnedMemoryBudget( 64 * 1024 * 1024, true );
  }
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
  std::lock_guard lifecycle( _lifecycle_mutex );
  if( _running )
    return;
  _ioc.restart();
  if( !_acceptor.is_open() )
  {
    try
    {
      _acceptor.open( _endpoint.protocol() );
      _acceptor.set_option( tcp::acceptor::reuse_address( true ) );
      _acceptor.bind( _endpoint );
      _acceptor.listen();
    }
    catch( ... )
    {
      beast::error_code ec; _acceptor.close( ec );
      throw;
    }
  }
  _running = true;
  try
  {
    _threads.reserve( _thread_count );
    do_accept();
    for( unsigned int i = 0; i < _thread_count; ++i )
      _threads.emplace_back( [this]() { _ioc.run(); } );
  }
  catch( ... )
  {
    _running = false;
    // Closing an acceptor concurrently with its handler is unsafe. Queue the
    // same cancellation path used by stop, then drain whichever workers exist.
    net::post( _accept_strand, [this] {
      beast::error_code ec; _acceptor.close( ec );
      // cancel() only posts to the session's own strand. Avoid retaining a
      // second strong owner in this accept-strand handler: if that owner were
      // last, Beast's stream could be destroyed here while its timeout handler
      // is still completing on the session strand.
      { std::lock_guard lock( _sessions_mutex );
        for( const auto& entry: _sessions ) entry.second->cancel(); }
    } );
    if( _threads.empty() ) _ioc.run();
    for( auto& thread: _threads ) if( thread.joinable() ) thread.join();
    _threads.clear();
    throw;
  }

  LOG( info ) << "[jsonrpc] Listening on "
              << _acceptor.local_endpoint().address().to_string()
              << ":" << _acceptor.local_endpoint().port()
              << " with " << _thread_count << " threads";
}

void JSONRPCServer::stop()
{
  std::lock_guard lifecycle( _lifecycle_mutex );
  if( !_running.exchange( false ) )
    return;

  // Drain handlers instead of abandoning them with io_context::stop(). Session
  // strands serialize cancellation with reads/writes and service dispatch.
  net::post( _accept_strand, [this] {
    beast::error_code ec;
    _acceptor.close( ec );
    // Keep the registry as the only accept-strand owner. cancel() posts a
    // self-owned close on each session strand; the registry is released there.
    { std::lock_guard lock( _sessions_mutex );
      for( const auto& entry: _sessions ) entry.second->cancel(); }
  } );
  for( auto& t: _threads )
  {
    if( t.joinable() )
      t.join();
  }
  _threads.clear();
}

std::size_t JSONRPCServer::active_sessions() const
{
  std::lock_guard lock( _sessions_mutex );
  return _sessions.size();
}

void JSONRPCServer::release_session( Session* session )
{
  std::lock_guard lock( _sessions_mutex );
  _sessions.erase( session );
}

JSONRPCServer::HTTPBodyMemory JSONRPCServer::http_body_memory() const
{
  std::lock_guard lock( _http_body_mutex );
  return _http_body_memory;
}

void JSONRPCServer::replace_body_capacities( std::size_t old_request, std::size_t old_response,
                                           std::size_t new_request, std::size_t new_response )
{
  std::lock_guard lock( _http_body_mutex );
  _http_body_memory.completed_request_capacity -= old_request;
  _http_body_memory.completed_request_capacity += new_request;
  _http_body_memory.materialized_response_capacity -= old_response;
  _http_body_memory.materialized_response_capacity += new_response;
  _http_body_memory.peak_combined_capacity = std::max( _http_body_memory.peak_combined_capacity,
    _http_body_memory.completed_request_capacity + _http_body_memory.materialized_response_capacity );
}

// ---------------------------------------------------------------------------
// Accept loop
// ---------------------------------------------------------------------------

void JSONRPCServer::do_accept()
{
  _acceptor.async_accept( net::make_strand( _ioc ), net::bind_executor( _accept_strand,
    [this]( beast::error_code ec, Session::session_socket socket ) {
    if( !ec && _running )
    {
      if( active_sessions() < _thread_count )
      {
        auto session = std::make_shared< Session >( *this, std::move( socket ) );
        { std::lock_guard lock( _sessions_mutex ); _sessions.emplace( session.get(), session ); }
        session->start();
      }
      else
      {
        http::response< http::string_body > res{ http::status::service_unavailable, 11 };
        res.set( http::field::content_type, "application/json" );
        res.body() = R"({"jsonrpc":"2.0","error":{"code":-32001,"message":"JSON-RPC session limit reached"},"id":null})";
        res.prepare_payload();
        beast::error_code write_ec;
        // Best-effort small rejection, never wait for an overloaded peer or
        // allocate another worker/session beyond the configured bound.
        socket.non_blocking( true, write_ec );
        if( !write_ec ) http::write( socket, res, write_ec );
      }
    }

    if( _running )
      do_accept();
  } ) );
}

// ---------------------------------------------------------------------------
// JSON-RPC processing
// ---------------------------------------------------------------------------

struct JSONRPCServer::PreparedTransactionReply
{
  OwnedMemoryBudget budget;
  nlohmann::json* envelope = nullptr; // Borrowed only during the synchronous call.
  OwnedMemoryBudget::Lease* batch_credit = nullptr;
  std::string encoded;
  std::exception_ptr serialization_error;
  bool ready = false, completed = false;

  static void prepare( void* context, const rpc::chain::submit_transaction_response& receipt )
  {
    auto& self = *static_cast< PreparedTransactionReply* >( context );
    if( self.ready || !self.envelope )
      throw std::logic_error( "Invalid transaction response preparation lifetime" );
    OwnedMemoryBudget::Scope scope( self.budget, false );
    // Reserve before entering Protobuf's non-exception-safe JSON writer. A
    // quota refusal therefore happens before any writer state exists. The
    // returned JSON string remains covered by the workspace lease while the
    // outer, normally captured nlohmann tree takes ownership of its fields.
    auto conversion_workspace = self.budget.reserve( OwnedMemoryBudget::json_message_workspace(
      OwnedMemoryBudget::protobuf_space_used( receipt ) ) );
    std::string receipt_json;
    {
      OwnedMemoryBudget::AllocationBypass third_party_conversion;
      receipt_json = proto_to_json( receipt );
    }
    self.envelope->at( "result" ) = nlohmann::json::parse( receipt_json );
    try
    {
      self.encoded = self.envelope->dump();
    }
    catch( const nlohmann::json::type_error& error )
    {
      if( error.id != 316 ) throw;
      // Preserve strict UTF-8 failure and later batch dispatch. This reply can
      // never be published, so there is no final-body contribution to reserve.
      self.serialization_error = std::current_exception();
    }
    if( !self.serialization_error && self.batch_credit )
      *self.batch_credit = self.budget.reserve(
        OwnedMemoryBudget::string_reservation( self.encoded.size() ) );
    self.ready = true;
  }

  void discard() noexcept
  {
    std::string().swap( encoded );
    if( batch_credit ) batch_credit->reset();
    serialization_error = {};
  }


  std::string encode_unprepared( const nlohmann::json& response )
  {
    OwnedMemoryBudget::Scope scope( budget, false );
    auto result = response.dump();
    if( batch_credit )
      *batch_credit = budget.reserve( OwnedMemoryBudget::string_reservation( result.size() ) );
    return result;
  }
};

std::string JSONRPCServer::process_body( const std::string& body,
                                          MempoolOutputLeases& mempool_output_leases,
                                          OwnedMemoryBudget::Lease& refusal_workspace )
{
  const auto input_bytes = body.size();
  bool refusal_reserved = false;
  const auto encode_refusal = [&]( const LocalCapacityExceeded& error,
                                   const nlohmann::json& request, bool batch ) {
    // Admit one conservative envelope for this complete HTTP request before
    // allocating a fallback. The 1-MiB frame limit also bounds its borrowed
    // IDs; the fixed shared gate bounds concurrent retained refusals. A full
    // gate propagates local overload to the session's connection-close path.
    if( !refusal_reserved )
    {
      refusal_workspace = _refusal_workspace.reserve(
        OwnedMemoryBudget::checked_add( OwnedMemoryBudget::checked_multiply( input_bytes, 32 ), 65536 ) );
      refusal_reserved = true;
    }
    OwnedMemoryBudget::Scope scope( _mempool->memory_budget(), true );
    auto envelope = make_local_capacity_error( error, nullptr );
    JsonCleanup envelope_cleanup( envelope );
    std::string encoded;
    StringAppendBuffer buffer( encoded );
    std::ostream output( &buffer );
    output.exceptions( std::ios::badbit | std::ios::failbit );
    // Preserve the sorted object bytes while borrowing the original ID. Do
    // not clone an arbitrarily large ID/tree to report exhausted admission.
    output << "{\"error\":" << envelope.at( "error" ) << ",\"id\":";
    if( request.contains( "id" ) ) output << request.at( "id" );
    else output << "null";
    output << ",\"jsonrpc\":\"2.0\"}";
    if( batch )
      mempool_output_leases.push_back( _mempool->memory_budget().reserve(
        OwnedMemoryBudget::string_reservation( encoded.size() ) ) );
    return encoded;
  };
  nlohmann::json parsed;
  JsonCleanup parsed_cleanup( parsed );
  try
  {
    parsed = nlohmann::json::parse( body );
  }
  catch( ... )
  {
    return dump_owned_json( make_error( PARSE_ERROR, "Parse error" ) );
  }

  const auto admitted_mempool_request = [this]( const nlohmann::json& item ) {
    if( !_mempool || !item.is_object() || !item.contains( "method" ) ||
        !item[ "method" ].is_string() || !item.contains( "jsonrpc" ) ||
        !item[ "jsonrpc" ].is_string() ||
        item[ "jsonrpc" ].get_ref< const std::string& >() != "2.0" ) return false;
    const auto& name = item[ "method" ].get_ref< const std::string& >();
    const auto [ service, method ] = parse_method( name );
    return service == "mempool" && _access_policy.allows( service, method );
  };

  const auto admitted_transaction_request = [this]( const nlohmann::json& item ) {
    if( !_chain || !_mempool || !_mempool->memory_budget().enabled() ||
        !item.is_object() || !item.contains( "method" ) ||
        !item[ "method" ].is_string() || !item.contains( "jsonrpc" ) ||
        !item[ "jsonrpc" ].is_string() ||
        item[ "jsonrpc" ].get_ref< const std::string& >() != "2.0" ) return false;
    const auto [ service, method ] = parse_method( item[ "method" ].get_ref< const std::string& >() );
    return service == "chain" && method == "submit_transaction" &&
           _access_policy.allows( service, method );
  };

  // Batch request (JSON array)
  if( parsed.is_array() )
  {
    std::string body = "[";
    StringAppendBuffer buffer( body );
    std::ostream output( &buffer );
    output.exceptions( std::ios::badbit | std::ios::failbit );
    std::exception_ptr serialization_error;
    bool first = true;
    for( const auto& item: parsed )
    {
      PreparedTransactionReply prepared;
      std::optional< LocalCapacityExceeded > capacity_error;
      nlohmann::json response;
      JsonCleanup response_cleanup( response );
      if( admitted_transaction_request( item ) )
      {
        prepared.budget = _mempool->memory_budget();
        try
        {
          OwnedMemoryBudget::Scope scope( prepared.budget, false );
          // Allocate the lease slot before calling a mutating service. Neither
          // quota admission nor vector growth may fail after its acceptance.
          mempool_output_leases.emplace_back();
          prepared.batch_credit = &mempool_output_leases.back();
          response = process_single_request( item, &prepared );
        }
        catch( const LocalCapacityExceeded& error )
        {
          capacity_error = error;
        }
        if( !prepared.completed ) prepared.discard();
      }
      else if( admitted_mempool_request( item ) )
      {
        try
        {
          OwnedMemoryBudget::Scope scope( _mempool->memory_budget(), false );
          response = process_single_request( item );
        }
        catch( const LocalCapacityExceeded& error ) { capacity_error = error; }
      }
      else response = process_single_request( item );
      if( serialization_error )
        continue;
      try
      {
        if( capacity_error )
        {
          const auto serialized = encode_refusal( *capacity_error, item, true );
          if( !first ) output.put( ',' );
          output << serialized;
          first = false;
          continue;
        }
        if( prepared.completed )
        {
          if( prepared.serialization_error ) std::rethrow_exception( prepared.serialization_error );
          if( !first ) output.put( ',' );
          output << prepared.encoded;
          first = false;
          continue;
        }
        if( admitted_transaction_request( item ) )
        {
          std::string serialized;
          try
          {
            // Transaction failures can carry large contract-provided text.
            // Attribute the final error body just like a successful receipt;
            // a refusal becomes a small, explicitly local overload response.
            serialized = prepared.encode_unprepared( response );
          }
          catch( const LocalCapacityExceeded& error )
          {
            serialized = encode_refusal( error, item, true );
          }
          if( !first ) output.put( ',' );
          output << serialized;
          first = false;
          continue;
        }
        if( admitted_mempool_request( item ) )
        {
          const auto budget = _mempool->memory_budget();
          std::string serialized;
          try
          {
            OwnedMemoryBudget::Scope output_scope( budget );
            serialized = response.dump();
            // Reserve the mempool contribution before it is copied into the
            // shared body. Later non-mempool items may reallocate that body;
            // this lease stays with the HTTP response until write completion.
            auto body_credit = budget.reserve( OwnedMemoryBudget::string_reservation( serialized.size() ) );
            mempool_output_leases.push_back( std::move( body_credit ) );
          }
          catch( const LocalCapacityExceeded& error )
          {
            serialized = encode_refusal( error, item, true );
          }
          if( !first ) output.put( ',' );
          output << serialized;
          first = false;
          continue;
        }
        if( !first )
          output.put( ',' );
        output << response;
        first = false;
      }
      catch( const nlohmann::json::type_error& error )
      {
        if( error.id != 316 )
          throw;
        // Previously every item ran before strict UTF-8 dump failed. Preserve
        // those later service calls, but discard the unusable partial body.
        serialization_error = std::current_exception();
        std::string().swap( body );
      }
    }
    if( serialization_error )
      std::rethrow_exception( serialization_error );
    output.put( ']' );
    return body;
  }

  if( admitted_transaction_request( parsed ) )
  {
    PreparedTransactionReply prepared;
    prepared.budget = _mempool->memory_budget();
    nlohmann::json response;
    JsonCleanup response_cleanup( response );
    try
    {
      OwnedMemoryBudget::Scope scope( prepared.budget, false );
      response = process_single_request( parsed, &prepared );
    }
    catch( const LocalCapacityExceeded& error )
    {
      prepared.discard();
      return encode_refusal( error, parsed, false );
    }
    if( prepared.completed )
    {
      if( prepared.serialization_error ) std::rethrow_exception( prepared.serialization_error );
      // Move the already encoded allocation; its physical final owner keeps
      // M1 credit until the asynchronous HTTP write has stopped borrowing it.
      return std::move( prepared.encoded );
    }
    prepared.discard();
    try
    {
      return prepared.encode_unprepared( response );
    }
    catch( const LocalCapacityExceeded& error )
    {
      return encode_refusal( error, parsed, false );
    }
  }

  // Keep the complete mempool reply attributed through its final HTTP body
  // owner. The service result tree alone ends before dump() allocates the body.
  // Only an allowed mempool method enters this domain; unrelated services do
  // not inherit its optional local admission policy.
  if( admitted_mempool_request( parsed ) )
  {
    try
    {
      OwnedMemoryBudget::Scope output_scope( _mempool->memory_budget() );
      return dump_owned_json( process_single_request( parsed ) );
    }
    catch( const LocalCapacityExceeded& error )
    {
      return encode_refusal( error, parsed, false );
    }
  }

  // Single request outside the mempool domain.
  return dump_owned_json( process_single_request( parsed ) );
}

nlohmann::json JSONRPCServer::process_single_request( const nlohmann::json& request,
                                                       PreparedTransactionReply* prepared )
{
  // Extract ID (default null)
  nlohmann::json id = nullptr;
  JsonCleanup id_cleanup( id );
  if( request.contains( "id" ) )
    id = request[ "id" ];

  // Validate jsonrpc version
  // Comparing JSON directly with a string literal constructs a temporary JSON
  // string inside nlohmann's noexcept scalar comparison. Read the existing
  // string instead: exhausted local admission must propagate, never terminate.
  if( !request.contains( "jsonrpc" ) || !request[ "jsonrpc" ].is_string() ||
      request[ "jsonrpc" ].get_ref< const std::string& >() != "2.0" )
    return make_error( INVALID_REQUEST, "Invalid JSON-RPC version", id );

  // Validate method
  if( !request.contains( "method" ) || !request[ "method" ].is_string() )
    return make_error( INVALID_REQUEST, "Missing or invalid method", id );

  const auto& method_str = request[ "method" ].get_ref< const std::string& >();

  // Parse method → service + method
  auto [service, method] = parse_method( method_str );
  if( service.empty() || method.empty() )
    return make_error( METHOD_NOT_FOUND, "Malformed method: " + method_str, id );

  if( !_access_policy.allows( service, method ) )
    return make_error( METHOD_NOT_FOUND, "RPC method denied by access policy: " + service + "." + method, id );

  // The parsed request outlives this synchronous dispatch. Normalization owns
  // its mutable copy, so borrowing here avoids a second complete encoded tree.
  const bool has_params = request.contains( "params" ) && !request[ "params" ].is_null();
  nlohmann::json empty_params;
  JsonCleanup empty_params_cleanup( empty_params );
  if( !has_params ) empty_params = nlohmann::json::object();
  const auto& params = has_params ? request[ "params" ] : empty_params;

  // Dispatch to service
  try
  {
    // Prepare envelope/ID ownership before a potentially mutating service call.
    // Moving into the existing slot avoids copying the returned result tree.
    auto response = make_result( id );
    JsonCleanup response_cleanup( response );
    auto normalized_params = normalize_koinos_params( service, method, params );
    JsonCleanup normalized_params_cleanup( normalized_params );
    if( prepared )
    {
      rpc::chain::submit_transaction_request native_request;
      json_to_proto( normalized_params, native_request );
      // The decoded request now owns the native fields for the complete call.
      clear_json_without_allocation( normalized_params );
      prepared->envelope = &response;
      // The native adapter prepares this exact envelope/body before publishing
      // accepted work. Unsupported adapters refuse before executing anything.
      (void)_chain->submit_transaction_with_prepared_response( native_request,
        { prepared, &PreparedTransactionReply::prepare } );
      prepared->envelope = nullptr;
      if( !prepared->ready )
        throw std::logic_error( "Native transaction response was not prepared" );
      prepared->completed = true;
      // The cleanup guard owns the local, so do not allow NRVO to alias it to
      // the caller's returned tree. Moving leaves only null to clean locally.
      return std::move( response );
    }
    auto& result = response.at( "result" );
    result = dispatch( service, method, normalized_params );
    return std::move( response );
  }
  catch( const LocalCapacityExceeded& e )
  {
    return make_local_capacity_error( e, id );
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
  // Borrow the input and locate its final two fields. An iostream can swallow
  // allocation exceptions as stream state, incorrectly reporting a malformed
  // method instead of local capacity. Avoid its copies and intermediate vector.
  // getline ignores one empty field after a final delimiter; preserve that
  // existing behavior, including internal empty fields and repeated final dots.
  auto end = method_str.size();
  if( end && method_str[ end - 1 ] == '.' ) --end;
  if( !end ) return { "", "" };
  const auto separator = method_str.rfind( '.', end - 1 );
  if( separator == std::string::npos )
    return { "", "" };
  const auto previous = separator ? method_str.rfind( '.', separator - 1 ) : std::string::npos;
  const auto start = previous == std::string::npos ? 0 : previous + 1;
  return { method_str.substr( start, separator - start ),
           method_str.substr( separator + 1, end - separator - 1 ) };
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
    JsonCleanup status_cleanup( status );
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

    return std::move( status );
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

  OwnedMemoryBudget::Scope result_memory_scope( _mempool->memory_budget() );
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

nlohmann::json JSONRPCServer::make_local_capacity_error( const LocalCapacityExceeded& error,
                                                         const nlohmann::json& id )
{
  auto response = make_error( -32005, error.what(), id );
  JsonCleanup response_cleanup( response );
  response[ "error" ][ "data" ] = {
    { "retryable", true }, { "reason", error.reason_code() }, { "scope", "local" }
  };
  return std::move( response );
}

nlohmann::json JSONRPCServer::make_result( const nlohmann::json& id )
{
  return {
    { "jsonrpc", "2.0" },
    { "result", nullptr },
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
    JsonCleanup json_cleanup( json );
    rewrite_koinos_json_bytes( msg, json );
    return json.dump();
  }
  catch( const LocalCapacityExceeded& )
  {
    // Capacity refusal must not fall back to the unrewritten protobuf JSON
    // encoding (and must precede transaction admission for prepared replies).
    throw;
  }
  catch( const std::bad_alloc& )
  {
    throw;
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
