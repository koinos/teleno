#include "backup/backup_plan.hpp"

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
  std::filesystem::create_directories( path );
  return path;
}

std::filesystem::path write_file( const std::filesystem::path& path,
                                  const std::string& content,
                                  std::filesystem::perms permissions )
{
  std::filesystem::create_directories( path.parent_path() );
  std::ofstream out( path );
  out << content;
  out.close();
  std::filesystem::permissions( path, permissions, std::filesystem::perm_options::replace );
  return path;
}

bool has_issue_containing( const BackupDryRunPlan& plan,
                           const std::string& severity,
                           const std::string& needle )
{
  for( const auto& issue: plan.issues )
  {
    if( issue.severity == severity && issue.message.find( needle ) != std::string::npos )
      return true;
  }
  return false;
}

NodeConfig valid_backup_config( const std::filesystem::path& root )
{
  NodeConfig cfg;
  cfg.backup.enabled = true;
  cfg.backup.node_id = "testnet-producer-1";
  cfg.backup.workspace = ( root / "work" ).string();
  cfg.backup.schedule.enabled = true;
  cfg.backup.schedule.interval = "6h";
  cfg.backup.schedule.jitter_seconds = 120;
  cfg.backup.schedule.minimum_head_progress = 2;
  cfg.backup.local.enabled = true;
  cfg.backup.local.directory = ( root / "local-repo" ).string();
  cfg.backup.local.retention_count = 3;
  cfg.backup.remote.enabled = true;
  cfg.backup.remote.directory = "/srv/teleno-backups";
  cfg.backup.remote.retention_count = 14;
  cfg.backup.remote.retention_days = 30;
  cfg.backup.ssh.enabled = true;
  cfg.backup.ssh.host = "10.0.0.2";
  cfg.backup.ssh.port = 22;
  cfg.backup.ssh.user = "teleno-backup";
  cfg.backup.ssh.auth = "password-file";
  cfg.backup.ssh.password_file = ( root / "secrets" / "ssh-password" ).string();
  cfg.backup.ssh.known_hosts_file = ( root / "known_hosts" ).string();
  return cfg;
}

} // namespace

int main()
{
  {
    auto root = unique_temp_dir( "teleno-backup-plan-disabled" );
    NodeConfig cfg;
    auto plan = build_backup_dry_run_plan( cfg, root / "basedir", root / "config.yml" );
    assert( !plan.enabled );
    assert( !plan.has_errors() );
    assert( has_issue_containing( plan, "warning", "backup.enabled is false" ) );
    std::filesystem::remove_all( root );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-plan-valid" );
    auto basedir = root / "basedir";
    std::filesystem::create_directories( basedir / "db" );
    std::filesystem::create_directories( root / "work" );
    std::filesystem::create_directories( root / "local-repo" );
    write_file( root / "secrets" / "ssh-password",
                "secret\n",
                std::filesystem::perms::owner_read | std::filesystem::perms::owner_write );
    write_file( root / "known_hosts",
                "example ssh-ed25519 AAAA\n",
                std::filesystem::perms::owner_read | std::filesystem::perms::group_read
                  | std::filesystem::perms::others_read );

    auto cfg = valid_backup_config( root );
    auto plan = build_backup_dry_run_plan( cfg, basedir, basedir / "config.yml" );

    assert( plan.enabled );
    assert( plan.node_id == "testnet-producer-1" );
    assert( plan.schedule_enabled );
    assert( plan.schedule_interval == "6h" );
    assert( plan.local_enabled );
    assert( plan.remote_enabled );
    assert( plan.ssh_enabled );
    assert( plan.source_db_exists );
    assert( plan.source_unified_chain );
    assert( !plan.has_errors() );
    assert( backup_dry_run_plan_to_text( plan ).find( "Teleno native backup dry run" ) != std::string::npos );
    assert( backup_dry_run_plan_to_json( plan ).find( "\"node_id\": \"testnet-producer-1\"" ) != std::string::npos );

    std::filesystem::remove_all( root );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-plan-invalid" );
    auto basedir = root / "basedir";
    std::filesystem::create_directories( basedir / "db" );
    std::filesystem::create_directories( root / "work" );

    auto cfg = valid_backup_config( root );
    cfg.backup.local.enabled = false;
    cfg.backup.ssh.password_file = ( root / "missing-password" ).string();

    auto plan = build_backup_dry_run_plan( cfg, basedir, basedir / "config.yml" );
    assert( plan.has_errors() );
    assert( has_issue_containing( plan, "error", "backup.ssh.password-file does not exist" ) );

    std::filesystem::remove_all( root );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-plan-open-secret" );
    auto basedir = root / "basedir";
    std::filesystem::create_directories( basedir / "db" );
    std::filesystem::create_directories( root / "work" );
    write_file( root / "secrets" / "ssh-password",
                "secret\n",
                std::filesystem::perms::owner_read | std::filesystem::perms::owner_write
                  | std::filesystem::perms::group_read );

    auto cfg = valid_backup_config( root );
    cfg.backup.local.enabled = false;
    cfg.backup.ssh.known_hosts_file.clear();

    auto plan = build_backup_dry_run_plan( cfg, basedir, basedir / "config.yml" );
    assert( plan.has_errors() );
    assert( has_issue_containing( plan, "error", "must not be readable" ) );

    std::filesystem::remove_all( root );
  }

  return 0;
}
