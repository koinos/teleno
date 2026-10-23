#include "core/owned_allocation.hpp"
#include "p2p/p2p_node.hpp"
#include "core/transaction_work_budget.hpp"

#include <cassert>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <future>
#include <iostream>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif
#include <mutex>
#include <new>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
thread_local int peer_cache_allocation_failure = -1;
}
void* operator new(std::size_t size)
{
  if(peer_cache_allocation_failure==0){peer_cache_allocation_failure=-1;throw std::bad_alloc();}
  if(peer_cache_allocation_failure>0)--peer_cache_allocation_failure;
  if(auto pointer=koinos::node::owned_allocation::allocate(size?size:1))return pointer;throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* pointer) noexcept{koinos::node::owned_allocation::deallocate(pointer);}
void operator delete[](void* pointer) noexcept{koinos::node::owned_allocation::deallocate(pointer);}
void operator delete(void* pointer,std::size_t) noexcept{koinos::node::owned_allocation::deallocate(pointer);}
void operator delete[](void* pointer,std::size_t) noexcept{koinos::node::owned_allocation::deallocate(pointer);}

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
    ++_block_attempts; _cv.notify_all();
    if( _allocation_failure ) throw std::bad_alloc();
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
    ++_transaction_attempts;
    if( _allocation_failure ) throw std::bad_alloc();
    if( _local_capacity ) throw LocalCapacityExceeded( _capacity_reason );
    if( _reject_transactions )
      throw std::runtime_error( "invalid account nonce" );
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

  void reject_transactions( bool reject ) { _reject_transactions = reject; }
  void local_capacity( bool full, LocalCapacityReason reason = LocalCapacityReason::transaction_work )
  { std::lock_guard lock( _mutex ); _local_capacity = full; _capacity_reason = reason; }
  size_t transaction_attempts() const { return _transaction_attempts; }
  void allocation_failure( bool fail ) { std::lock_guard lock( _mutex ); _allocation_failure = fail; }
  bool wait_for_block_attempts( size_t n )
  {
    std::unique_lock lock( _mutex );
    return _cv.wait_for( lock, std::chrono::seconds( 5 ), [&] { return _block_attempts >= n; } );
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
  bool _reject_transactions = false;
  bool _local_capacity = false;
  bool _allocation_failure = false;
  size_t _block_attempts = 0;
  LocalCapacityReason _capacity_reason = LocalCapacityReason::transaction_work;
  size_t _transaction_attempts = 0;
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
    if( _fail_start ) { _fail_start = false; throw std::runtime_error( "synthetic transport startup failure" ); }
    if( _auto_connect && _on_connected )
      _on_connected( _peer );
  }

  void stop() override {}

  void connect_peer( const PeerID& peer ) override
  { std::lock_guard lock(_connect_mutex); _connect_attempts.push_back(peer); _connect_cv.notify_all(); }
  bool wait_for_connect_attempts(size_t count)
  { std::unique_lock lock(_connect_mutex); return _connect_cv.wait_for(lock,std::chrono::seconds(4),[&]{return _connect_attempts.size()>=count;}); }
  void fail_known_peers_once() { _fail_known_peers=true; }
  void refuse_snapshot_once() { _refuse_snapshot=true; }
  void disconnect_peer( const PeerID& ) override { _disconnects++; }
  uint32_t connected_peer_count() const override { return 1; }
  PeerSnapshot connected_peers() const override { return std::vector<PeerID>{ _peer }; }
  PeerSnapshot known_peers() const override
  { if(_refuse_snapshot.exchange(false))throw LocalTransportUnavailable(LocalTransportUnavailable::Reason::peer_snapshot_capacity);if(_fail_known_peers.exchange(false)) throw std::bad_alloc(); return _known_peers; }

  std::string peer_get_chain_id( const PeerID& ) override
  {
    _chain_id_calls++;
    if( _unavailable_chain_id.exchange( false ) ) throw LocalTransportUnavailable();
    std::unique_lock lock( _gate_mutex );
    _gate_entered = true;
    _gate_cv.notify_all();
    _gate_cv.wait( lock, [&] { return !_gate_chain_id; } );
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
    if( _unavailable_blocks.exchange( false ) ) throw LocalTransportUnavailable();
    _last_start_height = start_height;
    _last_count = count;
    if( _response_count_capacity && count > _response_count_capacity )
      throw LocalTransportUnavailable( LocalTransportUnavailable::Reason::response_capacity );
    {
      std::unique_lock lock( _gate_mutex );
      _blocks_entered = true;
      _gate_cv.notify_all();
      _gate_cv.wait( lock, [&] { return !_gate_blocks; } );
    }

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

  void disable_auto_connect() { _auto_connect = false; }
  void set_current_peer( PeerID peer ) { _peer=std::move(peer); }
  void fail_next_start() { _fail_start = true; }
  void local_unavailable_once( bool handshake )
  { _unavailable_chain_id = handshake;_unavailable_blocks = !handshake; }
  void response_count_capacity( uint32_t count ) { _response_count_capacity = count; }

  void gate_chain_id()
  {
    std::lock_guard lock( _gate_mutex );
    _gate_chain_id = true;
    _gate_entered = false;
  }
  bool wait_for_chain_id()
  {
    std::unique_lock lock( _gate_mutex );
    return _gate_cv.wait_for( lock, std::chrono::seconds( 2 ), [&] { return _gate_entered; } );
  }
  void release_chain_id()
  {
    std::lock_guard lock( _gate_mutex );
    _gate_chain_id = false;
    _gate_cv.notify_all();
  }
  void gate_blocks() { std::lock_guard lock( _gate_mutex ); _gate_blocks = true; _blocks_entered = false; }
  bool wait_for_blocks()
  {
    std::unique_lock lock( _gate_mutex );
    return _gate_cv.wait_for( lock, std::chrono::seconds( 2 ), [&] { return _blocks_entered; } );
  }
  void release_blocks() { std::lock_guard lock( _gate_mutex ); _gate_blocks = false; _gate_cv.notify_all(); }
  void emit_connected( const PeerID& peer ) { if( _on_connected ) _on_connected( peer ); }
  void emit_disconnected( const PeerID& peer ) { if( _on_disconnected ) _on_disconnected( peer ); }

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
  bool _auto_connect = true;
  bool _fail_start = false;
  uint32_t _response_count_capacity = 0;
  std::string _chain_id;
  std::vector< protocol::block > _blocks;
  PeerConnectedCallback _on_connected;
  PeerDisconnectedCallback _on_disconnected;
  BlockReceivedCallback _on_block;
  TxReceivedCallback _on_tx;
  PeerRpcRequestCallback _on_peer_rpc_request;
  std::atomic< int > _chain_id_calls{ 0 }, _head_calls{ 0 }, _ancestor_calls{ 0 }, _blocks_calls{ 0 }, _disconnects{ 0 };
  std::atomic< uint64_t > _last_start_height{ 0 };
  std::atomic< uint32_t > _last_count{ 0 };
  std::atomic< bool > _unavailable_chain_id{ false }, _unavailable_blocks{ false };
  std::mutex _gate_mutex;
  std::condition_variable _gate_cv;
  bool _gate_chain_id = false, _gate_entered = false;
  bool _gate_blocks = false, _blocks_entered = false;
  std::vector< PeerID > _known_peers;
  std::vector< PeerID > _connect_attempts;
  std::mutex _connect_mutex;
  std::condition_variable _connect_cv;
  mutable std::atomic<bool> _fail_known_peers{false};
  mutable std::atomic<bool> _refuse_snapshot{false};
};

} // namespace

void test_disconnected_worker_drain()
{
  FakeChain chain( "test-chain" );
  FakeBlockStore store;
  EventBus bus;
  auto transport = std::make_unique< FakeTransport >( "test-chain", std::vector< protocol::block >{ make_block( 1, "" ) } );
  auto* wire = transport.get();
  wire->disable_auto_connect();
  wire->gate_chain_id();
  P2POptions opts;
  opts.peer_discovery_enabled = false;
  opts.peer_log_interval = std::chrono::seconds( 0 );
  opts.always_disable_gossip = true;
  P2PNode node( opts, &chain, &store, &bus, std::move( transport ) );
  node.start();
  const PeerID peer{ "drain-peer", "/ip4/127.0.0.1/tcp/10000" };
  wire->emit_connected( peer );
  assert( wire->wait_for_chain_id() );
  wire->emit_disconnected( peer );
  auto stopping = std::async( std::launch::async, [&] { node.stop(); } );
  const auto before_release = stopping.wait_for( std::chrono::milliseconds( 250 ) );
  wire->release_chain_id();
  assert( stopping.wait_for( std::chrono::seconds( 2 ) ) == std::future_status::ready );
  stopping.get();
  assert( before_release == std::future_status::timeout ); // stop must own even disconnected work
  assert( wire->head_calls() == 0 ); // cancelled generation cannot continue handshake
  assert( wire->disconnects() == 0 );
  assert( node.worker_stats().owned == 0 );
  wire->emit_connected( peer ); // stopped callbacks cannot launch work
  assert( node.worker_stats().connected == 0 && node.worker_stats().owned == 0 );
}

void test_worker_capacity_and_churn( bool measure = false )
{
  FakeChain chain( "test-chain" ); FakeBlockStore store; EventBus bus;
  auto transport = std::make_unique< FakeTransport >( "test-chain", std::vector< protocol::block >{ make_block( 1, "" ) } );
  auto* wire = transport.get(); wire->disable_auto_connect(); wire->gate_chain_id();
  P2POptions opts;
  opts.max_peer_sync_workers = 2; opts.peer_discovery_enabled = false;
  opts.peer_log_interval = std::chrono::seconds( 0 ); opts.always_disable_gossip = true;
  P2PNode node( opts, &chain, &store, &bus, std::move( transport ) );
  node.start(); node.start();
  assert( bus.on_block_accepted.num_slots() == 1 );
  const PeerID peer{ "churn-peer", "/ip4/127.0.0.1/tcp/10000" };
  wire->emit_connected( peer ); assert( wire->wait_for_chain_id() );
  wire->emit_connected( peer ); assert( node.worker_stats().owned == 1 );
  wire->emit_disconnected( peer );
  assert( node.worker_stats().retired == 1 );
  wire->emit_connected( peer ); // new generation while old RPC is still outstanding
  auto s = node.worker_stats();
  assert( s.owned == 2 && s.connected == 1 && s.retired == 1 && s.limit == 2 );
  wire->emit_connected( { "over-capacity", "/ip4/127.0.0.1/tcp/10001" } );
  assert( node.worker_stats().owned == 2 && node.worker_stats().refused == 1 && wire->disconnects() == 1 );
  wire->emit_disconnected( peer ); wire->release_chain_id();
  auto await_reaped = [&] {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 2 );
    while( node.worker_stats().owned && std::chrono::steady_clock::now() < deadline )
      std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
    assert( node.worker_stats().owned == 0 );
  };
  await_reaped();
  const auto started = std::chrono::steady_clock::now();
  for( unsigned i = 0; i < 200; ++i )
  {
    wire->gate_chain_id(); wire->emit_connected( peer );
    assert( wire->wait_for_chain_id() );
    wire->emit_disconnected( peer );
    assert( node.worker_stats().retired == 1 );
    wire->release_chain_id(); await_reaped();
    assert( node.worker_stats().connected == 0 && node.worker_stats().owned <= 2 );
  }
  assert( wire->head_calls() == 0 && wire->disconnects() == 1 );
  node.stop(); node.stop();
  assert( node.worker_stats().owned == 0 && bus.on_block_accepted.num_slots() == 0 );
  node.start(); node.stop();
  assert( node.worker_stats().owned == 0 );
  if( measure )
  {
    const auto elapsed = std::chrono::duration_cast< std::chrono::microseconds >(
      std::chrono::steady_clock::now() - started ).count();
    std::cout << "{\"format\":\"lr2-worker-churn\",\"cycles\":200,\"observed_owned_high_water\":2,"
              << "\"final_owned\":" << node.worker_stats().owned << ",\"refused\":" << node.worker_stats().refused
              << ",\"elapsed_microseconds\":" << elapsed << ",\"process_peak_rss_bytes\":";
#if defined(__unix__) || defined(__APPLE__)
    rusage usage{};
    if( getrusage( RUSAGE_SELF, &usage ) == 0 )
      std::cout << usage.ru_maxrss
#if !defined(__APPLE__)
                * 1024
#endif
                ;
    else std::cout << "null";
#else
    std::cout << "null";
#endif
    std::cout << "}" << std::endl;
  }
}

void test_cancelled_sync_batch()
{
  FakeChain chain( "test-chain" ); FakeBlockStore store; EventBus bus;
  auto transport = std::make_unique< FakeTransport >( "test-chain", std::vector< protocol::block >{ make_block( 1, "" ) } );
  auto* wire = transport.get(); wire->gate_blocks();
  P2POptions opts;
  opts.peer_discovery_enabled = false; opts.peer_log_interval = std::chrono::seconds( 0 ); opts.always_disable_gossip = true;
  P2PNode node( opts, &chain, &store, &bus, std::move( transport ) );
  node.start(); assert( wire->wait_for_blocks() );
  wire->emit_disconnected( { "peer-a", "" } );
  assert( node.worker_stats().retired == 1 );
  wire->release_blocks(); node.stop();
  assert( chain.submitted_blocks().empty() && node.seen_stats().blocks.entries == 0 );
  assert( wire->disconnects() == 0 && node.worker_stats().owned == 0 );
}

void test_worker_zero_capacity_and_start_failure()
{
  FakeChain chain( "test-chain" ); FakeBlockStore store; EventBus bus;
  auto transport = std::make_unique< FakeTransport >( "test-chain", std::vector< protocol::block >{ make_block( 1, "" ) } );
  auto* wire = transport.get(); wire->disable_auto_connect(); wire->fail_next_start();
  P2POptions opts; opts.max_peer_sync_workers = 0;
  opts.peer_discovery_enabled = false; opts.peer_log_interval = std::chrono::seconds( 0 ); opts.always_disable_gossip = true;
  P2PNode node( opts, &chain, &store, &bus, std::move( transport ) );
  bool failed = false;
  try { node.start(); } catch( const std::runtime_error& ) { failed = true; }
  assert( failed && bus.on_block_accepted.num_slots() == 0 && node.worker_stats().owned == 0 );
  node.start(); assert( bus.on_block_accepted.num_slots() == 1 );
  wire->emit_connected( { "zero-capacity", "" } );
  auto s = node.worker_stats();
  assert( s.owned == 0 && s.connected == 0 && s.limit == 0 && s.refused == 1 );
  assert( wire->chain_id_calls() == 0 && wire->disconnects() == 1 );
  node.stop(); assert( bus.on_block_accepted.num_slots() == 0 );
}

void test_bounded_gossip()
{
  FakeChain chain( "test-chain" );
  FakeBlockStore store;
  EventBus bus;
  auto transport = std::make_unique<FakeTransport>( "test-chain", std::vector<protocol::block>{} );
  auto* wire = transport.get();
  wire->disable_auto_connect(); // synthetic callbacks only; no peer sync thread
  P2POptions opts;
  opts.peer_discovery_enabled = false;
  opts.peer_log_interval = std::chrono::seconds(0);
  opts.seen_block_entries = opts.seen_transaction_entries = 2;
  opts.seen_block_bytes = opts.seen_transaction_bytes = 128;
  opts.error_score_disconnect_threshold = opts.score_transaction_application;
  std::set<std::string> pending;
  bool unavailable = false;
  bool race = false;
  unsigned lookup_calls = 0;
  P2PNode node( opts, &chain, &store, &bus, std::move(transport), [&](const std::string& id) {
    if(unavailable) throw std::runtime_error("synthetic lookup failure");
    if(race) return ++lookup_calls > 1;
    return pending.count(id) != 0;
  } );
  node.start();
  auto b1 = make_block(1, "");
  wire->emit_block(b1); wire->emit_block(b1);
  assert(chain.submitted_blocks().size() == 1);
  wire->emit_block(make_block(2,b1.id()));
  wire->emit_block(make_block(3,"block-2"));
  wire->emit_block(b1); // evicted live fork/duplicate goes to idempotent controller
  assert(chain.submitted_blocks().size() == 3 && wire->disconnects() == 0);
  protocol::transaction tx; tx.set_id("tx-a");
  wire->emit_transaction(tx); wire->emit_transaction(tx);
  assert(chain.transaction_attempts() == 1);
  tx.set_id("tx-b"); wire->emit_transaction(tx);
  tx.set_id("tx-c"); wire->emit_transaction(tx);
  pending.insert("tx-a"); tx.set_id("tx-a"); wire->emit_transaction(tx);
  assert(chain.transaction_attempts() == 3 && wire->disconnects() == 0);
  pending.clear(); // expiry/reorg: do not consult a forever-seen validity oracle
  wire->emit_transaction(tx);
  assert(chain.transaction_attempts() == 4);

  const auto accepted = chain.submitted_transactions().size();
  chain.local_capacity( true );
  tx.set_id( "local-overload" );
  const auto overload_attempts = chain.transaction_attempts();
  wire->emit_transaction( tx ); wire->emit_transaction( tx );
  assert( chain.transaction_attempts() == overload_attempts + 2 ); // no hidden retry/cache
  assert( chain.submitted_transactions().size() == accepted && wire->disconnects() == 0 );
  assert( node.seen_stats().transactions.entries == 2 );
  chain.local_capacity( false ); wire->emit_transaction( tx );
  assert( chain.submitted_transactions().size() == accepted + 1 );
  wire->emit_transaction( tx ); assert( chain.transaction_attempts() == overload_attempts + 3 );

  chain.local_capacity( true, LocalCapacityReason::pending_record );
  tx.set_id( "pending-record-overload" );
  const auto record_attempts = chain.transaction_attempts();
  wire->emit_transaction( tx ); wire->emit_transaction( tx );
  assert( chain.transaction_attempts() == record_attempts + 2 );
  assert( chain.submitted_transactions().size() == accepted + 1 && wire->disconnects() == 0 );
  assert( node.seen_stats().transactions.entries == 2 );
  chain.local_capacity( false ); wire->emit_transaction( tx ); wire->emit_transaction( tx );
  assert( chain.transaction_attempts() == record_attempts + 3 && chain.submitted_transactions().size() == accepted + 2 );

  chain.local_capacity( true, LocalCapacityReason::mempool_memory );
  tx.set_id( "aggregate-memory-overload" );
  const auto aggregate_attempts = chain.transaction_attempts();
  wire->emit_transaction( tx ); wire->emit_transaction( tx );
  assert( chain.transaction_attempts() == aggregate_attempts + 2 );
  assert( chain.submitted_transactions().size() == accepted + 2 && wire->disconnects() == 0 );
  assert( node.seen_stats().transactions.entries == 2 );
  chain.local_capacity( false ); wire->emit_transaction( tx ); wire->emit_transaction( tx );
  assert( chain.transaction_attempts() == aggregate_attempts + 3 && chain.submitted_transactions().size() == accepted + 3 );

  chain.reject_transactions(true); race = true;
  tx.set_id("admitted-concurrently"); wire->emit_transaction(tx);
  assert(lookup_calls == 2 && wire->disconnects() == 0);
  assert(node.seen_stats().transactions.entries == 2); // failure is not cached
  race = false; unavailable = true;
  tx.set_id("invalid-unknown"); wire->emit_transaction(tx);
  assert(wire->disconnects() == 1); // lookup failure is not permission to ignore rejection
  unavailable = false; chain.reject_transactions(false);

  // Bounded sustained distinct input, with advancing LIB to clean watchdog too.
  for(uint64_t i=4;i<=10003;++i)
  {
    wire->emit_block(make_block(i,"block-"+std::to_string(i-1)));
    tx.set_id("sustained-tx-"+std::to_string(i)); wire->emit_transaction(tx);
    if(i%100==0)
    {
      chain.set_last_irreversible_block(i-1);
      koinos::broadcast::block_irreversible bi; bi.mutable_topology()->set_height(i-1);
      bus.on_block_irreversible(bi);
    }
    const auto s=node.seen_stats();
    assert(s.blocks.entries <= 2 && s.transactions.entries <= 2);
    assert(s.blocks.id_capacity_bytes <= 128 && s.transactions.id_capacity_bytes <= 128);
  }
  assert(chain.submitted_blocks().size() == 10003);
  assert(node.seen_stats().transactions.capacity_evictions >= 9999);
  auto before=chain.submitted_blocks().size(); wire->emit_block(b1);
  assert(chain.submitted_blocks().size() == before); // old LIB block ignored
  chain.reject_state_merkle_at(10004);
  const auto invalid=make_block(10004,"block-10003");
  auto failures=wire->disconnects();
  wire->emit_block(invalid); wire->emit_block(invalid);
  assert(wire->disconnects() == failures+2 && chain.submitted_blocks().size() == before);
  assert(node.seen_stats().blocks.hits == 1);
  node.stop();
  assert(node.seen_stats().blocks.entries == 0 && node.seen_stats().transactions.entries == 0);
  assert(node.seen_stats().blocks.id_capacity_bytes == 0 && node.seen_stats().transactions.id_capacity_bytes == 0);
  assert(bus.on_block_accepted.num_slots() == 0 && bus.on_block_irreversible.num_slots() == 0);
  auto attempts=chain.transaction_attempts(); wire->emit_transaction(tx); wire->emit_block(b1);
  assert(chain.transaction_attempts() == attempts && node.seen_stats().blocks.entries == 0);
  node.start();
  assert(bus.on_block_accepted.num_slots() == 1 && bus.on_block_irreversible.num_slots() == 1);
  node.stop();
  assert(bus.on_block_accepted.num_slots() == 0 && bus.on_block_irreversible.num_slots() == 0);

  // Evicting accepted IDs must not reset the independent fork-bomb detector.
  FakeChain fork_chain("test-chain"); fork_chain.set_head(b1);
  auto fork_transport=std::make_unique<FakeTransport>("test-chain",std::vector<protocol::block>{});
  auto* fork_wire=fork_transport.get(); fork_wire->disable_auto_connect();
  P2PNode forks(opts,&fork_chain,&store,&bus,std::move(fork_transport)); forks.start();
  for(int i=0;i<4;++i)
  {
    auto block=make_block(2,b1.id()); block.set_id("fork-"+std::to_string(i));
    fork_wire->emit_block(block);
  }
  assert(fork_chain.submitted_blocks().size() == 3 && fork_wire->disconnects() == 1);
  assert(forks.seen_stats().blocks.entries == 2);
  forks.stop();
}

void test_local_allocation_failure_has_no_peer_penalty()
{
  FakeChain chain( "test-chain" ); FakeBlockStore store; EventBus bus;
  auto transport = std::make_unique< FakeTransport >( "test-chain", std::vector< protocol::block >{} );
  auto* wire = transport.get(); wire->disable_auto_connect();
  P2POptions opts;
  opts.error_score_disconnect_threshold = std::min( opts.score_block_application, opts.score_transaction_application );
  P2PNode node( opts, &chain, &store, &bus, std::move( transport ) ); node.start();
  const auto block = make_block( 1, "" ); protocol::transaction tx; tx.set_id( "allocation-failure-tx" );
  chain.allocation_failure( true );
  for( unsigned i = 0; i < 3; ++i ) { wire->emit_block( block ); wire->emit_transaction( tx ); }
  assert( chain.submitted_blocks().empty() && chain.submitted_transactions().empty() );
  assert( !node.seen_stats().blocks.entries && !node.seen_stats().transactions.entries && !wire->disconnects() );
  chain.allocation_failure( false ); wire->emit_block( block ); wire->emit_transaction( tx );
  assert( chain.submitted_blocks().size() == 1 && chain.submitted_transactions().size() == 1 && !wire->disconnects() );
  node.stop();

  FakeChain sync( "test-chain" ); sync.allocation_failure( true );
  auto peer = std::make_unique< FakeTransport >( "test-chain", std::vector< protocol::block >{ block } );
  auto* peer_wire = peer.get();
  opts.sync_check_interval = opts.syncing_check_interval = std::chrono::seconds( 1 );
  P2PNode syncing( opts, &sync, &store, &bus, std::move( peer ) ); syncing.start();
  assert( sync.wait_for_block_attempts( 1 ) && sync.submitted_blocks().empty() && !peer_wire->disconnects() );
  assert( !syncing.seen_stats().blocks.entries );
  sync.allocation_failure( false ); assert( sync.wait_for_height( 1 ) );
  syncing.stop(); assert( sync.submitted_blocks().size() == 1 && !peer_wire->disconnects() );
}

void test_local_transport_unavailable_has_no_peer_penalty()
{
  for( bool handshake: { false, true } )
  {
    const auto block = make_block( 1, "" );
    FakeChain chain( "test-chain" );FakeBlockStore store;EventBus bus;
    auto transport = std::make_unique< FakeTransport >( "test-chain", std::vector< protocol::block >{ block } );
    auto* wire = transport.get();wire->local_unavailable_once( handshake );
    P2POptions opts;opts.error_score_disconnect_threshold = 1;
    opts.syncing_check_interval = std::chrono::seconds( 1 );
    opts.peer_discovery_enabled = false;opts.peer_log_interval = std::chrono::seconds( 0 );
    opts.always_disable_gossip = true;
    P2PNode node( opts, &chain, &store, &bus, std::move( transport ) );node.start();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 8 );
    while( chain.submitted_blocks().empty() && std::chrono::steady_clock::now() < deadline )
      std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    node.stop();
    assert( wire->disconnects() == 0 && chain.submitted_blocks().size() == 1 );
    assert( node.worker_stats().owned == 0 );
  }
}

void test_receive_capacity_reduces_batch_without_peer_penalty()
{
  const auto one = make_block( 1, "" ), two = make_block( 2, one.id() );
  const auto three = make_block( 3, two.id() ), four = make_block( 4, three.id() );
  FakeChain chain( "test-chain" );FakeBlockStore store;EventBus bus;
  auto transport = std::make_unique< FakeTransport >( "test-chain", std::vector< protocol::block >{ one, two, three, four } );
  auto* wire = transport.get();wire->response_count_capacity( 2 );
  P2POptions opts;opts.block_request_batch_size = 4;opts.error_score_disconnect_threshold = 1;
  opts.syncing_check_interval = std::chrono::seconds( 1 );
  opts.peer_discovery_enabled = false;opts.peer_log_interval = std::chrono::seconds( 0 );opts.always_disable_gossip = true;
  P2PNode node( opts, &chain, &store, &bus, std::move( transport ) );node.start();
  assert( chain.wait_for_height( 2 ) );node.stop();
  const auto applied = chain.submitted_blocks();
  assert( wire->disconnects() == 0 && wire->blocks_calls() >= 2 && wire->last_count() == 2 );
  assert( applied.size() == 2 && applied[0].id() == one.id() && applied[1].id() == two.id() );
  assert( wire->last_start_height() == 1 && node.worker_stats().owned == 0 );
}

void wait_for_all_workers(P2PNode& node)
{
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
  while(node.worker_stats().owned&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(node.worker_stats().owned==0);
}

void test_candidate_allocation_failure()
{
  FakeChain chain("test-chain");FakeBlockStore store;EventBus bus;
  auto transport=std::make_unique<FakeTransport>("test-chain",std::vector<protocol::block>{make_block(1,"")});
  auto* wire=transport.get();wire->disable_auto_connect();
  P2POptions opts;opts.peer_discovery_enabled=false;opts.peer_log_interval=std::chrono::seconds(0);opts.always_disable_gossip=true;
  P2PNode node(opts,&chain,&store,&bus,std::move(transport));node.start();
  const PeerID peer{std::string(128,'x'),"/ip4/192.0.2.10/tcp/10000/p2p/"+std::string(128,'x')};
  for(int fail=0;fail<4;++fail)
  {
    wire->gate_chain_id();peer_cache_allocation_failure=fail;
    try{wire->emit_connected(peer);}catch(...){peer_cache_allocation_failure=-1;wire->release_chain_id();throw;}
    peer_cache_allocation_failure=-1;assert(wire->wait_for_chain_id());
    const auto stats=node.bookkeeping_stats();
    assert(stats.candidates==0&&stats.candidate_string_capacity_bytes==0);
    assert(stats.candidate_allocation_refusals==static_cast<uint64_t>(fail+1));
    assert(node.worker_stats().connected==1&&node.worker_stats().owned==1);
    wire->emit_disconnected(peer);wire->release_chain_id();wait_for_all_workers(node);
  }
  wire->gate_chain_id();wire->emit_connected(peer);assert(wire->wait_for_chain_id());
  assert(node.bookkeeping_stats().candidates==1&&node.bookkeeping_stats().candidate_allocation_refusals==4);
  wire->emit_disconnected(peer);wire->release_chain_id();wait_for_all_workers(node);node.stop();
  assert(wire->disconnects()==0&&node.bookkeeping_stats().errors.records==0&&chain.submitted_blocks().empty());
}

void test_protected_candidates()
{
  FakeChain chain("test-chain");FakeBlockStore store;EventBus bus;
  {
    auto transport=std::make_unique<FakeTransport>("test-chain",std::vector<protocol::block>{make_block(1,"")});
    P2POptions opts;opts.max_peer_candidates=1;
    opts.seed_peers={{"seed-a","/ip4/192.0.2.1/tcp/10000"},{"seed-b","/ip4/192.0.2.2/tcp/10000"}};
    P2PNode node(opts,&chain,&store,&bus,std::move(transport));const auto stats=node.bookkeeping_stats();
    assert(stats.candidates==2&&stats.protected_candidates==2&&stats.configured_seeds==2&&stats.candidate_effective_limit==2);
  }
  auto transport=std::make_unique<FakeTransport>("test-chain",std::vector<protocol::block>{make_block(1,"")});
  auto* wire=transport.get();wire->disable_auto_connect();wire->gate_chain_id();
  P2POptions opts;opts.max_peer_candidates=1;opts.max_peer_sync_workers=2;opts.peer_discovery_enabled=false;
  opts.peer_log_interval=std::chrono::seconds(0);opts.always_disable_gossip=true;
  P2PNode node(opts,&chain,&store,&bus,std::move(transport));node.start();
  const PeerID a{"active-a","/ip4/192.0.2.3/tcp/10000"},b{"active-b","/ip4/192.0.2.4/tcp/10000"};
  wire->emit_connected(a);assert(wire->wait_for_chain_id());wire->gate_chain_id();
  wire->emit_connected(b);assert(wire->wait_for_chain_id());
  const auto active=node.bookkeeping_stats();
  assert(active.candidates==1&&active.protected_candidates==1&&active.candidate_evictions==1);
  assert(node.worker_stats().connected==2&&wire->disconnects()==0);
  wire->emit_disconnected(a);wire->emit_disconnected(b);wire->release_chain_id();wait_for_all_workers(node);node.stop();
  assert(node.bookkeeping_stats().candidates<=1&&node.bookkeeping_stats().protected_candidates==0);
}

void test_incoming_score_and_capacity_admission()
{
  FakeChain chain("test-chain");chain.reject_transactions(true);FakeBlockStore store;EventBus bus;
  auto transport=std::make_unique<FakeTransport>("test-chain",std::vector<protocol::block>{make_block(1,"")});
  auto* wire=transport.get();wire->disable_auto_connect();wire->gate_chain_id();
  const PeerID active{"active-scored","/ip4/192.0.2.5/tcp/10000/p2p/active-scored"};wire->set_current_peer(active);
  P2POptions opts;opts.max_peer_error_records=1;opts.error_score_disconnect_threshold=1000;opts.error_score_reconnect_threshold=50;
  opts.max_peer_sync_workers=2;opts.peer_discovery_enabled=false;opts.peer_log_interval=std::chrono::seconds(0);opts.always_disable_gossip=true;
  P2PNode node(opts,&chain,&store,&bus,std::move(transport));node.start();wire->emit_connected(active);assert(wire->wait_for_chain_id());
  protocol::transaction tx;tx.set_id("invalid-scored-transaction");wire->emit_transaction(tx);
  assert(node.bookkeeping_stats().errors.records==1&&wire->disconnects()==0);
  wire->emit_connected({"rotating-port","/ip4/192.0.2.5/tcp/10001/p2p/rotating-port"});
  assert(wire->disconnects()==1&&node.worker_stats().owned==1&&wire->chain_id_calls()==1);
  const auto before=node.bookkeeping_stats();wire->emit_connected({"new-address","/ip4/192.0.2.6/tcp/10000/p2p/new-address"});
  const auto after=node.bookkeeping_stats();
  assert(wire->disconnects()==2&&node.worker_stats().owned==1&&wire->chain_id_calls()==1);
  assert(after.errors.records==1&&after.errors.key_capacity_bytes==before.errors.key_capacity_bytes);
  assert(after.errors.capacity_refusals==before.errors.capacity_refusals+1&&after.errors.allocation_refusals==0);
  wire->emit_disconnected(active);wire->release_chain_id();wait_for_all_workers(node);node.stop();
  assert(chain.transaction_attempts()==1&&chain.submitted_transactions().empty());
}

bool test_discovery_allocation_failure()
{
  FakeChain chain("test-chain");FakeBlockStore store;EventBus bus;
  auto transport=std::make_unique<FakeTransport>("test-chain",std::vector<protocol::block>{make_block(1,"")});
  auto* wire=transport.get();wire->disable_auto_connect();wire->fail_known_peers_once();
  wire->add_known_peer({"discovered","/ip4/192.0.2.6/tcp/10000"});
  P2POptions opts;opts.peer_log_interval=std::chrono::seconds(0);opts.always_disable_gossip=true;
  opts.peer_acquisition_interval=std::chrono::seconds(1);opts.target_peer_count=2;
  P2PNode node(opts,&chain,&store,&bus,std::move(transport));node.start();
  const bool progressed=wire->wait_for_connect_attempts(1);node.stop();
  const auto stats=node.bookkeeping_stats();
  assert(stats.discovery_allocation_refusals==1&&stats.errors.records==0&&wire->disconnects()==0);
  return progressed;
}

bool test_discovery_snapshot_refusal()
{
  FakeChain chain("test-chain");FakeBlockStore store;EventBus bus;
  auto transport=std::make_unique<FakeTransport>("test-chain",std::vector<protocol::block>{make_block(1,"")});
  auto* wire=transport.get();wire->disable_auto_connect();wire->refuse_snapshot_once();wire->add_known_peer({"discovered","/ip4/192.0.2.6/tcp/10000"});
  P2POptions opts;opts.peer_log_interval=std::chrono::seconds(0);opts.always_disable_gossip=true;opts.peer_acquisition_interval=std::chrono::seconds(1);opts.target_peer_count=2;
  P2PNode node(opts,&chain,&store,&bus,std::move(transport));node.start();const bool progressed=wire->wait_for_connect_attempts(1);node.stop();
  const auto stats=node.bookkeeping_stats();assert(stats.discovery_snapshot_refusals==1&&stats.discovery_allocation_refusals==0&&stats.errors.records==0&&wire->disconnects()==0);return progressed;
}

bool test_peer_candidate_churn()
{
  FakeChain chain( "test-chain" ); FakeBlockStore store; EventBus bus;
  auto transport = std::make_unique< FakeTransport >( "test-chain", std::vector< protocol::block >{ make_block( 1, "" ) } );
  auto* wire = transport.get(); wire->disable_auto_connect();
  P2POptions opts;opts.max_peer_sync_workers=1;opts.max_peer_candidates=8;
  opts.peer_discovery_enabled=false;opts.peer_log_interval=std::chrono::seconds(0);opts.always_disable_gossip=true;
  P2PNode node(opts,&chain,&store,&bus,std::move(transport));node.start();
  for(unsigned i=0;i<128;++i)
  {
    const PeerID peer{"candidate-"+std::to_string(i),"/ip4/192.0.2.9/tcp/"+std::to_string(10000+i)};
    wire->gate_chain_id();wire->emit_connected(peer);assert(wire->wait_for_chain_id());wire->emit_disconnected(peer);wire->release_chain_id();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(node.worker_stats().owned&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(node.worker_stats().owned==0);
  }
  const auto stats=node.bookkeeping_stats();node.stop();
  if(stats.candidates>opts.max_peer_candidates)
  {
    std::cerr<<"candidate churn exceeds configured retention: "<<stats.candidates<<" > "<<opts.max_peer_candidates<<std::endl;return false;
  }
  assert(stats.errors.records==0&&wire->disconnects()==0);return true;
}

int main( int argc, char** argv )
{
  if( argc == 2 && std::string( argv[1] ) == "--measure-workers" )
  {
    test_worker_capacity_and_churn( true );
    return 0;
  }
  if( argc == 2 && std::string(argv[1]) == "--candidate-churn" ) return test_peer_candidate_churn() ? 0 : 1;
  if( argc == 2 && std::string(argv[1]) == "--discovery-allocation-fault" )
  {
    std::set_terminate([]{std::fputs("unhandled native discovery allocation failure\n",stderr);std::_Exit(91);});
    return test_discovery_allocation_failure() ? 0 : 1;
  }
  assert( argc == 1 );
  assert( test_peer_candidate_churn() );
  test_candidate_allocation_failure();
  test_protected_candidates();
  test_incoming_score_and_capacity_admission();
  assert( test_discovery_allocation_failure() );
  assert( test_discovery_snapshot_refusal() );
  test_disconnected_worker_drain();
  test_worker_capacity_and_churn();
  test_cancelled_sync_batch();
  test_worker_zero_capacity_and_start_failure();
  test_bounded_gossip();
  test_local_allocation_failure_has_no_peer_penalty();
  test_local_transport_unavailable_has_no_peer_penalty();
  test_receive_capacity_reduces_batch_without_peer_penalty();
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
