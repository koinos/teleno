#include "backup/direct_restore.hpp"
#include "../artifact_root.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>

using namespace koinos::node;
using namespace koinos::node::backup;
namespace fs = std::filesystem;
using json = nlohmann::json;
namespace {
unsigned checks = 0;
void check( bool value ) { ++checks; if( !value ) throw std::runtime_error( "SC2 assertion failed" ); }
template< typename F > void rejects( F action )
{ bool failed = false; try { action(); } catch( const std::exception& ) { failed = true; } check( failed ); }
void write( const fs::path& p, const std::string& bytes )
{ std::ofstream out( p ); out << bytes; out.close(); check( bool( out ) ); }
json read( const fs::path& p ) { std::ifstream in( p ); return json::parse( in ); }
std::string make_object( const fs::path& source, uint64_t size, char fill )
{
  std::array< char, 64 * 1024 > buffer; buffer.fill( fill );
  std::unique_ptr< EVP_MD_CTX, decltype( &EVP_MD_CTX_free ) > hash( EVP_MD_CTX_new(), EVP_MD_CTX_free );
  check( hash && EVP_DigestInit_ex( hash.get(), EVP_sha256(), nullptr ) == 1 );
  const auto temp = source / ( std::string( "input-" ) + fill ); std::ofstream out( temp, std::ios::binary );
  for( uint64_t done = 0; done < size; )
  {
    const auto count = size_t( std::min< uint64_t >( buffer.size(), size - done ) );
    out.write( buffer.data(), count ); check( EVP_DigestUpdate( hash.get(), buffer.data(), count ) == 1 ); done += count;
  }
  out.close(); check( bool( out ) );
  unsigned char bytes[ EVP_MAX_MD_SIZE ]; unsigned length = 0; check( EVP_DigestFinal_ex( hash.get(), bytes, &length ) == 1 );
  std::ostringstream text; text << std::hex << std::setfill( '0' );
  for( unsigned i = 0; i < length; ++i ) text << std::setw( 2 ) << unsigned( bytes[ i ] );
  const auto digest = text.str(); const auto path = source / "objects/sha256" / digest.substr( 0, 2 ) / digest.substr( 2, 2 ) / digest;
  fs::create_directories( path.parent_path() ); fs::rename( temp, path ); return digest;
}
uint64_t inode( const fs::path& p ) { struct stat s{}; check( ::stat( p.c_str(), &s ) == 0 ); return s.st_ino; }
} // namespace

int main()
{
  try
  {
    const auto repo = fs::canonical( TELENO_SC2_REPOSITORY_ROOT );
    const auto evidence = test_artifact_root( repo );
    const auto root = evidence / ( "sc2-unit-" + std::to_string( std::random_device{}() ) ); check( fs::create_directory( root ) );
    const auto source = root / "source"; check( fs::create_directory( source ) );
    const uint64_t large_size = 64ULL * 1024 * 1024;
    const auto current = make_object( source, 17, 'a' ), large = make_object( source, large_size, 'b' ), genesis = make_object( source, 10, 'c' );
    const auto empty = make_object( source, 0, 'd' );
    json files = json::array( {
      { { "path", "db/CURRENT" }, { "sha256", current }, { "size_bytes", 17 }, { "runtime_file", false } },
      { { "path", "db/000001.sst" }, { "sha256", large }, { "size_bytes", large_size }, { "runtime_file", false } },
      { { "path", "db/000002.sst" }, { "sha256", large }, { "size_bytes", large_size }, { "runtime_file", false } },
      { { "path", "db/EMPTY" }, { "sha256", empty }, { "size_bytes", 0 }, { "runtime_file", false } },
      { { "path", "chain/genesis_data.json" }, { "sha256", genesis }, { "size_bytes", 10 }, { "runtime_file", true } } } );
    json manifest = { { "format", "teleno-native-rocksdb-snapshot" }, { "version", 1 }, { "backup_id", "unit-1" },
      { "source", { { "storage_layout", "unified" } } }, { "public_bootstrap", { { "network", "testnet" }, { "chain_id", "synthetic" } } },
      { "snapshot", { { "file_count", 5 }, { "object_count", 4 }, { "total_bytes", large_size * 2 + 27 } } },
      { "sizes", { { "restored_database_bytes", large_size * 2 + 17 }, { "runtime_files_bytes", 10 },
        { "object_download_bytes", large_size + 27 }, { "archive_bytes", 0 } } } };
    RestoreMetadataInput input; input.backup_id = "unit-1"; input.source_identity = "file://" + source.string();
    input.expected_network = "testnet"; input.expected_chain_id = "synthetic"; input.manifest = manifest.dump();
    input.inventory = json{ { "format", "teleno-native-snapshot-files" }, { "version", 1 }, { "backup_id", "unit-1" }, { "files", files } }.dump();
    const auto spec = validate_restore_metadata( input );
    BackupPublicRestoreConfig config; config.base_url = input.source_identity; config.require_https = false; config.retries = 1;
    const auto target = root / "target", stage = root / "attempt";
    // The experiment changes headroom, not the payload or the 20% term. An
    // attempt cannot change its explicit profile during resume or activation.
    DirectStageOptions experimental; experimental.reserve_profile = RestoreReserveProfile::experimental_8gib;
    RestoreBudgetPolicy experiment_policy; experiment_policy.reserve_profile = experimental.reserve_profile;
    const auto experiment_budget = estimate_mode_restore_space( spec, RestoreMode::direct, experiment_policy );
    experimental.available_bytes = [=]( const auto& ) { return experiment_budget.recommended_free_bytes - 1; };
    const auto experiment_attempt = root / "experimental-attempt";
    rejects( [&] { stage_direct_restore_snapshot( spec, config, target, experiment_attempt, false, experimental ); } );
    check( !fs::exists( experiment_attempt ) );
    // Stop immediately after the descriptor, avoiding another large fixture.
    experimental.available_bytes = [=]( const auto& ) { return experiment_budget.recommended_free_bytes; };
    experimental.checkpoint = []( const auto& phase, uint64_t ) {
      if( phase == "after_descriptor" ) throw std::runtime_error( "stop after experimental descriptor" );
    };
    rejects( [&] { stage_direct_restore_snapshot( spec, config, target, experiment_attempt, false, experimental ); } );
    check( read( experiment_attempt / "ATTEMPT.json" ).at( "recovery_reserve_profile" ) == "experimental-8gib" );
    rejects( [&] { stage_direct_restore_snapshot( spec, config, target, experiment_attempt, true ); } );
    auto first = stage_direct_restore_snapshot( spec, config, target, stage );
    check( first.download_complete && first.downloaded_files == 5 && first.reused_files == 0 );
    check( first.attempt_id.size() == 32 );
    check( first.transferred_bytes == large_size * 2 + 27 && first.payload_bytes == first.transferred_bytes );
    check( !fs::exists( target ) && !fs::exists( stage / "objects" ) && fs::is_empty( stage / "partial" ) );
    check( inode( stage / "payload/db/000001.sst" ) != inode( stage / "payload/db/000002.sst" ) );
    const auto source_large = source / "objects/sha256" / large.substr( 0, 2 ) / large.substr( 2, 2 ) / large;
    check( inode( source_large ) != inode( stage / "payload/db/000001.sst" ) );
    check( first.allocated_attempt_bytes >= first.payload_bytes );
    auto again = stage_direct_restore_snapshot( spec, config, target, stage, true );
    check( again.download_complete && again.reused_files == 5 && again.downloaded_files == 0 && again.transferred_bytes == 0 );
    check( again.attempt_id == first.attempt_id );
    rejects( [&] { stage_direct_restore_snapshot( spec, config, target, stage ); } );
    check( json::parse( read_direct_restore_attempt_identity( stage ) ).at( "backup_id" ) == "unit-1" );
    rejects( [&] { stage_direct_restore_snapshot( spec, config, target, stage, true, experimental ); } );
    check( json::parse( direct_stage_result_to_json( first ) ).at( "ready_to_activate" ) == false );

    // Crash-shaped rename/progress gap; only the missing files are transferred.
    DirectStageOptions pause; bool once = false;
    pause.checkpoint = [&]( const std::string& phase, uint64_t i ) {
      if( !once && phase == "after_file_rename" && i == 0 ) { once = true; throw std::runtime_error( "injected interruption" ); }
    };
    const auto interrupted = root / "interrupted";
    auto stopped = stage_direct_restore_snapshot( spec, config, target, interrupted, false, pause );
    check( !stopped.download_complete && stopped.state == "paused" && !fs::exists( interrupted / "progress/0.json" ) );
    auto resumed = stage_direct_restore_snapshot( spec, config, target, interrupted, true );
    check( resumed.download_complete && resumed.reused_files == 1 && resumed.downloaded_files == 4 );
    write( interrupted / "payload/db/CURRENT", "externally changed" );
    auto repaired = stage_direct_restore_snapshot( spec, config, target, interrupted, true );
    check( repaired.download_complete && repaired.downloaded_files == 1 && repaired.reused_files == 4 );
    check( repaired.transferred_bytes == 17 );

    auto changed = input; auto changed_manifest = manifest; changed_manifest[ "node" ][ "version" ] = "changed"; changed.manifest = changed_manifest.dump();
    const auto changed_spec = validate_restore_metadata( changed );
    rejects( [&] { stage_direct_restore_snapshot( changed_spec, config, target, stage, true ); } );
    const auto original_descriptor = read( stage / "ATTEMPT.json" );
    auto wrong_descriptor = original_descriptor; wrong_descriptor[ "backup_id" ] = "another"; write( stage / "ATTEMPT.json", wrong_descriptor.dump() );
    rejects( [&] { stage_direct_restore_snapshot( spec, config, target, stage, true ); } );
    write( stage / "ATTEMPT.json", original_descriptor.dump() );
    const auto original_state = read( stage / "state.json" );
    for( const auto& phase: { "validating", "verified", "activation_pending", "activated", "unknown" } )
    { auto state = original_state; state[ "state" ] = phase; write( stage / "state.json", state.dump() ); rejects( [&] { stage_direct_restore_snapshot( spec, config, target, stage, true ); } ); }
    write( stage / "state.json", original_state.dump() );
    auto malformed_state = original_state; malformed_state[ "version" ] = 2;
    write( stage / "state.json", malformed_state.dump() );
    rejects( [&] { stage_direct_restore_snapshot( spec, config, target, stage, true ); } );
    write( stage / "state.json", original_state.dump() );
    malformed_state = original_state; malformed_state[ "attempt_id" ] = std::string( 32, '0' );
    write( stage / "state.json", malformed_state.dump() );
    rejects( [&] { stage_direct_restore_snapshot( spec, config, target, stage, true ); } );
    write( stage / "state.json", original_state.dump() );
    int held = ::open( ( stage / "lock" ).c_str(), O_RDWR ); check( held >= 0 && ::flock( held, LOCK_EX | LOCK_NB ) == 0 );
    rejects( [&] { stage_direct_restore_snapshot( spec, config, target, stage, true ); } ); ::close( held );

    DirectStageOptions low; low.available_bytes = []( const auto& ) { return 0; };
    rejects( [&] { stage_direct_restore_snapshot( spec, config, target, root / "low", false, low ); } );
    check( !fs::exists( root / "low" ) );
    DirectStageOptions cancelled_options; cancelled_options.transfer.cancel_requested = [] { return true; };
    auto cancelled_result = stage_direct_restore_snapshot( spec, config, target, root / "cancelled", false, cancelled_options );
    check( !cancelled_result.download_complete && cancelled_result.state == "paused" && cancelled_result.downloaded_files == 0 );
    for( const auto& phase: { "chunk_write", "file_sync", "file_rename" } )
    {
      DirectStageOptions fail; bool hit = false;
      fail.checkpoint = [&]( const std::string& point, uint64_t ) { if( point == phase && !hit ) { hit = true; throw std::runtime_error( "injected write/sync/rename failure" ); } };
      const auto failing = root / ( std::string( "fail-" ) + phase );
      auto result = stage_direct_restore_snapshot( spec, config, target, failing, false, fail );
      check( !result.download_complete && hit && result.state == "paused" );
      check( !fs::exists( target ) && !fs::exists( failing / "RESTORE_STAGE_COMPLETE" ) );
    }
    DirectStageOptions exhaust; unsigned calls = 0;
    exhaust.available_bytes = [&]( const auto& ) { return ++calls > 2 ? 0 : UINT64_MAX; };
    auto full = stage_direct_restore_snapshot( spec, config, target, root / "disk-full", false, exhaust );
    check( !full.download_complete && full.state == "paused" && full.transferred_bytes == 0 );

    // Journal fault after file rename: the object remains reusable without a
    // trusted progress record. The failure state must itself be durably written.
    DirectStageOptions journal_failure; bool armed = false, hit = false;
    journal_failure.checkpoint = [&]( const std::string& phase, uint64_t ) {
      if( phase == "after_file_rename" ) armed = true;
      if( phase == "journal_sync" && armed && !hit ) { hit = true; throw std::runtime_error( "injected journal sync failure" ); }
    };
    const auto journal_attempt = root / "journal-failure";
    auto journal_result = stage_direct_restore_snapshot( spec, config, target, journal_attempt, false, journal_failure );
    check( hit && !journal_result.download_complete && journal_result.state == "paused" );
    check( fs::exists( journal_attempt / "payload/db/CURRENT" ) && !fs::exists( journal_attempt / "progress/0.json" ) );

    const auto resumed_requirement = first.required_additional_bytes;
    DirectStageOptions reuse_space; reuse_space.available_bytes = [=]( const auto& ) { return resumed_requirement; };
    auto reused_without_double_count = stage_direct_restore_snapshot( spec, config, target, interrupted, true, reuse_space );
    check( reused_without_double_count.download_complete && reused_without_double_count.downloaded_files == 0 );
    check( reused_without_double_count.required_additional_bytes == resumed_requirement );
    fs::create_directory( root / "existing-target" ); fs::create_directory( root / "existing-target/db" );
    rejects( [&] { stage_direct_restore_snapshot( spec, config, root / "existing-target", root / "should-not-exist" ); } );
    check( !fs::exists( root / "should-not-exist" ) );

    fs::create_directory_symlink( source, stage / "payload/escape" );
    rejects( [&] { stage_direct_restore_snapshot( spec, config, target, stage, true ); } );
    const auto outside = root / "unowned"; check( fs::create_directory( outside ) ); write( outside / "keep", "keep" );
    rejects( [&] { stage_direct_restore_snapshot( spec, config, target, outside, true ); } );
    check( read_direct_restore_attempt_identity( interrupted ).size() < 65536 );
    std::ofstream( root / "result.json" ) << direct_stage_result_to_json( first );
    struct rusage usage{}; check( ::getrusage( RUSAGE_SELF, &usage ) == 0 );
    std::cout << "SC2 checks passed: " << checks << "; maxrss_platform_units=" << usage.ru_maxrss << "; evidence=" << root << '\n';
    return 0;
  }
  catch( const std::exception& error ) { std::cerr << error.what() << " after " << checks << " checks\n"; return 1; }
}
