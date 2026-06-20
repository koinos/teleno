#include "backup/public_restore.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/sha.h>

using namespace koinos::node::backup;
using koinos::node::BackupPublicRestoreConfig;

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

std::string json_string( const nlohmann::json& value )
{
  return value.dump( 2 ) + "\n";
}

std::string bytes_to_hex( const unsigned char* data, unsigned int size )
{
  static const char* hex = "0123456789abcdef";
  std::string out;
  out.reserve( size * 2 );
  for( unsigned int i = 0; i < size; ++i )
  {
    out.push_back( hex[ ( data[ i ] >> 4 ) & 0xf ] );
    out.push_back( hex[ data[ i ] & 0xf ] );
  }
  return out;
}

std::string sha256_string( const std::string& value )
{
  unsigned char digest[ SHA256_DIGEST_LENGTH ];
  SHA256( reinterpret_cast< const unsigned char* >( value.data() ), value.size(), digest );
  return bytes_to_hex( digest, SHA256_DIGEST_LENGTH );
}

void require( bool ok, const std::string& message )
{
  if( !ok )
    throw std::runtime_error( message );
}

struct SigningFixture
{
  std::string public_key_pem;
  std::string private_key_pem;
};

SigningFixture generate_ed25519_fixture()
{
  EVP_PKEY_CTX* raw_keygen = EVP_PKEY_CTX_new_id( EVP_PKEY_ED25519, nullptr );
  require( raw_keygen != nullptr, "failed to allocate Ed25519 keygen context" );
  std::unique_ptr< EVP_PKEY_CTX, decltype( &EVP_PKEY_CTX_free ) > keygen( raw_keygen, EVP_PKEY_CTX_free );
  require( EVP_PKEY_keygen_init( keygen.get() ) == 1, "failed to initialize Ed25519 keygen" );
  EVP_PKEY* raw_key = nullptr;
  require( EVP_PKEY_keygen( keygen.get(), &raw_key ) == 1, "failed to generate Ed25519 key" );
  std::unique_ptr< EVP_PKEY, decltype( &EVP_PKEY_free ) > key( raw_key, EVP_PKEY_free );

  BIO* raw_public = BIO_new( BIO_s_mem() );
  BIO* raw_private = BIO_new( BIO_s_mem() );
  require( raw_public && raw_private, "failed to allocate Ed25519 key BIO" );
  std::unique_ptr< BIO, decltype( &BIO_free ) > public_bio( raw_public, BIO_free );
  std::unique_ptr< BIO, decltype( &BIO_free ) > private_bio( raw_private, BIO_free );
  require( PEM_write_bio_PUBKEY( public_bio.get(), key.get() ) == 1, "failed to write Ed25519 public key PEM" );
  require( PEM_write_bio_PrivateKey( private_bio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr ) == 1,
           "failed to write Ed25519 private key PEM" );
  require( BIO_flush( public_bio.get() ) == 1, "failed to flush public key BIO" );
  require( BIO_flush( private_bio.get() ) == 1, "failed to flush private key BIO" );

  BUF_MEM* public_mem = nullptr;
  BUF_MEM* private_mem = nullptr;
  BIO_get_mem_ptr( public_bio.get(), &public_mem );
  BIO_get_mem_ptr( private_bio.get(), &private_mem );
  require( public_mem && public_mem->length > 0, "generated public key PEM is empty" );
  require( private_mem && private_mem->length > 0, "generated private key PEM is empty" );
  return {
    std::string( public_mem->data, public_mem->length ),
    std::string( private_mem->data, private_mem->length )
  };
}

std::string sign_ed25519_hex( const std::string& private_key_pem, const std::string& message )
{
  BIO* raw_bio = BIO_new_mem_buf( private_key_pem.data(), static_cast< int >( private_key_pem.size() ) );
  require( raw_bio != nullptr, "failed to allocate private key BIO" );
  std::unique_ptr< BIO, decltype( &BIO_free ) > bio( raw_bio, BIO_free );
  EVP_PKEY* raw_key = PEM_read_bio_PrivateKey( bio.get(), nullptr, nullptr, nullptr );
  require( raw_key != nullptr, "failed to read private key PEM" );
  std::unique_ptr< EVP_PKEY, decltype( &EVP_PKEY_free ) > key( raw_key, EVP_PKEY_free );

  EVP_MD_CTX* raw_ctx = EVP_MD_CTX_new();
  require( raw_ctx != nullptr, "failed to allocate Ed25519 signing context" );
  std::unique_ptr< EVP_MD_CTX, decltype( &EVP_MD_CTX_free ) > ctx( raw_ctx, EVP_MD_CTX_free );
  require( EVP_DigestSignInit( ctx.get(), nullptr, nullptr, nullptr, key.get() ) == 1,
           "failed to initialize Ed25519 signing" );
  std::size_t sig_len = 0;
  require( EVP_DigestSign( ctx.get(),
                           nullptr,
                           &sig_len,
                           reinterpret_cast< const unsigned char* >( message.data() ),
                           message.size() ) == 1,
           "failed to size Ed25519 signature" );
  std::vector< unsigned char > signature( sig_len );
  require( EVP_DigestSign( ctx.get(),
                           signature.data(),
                           &sig_len,
                           reinterpret_cast< const unsigned char* >( message.data() ),
                           message.size() ) == 1,
           "failed to sign Ed25519 payload" );
  return bytes_to_hex( signature.data(), static_cast< unsigned int >( sig_len ) );
}

std::filesystem::path object_path( const std::filesystem::path& repo, const std::string& sha256 )
{
  return repo / "objects" / "sha256" / sha256.substr( 0, 2 ) / sha256.substr( 2, 2 ) / sha256;
}

struct FileSpec
{
  std::string path;
  std::string content;
  bool runtime_file = false;
};

void write_public_snapshot( const std::filesystem::path& repo,
                            const std::string& backup_id,
                            const std::vector< FileSpec >& files,
                            const SigningFixture* signing = nullptr,
                            const std::filesystem::path& public_key_file = {} )
{
  uint64_t db_bytes = 0;
  uint64_t runtime_bytes = 0;
  uint64_t total_bytes = 0;

  std::string files_json =
    "{\n"
    "  \"format\": \"teleno-native-snapshot-files\",\n"
    "  \"version\": 1,\n"
    "  \"backup_id\": \"" + backup_id + "\",\n"
    "  \"files\": [\n";

  for( std::size_t i = 0; i < files.size(); ++i )
  {
    const auto& file = files[ i ];
    const auto sha = sha256_string( file.content );
    write_file( object_path( repo, sha ), file.content );
    total_bytes += file.content.size();
    if( file.runtime_file )
      runtime_bytes += file.content.size();
    else
      db_bytes += file.content.size();

    files_json += "    { \"path\": \"" + file.path
               + "\", \"sha256\": \"" + sha
               + "\", \"size_bytes\": " + std::to_string( file.content.size() )
               + ", \"runtime_file\": " + ( file.runtime_file ? "true" : "false" ) + " }";
    if( i + 1 != files.size() )
      files_json += ",";
    files_json += "\n";
  }
  files_json += "  ]\n}\n";

  const auto snapshot_dir = repo / "snapshots" / backup_id;
  const std::string manifest_json =
              "{\n"
              "  \"format\": \"teleno-native-rocksdb-snapshot\",\n"
              "  \"version\": 1,\n"
              "  \"backup_id\": \"" + backup_id + "\",\n"
              "  \"created_at\": \"20260619T000000Z\",\n"
              "  \"node\": { \"name\": \"teleno_node\", \"version\": \"test\" },\n"
              "  \"source\": { \"basedir\": \"/tmp/source\", \"node_id\": \"public-test\", \"storage_layout\": \"unified\", \"chain_id\": \"test-chain\" },\n"
              "  \"snapshot\": { \"file_count\": " + std::to_string( files.size() )
                + ", \"object_count\": " + std::to_string( files.size() )
                + ", \"total_bytes\": " + std::to_string( total_bytes ) + " },\n"
              "  \"sizes\": {\n"
              "    \"restored_database_bytes\": " + std::to_string( db_bytes ) + ",\n"
              "    \"runtime_files_bytes\": " + std::to_string( runtime_bytes ) + ",\n"
              "    \"object_download_bytes\": " + std::to_string( total_bytes ) + ",\n"
              "    \"archive_bytes\": 0\n"
              "  },\n"
              "  \"restore\": { \"start_as_observer_first\": true, \"force_block_producer_disabled_on_first_start\": true }\n"
              "}\n";
  const auto public_metadata_json = json_string( nlohmann::json{
    { "format", "teleno-public-bootstrap-snapshot" },
    { "version", 1 },
    { "network", "testnet" },
    { "backup_id", backup_id },
    { "source_backup_id", backup_id },
    { "public_base_url", "file://" + repo.string() },
    { "promoted_at", "20260619T000000Z" },
    { "sanitized_config_sha256", "config-sha" },
    { "file_count", files.size() },
    { "object_count", files.size() },
    { "total_bytes", total_bytes },
    { "producer_mode", false }
  } );

  nlohmann::json latest = {
    { "format", "teleno-native-latest-snapshot" },
    { "version", 1 },
    { "backup_id", backup_id },
    { "snapshot_dir", backup_id },
    { "manifest", "snapshots/" + backup_id + "/manifest.json" },
    { "files", "snapshots/" + backup_id + "/files.json" },
    { "public_metadata", "snapshots/" + backup_id + "/public-bootstrap.json" }
  };
  if( signing )
    latest[ "signature" ] = "snapshots/" + backup_id + "/public-bootstrap-signature.json";
  const auto latest_json = json_string( latest );

  write_file( snapshot_dir / "files.json", files_json );
  write_file( snapshot_dir / "manifest.json", manifest_json );
  write_file( snapshot_dir / "public-bootstrap.json", public_metadata_json );
  if( signing )
  {
    write_file( public_key_file, signing->public_key_pem );
    const nlohmann::json payload = {
      { "format", "teleno-public-bootstrap-signature-payload" },
      { "version", 1 },
      { "algorithm", "ed25519" },
      { "backup_id", backup_id },
      { "network", "testnet" },
      { "chain_id", "test-chain" },
      { "public_base_url", "file://" + repo.string() },
      { "latest_sha256", sha256_string( latest_json ) },
      { "manifest_sha256", sha256_string( manifest_json ) },
      { "files_sha256", sha256_string( files_json ) },
      { "public_metadata_sha256", sha256_string( public_metadata_json ) },
      { "object_count", files.size() },
      { "total_bytes", total_bytes },
      { "sanitized_config_sha256", "config-sha" },
      { "signed_at", "20260619T000000Z" }
    };
    const auto signature_hex = sign_ed25519_hex( signing->private_key_pem, payload.dump() );
    const nlohmann::json envelope = {
      { "format", "teleno-public-bootstrap-signature" },
      { "version", 1 },
      { "algorithm", "ed25519" },
      { "key_id", "unit-test-key" },
      { "public_key_sha256", "unit-test-public-key-sha" },
      { "payload", payload },
      { "signature_hex", signature_hex }
    };
    write_file( snapshot_dir / "public-bootstrap-signature.json", json_string( envelope ) );
  }
  write_file( snapshot_dir / "COMPLETE", "complete\n" );
  write_file( repo / "latest.json", latest_json );
}

} // namespace

int main()
{
  std::string phase = "start";
  try
  {
  {
    phase = "fixture";
    auto root = unique_temp_dir( "teleno-public-restore" );
    auto public_repo = root / "public";
    auto local_repo = root / "local-repo";
    auto target = root / "target";
    const std::string backup_id = "20260619T000000Z-ms-1-files-4";
    write_public_snapshot(
      public_repo,
      backup_id,
      {
        { "db/CURRENT", "CURRENT\n", false },
        { "config.yml", "features:\n  block_producer: true\n", true },
        { "chain/genesis_data.json", "{\"genesis\":true}\n", true },
        { "jsonrpc/descriptors/koinos_descriptors.pb", "descriptor", true }
      } );

    BackupPublicRestoreConfig cfg;
    cfg.enabled = true;
    cfg.base_url = "file://" + public_repo.string();
    cfg.require_https = true;
    cfg.retries = 1;

    phase = "list";
    auto list = list_public_backup_snapshots( local_repo, cfg );
    assert( list.latest_backup_id == backup_id );
    assert( list.snapshots.size() == 1 );
    assert( list.snapshots[ 0 ].backup_id == backup_id );
    assert( list.snapshots[ 0 ].latest );
    assert( list.snapshots[ 0 ].file_count == 4 );
    assert( !std::filesystem::exists( local_repo / "objects" ) );

    auto exact_local_repo = root / "exact-local-repo";
    auto exact_list = list_public_backup_snapshots( exact_local_repo, cfg, backup_id );
    assert( exact_list.snapshots.size() == 1 );
    assert( exact_list.snapshots[ 0 ].backup_id == backup_id );
    assert( !exact_list.snapshots[ 0 ].latest );

    phase = "fetch";
    auto fetch = fetch_public_restore_snapshot( local_repo, target, cfg );
    assert( fetch.backup_id == backup_id );
    assert( fetch.metadata_fetched );
    assert( fetch.objects_fetched );
    assert( fetch.object_file_count == 4 );
    assert( fetch.ready_to_stage );
    assert( fetch.preflight.missing_object_count == 0 );
    assert( public_restore_fetch_result_to_text( fetch ).find( "Fetched public backup restore data" ) != std::string::npos );
    assert( public_restore_fetch_result_to_json( fetch ).find( "\"transport\": \"file\"" ) != std::string::npos );

    phase = "selected";
    auto selected = fetch_public_restore_snapshot( local_repo, target, cfg, backup_id );
    assert( selected.backup_id == backup_id );
    assert( selected.ready_to_stage );
    assert( selected.object_file_count == 0 );

    std::filesystem::remove_all( root );
  }

  {
    phase = "signed";
    auto root = unique_temp_dir( "teleno-public-restore-signed" );
    auto public_repo = root / "public";
    auto local_repo = root / "local-repo";
    auto target = root / "target";
    auto public_key_file = root / "public-bootstrap.pub";
    const auto signing = generate_ed25519_fixture();
    const std::string backup_id = "20260619T010000Z-ms-2-files-4";
    write_public_snapshot(
      public_repo,
      backup_id,
      {
        { "db/CURRENT", "CURRENT\n", false },
        { "config.yml", "features:\n  block_producer: false\n", true },
        { "chain/genesis_data.json", "{\"genesis\":true}\n", true },
        { "jsonrpc/descriptors/koinos_descriptors.pb", "descriptor", true }
      },
      &signing,
      public_key_file );

    BackupPublicRestoreConfig cfg;
    cfg.enabled = true;
    cfg.base_url = "file://" + public_repo.string();
    cfg.require_https = true;
    cfg.retries = 1;
    cfg.signature_required = true;
    cfg.signature_public_key_file = public_key_file.string();

    auto list = list_public_backup_snapshots( local_repo, cfg );
    require( list.latest_backup_id == backup_id, "signed public list returned unexpected latest backup ID" );
    auto fetch = fetch_public_restore_snapshot( local_repo, target, cfg );
    require( fetch.signature_required, "signed public fetch did not report required signature" );
    require( fetch.signature_verified, "signed public fetch did not verify signature" );
    require( fetch.ready_to_stage, "signed public fetch was not ready to stage" );

    auto tampered_repo = root / "tampered-public";
    std::filesystem::copy( public_repo,
                           tampered_repo,
                           std::filesystem::copy_options::recursive );
    write_file( tampered_repo / "snapshots" / backup_id / "public-bootstrap.json",
                "{\"format\":\"teleno-public-bootstrap-snapshot\",\"network\":\"testnet\",\"backup_id\":\""
                + backup_id + "\",\"total_bytes\":1}\n" );
    cfg.base_url = "file://" + tampered_repo.string();
    bool threw = false;
    try
    {
      (void)list_public_backup_snapshots( root / "tampered-local-repo", cfg );
    }
    catch( const std::runtime_error& e )
    {
      threw = std::string( e.what() ).find( "metadata hash mismatch" ) != std::string::npos;
    }
    require( threw, "tampered signed public metadata did not fail verification" );

    std::filesystem::remove_all( root );
  }

  {
    phase = "require-https";
    BackupPublicRestoreConfig cfg;
    cfg.enabled = true;
    cfg.base_url = "http://example.invalid/backups";
    cfg.require_https = true;

    bool threw = false;
    try
    {
      (void)list_public_backup_snapshots( "/tmp/teleno-public-restore-invalid", cfg );
    }
    catch( const std::runtime_error& e )
    {
      threw = std::string( e.what() ).find( "requires an https base-url" ) != std::string::npos;
    }
    assert( threw );
  }

  return 0;
  }
  catch( const std::exception& e )
  {
    std::cerr << "phase: " << phase << " error: " << e.what() << "\n";
    throw;
  }
}
