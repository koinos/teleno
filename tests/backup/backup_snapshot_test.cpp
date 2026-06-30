#include "backup/checkpoint_manager.hpp"
#include "backup/restore_activation_supervisor.hpp"
#include "backup/snapshot_repository.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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

std::string snapshot_file_sha256( const std::filesystem::path& files_path, const std::string& relative_path )
{
  const auto files = nlohmann::json::parse( read_file( files_path ) );
  for( const auto& file: files.at( "files" ) )
  {
    if( file.at( "path" ).get< std::string >() == relative_path )
      return file.at( "sha256" ).get< std::string >();
  }
  assert( false && "snapshot file not found" );
  return {};
}

std::filesystem::path snapshot_object_path( const std::filesystem::path& repository_dir, const std::string& sha256 )
{
  return repository_dir / "objects" / "sha256" / sha256.substr( 0, 2 ) / sha256.substr( 2, 2 ) / sha256;
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
    const auto first_config_sha = snapshot_file_sha256( first.files_path, "config.yml" );
    const auto first_config_object = snapshot_object_path( cfg.backup.local.directory, first_config_sha );
    assert( std::filesystem::exists( first_config_object ) );
    assert( read_file( first_config_object ) == "backup:\n  enabled: true\n" );
    assert( !std::filesystem::equivalent( first_config_object, config_path ) );

    write_file( config_path, "backup:\n  enabled: true\n  changed: true\n" );
    assert( read_file( first_config_object ) == "backup:\n  enabled: true\n" );

    auto second = create_snapshot( root, manager, cfg, basedir, config_path, "checkpoint-2" );
    assert( std::filesystem::exists( second.snapshot_dir / "COMPLETE" ) );
    assert( second.reused_object_count > 0 );
    assert( read_file( second.latest_path ).find( second.backup_id ) != std::string::npos );

    auto list = list_local_backup_snapshots( cfg.backup.local.directory );
    assert( list.latest_backup_id == second.backup_id );
    assert( list.snapshots.size() == 2 );
    assert( list.snapshots[ 0 ].backup_id == first.backup_id );
    assert( !list.snapshots[ 0 ].latest );
    assert( list.snapshots[ 1 ].backup_id == second.backup_id );
    assert( list.snapshots[ 1 ].latest );
    assert( backup_snapshot_list_result_to_text( list ).find( "Native backup snapshots" ) != std::string::npos );
    assert( backup_snapshot_list_result_to_json( list ).find( "\"snapshot_count\": 2" ) != std::string::npos );

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

    auto selected_preflight = build_local_restore_preflight( cfg.backup.local.directory, restore_target, first.backup_id );
    assert( selected_preflight.backup_id == first.backup_id );
    assert( selected_preflight.ready_to_restore );

    write_file( first_config_object, "corrupt old config object\n" );
    auto selected_stage = stage_local_restore_snapshot(
      cfg.backup.local.directory,
      restore_target,
      first.backup_id,
      root / "selected-first-stage" );
    assert( selected_stage.preflight.backup_id == first.backup_id );
    assert( selected_stage.skipped_optional_runtime_files.size() == 1 );
    assert( selected_stage.skipped_optional_runtime_files[ 0 ] == "config.yml" );
    assert( !std::filesystem::exists( selected_stage.staging_dir / "config.yml" ) );
    assert( restore_stage_result_to_json( selected_stage ).find( "\"skipped_optional_runtime_files\": [\"config.yml\"]" ) != std::string::npos );

    std::vector< RestoreStageProgress > stage_progress;
    auto stage = stage_local_restore_snapshot(
      cfg.backup.local.directory,
      restore_target,
      std::filesystem::path{},
      [&]( const RestoreStageProgress& progress ) {
        stage_progress.push_back( progress );
      } );
    assert( stage.preflight.backup_id == second.backup_id );
    assert( std::filesystem::exists( stage.staging_dir / "RESTORE_STAGE_COMPLETE" ) );
    assert( std::filesystem::exists( stage.metadata_path ) );
    assert( std::filesystem::exists( stage.staging_dir / "config.yml" ) );
    assert( stage.restored_file_count == second.file_count );
    assert( stage.restored_bytes == second.total_bytes );
    assert( !stage_progress.empty() );
    assert( stage_progress.front().backup_id == second.backup_id );
    assert( stage_progress.front().completed_files == 0 );
    assert( stage_progress.front().total_files == second.file_count );
    assert( stage_progress.front().total_bytes == second.total_bytes );
    assert( stage_progress.back().completed_files == second.file_count );
    assert( stage_progress.back().completed_bytes == second.total_bytes );
    uint64_t last_progress_files = 0;
    uint64_t last_progress_bytes = 0;
    for( const auto& progress: stage_progress )
    {
      assert( progress.backup_id == second.backup_id );
      assert( progress.total_files == second.file_count );
      assert( progress.total_bytes == second.total_bytes );
      assert( progress.completed_files >= last_progress_files );
      assert( progress.completed_bytes >= last_progress_bytes );
      last_progress_files = progress.completed_files;
      last_progress_bytes = progress.completed_bytes;
    }
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
    auto root = unique_temp_dir( "teleno-backup-retention" );
    auto basedir = root / "basedir";
    auto config_path = basedir / "config.yml";
    write_file( config_path, "backup:\n  enabled: true\n" );
    write_file( basedir / "chain" / "genesis_data.json", "{\"genesis\":true}\n" );
    write_file( basedir / "jsonrpc" / "descriptors" / "koinos_descriptors.pb", "descriptor-bytes" );

    auto cfg = snapshot_config( root );
    cfg.backup.local.retention_count = 1;

    storage::RocksDBManager manager;
    manager.open( basedir, cfg );
    manager.write_metadata( "backup.retention.test", "first" );
    auto first = create_snapshot( root, manager, cfg, basedir, config_path, "checkpoint-retention-1" );

    manager.write_metadata( "backup.retention.test", "second" );
    auto second = create_snapshot( root, manager, cfg, basedir, config_path, "checkpoint-retention-2" );

    assert( first.backup_id != second.backup_id );
    assert( !std::filesystem::exists( first.snapshot_dir ) );
    assert( std::filesystem::exists( second.snapshot_dir / "COMPLETE" ) );
    assert( read_file( second.latest_path ).find( second.backup_id ) != std::string::npos );

    uint64_t completed_snapshots = 0;
    for( const auto& entry: std::filesystem::directory_iterator( root / "repo" / "snapshots" ) )
    {
      if( entry.is_directory() && std::filesystem::exists( entry.path() / "COMPLETE" ) )
        ++completed_snapshots;
    }
    assert( completed_snapshots == 1 );

    manager.close();
    std::filesystem::remove_all( root );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-delete" );
    auto repo = root / "repo";
    const std::string old_id = "20260615T100000Z-ms-1-files-2";
    const std::string new_id = "20260615T110000Z-ms-2-files-2";
    const std::string old_only_hash( 64, 'a' );
    const std::string new_only_hash( 64, 'b' );
    const std::string shared_hash( 64, 'c' );

    const auto manifest = []( const std::string& backup_id, uint64_t total_bytes ) {
      return "{\n"
             "  \"format\": \"teleno-native-rocksdb-snapshot\",\n"
             "  \"backup_id\": \"" + backup_id + "\",\n"
             "  \"created_at\": \"20260615T100000Z\",\n"
             "  \"node\": { \"version\": \"test\" },\n"
             "  \"source\": { \"node_id\": \"delete-test\", \"storage_layout\": \"unified\" },\n"
             "  \"snapshot\": { \"file_count\": 2, \"object_count\": 2, \"total_bytes\": " + std::to_string( total_bytes ) + " },\n"
             "  \"sizes\": { \"restored_database_bytes\": " + std::to_string( total_bytes ) + ", \"runtime_files_bytes\": 0, \"object_download_bytes\": " + std::to_string( total_bytes ) + ", \"archive_bytes\": 0 },\n"
             "  \"restore\": { \"start_as_observer_first\": true }\n"
             "}\n";
    };
    const auto files = []( const std::string& backup_id,
                           const std::string& first_hash,
                           uint64_t first_size,
                           const std::string& second_hash,
                           uint64_t second_size ) {
      return "{\n"
             "  \"format\": \"teleno-native-snapshot-files\",\n"
             "  \"backup_id\": \"" + backup_id + "\",\n"
             "  \"files\": [\n"
             "    { \"path\": \"db/one.sst\", \"sha256\": \"" + first_hash + "\", \"size_bytes\": " + std::to_string( first_size ) + " },\n"
             "    { \"path\": \"db/two.sst\", \"sha256\": \"" + second_hash + "\", \"size_bytes\": " + std::to_string( second_size ) + " }\n"
             "  ]\n"
             "}\n";
    };

    write_file( repo / "snapshots" / old_id / "manifest.json", manifest( old_id, 15 ) );
    write_file( repo / "snapshots" / old_id / "files.json", files( old_id, old_only_hash, 10, shared_hash, 5 ) );
    write_file( repo / "snapshots" / old_id / "COMPLETE", "complete\n" );
    write_file( repo / "snapshots" / new_id / "manifest.json", manifest( new_id, 25 ) );
    write_file( repo / "snapshots" / new_id / "files.json", files( new_id, new_only_hash, 20, shared_hash, 5 ) );
    write_file( repo / "snapshots" / new_id / "COMPLETE", "complete\n" );
    write_file( repo / "latest.json",
                "{ \"backup_id\": \"" + new_id + "\", \"snapshot_dir\": \"" + new_id + "\" }\n" );
    write_file( repo / "objects" / "sha256" / "aa" / "aa" / old_only_hash, "old-object" );
    write_file( repo / "objects" / "sha256" / "bb" / "bb" / new_only_hash, "new-object-payload" );
    write_file( repo / "objects" / "sha256" / "cc" / "cc" / shared_hash, "shared" );

    auto dry_run = delete_local_backup_snapshot( repo, old_id, true );
    assert( dry_run.dry_run );
    assert( dry_run.snapshot_found );
    assert( !dry_run.deleted_snapshot );
    assert( !dry_run.deleted_latest );
    assert( dry_run.previous_latest_backup_id == new_id );
    assert( dry_run.new_latest_backup_id == new_id );
    assert( dry_run.reclaimable_object_count == 1 );
    assert( dry_run.reclaimable_object_bytes == 10 );
    assert( std::filesystem::exists( repo / "snapshots" / old_id / "COMPLETE" ) );
    assert( backup_delete_result_to_text( dry_run ).find( "dry_run: true" ) != std::string::npos );
    assert( backup_delete_result_to_json( dry_run ).find( "\"source\": \"local\"" ) != std::string::npos );

    auto deleted_old = delete_local_backup_snapshot( repo, old_id, false );
    assert( deleted_old.deleted_snapshot );
    assert( deleted_old.deleted_object_count == 1 );
    assert( !std::filesystem::exists( repo / "snapshots" / old_id ) );
    assert( !std::filesystem::exists( repo / "objects" / "sha256" / "aa" / "aa" / old_only_hash ) );
    assert( std::filesystem::exists( repo / "objects" / "sha256" / "cc" / "cc" / shared_hash ) );
    assert( read_file( repo / "latest.json" ).find( new_id ) != std::string::npos );

    auto deleted_latest = delete_local_backup_snapshot( repo, new_id, false );
    assert( deleted_latest.deleted_snapshot );
    assert( deleted_latest.deleted_latest );
    assert( deleted_latest.new_latest_backup_id.empty() );
    assert( !std::filesystem::exists( repo / "latest.json" ) );
    assert( !std::filesystem::exists( repo / "objects" / "sha256" / "bb" / "bb" / new_only_hash ) );
    assert( !std::filesystem::exists( repo / "objects" / "sha256" / "cc" / "cc" / shared_hash ) );

    bool rejected_latest_alias = false;
    try
    {
      (void)delete_local_backup_snapshot( repo, "latest", true );
    }
    catch( const std::runtime_error& e )
    {
      rejected_latest_alias = std::string( e.what() ).find( "latest" ) != std::string::npos;
    }
    assert( rejected_latest_alias );

    std::filesystem::remove_all( root );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-activation-intent" );
    auto basedir = root / "basedir";
    auto staging = root / "staging";
    write_file( basedir / "db" / "CURRENT", "old-db" );
    write_file( staging / "db" / "CURRENT", "new-db" );
    write_file( staging / "RESTORE_STAGE_COMPLETE", "ok\n" );
    write_file( staging / ".teleno-restore-stage.json", "{ \"backup_id\": \"intent-test\" }\n" );

    const auto intent_path = restore_activation_intent_path( basedir );
    write_file( intent_path,
                "{\n"
                "  \"format\": \"teleno-native-restore-activation-request\",\n"
                "  \"version\": 1,\n"
                "  \"target_basedir\": \"" + basedir.string() + "\",\n"
                "  \"staging_dir\": \"" + staging.string() + "\",\n"
                "  \"requires_node_stop\": true\n"
                "}\n" );

    auto intent = read_pending_restore_activation_request( basedir );
    assert( intent );
    assert( intent->staging_dir == staging );
    assert( restore_activation_intent_to_json( *intent ).find( "\"requires_node_stop\": true" ) != std::string::npos );

    auto activation = activate_pending_restore_activation_request( basedir );
    assert( activation.backup_id == "intent-test" );
    assert( !std::filesystem::exists( intent_path ) );
    assert( read_file( basedir / "db" / "CURRENT" ) == "new-db" );
    assert( read_file( activation.pre_restore_dir / "db" / "CURRENT" ) == "old-db" );

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
