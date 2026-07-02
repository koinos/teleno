#include "core/config.hpp"
#include "p2p/identity.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace koinos::node;

namespace {

std::filesystem::path unique_temp_dir( const std::string& name )
{
  auto path = std::filesystem::temp_directory_path()
              / ( name + "-" + std::to_string( std::rand() ) );
  std::filesystem::remove_all( path );
  std::filesystem::create_directories( path );
  return path;
}

} // namespace

int main()
{
  {
    NodeConfig cfg;
    cfg.p2p_identity_seed_configured = true;
    cfg.p2p_identity_seed = "legacy-identity-seed";
    finalize_p2p_identity_config( cfg, unique_temp_dir( "koinos-p2p-identity-seed-a" ) );

    auto identity = p2p::resolve_p2p_identity( cfg );
    assert( identity.source == "seed-derived-legacy" );
    assert( identity.peer_id == "QmVzPifCqCK7uYyaVJDnv7zNmBJ77zGzSZfopqtdkMTRTP" );
    assert( identity.key_file.empty() );
  }

  {
    NodeConfig cfg;
    cfg.p2p_identity_seed_configured = true;
    cfg.p2p_identity_seed = "stable-seed";
    finalize_p2p_identity_config( cfg, unique_temp_dir( "koinos-p2p-identity-seed-b" ) );

    auto identity = p2p::resolve_p2p_identity( cfg );
    assert( identity.source == "seed-derived-legacy" );
    assert( identity.peer_id == "QmeWkhnMqsJvvUUzHizuYHDqXXkMaYJqU5JLxy5WUeVMKf" );
  }

  {
    auto basedir = unique_temp_dir( "koinos-p2p-identity-generated" );
    NodeConfig cfg;
    cfg.p2p_advertised_addresses = { "/ip4/203.0.113.10/tcp/8888" };
    finalize_p2p_identity_config( cfg, basedir );

    auto first = p2p::resolve_p2p_identity( cfg );
    auto second = p2p::resolve_p2p_identity( cfg );
    assert( first.source == "generated-key-file" );
    assert( first.peer_id == second.peer_id );
    assert( std::filesystem::exists( first.key_file ) );
    assert( first.advertised_multiaddrs.size() == 1 );
    assert( first.advertised_multiaddrs[ 0 ] == "/ip4/203.0.113.10/tcp/8888/p2p/" + first.peer_id );

    std::filesystem::remove_all( basedir );
  }

  {
    auto basedir = unique_temp_dir( "koinos-p2p-identity-configured" );
    NodeConfig generated_cfg;
    finalize_p2p_identity_config( generated_cfg, basedir );
    auto generated = p2p::resolve_p2p_identity( generated_cfg );

    NodeConfig configured_cfg;
    configured_cfg.p2p_identity_key_file_configured = true;
    configured_cfg.p2p_identity_key_file = "p2p/identity.key";
    finalize_p2p_identity_config( configured_cfg, basedir );
    auto configured = p2p::resolve_p2p_identity( configured_cfg );

    assert( configured.source == "configured-key-file" );
    assert( configured.peer_id == generated.peer_id );

    std::filesystem::remove_all( basedir );
  }

  return 0;
}
