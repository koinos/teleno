#include "backup/backup_admin_server.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>

using namespace koinos::node;
using namespace koinos::node::backup;

namespace {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

struct HttpResult
{
  http::status status;
  std::string body;
};

std::filesystem::path unique_temp_dir( const std::string& prefix )
{
  auto path = std::filesystem::temp_directory_path()
              / ( prefix + "-" + std::to_string( std::rand() ) );
  std::filesystem::remove_all( path );
  std::filesystem::create_directories( path );
  return path;
}

void write_file( const std::filesystem::path& path, const std::string& content )
{
  std::filesystem::create_directories( path.parent_path() );
  std::ofstream out( path, std::ios::binary | std::ios::trunc );
  out << content;
}

NodeConfig admin_config( const std::filesystem::path& root )
{
  NodeConfig cfg;
  cfg.rocksdb_compression = "none";
  cfg.rocksdb_blocks_compression = "none";
  cfg.backup.enabled = true;
  cfg.backup.node_id = "backup-admin-test-node";
  cfg.backup.workspace = ( root / "work" ).string();
  cfg.backup.local.enabled = true;
  cfg.backup.local.directory = ( root / "repo" ).string();
  return cfg;
}

HttpResult http_request( uint16_t port,
                         http::verb method,
                         const std::string& target,
                         const std::string& bearer_token = {},
                         const std::string& body = {} )
{
  net::io_context ioc;
  tcp::resolver resolver( ioc );
  beast::tcp_stream stream( ioc );
  auto const results = resolver.resolve( "127.0.0.1", std::to_string( port ) );
  stream.connect( results );

  http::request< http::string_body > req{ method, target, 11 };
  req.set( http::field::host, "127.0.0.1" );
  req.set( http::field::content_type, "application/json" );
  if( !bearer_token.empty() )
    req.set( http::field::authorization, "Bearer " + bearer_token );
  req.body() = body;
  req.prepare_payload();
  http::write( stream, req );

  beast::flat_buffer buffer;
  http::response< http::string_body > res;
  http::read( stream, buffer, res );

  beast::error_code ec;
  stream.socket().shutdown( tcp::socket::shutdown_both, ec );
  return { res.result(), res.body() };
}

nlohmann::json wait_for_terminal_backup_status( uint16_t port,
                                                const std::string& bearer_token )
{
  nlohmann::json last_status;
  for( int i = 0; i < 100; ++i )
  {
    auto status = http_request( port, http::verb::get, "/admin/backup/status", bearer_token );
    assert( status.status == http::status::ok );
    last_status = nlohmann::json::parse( status.body );
    const auto state = last_status.at( "state" ).get< std::string >();
    if( state == "succeeded" || state == "failed" )
      return last_status;
    std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
  }
  return last_status;
}

} // namespace

int main()
{
  {
    auto root = unique_temp_dir( "teleno-backup-admin" );
    auto basedir = root / "basedir";
    auto config_path = basedir / "config.yml";
    write_file( config_path, "backup:\n  enabled: true\n" );
    write_file( basedir / "chain" / "genesis_data.json", "{\"genesis\":true}\n" );
    write_file( basedir / "jsonrpc" / "descriptors" / "koinos_descriptors.pb", "descriptor-bytes" );

    auto cfg = admin_config( root );
    storage::RocksDBManager manager;
    manager.open( basedir, cfg );
    manager.write_metadata( "layout.chain_storage", "unified" );
    manager.write_metadata( "backup.admin.test", "present" );

    BackupService service( cfg, basedir, config_path, manager );
    BackupAdminServer server( &service, "127.0.0.1", 0, 1, "admin-test-token" );
    server.start();
    const auto port = server.port();
    assert( port != 0 );

    auto health = http_request( port, http::verb::get, "/health" );
    assert( health.status == http::status::ok );

    auto missing_auth = http_request( port, http::verb::get, "/admin/backup/status" );
    assert( missing_auth.status == http::status::unauthorized );

    auto wrong_auth = http_request( port, http::verb::get, "/admin/backup/status", "wrong-token" );
    assert( wrong_auth.status == http::status::unauthorized );

    auto initial = http_request( port, http::verb::get, "/admin/backup/status", "admin-test-token" );
    assert( initial.status == http::status::ok );
    auto initial_json = nlohmann::json::parse( initial.body );
    assert( initial_json.at( "state" ) == "idle" );

    auto created = http_request( port, http::verb::post, "/admin/backup/create", "admin-test-token" );
    assert( created.status == http::status::accepted );
    auto created_json = nlohmann::json::parse( created.body );
    assert( created_json.at( "ok" ) == true );
    assert( created_json.at( "status" ).at( "state" ) == "running" );
    const auto operation_id = created_json.at( "status" ).at( "operation_id" ).get< std::string >();
    assert( !operation_id.empty() );

    auto status_json = wait_for_terminal_backup_status( port, "admin-test-token" );
    assert( status_json.at( "state" ) == "succeeded" );
    assert( status_json.at( "has_snapshot" ) == true );

    auto status_by_id = http_request(
      port, http::verb::get, "/admin/backup/status/" + operation_id, "admin-test-token" );
    assert( status_by_id.status == http::status::ok );
    auto status_by_id_json = nlohmann::json::parse( status_by_id.body );
    assert( status_by_id_json.at( "operation_id" ) == operation_id );
    assert( status_by_id_json.at( "state" ) == "succeeded" );

    auto missing_status = http_request(
      port, http::verb::get, "/admin/backup/status/missing-operation", "admin-test-token" );
    assert( missing_status.status == http::status::not_found );
    auto missing_status_json = nlohmann::json::parse( missing_status.body );
    assert( missing_status_json.at( "ok" ) == false );
    assert( missing_status_json.at( "operation_id" ) == "missing-operation" );
    assert( missing_status_json.at( "status" ).at( "operation_id" ) == operation_id );

    auto empty_status_id = http_request(
      port, http::verb::get, "/admin/backup/status/", "admin-test-token" );
    assert( empty_status_id.status == http::status::bad_request );

    auto cancel = http_request( port, http::verb::post, "/admin/backup/cancel", "admin-test-token" );
    assert( cancel.status == http::status::accepted );
    auto cancel_json = nlohmann::json::parse( cancel.body );
    assert( cancel_json.at( "status" ).at( "state" ) == "succeeded" );

    auto cancel_by_id = http_request(
      port, http::verb::post, "/admin/backup/cancel/" + operation_id, "admin-test-token" );
    assert( cancel_by_id.status == http::status::accepted );
    auto cancel_by_id_json = nlohmann::json::parse( cancel_by_id.body );
    assert( cancel_by_id_json.at( "status" ).at( "operation_id" ) == operation_id );

    auto missing_cancel = http_request(
      port, http::verb::post, "/admin/backup/cancel/missing-operation", "admin-test-token" );
    assert( missing_cancel.status == http::status::not_found );
    auto missing_cancel_json = nlohmann::json::parse( missing_cancel.body );
    assert( missing_cancel_json.at( "ok" ) == false );
    assert( missing_cancel_json.at( "operation_id" ) == "missing-operation" );

    auto empty_cancel_id = http_request(
      port, http::verb::post, "/admin/backup/cancel/", "admin-test-token" );
    assert( empty_cancel_id.status == http::status::bad_request );

    auto stage = http_request( port, http::verb::post, "/admin/backup/restore/stage", "admin-test-token" );
    assert( stage.status == http::status::ok );
    auto stage_json = nlohmann::json::parse( stage.body );
    assert( stage_json.at( "ok" ) == true );
    const auto staging_dir = std::filesystem::path(
      stage_json.at( "stage" ).at( "staging_dir" ).get< std::string >() );
    assert( std::filesystem::exists( staging_dir / "RESTORE_STAGE_COMPLETE" ) );

    auto activate = http_request( port, http::verb::post, "/admin/backup/restore/activate", "admin-test-token" );
    assert( activate.status == http::status::accepted );
    auto activate_json = nlohmann::json::parse( activate.body );
    assert( activate_json.at( "ok" ) == true );
    auto activation_request = activate_json.at( "activation_request" );
    assert( activation_request.at( "requires_node_stop" ) == true );
    assert( activation_request.at( "ready_to_activate" ) == true );
    assert( std::filesystem::exists(
      std::filesystem::path( activation_request.at( "intent_path" ).get< std::string >() ) ) );
    assert( std::filesystem::exists( basedir / "db" ) );

    server.stop();
    manager.close();
    std::filesystem::remove_all( root );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-admin-reject" );
    auto cfg = admin_config( root );
    storage::RocksDBManager manager;
    auto basedir = root / "basedir";
    write_file( basedir / "config.yml", "backup:\n  enabled: true\n" );
    manager.open( basedir, cfg );
    BackupService service( cfg, basedir, basedir / "config.yml", manager );

    bool threw = false;
    try
    {
      BackupAdminServer server( &service, "0.0.0.0", 0, 1 );
    }
    catch( const std::runtime_error& )
    {
      threw = true;
    }
    assert( threw );

    manager.close();
    std::filesystem::remove_all( root );
  }

  return 0;
}
