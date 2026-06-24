#include "backup/sftp_uploader.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#include <utility>

#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <nlohmann/json.hpp>
#include <openssl/crypto.h>
#include <openssl/evp.h>

namespace koinos::node::backup {
namespace {

constexpr uint64_t repository_download_margin_bytes = 128ULL * 1024ULL * 1024ULL;
using SftpFileProgressCallback = std::function< void( uint64_t ) >;

std::string read_file( const std::filesystem::path& path );

std::string utc_timestamp()
{
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t( now );
  std::tm tm{};
#if defined( _WIN32 )
  gmtime_s( &tm, &time );
#else
  gmtime_r( &time, &tm );
#endif
  std::ostringstream out;
  out << std::put_time( &tm, "%Y%m%dT%H%M%SZ" );
  return out.str();
}

std::string read_secret_file( const std::filesystem::path& path )
{
  auto secret = read_file( path );
  while( !secret.empty() && ( secret.back() == '\n' || secret.back() == '\r' ) )
    secret.pop_back();
  return secret;
}

void burn_secret( std::string& secret )
{
  if( !secret.empty() )
    OPENSSL_cleanse( secret.data(), secret.size() );
  secret.clear();
}

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

std::string sha256_bytes( const std::string& bytes )
{
  EVP_MD_CTX* raw_ctx = EVP_MD_CTX_new();
  if( !raw_ctx )
    throw std::runtime_error( "failed to allocate SHA-256 context" );
  std::unique_ptr< EVP_MD_CTX, decltype( &EVP_MD_CTX_free ) > ctx( raw_ctx, EVP_MD_CTX_free );

  if( EVP_DigestInit_ex( ctx.get(), EVP_sha256(), nullptr ) != 1 )
    throw std::runtime_error( "failed to initialize SHA-256 context" );
  if( !bytes.empty()
      && EVP_DigestUpdate( ctx.get(), bytes.data(), bytes.size() ) != 1 )
    throw std::runtime_error( "failed to update SHA-256" );

  unsigned char digest[ EVP_MAX_MD_SIZE ];
  unsigned int digest_size = 0;
  if( EVP_DigestFinal_ex( ctx.get(), digest, &digest_size ) != 1 )
    throw std::runtime_error( "failed to finalize SHA-256" );
  return bytes_to_hex( digest, digest_size );
}

std::string json_bytes( const nlohmann::json& value )
{
  return value.dump( 2 ) + "\n";
}

std::string lower_ascii( std::string value )
{
  std::transform( value.begin(), value.end(), value.begin(), []( unsigned char ch ) {
    return static_cast< char >( std::tolower( ch ) );
  } );
  return value;
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

bool has_string_suffix( const std::string& value, const std::string& suffix )
{
  return value.size() >= suffix.size()
         && value.compare( value.size() - suffix.size(), suffix.size(), suffix ) == 0;
}

bool public_snapshot_path_allowed( const std::string& relative_path )
{
  if( relative_path == "config.yml"
      || relative_path == "chain/genesis_data.json"
      || relative_path == "jsonrpc/descriptors/koinos_descriptors.pb" )
    return true;
  return relative_path.rfind( "db/", 0 ) == 0;
}

void validate_public_snapshot_path( const std::string& relative_path )
{
  validate_relative_path( relative_path );
  const auto lower = lower_ascii( relative_path );
  if( lower == ".teleno-native-backups/admin.token"
      || lower == ".teleno-native-backups/teleno-native-backup-config.yml"
      || lower == "block_producer/private.key"
      || lower.find( "/.ssh/" ) != std::string::npos
      || lower.rfind( ".ssh/", 0 ) == 0
      || lower.find( "wallet" ) != std::string::npos
      || lower.find( "id_rsa" ) != std::string::npos
      || lower.find( "id_ed25519" ) != std::string::npos
      || lower.find( "private-key" ) != std::string::npos
      || lower.find( "private_key" ) != std::string::npos
      || lower.find( "password" ) != std::string::npos
      || lower.find( "passphrase" ) != std::string::npos
      || has_string_suffix( lower, ".token" )
      || has_string_suffix( lower, ".pem" )
      || has_string_suffix( lower, ".p12" )
      || has_string_suffix( lower, ".pfx" ) )
    throw std::runtime_error( "public bootstrap snapshot contains denied path: " + relative_path );
  if( !public_snapshot_path_allowed( relative_path ) )
    throw std::runtime_error( "public bootstrap snapshot contains non-allowlisted path: " + relative_path );
}

void validate_public_observer_config( const std::string& config )
{
  const auto lower = lower_ascii( config );
  if( lower.find( "block_producer: true" ) != std::string::npos )
    throw std::runtime_error( "public bootstrap observer config enables block_producer feature" );
  if( lower.find( "features:" ) == std::string::npos
      || lower.find( "block_producer: false" ) == std::string::npos )
    throw std::runtime_error( "public bootstrap observer config must explicitly disable block production" );
  if( lower.find( "verify-blocks: true" ) == std::string::npos )
    throw std::runtime_error( "public bootstrap observer config must enable chain.verify-blocks" );
  for( const auto& forbidden: { "private-key-file", "password-file", "passphrase-file", "admin.token" } )
  {
    if( lower.find( forbidden ) != std::string::npos )
      throw std::runtime_error( std::string( "public bootstrap observer config contains forbidden setting: " )
                                + forbidden );
  }
}

uint64_t optional_json_uint64( const nlohmann::json& first,
                               const nlohmann::json& second,
                               const std::initializer_list< std::string >& keys )
{
  for( const auto& key: keys )
  {
    if( first.is_object() && first.contains( key ) && first.at( key ).is_number_unsigned() )
      return first.at( key ).get< uint64_t >();
    if( second.is_object() && second.contains( key ) && second.at( key ).is_number_unsigned() )
      return second.at( key ).get< uint64_t >();
  }
  return 0;
}

std::string optional_json_string( const nlohmann::json& first,
                                  const nlohmann::json& second,
                                  const std::initializer_list< std::string >& keys )
{
  for( const auto& key: keys )
  {
    if( first.is_object() && first.contains( key ) && first.at( key ).is_string() )
      return first.at( key ).get< std::string >();
    if( second.is_object() && second.contains( key ) && second.at( key ).is_string() )
      return second.at( key ).get< std::string >();
  }
  return {};
}

void validate_backup_id_fragment( const std::string& backup_id )
{
  if( backup_id.empty() )
    throw std::runtime_error( "backup ID must not be empty" );
  if( backup_id == "latest" )
    return;
  if( backup_id == "." || backup_id == ".." )
    throw std::runtime_error( "unsafe backup ID: " + backup_id );
  if( backup_id.find( '/' ) != std::string::npos
      || backup_id.find( '\\' ) != std::string::npos )
    throw std::runtime_error( "backup ID must be a single snapshot directory name: " + backup_id );
  if( backup_id.size() >= 8 && backup_id.substr( backup_id.size() - 8 ) == ".partial" )
    throw std::runtime_error( "backup ID must not reference a partial snapshot: " + backup_id );
}

std::string remote_join( const std::string& lhs, const std::string& rhs )
{
  if( lhs.empty() )
    return rhs;
  if( lhs.back() == '/' )
    return lhs + rhs;
  return lhs + "/" + rhs;
}

std::string remote_parent_path( std::string path )
{
  while( path.size() > 1 && path.back() == '/' )
    path.pop_back();
  const auto slash = path.find_last_of( '/' );
  if( slash == std::string::npos )
    return {};
  if( slash == 0 )
    return "/";
  return path.substr( 0, slash );
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
    push_unique_command( commands, "mkdir " + sftp_quote( current.generic_string() ) );
  }
}

void add_remote_directory_mkdirs( std::vector< std::string >& commands,
                                  const std::string& remote_directory )
{
  std::filesystem::path current;
  const std::filesystem::path directory( remote_directory );
  for( const auto& part: directory )
  {
    const auto value = part.generic_string();
    if( value.empty() || value == "." )
      continue;
    if( value == "/" )
    {
      current = part;
      continue;
    }

    current /= part;
    const auto path = current.generic_string();
    if( path.empty() || path == "/" )
      continue;
    push_unique_command( commands, "mkdir " + sftp_quote( path ) );
  }
}

bool is_filesystem_metadata_artifact( const std::filesystem::path& path )
{
  const auto filename = path.filename().string();
  return filename == ".DS_Store"
         || filename == "Thumbs.db"
         || filename == "Desktop.ini"
         || filename.rfind( "._", 0 ) == 0;
}

bool is_remote_content_object_path( const std::string& path )
{
  return path.find( "/objects/sha256/" ) != std::string::npos
         || path.rfind( "objects/sha256/", 0 ) == 0;
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

bool transfer_cancel_requested( const SftpTransferOptions& options )
{
  return options.cancel_requested && options.cancel_requested();
}

void throw_if_transfer_cancelled( const SftpTransferOptions& options )
{
  if( transfer_cancel_requested( options ) )
    throw std::runtime_error( "SFTP operation cancelled" );
}

void emit_progress( const SftpTransferOptions& options,
                    std::string phase,
                    std::string backup_id,
                    uint64_t completed_batches,
                    uint64_t total_batches,
                    uint64_t attempt,
                    uint64_t file_count,
                    uint64_t total_bytes,
                    uint64_t completed_bytes = 0 )
{
  if( !options.progress )
    return;
  SftpTransferProgress progress;
  progress.phase = std::move( phase );
  progress.backup_id = std::move( backup_id );
  progress.completed_batches = completed_batches;
  progress.total_batches = total_batches;
  progress.attempt = attempt;
  progress.file_count = file_count;
  progress.completed_bytes = completed_bytes;
  progress.total_bytes = total_bytes;
  options.progress( progress );
}

struct ParsedSftpCommand
{
  bool ignore_errors = false;
  std::string op;
  std::vector< std::string > args;
};

struct RemoteDirectoryEntry
{
  std::string name;
  bool directory = false;
  bool regular_file = false;
};

struct RemoteSnapshotMetadata
{
  std::string backup_id;
  std::string snapshot_dir_name;
  bool latest = false;
  uint64_t metadata_file_count = 0;
  uint64_t metadata_bytes = 0;
  std::map< std::string, uint64_t > objects;
};

struct PublicBootstrapPublishPlan
{
  std::filesystem::path repository_dir;
  std::string backup_id;
  std::string public_directory;
  std::string public_base_url;
  std::string network;
  std::string sanitized_config_sha256;
  uint64_t sanitized_config_size = 0;
  uint64_t file_count = 0;
  uint64_t object_count = 0;
  uint64_t total_bytes = 0;
  nlohmann::json latest_json;
  nlohmann::json manifest_json;
  nlohmann::json files_json;
  nlohmann::json public_metadata_json;
  std::filesystem::path sanitized_config_tmp;
  std::filesystem::path latest_tmp;
  std::filesystem::path manifest_tmp;
  std::filesystem::path files_tmp;
  std::filesystem::path public_metadata_tmp;
  std::filesystem::path complete_tmp;
};

ParsedSftpCommand parse_sftp_command( const std::string& command )
{
  ParsedSftpCommand parsed;
  std::size_t pos = 0;
  while( pos < command.size() && command[ pos ] == ' ' )
    ++pos;
  if( pos < command.size() && command[ pos ] == '-' )
  {
    parsed.ignore_errors = true;
    ++pos;
  }
  while( pos < command.size() && command[ pos ] == ' ' )
    ++pos;

  const auto op_begin = pos;
  while( pos < command.size() && command[ pos ] != ' ' )
    ++pos;
  parsed.op = command.substr( op_begin, pos - op_begin );
  if( parsed.op.empty() )
    throw std::runtime_error( "empty SFTP batch command" );

  while( pos < command.size() )
  {
    while( pos < command.size() && command[ pos ] == ' ' )
      ++pos;
    if( pos >= command.size() )
      break;

    std::string arg;
    if( command[ pos ] == '"' )
    {
      ++pos;
      bool escaped = false;
      bool closed = false;
      for( ; pos < command.size(); ++pos )
      {
        const auto ch = command[ pos ];
        if( escaped )
        {
          arg.push_back( ch );
          escaped = false;
          continue;
        }
        if( ch == '\\' )
        {
          escaped = true;
          continue;
        }
        if( ch == '"' )
        {
          closed = true;
          ++pos;
          break;
        }
        arg.push_back( ch );
      }
      if( !closed )
        throw std::runtime_error( "unterminated quoted SFTP batch argument" );
    }
    else
    {
      const auto arg_begin = pos;
      while( pos < command.size() && command[ pos ] != ' ' )
        ++pos;
      arg = command.substr( arg_begin, pos - arg_begin );
    }
    parsed.args.push_back( std::move( arg ) );
  }

  return parsed;
}

std::string ssh_error_message( ssh_session session )
{
  const auto error = session ? ssh_get_error( session ) : nullptr;
  return error ? std::string( error ) : std::string( "unknown SSH error" );
}

std::string sftp_error_message( ssh_session session, sftp_session sftp )
{
  std::ostringstream out;
  out << "sftp_error=" << ( sftp ? sftp_get_error( sftp ) : SSH_ERROR )
      << " ssh_error=" << ssh_error_message( session );
  return out.str();
}

void throw_if_ssh_error( int rc, ssh_session session, const std::string& action )
{
  if( rc != SSH_OK )
    throw std::runtime_error( action + ": " + ssh_error_message( session ) );
}

void require_regular_file( const std::string& field, const std::filesystem::path& path )
{
  if( path.empty() )
    throw std::runtime_error( field + " is required for SFTP operation" );

  std::error_code ec;
  if( !std::filesystem::is_regular_file( path, ec ) )
  {
    auto message = field + " must point to a regular file: " + path.string();
    if( ec )
      message += " (" + ec.message() + ")";
    throw std::runtime_error( message );
  }

  std::ifstream input( path, std::ios::binary );
  if( !input )
    throw std::runtime_error( field + " must be readable: " + path.string() );
}

class NativeSftpClient
{
public:
  explicit NativeSftpClient( const BackupSshConfig& ssh )
    : _ssh( ssh )
  {
    connect();
  }

  ~NativeSftpClient()
  {
    if( _sftp )
      sftp_free( _sftp );
    if( _session )
    {
      ssh_disconnect( _session );
      ssh_free( _session );
    }
  }

  NativeSftpClient( const NativeSftpClient& ) = delete;
  NativeSftpClient& operator=( const NativeSftpClient& ) = delete;

  void execute( const ParsedSftpCommand& command )
  {
    try
    {
      if( command.op == "cd" )
      {
        require_arg_count( command, 1 );
        _cwd = command.args[ 0 ];
      }
      else if( command.op == "mkdir" )
      {
        require_arg_count( command, 1 );
        make_directory( remote_path( command.args[ 0 ] ) );
      }
      else if( command.op == "put" )
      {
        require_arg_count( command, 2 );
        upload_file( command.args[ 0 ], remote_path( command.args[ 1 ] ) );
      }
      else if( command.op == "get" )
      {
        require_arg_count( command, 2 );
        download_file( remote_path( command.args[ 0 ] ), command.args[ 1 ] );
      }
      else if( command.op == "rm" )
      {
        require_arg_count( command, 1 );
        remove_file( remote_path( command.args[ 0 ] ) );
      }
      else if( command.op == "rmdir" )
      {
        require_arg_count( command, 1 );
        remove_directory( remote_path( command.args[ 0 ] ), false );
      }
      else if( command.op == "rename" )
      {
        require_arg_count( command, 2 );
        rename_path( remote_path( command.args[ 0 ] ), remote_path( command.args[ 1 ] ) );
      }
      else
      {
        throw std::runtime_error( "unsupported native SFTP command: " + command.op );
      }
    }
    catch( const std::exception& )
    {
      if( command.ignore_errors )
        return;
      throw;
    }
  }

  std::vector< RemoteDirectoryEntry > list_directory( const std::string& path )
  {
    std::vector< RemoteDirectoryEntry > entries;
    const auto full_path = remote_path( path );
    auto dir = sftp_opendir( _sftp, full_path.c_str() );
    if( !dir )
      throw std::runtime_error( "failed to open remote SFTP directory " + full_path
                                + ": " + sftp_error_message( _session, _sftp ) );

    try
    {
      while( true )
      {
        auto attrs = sftp_readdir( _sftp, dir );
        if( !attrs )
          break;

        std::unique_ptr< sftp_attributes_struct, decltype( &sftp_attributes_free ) > guard(
          attrs,
          sftp_attributes_free );
        const std::string name = attrs->name ? attrs->name : "";
        if( name.empty() || name == "." || name == ".." )
          continue;

        RemoteDirectoryEntry entry;
        entry.name = name;
        entry.directory = attrs->type == SSH_FILEXFER_TYPE_DIRECTORY
                          || S_ISDIR( attrs->permissions );
        entry.regular_file = attrs->type == SSH_FILEXFER_TYPE_REGULAR
                             || S_ISREG( attrs->permissions );
        entries.push_back( std::move( entry ) );
      }

      if( sftp_dir_eof( dir ) == 0 )
        throw std::runtime_error( "failed while reading remote SFTP directory " + full_path
                                  + ": " + sftp_error_message( _session, _sftp ) );
    }
    catch( ... )
    {
      sftp_closedir( dir );
      throw;
    }

    if( sftp_closedir( dir ) != SSH_OK )
      throw std::runtime_error( "failed to close remote SFTP directory " + full_path
                                + ": " + sftp_error_message( _session, _sftp ) );

    std::sort( entries.begin(), entries.end(), []( const auto& lhs, const auto& rhs ) {
      return lhs.name < rhs.name;
    } );
    return entries;
  }

  bool exists( const std::string& path )
  {
    return remote_exists( remote_path( path ) );
  }

  uint64_t available_bytes( const std::string& path, std::string* checked_path = nullptr )
  {
    auto probe_path = remote_path( path );
    while( !probe_path.empty() && !remote_exists( probe_path ) )
    {
      const auto parent_path = remote_parent_path( probe_path );
      if( parent_path == probe_path )
        break;
      probe_path = parent_path;
    }
    if( probe_path.empty() )
      probe_path = "/";

    auto stats = sftp_statvfs( _sftp, probe_path.c_str() );
    if( !stats )
      throw std::runtime_error( "failed to read remote SFTP free space at " + probe_path
                                + ": " + sftp_error_message( _session, _sftp ) );

    std::unique_ptr< sftp_statvfs_struct, decltype( &sftp_statvfs_free ) > guard(
      stats,
      sftp_statvfs_free );
    const auto block_size = stats->f_frsize ? stats->f_frsize : stats->f_bsize;
    if( checked_path )
      *checked_path = probe_path;
    if( block_size == 0 || stats->f_bavail == 0 )
      return 0;
    if( stats->f_bavail > std::numeric_limits< uint64_t >::max() / block_size )
      return std::numeric_limits< uint64_t >::max();
    return stats->f_bavail * block_size;
  }

  void download( const std::string& remote_file,
                 const std::filesystem::path& local_file,
                 SftpFileProgressCallback progress = {} )
  {
    download_file( remote_path( remote_file ), local_file, progress );
  }

  void upload( const std::filesystem::path& local_file, const std::string& remote_file )
  {
    upload_file( local_file, remote_path( remote_file ) );
  }

  void rename( const std::string& source, const std::string& destination )
  {
    rename_path( remote_path( source ), remote_path( destination ) );
  }

  bool remove_file_if_exists( const std::string& path )
  {
    const auto full_path = remote_path( path );
    if( !remote_exists( full_path ) )
      return false;
    remove_file( full_path );
    return true;
  }

  bool remove_directory_if_exists( const std::string& path, bool ignore_not_empty )
  {
    const auto full_path = remote_path( path );
    if( !remote_exists( full_path ) )
      return false;
    return remove_directory( full_path, ignore_not_empty );
  }

private:
  void require_arg_count( const ParsedSftpCommand& command, std::size_t expected )
  {
    if( command.args.size() != expected )
      throw std::runtime_error( "SFTP command " + command.op + " expected "
                                + std::to_string( expected ) + " args, got "
                                + std::to_string( command.args.size() ) );
  }

  void set_option( enum ssh_options_e option, const void* value, const std::string& name )
  {
    throw_if_ssh_error( ssh_options_set( _session, option, value ), _session, "failed to set SSH option " + name );
  }

  void connect()
  {
    if( ssh_init() != SSH_OK )
      throw std::runtime_error( "failed to initialize libssh" );
    if( ! _ssh.transport.empty()
        && _ssh.transport != "native"
        && _ssh.transport != "libssh" )
      throw std::runtime_error( "unsupported backup.ssh.transport for native libssh SFTP backend: " + _ssh.transport );
    if( _ssh.host.empty() || _ssh.user.empty() )
      throw std::runtime_error( "backup.ssh.host and backup.ssh.user are required for SFTP operation" );
    if( _ssh.port == 0 || _ssh.port > 65535 )
      throw std::runtime_error( "backup.ssh.port must be between 1 and 65535" );
    validate_auth_config();

    _session = ssh_new();
    if( !_session )
      throw std::runtime_error( "failed to allocate libssh session" );

    const auto port = static_cast< unsigned int >( _ssh.port );
    const auto timeout = static_cast< long >( _ssh.connect_timeout_seconds ? _ssh.connect_timeout_seconds : 15 );
    const int process_config = 0;
    set_option( SSH_OPTIONS_HOST, _ssh.host.c_str(), "host" );
    set_option( SSH_OPTIONS_USER, _ssh.user.c_str(), "user" );
    set_option( SSH_OPTIONS_PORT, &port, "port" );
    set_option( SSH_OPTIONS_TIMEOUT, &timeout, "timeout" );
    set_option( SSH_OPTIONS_PROCESS_CONFIG, &process_config, "process-config" );
    if( !_ssh.known_hosts_file.empty() )
      set_option( SSH_OPTIONS_KNOWNHOSTS, _ssh.known_hosts_file.c_str(), "known-hosts" );
    if( _ssh.auth == "private-key" )
      set_option( SSH_OPTIONS_ADD_IDENTITY, _ssh.private_key_file.c_str(), "identity" );

    throw_if_ssh_error( ssh_connect( _session ), _session, "failed to connect SSH session" );
    verify_known_host();
    authenticate();

    _sftp = sftp_new( _session );
    if( !_sftp )
      throw std::runtime_error( "failed to create SFTP session: " + ssh_error_message( _session ) );
    if( sftp_init( _sftp ) != SSH_OK )
      throw std::runtime_error( "failed to initialize SFTP session: " + sftp_error_message( _session, _sftp ) );
  }

  void validate_auth_config()
  {
    if( _ssh.auth == "private-key" )
    {
      require_regular_file( "backup.ssh.private-key-file", _ssh.private_key_file );
      if( !_ssh.passphrase_file.empty() )
        require_regular_file( "backup.ssh.passphrase-file", _ssh.passphrase_file );
    }
    else if( _ssh.auth == "password-file" )
    {
      require_regular_file( "backup.ssh.password-file", _ssh.password_file );
    }
    else if( _ssh.auth == "env-password" )
    {
      const auto* env_password = std::getenv( "TELENO_BACKUP_SSH_PASSWORD" );
      if( !env_password || std::strlen( env_password ) == 0 )
        throw std::runtime_error( "backup.ssh.auth=env-password requires TELENO_BACKUP_SSH_PASSWORD" );
    }
    else
    {
      throw std::runtime_error( "backup.ssh.auth must be password-file, private-key, or env-password" );
    }
  }

  void verify_known_host()
  {
    const auto known = ssh_session_is_known_server( _session );
    switch( known )
    {
      case SSH_KNOWN_HOSTS_OK:
        return;
      case SSH_KNOWN_HOSTS_NOT_FOUND:
      case SSH_KNOWN_HOSTS_UNKNOWN:
        if( _ssh.strict_host_key_checking )
          throw std::runtime_error( "SSH host key is not trusted for " + _ssh.host );
        throw_if_ssh_error( ssh_session_update_known_hosts( _session ),
                            _session,
                            "failed to record new SSH host key" );
        return;
      case SSH_KNOWN_HOSTS_CHANGED:
        throw std::runtime_error( "SSH host key changed for " + _ssh.host );
      case SSH_KNOWN_HOSTS_OTHER:
        throw std::runtime_error( "SSH host key type differs for " + _ssh.host );
      case SSH_KNOWN_HOSTS_ERROR:
      default:
        throw std::runtime_error( "failed to verify SSH host key for " + _ssh.host
                                  + ": " + ssh_error_message( _session ) );
    }
  }

  void authenticate()
  {
    int rc = SSH_AUTH_ERROR;
    if( _ssh.auth == "private-key" )
    {
      std::string passphrase;
      if( !_ssh.passphrase_file.empty() )
        passphrase = read_secret_file( _ssh.passphrase_file );
      rc = ssh_userauth_publickey_auto( _session,
                                        nullptr,
                                        passphrase.empty() ? nullptr : passphrase.c_str() );
      burn_secret( passphrase );
    }
    else if( _ssh.auth == "password-file" )
    {
      if( _ssh.password_file.empty() )
        throw std::runtime_error( "backup.ssh.password-file is required for password-file SFTP operation" );
      auto password = read_secret_file( _ssh.password_file );
      rc = ssh_userauth_password( _session, nullptr, password.c_str() );
      burn_secret( password );
    }
    else if( _ssh.auth == "env-password" )
    {
      const auto* env_password = std::getenv( "TELENO_BACKUP_SSH_PASSWORD" );
      if( !env_password || std::strlen( env_password ) == 0 )
        throw std::runtime_error( "backup.ssh.auth=env-password requires TELENO_BACKUP_SSH_PASSWORD" );
      rc = ssh_userauth_password( _session, nullptr, env_password );
    }
    else
    {
      throw std::runtime_error( "backup.ssh.auth must be password-file, private-key, or env-password" );
    }

    if( rc != SSH_AUTH_SUCCESS )
      throw std::runtime_error( "SSH authentication failed for backup.ssh.auth="
                                + _ssh.auth + ": " + ssh_error_message( _session ) );
  }

  std::string remote_path( const std::string& path ) const
  {
    if( path.empty() || path.front() == '/' || _cwd.empty() )
      return path;
    if( _cwd == "/" )
      return "/" + path;
    if( _cwd.back() == '/' )
      return _cwd + path;
    return _cwd + "/" + path;
  }

  void make_directory( const std::string& path )
  {
    if( remote_exists( path ) )
      return;
    if( sftp_mkdir( _sftp, path.c_str(), 0755 ) != SSH_OK )
    {
      if( remote_exists( path ) )
        return;
      throw std::runtime_error( "failed to create remote directory " + path
                                + ": " + sftp_error_message( _session, _sftp ) );
    }
  }

  void rename_path( const std::string& source, const std::string& destination )
  {
    if( sftp_rename( _sftp, source.c_str(), destination.c_str() ) == SSH_OK )
      return;

    if( !remote_exists( source ) && remote_exists( destination ) )
      return;

    throw std::runtime_error( "failed to rename remote path " + source + " to " + destination
                              + ": " + sftp_error_message( _session, _sftp ) );
  }

  void remove_file( const std::string& path )
  {
    if( sftp_unlink( _sftp, path.c_str() ) != SSH_OK )
      throw std::runtime_error( "failed to remove remote file " + path
                                + ": " + sftp_error_message( _session, _sftp ) );
  }

  bool remove_directory( const std::string& path, bool ignore_not_empty )
  {
    if( sftp_rmdir( _sftp, path.c_str() ) == SSH_OK )
      return true;

    const auto error = sftp_get_error( _sftp );
    if( error == SSH_FX_NO_SUCH_FILE )
      return false;
    if( ignore_not_empty )
      return false;

    throw std::runtime_error( "failed to remove remote directory " + path
                              + ": " + sftp_error_message( _session, _sftp ) );
  }

  bool remote_exists( const std::string& path )
  {
    auto attributes = sftp_lstat( _sftp, path.c_str() );
    if( attributes )
    {
      sftp_attributes_free( attributes );
      return true;
    }

    const auto error = sftp_get_error( _sftp );
    if( error == SSH_FX_NO_SUCH_FILE )
      return false;
    throw std::runtime_error( "failed to stat remote path " + path + ": "
                              + sftp_error_message( _session, _sftp ) );
  }

  std::optional< uint64_t > remote_regular_file_size( const std::string& path )
  {
    auto attributes = sftp_lstat( _sftp, path.c_str() );
    if( attributes )
    {
      const auto is_regular = attributes->type == SSH_FILEXFER_TYPE_REGULAR
                              || S_ISREG( attributes->permissions );
      const auto size = static_cast< uint64_t >( attributes->size );
      sftp_attributes_free( attributes );
      return is_regular ? std::optional< uint64_t >( size ) : std::nullopt;
    }

    const auto error = sftp_get_error( _sftp );
    if( error == SSH_FX_NO_SUCH_FILE )
      return std::nullopt;
    throw std::runtime_error( "failed to stat remote path " + path + ": "
                              + sftp_error_message( _session, _sftp ) );
  }

  void upload_file( const std::filesystem::path& local_file, const std::string& remote_file )
  {
    std::ifstream input( local_file, std::ios::binary );
    if( !input )
      throw std::runtime_error( "failed to open local SFTP upload source: " + local_file.string() );

    if( is_remote_content_object_path( remote_file ) )
    {
      std::error_code ec;
      const auto local_size = std::filesystem::file_size( local_file, ec );
      if( !ec )
      {
        const auto remote_size = remote_regular_file_size( remote_file );
        if( remote_size && *remote_size == local_size )
          return;
      }
    }

    auto remote = sftp_open( _sftp,
                             remote_file.c_str(),
                             O_WRONLY | O_CREAT | O_TRUNC,
                             S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
    if( !remote )
      throw std::runtime_error( "failed to open remote SFTP upload target " + remote_file
                                + ": " + sftp_error_message( _session, _sftp ) );

    std::array< char, 128 * 1024 > buffer{};
    try
    {
      while( input )
      {
        input.read( buffer.data(), static_cast< std::streamsize >( buffer.size() ) );
        auto remaining = input.gcount();
        const char* cursor = buffer.data();
        while( remaining > 0 )
        {
          const auto written = sftp_write( remote, cursor, static_cast< size_t >( remaining ) );
          if( written <= 0 )
            throw std::runtime_error( "failed to write remote SFTP file " + remote_file
                                      + ": " + sftp_error_message( _session, _sftp ) );
          cursor += written;
          remaining -= written;
        }
      }
    }
    catch( ... )
    {
      sftp_close( remote );
      throw;
    }

    if( sftp_close( remote ) != SSH_OK )
      throw std::runtime_error( "failed to close remote SFTP upload target " + remote_file
                                + ": " + sftp_error_message( _session, _sftp ) );
  }

  void download_file( const std::string& remote_file,
                      const std::filesystem::path& local_file,
                      const SftpFileProgressCallback& progress = {} )
  {
    std::filesystem::create_directories( local_file.parent_path() );
    std::ofstream output( local_file, std::ios::binary | std::ios::trunc );
    if( !output )
      throw std::runtime_error( "failed to open local SFTP download target: " + local_file.string() );

    auto remote = sftp_open( _sftp, remote_file.c_str(), O_RDONLY, 0 );
    if( !remote )
      throw std::runtime_error( "failed to open remote SFTP download source " + remote_file
                                + ": " + sftp_error_message( _session, _sftp ) );

    std::array< char, 128 * 1024 > buffer{};
    uint64_t downloaded_bytes = 0;
    try
    {
      while( true )
      {
        const auto read_count = sftp_read( remote, buffer.data(), buffer.size() );
        if( read_count < 0 )
          throw std::runtime_error( "failed to read remote SFTP file " + remote_file
                                    + ": " + sftp_error_message( _session, _sftp ) );
        if( read_count == 0 )
          break;
        output.write( buffer.data(), static_cast< std::streamsize >( read_count ) );
        if( !output )
          throw std::runtime_error( "failed to write local SFTP download target: " + local_file.string() );
        downloaded_bytes += static_cast< uint64_t >( read_count );
        if( progress )
          progress( downloaded_bytes );
      }
    }
    catch( ... )
    {
      sftp_close( remote );
      throw;
    }

    if( sftp_close( remote ) != SSH_OK )
      throw std::runtime_error( "failed to close remote SFTP download source " + remote_file
                                + ": " + sftp_error_message( _session, _sftp ) );
  }

  BackupSshConfig _ssh;
  ssh_session _session = nullptr;
  sftp_session _sftp = nullptr;
  std::string _cwd;
};

void execute_native_sftp_commands( const std::vector< std::string >& commands,
                                   const BackupSshConfig& ssh,
                                   const SftpTransferOptions& options,
                                   const std::string& phase,
                                   const std::string& backup_id,
                                   uint64_t attempt,
                                   uint64_t file_count,
                                   uint64_t total_bytes )
{
  NativeSftpClient client( ssh );
  uint64_t completed_commands = 0;
  for( const auto& command: commands )
  {
    throw_if_transfer_cancelled( options );
    client.execute( parse_sftp_command( command ) );
    ++completed_commands;
    emit_progress( options,
                   phase,
                   backup_id,
                   completed_commands,
                   commands.size(),
                   attempt,
                   file_count,
                   total_bytes );
  }
}

void run_native_sftp_commands_with_retries( const std::vector< std::string >& commands,
                                            const BackupSshConfig& ssh,
                                            const SftpTransferOptions& options,
                                            const std::string& phase,
                                            const std::string& backup_id,
                                            uint64_t completed_batches,
                                            uint64_t total_batches,
                                            uint64_t file_count,
                                            uint64_t total_bytes,
                                            uint64_t& retry_count )
{
  const auto max_attempts = std::max< uint64_t >( 1, options.max_attempts );
  for( uint64_t attempt = 1; attempt <= max_attempts; ++attempt )
  {
    throw_if_transfer_cancelled( options );
    emit_progress( options,
                   phase,
                   backup_id,
                   completed_batches,
                   total_batches,
                   attempt,
                   file_count,
                   total_bytes );

    try
    {
      execute_native_sftp_commands( commands,
                                    ssh,
                                    options,
                                    phase,
                                    backup_id,
                                    attempt,
                                    file_count,
                                    total_bytes );
      return;
    }
    catch( const std::exception& )
    {
      if( attempt == max_attempts )
        throw;
    }

    ++retry_count;
    const auto retry_delay = std::chrono::seconds( options.retry_delay_seconds );
    const auto deadline = std::chrono::steady_clock::now() + retry_delay;
    while( std::chrono::steady_clock::now() < deadline )
    {
      throw_if_transfer_cancelled( options );
      std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
  }
}

void run_sftp_batch( const std::vector< std::string >& commands,
                     const std::string& batch_prefix,
                     const BackupSshConfig& ssh,
                     const SftpTransferOptions& options,
                     const std::string& phase,
                     const std::string& backup_id,
                     uint64_t total_batches,
                     uint64_t file_count,
                     uint64_t total_bytes,
                     uint64_t& batch_file_count,
                     uint64_t& retry_count )
{
  throw_if_transfer_cancelled( options );
  const auto batch_file = write_batch_file( commands, batch_prefix );
  try
  {
    run_native_sftp_commands_with_retries( commands,
                                           ssh,
                                           options,
                                           phase,
                                           backup_id,
                                           batch_file_count,
                                           total_batches,
                                           file_count,
                                           total_bytes,
                                           retry_count );
    ++batch_file_count;
  }
  catch( ... )
  {
    std::error_code ec;
    std::filesystem::remove( batch_file, ec );
    throw;
  }
  const auto removed = std::filesystem::remove( batch_file );
  (void)removed;
}

void fetch_sftp_restore_objects_with_progress( const SftpRestoreObjectFetchPlan& plan,
                                               const BackupSshConfig& ssh,
                                               const SftpTransferOptions& options,
                                               uint64_t& batch_file_count,
                                               uint64_t& retry_count )
{
  throw_if_transfer_cancelled( options );
  const auto batch_file = write_batch_file( plan.batch_commands, "teleno-sftp-restore-objects" );
  const auto max_attempts = std::max< uint64_t >( 1, options.max_attempts );
  try
  {
    for( uint64_t attempt = 1; attempt <= max_attempts; ++attempt )
    {
      try
      {
        NativeSftpClient client( ssh );
        client.execute( parse_sftp_command( "cd " + sftp_quote( plan.remote_directory ) ) );

        uint64_t completed_objects = 0;
        uint64_t completed_bytes = 0;
        emit_progress( options,
                       "restore-objects",
                       plan.backup_id,
                       completed_objects,
                       plan.object_count,
                       attempt,
                       plan.object_count,
                       plan.total_bytes,
                       completed_bytes );

        for( const auto& download: plan.downloads )
        {
          throw_if_transfer_cancelled( options );
          auto last_emit = std::chrono::steady_clock::now() - std::chrono::seconds( 1 );
          client.download(
            download.remote_relative_path,
            download.local_partial_path,
            [&]( uint64_t current_object_bytes ) {
              const auto now = std::chrono::steady_clock::now();
              if( now - last_emit < std::chrono::milliseconds( 500 ) )
                return;
              last_emit = now;
              emit_progress( options,
                             "restore-objects",
                             plan.backup_id,
                             completed_objects,
                             plan.object_count,
                             attempt,
                             plan.object_count,
                             plan.total_bytes,
                             std::min( plan.total_bytes, completed_bytes + current_object_bytes ) );
            } );

          const auto object_bytes = download.size_bytes != 0
            ? download.size_bytes
            : std::filesystem::file_size( download.local_partial_path );
          completed_bytes += object_bytes;
          ++completed_objects;
          emit_progress( options,
                         "restore-objects",
                         plan.backup_id,
                         completed_objects,
                         plan.object_count,
                         attempt,
                         plan.object_count,
                         plan.total_bytes,
                         std::min( plan.total_bytes, completed_bytes ) );
        }

        ++batch_file_count;
        std::error_code remove_ec;
        std::filesystem::remove( batch_file, remove_ec );
        return;
      }
      catch( const std::exception& )
      {
        if( attempt == max_attempts )
          throw;
      }

      ++retry_count;
      const auto retry_delay = std::chrono::seconds( options.retry_delay_seconds );
      const auto deadline = std::chrono::steady_clock::now() + retry_delay;
      while( std::chrono::steady_clock::now() < deadline )
      {
        throw_if_transfer_cancelled( options );
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
      }
    }
  }
  catch( ... )
  {
    std::error_code ec;
    std::filesystem::remove( batch_file, ec );
    throw;
  }
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

std::map< std::string, uint64_t > read_snapshot_object_sizes_from_file( const std::filesystem::path& files_path )
{
  std::map< std::string, uint64_t > objects;
  const auto files = nlohmann::json::parse( read_file( files_path ) );
  for( const auto& file: files.at( "files" ) )
  {
    const auto sha256 = file.at( "sha256" ).get< std::string >();
    if( sha256.size() != 64 )
      throw std::runtime_error( "invalid SHA-256 in remote backup files manifest: " + sha256 );
    objects.emplace( sha256, file.value( "size_bytes", 0ULL ) );
  }
  return objects;
}

std::string latest_json_for_remote_backup_id( const std::string& backup_id )
{
  validate_backup_id_fragment( backup_id );
  std::ostringstream out;
  out << "{\n";
  out << "  \"format\": \"teleno-native-latest-snapshot\",\n";
  out << "  \"version\": 1,\n";
  out << "  \"backup_id\": \"" << json_escape( backup_id ) << "\",\n";
  out << "  \"snapshot_dir\": \"" << json_escape( backup_id ) << "\",\n";
  out << "  \"manifest\": \"snapshots/" << json_escape( backup_id ) << "/manifest.json\",\n";
  out << "  \"files\": \"snapshots/" << json_escape( backup_id ) << "/files.json\"\n";
  out << "}\n";
  return out.str();
}

std::filesystem::path write_temp_file( const std::string& prefix,
                                       const std::string& content )
{
  const auto file = std::filesystem::temp_directory_path()
                    / ( prefix + "-" + std::to_string( std::chrono::duration_cast< std::chrono::milliseconds >(
                          std::chrono::system_clock::now().time_since_epoch() ).count() )
                        + "-" + std::to_string( std::rand() ) );
  std::ofstream out( file, std::ios::binary | std::ios::trunc );
  if( !out )
    throw std::runtime_error( "failed to write temporary file: " + file.string() );
  out << content;
  return file;
}

nlohmann::json public_latest_json_for_backup_id( const std::string& backup_id )
{
  return {
    { "format", "teleno-native-latest-snapshot" },
    { "version", 1 },
    { "backup_id", backup_id },
    { "snapshot_dir", backup_id },
    { "manifest", "snapshots/" + backup_id + "/manifest.json" },
    { "files", "snapshots/" + backup_id + "/files.json" },
    { "public_metadata", "snapshots/" + backup_id + "/public-bootstrap.json" },
  };
}

PublicBootstrapPublishPlan build_public_bootstrap_publish_plan(
  const std::filesystem::path& repository_dir,
  const BackupPublicPublishConfig& public_publish )
{
  if( repository_dir.empty() )
    throw std::runtime_error( "public bootstrap publish requires backup.local.directory" );
  if( !public_publish.enabled )
    throw std::runtime_error( "public bootstrap publish requires backup.public-publish.enabled=true" );
  if( public_publish.directory.empty() )
    throw std::runtime_error( "public bootstrap publish requires backup.public-publish.directory" );
  if( public_publish.base_url.empty() )
    throw std::runtime_error( "public bootstrap publish requires backup.public-publish.base-url" );
  if( public_publish.network.empty() )
    throw std::runtime_error( "public bootstrap publish requires backup.public-publish.network" );
  if( public_publish.observer_config_file.empty() )
    throw std::runtime_error( "public bootstrap publish requires backup.public-publish.observer-config-file" );

  const auto backup_id = latest_backup_id( repository_dir );
  validate_backup_id_fragment( backup_id );
  const auto snapshot_dir = repository_dir / "snapshots" / backup_id;
  if( !std::filesystem::exists( snapshot_dir / "COMPLETE" ) )
    throw std::runtime_error( "latest local snapshot is not complete: " + snapshot_dir.string() );

  auto source_manifest = nlohmann::json::parse( read_file( snapshot_dir / "manifest.json" ) );
  auto source_files = nlohmann::json::parse( read_file( snapshot_dir / "files.json" ) );
  if( source_manifest.value( "backup_id", std::string{} ) != backup_id )
    throw std::runtime_error( "snapshot manifest backup_id does not match latest backup" );
  if( source_files.value( "backup_id", std::string{} ) != backup_id )
    throw std::runtime_error( "snapshot files backup_id does not match latest backup" );
  if( !source_files.contains( "files" ) || !source_files.at( "files" ).is_array() )
    throw std::runtime_error( "snapshot files.json has unexpected format" );

  const auto observer_config = read_file( public_publish.observer_config_file );
  validate_public_observer_config( observer_config );
  const auto config_sha = sha256_bytes( observer_config );
  const auto config_size = static_cast< uint64_t >( observer_config.size() );

  std::vector< nlohmann::json > output_entries;
  std::map< std::string, uint64_t > unique_objects;
  uint64_t total_bytes = 0;
  uint64_t restored_database_bytes = 0;
  uint64_t runtime_files_bytes = 0;

  for( const auto& file: source_files.at( "files" ) )
  {
    const auto relative_path = file.at( "path" ).get< std::string >();
    validate_public_snapshot_path( relative_path );
    const auto sha256 = file.at( "sha256" ).get< std::string >();
    if( sha256.size() != 64 )
      throw std::runtime_error( "invalid SHA-256 in snapshot files.json: " + sha256 );
    const auto size_bytes = file.value( "size_bytes", 0ULL );
    const auto object_file = object_path( repository_dir, sha256 );
    if( !std::filesystem::is_regular_file( object_file ) )
      throw std::runtime_error( "missing snapshot object for public publish: " + object_file.string() );
    std::error_code size_ec;
    const auto actual_size = std::filesystem::file_size( object_file, size_ec );
    if( size_ec || actual_size != size_bytes )
      throw std::runtime_error( "snapshot object size mismatch for public publish: " + object_file.string() );

    nlohmann::json entry = {
      { "path", relative_path },
      { "sha256", sha256 },
      { "size_bytes", size_bytes },
      { "runtime_file", file.value( "runtime_file", false ) },
    };
    if( relative_path == "config.yml" )
    {
      entry[ "sha256" ] = config_sha;
      entry[ "size_bytes" ] = config_size;
      entry[ "runtime_file" ] = true;
    }

    const auto entry_size = entry.at( "size_bytes" ).get< uint64_t >();
    const auto entry_sha = entry.at( "sha256" ).get< std::string >();
    total_bytes += entry_size;
    if( relative_path.rfind( "db/", 0 ) == 0 )
      restored_database_bytes += entry_size;
    else
      runtime_files_bytes += entry_size;
    unique_objects.emplace( entry_sha, entry_size );
    output_entries.push_back( std::move( entry ) );
  }

  std::sort( output_entries.begin(), output_entries.end(), []( const auto& lhs, const auto& rhs ) {
    return lhs.at( "path" ).template get< std::string >()
           < rhs.at( "path" ).template get< std::string >();
  } );

  const uint64_t metadata_overhead_bytes = 128ULL * 1024ULL * 1024ULL;
  const uint64_t recommended_margin_bytes = std::max< uint64_t >(
    10ULL * 1024ULL * 1024ULL * 1024ULL,
    restored_database_bytes / 5ULL );
  nlohmann::json sizes = {
    { "restored_database_bytes", restored_database_bytes },
    { "runtime_files_bytes", runtime_files_bytes },
    { "object_download_bytes", total_bytes },
    { "archive_bytes", 0 },
    { "minimum_target_free_bytes", restored_database_bytes + runtime_files_bytes + metadata_overhead_bytes },
    { "recommended_target_free_bytes", restored_database_bytes + runtime_files_bytes + metadata_overhead_bytes + recommended_margin_bytes },
  };

  const auto source = source_manifest.value( "source", nlohmann::json::object() );
  const auto node = source_manifest.value( "node", nlohmann::json::object() );
  const auto source_chain = source_manifest.value( "chain", nlohmann::json::object() );
  const auto source_head = source_manifest.value( "head", nlohmann::json::object() );
  const auto source_lib = source_manifest.value( "lib", nlohmann::json::object() );
  const auto source_snapshot = source_manifest.value( "snapshot", nlohmann::json::object() );
  nlohmann::json public_source = {
    { "backup_id", backup_id },
    { "created_at", source_manifest.value( "created_at", std::string{} ) },
    { "node_id", source.value( "node_id", std::string{} ) },
    { "node_version", node.value( "version", std::string{} ) },
    { "storage_layout", source.value( "storage_layout", std::string( "unified" ) ) },
    { "chain_id", optional_json_string( source, source_chain, { "chain_id" } ) },
    { "head_height", optional_json_uint64( source, source_head, { "head_height", "height" } ) },
    { "lib_height", optional_json_uint64( source, source_lib, { "lib_height", "height" } ) },
  };
  if( public_source.at( "head_height" ).get< uint64_t >() == 0 )
    public_source[ "head_height" ] = optional_json_uint64( source_snapshot, source_chain, { "head_height" } );
  if( public_source.at( "lib_height" ).get< uint64_t >() == 0 )
    public_source[ "lib_height" ] = optional_json_uint64( source_snapshot, source_chain, { "lib_height" } );

  auto public_base_url = public_publish.base_url;
  while( !public_base_url.empty() && public_base_url.back() == '/' )
    public_base_url.pop_back();
  const auto promoted_at = utc_timestamp();

  auto public_manifest = source_manifest;
  public_manifest[ "source" ] = {
    { "node_id", "public-bootstrap-" + public_publish.network },
    { "storage_layout", source.value( "storage_layout", std::string( "unified" ) ) },
    { "network", public_publish.network },
    { "chain_id", public_source.value( "chain_id", std::string{} ) },
  };
  public_manifest[ "repository" ] = {
    { "type", "public-bootstrap-object-store" },
    { "base_url", public_base_url },
  };
  public_manifest[ "snapshot" ] = {
    { "file_count", static_cast< uint64_t >( output_entries.size() ) },
    { "object_count", static_cast< uint64_t >( unique_objects.size() ) },
    { "total_bytes", total_bytes },
  };
  public_manifest[ "sizes" ] = sizes;
  public_manifest[ "restore" ] = {
    { "requires_node_stop", true },
    { "start_as_observer_first", true },
    { "force_block_producer_disabled_on_first_start", true },
  };
  public_manifest[ "public_bootstrap" ] = {
    { "version", 1 },
    { "network", public_publish.network },
    { "chain_id", public_source.value( "chain_id", std::string{} ) },
    { "source_backup_id", backup_id },
    { "public_base_url", public_base_url },
    { "promoted_at", promoted_at },
    { "producer_mode", false },
    { "sanitized_config", true },
    { "source", public_source },
    { "restore_space", sizes },
  };

  nlohmann::json public_files = {
    { "format", "teleno-native-snapshot-files" },
    { "version", source_files.value( "version", 1 ) },
    { "backup_id", backup_id },
    { "files", output_entries },
  };

  nlohmann::json public_metadata = {
    { "format", "teleno-public-bootstrap-snapshot" },
    { "version", 1 },
    { "network", public_publish.network },
    { "chain_id", public_source.value( "chain_id", std::string{} ) },
    { "backup_id", backup_id },
    { "source_backup_id", backup_id },
    { "public_base_url", public_base_url },
    { "promoted_at", promoted_at },
    { "sanitized_config_sha256", config_sha },
    { "sanitized_config_size_bytes", config_size },
    { "file_count", static_cast< uint64_t >( output_entries.size() ) },
    { "object_count", static_cast< uint64_t >( unique_objects.size() ) },
    { "total_bytes", total_bytes },
    { "producer_mode", false },
    { "source", public_source },
    { "restore_space", sizes },
  };

  PublicBootstrapPublishPlan plan;
  plan.repository_dir = repository_dir;
  plan.backup_id = backup_id;
  plan.public_directory = public_publish.directory;
  plan.public_base_url = public_base_url;
  plan.network = public_publish.network;
  plan.sanitized_config_sha256 = config_sha;
  plan.sanitized_config_size = config_size;
  plan.file_count = static_cast< uint64_t >( output_entries.size() );
  plan.object_count = static_cast< uint64_t >( unique_objects.size() );
  plan.total_bytes = total_bytes;
  plan.latest_json = public_latest_json_for_backup_id( backup_id );
  plan.manifest_json = std::move( public_manifest );
  plan.files_json = std::move( public_files );
  plan.public_metadata_json = std::move( public_metadata );
  plan.sanitized_config_tmp = write_temp_file( "teleno-public-config", observer_config );
  plan.latest_tmp = write_temp_file( "teleno-public-latest", json_bytes( plan.latest_json ) );
  plan.manifest_tmp = write_temp_file( "teleno-public-manifest", json_bytes( plan.manifest_json ) );
  plan.files_tmp = write_temp_file( "teleno-public-files", json_bytes( plan.files_json ) );
  plan.public_metadata_tmp = write_temp_file( "teleno-public-metadata", json_bytes( plan.public_metadata_json ) );
  plan.complete_tmp = write_temp_file( "teleno-public-complete", "complete\n" );
  return plan;
}

std::vector< RemoteSnapshotMetadata > download_remote_snapshot_metadata(
  NativeSftpClient& client,
  const std::filesystem::path& metadata_root,
  const BackupRemoteConfig& remote,
  const SftpTransferOptions& options,
  std::string& latest_backup_id )
{
  std::vector< RemoteSnapshotMetadata > snapshots;
  latest_backup_id.clear();

  const auto remote_latest = remote_join( remote.directory, "latest.json" );
  if( client.exists( remote_latest ) )
  {
    const auto latest_tmp = metadata_root / "latest.json";
    client.download( remote_latest, latest_tmp );
    const auto latest = nlohmann::json::parse( read_file( latest_tmp ) );
    latest_backup_id = latest.at( "backup_id" ).get< std::string >();
    validate_backup_id_fragment( latest_backup_id );
  }

  const auto remote_snapshots_dir = remote_join( remote.directory, "snapshots" );
  const auto entries = client.list_directory( remote_snapshots_dir );
  uint64_t index = 0;
  for( const auto& entry: entries )
  {
    ++index;
    throw_if_transfer_cancelled( options );
    if( !entry.directory )
      continue;
    if( entry.name.size() >= 8 && entry.name.substr( entry.name.size() - 8 ) == ".partial" )
      continue;
    validate_backup_id_fragment( entry.name );

    const auto snapshot_rel = remote_join( "snapshots", entry.name );
    if( !client.exists( remote_join( remote.directory, remote_join( snapshot_rel, "COMPLETE" ) ) ) )
      continue;

    emit_progress( options,
                   "delete-remote-metadata",
                   entry.name,
                   index,
                   entries.size(),
                   1,
                   3,
                   0 );

    const auto snapshot_tmp = metadata_root / "snapshots" / entry.name;
    std::filesystem::create_directories( snapshot_tmp );
    const auto manifest_tmp = snapshot_tmp / "manifest.json";
    const auto files_tmp = snapshot_tmp / "files.json";
    const auto complete_tmp = snapshot_tmp / "COMPLETE";

    client.download( remote_join( remote.directory, remote_join( snapshot_rel, "manifest.json" ) ),
                     manifest_tmp );
    client.download( remote_join( remote.directory, remote_join( snapshot_rel, "files.json" ) ),
                     files_tmp );
    client.download( remote_join( remote.directory, remote_join( snapshot_rel, "COMPLETE" ) ),
                     complete_tmp );

    RemoteSnapshotMetadata snapshot;
    snapshot.snapshot_dir_name = entry.name;
    const auto manifest = nlohmann::json::parse( read_file( manifest_tmp ) );
    snapshot.backup_id = manifest.value( "backup_id", entry.name );
    validate_backup_id_fragment( snapshot.backup_id );
    snapshot.latest = !latest_backup_id.empty() && snapshot.backup_id == latest_backup_id;
    snapshot.objects = read_snapshot_object_sizes_from_file( files_tmp );
    snapshot.metadata_file_count = 3;
    snapshot.metadata_bytes = std::filesystem::file_size( manifest_tmp )
                              + std::filesystem::file_size( files_tmp )
                              + std::filesystem::file_size( complete_tmp );
    snapshots.push_back( std::move( snapshot ) );
  }

  std::sort( snapshots.begin(), snapshots.end(), []( const auto& lhs, const auto& rhs ) {
    return lhs.backup_id < rhs.backup_id;
  } );
  return snapshots;
}

void cache_downloaded_snapshot_metadata( const std::filesystem::path& repository_dir,
                                         const std::string& snapshot_dir_name,
                                         const std::filesystem::path& manifest_tmp,
                                         const std::filesystem::path& files_tmp,
                                         const std::filesystem::path& complete_tmp )
{
  validate_backup_id_fragment( snapshot_dir_name );
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
}

void execute_sftp_commands( NativeSftpClient& client,
                            const std::vector< std::string >& commands )
{
  for( const auto& command: commands )
    client.execute( parse_sftp_command( command ) );
}

void ensure_remote_directory( NativeSftpClient& client,
                              const std::string& remote_directory )
{
  std::vector< std::string > commands;
  add_remote_directory_mkdirs( commands, remote_directory );
  execute_sftp_commands( client, commands );
}

void ensure_remote_file_parent( NativeSftpClient& client,
                                const std::string& remote_file )
{
  const auto parent = remote_parent_path( remote_file );
  if( !parent.empty() )
    ensure_remote_directory( client, parent );
}

void upload_public_file( NativeSftpClient& client,
                         const std::filesystem::path& local_file,
                         const std::string& remote_file )
{
  ensure_remote_file_parent( client, remote_file );
  client.upload( local_file, remote_file );
}

bool remove_public_snapshot_metadata_if_exists( NativeSftpClient& client,
                                                const std::string& public_directory,
                                                const std::string& backup_id )
{
  validate_backup_id_fragment( backup_id );
  const auto snapshot_dir = remote_join( public_directory, remote_join( "snapshots", backup_id ) );
  if( !client.exists( snapshot_dir ) )
    return false;

  const auto entries = client.list_directory( snapshot_dir );
  for( const auto& entry: entries )
  {
    if( entry.directory )
      throw std::runtime_error( "public bootstrap snapshot contains unexpected directory: "
                                + remote_join( snapshot_dir, entry.name ) );
    client.remove_file_if_exists( remote_join( snapshot_dir, entry.name ) );
  }
  return client.remove_directory_if_exists( snapshot_dir, false );
}

std::vector< std::string > prune_public_bootstrap_metadata( NativeSftpClient& client,
                                                            const std::string& public_directory,
                                                            uint64_t retention_count )
{
  std::vector< std::string > removed;
  if( retention_count == 0 )
    return removed;

  const auto snapshots_dir = remote_join( public_directory, "snapshots" );
  if( !client.exists( snapshots_dir ) )
    return removed;

  std::vector< std::string > completed;
  for( const auto& entry: client.list_directory( snapshots_dir ) )
  {
    if( !entry.directory )
      continue;
    if( entry.name.size() >= 8 && entry.name.substr( entry.name.size() - 8 ) == ".partial" )
      continue;
    if( entry.name.find( ".partial-" ) != std::string::npos )
      continue;
    validate_backup_id_fragment( entry.name );
    if( client.exists( remote_join( snapshots_dir, remote_join( entry.name, "COMPLETE" ) ) ) )
      completed.push_back( entry.name );
  }

  std::sort( completed.begin(), completed.end() );
  if( completed.size() <= retention_count )
    return removed;

  const auto remove_count = completed.size() - static_cast< std::size_t >( retention_count );
  for( std::size_t i = 0; i < remove_count; ++i )
  {
    if( remove_public_snapshot_metadata_if_exists( client, public_directory, completed[ i ] ) )
      removed.push_back( completed[ i ] );
  }
  return removed;
}

PublicBootstrapPublishResult publish_public_bootstrap_plan_with_sftp(
  const PublicBootstrapPublishPlan& plan,
  const BackupSshConfig& ssh,
  const BackupPublicPublishConfig& public_publish,
  const SftpTransferOptions& options )
{
  NativeSftpClient client( ssh );
  const auto public_directory = plan.public_directory;
  ensure_remote_directory( client, public_directory );
  if( !client.exists( remote_join( public_directory, "objects" ) ) )
    throw std::runtime_error( "public bootstrap publish requires pre-provisioned public objects overlay: "
                              + remote_join( public_directory, "objects" ) );

  emit_progress( options,
                 "public-publish-config-object",
                 plan.backup_id,
                 0,
                 4,
                 1,
                 1,
                 plan.sanitized_config_size );
  const auto config_object = remote_join(
    public_directory,
    "objects/sha256/" + plan.sanitized_config_sha256.substr( 0, 2 ) + "/"
    + plan.sanitized_config_sha256.substr( 2, 2 ) + "/" + plan.sanitized_config_sha256 );
  upload_public_file( client, plan.sanitized_config_tmp, config_object );

  emit_progress( options,
                 "public-publish-metadata",
                 plan.backup_id,
                 1,
                 4,
                 1,
                 4,
                 plan.total_bytes );
  const auto suffix = public_publish.upload_temp_suffix.empty()
    ? std::string( ".partial" )
    : public_publish.upload_temp_suffix;
  const auto partial_snapshot_name = plan.backup_id + suffix + "-"
                                     + std::to_string( std::chrono::duration_cast< std::chrono::milliseconds >(
                                       std::chrono::system_clock::now().time_since_epoch() ).count() );
  const auto snapshots_dir = remote_join( public_directory, "snapshots" );
  const auto partial_snapshot_dir = remote_join( snapshots_dir, partial_snapshot_name );
  const auto final_snapshot_dir = remote_join( snapshots_dir, plan.backup_id );
  ensure_remote_directory( client, partial_snapshot_dir );
  upload_public_file( client, plan.manifest_tmp, remote_join( partial_snapshot_dir, "manifest.json" ) );
  upload_public_file( client, plan.files_tmp, remote_join( partial_snapshot_dir, "files.json" ) );
  upload_public_file( client, plan.public_metadata_tmp, remote_join( partial_snapshot_dir, "public-bootstrap.json" ) );
  upload_public_file( client, plan.complete_tmp, remote_join( partial_snapshot_dir, "COMPLETE" ) );

  emit_progress( options,
                 "public-publish-activate",
                 plan.backup_id,
                 2,
                 4,
                 1,
                 4,
                 plan.total_bytes );
  remove_public_snapshot_metadata_if_exists( client, public_directory, plan.backup_id );
  client.rename( partial_snapshot_dir, final_snapshot_dir );

  const auto latest_partial = remote_join( public_directory, "latest.json" + suffix );
  upload_public_file( client, plan.latest_tmp, latest_partial );
  client.remove_file_if_exists( remote_join( public_directory, "latest.json" ) );
  client.rename( latest_partial, remote_join( public_directory, "latest.json" ) );

  emit_progress( options,
                 "public-publish-prune",
                 plan.backup_id,
                 3,
                 4,
                 1,
                 plan.file_count,
                 plan.total_bytes );
  auto removed = prune_public_bootstrap_metadata( client, public_directory, public_publish.retention_count );

  PublicBootstrapPublishResult result;
  result.backup_id = plan.backup_id;
  result.repository_dir = plan.repository_dir;
  result.public_directory = public_directory;
  result.public_base_url = plan.public_base_url;
  result.network = plan.network;
  result.sanitized_config_sha256 = plan.sanitized_config_sha256;
  result.sanitized_config_size = plan.sanitized_config_size;
  result.file_count = plan.file_count;
  result.object_count = plan.object_count;
  result.total_bytes = plan.total_bytes;
  result.removed_public_snapshot_ids = std::move( removed );
  result.removed_public_snapshot_count = static_cast< uint64_t >( result.removed_public_snapshot_ids.size() );

  emit_progress( options,
                 "public-publish-complete",
                 plan.backup_id,
                 4,
                 4,
                 1,
                 result.file_count,
                 result.total_bytes );
  return result;
}

void fetch_remote_metadata( const std::filesystem::path& repository_dir,
                            const BackupSshConfig& ssh,
                            const BackupRemoteConfig& remote,
                            const std::string& requested_backup_id,
                            const SftpTransferOptions& options,
                            uint64_t& batch_file_count,
                            uint64_t& retry_count,
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
    std::filesystem::path latest_tmp;
    std::string backup_id;
    std::string snapshot_dir_name;
    std::string manifest_rel;
    std::string files_rel;

    if( requested_backup_id.empty() || requested_backup_id == "latest" )
    {
      latest_tmp = metadata_root / "latest.json";
      run_sftp_batch(
        {
          "cd " + sftp_quote( remote.directory ),
          "get " + sftp_quote( "latest.json" ) + " " + sftp_quote( latest_tmp.string() )
        },
        "teleno-sftp-restore-metadata",
        ssh,
        options,
        "restore-metadata-latest",
        {},
        2,
        1,
        0,
        batch_file_count,
        retry_count );
      ++metadata_file_count;

      const auto latest = nlohmann::json::parse( read_file( latest_tmp ) );
      backup_id = latest.at( "backup_id" ).get< std::string >();
      snapshot_dir_name = latest.value( "snapshot_dir", backup_id );
      manifest_rel = latest.value( "manifest", "snapshots/" + snapshot_dir_name + "/manifest.json" );
      files_rel = latest.value( "files", "snapshots/" + snapshot_dir_name + "/files.json" );
    }
    else
    {
      validate_backup_id_fragment( requested_backup_id );
      backup_id = requested_backup_id;
      snapshot_dir_name = requested_backup_id;
      manifest_rel = "snapshots/" + snapshot_dir_name + "/manifest.json";
      files_rel = "snapshots/" + snapshot_dir_name + "/files.json";
    }

    validate_backup_id_fragment( backup_id );
    validate_backup_id_fragment( snapshot_dir_name );
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
      options,
      "restore-metadata-snapshot",
      backup_id,
      2,
      3,
      0,
      batch_file_count,
      retry_count );
    metadata_file_count += 3;

    cache_downloaded_snapshot_metadata( repository_dir,
                                        snapshot_dir_name,
                                        manifest_tmp,
                                        files_tmp,
                                        complete_tmp );

    if( !latest_tmp.empty() )
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

BackupSnapshotListResult list_remote_backup_snapshots_once( const std::filesystem::path& repository_dir,
                                                            const BackupSshConfig& ssh,
                                                            const BackupRemoteConfig& remote,
                                                            const SftpTransferOptions& options )
{
  if( repository_dir.empty() )
    throw std::runtime_error( "local snapshot repository directory is required" );
  if( remote.directory.empty() )
    throw std::runtime_error( "remote backup directory is required" );

  const auto metadata_root = repository_dir / ".remote-list-metadata"
                             / std::to_string( std::chrono::duration_cast< std::chrono::milliseconds >(
                               std::chrono::system_clock::now().time_since_epoch() ).count() );
  std::filesystem::create_directories( metadata_root );
  std::filesystem::create_directories( repository_dir / "snapshots" );

  try
  {
    NativeSftpClient client( ssh );
    std::set< std::string > remote_backup_ids;
    std::string remote_latest_backup_id;
    bool remote_space_check_ok = false;
    uint64_t remote_available_bytes = 0;
    std::string remote_space_target_path = remote.directory;
    std::string remote_space_message;

    try
    {
      remote_available_bytes = client.available_bytes( remote.directory, &remote_space_target_path );
      remote_space_check_ok = true;
      std::ostringstream message;
      message << "Remote backup directory has " << remote_available_bytes
              << " bytes available at " << remote_space_target_path;
      remote_space_message = message.str();
    }
    catch( const std::exception& e )
    {
      remote_space_message = std::string( "Remote free-space check failed: " ) + e.what();
    }

    const auto remote_latest = remote_join( remote.directory, "latest.json" );
    if( client.exists( remote_latest ) )
    {
      const auto latest_tmp = metadata_root / "latest.json";
      client.download( remote_latest, latest_tmp );
      const auto latest = nlohmann::json::parse( read_file( latest_tmp ) );
      const auto latest_backup_id = latest.at( "backup_id" ).get< std::string >();
      validate_backup_id_fragment( latest_backup_id );
      remote_latest_backup_id = latest_backup_id;
      copy_file_atomic( latest_tmp, repository_dir / "latest.json" );
    }

    const auto remote_snapshots_dir = remote_join( remote.directory, "snapshots" );
    const auto entries = client.exists( remote_snapshots_dir )
      ? client.list_directory( remote_snapshots_dir )
      : std::vector< RemoteDirectoryEntry >{};
    uint64_t index = 0;
    uint64_t snapshot_count = 0;
    for( const auto& entry: entries )
    {
      ++index;
      throw_if_transfer_cancelled( options );
      if( !entry.directory )
        continue;
      if( entry.name.size() >= 8 && entry.name.substr( entry.name.size() - 8 ) == ".partial" )
        continue;
      validate_backup_id_fragment( entry.name );

      const auto snapshot_rel = remote_join( "snapshots", entry.name );
      if( !client.exists( remote_join( remote.directory, remote_join( snapshot_rel, "COMPLETE" ) ) ) )
        continue;

      emit_progress( options,
                     "list-remote-snapshot",
                     entry.name,
                     index,
                     entries.size(),
                     1,
                     3,
                     0 );

      const auto snapshot_tmp = metadata_root / "snapshots" / entry.name;
      std::filesystem::create_directories( snapshot_tmp );
      const auto manifest_tmp = snapshot_tmp / "manifest.json";
      const auto files_tmp = snapshot_tmp / "files.json";
      const auto complete_tmp = snapshot_tmp / "COMPLETE";

      client.download( remote_join( remote.directory, remote_join( snapshot_rel, "manifest.json" ) ),
                       manifest_tmp );
      client.download( remote_join( remote.directory, remote_join( snapshot_rel, "files.json" ) ),
                       files_tmp );
      client.download( remote_join( remote.directory, remote_join( snapshot_rel, "COMPLETE" ) ),
                       complete_tmp );
      const auto manifest = nlohmann::json::parse( read_file( manifest_tmp ) );
      const auto backup_id = manifest.value( "backup_id", entry.name );
      validate_backup_id_fragment( backup_id );
      cache_downloaded_snapshot_metadata( repository_dir,
                                          entry.name,
                                          manifest_tmp,
                                          files_tmp,
                                          complete_tmp );
      remote_backup_ids.insert( backup_id );
      ++snapshot_count;
    }

    emit_progress( options,
                   "list-remote-complete",
                   {},
                   snapshot_count,
                   snapshot_count,
                   1,
                   snapshot_count,
                   0 );

    auto result = list_local_backup_snapshots( repository_dir );
    std::vector< BackupSnapshotSummary > remote_snapshots;
    for( auto& snapshot: result.snapshots )
    {
      if( remote_backup_ids.find( snapshot.backup_id ) == remote_backup_ids.end() )
        continue;
      snapshot.latest = !remote_latest_backup_id.empty() && snapshot.backup_id == remote_latest_backup_id;
      remote_snapshots.push_back( std::move( snapshot ) );
    }
    result.latest_backup_id = remote_latest_backup_id;
    result.remote_directory = remote.directory;
    result.remote_space_target_path = remote_space_target_path;
    result.remote_space_message = remote_space_message;
    result.remote_available_bytes = remote_available_bytes;
    result.remote_space_check_ok = remote_space_check_ok;
    result.snapshots = std::move( remote_snapshots );
    std::error_code cleanup_ec;
    std::filesystem::remove_all( metadata_root, cleanup_ec );
    return result;
  }
  catch( ... )
  {
    std::error_code ec;
    std::filesystem::remove_all( metadata_root, ec );
    throw;
  }
}

} // namespace

SftpUploadPlan build_sftp_upload_plan( const std::filesystem::path& repository_dir,
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
  add_remote_directory_mkdirs( plan.batch_commands, remote_directory );
  plan.batch_commands.push_back( "cd " + sftp_quote( remote_directory ) );

  const auto objects_root = repository_dir / "objects";
  if( std::filesystem::exists( objects_root ) )
  {
    for( const auto& entry: std::filesystem::recursive_directory_iterator( objects_root ) )
    {
      if( !entry.is_regular_file() )
        continue;
      if( is_filesystem_metadata_artifact( entry.path() ) )
        continue;
      auto relative = std::filesystem::relative( entry.path(), repository_dir );
      add_put( plan, entry.path(), relative.generic_string() );
    }
  }

  const auto remote_partial_snapshot = "snapshots/" + backup_id + ".partial";
  const auto remote_final_snapshot = "snapshots/" + backup_id;
  push_unique_command( plan.batch_commands, "mkdir " + sftp_quote( "snapshots" ) );
  push_unique_command( plan.batch_commands, "mkdir " + sftp_quote( remote_partial_snapshot ) );
  add_put( plan, snapshot_dir / "files.json", remote_join( remote_partial_snapshot, "files.json" ) );
  add_put( plan, snapshot_dir / "manifest.json", remote_join( remote_partial_snapshot, "manifest.json" ) );
  add_put( plan, snapshot_dir / "COMPLETE", remote_join( remote_partial_snapshot, "COMPLETE" ) );
  plan.batch_commands.push_back( "rename " + sftp_quote( remote_partial_snapshot ) + " "
                                  + sftp_quote( remote_final_snapshot ) );

  add_put( plan, repository_dir / "latest.json", "latest.json.partial" );
  plan.batch_commands.push_back( "-rm " + sftp_quote( "latest.json" ) );
  plan.batch_commands.push_back( "rename " + sftp_quote( "latest.json.partial" ) + " "
                                  + sftp_quote( "latest.json" ) );

  return plan;
}

SftpRestoreObjectFetchPlan build_sftp_restore_object_fetch_plan(
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

SftpUploadResult upload_latest_snapshot_with_sftp( const std::filesystem::path& repository_dir,
                                                   const BackupSshConfig& ssh,
                                                   const BackupRemoteConfig& remote )
{
  return upload_latest_snapshot_with_managed_sftp( repository_dir, ssh, remote );
}

SftpUploadResult upload_latest_snapshot_with_managed_sftp( const std::filesystem::path& repository_dir,
                                                           const BackupSshConfig& ssh,
                                                           const BackupRemoteConfig& remote,
                                                           const SftpTransferOptions& options )
{
  auto plan = build_sftp_upload_plan( repository_dir, remote.directory );
  auto batch_file = write_batch_file( plan );

  uint64_t retry_count = 0;
  try
  {
    run_native_sftp_commands_with_retries( plan.batch_commands,
                                           ssh,
                                           options,
                                           "upload-latest",
                                           plan.backup_id,
                                           0,
                                           1,
                                           plan.file_count,
                                           plan.total_bytes,
                                           retry_count );
  }
  catch( ... )
  {
    std::error_code ec;
    std::filesystem::remove( batch_file, ec );
    throw;
  }

  SftpUploadResult result;
  result.backup_id = plan.backup_id;
  result.repository_dir = plan.repository_dir;
  result.remote_directory = plan.remote_directory;
  result.transport = "native-libssh";
  result.batch_file = batch_file;
  result.batch_file_removed = std::filesystem::remove( batch_file );
  result.file_count = plan.file_count;
  result.total_bytes = plan.total_bytes;
  result.batch_file_count = 1;
  result.retry_count = retry_count;
  return result;
}

PublicBootstrapPublishResult publish_latest_public_bootstrap_with_managed_sftp(
  const std::filesystem::path& repository_dir,
  const BackupSshConfig& ssh,
  const BackupPublicPublishConfig& public_publish,
  const SftpTransferOptions& options )
{
  auto plan = build_public_bootstrap_publish_plan( repository_dir, public_publish );
  try
  {
    auto result = publish_public_bootstrap_plan_with_sftp( plan, ssh, public_publish, options );
    std::error_code ec;
    std::filesystem::remove( plan.sanitized_config_tmp, ec );
    std::filesystem::remove( plan.latest_tmp, ec );
    std::filesystem::remove( plan.manifest_tmp, ec );
    std::filesystem::remove( plan.files_tmp, ec );
    std::filesystem::remove( plan.public_metadata_tmp, ec );
    std::filesystem::remove( plan.complete_tmp, ec );
    return result;
  }
  catch( ... )
  {
    std::error_code ec;
    std::filesystem::remove( plan.sanitized_config_tmp, ec );
    std::filesystem::remove( plan.latest_tmp, ec );
    std::filesystem::remove( plan.manifest_tmp, ec );
    std::filesystem::remove( plan.files_tmp, ec );
    std::filesystem::remove( plan.public_metadata_tmp, ec );
    std::filesystem::remove( plan.complete_tmp, ec );
    throw;
  }
}

BackupSnapshotListResult list_remote_backup_snapshots_with_sftp( const std::filesystem::path& repository_dir,
                                                                 const BackupSshConfig& ssh,
                                                                 const BackupRemoteConfig& remote )
{
  return list_remote_backup_snapshots_with_managed_sftp( repository_dir, ssh, remote );
}

BackupSnapshotListResult list_remote_backup_snapshots_with_managed_sftp(
  const std::filesystem::path& repository_dir,
  const BackupSshConfig& ssh,
  const BackupRemoteConfig& remote,
  const SftpTransferOptions& options )
{
  const auto max_attempts = std::max< uint64_t >( 1, options.max_attempts );
  for( uint64_t attempt = 1; attempt <= max_attempts; ++attempt )
  {
    throw_if_transfer_cancelled( options );
    emit_progress( options,
                   "list-remote",
                   {},
                   0,
                   1,
                   attempt,
                   0,
                   0 );
    try
    {
      return list_remote_backup_snapshots_once( repository_dir, ssh, remote, options );
    }
    catch( const std::exception& )
    {
      if( attempt == max_attempts )
        throw;
    }

    const auto retry_delay = std::chrono::seconds( options.retry_delay_seconds );
    const auto deadline = std::chrono::steady_clock::now() + retry_delay;
    while( std::chrono::steady_clock::now() < deadline )
    {
      throw_if_transfer_cancelled( options );
      std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    }
  }

  throw std::runtime_error( "remote backup snapshot listing did not complete" );
}

BackupDeleteResult delete_remote_backup_snapshot_with_managed_sftp( const std::filesystem::path& repository_dir,
                                                                    const BackupSshConfig& ssh,
                                                                    const BackupRemoteConfig& remote,
                                                                    const std::string& backup_id,
                                                                    bool dry_run,
                                                                    const SftpTransferOptions& options )
{
  if( repository_dir.empty() )
    throw std::runtime_error( "local snapshot repository directory is required for remote backup deletion metadata" );
  if( remote.directory.empty() )
    throw std::runtime_error( "remote backup directory is required for backup deletion" );
  if( backup_id.empty() || backup_id == "latest" )
    throw std::runtime_error( "remote backup deletion requires an exact backup ID; 'latest' is not accepted" );
  validate_backup_id_fragment( backup_id );

  const auto metadata_root = repository_dir / ".remote-delete-metadata"
                             / std::to_string( std::chrono::duration_cast< std::chrono::milliseconds >(
                               std::chrono::system_clock::now().time_since_epoch() ).count() );
  std::filesystem::create_directories( metadata_root );

  try
  {
    NativeSftpClient client( ssh );
    std::string latest_backup_id;
    auto snapshots = download_remote_snapshot_metadata( client,
                                                        metadata_root,
                                                        remote,
                                                        options,
                                                        latest_backup_id );

    auto target_it = std::find_if( snapshots.begin(), snapshots.end(), [&]( const auto& snapshot ) {
      return snapshot.backup_id == backup_id;
    } );
    if( target_it == snapshots.end() )
      throw std::runtime_error( "remote backup snapshot not found: " + backup_id );

    BackupDeleteResult result;
    result.source = "remote_sftp";
    result.backup_id = backup_id;
    result.repository_dir = repository_dir;
    result.remote_directory = remote.directory;
    result.transport = "native-libssh";
    result.dry_run = dry_run;
    result.snapshot_found = true;
    result.previous_latest_backup_id = latest_backup_id;
    result.deleted_latest = latest_backup_id == backup_id;
    result.snapshot_metadata_file_count = target_it->metadata_file_count;
    result.snapshot_metadata_bytes = target_it->metadata_bytes;

    std::set< std::string > remaining_object_hashes;
    for( const auto& snapshot: snapshots )
    {
      if( snapshot.backup_id == backup_id )
        continue;
      for( const auto& [sha256, _]: snapshot.objects )
      {
        (void)_;
        remaining_object_hashes.insert( sha256 );
      }
      if( result.deleted_latest && ( result.new_latest_backup_id.empty()
                                     || snapshot.backup_id > result.new_latest_backup_id ) )
        result.new_latest_backup_id = snapshot.backup_id;
    }
    if( !result.deleted_latest )
      result.new_latest_backup_id = latest_backup_id;

    std::map< std::string, uint64_t > reclaimable_objects;
    for( const auto& [sha256, size_bytes]: target_it->objects )
    {
      if( remaining_object_hashes.find( sha256 ) != remaining_object_hashes.end() )
        continue;
      reclaimable_objects.emplace( sha256, size_bytes );
      ++result.reclaimable_object_count;
      result.reclaimable_object_bytes += size_bytes;
    }

    if( dry_run )
    {
      std::error_code cleanup_ec;
      std::filesystem::remove_all( metadata_root, cleanup_ec );
      return result;
    }

    const auto remote_snapshot_dir = remote_join(
      remote.directory,
      remote_join( "snapshots", target_it->snapshot_dir_name ) );
    const auto snapshot_entries = client.list_directory( remote_snapshot_dir );
    for( const auto& entry: snapshot_entries )
    {
      if( entry.directory )
        throw std::runtime_error( "remote backup snapshot contains unexpected directory: "
                                  + remote_join( remote_snapshot_dir, entry.name ) );
      client.remove_file_if_exists( remote_join( remote_snapshot_dir, entry.name ) );
    }
    result.deleted_snapshot = client.remove_directory_if_exists( remote_snapshot_dir, false );

    for( const auto& [sha256, size_bytes]: reclaimable_objects )
    {
      const auto relative_object = "objects/sha256/" + sha256.substr( 0, 2 ) + "/"
                                 + sha256.substr( 2, 2 ) + "/" + sha256;
      if( client.remove_file_if_exists( remote_join( remote.directory, relative_object ) ) )
      {
        ++result.deleted_object_count;
        result.deleted_object_bytes += size_bytes;
      }

      client.remove_directory_if_exists(
        remote_join( remote.directory,
                     "objects/sha256/" + sha256.substr( 0, 2 ) + "/" + sha256.substr( 2, 2 ) ),
        true );
      client.remove_directory_if_exists(
        remote_join( remote.directory, "objects/sha256/" + sha256.substr( 0, 2 ) ),
        true );
    }

    if( result.deleted_latest )
    {
      const auto remote_latest = remote_join( remote.directory, "latest.json" );
      const auto remote_latest_partial = remote_join( remote.directory, "latest.json.partial" );
      if( result.new_latest_backup_id.empty() )
      {
        client.remove_file_if_exists( remote_latest );
      }
      else
      {
        const auto latest_tmp = metadata_root / "latest.json";
        std::ofstream out( latest_tmp, std::ios::binary | std::ios::trunc );
        if( !out )
          throw std::runtime_error( "failed to write temporary remote latest metadata: " + latest_tmp.string() );
        out << latest_json_for_remote_backup_id( result.new_latest_backup_id );
        out.close();
        client.remove_file_if_exists( remote_latest_partial );
        client.upload( latest_tmp, remote_latest_partial );
        client.remove_file_if_exists( remote_latest );
        client.rename( remote_latest_partial, remote_latest );
      }
    }

    emit_progress( options,
                   "delete-remote-complete",
                   backup_id,
                   1,
                   1,
                   1,
                   result.snapshot_metadata_file_count + result.deleted_object_count,
                   result.snapshot_metadata_bytes + result.deleted_object_bytes );

    std::error_code cleanup_ec;
    std::filesystem::remove_all( metadata_root, cleanup_ec );
    return result;
  }
  catch( ... )
  {
    std::error_code ec;
    std::filesystem::remove_all( metadata_root, ec );
    throw;
  }
}

SftpRestoreFetchResult fetch_latest_restore_snapshot_with_sftp( const std::filesystem::path& repository_dir,
                                                                const std::filesystem::path& target_basedir,
                                                                const BackupSshConfig& ssh,
                                                                const BackupRemoteConfig& remote )
{
  return fetch_restore_snapshot_with_managed_sftp( repository_dir, target_basedir, ssh, remote, {} );
}

SftpRestoreFetchResult fetch_latest_restore_snapshot_with_managed_sftp( const std::filesystem::path& repository_dir,
                                                                        const std::filesystem::path& target_basedir,
                                                                        const BackupSshConfig& ssh,
                                                                        const BackupRemoteConfig& remote,
                                                                        const SftpTransferOptions& options )
{
  return fetch_restore_snapshot_with_managed_sftp( repository_dir, target_basedir, ssh, remote, {}, options );
}

SftpRestoreFetchResult fetch_restore_snapshot_with_sftp( const std::filesystem::path& repository_dir,
                                                         const std::filesystem::path& target_basedir,
                                                         const BackupSshConfig& ssh,
                                                         const BackupRemoteConfig& remote,
                                                         const std::string& backup_id )
{
  return fetch_restore_snapshot_with_managed_sftp( repository_dir, target_basedir, ssh, remote, backup_id );
}

SftpRestoreFetchResult fetch_restore_snapshot_with_managed_sftp( const std::filesystem::path& repository_dir,
                                                                 const std::filesystem::path& target_basedir,
                                                                 const BackupSshConfig& ssh,
                                                                 const BackupRemoteConfig& remote,
                                                                 const std::string& backup_id,
                                                                 const SftpTransferOptions& options )
{
  SftpRestoreFetchResult result;
  result.repository_dir = repository_dir;
  result.target_basedir = target_basedir;
  result.remote_directory = remote.directory;
  result.transport = "native-libssh";

  const auto selected_backup_id = backup_id == "latest" ? std::string{} : backup_id;
  if( !selected_backup_id.empty() )
    validate_backup_id_fragment( selected_backup_id );

  std::string metadata_backup_id;
  fetch_remote_metadata( repository_dir,
                         ssh,
                         remote,
                         selected_backup_id,
                         options,
                         result.batch_file_count,
                         result.retry_count,
                         result.metadata_file_count,
                         metadata_backup_id );
  result.metadata_fetched = true;
  result.backup_id = metadata_backup_id;
  result.preflight = build_local_restore_preflight( repository_dir, target_basedir, selected_backup_id );

  if( !result.preflight.space_check.passes_minimum )
  {
    result.download_skipped_reason = result.preflight.space_check.message;
    return result;
  }

  auto object_plan = build_sftp_restore_object_fetch_plan(
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
    fetch_sftp_restore_objects_with_progress(
      object_plan,
      ssh,
      options,
      result.batch_file_count,
      result.retry_count );

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

  result.preflight = build_local_restore_preflight( repository_dir, target_basedir, selected_backup_id );
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
  out << "transport: " << result.transport << "\n";
  out << "batch_file: " << result.batch_file.string()
      << ( result.batch_file_removed ? " (removed)" : "" ) << "\n";
  out << "batch_file_count: " << result.batch_file_count << "\n";
  out << "retry_count: " << result.retry_count << "\n";
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
  out << "  \"transport\": \"" << json_escape( result.transport ) << "\",\n";
  out << "  \"batch_file\": \"" << json_escape( result.batch_file.string() ) << "\",\n";
  out << "  \"batch_file_removed\": " << ( result.batch_file_removed ? "true" : "false" ) << ",\n";
  out << "  \"batch_file_count\": " << result.batch_file_count << ",\n";
  out << "  \"retry_count\": " << result.retry_count << ",\n";
  out << "  \"file_count\": " << result.file_count << ",\n";
  out << "  \"total_bytes\": " << result.total_bytes << "\n";
  out << "}\n";
  return out.str();
}

std::string public_bootstrap_publish_result_to_text( const PublicBootstrapPublishResult& result )
{
  std::ostringstream out;
  out << "Published public bootstrap snapshot\n";
  out << "backup_id: " << result.backup_id << "\n";
  out << "repository_dir: " << result.repository_dir.string() << "\n";
  out << "public_directory: " << result.public_directory << "\n";
  out << "public_base_url: " << result.public_base_url << "\n";
  out << "network: " << result.network << "\n";
  out << "sanitized_config_sha256: " << result.sanitized_config_sha256 << "\n";
  out << "sanitized_config_size: " << result.sanitized_config_size << "\n";
  out << "file_count: " << result.file_count << "\n";
  out << "object_count: " << result.object_count << "\n";
  out << "total_bytes: " << result.total_bytes << "\n";
  out << "removed_public_snapshot_count: " << result.removed_public_snapshot_count << "\n";
  for( const auto& backup_id: result.removed_public_snapshot_ids )
    out << "removed_public_snapshot: " << backup_id << "\n";
  return out.str();
}

std::string public_bootstrap_publish_result_to_json( const PublicBootstrapPublishResult& result )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"backup_id\": \"" << json_escape( result.backup_id ) << "\",\n";
  out << "  \"repository_dir\": \"" << json_escape( result.repository_dir.string() ) << "\",\n";
  out << "  \"public_directory\": \"" << json_escape( result.public_directory ) << "\",\n";
  out << "  \"public_base_url\": \"" << json_escape( result.public_base_url ) << "\",\n";
  out << "  \"network\": \"" << json_escape( result.network ) << "\",\n";
  out << "  \"sanitized_config_sha256\": \"" << json_escape( result.sanitized_config_sha256 ) << "\",\n";
  out << "  \"sanitized_config_size\": " << result.sanitized_config_size << ",\n";
  out << "  \"file_count\": " << result.file_count << ",\n";
  out << "  \"object_count\": " << result.object_count << ",\n";
  out << "  \"total_bytes\": " << result.total_bytes << ",\n";
  out << "  \"removed_public_snapshot_count\": " << result.removed_public_snapshot_count << ",\n";
  out << "  \"removed_public_snapshot_ids\": [";
  for( std::size_t i = 0; i < result.removed_public_snapshot_ids.size(); ++i )
  {
    if( i )
      out << ", ";
    out << "\"" << json_escape( result.removed_public_snapshot_ids[ i ] ) << "\"";
  }
  out << "]\n";
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
  out << "transport: " << result.transport << "\n";
  out << "metadata_fetched: " << ( result.metadata_fetched ? "true" : "false" ) << "\n";
  out << "metadata_file_count: " << result.metadata_file_count << "\n";
  out << "object_file_count: " << result.object_file_count << "\n";
  out << "object_bytes: " << result.object_bytes << "\n";
  out << "repository_available_bytes: " << result.repository_available_bytes << "\n";
  out << "repository_required_bytes: " << result.repository_required_bytes << "\n";
  out << "batch_file_count: " << result.batch_file_count << "\n";
  out << "retry_count: " << result.retry_count << "\n";
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
  out << "  \"transport\": \"" << json_escape( result.transport ) << "\",\n";
  out << "  \"metadata_fetched\": " << ( result.metadata_fetched ? "true" : "false" ) << ",\n";
  out << "  \"metadata_file_count\": " << result.metadata_file_count << ",\n";
  out << "  \"object_file_count\": " << result.object_file_count << ",\n";
  out << "  \"object_bytes\": " << result.object_bytes << ",\n";
  out << "  \"repository_available_bytes\": " << result.repository_available_bytes << ",\n";
  out << "  \"repository_required_bytes\": " << result.repository_required_bytes << ",\n";
  out << "  \"batch_file_count\": " << result.batch_file_count << ",\n";
  out << "  \"retry_count\": " << result.retry_count << ",\n";
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
