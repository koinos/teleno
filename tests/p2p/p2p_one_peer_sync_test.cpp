#include "p2p/p2p_node.hpp"

#include <cassert>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace koinos::node;
using namespace koinos::node::p2p;
namespace protocol = koinos::protocol;
namespace rpc = koinos::rpc;

namespace {

protocol::block make_block( uint64_t height, const std::string& previous )
{
  protocol::block block;
  block.set_id( "block-" + std::to_string( height ) );
  block.set_signature( "producer-a" );
  block.mutable_header()->set_height( height );
  block.mutable_header()->set_previous( previous );
  return block;
}

uint64_t unix_time_ms( std::chrono::system_clock::time_point t )
{
  return std::chrono::duration_cast< std::chrono::milliseconds >( t.time_since_epoch() ).count();
}

class FakeChain final : public IChain
{
public:
  explicit FakeChain( std::string chain_id ) : _chain_id( std::move( chain_id ) ) {}

  rpc::chain::submit_block_response
  submit_block( const rpc::chain::submit_block_request& req ) override
  {
    std::lock_guard lock( _mutex );
    const auto& block = req.block();
    if( !block.has_header() )
      throw std::runtime_error( "missing header" );

    if( _state_merkle_mismatch_height != 0 && block.header().height() == _state_merkle_mismatch_height )
      throw std::runtime_error( "block previous state merkle mismatch" );

    if( _known_blocks.count( block.id() ) )
      return {};

    if( block.header().height() == 1 )
    {
      if( !block.header().previous().empty() )
        throw std::runtime_error( "unexpected previous" );
    }
    else
    {
      auto itr = _known_blocks.find( block.header().previous() );
      if( itr == _known_blocks.end() )
        throw std::runtime_error( "unexpected previous" );
      if( block.header().height() != itr->second + 1 )
        throw std::runtime_error( "unexpected height" );
    }

    _head_height = block.header().height();
    _head_id = block.id();
    _known_blocks[ block.id() ] = block.header().height();
    _submitted.push_back( block );
    _cv.notify_all();
    return {};
  }

  rpc::chain::submit_transaction_response
  submit_transaction( const rpc::chain::submit_transaction_request& req ) override
  {
    std::lock_guard lock( _mutex );
    _submitted_transactions.push_back( req.transaction() );
    return {};
  }

  rpc::chain::get_head_info_response
  get_head_info( const rpc::chain::get_head_info_request& = {} ) override
  {
    std::lock_guard lock( _mutex );
    rpc::chain::get_head_info_response resp;
    resp.mutable_head_topology()->set_id( _head_id );
    resp.mutable_head_topology()->set_height( _head_height );
    resp.set_last_irreversible_block( _lib_height );
    return resp;
  }

  rpc::chain::get_chain_id_response
  get_chain_id( const rpc::chain::get_chain_id_request& = {} ) override
  {
    rpc::chain::get_chain_id_response resp;
    resp.set_chain_id( _chain_id );
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
    return _cv.wait_for( lock, std::chrono::seconds( 5 ), [&] { return _head_height >= height; } );
  }

  void add_known_block( const protocol::block& block )
  {
    std::lock_guard lock( _mutex );
    _known_blocks[ block.id() ] = block.header().height();
  }

  void set_head( const protocol::block& block )
  {
    std::lock_guard lock( _mutex );
    _head_height = block.header().height();
    _head_id = block.id();
    _known_blocks[ block.id() ] = block.header().height();
    _cv.notify_all();
  }

  void set_last_irreversible_block( uint64_t height )
  {
    std::lock_guard lock( _mutex );
    _lib_height = height;
  }

  void reject_state_merkle_at( uint64_t height )
  {
    std::lock_guard lock( _mutex );
    _state_merkle_mismatch_height = height;
  }

  std::vector< protocol::block > submitted_blocks() const
  {
    std::lock_guard lock( _mutex );
    return _submitted;
  }

  std::vector< protocol::transaction > submitted_transactions() const
  {
    std::lock_guard lock( _mutex );
    return _submitted_transactions;
  }

private:
  std::string _chain_id;
  mutable std::mutex _mutex;
  std::condition_variable _cv;
  uint64_t _head_height = 0;
  uint64_t _lib_height = 0;
  uint64_t _state_merkle_mismatch_height = 0;
  std::string _head_id;
  std::map< std::string, uint64_t > _known_blocks;
  std::vector< protocol::block > _submitted;
  std::vector< protocol::transaction > _submitted_transactions;
};

class FakeBlockStore final : public IBlockStore
{
public:
  FakeBlockStore() = default;

  explicit FakeBlockStore( std::vector< protocol::block > blocks )
  {
    for( auto& block: blocks )
      _blocks.emplace( block.id(), std::move( block ) );
  }

  rpc::block_store::get_blocks_by_height_response
  get_blocks_by_height( const rpc::block_store::get_blocks_by_height_request& req ) override
  {
    rpc::block_store::get_blocks_by_height_response resp;
    for( uint32_t i = 0; i < req.num_blocks(); ++i )
    {
      auto block_id = ancestor_at_height( req.head_block_id(), req.ancestor_start_height() + i );
      if( block_id.empty() )
        break;

      auto itr = _blocks.find( block_id );
      if( itr == _blocks.end() )
        break;

      auto* item = resp.add_block_items();
      item->set_block_id( itr->second.id() );
      item->set_block_height( itr->second.header().height() );
      if( req.return_block() )
        *item->mutable_block() = itr->second;
    }
    return resp;
  }

  rpc::block_store::get_blocks_by_id_response
  get_blocks_by_id( const rpc::block_store::get_blocks_by_id_request& req ) override
  {
    rpc::block_store::get_blocks_by_id_response resp;
    for( const auto& block_id: req.block_ids() )
    {
      auto itr = _blocks.find( block_id );
      if( itr == _blocks.end() )
        continue;

      auto* item = resp.add_block_items();
      item->set_block_id( itr->second.id() );
      item->set_block_height( itr->second.header().height() );
      if( req.return_block() )
        *item->mutable_block() = itr->second;
    }
    return resp;
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

private:
  std::string ancestor_at_height( std::string block_id, uint64_t height ) const
  {
    while( !block_id.empty() )
    {
      auto itr = _blocks.find( block_id );
      if( itr == _blocks.end() )
        return {};
      if( itr->second.header().height() == height )
        return block_id;
      if( itr->second.header().height() < height )
        return {};
      block_id = itr->second.header().previous();
    }
    return {};
  }

  std::map< std::string, protocol::block > _blocks;
};

class FakeTransport final : public ITransport
{
public:
  FakeTransport( std::string chain_id, std::vector< protocol::block > blocks )
      : _chain_id( std::move( chain_id ) ), _blocks( std::move( blocks ) )
  {}

  void start() override
  {
    if( _on_connected )
      _on_connected( _peer );
  }

  void stop() override {}

  void connect_peer( const PeerID& peer ) override { _connect_attempts.push_back( peer ); }
  void disconnect_peer( const PeerID& ) override { _disconnects++; }
  uint32_t connected_peer_count() const override { return 1; }
  std::vector< PeerID > connected_peers() const override { return { _peer }; }
  std::vector< PeerID > known_peers() const override { return _known_peers; }

  std::string peer_get_chain_id( const PeerID& ) override
  {
    _chain_id_calls++;
    return _chain_id;
  }

  PeerHeadInfo peer_get_head_block( const PeerID& ) override
  {
    _head_calls++;
    return { _blocks.back().id(), _blocks.back().header().height() };
  }

  std::string peer_get_ancestor_block_id( const PeerID&, const std::string&, uint64_t height ) override
  {
    _ancestor_calls++;
    for( const auto& block: _blocks )
      if( block.header().height() == height )
        return block.id();
    return {};
  }

  std::vector< protocol::block >
  peer_get_blocks( const PeerID&, const std::string&, uint64_t start_height, uint32_t count ) override
  {
    _blocks_calls++;
    _last_start_height = start_height;
    _last_count = count;

    std::vector< protocol::block > result;
    for( const auto& block: _blocks )
    {
      if( block.header().height() < start_height )
        continue;
      if( result.size() >= count )
        break;
      result.push_back( block );
    }
    return result;
  }

  void publish_block( const protocol::block& ) override {}
  void publish_transaction( const protocol::transaction& ) override {}

  void on_peer_connected( PeerConnectedCallback cb ) override { _on_connected = std::move( cb ); }
  void on_peer_disconnected( PeerDisconnectedCallback cb ) override { _on_disconnected = std::move( cb ); }
  void on_block_received( BlockReceivedCallback cb ) override { _on_block = std::move( cb ); }
  void on_transaction_received( TxReceivedCallback cb ) override { _on_tx = std::move( cb ); }
  void on_peer_rpc_request( PeerRpcRequestCallback cb ) override { _on_peer_rpc_request = std::move( cb ); }

  int chain_id_calls() const { return _chain_id_calls; }
  int head_calls() const { return _head_calls; }
  int ancestor_calls() const { return _ancestor_calls; }
  int blocks_calls() const { return _blocks_calls; }
  uint64_t last_start_height() const { return _last_start_height; }
  uint32_t last_count() const { return _last_count; }
  int disconnects() const { return _disconnects; }
  const std::vector< PeerID >& connect_attempts() const { return _connect_attempts; }

  void add_known_peer( PeerID peer )
  {
    _known_peers.push_back( std::move( peer ) );
  }

  void emit_block( const protocol::block& block )
  {
    if( _on_block )
      _on_block( _peer, block );
  }

  void emit_transaction( const protocol::transaction& tx )
  {
    if( _on_tx )
      _on_tx( _peer, tx );
  }

private:
  PeerID _peer{ "peer-a", "/ip4/127.0.0.1/tcp/10000/p2p/peer-a" };
  std::string _chain_id;
  std::vector< protocol::block > _blocks;
  PeerConnectedCallback _on_connected;
  PeerDisconnectedCallback _on_disconnected;
  BlockReceivedCallback _on_block;
  TxReceivedCallback _on_tx;
  PeerRpcRequestCallback _on_peer_rpc_request;
  int _chain_id_calls = 0;
  int _head_calls = 0;
  int _ancestor_calls = 0;
  int _blocks_calls = 0;
  int _disconnects = 0;
  uint64_t _last_start_height = 0;
  uint32_t _last_count = 0;
  std::vector< PeerID > _known_peers;
  std::vector< PeerID > _connect_attempts;
};

} // namespace

int main()
{
  {
    P2POptions toggle_opts;
    toggle_opts.gossip_head_threshold = std::chrono::seconds( 45 );

    std::atomic< bool > enabled{ false };
    GossipToggle toggle(
      toggle_opts,
      [&]( bool value ) { enabled.store( value ); },
      [] { return 1; } );

    toggle.start();
    toggle.update_head_time( unix_time_ms( std::chrono::system_clock::now() ) );
    std::this_thread::sleep_for( std::chrono::milliseconds( 1200 ) );
    assert( enabled.load() );

    toggle.update_head_time( unix_time_ms( std::chrono::system_clock::now() - std::chrono::seconds( 60 ) ) );
    std::this_thread::sleep_for( std::chrono::milliseconds( 1200 ) );
    assert( !enabled.load() );
    toggle.stop();
  }

  EventBus event_bus;

  {
    std::vector< protocol::block > checkpoint_blocks;
    checkpoint_blocks.push_back( make_block( 1, "" ) );
    checkpoint_blocks.push_back( make_block( 2, checkpoint_blocks.back().id() ) );
    checkpoint_blocks.push_back( make_block( 3, checkpoint_blocks.back().id() ) );

    auto checkpoint_transport = std::make_unique< FakeTransport >( "test-chain", checkpoint_blocks );
    auto* checkpoint_transport_ptr = checkpoint_transport.get();

    FakeChain checkpoint_chain( "test-chain" );
    FakeBlockStore checkpoint_block_store;
    P2POptions checkpoint_opts;
    checkpoint_opts.block_request_batch_size = 500;
    checkpoint_opts.synced_block_delta = 1;
    checkpoint_opts.sync_check_interval = std::chrono::seconds( 1 );
    checkpoint_opts.syncing_check_interval = std::chrono::seconds( 1 );
    checkpoint_opts.checkpoints.push_back( { 2, checkpoint_blocks[ 1 ].id() } );

    P2PNode checkpoint_node(
      checkpoint_opts, &checkpoint_chain, &checkpoint_block_store, &event_bus, std::move( checkpoint_transport ) );
    checkpoint_node.start();

    assert( checkpoint_chain.wait_for_height( 3 ) );
    checkpoint_node.stop();
    assert( checkpoint_transport_ptr->ancestor_calls() >= 1 );
    assert( checkpoint_transport_ptr->disconnects() == 0 );
  }

  {
    std::vector< protocol::block > mismatch_checkpoint_blocks;
    mismatch_checkpoint_blocks.push_back( make_block( 1, "" ) );
    mismatch_checkpoint_blocks.push_back( make_block( 2, mismatch_checkpoint_blocks.back().id() ) );
    mismatch_checkpoint_blocks.push_back( make_block( 3, mismatch_checkpoint_blocks.back().id() ) );

    auto mismatch_checkpoint_transport =
      std::make_unique< FakeTransport >( "test-chain", mismatch_checkpoint_blocks );
    auto* mismatch_checkpoint_transport_ptr = mismatch_checkpoint_transport.get();

    FakeChain mismatch_checkpoint_chain( "test-chain" );
    FakeBlockStore mismatch_checkpoint_block_store;
    P2POptions mismatch_checkpoint_opts;
    mismatch_checkpoint_opts.block_request_batch_size = 500;
    mismatch_checkpoint_opts.synced_block_delta = 1;
    mismatch_checkpoint_opts.sync_check_interval = std::chrono::seconds( 1 );
    mismatch_checkpoint_opts.syncing_check_interval = std::chrono::seconds( 1 );
    mismatch_checkpoint_opts.checkpoints.push_back( { 2, "not-block-2" } );

    P2PNode mismatch_checkpoint_node(
      mismatch_checkpoint_opts,
      &mismatch_checkpoint_chain,
      &mismatch_checkpoint_block_store,
      &event_bus,
      std::move( mismatch_checkpoint_transport ) );
    mismatch_checkpoint_node.start();
    std::this_thread::sleep_for( std::chrono::milliseconds( 300 ) );
    mismatch_checkpoint_node.stop();

    assert( mismatch_checkpoint_transport_ptr->ancestor_calls() >= 1 );
    assert( mismatch_checkpoint_transport_ptr->disconnects() >= 1 );
    assert( mismatch_checkpoint_chain.submitted_blocks().empty() );
  }

  {
    std::vector< protocol::block > behind_checkpoint_blocks;
    behind_checkpoint_blocks.push_back( make_block( 1, "" ) );

    auto behind_checkpoint_transport =
      std::make_unique< FakeTransport >( "test-chain", behind_checkpoint_blocks );
    auto* behind_checkpoint_transport_ptr = behind_checkpoint_transport.get();

    FakeChain behind_checkpoint_chain( "test-chain" );
    FakeBlockStore behind_checkpoint_block_store;
    P2POptions behind_checkpoint_opts;
    behind_checkpoint_opts.block_request_batch_size = 500;
    behind_checkpoint_opts.synced_block_delta = 1;
    behind_checkpoint_opts.sync_check_interval = std::chrono::seconds( 1 );
    behind_checkpoint_opts.syncing_check_interval = std::chrono::seconds( 1 );
    behind_checkpoint_opts.checkpoints.push_back( { 2, "block-2" } );

    P2PNode behind_checkpoint_node(
      behind_checkpoint_opts,
      &behind_checkpoint_chain,
      &behind_checkpoint_block_store,
      &event_bus,
      std::move( behind_checkpoint_transport ) );
    behind_checkpoint_node.start();
    std::this_thread::sleep_for( std::chrono::milliseconds( 300 ) );
    behind_checkpoint_node.stop();

    assert( behind_checkpoint_transport_ptr->ancestor_calls() == 0 );
    assert( behind_checkpoint_transport_ptr->disconnects() == 0 );
    assert( behind_checkpoint_chain.submitted_blocks().empty() );
  }

  std::vector< protocol::block > remote_blocks;
  remote_blocks.push_back( make_block( 1, "" ) );
  remote_blocks.push_back( make_block( 2, remote_blocks.back().id() ) );
  remote_blocks.push_back( make_block( 3, remote_blocks.back().id() ) );

  auto transport = std::make_unique< FakeTransport >( "test-chain", remote_blocks );
  auto* transport_ptr = transport.get();

  FakeChain chain( "test-chain" );
  FakeBlockStore block_store;

  P2POptions opts;
  opts.block_request_batch_size = 500;
  opts.synced_block_delta = 1;
  opts.sync_check_interval = std::chrono::seconds( 1 );
  opts.syncing_check_interval = std::chrono::seconds( 1 );

  P2PNode node( opts, &chain, &block_store, &event_bus, std::move( transport ) );
  node.start();

  assert( chain.wait_for_height( 3 ) );
  std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

  transport_ptr->emit_block( remote_blocks[ 1 ] );
  transport_ptr->emit_block( remote_blocks[ 1 ] );
  transport_ptr->emit_block( make_block( 100, "unknown-future-parent" ) );

  protocol::transaction tx;
  tx.set_id( "tx-1" );
  transport_ptr->emit_transaction( tx );
  transport_ptr->emit_transaction( tx );

  node.stop();

  auto submitted = chain.submitted_blocks();
  assert( submitted.size() == 3 );
  assert( submitted[ 0 ].id() == "block-1" );
  assert( submitted[ 1 ].id() == "block-2" );
  assert( submitted[ 2 ].id() == "block-3" );
  assert( transport_ptr->chain_id_calls() >= 1 );
  assert( transport_ptr->head_calls() >= 1 );
  assert( transport_ptr->blocks_calls() == 1 );
  assert( transport_ptr->last_start_height() == 1 );
  assert( transport_ptr->last_count() == 3 );
  assert( transport_ptr->disconnects() == 0 );

  auto submitted_transactions = chain.submitted_transactions();
  assert( submitted_transactions.size() == 1 );
  assert( submitted_transactions[ 0 ].id() == "tx-1" );

  {
    auto common_1 = make_block( 1, "" );
    auto common_2 = make_block( 2, common_1.id() );
    auto local_3 = make_block( 3, common_2.id() );
    local_3.set_id( "local-3" );
    auto remote_3 = make_block( 3, common_2.id() );
    remote_3.set_id( "remote-3" );
    auto remote_4 = make_block( 4, remote_3.id() );
    remote_4.set_id( "remote-4" );

    std::vector< protocol::block > remote_fork{ common_1, common_2, remote_3, remote_4 };
    auto fork_transport = std::make_unique< FakeTransport >( "test-chain", remote_fork );
    auto* fork_transport_ptr = fork_transport.get();

    FakeChain fork_chain( "test-chain" );
    fork_chain.add_known_block( common_1 );
    fork_chain.add_known_block( common_2 );
    fork_chain.set_head( local_3 );
    fork_chain.set_last_irreversible_block( 2 );
    FakeBlockStore fork_block_store( { common_1, common_2, local_3 } );

    P2POptions fork_opts;
    fork_opts.block_request_batch_size = 500;
    fork_opts.synced_block_delta = 1;
    fork_opts.sync_check_interval = std::chrono::seconds( 1 );
    fork_opts.syncing_check_interval = std::chrono::seconds( 1 );

    P2PNode fork_node( fork_opts, &fork_chain, &fork_block_store, &event_bus, std::move( fork_transport ) );
    fork_node.start();

    assert( fork_chain.wait_for_height( 4 ) );
    std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    fork_node.stop();

    auto fork_submitted = fork_chain.submitted_blocks();
    assert( fork_submitted.size() == 2 );
    assert( fork_submitted[ 0 ].id() == "remote-3" );
    assert( fork_submitted[ 1 ].id() == "remote-4" );
    assert( fork_transport_ptr->ancestor_calls() >= 1 );
    assert( fork_transport_ptr->blocks_calls() == 1 );
	  assert( fork_transport_ptr->last_start_height() == 3 );
	  assert( fork_transport_ptr->disconnects() == 0 );
	}

	{
	  auto acquisition_transport = std::make_unique< FakeTransport >( "test-chain", remote_blocks );
	  acquisition_transport->add_known_peer(
	    { "peer-b", "/ip4/127.0.0.1/tcp/10001/p2p/peer-b" } );
	  acquisition_transport->add_known_peer(
	    { "peer-c", "/ip4/127.0.0.1/tcp/10002/p2p/peer-c" } );
	  auto* acquisition_transport_ptr = acquisition_transport.get();

	  FakeChain acquisition_chain( "test-chain" );
	  FakeBlockStore acquisition_block_store;
	  P2POptions acquisition_opts;
	  acquisition_opts.block_request_batch_size = 500;
	  acquisition_opts.synced_block_delta = 1;
	  acquisition_opts.sync_check_interval = std::chrono::seconds( 1 );
	  acquisition_opts.syncing_check_interval = std::chrono::seconds( 1 );
	  acquisition_opts.seed_reconnect_interval = std::chrono::seconds( 60 );
	  acquisition_opts.peer_acquisition_interval = std::chrono::seconds( 1 );
	  acquisition_opts.target_peer_count = 3;
	  acquisition_opts.max_candidate_dials_per_cycle = 2;

	  P2PNode acquisition_node(
	    acquisition_opts,
	    &acquisition_chain,
	    &acquisition_block_store,
	    &event_bus,
	    std::move( acquisition_transport ) );
	  acquisition_node.start();

	  std::this_thread::sleep_for( std::chrono::milliseconds( 1300 ) );
	  acquisition_node.stop();

	  const auto& attempts = acquisition_transport_ptr->connect_attempts();
	  assert( attempts.size() == 2 );
	  assert( attempts[ 0 ].id == "peer-b" );
	  assert( attempts[ 1 ].id == "peer-c" );
	}

	{
	  auto local_1 = make_block( 1, "" );
	  auto remote_2 = make_block( 2, local_1.id() );
    std::vector< protocol::block > remote_blocks_with_mismatch{ local_1, remote_2 };
    auto mismatch_transport = std::make_unique< FakeTransport >( "test-chain", remote_blocks_with_mismatch );
    auto* mismatch_transport_ptr = mismatch_transport.get();

    FakeChain mismatch_chain( "test-chain" );
    mismatch_chain.set_head( local_1 );
    mismatch_chain.set_last_irreversible_block( 1 );
    mismatch_chain.reject_state_merkle_at( 2 );
    FakeBlockStore mismatch_block_store( { local_1 } );

    P2POptions mismatch_opts;
    mismatch_opts.block_request_batch_size = 500;
    mismatch_opts.synced_block_delta = 1;
    mismatch_opts.sync_check_interval = std::chrono::seconds( 1 );
    mismatch_opts.syncing_check_interval = std::chrono::seconds( 1 );

    P2PNode mismatch_node( mismatch_opts, &mismatch_chain, &mismatch_block_store, &event_bus, std::move( mismatch_transport ) );
    mismatch_node.start();
    std::this_thread::sleep_for( std::chrono::milliseconds( 1200 ) );
    mismatch_node.stop();

    assert( mismatch_transport_ptr->blocks_calls() >= 1 );
    assert( mismatch_transport_ptr->disconnects() == 0 );
    assert( mismatch_chain.submitted_blocks().empty() );
  }

  return 0;
}
