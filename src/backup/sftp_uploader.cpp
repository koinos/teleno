#include "backup/sftp_uploader.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

namespace koinos::node::backup {
namespace {

constexpr uint64_t repository_download_margin_bytes = 128ULL * 1024ULL * 1024ULL;

std::string read_file( const std::filesystem::path& path )
{
  std::ifstream input( path, std::ios::binary );
  if( !input )
    throw std::runtime_error( "failed to read file: " + path.string() );
  return std::string( ( std::istreambuf_iterator< char >( input ) ),
                      std::istreambuf_iterator< char >() );
}

std::string extract_json_string_field( const std::string& json, const std::string& key )
{
  const auto marker = "\"" + key + "\"";
  auto pos = json.find( marker );
  if( pos == std::string::npos )
    return {};
  pos = json.find( ':', pos + marker.size() );
  if( pos == std::string::npos )
    return {};
  pos = json.find( '"', pos + 1 );
  if( pos == std::string::npos )
    return {};
  auto end = pos + 1;
  std::string value;
  bool escaped = false;
  for( ; end < json.size(); ++end )
  {
    const auto ch = json[ end ];
    if( escaped )
    {
      value.push_back( ch );
      escaped = false;
      continue;
    }
    if( ch == '\\' )
    {
      escaped = true;
      continue;
    }
    if( ch == '"' )
      return value;
    value.push_back( ch );
  }
  return {};
}

std::string latest_backup_id( const std::filesystem::path& repository_dir )
{
  const auto latest_path = repository_dir / "latest.json";
  const auto latest = read_file( latest_path );
  auto backup_id = extract_json_string_field( latest, "backup_id" );
  if( backup_id.empty() )
    throw std::runtime_error( "latest.json does not contain backup_id: " + latest_path.string() );
  return backup_id;
}

std::string bytes_to_hex( const unsigned char* data, unsigned int size )
{
  std::ostringstream out;
  out << std::hex << std::setfill( '0' );
  for( unsigned int i = 0; i < size; ++i )
    out << std::setw( 2 ) << static_cast< unsigned int >( data[ i ] );
  return out.str();
}

std::string sha256_file( const std::filesystem::path& path )
{
  std::ifstream input( path, std::ios::binary );
  if( !input )
    throw std::runtime_error( "failed to open file for SHA-256: " + path.string() );

  EVP_MD_CTX* raw_ctx = EVP_MD_CTX_new();
  if( !raw_ctx )
    throw std::runtime_error( "failed to allocate SHA-256 context" );
  std::unique_ptr< EVP_MD_CTX, decltype( &EVP_MD_CTX_free ) > ctx( raw_ctx, EVP_MD_CTX_free );

  if( EVP_DigestInit_ex( ctx.get(), EVP_sha256(), nullptr ) != 1 )
    throw std::runtime_error( "failed to initialize SHA-256 context" );

  std::array< char, 1024 * 1024 > buffer{};
  while( input )
  {
    input.read( buffer.data(), static_cast< std::streamsize >( buffer.size() ) );
    const auto count = input.gcount();
    if( count > 0
        && EVP_DigestUpdate( ctx.get(), buffer.data(), static_cast< std::size_t >( count ) ) != 1 )
      throw std::runtime_error( "failed to update SHA-256 for file: " + path.string() );
  }

  unsigned char digest[ EVP_MAX_MD_SIZE ];
  unsigned int digest_size = 0;
  if( EVP_DigestFinal_ex( ctx.get(), digest, &digest_size ) != 1 )
    throw std::runtime_error( "failed to finalize SHA-256 for file: " + path.string() );

  return bytes_to_hex( digest, digest_size );
}

std::string sftp_quote( const std::string& value )
{
  std::string out = "\"";
  for( char ch: value )
  {
    if( ch == '"' || ch == '\\' )
      out.push_back( '\\' );
    out.push_back( ch );
  }
  out.push_back( '"' );
  return out;
}

std::string shell_quote( const std::string& value )
{
  std::string out = "'";
  for( char ch: value )
  {
    if( ch == '\'' )
      out += "'\\''";
    else
      out.push_back( ch );
  }
  out.push_back( '\'' );
  return out;
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

void validate_relative_path( const std::string& relative_path )
{
  if( relative_path.empty() )
    throw std::runtime_error( "backup metadata contains an empty relative path" );
  std::filesystem::path path( relative_path );
  if( path.is_absolute() )
    throw std::runtime_error( "backup metadata contains an absolute path: " + relative_path );
  for( const auto& part: path )
  {
    const auto value = part.string();
    if( value == "." || value == ".." )
      throw std::runtime_error( "backup metadata contains an unsafe path: " + relative_path );
  }
}

std::string remote_join( const std::string& lhs, const std::string& rhs )
{
  if( lhs.empty() )
    return rhs;
  if( lhs.back() == '/' )
    return lhs + rhs;
  return lhs + "/" + rhs;
}

void push_unique_command( std::vector< std::string >& commands, const std::string& command )
{
  if( std::find( commands.begin(), commands.end(), command ) == commands.end() )
    commands.push_back( command );
}

void add_remote_parent_mkdirs( std::vector< std::string >& commands,
                               const std::string& remote_relative_path )
{
  std::filesystem::path current;
  auto parent = std::filesystem::path( remote_relative_path ).parent_path();
  for( const auto& part: parent )
  {
    current /= part;
    if( current.empty() )
      continue;
    push_unique_command( commands, "-mkdir " + sftp_quote( current.generic_string() ) );
  }
}

void add_put( SftpUploadPlan& plan,
              const std::filesystem::path& local_file,
              const std::string& remote_relative_file )
{
  if( !std::filesystem::is_regular_file( local_file ) )
    throw std::runtime_error( "local upload source is not a regular file: " + local_file.string() );

  add_remote_parent_mkdirs( plan.batch_commands, remote_relative_file );
  plan.batch_commands.push_back( "put " + sftp_quote( local_file.string() ) + " "
                                  + sftp_quote( remote_relative_file ) );
  ++plan.file_count;
  plan.total_bytes += std::filesystem::file_size( local_file );
}

std::filesystem::path object_path( const std::filesystem::path& repository_dir, const std::string& sha256 )
{
  if( sha256.size() != 64 )
    throw std::runtime_error( "invalid SHA-256 length for object path" );
  return repository_dir / "objects" / "sha256" / sha256.substr( 0, 2 ) / sha256.substr( 2, 2 ) / sha256;
}

std::filesystem::path partial_object_path( const std::filesystem::path& final_path )
{
  auto partial = final_path;
  partial += ".partial";
  return partial;
}

std::filesystem::path write_batch_file( const std::vector< std::string >& commands, const std::string& prefix )
{
  const auto batch_file = std::filesystem::temp_directory_path()
                          / ( prefix + "-" + std::to_string( std::chrono::duration_cast< std::chrono::milliseconds >(
                                std::chrono::system_clock::now().time_since_epoch() ).count() ) + ".batch" );
  std::ofstream out( batch_file, std::ios::binary | std::ios::trunc );
  if( !out )
    throw std::runtime_error( "failed to write SFTP batch file: " + batch_file.string() );
  for( const auto& command: commands )
    out << command << "\n";
  return batch_file;
}

std::filesystem::path write_batch_file( const SftpUploadPlan& plan )
{
  return write_batch_file( plan.batch_commands, "teleno-sftp-upload" );
}

std::string build_sftp_command( const std::filesystem::path& batch_file,
                                const BackupSshConfig& ssh )
{
  if( ssh.auth != "private-key" )
    throw std::runtime_error( "OpenSSH SFTP validation backend currently requires backup.ssh.auth=private-key" );
  if( ssh.host.empty() || ssh.user.empty() )
    throw std::runtime_error( "backup.ssh.host and backup.ssh.user are required for SFTP operation" );
  if( ssh.private_key_file.empty() )
    throw std::runtime_error( "backup.ssh.private-key-file is required for SFTP operation" );

  std::ostringstream cmd;
  cmd << "/usr/bin/sftp"
      << " -q"
      << " -o BatchMode=yes"
      << " -o ConnectTimeout=" << ( ssh.connect_timeout_seconds ? ssh.connect_timeout_seconds : 15 )
      << " -o StrictHostKeyChecking=" << ( ssh.strict_host_key_checking ? "yes" : "accept-new" )
      << " -i " << shell_quote( ssh.private_key_file );
  if( !ssh.known_hosts_file.empty() )
    cmd << " -o UserKnownHostsFile=" << shell_quote( ssh.known_hosts_file );
  cmd << " -b " << shell_quote( batch_file.string() )
      << " " << shell_quote( ssh.user + "@" + ssh.host )
      << " 1>/dev/null";
  return cmd.str();
}

void run_sftp_batch( const std::vector< std::string >& commands,
                     const std::string& batch_prefix,
                     const BackupSshConfig& ssh,
                     uint64_t& batch_file_count )
{
  const auto batch_file = write_batch_file( commands, batch_prefix );
  const auto command = build_sftp_command( batch_file, ssh );
  const auto code = std::system( command.c_str() );
  const auto removed = std::filesystem::remove( batch_file );
  (void)removed;
  ++batch_file_count;
  if( code != 0 )
    throw std::runtime_error( "SFTP operation failed with command exit code " + std::to_string( code ) );
}

uint64_t available_space_bytes( std::filesystem::path path )
{
  std::error_code ec;
  while( !path.empty() && !std::filesystem::exists( path, ec ) )
  {
    ec.clear();
    path = path.parent_path();
  }
  if( path.empty() )
    path = std::filesystem::current_path();

  const auto info = std::filesystem::space( path, ec );
  if( ec )
    throw std::runtime_error( "failed to read available space at " + path.string() + ": " + ec.message() );
  return info.available;
}

void copy_file_atomic( const std::filesystem::path& source, const std::filesystem::path& destination )
{
  std::filesystem::create_directories( destination.parent_path() );
  auto partial = destination;
  partial += ".tmp";
  std::error_code ec;
  std::filesystem::remove( partial, ec );
  std::filesystem::copy_file( source, partial, std::filesystem::copy_options::overwrite_existing );
  std::filesystem::rename( partial, destination );
}

void fetch_remote_metadata( const std::filesystem::path& repository_dir,
                            const BackupSshConfig& ssh,
                            const BackupRemoteConfig& remote,
                            uint64_t& batch_file_count,
                            uint64_t& metadata_file_count,
                            std::string& backup_id_out )
{
  if( repository_dir.empty() )
    throw std::runtime_error( "local snapshot repository directory is required" );
  if( remote.directory.empty() )
    throw std::runtime_error( "remote backup directory is required" );

  const auto metadata_root = repository_dir / ".remote-restore-metadata"
                             / std::to_string( std::chrono::duration_cast< std::chrono::milliseconds >(
                               std::chrono::system_clock::now().time_since_epoch() ).count() );
  std::filesystem::create_directories( metadata_root );

  try
  {
    const auto latest_tmp = metadata_root / "latest.json";
    run_sftp_batch(
      {
        "cd " + sftp_quote( remote.directory ),
        "get " + sftp_quote( "latest.json" ) + " " + sftp_quote( latest_tmp.string() )
      },
      "teleno-sftp-restore-metadata",
      ssh,
      batch_file_count );
    ++metadata_file_count;

    const auto latest = nlohmann::json::parse( read_file( latest_tmp ) );
    const auto backup_id = latest.at( "backup_id" ).get< std::string >();
    const auto snapshot_dir_name = latest.value( "snapshot_dir", backup_id );
    const auto manifest_rel = latest.value( "manifest", "snapshots/" + snapshot_dir_name + "/manifest.json" );
    const auto files_rel = latest.value( "files", "snapshots/" + snapshot_dir_name + "/files.json" );
    validate_relative_path( snapshot_dir_name );
    validate_relative_path( manifest_rel );
    validate_relative_path( files_rel );

    const auto snapshot_tmp = metadata_root / "snapshot";
    std::filesystem::create_directories( snapshot_tmp );
    const auto manifest_tmp = snapshot_tmp / "manifest.json";
    const auto files_tmp = snapshot_tmp / "files.json";
    const auto complete_tmp = snapshot_tmp / "COMPLETE";

    run_sftp_batch(
      {
        "cd " + sftp_quote( remote.directory ),
        "get " + sftp_quote( manifest_rel ) + " " + sftp_quote( manifest_tmp.string() ),
        "get " + sftp_quote( files_rel ) + " " + sftp_quote( files_tmp.string() ),
        "get " + sftp_quote( remote_join( "snapshots/" + snapshot_dir_name, "COMPLETE" ) )
          + " " + sftp_quote( complete_tmp.string() )
      },
      "teleno-sftp-restore-metadata",
      ssh,
      batch_file_count );
    metadata_file_count += 3;

    const auto local_snapshot_dir = repository_dir / "snapshots" / snapshot_dir_name;
    const auto local_snapshot_partial = repository_dir / "snapshots" / ( snapshot_dir_name + ".partial" );
    if( !std::filesystem::exists( local_snapshot_dir ) )
    {
      if( std::filesystem::exists( local_snapshot_partial ) )
        throw std::runtime_error( "local partial snapshot metadata already exists: " + local_snapshot_partial.string() );
      std::filesystem::create_directories( local_snapshot_partial );
      copy_file_atomic( files_tmp, local_snapshot_partial / "files.json" );
      copy_file_atomic( manifest_tmp, local_snapshot_partial / "manifest.json" );
      copy_file_atomic( complete_tmp, local_snapshot_partial / "COMPLETE" );
      std::filesystem::rename( local_snapshot_partial, local_snapshot_dir );
    }
    else if( !std::filesystem::exists( local_snapshot_dir / "COMPLETE" ) )
    {
      throw std::runtime_error( "local snapshot metadata exists but is incomplete: " + local_snapshot_dir.string() );
    }

    copy_file_atomic( latest_tmp, repository_dir / "latest.json" );
    backup_id_out = backup_id;
  }
  catch( ... )
  {
    std::error_code ec;
    std::filesystem::remove_all( metadata_root, ec );
    throw;
  }

  std::error_code ec;
  std::filesystem::remove_all( metadata_root, ec );
}

} // namespace

SftpUploadPlan build_open_ssh_sftp_upload_plan( const std::filesystem::path& repository_dir,
                                                const std::string& remote_directory )
{
  if( repository_dir.empty() )
    throw std::runtime_error( "local snapshot repository directory is required" );
  if( remote_directory.empty() )
    throw std::runtime_error( "remote backup directory is required" );

  const auto backup_id = latest_backup_id( repository_dir );
  const auto snapshot_dir = repository_dir / "snapshots" / backup_id;
  if( !std::filesystem::exists( snapshot_dir / "COMPLETE" ) )
    throw std::runtime_error( "latest local snapshot is not complete: " + snapshot_dir.string() );

  SftpUploadPlan plan;
  plan.repository_dir = repository_dir;
  plan.backup_id = backup_id;
  plan.remote_directory = remote_directory;
  plan.batch_commands.push_back( "cd " + sftp_quote( remote_directory ) );

  const auto objects_root = repository_dir / "objects";
  if( std::filesystem::exists( objects_root ) )
  {
    for( const auto& entry: std::filesystem::recursive_directory_iterator( objects_root ) )
    {
      if( !entry.is_regular_file() )
        continue;
      auto relative = std::filesystem::relative( entry.path(), repository_dir );
      add_put( plan, entry.path(), relative.generic_string() );
    }
  }

  const auto remote_partial_snapshot = "snapshots/" + backup_id + ".partial";
  const auto remote_final_snapshot = "snapshots/" + backup_id;
  push_unique_command( plan.batch_commands, "-mkdir " + sftp_quote( "snapshots" ) );
  push_unique_command( plan.batch_commands, "-mkdir " + sftp_quote( remote_partial_snapshot ) );
  add_put( plan, snapshot_dir / "files.json", remote_join( remote_partial_snapshot, "files.json" ) );
  add_put( plan, snapshot_dir / "manifest.json", remote_join( remote_partial_snapshot, "manifest.json" ) );
  add_put( plan, snapshot_dir / "COMPLETE", remote_join( remote_partial_snapshot, "COMPLETE" ) );
  plan.batch_commands.push_back( "-rename " + sftp_quote( remote_partial_snapshot ) + " "
                                  + sftp_quote( remote_final_snapshot ) );

  add_put( plan, repository_dir / "latest.json", "latest.json.partial" );
  plan.batch_commands.push_back( "-rename " + sftp_quote( "latest.json.partial" ) + " "
                                  + sftp_quote( "latest.json" ) );

  return plan;
}

SftpRestoreObjectFetchPlan build_open_ssh_sftp_restore_object_fetch_plan(
  const std::filesystem::path& repository_dir,
  const std::string& remote_directory,
  const RestorePreflightResult& preflight )
{
  if( repository_dir.empty() )
    throw std::runtime_error( "local snapshot repository directory is required" );
  if( remote_directory.empty() )
    throw std::runtime_error( "remote backup directory is required" );
  if( preflight.files_path.empty() )
    throw std::runtime_error( "restore preflight files path is required" );

  SftpRestoreObjectFetchPlan plan;
  plan.repository_dir = repository_dir;
  plan.backup_id = preflight.backup_id;
  plan.remote_directory = remote_directory;
  plan.batch_commands.push_back( "cd " + sftp_quote( remote_directory ) );

  const auto files = nlohmann::json::parse( read_file( preflight.files_path ) );
  std::set< std::string > planned_hashes;
  for( const auto& file: files.at( "files" ) )
  {
    const auto sha256 = file.at( "sha256" ).get< std::string >();
    if( !planned_hashes.insert( sha256 ).second )
      continue;

    const auto local_object = object_path( repository_dir, sha256 );
    if( std::filesystem::exists( local_object ) )
      continue;

    const auto remote_relative = "objects/sha256/" + sha256.substr( 0, 2 ) + "/"
                               + sha256.substr( 2, 2 ) + "/" + sha256;
    const auto local_partial = partial_object_path( local_object );
    std::filesystem::create_directories( local_object.parent_path() );
    std::error_code ec;
    std::filesystem::remove( local_partial, ec );

    SftpRestoreObjectDownload download;
    download.sha256 = sha256;
    download.remote_relative_path = remote_relative;
    download.local_object_path = local_object;
    download.local_partial_path = local_partial;
    download.size_bytes = file.value( "size_bytes", 0ULL );

    plan.batch_commands.push_back( "get " + sftp_quote( remote_relative ) + " "
                                    + sftp_quote( local_partial.string() ) );
    plan.total_bytes += download.size_bytes;
    plan.downloads.push_back( std::move( download ) );
  }

  plan.object_count = static_cast< uint64_t >( plan.downloads.size() );
  return plan;
}

SftpUploadResult upload_latest_snapshot_with_open_ssh_sftp( const std::filesystem::path& repository_dir,
                                                            const BackupSshConfig& ssh,
                                                            const BackupRemoteConfig& remote )
{
  auto plan = build_open_ssh_sftp_upload_plan( repository_dir, remote.directory );
  auto batch_file = write_batch_file( plan );

  const auto command = build_sftp_command( batch_file, ssh );
  const auto code = std::system( command.c_str() );
  if( code != 0 )
    throw std::runtime_error( "SFTP upload failed with command exit code " + std::to_string( code ) );

  SftpUploadResult result;
  result.backup_id = plan.backup_id;
  result.repository_dir = plan.repository_dir;
  result.remote_directory = plan.remote_directory;
  result.batch_file = batch_file;
  result.batch_file_removed = std::filesystem::remove( batch_file );
  result.file_count = plan.file_count;
  result.total_bytes = plan.total_bytes;
  return result;
}

SftpRestoreFetchResult fetch_latest_restore_snapshot_with_open_ssh_sftp( const std::filesystem::path& repository_dir,
                                                                         const std::filesystem::path& target_basedir,
                                                                         const BackupSshConfig& ssh,
                                                                         const BackupRemoteConfig& remote )
{
  SftpRestoreFetchResult result;
  result.repository_dir = repository_dir;
  result.target_basedir = target_basedir;
  result.remote_directory = remote.directory;

  std::string metadata_backup_id;
  fetch_remote_metadata( repository_dir,
                         ssh,
                         remote,
                         result.batch_file_count,
                         result.metadata_file_count,
                         metadata_backup_id );
  result.metadata_fetched = true;
  result.backup_id = metadata_backup_id;
  result.preflight = build_local_restore_preflight( repository_dir, target_basedir );

  if( !result.preflight.space_check.passes_minimum )
  {
    result.download_skipped_reason = result.preflight.space_check.message;
    return result;
  }

  auto object_plan = build_open_ssh_sftp_restore_object_fetch_plan(
    repository_dir,
    remote.directory,
    result.preflight );
  result.object_file_count = object_plan.object_count;
  result.object_bytes = object_plan.total_bytes;

  result.repository_required_bytes = object_plan.total_bytes + repository_download_margin_bytes;
  result.repository_available_bytes = available_space_bytes( repository_dir );
  if( result.repository_available_bytes < result.repository_required_bytes )
  {
    std::ostringstream message;
    message << "Backup restore object download requires at least "
            << result.repository_required_bytes << " bytes free for local repository "
            << repository_dir.string() << "; available bytes: " << result.repository_available_bytes;
    result.download_skipped_reason = message.str();
    return result;
  }

  if( !object_plan.downloads.empty() )
  {
    run_sftp_batch( object_plan.batch_commands,
                    "teleno-sftp-restore-objects",
                    ssh,
                    result.batch_file_count );

    for( const auto& download: object_plan.downloads )
    {
      if( !std::filesystem::is_regular_file( download.local_partial_path ) )
        throw std::runtime_error( "downloaded backup object is missing: " + download.local_partial_path.string() );
      if( download.size_bytes != 0 && std::filesystem::file_size( download.local_partial_path ) != download.size_bytes )
        throw std::runtime_error( "downloaded backup object size mismatch: " + download.local_partial_path.string() );

      const auto actual_sha256 = sha256_file( download.local_partial_path );
      if( actual_sha256 != download.sha256 )
        throw std::runtime_error( "downloaded backup object checksum mismatch: " + download.local_partial_path.string() );
      std::filesystem::rename( download.local_partial_path, download.local_object_path );
    }
    result.objects_fetched = true;
  }
  else
  {
    result.objects_fetched = true;
  }

  result.preflight = build_local_restore_preflight( repository_dir, target_basedir );
  result.ready_to_stage = result.preflight.ready_to_restore;
  return result;
}

std::string sftp_upload_plan_to_text( const SftpUploadPlan& plan )
{
  std::ostringstream out;
  out << "SFTP upload plan\n";
  out << "backup_id: " << plan.backup_id << "\n";
  out << "repository_dir: " << plan.repository_dir.string() << "\n";
  out << "remote_directory: " << plan.remote_directory << "\n";
  out << "file_count: " << plan.file_count << "\n";
  out << "total_bytes: " << plan.total_bytes << "\n";
  out << "batch_commands: " << plan.batch_commands.size() << "\n";
  return out.str();
}

std::string sftp_upload_result_to_text( const SftpUploadResult& result )
{
  std::ostringstream out;
  out << "Uploaded backup snapshot with SFTP\n";
  out << "backup_id: " << result.backup_id << "\n";
  out << "repository_dir: " << result.repository_dir.string() << "\n";
  out << "remote_directory: " << result.remote_directory << "\n";
  out << "batch_file: " << result.batch_file.string()
      << ( result.batch_file_removed ? " (removed)" : "" ) << "\n";
  out << "file_count: " << result.file_count << "\n";
  out << "total_bytes: " << result.total_bytes << "\n";
  return out.str();
}

std::string sftp_upload_result_to_json( const SftpUploadResult& result )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"backup_id\": \"" << json_escape( result.backup_id ) << "\",\n";
  out << "  \"repository_dir\": \"" << json_escape( result.repository_dir.string() ) << "\",\n";
  out << "  \"remote_directory\": \"" << json_escape( result.remote_directory ) << "\",\n";
  out << "  \"batch_file\": \"" << json_escape( result.batch_file.string() ) << "\",\n";
  out << "  \"batch_file_removed\": " << ( result.batch_file_removed ? "true" : "false" ) << ",\n";
  out << "  \"file_count\": " << result.file_count << ",\n";
  out << "  \"total_bytes\": " << result.total_bytes << "\n";
  out << "}\n";
  return out.str();
}

std::string sftp_restore_fetch_result_to_text( const SftpRestoreFetchResult& result )
{
  std::ostringstream out;
  out << "Fetched remote backup restore data with SFTP\n";
  out << "backup_id: " << result.backup_id << "\n";
  out << "repository_dir: " << result.repository_dir.string() << "\n";
  out << "target_basedir: " << result.target_basedir.string() << "\n";
  out << "remote_directory: " << result.remote_directory << "\n";
  out << "metadata_fetched: " << ( result.metadata_fetched ? "true" : "false" ) << "\n";
  out << "metadata_file_count: " << result.metadata_file_count << "\n";
  out << "object_file_count: " << result.object_file_count << "\n";
  out << "object_bytes: " << result.object_bytes << "\n";
  out << "repository_available_bytes: " << result.repository_available_bytes << "\n";
  out << "repository_required_bytes: " << result.repository_required_bytes << "\n";
  out << "objects_fetched: " << ( result.objects_fetched ? "true" : "false" ) << "\n";
  out << "ready_to_stage: " << ( result.ready_to_stage ? "true" : "false" ) << "\n";
  out << "target_space_check: " << result.preflight.space_check.message << "\n";
  if( !result.download_skipped_reason.empty() )
    out << "download_skipped_reason: " << result.download_skipped_reason << "\n";
  return out.str();
}

std::string sftp_restore_fetch_result_to_json( const SftpRestoreFetchResult& result )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"backup_id\": \"" << json_escape( result.backup_id ) << "\",\n";
  out << "  \"repository_dir\": \"" << json_escape( result.repository_dir.string() ) << "\",\n";
  out << "  \"target_basedir\": \"" << json_escape( result.target_basedir.string() ) << "\",\n";
  out << "  \"remote_directory\": \"" << json_escape( result.remote_directory ) << "\",\n";
  out << "  \"metadata_fetched\": " << ( result.metadata_fetched ? "true" : "false" ) << ",\n";
  out << "  \"metadata_file_count\": " << result.metadata_file_count << ",\n";
  out << "  \"object_file_count\": " << result.object_file_count << ",\n";
  out << "  \"object_bytes\": " << result.object_bytes << ",\n";
  out << "  \"repository_available_bytes\": " << result.repository_available_bytes << ",\n";
  out << "  \"repository_required_bytes\": " << result.repository_required_bytes << ",\n";
  out << "  \"objects_fetched\": " << ( result.objects_fetched ? "true" : "false" ) << ",\n";
  out << "  \"ready_to_stage\": " << ( result.ready_to_stage ? "true" : "false" ) << ",\n";
  out << "  \"download_skipped_reason\": \"" << json_escape( result.download_skipped_reason ) << "\",\n";
  out << "  \"preflight\": {\n";
  out << "    \"snapshot_complete\": " << ( result.preflight.snapshot_complete ? "true" : "false" ) << ",\n";
  out << "    \"missing_object_count\": " << result.preflight.missing_object_count << ",\n";
  out << "    \"missing_object_bytes\": " << result.preflight.missing_object_bytes << ",\n";
  out << "    \"target_space_passes_minimum\": "
      << ( result.preflight.space_check.passes_minimum ? "true" : "false" ) << ",\n";
  out << "    \"target_available_bytes\": " << result.preflight.space_check.available_bytes << ",\n";
  out << "    \"target_minimum_free_bytes\": " << result.preflight.restore_space.minimum_target_free_bytes << ",\n";
  out << "    \"target_recommended_free_bytes\": " << result.preflight.restore_space.recommended_target_free_bytes << ",\n";
  out << "    \"message\": \"" << json_escape( result.preflight.space_check.message ) << "\"\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

} // namespace koinos::node::backup
