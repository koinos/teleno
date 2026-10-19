// SC0 diagnostic fixture. Never opens an operator database or starts a node.
#include "backup/checkpoint_manager.hpp"
#include "backup/public_restore.hpp"
#include "backup/snapshot_repository.hpp"
#include "../../tests/artifact_root.hpp"
#include "storage/rocksdb_manager.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/resource.h>
#include <sys/stat.h>
#include <vector>

#include <boost/system/system_error.hpp>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <rocksdb/version.h>
#include <rocksdb/write_batch.h>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace koinos::node;
using namespace koinos::node::backup;

namespace {
constexpr uint32_t rows = 2048;
constexpr uint32_t value_bytes = 1024;
constexpr uint32_t seed = 0x5c0001;
const std::array< storage::ColumnFamily, 3 > data_cfs = {
  storage::ColumnFamily::default_state, storage::ColumnFamily::contract_meta,
  storage::ColumnFamily::account_history
};

void require( bool condition, const std::string& message )
{
  if( !condition ) throw std::runtime_error( message );
}

std::string hex( const unsigned char* bytes, size_t length )
{
  std::ostringstream out;
  out << std::hex << std::setfill( '0' );
  for( size_t i = 0; i < length; ++i ) out << std::setw( 2 ) << unsigned( bytes[ i ] );
  return out.str();
}

class Digest
{
  std::unique_ptr< EVP_MD_CTX, decltype( &EVP_MD_CTX_free ) > ctx{ EVP_MD_CTX_new(), EVP_MD_CTX_free };
public:
  Digest() { require( ctx && EVP_DigestInit_ex( ctx.get(), EVP_sha256(), nullptr ) == 1, "SHA initialization" ); }
  void add( const std::string& bytes )
  { require( EVP_DigestUpdate( ctx.get(), bytes.data(), bytes.size() ) == 1, "SHA update" ); }
  std::string finish()
  {
    unsigned char bytes[ EVP_MAX_MD_SIZE ]; unsigned length = 0;
    require( EVP_DigestFinal_ex( ctx.get(), bytes, &length ) == 1, "SHA finish" );
    return hex( bytes, length );
  }
};

std::string hash_file( const fs::path& path )
{
  std::ifstream in( path, std::ios::binary );
  require( bool( in ), "cannot hash " + path.string() );
  Digest d; std::array< char, 64 * 1024 > buffer;
  while( in ) { in.read( buffer.data(), buffer.size() ); d.add( std::string( buffer.data(), size_t( in.gcount() ) ) ); }
  require( !in.bad(), "hash read failed" );
  return d.finish();
}

std::string read_text( const fs::path& path )
{
  std::ifstream in( path, std::ios::binary ); require( bool( in ), "cannot read " + path.string() );
  return { std::istreambuf_iterator< char >( in ), std::istreambuf_iterator< char >() };
}
void write_text( const fs::path& path, const std::string& bytes )
{
  fs::create_directories( path.parent_path() );
  std::ofstream out( path, std::ios::binary | std::ios::trunc );
  out.write( bytes.data(), bytes.size() ); out.close();
  require( bool( out ), "cannot write " + path.string() );
}
void write_json( const fs::path& path, const json& value ) { write_text( path, value.dump( 2 ) + "\n" ); }

fs::path new_output( const fs::path& requested )
{
  auto path = fs::weakly_canonical( fs::absolute( requested ) );
  auto root = test_artifact_root( TELENO_SC0_REPOSITORY_ROOT );
  require( test_artifact_child( path, root ), "output must be inside selected test evidence" );
  require( !fs::exists( path ), "output already exists; choose a new attempt" );
  fs::create_directories( path );
  write_text( path / ".sc0-owned", "synthetic fixture; no operator data\n" );
  return path;
}

json allocation( const fs::path& root )
{
  uint64_t logical = 0, allocated = 0, files = 0;
  std::set< std::pair< uint64_t, uint64_t > > inodes;
  if( fs::exists( root ) ) for( const auto& entry: fs::recursive_directory_iterator( root ) )
  {
    if( !entry.is_regular_file() ) continue;
    struct stat st{};
    require( ::stat( entry.path().c_str(), &st ) == 0, "stat failed" );
    ++files; logical += st.st_size;
    if( inodes.emplace( st.st_dev, st.st_ino ).second ) allocated += uint64_t( st.st_blocks ) * 512;
  }
  return { { "files", files }, { "logical_bytes", logical }, { "allocated_bytes", allocated },
           { "distinct_inodes", inodes.size() } };
}

NodeConfig config()
{
  NodeConfig cfg;
  cfg.rocksdb_compression = "none"; cfg.rocksdb_blocks_compression = "none";
  cfg.rocksdb_block_cache_mb = 16;
  cfg.backup.enabled = true; cfg.backup.node_id = "sc0-synthetic";
  cfg.backup.local.enabled = true;
  cfg.features[ "block_producer" ] = false;
  return cfg;
}

std::string key( uint32_t index )
{
  std::ostringstream out; out << "sc0/" << std::setw( 8 ) << std::setfill( '0' ) << index;
  return out.str();
}
std::string value( uint32_t cf, uint32_t index )
{
  uint32_t state = seed ^ ( ( cf + 1 ) * 0x9e3779b9U ) ^ index;
  std::string bytes( value_bytes, '\0' );
  for( char& byte: bytes ) { state ^= state << 13; state ^= state >> 17; state ^= state << 5; byte = char( state & 255 ); }
  return bytes;
}

std::string verify_contents( storage::RocksDBManager& manager )
{
  require( manager.column_family_count() == 9, "fixture CF count" );
  require( manager.read_metadata( "sc0.fixture" ) == "seed-0x5c0001-v1", "fixture metadata" );
  Digest digest;
  for( uint32_t cf = 0; cf < data_cfs.size(); ++cf ) for( uint32_t row = 0; row < rows; ++row )
  {
    std::string actual; const auto k = key( row );
    const auto status = manager.db()->Get( rocksdb::ReadOptions(), manager.handle( data_cfs[ cf ] ), k, &actual );
    require( status.ok() && actual == value( cf, row ), "fixture record mismatch" );
    // Fixed widths and fixed iteration order make the digest framing unambiguous.
    digest.add( std::to_string( cf ) ); digest.add( k ); digest.add( actual );
  }
  return digest.finish();
}

void sign_snapshot( const LocalSnapshotResult& snapshot, const fs::path& public_key )
{
  // Publicly known synthetic seed. This is not a producer or operator key.
  std::array< unsigned char, 32 > synthetic_seed{};
  for( size_t i = 0; i < synthetic_seed.size(); ++i ) synthetic_seed[ i ] = unsigned( i + 1 );
  std::unique_ptr< EVP_PKEY, decltype( &EVP_PKEY_free ) > signing(
    EVP_PKEY_new_raw_private_key( EVP_PKEY_ED25519, nullptr, synthetic_seed.data(), synthetic_seed.size() ), EVP_PKEY_free );
  require( bool( signing ), "fixture signing key" );
  std::unique_ptr< BIO, decltype( &BIO_free ) > bio( BIO_new( BIO_s_mem() ), BIO_free );
  require( bio && PEM_write_bio_PUBKEY( bio.get(), signing.get() ) == 1, "fixture public key" );
  char* pem = nullptr; const auto length = BIO_get_mem_data( bio.get(), &pem );
  require( length > 0, "empty public key" ); write_text( public_key, std::string( pem, length ) );

  auto manifest = json::parse( read_text( snapshot.manifest_path ) );
  manifest[ "public_bootstrap" ] = { { "network", "testnet" }, { "chain_id", "sc0-synthetic-not-a-chain" },
    { "producer_mode", false }, { "sanitized_config", true } };
  write_json( snapshot.manifest_path, manifest );
  const auto metadata_path = snapshot.snapshot_dir / "public-bootstrap.json";
  write_json( metadata_path, { { "format", "teleno-public-bootstrap-snapshot" }, { "version", 1 },
    { "backup_id", snapshot.backup_id }, { "network", "testnet" }, { "producer_mode", false } } );
  auto latest = json::parse( read_text( snapshot.latest_path ) );
  latest[ "public_metadata" ] = "snapshots/" + snapshot.backup_id + "/public-bootstrap.json";
  latest[ "signature" ] = "snapshots/" + snapshot.backup_id + "/public-bootstrap-signature.json";
  write_json( snapshot.latest_path, latest );
  const json payload = { { "backup_id", snapshot.backup_id }, { "network", "testnet" },
    { "chain_id", "sc0-synthetic-not-a-chain" }, { "manifest_sha256", hash_file( snapshot.manifest_path ) },
    { "files_sha256", hash_file( snapshot.files_path ) }, { "latest_sha256", hash_file( snapshot.latest_path ) },
    { "public_metadata_sha256", hash_file( metadata_path ) }, { "object_count", snapshot.object_count },
    { "total_bytes", snapshot.total_bytes } };
  const auto message = payload.dump();
  std::unique_ptr< EVP_MD_CTX, decltype( &EVP_MD_CTX_free ) > context( EVP_MD_CTX_new(), EVP_MD_CTX_free );
  require( context && EVP_DigestSignInit( context.get(), nullptr, nullptr, nullptr, signing.get() ) == 1, "sign init" );
  std::array< unsigned char, 64 > signature{}; size_t size = signature.size();
  require( EVP_DigestSign( context.get(), signature.data(), &size,
    reinterpret_cast< const unsigned char* >( message.data() ), message.size() ) == 1, "sign failed" );
  write_json( snapshot.snapshot_dir / "public-bootstrap-signature.json",
    { { "format", "teleno-public-bootstrap-signature" }, { "version", 1 }, { "algorithm", "ed25519" },
      { "key_id", "sc0-publicly-known-fixture-key" }, { "payload", payload }, { "signature_hex", hex( signature.data(), size ) } } );
}

void prepare( const fs::path& output )
{
  auto cfg = config(); cfg.backup.local.directory = ( output / "public" ).string();
  const auto node = output / "source-node";
  write_text( node / "config.yml", "features:\n  block_producer: false\n" );
  write_text( node / "chain/genesis_data.json", "{\"sc0_synthetic\":true}\n" );
  storage::RocksDBManager manager; manager.open( node, cfg );
  manager.write_metadata( "sc0.fixture", "seed-0x5c0001-v1" );
  rocksdb::WriteOptions writes; writes.sync = true;
  rocksdb::WriteBatch batch;
  for( uint32_t cf = 0; cf < data_cfs.size(); ++cf ) for( uint32_t row = 0; row < rows; ++row )
    require( batch.Put( manager.handle( data_cfs[ cf ] ), key( row ), value( cf, row ) ).ok(), "batch put failed" );
  require( manager.db()->Write( writes, &batch ).ok(), "fixture write failed" );
  std::vector< rocksdb::ColumnFamilyHandle* > handles;
  for( int cf = 0; cf < 9; ++cf ) handles.push_back( manager.handle( storage::ColumnFamily( cf ) ) );
  rocksdb::FlushOptions flush; flush.wait = true;
  require( manager.db()->Flush( flush, handles ).ok(), "fixture flush failed" );
  const auto logical_digest = verify_contents( manager );
  CheckpointManager checkpoints( node, manager );
  const auto checkpoint = checkpoints.create_checkpoint( output / "checkpoint" );
  LocalSnapshotRepository repository( output / "public" );
  const auto snapshot = repository.store_checkpoint_snapshot( checkpoint, cfg, node, node / "config.yml" );
  manager.close();
  sign_snapshot( snapshot, output / "fixture-signing.pub" );
  write_json( output / "fixture.json", { { "format", "teleno-sc0-fixture-v2" }, { "seed", seed },
    { "populated_cf_names", {"default","contract_meta","account_history"} },
    { "rows_per_populated_cf", rows }, { "value_bytes", value_bytes }, { "populated_cfs", 3 }, { "column_families", 9 },
    { "logical_data_sha256", logical_digest }, { "backup_id", snapshot.backup_id },
    { "manifest_sha256", hash_file( snapshot.manifest_path ) }, { "files_sha256", hash_file( snapshot.files_path ) },
    { "snapshot_payload_bytes", snapshot.total_bytes }, { "objects", snapshot.object_count },
    { "rocksdb_version", std::to_string( ROCKSDB_MAJOR ) + "." + std::to_string( ROCKSDB_MINOR ) + "." + std::to_string( ROCKSDB_PATCH ) },
    { "compression", "none" }, { "block_cache_mb", 16 }, { "chain_validated", false },
    { "note", "Valid multi-CF RocksDB fixture with synthetic records; not a Koinos genesis or replay fixture" } } );
}

void restore( const fs::path& source, const fs::path& output, const std::string& url )
{
  require( test_artifact_child( source, test_artifact_root( TELENO_SC0_REPOSITORY_ROOT ) ),
           "source must be an SC0 fixture within selected test evidence" );
  require( fs::is_regular_file( source / ".sc0-owned" ), "source is not an SC0 fixture" );
  const std::string loopback_prefix = "http://127.0.0.1:";
  bool local_http = false;
  if( url.rfind( loopback_prefix, 0 ) == 0 )
  {
    const auto slash = url.find( '/', loopback_prefix.size() );
    const auto port = url.substr( loopback_prefix.size(), slash - loopback_prefix.size() );
    local_http = slash != std::string::npos && !port.empty() && port.size() <= 5
      && port.find_first_not_of( "0123456789" ) == std::string::npos
      && std::stoul( port ) > 0 && std::stoul( port ) <= 65535
      && ( url.substr( slash ) == "/valid" || url.substr( slash ) == "/tampered"
           || url.substr( slash ) == "/truncated" );
  }
  require( url == "file://" + ( source / "public" ).string() || local_http,
           "source URL must be this file fixture or the bounded loopback harness" );
  const auto fixture = json::parse( read_text( source / "fixture.json" ) );
  require(fixture.at("format")=="teleno-sc0-fixture-v2","use a freshly prepared SC0 v2 fixture; prior measurements remain archived");
  const auto repo = output / "downloaded-repository"; const auto target = output / "target";
  const auto started = std::chrono::steady_clock::now(); json phases = json::array();
  auto sample = [&]( const std::string& phase ) {
    phases.push_back( { { "phase", phase }, { "elapsed_seconds", std::chrono::duration< double >( std::chrono::steady_clock::now() - started ).count() },
      { "repository", allocation( repo ) }, { "target", allocation( target ) },
      { "available_bytes", fs::space( output ).available } } );
  };
  sample( "before_fetch" );
  BackupPublicRestoreConfig public_cfg;
  public_cfg.enabled = true; public_cfg.base_url = url; public_cfg.network = "testnet";
  public_cfg.require_https = url.rfind( "file://", 0 ) == 0; public_cfg.timeout_seconds = 2; public_cfg.retries = 1;
  public_cfg.signature_required = true; public_cfg.signature_public_key_file = ( source / "fixture-signing.pub" ).string();
  const auto fetch = fetch_public_restore_snapshot( repo, target, public_cfg, fixture.at( "backup_id" ).get< std::string >() );
  require( fetch.signature_verified && fetch.ready_to_stage, "signed fixture fetch not ready" );
  sample( "after_fetch" );
  const auto inventory = json::parse( read_text( fetch.preflight.files_path ) );
  auto stage = stage_local_restore_snapshot( repo, target, fetch.backup_id, {},
    [&]( const RestoreStageProgress& ) { sample( "staging_progress" ); } );
  sample( "after_stage" );
  json verified = json::array(); uint64_t duplicated_bytes = 0;
  for( const auto& file: inventory.at( "files" ) )
  {
    const auto relative = file.at( "path" ).get< std::string >(); const auto sha = file.at( "sha256" ).get< std::string >();
    const auto object = repo / "objects/sha256" / sha.substr( 0, 2 ) / sha.substr( 2, 2 ) / sha;
    const auto restored = stage.staging_dir / relative;
    require( hash_file( object ) == sha && hash_file( restored ) == sha, "copied payload hash mismatch" );
    struct stat a{}, b{}; require( ::stat( object.c_str(), &a ) == 0 && ::stat( restored.c_str(), &b ) == 0, "payload stat" );
    require( a.st_dev != b.st_dev || a.st_ino != b.st_ino, "legacy copy unexpectedly shares inode" );
    duplicated_bytes += fs::file_size( restored );
    verified.push_back( { { "path", relative }, { "sha256", sha }, { "size_bytes", file.at( "size_bytes" ) },
      { "repository_inode", a.st_ino }, { "stage_inode", b.st_ino }, { "stage_allocated_bytes", uint64_t( b.st_blocks ) * 512 } } );
  }
  struct stat staged_current{}; require( ::stat( ( stage.staging_dir / "db/CURRENT" ).c_str(), &staged_current ) == 0, "stage CURRENT" );
  const auto activation = activate_staged_restore_snapshot( stage.staging_dir, target );
  sample( "after_activate" );
  struct stat active_current{}; require( ::stat( ( target / "db/CURRENT" ).c_str(), &active_current ) == 0, "active CURRENT" );
  require( staged_current.st_dev == active_current.st_dev && staged_current.st_ino == active_current.st_ino, "activation was not same-filesystem rename" );
  require( activation.preserved_paths.empty(), "fresh-target experiment unexpectedly preserved old paths" );
  require( fs::exists( target / ".backup-just-restored" ), "observer restore marker missing" );
  auto cfg = config(); storage::RocksDBManager reopened; reopened.open( target, cfg );
  const auto contents = verify_contents( reopened ); reopened.close(); sample( "after_reopen" );
  require( contents == fixture.at( "logical_data_sha256" ).get< std::string >(), "logical digest after reopen differs" );
  rusage usage{}; require( getrusage( RUSAGE_SELF, &usage ) == 0, "rusage failed" );
#if defined( __APPLE__ )
  const uint64_t peak_rss = usage.ru_maxrss;
#else
  const uint64_t peak_rss = uint64_t( usage.ru_maxrss ) * 1024;
#endif
  write_json( output / "restore.json", { { "format", "teleno-sc0-restore-baseline-v1" }, { "backup_id", fetch.backup_id },
    { "signature_verified", fetch.signature_verified }, { "transport", fetch.transport },
    { "manifest_sha256", hash_file( fetch.preflight.manifest_path ) }, { "files_sha256", hash_file( fetch.preflight.files_path ) },
    { "duplicated_payload_bytes", duplicated_bytes }, { "verified_files", verified }, { "phases", phases },
    { "logical_data_sha256", contents }, { "peak_rss_bytes", peak_rss }, { "elapsed_seconds", std::chrono::duration< double >( std::chrono::steady_clock::now() - started ).count() },
    { "activation_same_inode", true }, { "observer_restore_marker", true }, { "node_started", false },
    { "allocation_note", "st_blocks counts distinct inodes; shared APFS extents and unrelated volume activity are not attributed" } } );
}
} // namespace

int main( int argc, char** argv )
{
  try
  {
    // Exercise dependency error formatting in this executable as well as the node.
    // Its static-library linkage differs from the full runtime's linkage.
    const boost::system::system_error dependency_error(
      boost::system::error_code( 1, boost::system::generic_category() ) );
    require( std::string( dependency_error.what() ).find( "generic:1" ) != std::string::npos,
             "dependency error reporting is incompatible with this fixture build" );
    require( argc == 3 || argc == 5, "usage: fixture prepare OUTPUT | fixture restore SOURCE OUTPUT URL" );
    if( std::string( argv[ 1 ] ) == "prepare" && argc == 3 ) prepare( new_output( argv[ 2 ] ) );
    else if( std::string( argv[ 1 ] ) == "restore" && argc == 5 ) restore( fs::canonical( argv[ 2 ] ), new_output( argv[ 3 ] ), argv[ 4 ] );
    else throw std::runtime_error( "invalid fixture command" );
    return 0;
  }
  catch( const std::exception& error ) { std::cerr << "SC0 fixture failed: " << error.what() << '\n'; return 1; }
}
