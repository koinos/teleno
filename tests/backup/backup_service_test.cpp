#include "backup/backup_service.hpp"
#include "backup/backup_scheduler.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>

using namespace koinos::node;
using namespace koinos::node::backup;

namespace {

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

NodeConfig service_config( const std::filesystem::path& root )
{
  NodeConfig cfg;
  cfg.rocksdb_compression = "none";
  cfg.rocksdb_blocks_compression = "none";
  cfg.backup.enabled = true;
  cfg.backup.node_id = "backup-service-test-node";
  cfg.backup.workspace = ( root / "work" ).string();
  cfg.backup.local.enabled = true;
  cfg.backup.local.directory = ( root / "repo" ).string();
  return cfg;
}

BackupOperationStatus wait_for_terminal_status( BackupService& service )
{
  BackupOperationStatus status;
  for( int i = 0; i < 100; ++i )
  {
    status = service.status();
    if( status.state == BackupOperationState::succeeded
        || status.state == BackupOperationState::failed )
      return status;
    std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
  }
  return status;
}

} // namespace

int main()
{
  {
    auto root = unique_temp_dir( "teleno-backup-service" );
    auto basedir = root / "basedir";
    auto config_path = basedir / "config.yml";
    write_file( config_path, "backup:\n  enabled: true\n" );
    write_file( basedir / "chain" / "genesis_data.json", "{\"genesis\":true}\n" );
    write_file( basedir / "jsonrpc" / "descriptors" / "koinos_descriptors.pb", "descriptor-bytes" );

    auto cfg = service_config( root );
    storage::RocksDBManager manager;
    manager.open( basedir, cfg );
    manager.write_metadata( "layout.chain_storage", "unified" );
    manager.write_metadata( "backup.service.test", "present" );

    BackupService service( cfg, basedir, config_path, manager );
    auto initial_status = service.status();
    assert( initial_status.state == BackupOperationState::idle );
    assert( backup_operation_state_name( initial_status.state ) == std::string( "idle" ) );

    auto snapshot = service.create_local_snapshot();
    assert( !snapshot.backup_id.empty() );
    assert( std::filesystem::exists( snapshot.snapshot_dir / "COMPLETE" ) );
    assert( snapshot.file_count >= 4 );
    assert( snapshot.object_count >= 4 );
    assert( !std::filesystem::exists( root / "work" / ".teleno-checkpoints" / service.status().operation_id ) );

    auto status = service.status();
    assert( status.state == BackupOperationState::succeeded );
    assert( status.has_snapshot );
    assert( status.snapshot.backup_id == snapshot.backup_id );
    assert( backup_operation_status_to_text( status ).find( "Backup operation status" ) != std::string::npos );
    assert( backup_operation_status_to_json( status ).find( "\"state\": \"succeeded\"" ) != std::string::npos );

    auto restore_target = root / "restore-target";
    auto preflight = build_local_restore_preflight( cfg.backup.local.directory, restore_target );
    assert( preflight.ready_to_restore );
    auto stage = stage_local_restore_snapshot( cfg.backup.local.directory, restore_target );

    storage::RocksDBManager restored;
    restored.open( stage.staging_dir, cfg );
    assert( restored.read_metadata( "backup.service.test" ) == "present" );
    restored.close();

    manager.close();
    std::filesystem::remove_all( root );
  }

  {
    assert( parse_backup_schedule_interval( "25ms" ) == std::chrono::milliseconds( 25 ) );
    assert( parse_backup_schedule_interval( "2s" ) == std::chrono::seconds( 2 ) );
    assert( parse_backup_schedule_interval( "3m" ) == std::chrono::minutes( 3 ) );
    assert( parse_backup_schedule_interval( "4h" ) == std::chrono::hours( 4 ) );
    assert( parse_backup_schedule_interval( "1d" ) == std::chrono::hours( 24 ) );

    bool threw = false;
    try
    {
      (void)parse_backup_schedule_interval( "0s" );
    }
    catch( const std::runtime_error& )
    {
      threw = true;
    }
    assert( threw );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-scheduler" );
    auto basedir = root / "basedir";
    auto config_path = basedir / "config.yml";
    write_file( config_path, "backup:\n  enabled: true\n" );
    write_file( basedir / "chain" / "genesis_data.json", "{\"genesis\":true}\n" );
    write_file( basedir / "jsonrpc" / "descriptors" / "koinos_descriptors.pb", "descriptor-bytes" );

    auto cfg = service_config( root );
    cfg.backup.schedule.enabled = true;
    cfg.backup.schedule.interval = "50ms";
    cfg.backup.schedule.run_on_startup_if_missed = true;
    cfg.backup.schedule.jitter_seconds = 0;
    cfg.backup.schedule.minimum_head_progress = 1;
    cfg.backup.schedule.skip_if_syncing_from_genesis = true;

    storage::RocksDBManager manager;
    manager.open( basedir, cfg );
    manager.write_metadata( "layout.chain_storage", "unified" );
    manager.write_metadata( "backup.scheduler.test", "present" );

    std::atomic< uint64_t > head_height{ 2 };
    BackupService service( cfg, basedir, config_path, manager );
    BackupScheduler scheduler( &service, cfg, [&]() { return head_height.load(); } );
    scheduler.start();

    auto status = wait_for_terminal_status( service );
    scheduler.stop();
    service.wait_for_current_operation();
    assert( status.state == BackupOperationState::succeeded );
    assert( status.has_snapshot );
    assert( std::filesystem::exists( status.snapshot.snapshot_dir / "COMPLETE" ) );

    manager.close();
    std::filesystem::remove_all( root );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-scheduler-skip" );
    auto basedir = root / "basedir";
    auto config_path = basedir / "config.yml";
    write_file( config_path, "backup:\n  enabled: true\n" );
    write_file( basedir / "chain" / "genesis_data.json", "{\"genesis\":true}\n" );
    write_file( basedir / "jsonrpc" / "descriptors" / "koinos_descriptors.pb", "descriptor-bytes" );

    auto cfg = service_config( root );
    cfg.backup.schedule.enabled = true;
    cfg.backup.schedule.interval = "30ms";
    cfg.backup.schedule.run_on_startup_if_missed = true;
    cfg.backup.schedule.jitter_seconds = 0;
    cfg.backup.schedule.minimum_head_progress = 1;
    cfg.backup.schedule.skip_if_syncing_from_genesis = true;

    storage::RocksDBManager manager;
    manager.open( basedir, cfg );
    manager.write_metadata( "layout.chain_storage", "unified" );
    manager.write_metadata( "backup.scheduler.skip.test", "present" );

    std::atomic< uint64_t > head_height{ 0 };
    BackupService service( cfg, basedir, config_path, manager );
    BackupScheduler scheduler( &service, cfg, [&]() { return head_height.load(); } );
    scheduler.start();
    std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    assert( service.status().state == BackupOperationState::idle );

    head_height = 2;
    auto status = wait_for_terminal_status( service );
    scheduler.stop();
    service.wait_for_current_operation();
    assert( status.state == BackupOperationState::succeeded );

    manager.close();
    std::filesystem::remove_all( root );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-service-async" );
    auto basedir = root / "basedir";
    auto config_path = basedir / "config.yml";
    write_file( config_path, "backup:\n  enabled: true\n" );
    write_file( basedir / "chain" / "genesis_data.json", "{\"genesis\":true}\n" );
    write_file( basedir / "jsonrpc" / "descriptors" / "koinos_descriptors.pb", "descriptor-bytes" );

    auto cfg = service_config( root );
    storage::RocksDBManager manager;
    manager.open( basedir, cfg );
    manager.write_metadata( "layout.chain_storage", "unified" );
    manager.write_metadata( "backup.service.async.test", "present" );

    BackupService service( cfg, basedir, config_path, manager );
    auto started = service.start_local_snapshot_async();
    assert( started.state == BackupOperationState::running );
    assert( !started.operation_id.empty() );

    auto status = wait_for_terminal_status( service );
    service.wait_for_current_operation();
    assert( status.state == BackupOperationState::succeeded );
    assert( status.has_snapshot );
    assert( std::filesystem::exists( status.snapshot.snapshot_dir / "COMPLETE" ) );
    assert( !std::filesystem::exists( root / "work" / ".teleno-checkpoints" / status.operation_id ) );
    assert( backup_operation_status_to_json( status ).find( "\"cancel_requested\": false" ) != std::string::npos );

    manager.close();
    std::filesystem::remove_all( root );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-service-reject" );
    auto basedir = root / "basedir";
    auto config_path = basedir / "config.yml";
    write_file( config_path, "backup:\n  enabled: true\n" );
    write_file( basedir / "chain" / "genesis_data.json", "{\"genesis\":true}\n" );
    write_file( basedir / "jsonrpc" / "descriptors" / "koinos_descriptors.pb", "descriptor-bytes" );

    auto cfg = service_config( root );
    cfg.backup.local.enabled = false;

    storage::RocksDBManager manager;
    manager.open( basedir, cfg );
    manager.write_metadata( "layout.chain_storage", "unified" );

    BackupService service( cfg, basedir, config_path, manager );
    bool threw = false;
    try
    {
      (void)service.create_local_snapshot();
    }
    catch( const std::runtime_error& )
    {
      threw = true;
    }
    assert( threw );
    assert( service.status().state == BackupOperationState::idle );

    manager.close();
    std::filesystem::remove_all( root );
  }

  return 0;
}
