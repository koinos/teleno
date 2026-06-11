#include "p2p/libp2p_transport.hpp"
#include "p2p/p2p_node.hpp"

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace koinos::node;
using namespace koinos::node::p2p;
namespace protocol = koinos::protocol;
namespace rpc = koinos::rpc;

namespace {

std::string fixture_chain_id()
{
  const char bytes[] = { 0x01, 0x00 };
  return std::string( bytes, sizeof( bytes ) );
}

std::string peer_id_from_multiaddr( const std::string& multiaddr )
{
  auto marker = multiaddr.rfind( "/p2p/" );
  if( marker == std::string::npos )
    throw std::runtime_error( "multiaddr has no /p2p/ peer ID" );
  return multiaddr.substr( marker + 5 );
}

class LiveFakeChain final : public IChain
{
public:
  rpc::chain::submit_block_response
  submit_block( const rpc::chain::submit_block_request& req ) override
  {
    std::lock_guard lock( _mutex );
    const auto& block = req.block();
    if( !block.has_header() )
      throw std::runtime_error( "missing header" );
    if( block.header().height() != _head_height + 1 )
      throw std::runtime_error( "unexpected height" );
    if( _head_height > 0 && block.header().previous() != _head_id )
      throw std::runtime_error( "unexpected previous" );

    _head_height = block.header().height();
    _head_id = block.id();
    _submitted.push_back( block );
    _cv.notify_all();
    return {};
  }

  rpc::chain::submit_transaction_response
  submit_transaction( const rpc::chain::submit_transaction_request& ) override
  {
    return {};
  }

  rpc::chain::get_head_info_response
  get_head_info( const rpc::chain::get_head_info_request& = {} ) override
  {
    std::lock_guard lock( _mutex );
    rpc::chain::get_head_info_response resp;
    resp.mutable_head_topology()->set_id( _head_id );
    resp.mutable_head_topology()->set_height( _head_height );
    return resp;
  }

  rpc::chain::get_chain_id_response
  get_chain_id( const rpc::chain::get_chain_id_request& = {} ) override
  {
    rpc::chain::get_chain_id_response resp;
    resp.set_chain_id( fixture_chain_id() );
    return resp;
  }

  rpc::chain::get_fork_heads_response
  get_fork_heads( const rpc::chain::get_fork_heads_request& = {} ) override
  {
    return {};
  }

  rpc::chain::read_contract_response
  read_contract( const rpc::chain::read_contract_request& ) override
  {
    return {};
  }

  rpc::chain::get_account_nonce_response
  get_account_nonce( const rpc::chain::get_account_nonce_request& ) override
  {
    return {};
  }

  rpc::chain::get_account_rc_response
  get_account_rc( const rpc::chain::get_account_rc_request& ) override
  {
    return {};
  }

  rpc::chain::get_resource_limits_response
  get_resource_limits( const rpc::chain::get_resource_limits_request& ) override
  {
    return {};
  }

  rpc::chain::invoke_system_call_response
  invoke_system_call( const rpc::chain::invoke_system_call_request& ) override
  {
    return {};
  }

  rpc::chain::propose_block_response
  propose_block( const rpc::chain::propose_block_request& ) override
  {
    return {};
  }

  bool wait_for_height( uint64_t height )
  {
    std::unique_lock lock( _mutex );
    return _cv.wait_for( lock, std::chrono::seconds( 15 ), [&] { return _head_height >= height; } );
  }

  std::vector< protocol::block > submitted_blocks() const
  {
    std::lock_guard lock( _mutex );
    return _submitted;
  }

private:
  mutable std::mutex _mutex;
  std::condition_variable _cv;
  uint64_t _head_height = 0;
  std::string _head_id;
  std::vector< protocol::block > _submitted;
};

class LiveFakeBlockStore final : public IBlockStore
{
public:
  rpc::block_store::get_blocks_by_height_response
  get_blocks_by_height( const rpc::block_store::get_blocks_by_height_request& ) override
  {
    return {};
  }

  rpc::block_store::get_blocks_by_id_response
  get_blocks_by_id( const rpc::block_store::get_blocks_by_id_request& ) override
  {
    return {};
  }

  rpc::block_store::get_highest_block_response
  get_highest_block( const rpc::block_store::get_highest_block_request& ) override
  {
    return {};
  }

  rpc::block_store::add_block_response
  add_block( const rpc::block_store::add_block_request& ) override
  {
    return {};
  }
};

} // namespace

int main( int argc, char** argv )
{
  if( argc != 2 )
    return 2;

  const std::string peer_addr = argv[ 1 ];
  PeerID seed{ peer_id_from_multiaddr( peer_addr ), peer_addr };

  Libp2pTransport::Config transport_config;
  transport_config.listen_address = "/ip4/127.0.0.1/tcp/0";
  transport_config.seed_peers = { peer_addr };

  P2POptions opts;
  opts.seed_peers = { seed };
  opts.block_request_batch_size = 500;
  opts.synced_block_delta = 1;
  opts.sync_check_interval = std::chrono::seconds( 1 );
  opts.syncing_check_interval = std::chrono::seconds( 1 );

  LiveFakeChain chain;
  LiveFakeBlockStore block_store;
  EventBus event_bus;

  auto transport = std::make_unique< Libp2pTransport >( transport_config );
  P2PNode node( opts, &chain, &block_store, &event_bus, std::move( transport ) );
  node.start();

  assert( chain.wait_for_height( 12 ) );
  node.stop();

  auto submitted = chain.submitted_blocks();
  assert( submitted.size() == 12 );
  for( size_t i = 0; i < submitted.size(); ++i )
    assert( submitted[ i ].header().height() == i + 1 );

  return 0;
}
