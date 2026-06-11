#include "core/config.hpp"
#include "core/rpc_access_policy.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace koinos::node;

namespace {

std::filesystem::path write_config( const std::string& content )
{
  auto path = std::filesystem::temp_directory_path()
              / ( "koinos-node-config-test-" + std::to_string( std::rand() ) + ".yml" );
  std::ofstream out( path );
  out << content;
  out.close();
  return path;
}

} // namespace

int main()
{
  {
    auto path = write_config( R"(
global:
  blacklist:
    - block_store
    - chain.propose_block
  whitelist:
    - chain.get_head_info
p2p:
  identity-seed: stable-seed
  checkpoint:
    - "7:0x1220abcd"
  peer:
    - /dns4/seed.example/tcp/8888/p2p/QmSeed
)" );

    auto cfg = load_config( path );
    assert( cfg.rpc_blacklist.size() == 2 );
    assert( cfg.rpc_whitelist.size() == 1 );
    assert( cfg.p2p_identity_seed == "stable-seed" );
    assert( cfg.p2p_seeds.size() == 1 );
    assert( cfg.p2p_checkpoints.size() == 1 );
    assert( cfg.p2p_checkpoints[ 0 ].block_height == 7 );
    assert( cfg.p2p_checkpoints[ 0 ].block_id == std::string( "\x12\x20\xab\xcd", 4 ) );

    std::filesystem::remove( path );
  }

  {
    auto path = write_config( R"(
p2p:
  seed: legacy-identity-seed
)" );

    auto cfg = load_config( path );
    assert( cfg.p2p_identity_seed == "legacy-identity-seed" );
    assert( cfg.p2p_seeds.empty() );

    std::filesystem::remove( path );
  }

  {
    auto path = write_config( R"(
p2p:
  seed:
    - /dns4/legacy-seed.example/tcp/8888/p2p/QmSeed
)" );

    auto cfg = load_config( path );
    assert( cfg.p2p_identity_seed.empty() );
    assert( cfg.p2p_seeds.size() == 1 );

    std::filesystem::remove( path );
  }

  {
    auto path = write_config( R"(
p2p:
  checkpoint:
    - not-a-checkpoint
)" );

    bool threw = false;
    try
    {
      (void)load_config( path );
    }
    catch( const std::runtime_error& )
    {
      threw = true;
    }
    assert( threw );

    std::filesystem::remove( path );
  }

  {
    RpcAccessPolicy policy;
    assert( policy.allows( "chain", "get_head_info" ) );

    policy.blacklist = { "block_store", "chain.propose_block" };
    assert( policy.allows( "chain", "get_head_info" ) );
    assert( !policy.allows( "chain", "propose_block" ) );
    assert( !policy.allows( "block_store", "get_blocks_by_id" ) );

    policy.whitelist = { "koinos.rpc.chain.get_head_info" };
    assert( policy.allows( "chain", "get_head_info" ) );
    assert( !policy.allows( "chain", "propose_block" ) );
    assert( !policy.allows( "block_store", "get_blocks_by_id" ) );
  }

  return 0;
}
