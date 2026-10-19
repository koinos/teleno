#include "backup/restore_plan.hpp"
#include "../artifact_root.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <nlohmann/json.hpp>

using namespace koinos::node::backup;
using json = nlohmann::json;
namespace fs = std::filesystem;
namespace {
unsigned checks = 0;
void check( bool condition ) { ++checks; if( !condition ) throw std::runtime_error( "SC1 assertion failed" ); }
template< typename F > void rejects( F action )
{ bool failed = false; try { action(); } catch( const std::exception& ) { failed = true; } check( failed ); }
RestoreMetadataInput fixture()
{
  json files = json::array( {
    { { "path", "db/CURRENT" }, { "sha256", std::string( 64, 'a' ) }, { "size_bytes", 17 }, { "runtime_file", false } },
    { { "path", "db/000001.sst" }, { "sha256", std::string( 64, 'b' ) }, { "size_bytes", 65536 }, { "runtime_file", false } },
    { { "path", "db/000002.sst" }, { "sha256", std::string( 64, 'b' ) }, { "size_bytes", 65536 }, { "runtime_file", false } },
    { { "path", "chain/genesis_data.json" }, { "sha256", std::string( 64, 'c' ) }, { "size_bytes", 10 }, { "runtime_file", true } } } );
  json manifest = { { "format", "teleno-native-rocksdb-snapshot" }, { "version", 1 }, { "backup_id", "fixture-1" },
    { "source", { { "storage_layout", "unified" } } },
    { "public_bootstrap", { { "network", "testnet" }, { "chain_id", "synthetic-chain" } } },
    { "snapshot", { { "file_count", 4 }, { "object_count", 3 }, { "total_bytes", 131099 } } },
    { "sizes", { { "restored_database_bytes", 131089 }, { "runtime_files_bytes", 10 },
      { "object_download_bytes", 65563 }, { "archive_bytes", 0 } } } };
  RestoreMetadataInput input;
  input.backup_id = "fixture-1"; input.source_identity = "https://example.invalid/backups";
  input.expected_network = "testnet"; input.expected_chain_id = "synthetic-chain";
  input.manifest = manifest.dump();
  input.inventory = json( { { "format", "teleno-native-snapshot-files" }, { "version", 1 },
    { "backup_id", "fixture-1" }, { "files", files } } ).dump();
  return input;
}
void write( const fs::path& file, const std::string& text )
{ std::ofstream out( file ); out << text; out.close(); check( bool( out ) ); }
} // namespace

int main()
{
  try
  {
    const auto input = fixture(); auto spec = validate_restore_metadata( input );
    check( spec.data().files.size() == 4 && spec.data().unique_object_bytes == 65563 );
    check( spec.data().expected_chain_checked && spec.data().manifest_sha256.size() == 64 );
    check( parse_restore_mode( "repository" ) == RestoreMode::repository );
    check( parse_restore_mode( "direct" ) == RestoreMode::direct );
    rejects( [] { parse_restore_mode( "ignore" ); } );
    check( parse_restore_reserve_profile( "standard" ) == RestoreReserveProfile::standard );
    check( parse_restore_reserve_profile( "experimental-8gib" ) == RestoreReserveProfile::experimental_8gib );
    rejects( [] { parse_restore_reserve_profile( "8" ); } );
    rejects( [] { restore_recovery_reserve_bytes( 0, static_cast< RestoreReserveProfile >( 2 ) ); } );
    constexpr uint64_t gib = 1024ULL * 1024 * 1024;
    check( restore_recovery_reserve_bytes( 0 ) == 10 * gib );
    check( restore_recovery_reserve_bytes( 0, RestoreReserveProfile::experimental_8gib ) == 8 * gib );
    check( restore_recovery_reserve_bytes( 60 * gib, RestoreReserveProfile::experimental_8gib ) == 12 * gib );
    const auto direct = estimate_mode_restore_space( spec, RestoreMode::direct );
    const auto repository = estimate_mode_restore_space( spec, RestoreMode::repository );
    RestoreBudgetPolicy experimental; experimental.reserve_profile = RestoreReserveProfile::experimental_8gib;
    const auto smaller = estimate_mode_restore_space( spec, RestoreMode::direct, experimental );
    check( direct.reserve_bytes == 10 * gib && smaller.reserve_bytes == 8 * gib );
    check( smaller.required_free_bytes == direct.required_free_bytes );
    check( direct.recommended_free_bytes - smaller.recommended_free_bytes == 2 * gib );
    const auto legacy_default = estimate_restore_space( 12345, 0, 0 );
    const auto legacy_smaller = estimate_restore_space( 12345, 0, 0, 0, false, 0, RestoreReserveProfile::experimental_8gib );
    check( legacy_default.recommended_target_free_bytes - legacy_smaller.recommended_target_free_bytes == 2 * gib );
    check( direct.logical_payload_bytes == 131099 && direct.allocated_payload_bytes == 262144 );
    check( direct.repository_cache_bytes == 0 && direct.transfer_extra_bytes == 0 );
    check( !check_mode_restore_space( direct, direct.required_free_bytes - 1, "fixture" ).passes_minimum );
    check( check_mode_restore_space( direct, direct.required_free_bytes, "fixture" ).below_recommended );
    check( !check_mode_restore_space( direct, direct.recommended_free_bytes, "fixture" ).below_recommended );
    check( repository.repository_cache_bytes == 196608 && repository.required_free_bytes == direct.required_free_bytes + 196608 );
    RestoreBudgetPolicy policy; policy.repository_on_target_filesystem = false;
    auto other = estimate_mode_restore_space( spec, RestoreMode::repository, policy );
    check( other.repository_cache_bytes == 0 && other.repository_other_filesystem_bytes == 196608 );
    policy = {}; policy.allocation_quantum = 64ULL * 1024 * 1024;
    check( estimate_mode_restore_space( spec, RestoreMode::direct, policy ).metadata_journal_bytes
           >= spec.data().files.size() * policy.allocation_quantum );
    policy = {};
    policy.metadata_journal_bytes = std::numeric_limits< uint64_t >::max();
    rejects( [&] { estimate_mode_restore_space( spec, RestoreMode::direct, policy ); } );
    policy = {}; policy.allocation_quantum = 0;
    rejects( [&] { estimate_mode_restore_space( spec, RestoreMode::direct, policy ); } );
    for( const auto& field: { "version", "backup_id", "source", "snapshot", "sizes", "public_bootstrap" } )
    { auto broken = input; auto j = json::parse( broken.manifest ); j.erase( field ); broken.manifest = j.dump(); rejects( [&] { validate_restore_metadata( broken ); } ); }
    auto mutate_manifest = [&]( auto mutate ) { auto broken = input; auto j = json::parse( broken.manifest ); mutate( j ); broken.manifest = j.dump(); rejects( [&] { validate_restore_metadata( broken ); } ); };
    mutate_manifest( []( auto& j ) { j[ "version" ] = 2; } );
    mutate_manifest( []( auto& j ) { j[ "source" ][ "storage_layout" ] = "split"; } );
    mutate_manifest( []( auto& j ) { j[ "snapshot" ][ "total_bytes" ] = -1; } );
    mutate_manifest( []( auto& j ) { j[ "snapshot" ][ "total_bytes" ] = 0; } );
    mutate_manifest( []( auto& j ) { j[ "source" ][ "chain_id" ] = "contradictory-chain"; } );
    mutate_manifest( []( auto& j ) { j[ "source" ][ "network" ] = "mainnet"; } );
    for(const auto failure:{RestoreSourceFailure::missing_chain_identity,RestoreSourceFailure::conflicting_chain_identity}) {
      auto broken=input; auto j=json::parse(input.manifest);
      if(failure==RestoreSourceFailure::missing_chain_identity) j["public_bootstrap"]["chain_id"]="";
      else j["source"]["chain_id"]="other-chain";
      broken.manifest=j.dump(); bool correct=false;
      try { validate_restore_metadata(broken); } catch(const RestoreSourceError& e) { correct=e.failure==failure; }
      check(correct);
    }
    auto published_input = input;
    auto published_manifest = json::parse( input.manifest );
    published_manifest[ "sizes" ][ "object_download_bytes" ] = 131099;
    published_input.manifest = published_manifest.dump();
    check( validate_restore_metadata( published_input ).data().unique_object_bytes == 65563 );
    auto deep = input;
    auto deep_manifest = json::parse( input.manifest );
    json nested = 1; for( unsigned i = 0; i < 40; ++i ) nested = json::array( { nested } );
    deep_manifest[ "extra" ] = nested; deep.manifest = deep_manifest.dump();
    rejects( [&] { validate_restore_metadata( deep ); } );
    for( auto mutation: { 0, 1, 2, 3 } )
    {
      auto invalid = input; auto j = json::parse( invalid.inventory );
      if( mutation == 0 ) j[ "files" ][ 1 ][ "size_bytes" ] = -1;
      if( mutation == 1 ) j[ "files" ][ 1 ][ "size_bytes" ] = 65535; // Repeated hash, inconsistent size.
      if( mutation == 2 ) j[ "files" ][ 1 ][ "path" ] = "db/CURRENT/subfile";
      if( mutation == 3 ) { j[ "files" ][ 1 ][ "path" ] = "db/Dir/a"; j[ "files" ][ 2 ][ "path" ] = "db/dir/b"; }
      invalid.inventory = j.dump(); rejects( [&] { validate_restore_metadata( invalid ); } );
    }
    for( const auto& path: { "../escape", "/absolute", "db/../CURRENT", "db//CURRENT", "db\\CURRENT", "db/CURRENT.", "db/current", "db", "config.yml", "db/a\n" } )
    {
      auto broken = input; auto j = json::parse( broken.inventory ); j[ "files" ][ 1 ][ "path" ] = path;
      broken.inventory = j.dump(); rejects( [&] { validate_restore_metadata( broken ); } );
    }
    for( const auto& hash: std::vector< std::string >{ "bad", std::string( 64, 'g' ), std::string( 64, 'A' ) } )
    { auto broken = input; auto j = json::parse( broken.inventory ); j[ "files" ][ 0 ][ "sha256" ] = hash; broken.inventory = j.dump(); rejects( [&] { validate_restore_metadata( broken ); } ); }
    auto broken = input; broken.expected_network = "mainnet"; rejects( [&] { validate_restore_metadata( broken ); } );
    broken = input; broken.expected_chain_id = "other-chain"; rejects( [&] { validate_restore_metadata( broken ); } );
    broken = input; broken.signature_required = true; rejects( [&] { validate_restore_metadata( broken ); } );
    broken.signature_verified = true; broken.authenticated_chain_id = "other-chain"; rejects( [&] { validate_restore_metadata( broken ); } );
    broken.authenticated_chain_id = "synthetic-chain"; check( validate_restore_metadata( broken ).data().signature_verified );
    RestoreMetadataLimits limits; limits.file_bytes = 1; rejects( [&] { validate_restore_metadata( input, limits ); } );
    limits = {}; limits.entries = 3; rejects( [&] { validate_restore_metadata( input, limits ); } );
    limits = {}; limits.object_bytes = 100; rejects( [&] { validate_restore_metadata( input, limits ); } );
    limits = {}; limits.total_bytes = 100; rejects( [&] { validate_restore_metadata( input, limits ); } );

#if defined(__unix__) || defined(__APPLE__)
    const auto root = test_artifact_root( TELENO_SC1_REPOSITORY_ROOT )
      / ( "sc1-unit-" + std::to_string( std::random_device{}() ) );
    fs::create_directories( root.parent_path() );
    check( fs::create_directory( root ) );
    auto target = root / "target", scratch = root / "scratch";
    auto result = build_direct_restore_preflight( spec, target, scratch );
    check( result.target_eligible && !fs::exists( target ) && !fs::exists( scratch ) );
    const auto output = json::parse( direct_restore_preflight_to_json( result ) );
    const auto experiment_plan = build_direct_restore_preflight( spec, target, scratch, RestoreReserveProfile::experimental_8gib );
    const auto experiment_json = json::parse( direct_restore_preflight_to_json( experiment_plan ) );
    check( experiment_json.at( "recovery_reserve_profile" ) == "experimental-8gib" );
    check( experiment_json.at( "experimental_reserve" ) == true );
    check( experiment_plan.budget.reserve_bytes == 8 * gib && result.budget.reserve_bytes == 10 * gib );
    check( output.at( "mode" ) == "direct" && output.at( "ready_to_restore" ) == false );
    check( output.at( "existing_filesystem_usage_bytes" ).get< uint64_t >() + result.available_bytes == result.capacity_bytes );
    fs::create_directory( target ); write( target / "config.yml", "features: {}\n" );
    check( build_direct_restore_preflight( spec, target, scratch ).target_eligible );
    fs::create_directory( target / "db" ); check( !build_direct_restore_preflight( spec, target, scratch ).target_eligible );
    fs::create_directories( root / "legacy/chain/blockchain" );
    check( !build_direct_restore_preflight( spec, root / "legacy", scratch ).target_eligible );
    fs::create_directory( root / "held" ); write( root / "held/.backup-observer-recovery", "hold\n" );
    check( !build_direct_restore_preflight( spec, root / "held", scratch ).target_eligible );
    rejects( [&] { validate_direct_planning_paths( target, target / "metadata", input.source_identity ); } );
    fs::create_directory_symlink( target, root / "linked" );
    rejects( [&] { validate_direct_planning_paths( root / "linked/new", scratch, input.source_identity ); } );
    rejects( [&] { validate_direct_planning_paths( target, scratch, "https://user:password@example.invalid" ); } );
    rejects( [&] { validate_direct_planning_paths( target, scratch, "file://" + root.string() ); } );
#endif
    std::cout << "SC1 checks passed: " << checks << '\n'; return 0;
  }
  catch( const std::exception& e ) { std::cerr << e.what() << " after " << checks << " checks\n"; return 1; }
}
