#include "backup/backup_service.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>
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

std::string BackupService::config_summary_json() const
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"basedir\": \"" << json_escape( _basedir.string() ) << "\",\n";
  out << "  \"config_path\": \"" << json_escape( _config_path.string() ) << "\",\n";
  out << "  \"backup\": {\n";
  out << "    \"enabled\": " << ( _cfg.backup.enabled ? "true" : "false" ) << ",\n";
  out << "    \"node_id\": \"" << json_escape( _cfg.backup.node_id ) << "\",\n";
  out << "    \"workspace\": \"" << json_escape( _cfg.backup.workspace ) << "\",\n";
  out << "    \"local\": {\n";
  out << "      \"enabled\": " << ( _cfg.backup.local.enabled ? "true" : "false" ) << ",\n";
  out << "      \"directory\": \"" << json_escape( _cfg.backup.local.directory ) << "\",\n";
  out << "      \"retention_count\": " << _cfg.backup.local.retention_count << "\n";
  out << "    },\n";
  out << "    \"remote\": {\n";
  out << "      \"enabled\": " << ( _cfg.backup.remote.enabled ? "true" : "false" ) << ",\n";
  out << "      \"directory\": \"" << json_escape( _cfg.backup.remote.directory ) << "\",\n";
  out << "      \"retention_count\": " << _cfg.backup.remote.retention_count << ",\n";
  out << "      \"retention_days\": " << _cfg.backup.remote.retention_days << ",\n";
  out << "      \"upload_temp_suffix\": \"" << json_escape( _cfg.backup.remote.upload_temp_suffix ) << "\"\n";
  out << "    },\n";
  out << "    \"ssh\": {\n";
  out << "      \"enabled\": " << ( _cfg.backup.ssh.enabled ? "true" : "false" ) << ",\n";
  out << "      \"transport\": \"" << json_escape( _cfg.backup.ssh.transport ) << "\",\n";
  out << "      \"host\": \"" << json_escape( _cfg.backup.ssh.host ) << "\",\n";
  out << "      \"port\": " << _cfg.backup.ssh.port << ",\n";
  out << "      \"user\": \"" << json_escape( _cfg.backup.ssh.user ) << "\",\n";
  out << "      \"auth\": \"" << json_escape( _cfg.backup.ssh.auth ) << "\",\n";
  out << "      \"password_file_configured\": " << ( _cfg.backup.ssh.password_file.empty() ? "false" : "true" ) << ",\n";
  out << "      \"private_key_file\": \"" << json_escape( _cfg.backup.ssh.private_key_file ) << "\",\n";
  out << "      \"passphrase_file_configured\": " << ( _cfg.backup.ssh.passphrase_file.empty() ? "false" : "true" ) << ",\n";
  out << "      \"known_hosts_file\": \"" << json_escape( _cfg.backup.ssh.known_hosts_file ) << "\",\n";
  out << "      \"strict_host_key_checking\": " << ( _cfg.backup.ssh.strict_host_key_checking ? "true" : "false" ) << ",\n";
  out << "      \"connect_timeout_seconds\": " << _cfg.backup.ssh.connect_timeout_seconds << "\n";
  out << "    },\n";
  out << "    \"admin\": {\n";
  out << "      \"enabled\": " << ( _cfg.backup.admin.enabled ? "true" : "false" ) << ",\n";
  out << "      \"listen\": \"" << json_escape( _cfg.backup.admin.listen ) << "\",\n";
  out << "      \"token_file_configured\": " << ( _cfg.backup.admin.token_file.empty() ? "false" : "true" ) << ",\n";
  out << "      \"jobs\": " << _cfg.backup.admin.jobs << "\n";
  out << "    },\n";
  out << "    \"public_restore\": {\n";
  out << "      \"enabled\": " << ( _cfg.backup.public_restore.enabled ? "true" : "false" ) << ",\n";
  out << "      \"base_url\": \"" << json_escape( _cfg.backup.public_restore.base_url ) << "\",\n";
  out << "      \"network\": \"" << json_escape( _cfg.backup.public_restore.network ) << "\",\n";
  out << "      \"require_https\": " << ( _cfg.backup.public_restore.require_https ? "true" : "false" ) << ",\n";
  out << "      \"timeout_seconds\": " << _cfg.backup.public_restore.timeout_seconds << ",\n";
  out << "      \"retries\": " << _cfg.backup.public_restore.retries << "\n";
  out << "    },\n";
  out << "    \"public_publish\": {\n";
  out << "      \"enabled\": " << ( _cfg.backup.public_publish.enabled ? "true" : "false" ) << ",\n";
  out << "      \"directory\": \"" << json_escape( _cfg.backup.public_publish.directory ) << "\",\n";
  out << "      \"base_url\": \"" << json_escape( _cfg.backup.public_publish.base_url ) << "\",\n";
  out << "      \"network\": \"" << json_escape( _cfg.backup.public_publish.network ) << "\",\n";
  out << "      \"observer_config_file\": \"" << json_escape( _cfg.backup.public_publish.observer_config_file ) << "\",\n";
  out << "      \"retention_count\": " << _cfg.backup.public_publish.retention_count << ",\n";
  out << "      \"upload_temp_suffix\": \"" << json_escape( _cfg.backup.public_publish.upload_temp_suffix ) << "\"\n";
  out << "    }\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

BackupSnapshotListResult BackupService::list_local_snapshots() const
{
  validate_local_repository_request( "local backup list" );
  return list_local_backup_snapshots( _cfg.backup.local.directory );
}

BackupSnapshotListResult BackupService::list_remote_snapshots()
{
  validate_remote_repository_request( "remote backup list" );
  return list_remote_backup_snapshots_with_managed_sftp(
    _cfg.backup.local.directory,
    _cfg.backup.ssh,
    _cfg.backup.remote );
}

BackupSnapshotListResult BackupService::list_public_snapshots( const std::string& backup_id )
{
  validate_public_restore_request( "public backup list" );
  return list_public_backup_snapshots(
    _cfg.backup.local.directory,
    _cfg.backup.public_restore,
    backup_id == "latest" ? std::string{} : backup_id );
}

RestorePreflightResult BackupService::restore_preflight( const std::string& backup_id ) const
{
  validate_local_repository_request( "restore preflight" );
  return build_local_restore_preflight(
    _cfg.backup.local.directory,
    _basedir,
    backup_id == "latest" ? std::string{} : backup_id );
}

RestorePreflightResult BackupService::public_restore_preflight( const std::string& backup_id )
{
  const auto selected_backup_id = backup_id == "latest" ? std::string{} : backup_id;
  validate_public_restore_request( "public restore preflight" );
  (void)list_public_backup_snapshots(
    _cfg.backup.local.directory,
    _cfg.backup.public_restore,
    selected_backup_id );
  return build_local_restore_preflight(
    _cfg.backup.local.directory,
    _basedir,
    selected_backup_id );
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

void BackupService::validate_local_repository_request( const std::string& operation_name ) const
{
  if( !_cfg.backup.local.enabled )
    throw std::invalid_argument( operation_name + " requires backup.local.enabled=true" );
  if( _cfg.backup.local.directory.empty() )
    throw std::invalid_argument( operation_name + " requires backup.local.directory" );
}

void BackupService::validate_remote_repository_request( const std::string& operation_name ) const
{
  validate_local_repository_request( operation_name );
  if( !_cfg.backup.remote.enabled )
    throw std::invalid_argument( operation_name + " requires backup.remote.enabled=true" );
  if( _cfg.backup.remote.directory.empty() )
    throw std::invalid_argument( operation_name + " requires backup.remote.directory" );
  if( !_cfg.backup.ssh.enabled )
    throw std::invalid_argument( operation_name + " requires backup.ssh.enabled=true" );
}

void BackupService::validate_public_restore_request( const std::string& operation_name ) const
{
  validate_local_repository_request( operation_name );
  if( !_cfg.backup.public_restore.enabled )
    throw std::invalid_argument( operation_name + " requires backup.public-restore.enabled=true" );
  if( _cfg.backup.public_restore.base_url.empty() )
    throw std::invalid_argument( operation_name + " requires backup.public-restore.base-url" );
}

void BackupService::validate_public_publish_request( const std::string& operation_name ) const
{
  validate_remote_repository_request( operation_name );
  if( !_cfg.backup.public_publish.enabled )
    throw std::invalid_argument( operation_name + " requires backup.public-publish.enabled=true" );
  if( _cfg.backup.public_publish.directory.empty() )
    throw std::invalid_argument( operation_name + " requires backup.public-publish.directory" );
  if( _cfg.backup.public_publish.base_url.empty() )
    throw std::invalid_argument( operation_name + " requires backup.public-publish.base-url" );
  if( _cfg.backup.public_publish.network.empty() )
    throw std::invalid_argument( operation_name + " requires backup.public-publish.network" );
  if( _cfg.backup.public_publish.observer_config_file.empty() )
    throw std::invalid_argument( operation_name + " requires backup.public-publish.observer-config-file" );
}

std::string BackupService::next_operation_id() const
{
  return next_operation_id( "local-snapshot" );
}

std::string BackupService::next_operation_id( const std::string& kind ) const
{
  return "runtime-" + kind + "-" + std::to_string( now_milliseconds() );
}

void BackupService::begin_operation( const std::string& operation_id,
                                     const std::string& operation_kind,
                                     const std::string& phase,
                                     const std::string& message,
                                     const std::filesystem::path& checkpoint_dir )
{
  std::lock_guard< std::mutex > lock( _mutex );
  if( _running )
    throw std::runtime_error( "backup operation already running: " + _status.operation_id );
  _running = true;
  _status = BackupOperationStatus{};
  _status.operation_id = operation_id;
  _status.operation_kind = operation_kind;
  _status.state = BackupOperationState::running;
  _status.phase = phase;
  _status.message = message;
  _status.checkpoint_dir = checkpoint_dir;
  _status.started_at_ms = now_milliseconds();
}

void BackupService::update_operation_progress( const std::string& phase,
                                               const std::string& message,
                                               const SftpTransferProgress* progress )
{
  std::lock_guard< std::mutex > lock( _mutex );
  _status.phase = phase;
  _status.message = message;
  if( progress )
  {
    _status.completed_batches = progress->completed_batches;
    _status.total_batches = progress->total_batches;
    _status.attempt = progress->attempt;
    _status.progress_file_count = progress->file_count;
    _status.progress_completed_bytes = progress->completed_bytes;
    _status.progress_total_bytes = progress->total_bytes;
  }
}

void BackupService::finish_operation_success( const std::string& message )
{
  std::lock_guard< std::mutex > lock( _mutex );
  _status.state = BackupOperationState::succeeded;
  _status.phase = "complete";
  _status.message = message;
  _status.finished_at_ms = now_milliseconds();
  _status.cancel_requested = false;
  _running = false;
}

void BackupService::finish_operation_failure( const std::string& message )
{
  std::lock_guard< std::mutex > lock( _mutex );
  _status.state = BackupOperationState::failed;
  _status.phase = "error";
  _status.message = message;
  _status.finished_at_ms = now_milliseconds();
  _running = false;
}

void BackupService::throw_if_cancel_requested() const
{
  std::lock_guard< std::mutex > lock( _mutex );
  if( _status.cancel_requested )
    throw std::runtime_error( "backup operation cancelled" );
}

LocalSnapshotResult BackupService::create_local_snapshot_body( const std::filesystem::path& checkpoint_dir )
{
  throw_if_cancel_requested();
  update_operation_progress( "checkpoint", "creating hot RocksDB checkpoint" );

  CheckpointManager checkpoint_manager( _basedir, _storage_db );
  auto checkpoint = checkpoint_manager.create_checkpoint( checkpoint_dir );

  throw_if_cancel_requested();
  update_operation_progress( "snapshot", "storing local backup snapshot" );

  LocalSnapshotRepository repository( _cfg.backup.local.directory );
  return repository.store_checkpoint_snapshot( checkpoint, _cfg, _basedir, _config_path );
}

LocalSnapshotResult BackupService::execute_local_snapshot_operation( const std::string& operation_id,
                                                                     const std::filesystem::path& checkpoint_dir,
                                                                     bool rethrow_on_failure )
{
  try
  {
    auto snapshot = create_local_snapshot_body( checkpoint_dir );

    std::error_code cleanup_ec;
    std::filesystem::remove_all( checkpoint_dir, cleanup_ec );

    {
      std::lock_guard< std::mutex > lock( _mutex );
      _status.has_snapshot = true;
      _status.snapshot = snapshot;
    }
    finish_operation_success( "local backup snapshot created" );
    return snapshot;
  }
  catch( const std::exception& e )
  {
    std::error_code cleanup_ec;
    std::filesystem::remove_all( checkpoint_dir, cleanup_ec );
    finish_operation_failure( e.what() );
    if( rethrow_on_failure )
      throw;
  }
  catch( ... )
  {
    std::error_code cleanup_ec;
    std::filesystem::remove_all( checkpoint_dir, cleanup_ec );
    finish_operation_failure( "unknown backup failure" );
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
  begin_operation( operation_id,
                   "local-snapshot",
                   "prepare",
                   "creating local backup snapshot",
                   checkpoint_dir );
  return execute_local_snapshot_operation( operation_id, checkpoint_dir, true );
}

void BackupService::execute_configured_backup_operation( const std::string&,
                                                         const std::filesystem::path& checkpoint_dir,
                                                         bool upload_remote )
{
  try
  {
    auto snapshot = create_local_snapshot_body( checkpoint_dir );
    std::error_code cleanup_ec;
    std::filesystem::remove_all( checkpoint_dir, cleanup_ec );

    {
      std::lock_guard< std::mutex > lock( _mutex );
      _status.has_snapshot = true;
      _status.snapshot = snapshot;
    }

    if( upload_remote )
    {
      throw_if_cancel_requested();
      update_operation_progress( "upload", "uploading latest native backup to remote SFTP" );
      auto upload = upload_latest_snapshot_with_managed_sftp(
        _cfg.backup.local.directory,
        _cfg.backup.ssh,
        _cfg.backup.remote,
        operation_sftp_transfer_options() );
      {
        std::lock_guard< std::mutex > lock( _mutex );
        _status.has_remote_upload = true;
        _status.remote_upload = upload;
      }
      publish_public_bootstrap_if_configured();
      prune_remote_retention_if_configured( upload.backup_id );
      finish_operation_success( _cfg.backup.public_publish.enabled
                                  ? "local backup snapshot uploaded and published as public bootstrap"
                                  : "local backup snapshot created and uploaded to remote SFTP" );
    }
    else
    {
      finish_operation_success( "local backup snapshot created" );
    }
  }
  catch( const std::exception& e )
  {
    std::error_code cleanup_ec;
    std::filesystem::remove_all( checkpoint_dir, cleanup_ec );
    finish_operation_failure( e.what() );
  }
  catch( ... )
  {
    std::error_code cleanup_ec;
    std::filesystem::remove_all( checkpoint_dir, cleanup_ec );
    finish_operation_failure( "unknown backup failure" );
  }
}

void BackupService::publish_public_bootstrap_if_configured()
{
  if( !_cfg.backup.public_publish.enabled )
    return;

  validate_public_publish_request( "public bootstrap publish" );
  throw_if_cancel_requested();
  update_operation_progress( "public-publish", "publishing latest backup to public bootstrap URL" );
  auto published = publish_latest_public_bootstrap_with_managed_sftp(
    _cfg.backup.local.directory,
    _cfg.backup.ssh,
    _cfg.backup.public_publish,
    operation_sftp_transfer_options() );
  {
    std::lock_guard< std::mutex > lock( _mutex );
    _status.has_public_publish = true;
    _status.public_publish = published;
  }
}

void BackupService::prune_remote_retention_if_configured( const std::string& keep_backup_id )
{
  const auto retention_count = _cfg.backup.remote.retention_count;
  if( retention_count == 0 )
    return;

  throw_if_cancel_requested();
  update_operation_progress( "remote-retention", "pruning old remote backup snapshots" );
  auto snapshots = list_remote_backup_snapshots_with_managed_sftp(
    _cfg.backup.local.directory,
    _cfg.backup.ssh,
    _cfg.backup.remote,
    operation_sftp_transfer_options() );
  if( snapshots.snapshots.size() <= retention_count )
    return;

  std::vector< std::string > backup_ids;
  for( const auto& snapshot: snapshots.snapshots )
  {
    if( !snapshot.backup_id.empty() )
      backup_ids.push_back( snapshot.backup_id );
  }
  std::sort( backup_ids.begin(), backup_ids.end() );
  backup_ids.erase( std::unique( backup_ids.begin(), backup_ids.end() ), backup_ids.end() );
  if( backup_ids.size() <= retention_count )
    return;

  std::set< std::string > keep;
  for( std::size_t i = backup_ids.size() - static_cast< std::size_t >( retention_count );
       i < backup_ids.size();
       ++i )
  {
    keep.insert( backup_ids[ i ] );
  }
  if( !keep_backup_id.empty() )
    keep.insert( keep_backup_id );

  std::vector< BackupDeleteResult > results;
  {
    std::lock_guard< std::mutex > lock( _mutex );
    results = _status.delete_results;
  }

  for( const auto& backup_id: backup_ids )
  {
    if( keep.find( backup_id ) != keep.end() )
      continue;
    throw_if_cancel_requested();
    update_operation_progress( "remote-retention-delete", "deleting old remote backup snapshot " + backup_id );
    results.push_back( delete_remote_backup_snapshot_with_managed_sftp(
      _cfg.backup.local.directory,
      _cfg.backup.ssh,
      _cfg.backup.remote,
      backup_id,
      false,
      operation_sftp_transfer_options() ) );
    {
      std::lock_guard< std::mutex > lock( _mutex );
      _status.delete_results = results;
    }
  }
}

void BackupService::execute_upload_latest_operation()
{
  try
  {
    throw_if_cancel_requested();
    update_operation_progress( "upload", "uploading latest native backup to remote SFTP" );
    auto upload = upload_latest_snapshot_with_managed_sftp(
      _cfg.backup.local.directory,
      _cfg.backup.ssh,
      _cfg.backup.remote,
      operation_sftp_transfer_options() );
    {
      std::lock_guard< std::mutex > lock( _mutex );
      _status.has_remote_upload = true;
      _status.remote_upload = upload;
    }
    publish_public_bootstrap_if_configured();
    prune_remote_retention_if_configured( upload.backup_id );
    finish_operation_success( _cfg.backup.public_publish.enabled
                                ? "latest native backup uploaded and published as public bootstrap"
                                : "latest native backup uploaded to remote SFTP" );
  }
  catch( const std::exception& e )
  {
    finish_operation_failure( e.what() );
  }
  catch( ... )
  {
    finish_operation_failure( "unknown remote upload failure" );
  }
}

void BackupService::execute_delete_operation( const std::string& scope,
                                              const std::string& backup_id,
                                              bool dry_run )
{
  try
  {
    std::vector< BackupDeleteResult > results;
    const auto delete_local = [&]() {
      throw_if_cancel_requested();
      update_operation_progress(
        "delete-local",
        dry_run ? "checking local backup delete impact" : "deleting local backup snapshot" );
      results.push_back( delete_local_backup_snapshot(
        _cfg.backup.local.directory,
        backup_id,
        dry_run ) );
    };
    const auto delete_remote = [&]() {
      throw_if_cancel_requested();
      update_operation_progress(
        "delete-remote",
        dry_run ? "checking remote backup delete impact" : "deleting remote backup snapshot" );
      results.push_back( delete_remote_backup_snapshot_with_managed_sftp(
        _cfg.backup.local.directory,
        _cfg.backup.ssh,
        _cfg.backup.remote,
        backup_id,
        dry_run,
        operation_sftp_transfer_options() ) );
    };

    if( scope == "both" && !dry_run )
    {
      delete_remote();
      delete_local();
    }
    else
    {
      if( scope == "local" || scope == "both" )
        delete_local();
      if( scope == "remote" || scope == "both" )
        delete_remote();
    }

    {
      std::lock_guard< std::mutex > lock( _mutex );
      _status.delete_results = results;
    }
    finish_operation_success( dry_run ? "native backup delete dry-run complete" : "native backup delete complete" );
  }
  catch( const std::exception& e )
  {
    finish_operation_failure( e.what() );
  }
  catch( ... )
  {
    finish_operation_failure( "unknown native backup delete failure" );
  }
}

void BackupService::execute_restore_fetch_operation( const std::string& backup_id )
{
  try
  {
    throw_if_cancel_requested();
    update_operation_progress( "restore-fetch", "fetching remote backup restore data" );
    auto fetch = fetch_restore_snapshot_with_managed_sftp(
      _cfg.backup.local.directory,
      _basedir,
      _cfg.backup.ssh,
      _cfg.backup.remote,
      backup_id,
      operation_sftp_transfer_options() );
    {
      std::lock_guard< std::mutex > lock( _mutex );
      _status.has_restore_fetch = true;
      _status.restore_fetch = fetch;
      _status.has_restore_preflight = true;
      _status.restore_preflight = fetch.preflight;
    }
    finish_operation_success( fetch.ready_to_stage
                                ? "remote backup restore data fetched and ready to stage"
                                : "remote backup restore data fetched but not ready to stage" );
  }
  catch( const std::exception& e )
  {
    finish_operation_failure( e.what() );
  }
  catch( ... )
  {
    finish_operation_failure( "unknown remote restore fetch failure" );
  }
}

void BackupService::execute_public_restore_fetch_operation( const std::string& backup_id )
{
  try
  {
    throw_if_cancel_requested();
    update_operation_progress( "public-restore-fetch", "fetching public backup restore data" );
    auto fetch = fetch_public_restore_snapshot(
      _cfg.backup.local.directory,
      _basedir,
      _cfg.backup.public_restore,
      backup_id,
      operation_public_restore_options() );
    {
      std::lock_guard< std::mutex > lock( _mutex );
      _status.has_public_restore_fetch = true;
      _status.public_restore_fetch = fetch;
      _status.has_restore_preflight = true;
      _status.restore_preflight = fetch.preflight;
    }
    finish_operation_success( fetch.ready_to_stage
                                ? "public backup restore data fetched and ready to stage"
                                : "public backup restore data fetched but not ready to stage" );
  }
  catch( const std::exception& e )
  {
    finish_operation_failure( e.what() );
  }
  catch( ... )
  {
    finish_operation_failure( "unknown public restore fetch failure" );
  }
}

SftpTransferOptions BackupService::operation_sftp_transfer_options()
{
  SftpTransferOptions options;
  options.cancel_requested = [this]() {
    std::lock_guard< std::mutex > lock( _mutex );
    return _status.cancel_requested;
  };
  options.progress = [this]( const SftpTransferProgress& progress ) {
    update_operation_progress(
      progress.phase,
      progress.phase.empty() ? std::string( "remote SFTP transfer in progress" )
                             : std::string( "remote SFTP transfer phase: " ) + progress.phase,
      &progress );
  };
  return options;
}

PublicRestoreOptions BackupService::operation_public_restore_options()
{
  PublicRestoreOptions options;
  options.cancel_requested = [this]() {
    std::lock_guard< std::mutex > lock( _mutex );
    return _status.cancel_requested;
  };
  options.progress = [this]( const PublicRestoreProgress& progress ) {
    SftpTransferProgress normalized;
    normalized.phase = progress.phase;
    normalized.completed_batches = progress.completed_batches;
    normalized.total_batches = progress.total_batches;
    normalized.attempt = progress.attempt;
    normalized.file_count = progress.file_count;
    normalized.completed_bytes = progress.completed_bytes;
    normalized.total_bytes = progress.total_bytes;
    update_operation_progress(
      progress.phase,
      progress.phase.empty() ? std::string( "public restore transfer in progress" )
                             : std::string( "public restore transfer phase: " ) + progress.phase,
      &normalized );
  };
  return options;
}

BackupOperationStatus BackupService::start_configured_backup_async( bool upload_remote )
{
  join_finished_worker();
  validate_local_snapshot_request();
  if( upload_remote )
  {
    validate_remote_repository_request( "configured backup create" );
    if( _cfg.backup.public_publish.enabled )
      validate_public_publish_request( "configured backup public publish" );
  }

  const auto operation_id = next_operation_id( upload_remote ? "configured-backup" : "local-snapshot" );
  const auto checkpoint_dir = std::filesystem::path( _cfg.backup.workspace )
                              / ".teleno-checkpoints"
                              / operation_id;
  begin_operation( operation_id,
                   upload_remote ? "configured-backup" : "local-snapshot",
                   "prepare",
                   upload_remote ? "creating local backup snapshot before remote upload"
                                 : "creating local backup snapshot",
                   checkpoint_dir );

  {
    std::lock_guard< std::mutex > lock( _mutex );
    _worker_active = true;
  }

  try
  {
    _worker = std::thread( [this, operation_id, checkpoint_dir, upload_remote]() {
      execute_configured_backup_operation( operation_id, checkpoint_dir, upload_remote );
      std::lock_guard< std::mutex > lock( _mutex );
      _worker_active = false;
    } );
  }
  catch( const std::exception& e )
  {
    finish_operation_failure( e.what() );
    std::lock_guard< std::mutex > lock( _mutex );
    _worker_active = false;
    throw;
  }

  return status();
}

BackupOperationStatus BackupService::start_local_snapshot_async()
{
  return start_configured_backup_async( false );
}

BackupOperationStatus BackupService::start_upload_latest_async()
{
  join_finished_worker();
  validate_remote_repository_request( "remote upload latest" );
  if( _cfg.backup.public_publish.enabled )
    validate_public_publish_request( "remote upload latest public publish" );

  const auto operation_id = next_operation_id( "upload-latest" );
  begin_operation( operation_id,
                   "upload-latest",
                   "prepare",
                   "preparing latest native backup upload",
                   {} );
  {
    std::lock_guard< std::mutex > lock( _mutex );
    _worker_active = true;
  }

  try
  {
    _worker = std::thread( [this]() {
      execute_upload_latest_operation();
      std::lock_guard< std::mutex > lock( _mutex );
      _worker_active = false;
    } );
  }
  catch( const std::exception& e )
  {
    finish_operation_failure( e.what() );
    std::lock_guard< std::mutex > lock( _mutex );
    _worker_active = false;
    throw;
  }

  return status();
}

BackupOperationStatus BackupService::start_delete_async( const std::string& scope,
                                                         const std::string& backup_id,
                                                         const std::string& confirm )
{
  join_finished_worker();
  if( backup_id.empty() || backup_id == "latest" )
    throw std::invalid_argument( "backup delete requires an exact backup_id; 'latest' is not accepted" );
  if( scope != "local" && scope != "remote" && scope != "both" )
    throw std::invalid_argument( "backup delete scope must be local, remote, or both" );
  if( !confirm.empty() && confirm != backup_id )
    throw std::invalid_argument( "backup delete confirm must exactly match backup_id" );
  if( scope == "local" || scope == "both" )
    validate_local_repository_request( "local backup delete" );
  if( scope == "remote" || scope == "both" )
    validate_remote_repository_request( "remote backup delete" );

  const bool dry_run = confirm != backup_id;
  const auto operation_id = next_operation_id( "delete" );
  begin_operation( operation_id,
                   "delete",
                   "prepare",
                   dry_run ? "preparing native backup delete dry-run"
                           : "preparing native backup delete",
                   {} );

  {
    std::lock_guard< std::mutex > lock( _mutex );
    _worker_active = true;
  }

  try
  {
    _worker = std::thread( [this, scope, backup_id, dry_run]() {
      execute_delete_operation( scope, backup_id, dry_run );
      std::lock_guard< std::mutex > lock( _mutex );
      _worker_active = false;
    } );
  }
  catch( const std::exception& e )
  {
    finish_operation_failure( e.what() );
    std::lock_guard< std::mutex > lock( _mutex );
    _worker_active = false;
    throw;
  }

  return status();
}

BackupOperationStatus BackupService::start_restore_fetch_async( const std::string& backup_id )
{
  join_finished_worker();
  validate_remote_repository_request( "remote restore fetch" );

  const auto operation_id = next_operation_id( "restore-fetch" );
  begin_operation( operation_id,
                   "restore-fetch",
                   "prepare",
                   "preparing remote backup restore fetch",
                   {} );

  {
    std::lock_guard< std::mutex > lock( _mutex );
    _worker_active = true;
  }

  try
  {
    _worker = std::thread( [this, backup_id]() {
      execute_restore_fetch_operation( backup_id );
      std::lock_guard< std::mutex > lock( _mutex );
      _worker_active = false;
    } );
  }
  catch( const std::exception& e )
  {
    finish_operation_failure( e.what() );
    std::lock_guard< std::mutex > lock( _mutex );
    _worker_active = false;
    throw;
  }

  return status();
}

BackupOperationStatus BackupService::start_public_restore_fetch_async( const std::string& backup_id )
{
  join_finished_worker();
  validate_public_restore_request( "public restore fetch" );

  const auto operation_id = next_operation_id( "public-restore-fetch" );
  begin_operation( operation_id,
                   "public-restore-fetch",
                   "prepare",
                   "preparing public backup restore fetch",
                   {} );

  {
    std::lock_guard< std::mutex > lock( _mutex );
    _worker_active = true;
  }

  try
  {
    _worker = std::thread( [this, backup_id]() {
      execute_public_restore_fetch_operation( backup_id );
      std::lock_guard< std::mutex > lock( _mutex );
      _worker_active = false;
    } );
  }
  catch( const std::exception& e )
  {
    finish_operation_failure( e.what() );
    std::lock_guard< std::mutex > lock( _mutex );
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

RestoreStageResult BackupService::stage_restore_snapshot( const std::string& backup_id,
                                                          const std::filesystem::path& requested_staging_dir )
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

  return stage_local_restore_snapshot(
    _cfg.backup.local.directory,
    _basedir,
    backup_id == "latest" ? std::string{} : backup_id,
    requested_staging_dir );
}

RestoreActivationRequest BackupService::request_restore_activation( const std::string& backup_id,
                                                                    const std::filesystem::path& requested_staging_dir )
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
    auto preflight = build_local_restore_preflight(
      _cfg.backup.local.directory,
      _basedir,
      backup_id == "latest" ? std::string{} : backup_id );
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
  out << "operation_kind: " << status.operation_kind << "\n";
  out << "state: " << backup_operation_state_name( status.state ) << "\n";
  out << "phase: " << status.phase << "\n";
  out << "message: " << status.message << "\n";
  out << "checkpoint_dir: " << status.checkpoint_dir.string() << "\n";
  out << "started_at_ms: " << status.started_at_ms << "\n";
  out << "finished_at_ms: " << status.finished_at_ms << "\n";
  out << "cancel_requested: " << ( status.cancel_requested ? "true" : "false" ) << "\n";
  out << "completed_batches: " << status.completed_batches << "\n";
  out << "total_batches: " << status.total_batches << "\n";
  out << "attempt: " << status.attempt << "\n";
  out << "has_snapshot: " << ( status.has_snapshot ? "true" : "false" ) << "\n";
  if( status.has_snapshot )
  {
    out << "backup_id: " << status.snapshot.backup_id << "\n";
    out << "snapshot_dir: " << status.snapshot.snapshot_dir.string() << "\n";
  }
  out << "has_remote_upload: " << ( status.has_remote_upload ? "true" : "false" ) << "\n";
  out << "has_public_publish: " << ( status.has_public_publish ? "true" : "false" ) << "\n";
  out << "has_restore_fetch: " << ( status.has_restore_fetch ? "true" : "false" ) << "\n";
  out << "has_public_restore_fetch: " << ( status.has_public_restore_fetch ? "true" : "false" ) << "\n";
  out << "delete_result_count: " << status.delete_results.size() << "\n";
  return out.str();
}

std::string backup_operation_status_to_json( const BackupOperationStatus& status )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"operation_id\": \"" << json_escape( status.operation_id ) << "\",\n";
  out << "  \"operation_kind\": \"" << json_escape( status.operation_kind ) << "\",\n";
  out << "  \"state\": \"" << backup_operation_state_name( status.state ) << "\",\n";
  out << "  \"phase\": \"" << json_escape( status.phase ) << "\",\n";
  out << "  \"message\": \"" << json_escape( status.message ) << "\",\n";
  out << "  \"checkpoint_dir\": \"" << json_escape( status.checkpoint_dir.string() ) << "\",\n";
  out << "  \"started_at_ms\": " << status.started_at_ms << ",\n";
  out << "  \"finished_at_ms\": " << status.finished_at_ms << ",\n";
  out << "  \"cancel_requested\": " << ( status.cancel_requested ? "true" : "false" ) << ",\n";
  out << "  \"progress\": {\n";
  out << "    \"completed_batches\": " << status.completed_batches << ",\n";
  out << "    \"total_batches\": " << status.total_batches << ",\n";
  out << "    \"attempt\": " << status.attempt << ",\n";
  out << "    \"file_count\": " << status.progress_file_count << ",\n";
  out << "    \"completed_bytes\": " << status.progress_completed_bytes << ",\n";
  out << "    \"total_bytes\": " << status.progress_total_bytes << "\n";
  out << "  },\n";
  out << "  \"has_snapshot\": " << ( status.has_snapshot ? "true" : "false" ) << ",\n";
  out << "  \"has_remote_upload\": " << ( status.has_remote_upload ? "true" : "false" ) << ",\n";
  out << "  \"has_public_publish\": " << ( status.has_public_publish ? "true" : "false" ) << ",\n";
  out << "  \"has_restore_fetch\": " << ( status.has_restore_fetch ? "true" : "false" ) << ",\n";
  out << "  \"has_public_restore_fetch\": " << ( status.has_public_restore_fetch ? "true" : "false" ) << ",\n";
  out << "  \"has_restore_preflight\": " << ( status.has_restore_preflight ? "true" : "false" ) << ",\n";
  out << "  \"has_restore_stage\": " << ( status.has_restore_stage ? "true" : "false" ) << ",\n";
  out << "  \"has_activation_request\": " << ( status.has_activation_request ? "true" : "false" ) << ",\n";
  out << "  \"delete_result_count\": " << status.delete_results.size();
  if( status.has_snapshot )
  {
    out << ",\n";
    out << "  \"snapshot\": " << local_snapshot_result_to_json( status.snapshot );
  }
  if( status.has_remote_upload )
  {
    out << ",\n";
    out << "  \"remote_upload\": " << sftp_upload_result_to_json( status.remote_upload );
  }
  if( status.has_public_publish )
  {
    out << ",\n";
    out << "  \"public_publish\": " << public_bootstrap_publish_result_to_json( status.public_publish );
  }
  if( status.has_restore_fetch )
  {
    out << ",\n";
    out << "  \"restore_fetch\": " << sftp_restore_fetch_result_to_json( status.restore_fetch );
  }
  if( status.has_public_restore_fetch )
  {
    out << ",\n";
    out << "  \"public_restore_fetch\": " << public_restore_fetch_result_to_json( status.public_restore_fetch );
  }
  if( status.has_restore_preflight )
  {
    out << ",\n";
    out << "  \"restore_preflight\": " << restore_preflight_result_to_json( status.restore_preflight );
  }
  if( status.has_restore_stage )
  {
    out << ",\n";
    out << "  \"restore_stage\": " << restore_stage_result_to_json( status.restore_stage );
  }
  if( status.has_activation_request )
  {
    out << ",\n";
    out << "  \"activation_request\": " << restore_activation_request_to_json( status.activation_request );
  }
  if( !status.delete_results.empty() )
  {
    out << ",\n";
    out << "  \"delete_results\": [\n";
    for( std::size_t i = 0; i < status.delete_results.size(); ++i )
    {
      out << backup_delete_result_to_json( status.delete_results[ i ] );
      if( i + 1 != status.delete_results.size() )
        out << ",";
      out << "\n";
    }
    out << "  ]\n";
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
