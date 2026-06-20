#include "backup/public_restore.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <nlohmann/json.hpp>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

namespace koinos::node::backup {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

constexpr uint64_t repository_download_margin_bytes = 128ULL * 1024ULL * 1024ULL;
constexpr uint64_t max_redirects = 5;

struct ParsedUrl
{
  std::string scheme;
  std::string host;
  std::string port;
  std::string target;
  std::filesystem::path file_path;
};

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

std::string read_file( const std::filesystem::path& path )
{
  std::ifstream input( path, std::ios::binary );
  if( !input )
    throw std::runtime_error( "failed to read file: " + path.string() );
  return std::string( ( std::istreambuf_iterator< char >( input ) ),
                      std::istreambuf_iterator< char >() );
}

void write_file_atomic( const std::filesystem::path& path, const std::string& content )
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

  std::vector< char > buffer( 1024 * 1024 );
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

std::vector< unsigned char > hex_to_bytes( const std::string& value )
{
  if( value.size() % 2 != 0 )
    throw std::runtime_error( "hex value has odd length" );
  std::vector< unsigned char > out;
  out.reserve( value.size() / 2 );
  for( std::size_t i = 0; i < value.size(); i += 2 )
  {
    const auto byte = value.substr( i, 2 );
    char* end = nullptr;
    const auto parsed = std::strtoul( byte.c_str(), &end, 16 );
    if( !end || *end != '\0' || parsed > 0xff )
      throw std::runtime_error( "invalid hex byte: " + byte );
    out.push_back( static_cast< unsigned char >( parsed ) );
  }
  return out;
}

std::string canonical_signature_payload( const nlohmann::json& payload )
{
  if( !payload.is_object() )
    throw std::runtime_error( "public bootstrap signature payload must be a JSON object" );
  return payload.dump();
}

void verify_ed25519_signature( const std::filesystem::path& public_key_file,
                               const std::string& message,
                               const std::string& signature_hex )
{
  if( public_key_file.empty() )
    throw std::runtime_error( "public bootstrap signature verification requires signature-public-key-file" );
  const auto public_key_pem = read_file( public_key_file );
  BIO* raw_bio = BIO_new_mem_buf( public_key_pem.data(), static_cast< int >( public_key_pem.size() ) );
  if( !raw_bio )
    throw std::runtime_error( "failed to allocate public bootstrap key BIO" );
  std::unique_ptr< BIO, decltype( &BIO_free ) > bio( raw_bio, BIO_free );

  EVP_PKEY* raw_key = PEM_read_bio_PUBKEY( bio.get(), nullptr, nullptr, nullptr );
  if( !raw_key )
    throw std::runtime_error( "failed to parse public bootstrap Ed25519 public key: " + public_key_file.string() );
  std::unique_ptr< EVP_PKEY, decltype( &EVP_PKEY_free ) > key( raw_key, EVP_PKEY_free );

  EVP_MD_CTX* raw_ctx = EVP_MD_CTX_new();
  if( !raw_ctx )
    throw std::runtime_error( "failed to allocate public bootstrap signature context" );
  std::unique_ptr< EVP_MD_CTX, decltype( &EVP_MD_CTX_free ) > ctx( raw_ctx, EVP_MD_CTX_free );

  if( EVP_DigestVerifyInit( ctx.get(), nullptr, nullptr, nullptr, key.get() ) != 1 )
    throw std::runtime_error( "failed to initialize public bootstrap Ed25519 verification" );

  const auto signature = hex_to_bytes( signature_hex );
  const auto ok = EVP_DigestVerify( ctx.get(),
                                    signature.data(),
                                    signature.size(),
                                    reinterpret_cast< const unsigned char* >( message.data() ),
                                    message.size() );
  if( ok != 1 )
    throw std::runtime_error( "public bootstrap signature verification failed" );
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

std::string trim_trailing_slash( std::string value )
{
  while( value.size() > 1 && value.back() == '/' )
    value.pop_back();
  return value;
}

ParsedUrl parse_url( const std::string& url )
{
  const auto scheme_end = url.find( "://" );
  if( scheme_end == std::string::npos )
    throw std::runtime_error( "URL must include scheme://: " + url );

  ParsedUrl parsed;
  parsed.scheme = url.substr( 0, scheme_end );
  const auto rest = url.substr( scheme_end + 3 );

  if( parsed.scheme == "file" )
  {
    parsed.file_path = !rest.empty() && rest.front() == '/' ? rest : "/" + rest;
    return parsed;
  }

  if( parsed.scheme != "http" && parsed.scheme != "https" )
    throw std::runtime_error( "unsupported public backup URL scheme: " + parsed.scheme );

  const auto slash = rest.find( '/' );
  const auto authority = slash == std::string::npos ? rest : rest.substr( 0, slash );
  parsed.target = slash == std::string::npos ? "/" : rest.substr( slash );
  if( parsed.target.empty() )
    parsed.target = "/";

  const auto colon = authority.rfind( ':' );
  if( colon != std::string::npos )
  {
    parsed.host = authority.substr( 0, colon );
    parsed.port = authority.substr( colon + 1 );
  }
  else
  {
    parsed.host = authority;
    parsed.port = parsed.scheme == "https" ? "443" : "80";
  }

  if( parsed.host.empty() )
    throw std::runtime_error( "public backup URL host is empty: " + url );
  return parsed;
}

std::string join_url( const std::string& base_url, const std::string& relative )
{
  validate_relative_path( relative );
  return trim_trailing_slash( base_url ) + "/" + relative;
}

std::string resolve_redirect( const ParsedUrl& original, const std::string& location )
{
  if( location.find( "://" ) != std::string::npos )
    return location;
  if( !location.empty() && location.front() == '/' )
    return original.scheme + "://" + original.host
         + ( ( original.scheme == "https" && original.port == "443" )
             || ( original.scheme == "http" && original.port == "80" )
             ? std::string{} : ":" + original.port )
         + location;

  auto base = original.target;
  const auto slash = base.find_last_of( '/' );
  base = slash == std::string::npos ? "/" : base.substr( 0, slash + 1 );
  return original.scheme + "://" + original.host
       + ( ( original.scheme == "https" && original.port == "443" )
           || ( original.scheme == "http" && original.port == "80" )
           ? std::string{} : ":" + original.port )
       + base + location;
}

void throw_if_cancelled( const PublicRestoreOptions& options )
{
  if( options.cancel_requested && options.cancel_requested() )
    throw std::runtime_error( "public backup operation cancelled" );
}

void emit_progress( const PublicRestoreOptions& options,
                    std::string phase,
                    std::string backup_id,
                    uint64_t completed_batches,
                    uint64_t total_batches,
                    uint64_t attempt,
                    uint64_t file_count,
                    uint64_t total_bytes )
{
  if( !options.progress )
    return;
  PublicRestoreProgress progress;
  progress.phase = std::move( phase );
  progress.backup_id = std::move( backup_id );
  progress.completed_batches = completed_batches;
  progress.total_batches = total_batches;
  progress.attempt = attempt;
  progress.file_count = file_count;
  progress.total_bytes = total_bytes;
  options.progress( progress );
}

class PublicBackupClient
{
public:
  explicit PublicBackupClient( BackupPublicRestoreConfig config )
    : _config( std::move( config ) )
  {
    _config.base_url = trim_trailing_slash( _config.base_url );
    if( _config.base_url.empty() )
      throw std::runtime_error( "backup.public-restore.base-url is required" );
    auto parsed = parse_url( _config.base_url );
    if( _config.require_https && parsed.scheme != "https" && parsed.scheme != "file" )
      throw std::runtime_error( "backup.public-restore requires an https base-url unless require-https=false" );
  }

  const std::string& base_url() const
  {
    return _config.base_url;
  }

  const BackupPublicRestoreConfig& config() const
  {
    return _config;
  }

  uint64_t retry_count() const
  {
    return _retry_count;
  }

  uint64_t request_count() const
  {
    return _request_count;
  }

  void download_relative( const std::string& relative,
                          const std::filesystem::path& destination,
                          const PublicRestoreOptions& options )
  {
    const auto attempts = std::max< uint64_t >( 1, _config.retries );
    for( uint64_t attempt = 1; attempt <= attempts; ++attempt )
    {
      throw_if_cancelled( options );
      try
      {
        download_url_to_file( join_url( _config.base_url, relative ), destination, 0 );
        ++_request_count;
        return;
      }
      catch( const std::exception& )
      {
        if( attempt == attempts )
          throw;
        ++_retry_count;
        std::this_thread::sleep_for( std::chrono::milliseconds( 250 * attempt ) );
      }
    }
  }

  bool try_download_relative( const std::string& relative,
                              const std::filesystem::path& destination,
                              const PublicRestoreOptions& options )
  {
    try
    {
      download_relative( relative, destination, options );
      return true;
    }
    catch( const std::exception& )
    {
      if( options.cancel_requested && options.cancel_requested() )
        throw;
      std::error_code ec;
      std::filesystem::remove( destination, ec );
      return false;
    }
  }

private:
  void download_url_to_file( const std::string& url,
                             const std::filesystem::path& destination,
                             uint64_t redirect_count )
  {
    if( redirect_count > max_redirects )
      throw std::runtime_error( "too many redirects while fetching public backup URL: " + url );

    const auto parsed = parse_url( url );
    if( parsed.scheme == "file" )
    {
      copy_file_atomic( parsed.file_path, destination );
      return;
    }
    if( parsed.scheme == "https" )
      download_https_to_file( parsed, destination, redirect_count );
    else
      download_http_to_file( parsed, destination, redirect_count );
  }

  void download_http_to_file( const ParsedUrl& parsed,
                              const std::filesystem::path& destination,
                              uint64_t redirect_count )
  {
    asio::io_context ioc;
    tcp::resolver resolver( ioc );
    beast::tcp_stream stream( ioc );
    stream.expires_after( std::chrono::seconds( std::max< uint64_t >( 1, _config.timeout_seconds ) ) );
    auto const results = resolver.resolve( parsed.host, parsed.port );
    stream.connect( results );

    http::request< http::empty_body > req{ http::verb::get, parsed.target, 11 };
    req.set( http::field::host, parsed.host );
    req.set( http::field::user_agent, "teleno_node-public-restore" );
    req.set( http::field::accept, "*/*" );

    http::write( stream, req );
    read_response_to_file( stream, parsed, destination, redirect_count );

    beast::error_code ec;
    stream.socket().shutdown( tcp::socket::shutdown_both, ec );
  }

  void download_https_to_file( const ParsedUrl& parsed,
                               const std::filesystem::path& destination,
                               uint64_t redirect_count )
  {
    asio::io_context ioc;
    asio::ssl::context ctx( asio::ssl::context::tls_client );
    ctx.set_verify_mode( configure_certificate_verification( ctx )
                         ? asio::ssl::verify_peer
                         : asio::ssl::verify_none );

    tcp::resolver resolver( ioc );
    beast::ssl_stream< beast::tcp_stream > stream( ioc, ctx );
    if( !SSL_set_tlsext_host_name( stream.native_handle(), parsed.host.c_str() ) )
      throw beast::system_error(
        beast::error_code( static_cast< int >( ::ERR_get_error() ), asio::error::get_ssl_category() ) );

    beast::get_lowest_layer( stream ).expires_after(
      std::chrono::seconds( std::max< uint64_t >( 1, _config.timeout_seconds ) ) );
    auto const results = resolver.resolve( parsed.host, parsed.port );
    beast::get_lowest_layer( stream ).connect( results );
    stream.handshake( asio::ssl::stream_base::client );

    http::request< http::empty_body > req{ http::verb::get, parsed.target, 11 };
    req.set( http::field::host, parsed.host );
    req.set( http::field::user_agent, "teleno_node-public-restore" );
    req.set( http::field::accept, "*/*" );

    http::write( stream, req );
    read_response_to_file( stream, parsed, destination, redirect_count );

    beast::error_code ec;
    stream.shutdown( ec );
  }

  bool configure_certificate_verification( asio::ssl::context& ctx )
  {
    const std::filesystem::path candidates[] = {
      "/etc/ssl/cert.pem",
      "/etc/ssl/certs/ca-certificates.crt",
      "/opt/homebrew/etc/openssl@3/cert.pem",
      "/opt/homebrew/etc/ca-certificates/cert.pem",
      "/usr/local/etc/openssl@3/cert.pem",
      "/usr/local/etc/openssl/cert.pem"
    };

    bool loaded = false;
    for( const auto& candidate: candidates )
    {
      std::error_code fs_ec;
      if( !std::filesystem::is_regular_file( candidate, fs_ec ) )
        continue;

      beast::error_code ssl_ec;
      ctx.load_verify_file( candidate.string(), ssl_ec );
      if( !ssl_ec )
        loaded = true;
    }

    if( loaded )
      return true;

    beast::error_code default_ec;
    ctx.set_default_verify_paths( default_ec );
    return !default_ec;
  }

  template< typename Stream >
  void read_response_to_file( Stream& stream,
                              const ParsedUrl& parsed,
                              const std::filesystem::path& destination,
                              uint64_t redirect_count )
  {
    std::filesystem::create_directories( destination.parent_path() );
    auto partial = destination;
    partial += ".download";
    std::error_code remove_ec;
    std::filesystem::remove( partial, remove_ec );

    beast::flat_buffer buffer;
    http::response_parser< http::file_body > parser;
    parser.body_limit( std::numeric_limits< std::uint64_t >::max() );
    beast::error_code file_ec;
    parser.get().body().open( partial.string().c_str(), beast::file_mode::write, file_ec );
    if( file_ec )
      throw std::runtime_error( "failed to open public backup download destination: " + partial.string() );

    http::read( stream, buffer, parser );
    auto response = parser.release();

    if( response.result_int() >= 300 && response.result_int() < 400 )
    {
      std::filesystem::remove( partial, remove_ec );
      auto location = response[ http::field::location ];
      if( location.empty() )
        throw std::runtime_error( "public backup redirect response is missing Location header" );
      download_url_to_file( resolve_redirect( parsed, std::string( location ) ),
                            destination,
                            redirect_count + 1 );
      return;
    }

    if( response.result() != http::status::ok )
    {
      std::filesystem::remove( partial, remove_ec );
      std::ostringstream message;
      message << "failed to fetch public backup URL " << parsed.scheme << "://"
              << parsed.host << parsed.target << ": HTTP " << response.result_int();
      throw std::runtime_error( message.str() );
    }

    std::filesystem::rename( partial, destination );
  }

  BackupPublicRestoreConfig _config;
  uint64_t _retry_count = 0;
  uint64_t _request_count = 0;
};

struct PublicSnapshotMetadata
{
  std::string backup_id;
  std::string snapshot_dir_name;
  std::string manifest_rel;
  std::string files_rel;
  std::string public_metadata_rel;
  std::string signature_rel;
  std::filesystem::path manifest_tmp;
  std::filesystem::path files_tmp;
  std::filesystem::path public_metadata_tmp;
  std::filesystem::path signature_tmp;
  std::filesystem::path complete_tmp;
  bool latest = false;
  bool signature_verified = false;
};

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

std::string latest_json_for_backup_id( const std::string& backup_id )
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

void verify_public_bootstrap_signature( PublicSnapshotMetadata& metadata,
                                        PublicBackupClient& client,
                                        const std::filesystem::path& latest_tmp,
                                        uint64_t& metadata_file_count,
                                        const PublicRestoreOptions& options )
{
  const auto& cfg = client.config();
  const bool should_attempt_signature = cfg.signature_required || !cfg.signature_public_key_file.empty();
  if( !should_attempt_signature )
    return;

  if( metadata.public_metadata_rel.empty() )
    metadata.public_metadata_rel = "snapshots/" + metadata.snapshot_dir_name + "/public-bootstrap.json";
  if( metadata.signature_rel.empty() )
    metadata.signature_rel = "snapshots/" + metadata.snapshot_dir_name + "/public-bootstrap-signature.json";
  validate_relative_path( metadata.public_metadata_rel );
  validate_relative_path( metadata.signature_rel );

  metadata.public_metadata_tmp = metadata.manifest_tmp.parent_path() / "public-bootstrap.json";
  metadata.signature_tmp = metadata.manifest_tmp.parent_path() / "public-bootstrap-signature.json";

  const auto public_metadata_ok = cfg.signature_required
    ? ( client.download_relative( metadata.public_metadata_rel, metadata.public_metadata_tmp, options ), true )
    : client.try_download_relative( metadata.public_metadata_rel, metadata.public_metadata_tmp, options );
  if( !public_metadata_ok )
    return;
  ++metadata_file_count;

  const auto signature_ok = cfg.signature_required
    ? ( client.download_relative( metadata.signature_rel, metadata.signature_tmp, options ), true )
    : client.try_download_relative( metadata.signature_rel, metadata.signature_tmp, options );
  if( !signature_ok )
    return;
  ++metadata_file_count;

  const auto envelope = nlohmann::json::parse( read_file( metadata.signature_tmp ) );
  if( envelope.value( "format", std::string{} ) != "teleno-public-bootstrap-signature" )
    throw std::runtime_error( "public bootstrap signature has unexpected format" );
  if( envelope.value( "version", 0 ) != 1 )
    throw std::runtime_error( "public bootstrap signature has unsupported version" );
  if( envelope.value( "algorithm", std::string{} ) != "ed25519" )
    throw std::runtime_error( "public bootstrap signature must use ed25519" );
  const auto payload = envelope.at( "payload" );
  const auto signature_hex = envelope.at( "signature_hex" ).get< std::string >();
  verify_ed25519_signature( cfg.signature_public_key_file,
                            canonical_signature_payload( payload ),
                            signature_hex );

  if( payload.value( "backup_id", std::string{} ) != metadata.backup_id )
    throw std::runtime_error( "public bootstrap signature backup_id mismatch" );
  if( !cfg.network.empty() && payload.value( "network", std::string{} ) != cfg.network )
    throw std::runtime_error( "public bootstrap signature network mismatch" );
  if( !latest_tmp.empty()
      && payload.value( "latest_sha256", std::string{} ) != sha256_file( latest_tmp ) )
    throw std::runtime_error( "public bootstrap signature latest.json hash mismatch" );
  if( payload.value( "manifest_sha256", std::string{} ) != sha256_file( metadata.manifest_tmp ) )
    throw std::runtime_error( "public bootstrap signature manifest hash mismatch" );
  if( payload.value( "files_sha256", std::string{} ) != sha256_file( metadata.files_tmp ) )
    throw std::runtime_error( "public bootstrap signature files hash mismatch" );
  if( payload.value( "public_metadata_sha256", std::string{} ) != sha256_file( metadata.public_metadata_tmp ) )
    throw std::runtime_error( "public bootstrap signature metadata hash mismatch" );

  const auto manifest = nlohmann::json::parse( read_file( metadata.manifest_tmp ) );
  const auto snapshot = manifest.at( "snapshot" );
  if( payload.value( "object_count", 0ULL ) != snapshot.value( "object_count", 0ULL ) )
    throw std::runtime_error( "public bootstrap signature object_count mismatch" );
  if( payload.value( "total_bytes", 0ULL ) != snapshot.value( "total_bytes", 0ULL ) )
    throw std::runtime_error( "public bootstrap signature total_bytes mismatch" );

  const auto public_metadata = nlohmann::json::parse( read_file( metadata.public_metadata_tmp ) );
  if( public_metadata.value( "backup_id", std::string{} ) != metadata.backup_id )
    throw std::runtime_error( "public bootstrap metadata backup_id mismatch" );
  if( public_metadata.value( "network", std::string{} ) != payload.value( "network", std::string{} ) )
    throw std::runtime_error( "public bootstrap metadata network mismatch" );
  metadata.signature_verified = true;
}

PublicSnapshotMetadata fetch_public_metadata( const std::filesystem::path& repository_dir,
                                              PublicBackupClient& client,
                                              const std::string& requested_backup_id,
                                              const PublicRestoreOptions& options,
                                              uint64_t& metadata_file_count )
{
  if( repository_dir.empty() )
    throw std::runtime_error( "local snapshot repository directory is required" );

  const auto metadata_root = repository_dir / ".public-restore-metadata"
                             / std::to_string( std::chrono::duration_cast< std::chrono::milliseconds >(
                               std::chrono::system_clock::now().time_since_epoch() ).count() );
  std::filesystem::create_directories( metadata_root );

  try
  {
    PublicSnapshotMetadata metadata;
    std::filesystem::path latest_tmp;

    if( requested_backup_id.empty() || requested_backup_id == "latest" )
    {
      latest_tmp = metadata_root / "latest.json";
      emit_progress( options, "public-restore-metadata-latest", {}, 0, 1, 1, 1, 0 );
      client.download_relative( "latest.json", latest_tmp, options );
      ++metadata_file_count;

      const auto latest = nlohmann::json::parse( read_file( latest_tmp ) );
      metadata.backup_id = latest.at( "backup_id" ).get< std::string >();
      metadata.snapshot_dir_name = latest.value( "snapshot_dir", metadata.backup_id );
      metadata.manifest_rel = latest.value( "manifest", "snapshots/" + metadata.snapshot_dir_name + "/manifest.json" );
      metadata.files_rel = latest.value( "files", "snapshots/" + metadata.snapshot_dir_name + "/files.json" );
      metadata.public_metadata_rel =
        latest.value( "public_metadata", "snapshots/" + metadata.snapshot_dir_name + "/public-bootstrap.json" );
      metadata.signature_rel =
        latest.value( "signature", "snapshots/" + metadata.snapshot_dir_name + "/public-bootstrap-signature.json" );
      metadata.latest = true;
    }
    else
    {
      validate_backup_id_fragment( requested_backup_id );
      metadata.backup_id = requested_backup_id;
      metadata.snapshot_dir_name = requested_backup_id;
      metadata.manifest_rel = "snapshots/" + metadata.snapshot_dir_name + "/manifest.json";
      metadata.files_rel = "snapshots/" + metadata.snapshot_dir_name + "/files.json";
      metadata.public_metadata_rel = "snapshots/" + metadata.snapshot_dir_name + "/public-bootstrap.json";
      metadata.signature_rel = "snapshots/" + metadata.snapshot_dir_name + "/public-bootstrap-signature.json";
    }

    validate_backup_id_fragment( metadata.backup_id );
    validate_backup_id_fragment( metadata.snapshot_dir_name );
    validate_relative_path( metadata.snapshot_dir_name );
    validate_relative_path( metadata.manifest_rel );
    validate_relative_path( metadata.files_rel );
    validate_relative_path( metadata.public_metadata_rel );
    validate_relative_path( metadata.signature_rel );

    const auto snapshot_tmp = metadata_root / "snapshot";
    std::filesystem::create_directories( snapshot_tmp );
    metadata.manifest_tmp = snapshot_tmp / "manifest.json";
    metadata.files_tmp = snapshot_tmp / "files.json";
    metadata.complete_tmp = snapshot_tmp / "COMPLETE";

    emit_progress( options, "public-restore-metadata-snapshot", metadata.backup_id, 0, 3, 1, 3, 0 );
    client.download_relative( metadata.manifest_rel, metadata.manifest_tmp, options );
    emit_progress( options, "public-restore-metadata-snapshot", metadata.backup_id, 1, 3, 1, 3, 0 );
    client.download_relative( metadata.files_rel, metadata.files_tmp, options );
    emit_progress( options, "public-restore-metadata-snapshot", metadata.backup_id, 2, 3, 1, 3, 0 );
    client.download_relative( "snapshots/" + metadata.snapshot_dir_name + "/COMPLETE", metadata.complete_tmp, options );
    emit_progress( options, "public-restore-metadata-snapshot", metadata.backup_id, 3, 3, 1, 3, 0 );
    metadata_file_count += 3;

    verify_public_bootstrap_signature( metadata, client, latest_tmp, metadata_file_count, options );

    const auto manifest = nlohmann::json::parse( read_file( metadata.manifest_tmp ) );
    const auto manifest_backup_id = manifest.value( "backup_id", metadata.backup_id );
    validate_backup_id_fragment( manifest_backup_id );
    if( manifest_backup_id != metadata.backup_id )
      throw std::runtime_error( "public backup manifest backup_id does not match requested metadata" );

    cache_downloaded_snapshot_metadata( repository_dir,
                                        metadata.snapshot_dir_name,
                                        metadata.manifest_tmp,
                                        metadata.files_tmp,
                                        metadata.complete_tmp );

    if( !latest_tmp.empty() )
      copy_file_atomic( latest_tmp, repository_dir / "latest.json" );
    else if( !std::filesystem::exists( repository_dir / "latest.json" ) )
      write_file_atomic( repository_dir / "latest.json", latest_json_for_backup_id( metadata.backup_id ) );

    std::error_code ec;
    std::filesystem::remove_all( metadata_root, ec );
    return metadata;
  }
  catch( ... )
  {
    std::error_code ec;
    std::filesystem::remove_all( metadata_root, ec );
    throw;
  }
}

std::vector< std::pair< std::string, uint64_t > > missing_objects_from_files(
  const std::filesystem::path& repository_dir,
  const std::filesystem::path& files_path )
{
  const auto files = nlohmann::json::parse( read_file( files_path ) );
  std::set< std::string > planned;
  std::vector< std::pair< std::string, uint64_t > > objects;
  for( const auto& file: files.at( "files" ) )
  {
    const auto sha256 = file.at( "sha256" ).get< std::string >();
    if( sha256.size() != 64 )
      throw std::runtime_error( "invalid SHA-256 in public backup files manifest: " + sha256 );
    if( !planned.insert( sha256 ).second )
      continue;
    if( std::filesystem::exists( object_path( repository_dir, sha256 ) ) )
      continue;
    objects.emplace_back( sha256, file.value( "size_bytes", 0ULL ) );
  }
  return objects;
}

void fetch_missing_objects( PublicBackupClient& client,
                            const std::filesystem::path& repository_dir,
                            const RestorePreflightResult& preflight,
                            PublicRestoreFetchResult& result,
                            const PublicRestoreOptions& options )
{
  const auto missing_objects = missing_objects_from_files( repository_dir, preflight.files_path );
  result.object_file_count = static_cast< uint64_t >( missing_objects.size() );
  for( const auto& [_, size]: missing_objects )
  {
    (void)_;
    result.object_bytes += size;
  }

  result.repository_required_bytes = result.object_bytes + repository_download_margin_bytes;
  result.repository_available_bytes = available_space_bytes( repository_dir );
  if( result.repository_available_bytes < result.repository_required_bytes )
  {
    std::ostringstream message;
    message << "Public backup restore object download requires at least "
            << result.repository_required_bytes << " bytes free for local repository "
            << repository_dir.string() << "; available bytes: " << result.repository_available_bytes;
    result.download_skipped_reason = message.str();
    return;
  }

  uint64_t completed = 0;
  for( const auto& [sha256, size_bytes]: missing_objects )
  {
    throw_if_cancelled( options );
    const auto local_object = object_path( repository_dir, sha256 );
    const auto local_partial = partial_object_path( local_object );
    std::filesystem::create_directories( local_object.parent_path() );
    std::error_code ec;
    std::filesystem::remove( local_partial, ec );

    const auto relative = "objects/sha256/" + sha256.substr( 0, 2 ) + "/"
                        + sha256.substr( 2, 2 ) + "/" + sha256;
    emit_progress( options,
                   "public-restore-objects",
                   result.backup_id,
                   completed,
                   missing_objects.size(),
                   1,
                   missing_objects.size(),
                   result.object_bytes );
    client.download_relative( relative, local_partial, options );
    if( size_bytes != 0 && std::filesystem::file_size( local_partial ) != size_bytes )
      throw std::runtime_error( "downloaded public backup object size mismatch: " + local_partial.string() );
    const auto actual_sha256 = sha256_file( local_partial );
    if( actual_sha256 != sha256 )
      throw std::runtime_error( "downloaded public backup object checksum mismatch: " + local_partial.string() );
    std::filesystem::rename( local_partial, local_object );
    ++completed;
  }
  emit_progress( options,
                 "public-restore-objects",
                 result.backup_id,
                 completed,
                 missing_objects.size(),
                 1,
                 missing_objects.size(),
                 result.object_bytes );
  result.objects_fetched = true;
}

} // namespace

BackupSnapshotListResult list_public_backup_snapshots(
  const std::filesystem::path& repository_dir,
  const BackupPublicRestoreConfig& public_restore,
  const std::string& backup_id,
  const PublicRestoreOptions& options )
{
  PublicBackupClient client( public_restore );
  uint64_t metadata_count = 0;
  auto metadata = fetch_public_metadata( repository_dir,
                                         client,
                                         backup_id == "latest" ? std::string{} : backup_id,
                                         options,
                                         metadata_count );
  auto result = list_local_backup_snapshots( repository_dir );
  std::vector< BackupSnapshotSummary > snapshots;
  for( auto& snapshot: result.snapshots )
  {
    if( snapshot.backup_id != metadata.backup_id )
      continue;
    snapshot.latest = metadata.latest;
    snapshots.push_back( std::move( snapshot ) );
  }
  result.latest_backup_id = metadata.latest ? metadata.backup_id : result.latest_backup_id;
  result.remote_directory = client.base_url();
  result.snapshots = std::move( snapshots );
  return result;
}

PublicRestoreFetchResult fetch_public_restore_snapshot(
  const std::filesystem::path& repository_dir,
  const std::filesystem::path& target_basedir,
  const BackupPublicRestoreConfig& public_restore,
  const std::string& backup_id,
  const PublicRestoreOptions& options )
{
  PublicBackupClient client( public_restore );
  PublicRestoreFetchResult result;
  result.repository_dir = repository_dir;
  result.target_basedir = target_basedir;
  result.public_base_url = client.base_url();
  result.transport = public_restore.base_url.rfind( "file://", 0 ) == 0 ? "file" : "public-http";
  result.signature_required = public_restore.signature_required;

  const auto selected_backup_id = backup_id == "latest" ? std::string{} : backup_id;
  uint64_t metadata_count = 0;
  auto metadata = fetch_public_metadata( repository_dir,
                                         client,
                                         selected_backup_id,
                                         options,
                                         metadata_count );
  result.metadata_fetched = true;
  result.metadata_file_count = metadata_count;
  result.backup_id = metadata.backup_id;
  result.signature_verified = metadata.signature_verified;

  result.preflight = build_local_restore_preflight(
    repository_dir,
    target_basedir,
    selected_backup_id.empty() ? std::string{} : selected_backup_id );
  if( !result.preflight.space_check.passes_minimum )
  {
    result.download_skipped_reason = result.preflight.space_check.message;
    result.request_count = client.request_count();
    result.retry_count = client.retry_count();
    return result;
  }

  fetch_missing_objects( client, repository_dir, result.preflight, result, options );
  result.request_count = client.request_count();
  result.retry_count = client.retry_count();

  result.preflight = build_local_restore_preflight(
    repository_dir,
    target_basedir,
    selected_backup_id.empty() ? std::string{} : selected_backup_id );
  result.ready_to_stage = result.preflight.ready_to_restore;
  return result;
}

std::string public_restore_fetch_result_to_text( const PublicRestoreFetchResult& result )
{
  std::ostringstream out;
  out << "Fetched public backup restore data\n";
  out << "backup_id: " << result.backup_id << "\n";
  out << "repository_dir: " << result.repository_dir.string() << "\n";
  out << "target_basedir: " << result.target_basedir.string() << "\n";
  out << "public_base_url: " << result.public_base_url << "\n";
  out << "transport: " << result.transport << "\n";
  out << "metadata_fetched: " << ( result.metadata_fetched ? "true" : "false" ) << "\n";
  out << "metadata_file_count: " << result.metadata_file_count << "\n";
  out << "signature_required: " << ( result.signature_required ? "true" : "false" ) << "\n";
  out << "signature_verified: " << ( result.signature_verified ? "true" : "false" ) << "\n";
  out << "object_file_count: " << result.object_file_count << "\n";
  out << "object_bytes: " << result.object_bytes << "\n";
  out << "repository_available_bytes: " << result.repository_available_bytes << "\n";
  out << "repository_required_bytes: " << result.repository_required_bytes << "\n";
  out << "request_count: " << result.request_count << "\n";
  out << "retry_count: " << result.retry_count << "\n";
  out << "objects_fetched: " << ( result.objects_fetched ? "true" : "false" ) << "\n";
  out << "ready_to_stage: " << ( result.ready_to_stage ? "true" : "false" ) << "\n";
  out << "target_space_check: " << result.preflight.space_check.message << "\n";
  if( !result.download_skipped_reason.empty() )
    out << "download_skipped_reason: " << result.download_skipped_reason << "\n";
  return out.str();
}

std::string public_restore_fetch_result_to_json( const PublicRestoreFetchResult& result )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"backup_id\": \"" << json_escape( result.backup_id ) << "\",\n";
  out << "  \"repository_dir\": \"" << json_escape( result.repository_dir.string() ) << "\",\n";
  out << "  \"target_basedir\": \"" << json_escape( result.target_basedir.string() ) << "\",\n";
  out << "  \"public_base_url\": \"" << json_escape( result.public_base_url ) << "\",\n";
  out << "  \"transport\": \"" << json_escape( result.transport ) << "\",\n";
  out << "  \"metadata_fetched\": " << ( result.metadata_fetched ? "true" : "false" ) << ",\n";
  out << "  \"metadata_file_count\": " << result.metadata_file_count << ",\n";
  out << "  \"signature_required\": " << ( result.signature_required ? "true" : "false" ) << ",\n";
  out << "  \"signature_verified\": " << ( result.signature_verified ? "true" : "false" ) << ",\n";
  out << "  \"object_file_count\": " << result.object_file_count << ",\n";
  out << "  \"object_bytes\": " << result.object_bytes << ",\n";
  out << "  \"repository_available_bytes\": " << result.repository_available_bytes << ",\n";
  out << "  \"repository_required_bytes\": " << result.repository_required_bytes << ",\n";
  out << "  \"request_count\": " << result.request_count << ",\n";
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
