#include "backup/checkpoint_manager.hpp"
#include "backup/snapshot_repository.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

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

std::string read_file( const std::filesystem::path& path )
{
  std::ifstream input( path, std::ios::binary );
  return std::string( ( std::istreambuf_iterator< char >( input ) ),
                      std::istreambuf_iterator< char >() );
}

NodeConfig snapshot_config( const std::filesystem::path& root )
{
  NodeConfig cfg;
  cfg.rocksdb_compression = "none";
  cfg.rocksdb_blocks_compression = "none";
  cfg.backup.enabled = true;
  cfg.backup.node_id = "snapshot-test-node";
  cfg.backup.workspace = ( root / "work" ).string();
  cfg.backup.local.enabled = true;
  cfg.backup.local.directory = ( root / "repo" ).string();
  return cfg;
}

LocalSnapshotResult create_snapshot( const std::filesystem::path& root,
                                     storage::RocksDBManager& manager,
                                     const NodeConfig& cfg,
                                     const std::filesystem::path& basedir,
                                     const std::filesystem::path& config_path,
                                     const std::string& checkpoint_name )
{
  CheckpointManager checkpoint_manager( basedir, manager );
  auto checkpoint = checkpoint_manager.create_checkpoint( root / checkpoint_name );

  LocalSnapshotRepository repository( cfg.backup.local.directory );
  auto result = repository.store_checkpoint_snapshot( checkpoint, cfg, basedir, config_path );
  std::filesystem::remove_all( checkpoint.checkpoint_dir );
  return result;
}

} // namespace

int main()
{
  {
    auto root = unique_temp_dir( "teleno-backup-snapshot" );
    auto basedir = root / "basedir";
    auto config_path = basedir / "config.yml";
    write_file( config_path, "backup:\n  enabled: true\n" );
    write_file( basedir / "chain" / "genesis_data.json", "{\"genesis\":true}\n" );
    write_file( basedir / "jsonrpc" / "descriptors" / "koinos_descriptors.pb", "descriptor-bytes" );

    auto cfg = snapshot_config( root );

    storage::RocksDBManager manager;
    manager.open( basedir, cfg );
    manager.write_metadata( "backup.snapshot.test", "present" );

    auto first = create_snapshot( root, manager, cfg, basedir, config_path, "checkpoint-1" );
    assert( !first.backup_id.empty() );
    assert( std::filesystem::exists( first.snapshot_dir / "COMPLETE" ) );
    assert( std::filesystem::exists( first.manifest_path ) );
    assert( std::filesystem::exists( first.files_path ) );
    assert( std::filesystem::exists( first.latest_path ) );
    assert( first.file_count >= 4 );
    assert( first.object_count >= 4 );
    assert( first.new_object_count > 0 );
    assert( first.new_object_count <= first.object_count );
    assert( first.restore_space.restored_database_bytes > 0 );
    assert( first.restore_space.runtime_files_bytes > 0 );
    assert( first.restore_space.minimum_target_free_bytes >= first.restore_space.restored_database_bytes );
    assert( read_file( first.manifest_path ).find( "\"format\": \"teleno-native-rocksdb-snapshot\"" ) != std::string::npos );
    assert( read_file( first.files_path ).find( "\"path\": \"config.yml\"" ) != std::string::npos );
    assert( read_file( first.latest_path ).find( first.backup_id ) != std::string::npos );

    auto second = create_snapshot( root, manager, cfg, basedir, config_path, "checkpoint-2" );
    assert( std::filesystem::exists( second.snapshot_dir / "COMPLETE" ) );
    assert( second.reused_object_count > 0 );
    assert( read_file( second.latest_path ).find( second.backup_id ) != std::string::npos );

    auto restore_target = root / "restore-target";
    storage::RocksDBManager existing_target;
    existing_target.open( restore_target, cfg );
    existing_target.write_metadata( "backup.snapshot.old", "preserved" );
    existing_target.close();
    write_file( restore_target / "chain" / "genesis_data.json", "{\"old_genesis\":true}\n" );
    write_file( restore_target / "jsonrpc" / "descriptors" / "koinos_descriptors.pb", "old-descriptor" );
    write_file( restore_target / "config.yml", "active: operator-config\n" );

    auto preflight = build_local_restore_preflight( cfg.backup.local.directory, restore_target );
    assert( preflight.backup_id == second.backup_id );
    assert( preflight.snapshot_complete );
    assert( preflight.file_count == second.file_count );
    assert( preflight.missing_object_count == 0 );
    assert( preflight.restore_space.existing_target_bytes > 0 );
    assert( preflight.space_check.passes_minimum );
    assert( preflight.ready_to_restore );
    assert( restore_preflight_result_to_text( preflight ).find( "Backup restore preflight" ) != std::string::npos );
    assert( restore_preflight_result_to_json( preflight ).find( "\"ready_to_restore\": true" ) != std::string::npos );

    auto stage = stage_local_restore_snapshot( cfg.backup.local.directory, restore_target );
    assert( stage.preflight.backup_id == second.backup_id );
    assert( std::filesystem::exists( stage.staging_dir / "RESTORE_STAGE_COMPLETE" ) );
    assert( std::filesystem::exists( stage.metadata_path ) );
    assert( std::filesystem::exists( stage.staging_dir / "config.yml" ) );
    assert( stage.restored_file_count == second.file_count );
    assert( stage.restored_bytes == second.total_bytes );
    assert( restore_stage_result_to_text( stage ).find( "Staged backup restore" ) != std::string::npos );
    assert( restore_stage_result_to_json( stage ).find( "\"restored_file_count\"" ) != std::string::npos );

    storage::RocksDBManager restored;
    restored.open( stage.staging_dir, cfg );
    assert( restored.read_metadata( "backup.snapshot.test" ) == "present" );
    restored.close();

    auto activation = activate_staged_restore_snapshot( stage.staging_dir, restore_target );
    assert( activation.backup_id == second.backup_id );
    assert( std::filesystem::exists( restore_target / "db" / "CURRENT" ) );
    assert( std::filesystem::exists( activation.pre_restore_dir / "db" / "CURRENT" ) );
    assert( !std::filesystem::exists( stage.staging_dir / "db" ) );
    assert( std::filesystem::exists( restore_target / ".backup-just-restored" ) );
    assert( std::filesystem::exists( restore_target / ".teleno-restore-manifest.json" ) );
    assert( read_file( restore_target / "config.yml" ) == "active: operator-config\n" );
    assert( std::filesystem::exists( restore_target / ".teleno-restored-config.yml" ) );
    assert( read_file( restore_target / "chain" / "genesis_data.json" ) == "{\"genesis\":true}\n" );
    assert( read_file( activation.pre_restore_dir / "chain" / "genesis_data.json" ) == "{\"old_genesis\":true}\n" );
    assert( restore_activation_result_to_text( activation ).find( "Activated staged backup restore" ) != std::string::npos );
    assert( restore_activation_result_to_json( activation ).find( "\"block_producer_disabled_on_first_start\": true" ) != std::string::npos );

    storage::RocksDBManager activated_db;
    activated_db.open( restore_target, cfg );
    assert( activated_db.read_metadata( "backup.snapshot.test" ) == "present" );
    activated_db.close();

    storage::RocksDBManager preserved_db;
    preserved_db.open( activation.pre_restore_dir, cfg );
    assert( preserved_db.read_metadata( "backup.snapshot.old" ) == "preserved" );
    preserved_db.close();

    bool removed_object = false;
    for( const auto& entry: std::filesystem::recursive_directory_iterator( root / "repo" / "objects" ) )
    {
      if( entry.is_regular_file() )
      {
        std::filesystem::remove( entry.path() );
        removed_object = true;
        break;
      }
    }
    assert( removed_object );
    auto missing_preflight = build_local_restore_preflight( cfg.backup.local.directory, restore_target );
    assert( missing_preflight.missing_object_count > 0 );
    assert( !missing_preflight.ready_to_restore );

    manager.close();
    std::filesystem::remove_all( root );
  }

  {
    const uint64_t gib = 1024ULL * 1024ULL * 1024ULL;
    auto estimate = estimate_restore_space( 80 * gib, 10 * 1024, 75 * gib );
    assert( estimate.minimum_target_free_bytes > 80 * gib );
    assert( estimate.recommended_target_free_bytes > estimate.minimum_target_free_bytes );

    auto fail_check = check_restore_space( estimate, estimate.minimum_target_free_bytes - 1, "/Volumes/Internal" );
    assert( !fail_check.passes_minimum );
    assert( fail_check.message.find( "requires at least" ) != std::string::npos );

    auto warn_check = check_restore_space( estimate, estimate.minimum_target_free_bytes, "/Volumes/Internal" );
    assert( warn_check.passes_minimum );
    assert( warn_check.below_recommended );

    auto pass_check = check_restore_space( estimate, estimate.recommended_target_free_bytes, "/Volumes/External" );
    assert( pass_check.passes_minimum );
    assert( !pass_check.below_recommended );
  }

  {
    const uint64_t gib = 1024ULL * 1024ULL * 1024ULL;
    auto archive_extract = estimate_restore_space( 2 * gib, 1024, gib, gib, false );
    auto streaming_extract = estimate_restore_space( 2 * gib, 1024, gib, gib, true );
    assert( archive_extract.minimum_target_free_bytes > streaming_extract.minimum_target_free_bytes );
  }

  return 0;
}
