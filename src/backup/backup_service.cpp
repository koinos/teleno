#include "backup/backup_service.hpp"

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "backup/checkpoint_manager.hpp"

namespace koinos::node::backup {
namespace {

uint64_t now_milliseconds()
{
  return static_cast< uint64_t >(
    std::chrono::duration_cast< std::chrono::milliseconds >(
      std::chrono::system_clock::now().time_since_epoch() ).count() );
}

std::string json_escape( const std::string& value )
{
  std::ostringstream out;
  for( unsigned char ch: value )
  {
    switch( ch )
    {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if( ch < 0x20 )
        {
          static const char* hex = "0123456789abcdef";
          out << "\\u00" << hex[ ch >> 4 ] << hex[ ch & 0x0f ];
        }
        else
        {
          out << static_cast< char >( ch );
        }
    }
  }
  return out.str();
}

void write_text_file_atomic( const std::filesystem::path& path, const std::string& content )
{
  std::filesystem::create_directories( path.parent_path() );
  const auto tmp = path.parent_path() / ( path.filename().string() + ".tmp" );
  {
    std::ofstream out( tmp, std::ios::binary | std::ios::trunc );
    if( !out )
      throw std::runtime_error( "failed to write temporary file: " + tmp.string() );
    out << content;
  }
  std::filesystem::rename( tmp, path );
}

} // anonymous namespace

BackupService::BackupService( NodeConfig cfg,
                              std::filesystem::path basedir,
                              std::filesystem::path config_path,
                              storage::RocksDBManager& storage_db )
  : _cfg( std::move( cfg ) ),
    _basedir( std::move( basedir ) ),
    _config_path( std::move( config_path ) ),
    _storage_db( storage_db )
{}

BackupService::~BackupService()
{
  wait_for_current_operation();
}

BackupOperationStatus BackupService::status() const
{
  std::lock_guard< std::mutex > lock( _mutex );
  return _status;
}

void BackupService::validate_local_snapshot_request() const
{
  if( !_cfg.backup.enabled )
    throw std::runtime_error( "backup.enabled must be true for native backup service" );
  if( !_cfg.backup.local.enabled )
    throw std::runtime_error( "backup.local.enabled must be true for local backup snapshots" );
  if( _cfg.backup.workspace.empty() )
    throw std::runtime_error( "backup.workspace is required for native backup service" );
  if( _cfg.backup.local.directory.empty() )
    throw std::runtime_error( "backup.local.directory is required for local backup snapshots" );
  if( !_storage_db.db() )
    throw std::runtime_error( "RocksDB is not open; cannot create native backup snapshot" );

  const auto chain_layout = _storage_db.read_metadata( "layout.chain_storage" );
  if( chain_layout != "unified" )
    throw std::runtime_error( "native backup service requires unified chain storage; current layout is "
                              + ( chain_layout.empty() ? std::string( "unknown" ) : chain_layout ) );
}

std::string BackupService::next_operation_id() const
{
  return "runtime-local-snapshot-" + std::to_string( now_milliseconds() );
}

void BackupService::begin_operation( const std::string& operation_id,
                                     const std::filesystem::path& checkpoint_dir )
{
  std::lock_guard< std::mutex > lock( _mutex );
  if( _running )
    throw std::runtime_error( "backup operation already running: " + _status.operation_id );
  _running = true;
  _status = BackupOperationStatus{};
  _status.operation_id = operation_id;
  _status.state = BackupOperationState::running;
  _status.message = "creating local backup snapshot";
  _status.checkpoint_dir = checkpoint_dir;
  _status.started_at_ms = now_milliseconds();
}

void BackupService::throw_if_cancel_requested() const
{
  std::lock_guard< std::mutex > lock( _mutex );
  if( _status.cancel_requested )
    throw std::runtime_error( "backup operation cancelled" );
}

LocalSnapshotResult BackupService::execute_local_snapshot_operation( const std::string& operation_id,
                                                                     const std::filesystem::path& checkpoint_dir,
                                                                     bool rethrow_on_failure )
{
  try
  {
    throw_if_cancel_requested();

    CheckpointManager checkpoint_manager( _basedir, _storage_db );
    auto checkpoint = checkpoint_manager.create_checkpoint( checkpoint_dir );

    throw_if_cancel_requested();

    LocalSnapshotRepository repository( _cfg.backup.local.directory );
    auto snapshot = repository.store_checkpoint_snapshot( checkpoint, _cfg, _basedir, _config_path );

    std::error_code cleanup_ec;
    std::filesystem::remove_all( checkpoint_dir, cleanup_ec );

    {
      std::lock_guard< std::mutex > lock( _mutex );
      _status.state = BackupOperationState::succeeded;
      _status.message = "local backup snapshot created";
      _status.finished_at_ms = now_milliseconds();
      _status.cancel_requested = false;
      _status.has_snapshot = true;
      _status.snapshot = snapshot;
      _running = false;
    }
    return snapshot;
  }
  catch( const std::exception& e )
  {
    std::error_code cleanup_ec;
    std::filesystem::remove_all( checkpoint_dir, cleanup_ec );
    {
      std::lock_guard< std::mutex > lock( _mutex );
      _status.state = BackupOperationState::failed;
      _status.message = e.what();
      _status.finished_at_ms = now_milliseconds();
      _running = false;
    }
    if( rethrow_on_failure )
      throw;
  }
  catch( ... )
  {
    std::error_code cleanup_ec;
    std::filesystem::remove_all( checkpoint_dir, cleanup_ec );
    {
      std::lock_guard< std::mutex > lock( _mutex );
      _status.state = BackupOperationState::failed;
      _status.message = "unknown backup failure";
      _status.finished_at_ms = now_milliseconds();
      _running = false;
    }
    if( rethrow_on_failure )
      throw;
  }

  return {};
}

void BackupService::join_finished_worker()
{
  std::thread finished_worker;
  {
    std::lock_guard< std::mutex > lock( _mutex );
    if( _worker.joinable() && !_worker_active )
      finished_worker = std::move( _worker );
  }

  if( finished_worker.joinable() )
    finished_worker.join();
}

LocalSnapshotResult BackupService::create_local_snapshot()
{
  join_finished_worker();
  validate_local_snapshot_request();

  const auto operation_id = next_operation_id();
  const auto checkpoint_dir = std::filesystem::path( _cfg.backup.workspace )
                              / ".teleno-checkpoints"
                              / operation_id;
  begin_operation( operation_id, checkpoint_dir );
  return execute_local_snapshot_operation( operation_id, checkpoint_dir, true );
}

BackupOperationStatus BackupService::start_local_snapshot_async()
{
  join_finished_worker();
  validate_local_snapshot_request();

  const auto operation_id = next_operation_id();
  const auto checkpoint_dir = std::filesystem::path( _cfg.backup.workspace )
                              / ".teleno-checkpoints"
                              / operation_id;
  begin_operation( operation_id, checkpoint_dir );

  {
    std::lock_guard< std::mutex > lock( _mutex );
    _worker_active = true;
  }

  try
  {
    _worker = std::thread( [this, operation_id, checkpoint_dir]() {
      (void)execute_local_snapshot_operation( operation_id, checkpoint_dir, false );
      std::lock_guard< std::mutex > lock( _mutex );
      _worker_active = false;
    } );
  }
  catch( const std::exception& e )
  {
    std::lock_guard< std::mutex > lock( _mutex );
    _status.state = BackupOperationState::failed;
    _status.message = e.what();
    _status.finished_at_ms = now_milliseconds();
    _running = false;
    _worker_active = false;
    throw;
  }

  return status();
}

BackupOperationStatus BackupService::cancel_current_operation()
{
  std::lock_guard< std::mutex > lock( _mutex );
  if( _running )
  {
    _status.cancel_requested = true;
    _status.message = "backup cancellation requested";
  }
  return _status;
}

void BackupService::wait_for_current_operation()
{
  std::thread worker;
  {
    std::lock_guard< std::mutex > lock( _mutex );
    if( _worker.joinable() )
      worker = std::move( _worker );
  }

  if( worker.joinable() )
    worker.join();
}

RestoreStageResult BackupService::stage_restore_snapshot( const std::filesystem::path& requested_staging_dir )
{
  {
    std::lock_guard< std::mutex > lock( _mutex );
    if( _running )
      throw std::runtime_error( "cannot stage restore while backup operation is running: " + _status.operation_id );
  }

  if( !_cfg.backup.local.enabled )
    throw std::runtime_error( "backup.local.enabled must be true for local restore staging" );
  if( _cfg.backup.local.directory.empty() )
    throw std::runtime_error( "backup.local.directory is required for local restore staging" );

  return stage_local_restore_snapshot( _cfg.backup.local.directory, _basedir, requested_staging_dir );
}

RestoreActivationRequest BackupService::request_restore_activation( const std::filesystem::path& requested_staging_dir )
{
  {
    std::lock_guard< std::mutex > lock( _mutex );
    if( _running )
      throw std::runtime_error( "cannot request restore activation while backup operation is running: " + _status.operation_id );
  }

  std::filesystem::path staging_dir = requested_staging_dir;
  if( staging_dir.empty() )
  {
    if( !_cfg.backup.local.enabled )
      throw std::runtime_error( "backup.local.enabled must be true when restore activation staging_dir is omitted" );
    if( _cfg.backup.local.directory.empty() )
      throw std::runtime_error( "backup.local.directory is required when restore activation staging_dir is omitted" );
    auto preflight = build_local_restore_preflight( _cfg.backup.local.directory, _basedir );
    staging_dir = _basedir / ".teleno-restore-staging" / preflight.backup_id;
  }

  if( !std::filesystem::exists( staging_dir / "RESTORE_STAGE_COMPLETE" ) )
    throw std::runtime_error( "restore staging directory is not complete: " + staging_dir.string() );
  if( !std::filesystem::is_directory( staging_dir / "db" ) )
    throw std::runtime_error( "restore staging directory does not contain a db directory: " + staging_dir.string() );

  RestoreActivationRequest request;
  request.target_basedir = _basedir;
  request.staging_dir = staging_dir;
  request.intent_path = _basedir / ".teleno-restore-activation-request.json";
  request.requires_node_stop = true;
  request.ready_to_activate = true;
  request.message = "restore activation request written; stop the node and run stopped-node activation";

  std::ostringstream intent;
  intent << "{\n";
  intent << "  \"format\": \"teleno-native-restore-activation-request\",\n";
  intent << "  \"version\": 1,\n";
  intent << "  \"target_basedir\": \"" << json_escape( request.target_basedir.string() ) << "\",\n";
  intent << "  \"staging_dir\": \"" << json_escape( request.staging_dir.string() ) << "\",\n";
  intent << "  \"requires_node_stop\": true,\n";
  intent << "  \"activation_command\": \"teleno_node --basedir "
         << json_escape( request.target_basedir.string() )
         << " --backup-restore-activate --backup-output "
         << json_escape( request.staging_dir.string() ) << "\"\n";
  intent << "}\n";
  write_text_file_atomic( request.intent_path, intent.str() );

  return request;
}

const char* backup_operation_state_name( BackupOperationState state )
{
  switch( state )
  {
    case BackupOperationState::idle: return "idle";
    case BackupOperationState::running: return "running";
    case BackupOperationState::succeeded: return "succeeded";
    case BackupOperationState::failed: return "failed";
  }
  return "unknown";
}

std::string backup_operation_status_to_text( const BackupOperationStatus& status )
{
  std::ostringstream out;
  out << "Backup operation status\n";
  out << "operation_id: " << status.operation_id << "\n";
  out << "state: " << backup_operation_state_name( status.state ) << "\n";
  out << "message: " << status.message << "\n";
  out << "checkpoint_dir: " << status.checkpoint_dir.string() << "\n";
  out << "started_at_ms: " << status.started_at_ms << "\n";
  out << "finished_at_ms: " << status.finished_at_ms << "\n";
  out << "cancel_requested: " << ( status.cancel_requested ? "true" : "false" ) << "\n";
  out << "has_snapshot: " << ( status.has_snapshot ? "true" : "false" ) << "\n";
  if( status.has_snapshot )
  {
    out << "backup_id: " << status.snapshot.backup_id << "\n";
    out << "snapshot_dir: " << status.snapshot.snapshot_dir.string() << "\n";
  }
  return out.str();
}

std::string backup_operation_status_to_json( const BackupOperationStatus& status )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"operation_id\": \"" << json_escape( status.operation_id ) << "\",\n";
  out << "  \"state\": \"" << backup_operation_state_name( status.state ) << "\",\n";
  out << "  \"message\": \"" << json_escape( status.message ) << "\",\n";
  out << "  \"checkpoint_dir\": \"" << json_escape( status.checkpoint_dir.string() ) << "\",\n";
  out << "  \"started_at_ms\": " << status.started_at_ms << ",\n";
  out << "  \"finished_at_ms\": " << status.finished_at_ms << ",\n";
  out << "  \"cancel_requested\": " << ( status.cancel_requested ? "true" : "false" ) << ",\n";
  out << "  \"has_snapshot\": " << ( status.has_snapshot ? "true" : "false" );
  if( status.has_snapshot )
  {
    out << ",\n";
    out << "  \"snapshot\": {\n";
    out << "    \"backup_id\": \"" << json_escape( status.snapshot.backup_id ) << "\",\n";
    out << "    \"snapshot_dir\": \"" << json_escape( status.snapshot.snapshot_dir.string() ) << "\",\n";
    out << "    \"file_count\": " << status.snapshot.file_count << ",\n";
    out << "    \"object_count\": " << status.snapshot.object_count << ",\n";
    out << "    \"total_bytes\": " << status.snapshot.total_bytes << "\n";
    out << "  }\n";
  }
  else
  {
    out << "\n";
  }
  out << "}\n";
  return out.str();
}

std::string restore_activation_request_to_json( const RestoreActivationRequest& request )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"target_basedir\": \"" << json_escape( request.target_basedir.string() ) << "\",\n";
  out << "  \"staging_dir\": \"" << json_escape( request.staging_dir.string() ) << "\",\n";
  out << "  \"intent_path\": \"" << json_escape( request.intent_path.string() ) << "\",\n";
  out << "  \"requires_node_stop\": " << ( request.requires_node_stop ? "true" : "false" ) << ",\n";
  out << "  \"ready_to_activate\": " << ( request.ready_to_activate ? "true" : "false" ) << ",\n";
  out << "  \"message\": \"" << json_escape( request.message ) << "\"\n";
  out << "}\n";
  return out.str();
}

} // namespace koinos::node::backup
