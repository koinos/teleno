#include "backup/backup_admin_server.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#include <koinos/log.hpp>

namespace koinos::node::backup {
namespace {

std::filesystem::path staging_dir_from_request_body( const std::string& body )
{
  if( body.empty() )
    return {};

  const auto request = nlohmann::json::parse( body );
  return request.value( "staging_dir", std::string{} );
}

bool is_route_or_child( const std::string& target, const std::string& route )
{
  return target == route || target.rfind( route + "/", 0 ) == 0;
}

std::string child_route_id( const std::string& target, const std::string& route )
{
  const auto prefix = route + "/";
  if( target.rfind( prefix, 0 ) != 0 )
    return {};
  return target.substr( prefix.size() );
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
                                      std::string bearer_token )
  : _backup_service( backup_service ),
    _listen_address( listen_address == "localhost" ? "127.0.0.1" : listen_address ),
    _bearer_token( std::move( bearer_token ) ),
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
  return target.rfind( "/admin/backup/", 0 ) == 0;
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

std::string BackupAdminServer::create_response()
{
  auto status = _backup_service->start_local_snapshot_async();
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "status" ] = nlohmann::json::parse( backup_operation_status_to_json( status ) );
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
  auto result = _backup_service->stage_restore_snapshot( staging_dir_from_request_body( body ) );
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "stage" ] = nlohmann::json::parse( restore_stage_result_to_json( result ) );
  return response.dump();
}

std::string BackupAdminServer::restore_activate_response( const std::string& body )
{
  auto result = _backup_service->request_restore_activation( staging_dir_from_request_body( body ) );
  nlohmann::json response;
  response[ "ok" ] = true;
  response[ "activation_request" ] =
    nlohmann::json::parse( restore_activation_request_to_json( result ) );
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
    if( is_admin_target( target ) && !request_authorized( req ) )
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
          && is_route_or_child( target, "/admin/backup/status" ) )
      {
        const auto operation_id = child_route_id( target, "/admin/backup/status" );
        const auto has_operation_id = target != "/admin/backup/status";
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
      else if( req.method() == http::verb::post && target == "/admin/backup/create" )
      {
        res.result( http::status::accepted );
        res.body() = create_response();
      }
      else if( req.method() == http::verb::post
               && is_route_or_child( target, "/admin/backup/cancel" ) )
      {
        const auto operation_id = child_route_id( target, "/admin/backup/cancel" );
        const auto has_operation_id = target != "/admin/backup/cancel";
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
      else if( req.method() == http::verb::post && target == "/admin/backup/restore/stage" )
      {
        res.result( http::status::ok );
        res.body() = restore_stage_response( req.body() );
      }
      else if( req.method() == http::verb::post && target == "/admin/backup/restore/activate" )
      {
        res.result( http::status::accepted );
        res.body() = restore_activate_response( req.body() );
      }
      else if( req.method() == http::verb::get && ( target == "/health" || target == "/healthz" ) )
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
    catch( const std::exception& e )
    {
      res.result( http::status::internal_server_error );
      nlohmann::json error;
      error[ "ok" ] = false;
      error[ "error" ] = e.what();
      error[ "status" ] = nlohmann::json::parse( backup_operation_status_to_json( _backup_service->status() ) );
      res.body() = error.dump();
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
