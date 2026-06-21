#include "backup/backup_scheduler.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

#include <koinos/log.hpp>

#include "backup/sftp_uploader.hpp"

namespace koinos::node::backup {
namespace {

uint64_t parse_positive_uint64( const std::string& value, const std::string& original )
{
  if( value.empty() )
    throw std::runtime_error( "backup schedule interval is missing a numeric value: " + original );

  uint64_t result = 0;
  for( char ch: value )
  {
    if( !std::isdigit( static_cast< unsigned char >( ch ) ) )
      throw std::runtime_error( "backup schedule interval has invalid numeric value: " + original );
    const auto digit = static_cast< uint64_t >( ch - '0' );
    if( result > ( UINT64_MAX - digit ) / 10 )
      throw std::runtime_error( "backup schedule interval is too large: " + original );
    result = result * 10 + digit;
  }

  if( result == 0 )
    throw std::runtime_error( "backup schedule interval must be greater than zero: " + original );
  return result;
}

bool has_suffix( const std::string& value, const std::string& suffix )
{
  return value.size() >= suffix.size()
         && value.compare( value.size() - suffix.size(), suffix.size(), suffix ) == 0;
}

} // anonymous namespace

std::chrono::milliseconds parse_backup_schedule_interval( const std::string& value )
{
  if( value.empty() )
    throw std::runtime_error( "backup.schedule.interval is required" );

  if( has_suffix( value, "ms" ) )
    return std::chrono::milliseconds( parse_positive_uint64( value.substr( 0, value.size() - 2 ), value ) );
  if( has_suffix( value, "s" ) )
    return std::chrono::seconds( parse_positive_uint64( value.substr( 0, value.size() - 1 ), value ) );
  if( has_suffix( value, "m" ) )
    return std::chrono::minutes( parse_positive_uint64( value.substr( 0, value.size() - 1 ), value ) );
  if( has_suffix( value, "h" ) )
    return std::chrono::hours( parse_positive_uint64( value.substr( 0, value.size() - 1 ), value ) );
  if( has_suffix( value, "d" ) )
    return std::chrono::hours( 24 * parse_positive_uint64( value.substr( 0, value.size() - 1 ), value ) );

  return std::chrono::seconds( parse_positive_uint64( value, value ) );
}

BackupScheduler::BackupScheduler( BackupService* backup_service,
                                  NodeConfig cfg,
                                  BackupHeadHeightProvider head_height_provider )
  : _backup_service( backup_service ),
    _cfg( std::move( cfg ) ),
    _head_height_provider( std::move( head_height_provider ) ),
    _interval( parse_backup_schedule_interval( _cfg.backup.schedule.interval ) ),
    _rng( std::random_device{}() )
{
  if( !_backup_service )
    throw std::runtime_error( "backup scheduler requires a backup service" );
  if( !_head_height_provider )
    throw std::runtime_error( "backup scheduler requires a head-height provider" );
  if( !_cfg.backup.enabled )
    throw std::runtime_error( "backup.enabled must be true when backup.schedule.enabled is true" );
  if( !_cfg.backup.local.enabled )
    throw std::runtime_error( "backup.local.enabled must be true for scheduled local backups" );
  if( _cfg.backup.local.directory.empty() )
    throw std::runtime_error( "backup.local.directory is required for scheduled local backups" );
  if( _cfg.backup.workspace.empty() )
    throw std::runtime_error( "backup.workspace is required for scheduled local backups" );
  if( _cfg.backup.remote.enabled )
  {
    if( !_cfg.backup.ssh.enabled )
      throw std::runtime_error( "backup.ssh.enabled must be true for scheduled remote backups" );
    if( _cfg.backup.remote.directory.empty() )
      throw std::runtime_error( "backup.remote.directory is required for scheduled remote backups" );
  }
  if( _cfg.backup.public_publish.enabled )
  {
    if( !_cfg.backup.remote.enabled )
      throw std::runtime_error( "backup.remote.enabled must be true for scheduled public bootstrap publishing" );
    if( _cfg.backup.public_publish.directory.empty() )
      throw std::runtime_error( "backup.public-publish.directory is required for scheduled public bootstrap publishing" );
    if( _cfg.backup.public_publish.base_url.empty() )
      throw std::runtime_error( "backup.public-publish.base-url is required for scheduled public bootstrap publishing" );
    if( _cfg.backup.public_publish.network.empty() )
      throw std::runtime_error( "backup.public-publish.network is required for scheduled public bootstrap publishing" );
    if( _cfg.backup.public_publish.observer_config_file.empty() )
      throw std::runtime_error( "backup.public-publish.observer-config-file is required for scheduled public bootstrap publishing" );
  }
}

BackupScheduler::~BackupScheduler()
{
  stop();
}

void BackupScheduler::start()
{
  std::lock_guard< std::mutex > lock( _mutex );
  if( _started )
    return;

  _stop_requested = false;
  _started = true;
  _thread = std::thread( [this]() { run_loop(); } );

  LOG( info ) << "[backup_scheduler] Started interval=" << _cfg.backup.schedule.interval
              << " jitter_seconds=" << _cfg.backup.schedule.jitter_seconds
              << " run_on_startup_if_missed="
              << ( _cfg.backup.schedule.run_on_startup_if_missed ? "true" : "false" );
}

void BackupScheduler::stop()
{
  std::thread thread;
  {
    std::lock_guard< std::mutex > lock( _mutex );
    if( !_started )
      return;
    _stop_requested = true;
    thread = std::move( _thread );
  }

  _cv.notify_all();
  _backup_service->cancel_current_operation();
  if( thread.joinable() )
    thread.join();

  {
    std::lock_guard< std::mutex > lock( _mutex );
    _started = false;
  }

  LOG( info ) << "[backup_scheduler] Stopped";
}

bool BackupScheduler::wait_for_stop_or_timeout( std::chrono::milliseconds timeout )
{
  std::unique_lock< std::mutex > lock( _mutex );
  return _cv.wait_for( lock, timeout, [this]() { return _stop_requested; } );
}

bool BackupScheduler::stop_requested() const
{
  std::lock_guard< std::mutex > lock( _mutex );
  return _stop_requested;
}

std::chrono::milliseconds BackupScheduler::jitter_delay()
{
  const auto jitter_seconds = _cfg.backup.schedule.jitter_seconds;
  if( jitter_seconds == 0 )
    return std::chrono::milliseconds( 0 );

  std::uniform_int_distribution< uint64_t > distribution( 0, jitter_seconds );
  return std::chrono::seconds( distribution( _rng ) );
}

bool BackupScheduler::should_run_at_height( uint64_t head_height )
{
  if( _cfg.backup.schedule.skip_if_syncing_from_genesis && head_height == 0 )
  {
    LOG( info ) << "[backup_scheduler] Skipping backup while local head is still at genesis";
    return false;
  }

  const auto minimum_progress = _cfg.backup.schedule.minimum_head_progress;
  if( minimum_progress == 0 )
    return true;

  const auto baseline = _last_successful_backup_height.value_or( 0 );
  if( head_height < baseline + minimum_progress )
  {
    LOG( info ) << "[backup_scheduler] Skipping backup: head_height=" << head_height
                << " baseline=" << baseline
                << " minimum_head_progress=" << minimum_progress;
    return false;
  }

  return true;
}

void BackupScheduler::run_once()
{
  const auto status = _backup_service->status();
  if( status.state == BackupOperationState::running )
  {
    LOG( info ) << "[backup_scheduler] Skipping backup because operation is already running: "
                << status.operation_id;
    return;
  }

  uint64_t head_height = 0;
  try
  {
    head_height = _head_height_provider();
  }
  catch( const std::exception& e )
  {
    LOG( warning ) << "[backup_scheduler] Failed to read head height: " << e.what();
    return;
  }

  if( !should_run_at_height( head_height ) )
    return;

  try
  {
    auto started = _backup_service->start_configured_backup_async( _cfg.backup.remote.enabled );
    LOG( info ) << "[backup_scheduler] Started scheduled backup operation_id="
                << started.operation_id << " head_height=" << head_height;

    _backup_service->wait_for_current_operation();
    const auto finished = _backup_service->status();
    if( finished.state == BackupOperationState::succeeded )
    {
      if( finished.has_remote_upload )
      {
        LOG( info ) << "[backup_scheduler] Scheduled remote backup uploaded"
                    << " backup_id=" << finished.remote_upload.backup_id
                    << " remote_directory=" << finished.remote_upload.remote_directory
                    << " file_count=" << finished.remote_upload.file_count
                    << " total_bytes=" << finished.remote_upload.total_bytes
                    << " retries=" << finished.remote_upload.retry_count;
      }
      if( finished.has_public_publish )
      {
        LOG( info ) << "[backup_scheduler] Scheduled public bootstrap published"
                    << " backup_id=" << finished.public_publish.backup_id
                    << " public_base_url=" << finished.public_publish.public_base_url
                    << " removed_public_snapshot_count="
                    << finished.public_publish.removed_public_snapshot_count;
      }
      if( !finished.delete_results.empty() )
      {
        LOG( info ) << "[backup_scheduler] Scheduled remote retention complete"
                    << " delete_result_count=" << finished.delete_results.size();
      }

      _last_successful_backup_height = head_height;
      LOG( info ) << "[backup_scheduler] Scheduled backup succeeded operation_id="
                  << finished.operation_id
                  << " backup_id=" << finished.snapshot.backup_id;
    }
    else
    {
      LOG( warning ) << "[backup_scheduler] Scheduled backup finished with state="
                     << backup_operation_state_name( finished.state )
                     << " message=" << finished.message;
    }
  }
  catch( const std::exception& e )
  {
    LOG( warning ) << "[backup_scheduler] Scheduled backup failed to start: " << e.what();
  }
}

void BackupScheduler::run_loop()
{
  auto next_delay = _cfg.backup.schedule.run_on_startup_if_missed
    ? jitter_delay()
    : _interval + jitter_delay();

  while( true )
  {
    if( wait_for_stop_or_timeout( next_delay ) )
      return;

    run_once();
    next_delay = _interval + jitter_delay();
  }
}

} // namespace koinos::node::backup
