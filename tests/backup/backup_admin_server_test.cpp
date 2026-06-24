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
    cfg.backup.public_restore.enabled = true;
    cfg.backup.public_restore.base_url = "file://" + ( root / "public-repo" ).string();
    cfg.backup.public_restore.network = "testnet";
    cfg.backup.public_restore.require_https = false;
    storage::RocksDBManager manager;
    manager.open( basedir, cfg );
    manager.write_metadata( "layout.chain_storage", "unified" );
    manager.write_metadata( "backup.admin.test", "present" );

    BackupService service( cfg, basedir, config_path, manager );
    AdminPeerSnapshot peer_snapshot;
    peer_snapshot.p2p_running = true;
    peer_snapshot.self_address = "/ip4/127.0.0.1/tcp/8888/p2p/12D3Self";
    peer_snapshot.connected.push_back(
      { "12D3Connected",
        "/ip4/46.225.170.6/tcp/18889/p2p/12D3Connected" } );
    peer_snapshot.connected.push_back( { "malformed-peer", "not-a-multiaddr" } );
    peer_snapshot.known.push_back(
      { "12D3Known",
        "/dns4/example.invalid/tcp/18888/p2p/12D3Known" } );

    BackupAdminServer server(
      &service,
      "127.0.0.1",
      0,
      1,
      "admin-test-token",
      [peer_snapshot]() { return peer_snapshot; } );
    server.start();
    const auto port = server.port();
    assert( port != 0 );

    auto health = http_request( port, http::verb::get, "/health" );
    assert( health.status == http::status::ok );

    auto missing_auth = http_request( port, http::verb::get, "/admin/backup/status" );
    assert( missing_auth.status == http::status::unauthorized );

    auto wrong_auth = http_request( port, http::verb::get, "/admin/backup/status", "wrong-token" );
    assert( wrong_auth.status == http::status::unauthorized );

    auto missing_p2p_auth = http_request( port, http::verb::get, "/admin/p2p/peers" );
    assert( missing_p2p_auth.status == http::status::unauthorized );

    auto live_peers = http_request( port, http::verb::get, "/admin/p2p/peers", "admin-test-token" );
    assert( live_peers.status == http::status::ok );
    auto live_peers_json = nlohmann::json::parse( live_peers.body );
    assert( live_peers_json.at( "ok" ) == true );
    assert( live_peers_json.at( "source" ) == "p2p-live" );
    assert( live_peers_json.at( "p2p_running" ) == true );
    assert( live_peers_json.at( "connected_count" ) == 2 );
    assert( live_peers_json.at( "known_count" ) == 1 );
    assert( live_peers_json.at( "self_address" ) == peer_snapshot.self_address );
    assert( live_peers_json.at( "snapshot_at" ).is_number_unsigned() );
    assert( live_peers_json.at( "connected" ).size() == 2 );
    assert( live_peers_json.at( "connected" ).at( 0 ).at( "peer_id" ) == "12D3Connected" );
    assert( live_peers_json.at( "connected" ).at( 0 ).at( "host" ) == "46.225.170.6" );
    assert( live_peers_json.at( "connected" ).at( 0 ).at( "port" ) == 18889 );
    assert( live_peers_json.at( "connected" ).at( 0 ).at( "connected" ) == true );
    assert( live_peers_json.at( "connected" ).at( 1 ).at( "peer_id" ) == "malformed-peer" );
    assert( live_peers_json.at( "connected" ).at( 1 ).at( "host" ).is_null() );
    assert( live_peers_json.at( "connected" ).at( 1 ).at( "port" ).is_null() );
    assert( live_peers_json.at( "known" ).empty() );

    auto live_peers_with_known = http_request(
      port,
      http::verb::get,
      "/admin/p2p/peers?include_known=true&limit=1",
      "admin-test-token" );
    assert( live_peers_with_known.status == http::status::ok );
    auto live_peers_with_known_json = nlohmann::json::parse( live_peers_with_known.body );
    assert( live_peers_with_known_json.at( "connected" ).size() == 1 );
    assert( live_peers_with_known_json.at( "known" ).size() == 1 );
    assert( live_peers_with_known_json.at( "known" ).at( 0 ).at( "peer_id" ) == "12D3Known" );
    assert( live_peers_with_known_json.at( "known" ).at( 0 ).at( "host" ) == "example.invalid" );
    assert( live_peers_with_known_json.at( "known" ).at( 0 ).at( "port" ) == 18888 );
    assert( live_peers_with_known_json.at( "known" ).at( 0 ).at( "connected" ) == false );

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
    assert( status_json.at( "operation_kind" ) == "local-snapshot" );
    const auto backup_id = status_json.at( "snapshot" ).at( "backup_id" ).get< std::string >();
    assert( !backup_id.empty() );

    const auto public_repo = root / "public-repo";
    std::filesystem::remove_all( public_repo );
    std::filesystem::copy(
      cfg.backup.local.directory,
      public_repo,
      std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing );

    auto config = http_request( port, http::verb::get, "/admin/backup/config", "admin-test-token" );
    assert( config.status == http::status::ok );
    auto config_json = nlohmann::json::parse( config.body );
    assert( config_json.at( "ok" ) == true );
    assert( config_json.at( "config" ).at( "backup" ).at( "local" ).at( "enabled" ) == true );
    assert( config_json.at( "config" ).at( "backup" ).at( "ssh" ).at( "password_file_configured" ) == false );

    auto local_list = http_request(
      port, http::verb::get, "/admin/backup/snapshots/local", "admin-test-token" );
    assert( local_list.status == http::status::ok );
    auto local_list_json = nlohmann::json::parse( local_list.body );
    assert( local_list_json.at( "ok" ) == true );
    assert( local_list_json.at( "source" ) == "local" );
    assert( local_list_json.at( "snapshots" ).at( "snapshot_count" ) == 1 );
    assert( local_list_json.at( "snapshots" ).at( "latest_backup_id" ) == backup_id );

    auto remote_list = http_request(
      port, http::verb::get, "/admin/backup/snapshots/remote", "admin-test-token" );
    assert( remote_list.status == http::status::bad_request );
    auto remote_list_json = nlohmann::json::parse( remote_list.body );
    assert( remote_list_json.at( "ok" ) == false );

    auto upload_latest = http_request(
      port, http::verb::post, "/admin/backup/upload-latest", "admin-test-token" );
    assert( upload_latest.status == http::status::bad_request );

    auto preflight = http_request(
      port,
      http::verb::post,
      "/admin/backup/restore/preflight",
      "admin-test-token",
      std::string( "{\"backup_id\":\"" ) + backup_id + "\"}" );
    assert( preflight.status == http::status::ok );
    auto preflight_json = nlohmann::json::parse( preflight.body );
    assert( preflight_json.at( "ok" ) == true );
    assert( preflight_json.at( "preflight" ).at( "backup_id" ) == backup_id );
    assert( preflight_json.at( "preflight" ).at( "ready_to_restore" ) == true );

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

    auto stage = http_request(
      port,
      http::verb::post,
      "/admin/backup/restore/stage",
      "admin-test-token",
      std::string( "{\"backup_id\":\"" ) + backup_id + "\"}" );
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

    auto invalid_json = http_request(
      port, http::verb::post, "/admin/backup/delete", "admin-test-token", "not-json" );
    assert( invalid_json.status == http::status::bad_request );

    auto delete_dry_run = http_request(
      port,
      http::verb::post,
      "/admin/backup/delete",
      "admin-test-token",
      std::string( "{\"scope\":\"local\",\"backup_id\":\"" ) + backup_id + "\"}" );
    assert( delete_dry_run.status == http::status::accepted );
    auto delete_dry_run_status = wait_for_terminal_backup_status( port, "admin-test-token" );
    assert( delete_dry_run_status.at( "operation_kind" ) == "delete" );
    assert( delete_dry_run_status.at( "delete_result_count" ) == 1 );
    assert( delete_dry_run_status.at( "delete_results" ).at( 0 ).at( "dry_run" ) == true );
    assert( delete_dry_run_status.at( "delete_results" ).at( 0 ).at( "snapshot_found" ) == true );
    assert( std::filesystem::exists( cfg.backup.local.directory + "/snapshots/" + backup_id ) );

    auto delete_confirmed = http_request(
      port,
      http::verb::post,
      "/admin/backup/delete",
      "admin-test-token",
      std::string( "{\"scope\":\"local\",\"backup_id\":\"" ) + backup_id + "\",\"confirm\":\"" + backup_id + "\"}" );
    assert( delete_confirmed.status == http::status::accepted );
    auto delete_confirmed_status = wait_for_terminal_backup_status( port, "admin-test-token" );
    assert( delete_confirmed_status.at( "delete_results" ).at( 0 ).at( "dry_run" ) == false );
    assert( delete_confirmed_status.at( "delete_results" ).at( 0 ).at( "deleted_snapshot" ) == true );
    assert( !std::filesystem::exists( cfg.backup.local.directory + "/snapshots/" + backup_id ) );

    auto public_config = http_request(
      port, http::verb::get, "/admin/backup/public/config", "admin-test-token" );
    assert( public_config.status == http::status::ok );
    auto public_config_json = nlohmann::json::parse( public_config.body );
    assert( public_config_json.at( "ok" ) == true );
    assert( public_config_json.at( "public_restore" ).at( "enabled" ) == true );
    assert( public_config_json.at( "public_restore" ).at( "base_url" ).get< std::string >().find( "file://" ) == 0 );

    auto public_list = http_request(
      port, http::verb::get, "/admin/backup/public/snapshots", "admin-test-token" );
    assert( public_list.status == http::status::ok );
    auto public_list_json = nlohmann::json::parse( public_list.body );
    assert( public_list_json.at( "ok" ) == true );
    assert( public_list_json.at( "source" ) == "public_http" );
    assert( public_list_json.at( "snapshots" ).at( "snapshot_count" ) == 1 );
    assert( public_list_json.at( "snapshots" ).at( "latest_backup_id" ) == backup_id );

    auto public_preflight = http_request(
      port,
      http::verb::post,
      "/admin/backup/public/preflight",
      "admin-test-token",
      std::string( "{\"backup_id\":\"" ) + backup_id + "\"}" );
    assert( public_preflight.status == http::status::ok );
    auto public_preflight_json = nlohmann::json::parse( public_preflight.body );
    assert( public_preflight_json.at( "ok" ) == true );
    assert( public_preflight_json.at( "preflight" ).at( "backup_id" ) == backup_id );

    auto public_fetch = http_request(
      port,
      http::verb::post,
      "/admin/backup/public/fetch",
      "admin-test-token",
      std::string( "{\"backup_id\":\"" ) + backup_id + "\"}" );
    assert( public_fetch.status == http::status::accepted );
    auto public_fetch_status = wait_for_terminal_backup_status( port, "admin-test-token" );
    assert( public_fetch_status.at( "operation_kind" ) == "public-restore-fetch" );
    assert( public_fetch_status.at( "has_public_restore_fetch" ) == true );
    assert( public_fetch_status.at( "public_restore_fetch" ).at( "ready_to_stage" ) == true );
    assert( public_fetch_status.at( "public_restore_fetch" ).at( "backup_id" ) == backup_id );

    const auto public_staging_dir = root / "public-stage";
    auto public_stage = http_request(
      port,
      http::verb::post,
      "/admin/backup/public/restore/stage",
      "admin-test-token",
      std::string( "{\"backup_id\":\"" ) + backup_id + "\",\"staging_dir\":\""
        + public_staging_dir.string() + "\"}" );
    assert( public_stage.status == http::status::ok );
    auto public_stage_json = nlohmann::json::parse( public_stage.body );
    assert( public_stage_json.at( "ok" ) == true );
    assert( std::filesystem::exists( public_staging_dir / "RESTORE_STAGE_COMPLETE" ) );

    auto public_activate = http_request(
      port,
      http::verb::post,
      "/admin/backup/public/restore/activate",
      "admin-test-token",
      std::string( "{\"backup_id\":\"" ) + backup_id + "\",\"staging_dir\":\""
        + public_staging_dir.string() + "\"}" );
    assert( public_activate.status == http::status::accepted );
    auto public_activate_json = nlohmann::json::parse( public_activate.body );
    assert( public_activate_json.at( "ok" ) == true );
    assert( public_activate_json.at( "activation_request" ).at( "requires_node_stop" ) == true );

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

    BackupAdminServer providerless_server( &service, "127.0.0.1", 0, 1, "admin-test-token" );
    providerless_server.start();
    const auto providerless_port = providerless_server.port();
    assert( providerless_port != 0 );
    auto providerless_peers = http_request(
      providerless_port, http::verb::get, "/admin/p2p/peers", "admin-test-token" );
    assert( providerless_peers.status == http::status::ok );
    auto providerless_peers_json = nlohmann::json::parse( providerless_peers.body );
    assert( providerless_peers_json.at( "ok" ) == false );
    assert( providerless_peers_json.at( "source" ) == "p2p-live" );
    assert( providerless_peers_json.at( "p2p_running" ) == false );
    assert( providerless_peers_json.at( "connected" ).empty() );
    assert( providerless_peers_json.at( "known" ).empty() );
    providerless_server.stop();

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
