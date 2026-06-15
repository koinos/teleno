#include "backup/sftp_uploader.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace koinos::node::backup;
using koinos::node::BackupRemoteConfig;
using koinos::node::BackupSshConfig;

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

bool has_command_containing( const SftpUploadPlan& plan, const std::string& needle )
{
  for( const auto& command: plan.batch_commands )
  {
    if( command.find( needle ) != std::string::npos )
      return true;
  }
  return false;
}

bool has_command_containing( const SftpRestoreObjectFetchPlan& plan, const std::string& needle )
{
  for( const auto& command: plan.batch_commands )
  {
    if( command.find( needle ) != std::string::npos )
      return true;
  }
  return false;
}

} // namespace

int main()
{
  {
    auto root = unique_temp_dir( "teleno-backup-sftp-plan" );
    auto repo = root / "repo";
    const std::string backup_id = "20260613T120000Z-ms-1-files-4";
    write_file( repo / "latest.json",
                "{\n"
                "  \"format\": \"teleno-native-latest-snapshot\",\n"
                "  \"backup_id\": \"" + backup_id + "\",\n"
                "  \"snapshot_dir\": \"" + backup_id + "\"\n"
                "}\n" );
    write_file( repo / "objects" / "sha256" / "ab" / "cd" / "abcd1234", "object-a" );
    write_file( repo / "objects" / ".DS_Store", "finder-root" );
    write_file( repo / "objects" / "sha256" / ".DS_Store", "finder-sha" );
    write_file( repo / "objects" / "sha256" / "ab" / ".DS_Store", "finder-prefix" );
    write_file( repo / "objects" / "sha256" / "ab" / "cd" / "._abcd1234", "finder-resource-fork" );
    write_file( repo / "snapshots" / backup_id / "files.json", "{\"files\":[]}\n" );
    write_file( repo / "snapshots" / backup_id / "manifest.json", "{\"manifest\":true}\n" );
    write_file( repo / "snapshots" / backup_id / "COMPLETE", "complete\n" );

    auto plan = build_sftp_upload_plan( repo, "/srv/teleno-backups/testnet/teleno-dev" );
    assert( plan.backup_id == backup_id );
    assert( plan.remote_directory == "/srv/teleno-backups/testnet/teleno-dev" );
    assert( plan.file_count == 5 );
    assert( plan.total_bytes > 0 );
    assert( !plan.batch_commands.empty() );
    assert( has_command_containing( plan, "mkdir \"/srv\"" ) );
    assert( has_command_containing( plan, "mkdir \"/srv/teleno-backups\"" ) );
    assert( has_command_containing( plan, "mkdir \"/srv/teleno-backups/testnet\"" ) );
    assert( has_command_containing( plan, "mkdir \"/srv/teleno-backups/testnet/teleno-dev\"" ) );
    assert( has_command_containing( plan, "cd \"/srv/teleno-backups/testnet/teleno-dev\"" ) );
    assert( has_command_containing( plan, "objects/sha256/ab/cd/abcd1234" ) );
    assert( !has_command_containing( plan, ".DS_Store" ) );
    assert( !has_command_containing( plan, "._abcd1234" ) );
    assert( has_command_containing( plan, "snapshots/" + backup_id + ".partial" ) );
    assert( has_command_containing( plan, "rename \"snapshots/" + backup_id + ".partial\"" ) );
    assert( has_command_containing( plan, "latest.json.partial" ) );
    assert( has_command_containing( plan, "-rm \"latest.json\"" ) );
    assert( has_command_containing( plan, "rename \"latest.json.partial\" \"latest.json\"" ) );
    assert( sftp_upload_plan_to_text( plan ).find( "SFTP upload plan" ) != std::string::npos );

    std::filesystem::remove_all( root );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-sftp-missing" );
    bool threw = false;
    try
    {
      (void)build_sftp_upload_plan( root / "repo", "/srv/teleno-backups/testnet/teleno-dev" );
    }
    catch( const std::runtime_error& )
    {
      threw = true;
    }
    assert( threw );
    std::filesystem::remove_all( root );
  }

  {
    SftpUploadResult result;
    result.backup_id = "backup\"with\\chars";
    result.repository_dir = "/tmp/repo\nwith-line";
    result.remote_directory = "/srv/teleno-backups/testnet/teleno-dev";
    result.transport = "native-libssh";
    result.batch_file = "/tmp/teleno.batch";
    result.batch_file_removed = true;
    result.batch_file_count = 1;
    result.retry_count = 2;
    result.file_count = 2;
    result.total_bytes = 42;

    const auto json = sftp_upload_result_to_json( result );
    assert( json.find( "backup\\\"with\\\\chars" ) != std::string::npos );
    assert( json.find( "/tmp/repo\\nwith-line" ) != std::string::npos );
    assert( json.find( "\"batch_file_removed\": true" ) != std::string::npos );
    assert( json.find( "\"transport\": \"native-libssh\"" ) != std::string::npos );
    assert( json.find( "\"retry_count\": 2" ) != std::string::npos );

    const auto text = sftp_upload_result_to_text( result );
    assert( text.find( "batch_file: /tmp/teleno.batch (removed)" ) != std::string::npos );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-sftp-managed-cancel" );
    auto repo = root / "repo";
    const std::string backup_id = "20260613T140000Z-ms-3-files-1";
    write_file( repo / "latest.json",
                "{ \"backup_id\": \"" + backup_id + "\", \"snapshot_dir\": \"" + backup_id + "\" }\n" );
    write_file( repo / "snapshots" / backup_id / "files.json", "{\"files\":[]}\n" );
    write_file( repo / "snapshots" / backup_id / "manifest.json", "{\"manifest\":true}\n" );
    write_file( repo / "snapshots" / backup_id / "COMPLETE", "complete\n" );

    BackupSshConfig ssh;
    ssh.enabled = true;
    ssh.transport = "native";
    ssh.host = "127.0.0.1";
    ssh.user = "backup";
    ssh.auth = "private-key";
    ssh.private_key_file = "/tmp/nonexistent-test-key";

    BackupRemoteConfig remote;
    remote.enabled = true;
    remote.directory = "/srv/teleno-backups/testnet/teleno-dev";

    SftpTransferOptions options;
    options.cancel_requested = []() { return true; };

    bool threw = false;
    try
    {
      (void)upload_latest_snapshot_with_managed_sftp( repo, ssh, remote, options );
    }
    catch( const std::runtime_error& e )
    {
      threw = std::string( e.what() ).find( "cancelled" ) != std::string::npos;
    }
    assert( threw );

    ssh.transport = "managed-openssh";
    options.cancel_requested = {};
    options.max_attempts = 1;
    threw = false;
    try
    {
      (void)upload_latest_snapshot_with_managed_sftp( repo, ssh, remote, options );
    }
    catch( const std::runtime_error& e )
    {
      threw = std::string( e.what() ).find( "unsupported backup.ssh.transport" ) != std::string::npos;
    }
    assert( threw );

    ssh.transport = "native";
    ssh.auth = "password-file";
    ssh.password_file = "/tmp/nonexistent-password";
    threw = false;
    try
    {
      (void)upload_latest_snapshot_with_managed_sftp( repo, ssh, remote, options );
    }
    catch( const std::runtime_error& e )
    {
      threw = std::string( e.what() ).find( "backup.ssh.password-file" ) != std::string::npos;
    }
    assert( threw );

    std::filesystem::remove_all( root );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-sftp-restore-fetch-plan" );
    auto repo = root / "repo";
    auto target = root / "target";
    const std::string backup_id = "20260613T130000Z-ms-2-files-3";
    const std::string existing_sha( 64, 'a' );
    const std::string missing_sha( 64, 'b' );

    write_file( repo / "latest.json",
                "{\n"
                "  \"format\": \"teleno-native-latest-snapshot\",\n"
                "  \"backup_id\": \"" + backup_id + "\",\n"
                "  \"snapshot_dir\": \"" + backup_id + "\",\n"
                "  \"manifest\": \"snapshots/" + backup_id + "/manifest.json\",\n"
                "  \"files\": \"snapshots/" + backup_id + "/files.json\"\n"
                "}\n" );
    write_file( repo / "snapshots" / backup_id / "manifest.json",
                "{\n"
                "  \"format\": \"teleno-native-rocksdb-snapshot\",\n"
                "  \"sizes\": {\n"
                "    \"restored_database_bytes\": 1,\n"
                "    \"runtime_files_bytes\": 1,\n"
                "    \"object_download_bytes\": 12,\n"
                "    \"archive_bytes\": 0\n"
                "  },\n"
                "  \"restore\": { \"start_as_observer_first\": true }\n"
                "}\n" );
    write_file( repo / "snapshots" / backup_id / "files.json",
                "{\n"
                "  \"files\": [\n"
                "    { \"path\": \"db/CURRENT\", \"sha256\": \"" + existing_sha + "\", \"size_bytes\": 1 },\n"
                "    { \"path\": \"db/000001.sst\", \"sha256\": \"" + missing_sha + "\", \"size_bytes\": 6 },\n"
                "    { \"path\": \"db/000002.sst\", \"sha256\": \"" + missing_sha + "\", \"size_bytes\": 6 }\n"
                "  ]\n"
                "}\n" );
    write_file( repo / "snapshots" / backup_id / "COMPLETE", "complete\n" );
    write_file( repo / "objects" / "sha256" / "aa" / "aa" / existing_sha, "x" );
    std::filesystem::create_directories( target );

    auto preflight = build_local_restore_preflight( repo, target );
    assert( preflight.missing_object_count == 2 );
    assert( !preflight.ready_to_restore );

    auto plan = build_sftp_restore_object_fetch_plan(
      repo,
      "/srv/teleno-backups/testnet/teleno-dev",
      preflight );
    assert( plan.backup_id == backup_id );
    assert( plan.object_count == 1 );
    assert( plan.total_bytes == 6 );
    assert( plan.downloads.size() == 1 );
    assert( has_command_containing( plan, "objects/sha256/bb/bb/" + missing_sha ) );
    assert( has_command_containing( plan, missing_sha + ".partial" ) );

    SftpRestoreFetchResult result;
    result.backup_id = backup_id;
    result.repository_dir = repo;
    result.target_basedir = target;
    result.remote_directory = "/srv/teleno-backups/testnet/teleno-dev";
    result.preflight = preflight;
    result.metadata_fetched = true;
    result.object_file_count = plan.object_count;
    result.object_bytes = plan.total_bytes;
    result.download_skipped_reason = "not enough target space";
    assert( sftp_restore_fetch_result_to_text( result ).find( "Fetched remote backup restore data" ) != std::string::npos );
    assert( sftp_restore_fetch_result_to_json( result ).find( "\"download_skipped_reason\": \"not enough target space\"" ) != std::string::npos );

    std::filesystem::remove_all( root );
  }

  return 0;
}
