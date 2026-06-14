#include "backup/backup_plan.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace koinos::node::backup {
namespace {

void add_issue( BackupDryRunPlan& plan, std::string severity, std::string message )
{
  plan.issues.push_back( BackupPlanIssue{ std::move( severity ), std::move( message ) } );
}

bool starts_with( const std::string& value, const std::string& prefix )
{
  return value.size() >= prefix.size()
         && std::equal( prefix.begin(), prefix.end(), value.begin() );
}

std::string json_escape( const std::string& value )
{
  std::ostringstream out;
  for( char ch: value )
  {
    switch( ch )
    {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\b':
        out << "\\b";
        break;
      case '\f':
        out << "\\f";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
      {
        const auto byte = static_cast< unsigned char >( ch );
        if( byte < 0x20 )
          out << "\\u00" << "0123456789abcdef"[ ( byte >> 4 ) & 0xf ]
              << "0123456789abcdef"[ byte & 0xf ];
        else
          out << ch;
      }
    }
  }
  return out.str();
}

std::string json_bool( bool value )
{
  return value ? "true" : "false";
}

void validate_directory_reference( BackupDryRunPlan& plan,
                                   const std::string& label,
                                   const std::string& directory )
{
  if( directory.empty() )
  {
    add_issue( plan, "error", label + " is required" );
    return;
  }

  std::error_code ec;
  if( !std::filesystem::exists( directory, ec ) )
  {
    add_issue( plan, "warning", label + " does not exist yet: " + directory );
    return;
  }

  if( !std::filesystem::is_directory( directory, ec ) )
    add_issue( plan, "error", label + " exists but is not a directory: " + directory );
}

void validate_regular_file( BackupDryRunPlan& plan,
                            const std::string& label,
                            const std::string& file_path )
{
  if( file_path.empty() )
  {
    add_issue( plan, "error", label + " is required" );
    return;
  }

  std::error_code ec;
  if( !std::filesystem::exists( file_path, ec ) )
  {
    add_issue( plan, "error", label + " does not exist: " + file_path );
    return;
  }

  if( !std::filesystem::is_regular_file( file_path, ec ) )
    add_issue( plan, "error", label + " is not a regular file: " + file_path );
}

void validate_secret_file( BackupDryRunPlan& plan,
                           const std::string& label,
                           const std::string& file_path )
{
  validate_regular_file( plan, label, file_path );
  if( file_path.empty() )
    return;

  std::error_code ec;
  if( !std::filesystem::exists( file_path, ec ) || !std::filesystem::is_regular_file( file_path, ec ) )
    return;
  const auto permissions = std::filesystem::status( file_path, ec ).permissions();
  if( ec )
  {
    add_issue( plan, "warning", label + " permissions could not be checked: " + file_path );
    return;
  }

  const auto open_to_group_or_other =
    ( permissions & std::filesystem::perms::group_read ) != std::filesystem::perms::none
    || ( permissions & std::filesystem::perms::group_write ) != std::filesystem::perms::none
    || ( permissions & std::filesystem::perms::group_exec ) != std::filesystem::perms::none
    || ( permissions & std::filesystem::perms::others_read ) != std::filesystem::perms::none
    || ( permissions & std::filesystem::perms::others_write ) != std::filesystem::perms::none
    || ( permissions & std::filesystem::perms::others_exec ) != std::filesystem::perms::none;

  if( open_to_group_or_other )
    add_issue( plan, "error", label + " must not be readable, writable, or executable by group/other: " + file_path );
}

void validate_ssh( BackupDryRunPlan& plan )
{
  if( !plan.ssh_enabled )
  {
    if( plan.remote_enabled )
      add_issue( plan, "error", "backup.remote.enabled requires backup.ssh.enabled" );
    return;
  }

  if( plan.ssh_host.empty() )
    add_issue( plan, "error", "backup.ssh.host is required when SSH backup is enabled" );
  if( !plan.ssh_transport.empty() && plan.ssh_transport != "native" && plan.ssh_transport != "libssh" )
    add_issue( plan, "error", "backup.ssh.transport must be native or libssh" );
  if( plan.ssh_user.empty() )
    add_issue( plan, "error", "backup.ssh.user is required when SSH backup is enabled" );
  if( plan.ssh_port == 0 || plan.ssh_port > 65535 )
    add_issue( plan, "error", "backup.ssh.port must be between 1 and 65535" );
  if( plan.ssh_connect_timeout_seconds == 0 )
    add_issue( plan, "error", "backup.ssh.connect-timeout-seconds must be greater than 0" );

  if( plan.ssh_auth == "password-file" )
    validate_secret_file( plan, "backup.ssh.password-file", plan.ssh_password_file );
  else if( plan.ssh_auth == "private-key" )
  {
    validate_secret_file( plan, "backup.ssh.private-key-file", plan.ssh_private_key_file );
    if( !plan.ssh_passphrase_file.empty() )
      validate_secret_file( plan, "backup.ssh.passphrase-file", plan.ssh_passphrase_file );
  }
  else if( plan.ssh_auth == "env-password" )
    add_issue( plan, "warning", "backup.ssh.auth=env-password is supported for automation but password-file is preferred" );
  else
    add_issue( plan, "error", "backup.ssh.auth must be password-file, private-key, or env-password" );

  if( plan.ssh_strict_host_key_checking && plan.ssh_known_hosts_file.empty() )
    add_issue( plan, "warning", "backup.ssh.known-hosts-file is empty while strict host checking is enabled" );
  if( plan.ssh_strict_host_key_checking && !plan.ssh_known_hosts_file.empty() )
    validate_regular_file( plan, "backup.ssh.known-hosts-file", plan.ssh_known_hosts_file );
}

} // anonymous namespace

bool BackupDryRunPlan::has_errors() const
{
  return std::any_of( issues.begin(), issues.end(), []( const auto& issue ) {
    return issue.severity == "error";
  } );
}

BackupDryRunPlan build_backup_dry_run_plan( const NodeConfig& cfg,
                                            const std::filesystem::path& basedir,
                                            const std::filesystem::path& config_path )
{
  BackupDryRunPlan plan;
  plan.enabled = cfg.backup.enabled;
  plan.node_id = cfg.backup.node_id;
  plan.basedir = basedir.string();
  plan.config_path = config_path.string();
  plan.workspace = cfg.backup.workspace;

  plan.schedule_enabled = cfg.backup.schedule.enabled;
  plan.schedule_interval = cfg.backup.schedule.interval;
  plan.schedule_run_on_startup_if_missed = cfg.backup.schedule.run_on_startup_if_missed;
  plan.schedule_jitter_seconds = cfg.backup.schedule.jitter_seconds;
  plan.schedule_minimum_head_progress = cfg.backup.schedule.minimum_head_progress;
  plan.schedule_skip_if_syncing_from_genesis = cfg.backup.schedule.skip_if_syncing_from_genesis;
  plan.schedule_max_concurrent_backups = cfg.backup.schedule.max_concurrent_backups;

  plan.local_enabled = cfg.backup.local.enabled;
  plan.local_directory = cfg.backup.local.directory;
  plan.local_retention_count = cfg.backup.local.retention_count;

  plan.remote_enabled = cfg.backup.remote.enabled;
  plan.remote_directory = cfg.backup.remote.directory;
  plan.remote_retention_count = cfg.backup.remote.retention_count;
  plan.remote_retention_days = cfg.backup.remote.retention_days;
  plan.remote_upload_temp_suffix = cfg.backup.remote.upload_temp_suffix;

  plan.ssh_enabled = cfg.backup.ssh.enabled;
  plan.ssh_transport = cfg.backup.ssh.transport;
  plan.ssh_host = cfg.backup.ssh.host;
  plan.ssh_port = cfg.backup.ssh.port;
  plan.ssh_user = cfg.backup.ssh.user;
  plan.ssh_auth = cfg.backup.ssh.auth;
  plan.ssh_password_file = cfg.backup.ssh.password_file;
  plan.ssh_private_key_file = cfg.backup.ssh.private_key_file;
  plan.ssh_passphrase_file = cfg.backup.ssh.passphrase_file;
  plan.ssh_known_hosts_file = cfg.backup.ssh.known_hosts_file;
  plan.ssh_strict_host_key_checking = cfg.backup.ssh.strict_host_key_checking;
  plan.ssh_connect_timeout_seconds = cfg.backup.ssh.connect_timeout_seconds;

  std::error_code ec;
  plan.source_db_exists = std::filesystem::exists( basedir / "db", ec );
  const auto chain_db_path = basedir / "chain" / "blockchain";
  plan.source_unified_chain = !std::filesystem::exists( chain_db_path, ec );

  if( !plan.enabled )
  {
    add_issue( plan, "warning", "backup.enabled is false; automatic and manual backup commands will be disabled" );
    return plan;
  }

  if( plan.node_id.empty() )
    add_issue( plan, "error", "backup.node-id is required when backup.enabled=true" );
  if( plan.workspace.empty() )
    add_issue( plan, "error", "backup.workspace is required when backup.enabled=true" );
  else
    validate_directory_reference( plan, "backup.workspace", plan.workspace );

  if( plan.schedule_enabled )
  {
    if( plan.schedule_interval.empty() )
      add_issue( plan, "error", "backup.schedule.interval is required when scheduled backups are enabled" );
    if( plan.schedule_max_concurrent_backups != 1 )
      add_issue( plan, "warning", "backup.schedule.max-concurrent-backups currently supports only 1" );
  }

  if( !plan.local_enabled && !plan.remote_enabled )
    add_issue( plan, "error", "at least one backup target must be enabled: backup.local.enabled or backup.remote.enabled" );

  if( plan.local_enabled )
  {
    validate_directory_reference( plan, "backup.local.directory", plan.local_directory );
    if( plan.local_retention_count == 0 )
      add_issue( plan, "warning", "backup.local.retention-count=0 will retain no completed local snapshots" );
  }

  if( plan.remote_enabled )
  {
    if( plan.remote_directory.empty() )
      add_issue( plan, "error", "backup.remote.directory is required when remote backup is enabled" );
    else if( !starts_with( plan.remote_directory, "/" ) )
      add_issue( plan, "error", "backup.remote.directory must be an absolute path on the remote server" );
    if( plan.remote_upload_temp_suffix.empty() )
      add_issue( plan, "error", "backup.remote.upload-temp-suffix must not be empty" );
    if( plan.remote_retention_count == 0 && plan.remote_retention_days == 0 )
      add_issue( plan, "warning", "remote retention is disabled; backups will accumulate until manually pruned" );
  }

  validate_ssh( plan );

  if( !plan.source_db_exists )
    add_issue( plan, "warning", "source DB does not exist yet: " + ( basedir / "db" ).string() );

  return plan;
}

std::string backup_dry_run_plan_to_text( const BackupDryRunPlan& plan )
{
  std::ostringstream out;
  out << "Teleno native backup dry run\n";
  out << "enabled: " << ( plan.enabled ? "true" : "false" ) << "\n";
  out << "node_id: " << plan.node_id << "\n";
  out << "basedir: " << plan.basedir << "\n";
  out << "config: " << plan.config_path << "\n";
  out << "workspace: " << plan.workspace << "\n";
  out << "storage_layout: " << ( plan.source_unified_chain ? "unified" : "legacy-two-db" ) << "\n";
  out << "source_db_exists: " << ( plan.source_db_exists ? "true" : "false" ) << "\n";
  out << "schedule: " << ( plan.schedule_enabled ? "enabled" : "disabled" )
      << " interval=" << plan.schedule_interval
      << " startup_if_missed=" << ( plan.schedule_run_on_startup_if_missed ? "true" : "false" )
      << " jitter_seconds=" << plan.schedule_jitter_seconds
      << " minimum_head_progress=" << plan.schedule_minimum_head_progress
      << "\n";
  out << "local: " << ( plan.local_enabled ? "enabled" : "disabled" )
      << " directory=" << plan.local_directory
      << " retention_count=" << plan.local_retention_count
      << "\n";
  out << "remote: " << ( plan.remote_enabled ? "enabled" : "disabled" )
      << " directory=" << plan.remote_directory
      << " retention_count=" << plan.remote_retention_count
      << " retention_days=" << plan.remote_retention_days
      << "\n";
  out << "ssh: " << ( plan.ssh_enabled ? "enabled" : "disabled" )
      << " transport=" << plan.ssh_transport
      << " host=" << plan.ssh_host
      << " port=" << plan.ssh_port
      << " user=" << plan.ssh_user
      << " auth=" << plan.ssh_auth
      << " strict_host_key_checking=" << ( plan.ssh_strict_host_key_checking ? "true" : "false" )
      << "\n";

  out << "validation:\n";
  if( plan.issues.empty() )
    out << "  [ok] backup configuration is ready for phase-1 planning\n";
  else
  {
    for( const auto& issue: plan.issues )
      out << "  [" << issue.severity << "] " << issue.message << "\n";
  }

  return out.str();
}

std::string backup_dry_run_plan_to_json( const BackupDryRunPlan& plan )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"enabled\": " << json_bool( plan.enabled ) << ",\n";
  out << "  \"node_id\": \"" << json_escape( plan.node_id ) << "\",\n";
  out << "  \"basedir\": \"" << json_escape( plan.basedir ) << "\",\n";
  out << "  \"config_path\": \"" << json_escape( plan.config_path ) << "\",\n";
  out << "  \"workspace\": \"" << json_escape( plan.workspace ) << "\",\n";
  out << "  \"storage_layout\": \"" << ( plan.source_unified_chain ? "unified" : "legacy-two-db" ) << "\",\n";
  out << "  \"source_db_exists\": " << json_bool( plan.source_db_exists ) << ",\n";
  out << "  \"schedule\": {\n";
  out << "    \"enabled\": " << json_bool( plan.schedule_enabled ) << ",\n";
  out << "    \"interval\": \"" << json_escape( plan.schedule_interval ) << "\",\n";
  out << "    \"run_on_startup_if_missed\": " << json_bool( plan.schedule_run_on_startup_if_missed ) << ",\n";
  out << "    \"jitter_seconds\": " << plan.schedule_jitter_seconds << ",\n";
  out << "    \"minimum_head_progress\": " << plan.schedule_minimum_head_progress << ",\n";
  out << "    \"skip_if_syncing_from_genesis\": " << json_bool( plan.schedule_skip_if_syncing_from_genesis ) << ",\n";
  out << "    \"max_concurrent_backups\": " << plan.schedule_max_concurrent_backups << "\n";
  out << "  },\n";
  out << "  \"local\": {\n";
  out << "    \"enabled\": " << json_bool( plan.local_enabled ) << ",\n";
  out << "    \"directory\": \"" << json_escape( plan.local_directory ) << "\",\n";
  out << "    \"retention_count\": " << plan.local_retention_count << "\n";
  out << "  },\n";
  out << "  \"remote\": {\n";
  out << "    \"enabled\": " << json_bool( plan.remote_enabled ) << ",\n";
  out << "    \"directory\": \"" << json_escape( plan.remote_directory ) << "\",\n";
  out << "    \"retention_count\": " << plan.remote_retention_count << ",\n";
  out << "    \"retention_days\": " << plan.remote_retention_days << ",\n";
  out << "    \"upload_temp_suffix\": \"" << json_escape( plan.remote_upload_temp_suffix ) << "\"\n";
  out << "  },\n";
  out << "  \"ssh\": {\n";
  out << "    \"enabled\": " << json_bool( plan.ssh_enabled ) << ",\n";
  out << "    \"transport\": \"" << json_escape( plan.ssh_transport ) << "\",\n";
  out << "    \"host\": \"" << json_escape( plan.ssh_host ) << "\",\n";
  out << "    \"port\": " << plan.ssh_port << ",\n";
  out << "    \"user\": \"" << json_escape( plan.ssh_user ) << "\",\n";
  out << "    \"auth\": \"" << json_escape( plan.ssh_auth ) << "\",\n";
  out << "    \"password_file\": \"" << json_escape( plan.ssh_password_file ) << "\",\n";
  out << "    \"private_key_file\": \"" << json_escape( plan.ssh_private_key_file ) << "\",\n";
  out << "    \"passphrase_file\": \"" << json_escape( plan.ssh_passphrase_file ) << "\",\n";
  out << "    \"known_hosts_file\": \"" << json_escape( plan.ssh_known_hosts_file ) << "\",\n";
  out << "    \"strict_host_key_checking\": " << json_bool( plan.ssh_strict_host_key_checking ) << ",\n";
  out << "    \"connect_timeout_seconds\": " << plan.ssh_connect_timeout_seconds << "\n";
  out << "  },\n";
  out << "  \"issues\": [\n";
  for( std::size_t i = 0; i < plan.issues.size(); ++i )
  {
    const auto& issue = plan.issues[ i ];
    out << "    { \"severity\": \"" << json_escape( issue.severity )
        << "\", \"message\": \"" << json_escape( issue.message ) << "\" }";
    if( i + 1 != plan.issues.size() )
      out << ",";
    out << "\n";
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

} // namespace koinos::node::backup
