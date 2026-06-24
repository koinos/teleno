#include "backup/backup_admin_server.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#include <koinos/log.hpp>

namespace koinos::node::backup {
namespace {

class RequestValidationError : public std::runtime_error
{
public:
  explicit RequestValidationError( const std::string& message )
    : std::runtime_error( message )
  {}
};

nlohmann::json parse_request_body( const std::string& body )
{
  if( body.empty() )
    return nlohmann::json::object();

  try
  {
    auto request = nlohmann::json::parse( body );
    if( !request.is_object() )
      throw RequestValidationError( "request body must be a JSON object" );
    return request;
  }
  catch( const nlohmann::json::parse_error& e )
  {
    throw RequestValidationError( std::string( "invalid JSON request body: " ) + e.what() );
  }
}

std::string optional_string_field( const nlohmann::json& request,
                                   const std::string& field )
{
  if( !request.contains( field ) || request.at( field ).is_null() )
    return {};
  if( !request.at( field ).is_string() )
    throw RequestValidationError( field + " must be a string" );
  return request.at( field ).get< std::string >();
}

bool optional_bool_field( const nlohmann::json& request,
                          const std::string& field,
                          bool default_value )
{
  if( !request.contains( field ) || request.at( field ).is_null() )
    return default_value;
  if( !request.at( field ).is_boolean() )
    throw RequestValidationError( field + " must be a boolean" );
  return request.at( field ).get< bool >();
}

std::string backup_id_from_request( const nlohmann::json& request )
{
  auto backup_id = optional_string_field( request, "backup_id" );
  return backup_id.empty() ? std::string( "latest" ) : backup_id;
}

std::filesystem::path staging_dir_from_request( const nlohmann::json& request )
{
  return optional_string_field( request, "staging_dir" );
}

bool is_route_or_child( const std::string& target, const std::string& route )
{
  return target == route || target.rfind( route + "/", 0 ) == 0;
}

std::string request_path( const std::string& target )
{
  const auto query_pos = target.find( '?' );
  return query_pos == std::string::npos ? target : target.substr( 0, query_pos );
}

std::string request_query( const std::string& target )
{
  const auto query_pos = target.find( '?' );
  return query_pos == std::string::npos ? std::string{} : target.substr( query_pos + 1 );
}

std::string query_param_value( const std::string& query, const std::string& key )
{
  std::size_t pos = 0;
  while( pos <= query.size() )
  {
    const auto next = query.find( '&', pos );
    const auto token = query.substr( pos, next == std::string::npos ? std::string::npos : next - pos );
    const auto eq = token.find( '=' );
    const auto name = eq == std::string::npos ? token : token.substr( 0, eq );
    if( name == key )
      return eq == std::string::npos ? std::string{} : token.substr( eq + 1 );
    if( next == std::string::npos )
      break;
    pos = next + 1;
  }
  return {};
}

bool query_param_true( const std::string& query, const std::string& key )
{
  const auto value = query_param_value( query, key );
  return value == "1" || value == "true" || value == "yes";
}

std::size_t query_limit( const std::string& query, std::size_t default_limit )
{
  const auto value = query_param_value( query, "limit" );
  if( value.empty() )
    return default_limit;
  try
  {
    const auto parsed = std::stoull( value );
    return static_cast< std::size_t >( std::min< unsigned long long >( parsed, 1000 ) );
  }
  catch( ... )
  {
    return default_limit;
  }
}

std::string child_route_id( const std::string& target, const std::string& route )
{
  const auto prefix = route + "/";
  if( target.rfind( prefix, 0 ) != 0 )
    return {};
  return target.substr( prefix.size() );
}

std::uint64_t now_milliseconds()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return static_cast< std::uint64_t >(
    std::chrono::duration_cast< std::chrono::milliseconds >( now ).count() );
}

std::string multiaddr_component( const std::string& address, const std::string& component )
{
  const auto prefix = "/" + component + "/";
  const auto start = address.find( prefix );
  if( start == std::string::npos )
    return {};
  const auto value_start = start + prefix.size();
  const auto value_end = address.find( '/', value_start );
  return address.substr( value_start, value_end == std::string::npos ? std::string::npos : value_end - value_start );
}

nlohmann::json peer_row_to_json( const AdminPeerRow& row, bool connected )
{
  nlohmann::json peer;
  const auto peer_id = row.peer_id.empty() ? multiaddr_component( row.address, "p2p" ) : row.peer_id;
  const auto host_ip4 = multiaddr_component( row.address, "ip4" );
  const auto host_ip6 = multiaddr_component( row.address, "ip6" );
  const auto host_dns4 = multiaddr_component( row.address, "dns4" );
  const auto host_dns6 = multiaddr_component( row.address, "dns6" );
  const auto host = !host_ip4.empty() ? host_ip4
    : !host_ip6.empty() ? host_ip6
    : !host_dns4.empty() ? host_dns4
    : host_dns6;
  const auto port_value = multiaddr_component( row.address, "tcp" );

  peer[ "peer_id" ] = peer_id.empty() ? nlohmann::json{} : nlohmann::json( peer_id );
  peer[ "address" ] = row.address;
  peer[ "host" ] = host.empty() ? nlohmann::json{} : nlohmann::json( host );
  if( port_value.empty() )
  {
    peer[ "port" ] = nullptr;
  }
  else
  {
    try
    {
      const auto parsed = std::stoul( port_value );
      peer[ "port" ] = parsed <= std::numeric_limits< std::uint16_t >::max()
        ? nlohmann::json( parsed )
        : nlohmann::json();
    }
    catch( ... )
    {
      peer[ "port" ] = nullptr;
    }
  }
  peer[ "connected" ] = connected;
  return peer;
}

std::string bad_request_response( const std::string& error )
{
  nlohmann::json response;
  response[ "ok" ] = false;
  response[ "error" ] = error;
  return response.dump();
}

} // anonymous namespace

BackupAdminServer::BackupAdminServer( BackupService* backup_service,
                                      const std::string& listen_address,
                                      uint16_t port,
                                      unsigned int threads,
                                      std::string bearer_token,
                                      PeerSnapshotProvider peer_snapshot_provider )
  : _backup_service( backup_service ),
    _listen_address( listen_address == "localhost" ? "127.0.0.1" : listen_address ),
    _bearer_token( std::move( bearer_token ) ),
    _peer_snapshot_provider( std::move( peer_snapshot_provider ) ),
    _ioc( std::max( threads, 1u ) ),
    _acceptor( _ioc, { loopback_address_or_throw( _listen_address ), port } ),
    _thread_count( std::max( threads, 1u ) )
{
  if( !_backup_service )
    throw std::runtime_error( "backup admin server requires a backup service" );
}

BackupAdminServer::~BackupAdminServer()
{
  stop();
}

net::ip::address BackupAdminServer::loopback_address_or_throw( const std::string& listen_address )
{
  beast::error_code ec;
  auto address = net::ip::make_address( listen_address, ec );
  if( ec )
    throw std::runtime_error( "backup admin listen address must be a numeric loopback address: " + listen_address );
  if( !address.is_loopback() )
    throw std::runtime_error( "backup admin listen address must be loopback-only: " + listen_address );
  return address;
}

void BackupAdminServer::start()
{
  if( _running.exchange( true ) )
    return;

  do_accept();
  _threads.reserve( _thread_count );
  for( unsigned int i = 0; i < _thread_count; ++i )
    _threads.emplace_back( [this]() { _ioc.run(); } );

  LOG( info ) << "[backup_admin] Listening on "
              << _acceptor.local_endpoint().address().to_string()
              << ":" << _acceptor.local_endpoint().port()
              << " with " << _thread_count << " threads";
}

void BackupAdminServer::stop()
{
  if( !_running.exchange( false ) )
    return;

  beast::error_code ec;
  _acceptor.close( ec );
  _ioc.stop();
  for( auto& thread: _threads )
  {
    if( thread.joinable() )
      thread.join();
  }
  _threads.clear();

  {
    std::lock_guard< std::mutex > lock( _session_mutex );
    for( auto& thread: _session_threads )
    {
      if( thread.joinable() )
        thread.join();
    }
    _session_threads.clear();
  }
}

uint16_t BackupAdminServer::port() const
{
  if( !_acceptor.is_open() )
    return 0;
  return _acceptor.local_endpoint().port();
}

void BackupAdminServer::do_accept()
{
  _acceptor.async_accept( [this]( beast::error_code ec, tcp::socket socket ) {
    if( !ec && _running )
    {
      std::lock_guard< std::mutex > lock( _session_mutex );
      _session_threads.emplace_back( [this, s = std::move( socket )]() mutable {
        handle_session( std::move( s ) );
      } );
    }

    if( _running )
      do_accept();
  } );
}

bool BackupAdminServer::is_admin_target( const std::string& target ) const
{
  return target.rfind( "/admin/backup/", 0 ) == 0
    || target.rfind( "/admin/p2p/", 0 ) == 0;
}

bool BackupAdminServer::request_authorized( const http::request< http::string_body >& req ) const
{
  if( _bearer_token.empty() )
    return true;

  const auto authorization = req[ http::field::authorization ];
  const auto expected = std::string( "Bearer " ) + _bearer_token;
  return std::string( authorization ) == expected;
}

std::string BackupAdminServer::status_response( const BackupOperationStatus& status ) const
{
  return backup_operation_status_to_json( status );
}

std::string BackupAdminServer::operation_not_found_response( const std::string& operation_id,
                                                             const BackupOperationStatus& status ) const
{
  nlohmann::json response;
  response[ "ok" ] = false;
  response[ "error" ] = "operation not found";
  response[ "operation_id" ] = operation_id;
  response[ "status" ] = nlohmann::json::parse( backup_operation_status_to_json( status ) );
  return response.dump();
}

std::string BackupAdminServer::config_response()
{
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "config" ] = nlohmann::json::parse( _backup_service->config_summary_json() );
  return response.dump();
}

std::string BackupAdminServer::public_config_response()
{
  auto config = nlohmann::json::parse( _backup_service->config_summary_json() );
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "basedir" ] = config.at( "basedir" );
  response[ "config_path" ] = config.at( "config_path" );
  response[ "local_repository" ] = config.at( "backup" ).at( "local" ).at( "directory" );
  response[ "public_restore" ] = config.at( "backup" ).at( "public_restore" );
  return response.dump();
}

std::string BackupAdminServer::list_response( bool remote )
{
  const auto result = remote
    ? _backup_service->list_remote_snapshots()
    : _backup_service->list_local_snapshots();
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "source" ] = remote ? "remote_sftp" : "local";
  response[ "snapshots" ] = nlohmann::json::parse( backup_snapshot_list_result_to_json( result ) );
  return response.dump();
}

std::string BackupAdminServer::public_list_response()
{
  auto result = _backup_service->list_public_snapshots();
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "source" ] = "public_http";
  response[ "snapshots" ] = nlohmann::json::parse( backup_snapshot_list_result_to_json( result ) );
  return response.dump();
}

std::string BackupAdminServer::create_response( const std::string& body )
{
  const auto request = parse_request_body( body );
  auto configured = nlohmann::json::parse( _backup_service->config_summary_json() );
  const bool remote_default = configured[ "backup" ][ "remote" ].value( "enabled", false );
  const bool upload_remote = optional_bool_field( request, "remote", remote_default );
  auto status = _backup_service->start_configured_backup_async( upload_remote );
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "status" ] = nlohmann::json::parse( backup_operation_status_to_json( status ) );
  return response.dump();
}

std::string BackupAdminServer::upload_latest_response()
{
  auto status = _backup_service->start_upload_latest_async();
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "status" ] = nlohmann::json::parse( backup_operation_status_to_json( status ) );
  return response.dump();
}

std::string BackupAdminServer::delete_response( const std::string& body )
{
  const auto request = parse_request_body( body );
  const auto scope = optional_string_field( request, "scope" ).empty()
    ? std::string( "local" )
    : optional_string_field( request, "scope" );
  const auto backup_id = optional_string_field( request, "backup_id" );
  const auto confirm = optional_string_field( request, "confirm" );
  auto status = _backup_service->start_delete_async( scope, backup_id, confirm );
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "status" ] = nlohmann::json::parse( backup_operation_status_to_json( status ) );
  return response.dump();
}

std::string BackupAdminServer::restore_fetch_response( const std::string& body )
{
  const auto request = parse_request_body( body );
  auto status = _backup_service->start_restore_fetch_async( backup_id_from_request( request ) );
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "status" ] = nlohmann::json::parse( backup_operation_status_to_json( status ) );
  return response.dump();
}

std::string BackupAdminServer::public_restore_fetch_response( const std::string& body )
{
  const auto request = parse_request_body( body );
  auto status = _backup_service->start_public_restore_fetch_async( backup_id_from_request( request ) );
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "status" ] = nlohmann::json::parse( backup_operation_status_to_json( status ) );
  return response.dump();
}

std::string BackupAdminServer::restore_preflight_response( const std::string& body )
{
  const auto request = parse_request_body( body );
  auto result = _backup_service->restore_preflight( backup_id_from_request( request ) );
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "preflight" ] = nlohmann::json::parse( restore_preflight_result_to_json( result ) );
  return response.dump();
}

std::string BackupAdminServer::public_restore_preflight_response( const std::string& body )
{
  const auto request = parse_request_body( body );
  auto result = _backup_service->public_restore_preflight( backup_id_from_request( request ) );
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "preflight" ] = nlohmann::json::parse( restore_preflight_result_to_json( result ) );
  return response.dump();
}

std::string BackupAdminServer::cancel_response( const BackupOperationStatus& status )
{
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "status" ] = nlohmann::json::parse( backup_operation_status_to_json( status ) );
  return response.dump();
}

std::string BackupAdminServer::restore_stage_response( const std::string& body )
{
  const auto request = parse_request_body( body );
  auto result = _backup_service->stage_restore_snapshot(
    backup_id_from_request( request ),
    staging_dir_from_request( request ) );
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "stage" ] = nlohmann::json::parse( restore_stage_result_to_json( result ) );
  return response.dump();
}

std::string BackupAdminServer::restore_activate_response( const std::string& body )
{
  const auto request = parse_request_body( body );
  auto result = _backup_service->request_restore_activation(
    backup_id_from_request( request ),
    staging_dir_from_request( request ) );
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "activation_request" ] =
    nlohmann::json::parse( restore_activation_request_to_json( result ) );
  return response.dump();
}

std::string BackupAdminServer::p2p_peers_response( const std::string& target ) const
{
  const auto query = request_query( target );
  const bool include_known = query_param_true( query, "include_known" )
    || query_param_true( query, "include-known" );
  const auto limit = query_limit( query, 100 );

  nlohmann::json response;
  response[ "source" ] = "p2p-live";
  response[ "snapshot_at" ] = now_milliseconds();

  if( !_peer_snapshot_provider )
  {
    response[ "ok" ] = false;
    response[ "p2p_running" ] = false;
    response[ "connected_count" ] = 0;
    response[ "known_count" ] = 0;
    response[ "self_address" ] = nullptr;
    response[ "connected" ] = nlohmann::json::array();
    response[ "known" ] = nlohmann::json::array();
    response[ "error" ] = "p2p live peer provider is not configured";
    return response.dump();
  }

  const auto snapshot = _peer_snapshot_provider();
  response[ "ok" ] = snapshot.p2p_running;
  response[ "p2p_running" ] = snapshot.p2p_running;
  response[ "connected_count" ] = snapshot.connected.size();
  response[ "known_count" ] = snapshot.known.size();
  response[ "self_address" ] =
    snapshot.self_address.empty() ? nlohmann::json{} : nlohmann::json( snapshot.self_address );

  response[ "connected" ] = nlohmann::json::array();
  for( std::size_t i = 0; i < snapshot.connected.size() && i < limit; ++i )
    response[ "connected" ].push_back( peer_row_to_json( snapshot.connected[ i ], true ) );

  response[ "known" ] = nlohmann::json::array();
  if( include_known )
  {
    for( std::size_t i = 0; i < snapshot.known.size() && i < limit; ++i )
      response[ "known" ].push_back( peer_row_to_json( snapshot.known[ i ], false ) );
  }

  if( !snapshot.p2p_running )
    response[ "error" ] = "p2p is not running";

  return response.dump();
}

void BackupAdminServer::handle_session( tcp::socket socket )
{
  try
  {
    beast::flat_buffer buffer;
    http::request< http::string_body > req;
    http::read( socket, buffer, req );

    http::response< http::string_body > res;
    res.version( req.version() );
    res.keep_alive( false );
    res.set( http::field::content_type, "application/json" );

    const auto target = std::string( req.target() );
    const auto route_path = request_path( target );
    if( is_admin_target( route_path ) && !request_authorized( req ) )
    {
      res.result( http::status::unauthorized );
      res.set( http::field::www_authenticate, "Bearer" );
      res.body() = R"({"ok":false,"error":"unauthorized"})";
      res.prepare_payload();
      http::write( socket, res );

      beast::error_code ec;
      socket.shutdown( tcp::socket::shutdown_send, ec );
      return;
    }

    try
    {
      if( req.method() == http::verb::get
          && is_route_or_child( route_path, "/admin/backup/status" ) )
      {
        const auto operation_id = child_route_id( route_path, "/admin/backup/status" );
        const auto has_operation_id = route_path != "/admin/backup/status";
        if( has_operation_id && operation_id.empty() )
        {
          res.result( http::status::bad_request );
          res.body() = bad_request_response( "operation id is required" );
        }
        else
        {
          const auto status = _backup_service->status();
          if( has_operation_id && status.operation_id != operation_id )
          {
            res.result( http::status::not_found );
            res.body() = operation_not_found_response( operation_id, status );
          }
          else
          {
            res.result( http::status::ok );
            res.body() = status_response( status );
          }
        }
      }
      else if( req.method() == http::verb::get && route_path == "/admin/backup/config" )
      {
        res.result( http::status::ok );
        res.body() = config_response();
      }
      else if( req.method() == http::verb::get && route_path == "/admin/backup/public/config" )
      {
        res.result( http::status::ok );
        res.body() = public_config_response();
      }
      else if( req.method() == http::verb::get && route_path == "/admin/backup/public/snapshots" )
      {
        res.result( http::status::ok );
        res.body() = public_list_response();
      }
      else if( req.method() == http::verb::get && route_path == "/admin/backup/snapshots/local" )
      {
        res.result( http::status::ok );
        res.body() = list_response( false );
      }
      else if( req.method() == http::verb::get && route_path == "/admin/backup/snapshots/remote" )
      {
        res.result( http::status::ok );
        res.body() = list_response( true );
      }
      else if( req.method() == http::verb::post && route_path == "/admin/backup/create" )
      {
        res.result( http::status::accepted );
        res.body() = create_response( req.body() );
      }
      else if( req.method() == http::verb::post && route_path == "/admin/backup/upload-latest" )
      {
        res.result( http::status::accepted );
        res.body() = upload_latest_response();
      }
      else if( req.method() == http::verb::post && route_path == "/admin/backup/delete" )
      {
        res.result( http::status::accepted );
        res.body() = delete_response( req.body() );
      }
      else if( req.method() == http::verb::post
               && is_route_or_child( route_path, "/admin/backup/cancel" ) )
      {
        const auto operation_id = child_route_id( route_path, "/admin/backup/cancel" );
        const auto has_operation_id = route_path != "/admin/backup/cancel";
        if( has_operation_id && operation_id.empty() )
        {
          res.result( http::status::bad_request );
          res.body() = bad_request_response( "operation id is required" );
        }
        else
        {
          auto status = _backup_service->status();
          if( has_operation_id && status.operation_id != operation_id )
          {
            res.result( http::status::not_found );
            res.body() = operation_not_found_response( operation_id, status );
          }
          else
          {
            status = _backup_service->cancel_current_operation();
            res.result( http::status::accepted );
            res.body() = cancel_response( status );
          }
        }
      }
      else if( req.method() == http::verb::post && route_path == "/admin/backup/restore/stage" )
      {
        res.result( http::status::ok );
        res.body() = restore_stage_response( req.body() );
      }
      else if( req.method() == http::verb::post && route_path == "/admin/backup/public/fetch" )
      {
        res.result( http::status::accepted );
        res.body() = public_restore_fetch_response( req.body() );
      }
      else if( req.method() == http::verb::post && route_path == "/admin/backup/public/preflight" )
      {
        res.result( http::status::ok );
        res.body() = public_restore_preflight_response( req.body() );
      }
      else if( req.method() == http::verb::post && route_path == "/admin/backup/public/restore/stage" )
      {
        res.result( http::status::ok );
        res.body() = restore_stage_response( req.body() );
      }
      else if( req.method() == http::verb::post && route_path == "/admin/backup/public/restore/activate" )
      {
        res.result( http::status::accepted );
        res.body() = restore_activate_response( req.body() );
      }
      else if( req.method() == http::verb::post && route_path == "/admin/backup/restore/fetch" )
      {
        res.result( http::status::accepted );
        res.body() = restore_fetch_response( req.body() );
      }
      else if( req.method() == http::verb::post && route_path == "/admin/backup/restore/preflight" )
      {
        res.result( http::status::ok );
        res.body() = restore_preflight_response( req.body() );
      }
      else if( req.method() == http::verb::post && route_path == "/admin/backup/restore/activate" )
      {
        res.result( http::status::accepted );
        res.body() = restore_activate_response( req.body() );
      }
      else if( req.method() == http::verb::get && route_path == "/admin/p2p/peers" )
      {
        res.result( http::status::ok );
        res.body() = p2p_peers_response( target );
      }
      else if( req.method() == http::verb::get && ( route_path == "/health" || route_path == "/healthz" ) )
      {
        res.result( http::status::ok );
        res.body() = R"({"status":"ok","service":"backup_admin"})";
      }
      else
      {
        res.result( http::status::not_found );
        res.body() = R"({"ok":false,"error":"not found"})";
      }
    }
    catch( const RequestValidationError& e )
    {
      res.result( http::status::bad_request );
      res.body() = bad_request_response( e.what() );
    }
    catch( const std::invalid_argument& e )
    {
      res.result( http::status::bad_request );
      res.body() = bad_request_response( e.what() );
    }
    catch( const std::exception& e )
    {
      res.result( http::status::internal_server_error );
      nlohmann::json error;
      error[ "ok" ] = false;
      error[ "error" ] = e.what();
      error[ "status" ] = nlohmann::json::parse( backup_operation_status_to_json( _backup_service->status() ) );
      res.body() = error.dump();
    }

    if( is_admin_target( route_path ) || route_path == "/health" || route_path == "/healthz" )
    {
      LOG( info ) << "[backup_admin] HTTP request"
                  << " method=" << req.method_string()
                  << " target=" << target
                  << " status=" << static_cast< unsigned int >( res.result_int() );
    }

    res.prepare_payload();
    http::write( socket, res );

    beast::error_code ec;
    socket.shutdown( tcp::socket::shutdown_send, ec );
  }
  catch( const beast::system_error& e )
  {
    if( e.code() != http::error::end_of_stream )
      LOG( debug ) << "[backup_admin] Session error: " << e.code().message();
  }
  catch( const std::exception& e )
  {
    LOG( warning ) << "[backup_admin] Session exception: " << e.what();
  }
  catch( ... )
  {
    LOG( warning ) << "[backup_admin] Session exception: unknown exception";
  }
}

} // namespace koinos::node::backup
