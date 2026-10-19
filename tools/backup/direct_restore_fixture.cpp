// SC2 diagnostic only: owned repository-local SC0 fixtures, no node startup.
#include "backup/direct_restore.hpp"
#include "../../tests/artifact_root.hpp"
#include <filesystem>
#include <chrono>
#include <iostream>
#include <sys/resource.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace koinos::node;
using namespace koinos::node::backup;
namespace {
fs::path confined( const fs::path& requested )
{
  const auto root = test_artifact_root( TELENO_SC2_REPOSITORY_ROOT );
  const auto path = fs::weakly_canonical( fs::absolute( requested ) );
  if( !test_artifact_child( path, root ) ) throw std::runtime_error( "SC2 fixture paths must be in selected test evidence" );
  return path;
}
}
int main( int argc, char** argv )
{
  try
  {
    if( argc != 7 ) throw std::runtime_error( "usage: fixture SOURCE TARGET ATTEMPT URL new|resume none|kill-after-descriptor|kill-mid-file|kill-after-rename|cancel|cancel-wait|no-space" );
    const auto source = confined( argv[ 1 ] ), target = confined( argv[ 2 ] ), attempt = confined( argv[ 3 ] );
    if( !fs::is_regular_file( source / ".sc0-owned" ) ) throw std::runtime_error( "source is not a synthetic SC0 fixture" );
    const std::string url = argv[ 4 ], mode = argv[ 5 ], fault = argv[ 6 ];
    if( mode != "new" && mode != "resume" ) throw std::runtime_error( "unsupported fixture mode" );
    const auto slash = url.find( '/', std::string( "http://127.0.0.1:" ).size() );
    bool loopback = false;
    if( url.rfind( "http://127.0.0.1:", 0 ) == 0 && slash != std::string::npos )
    {
      const auto port = url.substr( 17, slash - 17 ); const auto route = url.substr( slash + 1 );
      loopback = !port.empty() && port.size() <= 5 && port.find_first_not_of( "0123456789" ) == std::string::npos
        && std::stoul( port ) > 0 && std::stoul( port ) <= 65535 && !route.empty()
        && route.find_first_not_of( "abcdefghijklmnopqrstuvwxyz-" ) == std::string::npos;
    }
    if( url != "file://" + ( source / "public" ).string() && !loopback ) throw std::runtime_error( "fixture only accepts its file source or bounded loopback HTTP" );
    BackupPublicRestoreConfig config; config.enabled = true; config.base_url = url; config.network = "testnet";
    config.require_https = false; config.timeout_seconds = 1; config.retries = 2;
    config.signature_required = true; config.signature_public_key_file = ( source / "fixture-signing.pub" ).string();
    const auto id = mode == "resume" ? json::parse( read_direct_restore_attempt_identity( attempt ) ).at( "backup_id" ).get< std::string >() : "latest";
    const auto metadata = attempt.parent_path() / ( attempt.filename().string() + ".metadata" );
    validate_direct_planning_paths( target, metadata, url );
    const auto spec = acquire_public_restore_specification( metadata, config, id, "sc0-synthetic-not-a-chain" );
    DirectStageOptions options; unsigned chunks = 0; bool cancel = false;
    if( fault != "none" && fault != "kill-after-descriptor" && fault != "kill-mid-file" && fault != "kill-after-rename" && fault != "cancel" && fault != "cancel-wait" && fault != "no-space" ) throw std::runtime_error( "unsupported fixture fault" );
    options.checkpoint = [&]( const std::string& phase, uint64_t file ) {
      if( phase == "after_descriptor" && fault == "kill-after-descriptor" ) ::_exit( 99 );
      if( phase == "after_file_rename" && fault == "kill-after-rename" ) ::_exit( 99 );
      if( phase == "chunk_write" && spec.data().files[ file ].size_bytes > 128 * 1024 && ++chunks == 4 )
      {
        if( fault == "kill-mid-file" ) ::_exit( 99 );
        if( fault == "cancel" ) cancel = true;
      }
    };
    const auto started = std::chrono::steady_clock::now();
    options.transfer.cancel_requested = [&] {
      return cancel || ( fault == "cancel-wait" && std::chrono::steady_clock::now() - started > std::chrono::milliseconds( 300 ) );
    };
    if( fault == "no-space" ) options.available_bytes = []( const auto& ) { return 0; };
    auto result = stage_direct_restore_snapshot( spec, config, target, attempt, mode == "resume", options );
    auto output = json::parse( direct_stage_result_to_json( result ) );
    struct rusage usage{}; if( ::getrusage( RUSAGE_SELF, &usage ) != 0 ) throw std::runtime_error( "rusage failed" );
#if defined(__APPLE__)
    output[ "peak_rss_bytes" ] = usage.ru_maxrss;
#else
    output[ "peak_rss_bytes" ] = uint64_t( usage.ru_maxrss ) * 1024;
#endif
    output[ "node_started" ] = false;
    std::cout << output.dump( 2 ) << '\n'; return result.download_complete ? 0 : 2;
  }
  catch( const std::exception& error ) { std::cerr << "SC2 fixture: " << error.what() << '\n'; return 1; }
}
