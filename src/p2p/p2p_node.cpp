#include "p2p_node.hpp"

#include "p2p/gorpc_codec.hpp"
#include "core/transaction_work_budget.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iterator>
#include <new>
#include <optional>
#include <stdexcept>

namespace koinos::node::p2p {

namespace {

bool contains_case_insensitive( const std::string& haystack, const std::string& needle )
{
  auto lower = []( unsigned char c ) { return static_cast< char >( std::tolower( c ) ); };
  std::string h;
  std::string n;
  h.reserve( haystack.size() );
  n.reserve( needle.size() );
  std::transform( haystack.begin(), haystack.end(), std::back_inserter( h ), lower );
  std::transform( needle.begin(), needle.end(), std::back_inserter( n ), lower );
  return h.find( n ) != std::string::npos;
}

bool is_transport_disconnect_error( const std::string& error )
{
  return contains_case_insensitive( error, "end of file" )
         || contains_case_insensitive( error, "connection reset" )
         || contains_case_insensitive( error, "broken pipe" )
         || contains_case_insensitive( error, "zero bytes written" )
         || contains_case_insensitive( error, "failed to open stream" )
         || contains_case_insensitive( error, "failed to write rpc request" )
         || contains_case_insensitive( error, "failed to read rpc response" )
         || contains_case_insensitive( error, "libp2p" )
         || contains_case_insensitive( error, "protocol negotiation failed" )
         || contains_case_insensitive( error, "stream reset" )
         || contains_case_insensitive( error, "stream closed" )
         || contains_case_insensitive( error, "connection closed" );
}

bool is_irreversibility_error( const std::string& error )
{
  return contains_case_insensitive( error, "prior to irreversibility" )
         || contains_case_insensitive( error, "earlier than irreversibility" )
         || contains_case_insensitive( error, "block irreversibility" );
}

bool is_local_state_merkle_mismatch( const std::string& error )
{
  return contains_case_insensitive( error, "block previous state merkle mismatch" )
         || contains_case_insensitive( error, "previous state merkle mismatch" );
}

const std::string& peer_score_address( const PeerID& peer ) noexcept
{
  // A transport may not yet have a remote address. Keep such identities
  // separate rather than sharing an empty-address ban across unrelated peers.
  return peer.address.empty() ? peer.id : peer.address;
}

bool has_dialable_address( const PeerID& peer )
{
  return !peer.id.empty() && !peer.address.empty();
}

void sleep_while_running( const std::atomic< bool >& running, std::chrono::seconds delay )
{
  const auto ticks = std::max< int64_t >( 1, delay.count() * 10 );
  for( int64_t i = 0; i < ticks && running; ++i )
    std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
}

} // namespace

P2PNode::P2PNode( const P2POptions& opts,
                  IChain* chain,
                  IBlockStore* block_store,
                  EventBus* event_bus,
                  std::unique_ptr< ITransport > transport,
                  PendingTransactionLookup pending_transaction )
    : _opts( opts ),
      _chain( chain ),
      _block_store( block_store ),
      _event_bus( event_bus ),
      _transport( std::move( transport ) ),
      _error_handler( opts ),
      _seen_blocks( opts.seen_block_entries, opts.seen_block_bytes, opts.seen_id_lifetime ),
      _seen_transactions( opts.seen_transaction_entries, opts.seen_transaction_bytes, opts.seen_id_lifetime ),
      _pending_transaction( std::move( pending_transaction ) )
{
  _seed_peers = opts.seed_peers;
  for( const auto& peer: _seed_peers )
    add_peer_candidate( peer, true );
}

P2PNode::~P2PNode()
{
  stop();
}

void P2PNode::start()
{
  std::unique_lock lifecycle_lock( _lifecycle_mutex );
  if( _running.exchange( true ) )
    return;

  try
  {
    start_runtime();
  }
  catch( ... )
  {
    lifecycle_lock.unlock();
    stop();
    throw;
  }
}

void P2PNode::start_runtime()
{
  // Get our chain ID
  if( _chain )
  {
    auto resp = _chain->get_chain_id();
    _chain_id = resp.chain_id();
    LOG( info ) << "[p2p] Chain ID: " << _chain_id.substr( 0, 16 ) << "...";
  }

  // Set up transport callbacks
  _transport->on_peer_connected( [this]( const PeerID& p ) { on_peer_connected( p ); } );
  _transport->on_peer_disconnected( [this]( const PeerID& p ) { on_peer_disconnected( p ); } );
  _transport->on_block_received( [this]( const PeerID& p, const protocol::block& b ) { on_gossip_block( p, b ); } );
  _transport->on_transaction_received(
    [this]( const PeerID& p, const protocol::transaction& t ) { on_gossip_transaction( p, t ); } );
  _transport->on_peer_rpc_request(
    [this]( const std::string& service, const std::string& method, const std::string& args ) {
      return handle_peer_rpc_request( service, method, args );
    } );

  // Set up gossip toggle
  _gossip_toggle = std::make_unique< GossipToggle >(
    _opts,
    [this]( bool enabled ) {
      if( _event_bus )
        _event_bus->on_gossip_status( enabled );
    },
    [this]() -> uint32_t { return _transport->connected_peer_count(); } );

  // Subscribe to EventBus
  if( _event_bus )
  {
    _accepted_connection = _event_bus->on_block_accepted.connect(
      [this]( const broadcast::block_accepted& ba ) { on_block_accepted( ba ); } );
    _irreversible_connection = _event_bus->on_block_irreversible.connect(
      [this]( const broadcast::block_irreversible& bi ) { on_block_irreversible( bi ); } );
  }

  // Start transport
  _transport->start();

  // Start gossip toggle
  _gossip_toggle->start();

  // Start seed peer reconnection loop
  _reconnect_thread = std::thread( [this]() {
    const auto seed_dial_spacing = std::chrono::seconds( 1 );
    auto last_seed_cycle = std::chrono::steady_clock::now() - _opts.seed_reconnect_interval;
    auto last_peer_log = std::chrono::steady_clock::now() - _opts.peer_log_interval;

    while( _running )
    {
      try
      {
        refresh_known_peer_candidates();
        _error_handler.collect_expired();
        {
          std::lock_guard lock( _peers_mutex );
          reap_finished_workers_locked();
        }

        const auto now = std::chrono::steady_clock::now();
        if( now - last_seed_cycle >= _opts.seed_reconnect_interval )
        {
          last_seed_cycle = now;
          for( const auto& seed: _seed_peers )
          {
            if( !_running )
              break;
            if( !_error_handler.can_connect( peer_score_address( seed ) ) )
              continue;

            // Check if already connected
            bool found = false;
            {
              std::lock_guard lock( _peers_mutex );
              found = _peers.count( seed.id ) > 0;
            }
            if( !found )
            {
              try
              {
                LOG( info ) << "[p2p] Attempting to connect to seed " << seed.id;
                _transport->connect_peer( seed );
              }
              catch( const std::exception& e )
              {
                LOG( debug ) << "[p2p] Seed reconnect failed for " << seed.id << ": " << e.what();
              }

              sleep_while_running( _running, seed_dial_spacing );
            }
          }
        }

        run_peer_acquisition_cycle();
        if( _opts.peer_log_interval.count() > 0 )
        {
          const auto log_now = std::chrono::steady_clock::now();
          if( log_now - last_peer_log >= _opts.peer_log_interval )
          {
            log_peer_snapshot();
            last_peer_log = log_now;
          }
        }

      }
      catch( const LocalTransportUnavailable& )
      {
        std::lock_guard lock(_peers_mutex);++_discovery_snapshot_refusals;
      }
      catch( const std::bad_alloc& )
      {
        // Keep the periodic worker alive without logging/allocating during
        // local exhaustion. No peer score, block application or in-call retry.
        std::lock_guard lock( _peers_mutex );
        ++_discovery_allocation_refusals;
      }

      auto loop_interval = _opts.seed_reconnect_interval;
      if( _opts.peer_discovery_enabled )
        loop_interval = std::min( loop_interval, _opts.peer_acquisition_interval );
      sleep_while_running( _running, loop_interval );
    }
  } );

  LOG( info ) << "[p2p] Started with " << _seed_peers.size()
              << " seed peers, target peer count " << _opts.target_peer_count
              << ", peer log interval " << _opts.peer_log_interval.count() << "s";
}

void P2PNode::stop()
{
  std::lock_guard lifecycle_lock( _lifecycle_mutex );
  if( !_running.exchange( false ) )
    return;

  _accepted_connection.disconnect();
  _irreversible_connection.disconnect();

  if( _gossip_toggle )
    _gossip_toggle->stop();

  // Own and cancel all generations, including already disconnected peers.
  // Keep the transport alive until their outstanding RPCs return; stopping it
  // concurrently would race its borrowed host/runtime members.
  std::list< std::shared_ptr< PeerState > > sync_workers;
  {
    std::lock_guard lock( _peers_mutex );
    for( const auto& state: _sync_workers )
      state->cancelled = true;
    sync_workers.splice( sync_workers.end(), _sync_workers );
    _draining_workers = sync_workers.size();
    _peers.clear();
    trim_peer_candidates_locked();
  }

  for( const auto& state: sync_workers )
    if( state->sync_thread.joinable() )
      state->sync_thread.join();
  {
    std::lock_guard lock( _peers_mutex );
    _draining_workers = 0;
  }

  if( _reconnect_thread.joinable() )
    _reconnect_thread.join();

  _transport->stop();
  _seen_blocks.clear();
  _seen_transactions.clear();
}

uint32_t P2PNode::connected_peer_count() const
{
  return _transport->connected_peer_count();
}

PeerSnapshot P2PNode::connected_peers() const
{
  return _transport ? _transport->connected_peers() : PeerSnapshot{};
}

PeerSnapshot P2PNode::known_peers() const
{
  return _transport ? _transport->known_peers() : PeerSnapshot{};
}

P2PNode::BookkeepingStats P2PNode::bookkeeping_stats()
{
  BookkeepingStats result{};
  {
    std::lock_guard lock( _peers_mutex );
    result.candidates = _peer_candidates.size();
    result.candidate_limit = _opts.max_peer_candidates;
    result.configured_seeds = _seed_peers.size();
    result.candidate_evictions = _candidate_evictions;
    result.candidate_allocation_refusals = _candidate_allocation_refusals;
    result.discovery_allocation_refusals = _discovery_allocation_refusals;
    result.discovery_snapshot_refusals = _discovery_snapshot_refusals;
    for( const auto& [id, candidate]: _peer_candidates )
    {
      result.candidate_string_capacity_bytes += id.capacity()
        + candidate.peer.id.capacity() + candidate.peer.address.capacity();
      if( candidate.seed || _peers.count( id ) ) ++result.protected_candidates;
    }
    result.candidate_effective_limit = result.candidate_limit
      ? std::max( result.candidate_limit, result.protected_candidates ) : 0;
  }
  result.errors = _error_handler.stats();
  return result;
}

void P2PNode::log_peer_snapshot()
{
  const auto workers = worker_stats();
  LOG( info ) << "[p2p] Sync workers: connected=" << workers.connected
              << " owned=" << workers.owned << " retired=" << workers.retired
              << " limit=" << workers.limit << " capacity-refusals=" << workers.refused;
  const auto seen = seen_stats();
  LOG( info ) << "[p2p] Accepted-ID cache: blocks=" << seen.blocks.entries
              << " id-capacity-bytes=" << seen.blocks.id_capacity_bytes
              << " transactions=" << seen.transactions.entries
              << " id-capacity-bytes=" << seen.transactions.id_capacity_bytes
              << " capacity-evictions=" << seen.blocks.capacity_evictions + seen.transactions.capacity_evictions
              << " expired=" << seen.blocks.expired + seen.transactions.expired;
  const auto bookkeeping = bookkeeping_stats();
  LOG( info ) << "[p2p] Peer bookkeeping: candidates=" << bookkeeping.candidates
              << " candidate-string-capacity-bytes=" << bookkeeping.candidate_string_capacity_bytes
              << " protected-candidates=" << bookkeeping.protected_candidates
              << " candidate-effective-limit=" << bookkeeping.candidate_effective_limit
              << " candidate-evictions=" << bookkeeping.candidate_evictions
              << " candidate-allocation-refusals=" << bookkeeping.candidate_allocation_refusals
              << " discovery-allocation-refusals=" << bookkeeping.discovery_allocation_refusals
              << " discovery-snapshot-refusals=" << bookkeeping.discovery_snapshot_refusals
              << " error-records=" << bookkeeping.errors.records
              << " error-key-capacity-bytes=" << bookkeeping.errors.key_capacity_bytes
              << " error-record-limit=" << bookkeeping.errors.max_records
              << " error-key-capacity-limit=" << bookkeeping.errors.max_key_capacity_bytes
              << " local-capacity-refusals=" << bookkeeping.errors.capacity_refusals
              << " allocation-refusals=" << bookkeeping.errors.allocation_refusals;
  auto peers = _transport->connected_peers();
  LOG( info ) << "[p2p] Connected peers:";
  for( const auto& peer: peers )
  {
    if( !peer.address.empty() )
      LOG( info ) << "[p2p]   - " << peer.address;
  }
}

void P2PNode::add_peer_candidate( const PeerID& peer, bool seed )
{
  if( !has_dialable_address( peer ) ) return;
  std::lock_guard lock( _peers_mutex );
  remember_peer_candidate_locked( peer, seed, false );
}

void P2PNode::remember_peer_candidate_locked( const PeerID& peer, bool seed, bool replace ) noexcept
{
  if( !has_dialable_address( peer ) ) return;
  seed = seed || std::any_of( _seed_peers.begin(), _seed_peers.end(),
    [&]( const auto& configured ) { return configured.id == peer.id; } );
  try
  {
    auto it = _peer_candidates.find( peer.id );
    if( it == _peer_candidates.end() )
    {
      PeerCandidate ready;
      ready.peer = peer; // Prepare all strings before map publication.
      ready.seed = seed;
      _peer_candidates.emplace( peer.id, std::move( ready ) );
    }
    else
    {
      if( replace || it->second.peer.address.empty() || seed )
      {
        PeerID ready = peer;
        it->second.peer.id.swap( ready.id );
        it->second.peer.address.swap( ready.address );
      }
      it->second.seed = it->second.seed || seed;
    }
  }
  catch( const std::bad_alloc& )
  {
    // This is optional dial advice, not peer validity or accepted chain state.
    // Configuration and active generations already retain their own identities.
    ++_candidate_allocation_refusals;
  }
  trim_peer_candidates_locked();
}

void P2PNode::trim_peer_candidates_locked() noexcept
{
  if( !_opts.max_peer_candidates ) return; // Preserve the explicit unlimited option.
  for( auto it = _peer_candidates.begin();
       it != _peer_candidates.end() && _peer_candidates.size() > _opts.max_peer_candidates; )
  {
    if( it->second.seed || _peers.count( it->first ) ) { ++it; continue; }
    it = _peer_candidates.erase( it );
    ++_candidate_evictions;
  }
}

void P2PNode::refresh_known_peer_candidates()
{
  if( !_opts.peer_discovery_enabled )
    return;

  for( const auto& peer: _transport->known_peers() )
    add_peer_candidate( peer, false );
}

bool P2PNode::is_peer_connected_locked( const std::string& peer_id ) const
{
  return _peers.count( peer_id ) > 0;
}

void P2PNode::run_peer_acquisition_cycle()
{
  if( !_opts.peer_discovery_enabled || _opts.target_peer_count == 0 )
    return;

  const auto connected = _transport->connected_peer_count();
  if( connected >= _opts.target_peer_count )
    return;

  const auto now = std::chrono::steady_clock::now();
  const auto wanted = _opts.target_peer_count - connected;
  const auto max_dials = std::max< uint32_t >( 1, _opts.max_candidate_dials_per_cycle );
  std::vector< PeerID > peers_to_dial;
  peers_to_dial.reserve( std::min( wanted, max_dials ) );

  {
    std::lock_guard lock( _peers_mutex );
    for( auto& [id, candidate]: _peer_candidates )
    {
      if( peers_to_dial.size() >= wanted || peers_to_dial.size() >= max_dials )
        break;
      if( candidate.seed || is_peer_connected_locked( id ) )
        continue;
      if( !_error_handler.can_connect( peer_score_address( candidate.peer ) ) )
        continue;
      if( candidate.attempts > 0
          && now - candidate.last_dial < _opts.candidate_redial_interval )
        continue;

      candidate.last_dial = now;
      ++candidate.attempts;
      peers_to_dial.push_back( candidate.peer );
    }
  }

  for( const auto& peer: peers_to_dial )
  {
    if( !_running )
      break;
    try
    {
      LOG( info ) << "[p2p] Attempting to connect to discovered peer " << peer.id;
      _transport->connect_peer( peer );
    }
    catch( const std::exception& e )
    {
      LOG( debug ) << "[p2p] Discovered peer reconnect failed for " << peer.id
                   << ": " << e.what();
    }
  }
}

// ---------------------------------------------------------------------------
// Peer lifecycle
// ---------------------------------------------------------------------------

void P2PNode::on_peer_connected( const PeerID& peer )
{
  std::unique_lock lock( _peers_mutex );

  if( !_running )
    return;
  reap_finished_workers_locked();

  if( _peers.count( peer.id ) )
    return; // Already connected

  if( _sync_workers.size() >= _opts.max_peer_sync_workers || _peers.size() >= _opts.max_peer_sync_workers )
  {
    ++_worker_refusals;
    lock.unlock();
    LOG( debug ) << "[p2p] Local sync-worker capacity reached; closing connection without peer penalty";
    _transport->disconnect_peer( peer );
    return;
  }

  const auto admission = _error_handler.admission( peer_score_address( peer ) );
  if( admission != PeerErrorHandler::Admission::allowed )
  {
    lock.unlock();
    LOG( debug ) << "[p2p] Closing inadmissible peer connection: "
                << ( admission == PeerErrorHandler::Admission::local_capacity
                     ? "local bookkeeping capacity" : "existing network-address score" );
    _transport->disconnect_peer( peer );
    return;
  }

  remember_peer_candidate_locked( peer, false, true );

  auto state = std::make_shared< PeerState >();
  state->peer = peer;

  // Allocate registry ownership before launching a thread. A failed launch
  // cannot leave a detached worker or a joinable thread in an erased owner.
  _sync_workers.push_back( state );
  try
  {
    _peers.emplace( peer.id, state );
    state->sync_thread = std::thread( [this, state]() noexcept {
      try { peer_sync_loop( state ); }
      catch( const std::exception& e )
      {
        try { LOG( warning ) << "[p2p] Sync worker failed: " << e.what(); } catch( ... ) {}
      }
      catch( ... )
      {
        try { LOG( warning ) << "[p2p] Sync worker failed with an unknown exception"; } catch( ... ) {}
      }
      // Last access to node-owned data precedes this publication. Reaping a
      // finished worker while holding _peers_mutex cannot wait on that mutex.
      state->finished = true;
    } );
  }
  catch( ... )
  {
    _peers.erase( peer.id );
    _sync_workers.pop_back();
    throw;
  }
  LOG( info ) << "[p2p] Peer connected: " << peer.id;
}

void P2PNode::on_peer_disconnected( const PeerID& peer )
{
  std::lock_guard lock( _peers_mutex );

  auto it = _peers.find( peer.id );
  if( it == _peers.end() )
    return;

  it->second->cancelled = true;
  _peers.erase( it );
  trim_peer_candidates_locked();
  LOG( info ) << "[p2p] Peer disconnected: " << peer.id;
}

std::string P2PNode::handle_peer_rpc_request( const std::string& service,
                                              const std::string& method,
                                              const std::string& args )
{
  if( service != "PeerRPCService" )
    throw std::runtime_error( "unknown peer RPC service: " + service );

  if( method == "GetChainID" )
  {
    if( !_chain )
      throw std::runtime_error( "chain service is unavailable" );

    if( _chain_id.empty() )
      _chain_id = _chain->get_chain_id().chain_id();
    return gorpc::encode_id_response( _chain_id );
  }

  if( method == "GetHeadBlock" )
  {
    if( !_chain )
      throw std::runtime_error( "chain service is unavailable" );

    auto head = _chain->get_head_info().head_topology();
    return gorpc::encode_head_block_response( head.id(), head.height() );
  }

  if( method == "GetAncestorBlockID" )
  {
    if( !_block_store )
      throw std::runtime_error( "block store service is unavailable" );

    auto request = gorpc::decode_get_ancestor_block_id_request( args );

    rpc::block_store::get_blocks_by_height_request block_request;
    block_request.set_head_block_id( request.parent_id );
    block_request.set_ancestor_start_height( request.child_height );
    block_request.set_num_blocks( 1 );
    block_request.set_return_block( true );
    block_request.set_return_receipt( false );

    auto response = _block_store->get_blocks_by_height( block_request );
    if( response.block_items_size() != 1 )
      throw std::runtime_error( "unexpected number of blocks returned" );

    return gorpc::encode_id_response( response.block_items( 0 ).block_id() );
  }

  if( method == "GetBlocks" )
  {
    if( !_block_store )
      throw std::runtime_error( "block store service is unavailable" );

    auto request = gorpc::decode_get_blocks_request( args );

    rpc::block_store::get_blocks_by_height_request block_request;
    block_request.set_head_block_id( request.head_block_id );
    block_request.set_ancestor_start_height( request.start_block_height );
    block_request.set_num_blocks( request.num_blocks );
    block_request.set_return_block( true );
    block_request.set_return_receipt( false );

    auto response = _block_store->get_blocks_by_height( block_request );

    std::vector< std::string > block_payloads;
    block_payloads.reserve( response.block_items_size() );
    for( const auto& item: response.block_items() )
    {
      if( !item.has_block() )
        throw std::runtime_error( "block store response item is missing block payload" );
      block_payloads.push_back( item.block().SerializeAsString() );
    }

    return gorpc::encode_blocks_response( block_payloads );
  }

  throw std::runtime_error( "unknown peer RPC method: " + method );
}

// ---------------------------------------------------------------------------
// Sync protocol (per-peer thread)
// ---------------------------------------------------------------------------

bool P2PNode::worker_active( const std::shared_ptr< PeerState >& state ) const
{
  return _running && !state->cancelled;
}

void P2PNode::reap_finished_workers_locked()
{
  for( auto it = _sync_workers.begin(); it != _sync_workers.end(); )
  {
    if( !( *it )->finished ) { ++it; continue; }
    if( ( *it )->sync_thread.joinable() ) ( *it )->sync_thread.join();
    it = _sync_workers.erase( it );
  }
}

P2PNode::WorkerStats P2PNode::worker_stats()
{
  std::lock_guard lock( _peers_mutex );
  reap_finished_workers_locked();
  size_t retired = 0;
  for( const auto& state: _sync_workers ) retired += state->cancelled ? 1 : 0;
  return { _peers.size(), _sync_workers.size() + _draining_workers,
           retired + _draining_workers, _opts.max_peer_sync_workers, _worker_refusals };
}

void P2PNode::peer_sync_loop( const std::shared_ptr< PeerState >& state )
{
  const auto& peer = state->peer;
  // Handshake with retries
  while( worker_active( state ) )
  {
    try
    {
      if( peer_handshake( state ) )
        break;
      if( _error_handler.get_score( peer_score_address( peer ) ) >= _opts.error_score_disconnect_threshold )
        return;
    }
    catch( const LocalTransportUnavailable& )
    {
      if( !worker_active( state ) ) return;
    }
    catch( const std::bad_alloc& )
    {
      // Local exhaustion is not remote misconduct; use the normal backoff.
      if( !worker_active( state ) ) return;
    }
    catch( const std::exception& e )
    {
      if( !worker_active( state ) ) return;
      if( report_peer_error( peer, e.what(), score_for_error( e.what() ) ) )
        return;
    }

    // Backoff
    for( int i = 0; i < 60 && worker_active( state ); ++i )
      std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
  }

  // Sync loop
  bool is_synced = false;
  while( worker_active( state ) )
  {
    try
    {
      request_sync_blocks( state, is_synced );
      if( _error_handler.get_score( peer_score_address( peer ) ) >= _opts.error_score_disconnect_threshold )
        return;
    }
    catch( const LocalTransportUnavailable& )
    {
      if( !worker_active( state ) ) return;
    }
    catch( const std::bad_alloc& )
    {
      if( !worker_active( state ) ) return;
    }
    catch( const std::exception& e )
    {
      if( !worker_active( state ) ) return;
      if( report_peer_error( peer, e.what(), score_for_error( e.what() ) ) )
        return;
    }

    auto sleep_time = is_synced ? _opts.sync_check_interval : _opts.syncing_check_interval;
    auto sleep_ms = std::chrono::duration_cast< std::chrono::milliseconds >( sleep_time ).count();
    for( int64_t i = 0; i < sleep_ms / 100 && worker_active( state ); ++i )
      std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
  }
}

bool P2PNode::peer_handshake( const std::shared_ptr< PeerState >& state )
{
  const auto& peer = state->peer;
  if( !_chain )
    return false;

  // Verify chain ID matches
  auto peer_chain_id = _transport->peer_get_chain_id( peer );
  if( !worker_active( state ) ) return false;
  if( peer_chain_id != _chain_id )
  {
    report_peer_error( peer, "chain ID mismatch", _opts.score_chain_id_mismatch );
    return false;
  }

  auto peer_head = _transport->peer_get_head_block( peer );
  if( !worker_active( state ) ) return false;
  if( peer_head.block_id.empty() )
  {
    report_peer_error( peer, "peer head is empty", _opts.score_chain_not_connected );
    return false;
  }

  for( const auto& checkpoint: _opts.checkpoints )
  {
    if( peer_head.height < checkpoint.block_height )
    {
      LOG( info ) << "[p2p] Peer " << peer.id << " is behind configured checkpoint height "
                  << checkpoint.block_height << " (peer height " << peer_head.height << ")";
      return false;
    }

    auto peer_checkpoint =
      _transport->peer_get_ancestor_block_id( peer, peer_head.block_id, checkpoint.block_height );
    if( !worker_active( state ) ) return false;
    if( peer_checkpoint != checkpoint.block_id )
    {
      report_peer_error( peer, "checkpoint mismatch", _opts.score_checkpoint_mismatch );
      return false;
    }
  }

  LOG( info ) << "[p2p] Handshake complete with " << peer.id;
  return true;
}

bool P2PNode::request_sync_blocks( const std::shared_ptr< PeerState >& state, bool& is_synced )
{
  const auto& peer = state->peer;
  if( !_chain || !_block_store )
    return false;

  std::lock_guard sync_lock( _sync_mutex );
  if( !worker_active( state ) ) return false;

  auto local_head_info = _chain->get_head_info();
  auto local_head = local_head_info.head_topology();
  uint64_t local_height = local_head.height();
  uint64_t local_lib_height = std::min( local_head_info.last_irreversible_block(), local_height );

  // Get peer's head
  auto peer_head = _transport->peer_get_head_block( peer );
  if( !worker_active( state ) ) return false;
  if( peer_head.height <= local_lib_height )
  {
    is_synced = true;
    return true;
  }

  uint64_t known_peer_head_height = 0;
  if( get_local_block_height( peer_head.block_id, known_peer_head_height ) && known_peer_head_height != 0 )
  {
    is_synced = true;
    return true;
  }

  std::string sync_anchor_id;
  if( local_lib_height > 0 )
  {
    sync_anchor_id = get_local_ancestor_block_id( local_head.id(), local_lib_height );
    if( sync_anchor_id.empty() )
    {
      LOG( warning ) << "[p2p] Could not resolve local LIB ancestor at height " << local_lib_height;
      return false;
    }

    // Match legacy p2p behavior: live forks above LIB are normal. Only a peer
    // that does not connect to our LIB is a chain-connectivity fault.
    auto ancestor_id = _transport->peer_get_ancestor_block_id( peer, peer_head.block_id, local_lib_height );
    if( !worker_active( state ) ) return false;
    if( ancestor_id != sync_anchor_id )
    {
      report_peer_error( peer, "chain not connected at LIB", _opts.score_chain_not_connected );
      return false;
    }
  }

  // Calculate batch
  uint64_t blocks_needed = peer_head.height - local_lib_height;
  const auto limit = state->response_batch_limit == 0 ? _opts.block_request_batch_size
    : std::min( _opts.block_request_batch_size, state->response_batch_limit );
  uint32_t batch = std::min( static_cast< uint64_t >( limit ), blocks_needed );

  // Fetch blocks
  std::vector< protocol::block > blocks;
  try { blocks = _transport->peer_get_blocks( peer, peer_head.block_id, local_lib_height + 1, batch ); }
  catch( const LocalTransportUnavailable& e )
  {
    // A complete response that cannot fit our local receive window is neither
    // a peer fault nor block invalidity. No block has been applied. A later
    // ordinary sync iteration may request a smaller batch from the same anchor.
    if( e.reason == LocalTransportUnavailable::Reason::response_capacity && batch > 1 )
      state->response_batch_limit = std::max< uint32_t >( 1, batch / 2 );
    throw;
  }
  if( !worker_active( state ) ) return false;

  if( blocks.empty() )
    return true;

  // Apply sequentially
  uint64_t expected_height = local_lib_height + 1;
  std::string expected_previous = sync_anchor_id;
  for( const auto& block: blocks )
  {
    if( !worker_active( state ) )
      return false;

    if( !block.has_header() || block.id().empty() || block.header().height() != expected_height )
    {
      report_peer_error( peer, "invalid sync block sequence", _opts.score_checkpoint_mismatch );
      return false;
    }

    if( !expected_previous.empty() && block.header().previous() != expected_previous )
    {
      report_peer_error( peer, "sync block previous mismatch", _opts.score_chain_not_connected );
      return false;
    }

    // Fork bomb check
    if( _fork_watchdog.check( block.id(), block.signature(), block.header().previous(), block.header().height() ) )
    {
      report_peer_error( peer, "fork bomb detected", _opts.score_fork_bomb );
      return false;
    }

    // Apply block via chain
    try
    {
      rpc::chain::submit_block_request req;
      *req.mutable_block() = block;
      _chain->submit_block( req );
      _seen_blocks.remember( block.id(), block.header().height() );
    }
    catch( const std::bad_alloc& )
    {
      // No peer penalty for local exhaustion. A failed chain attempt is not
      // remembered; later normal polling is not an in-attempt replay retry.
      return false;
    }
    catch( const std::exception& e )
    {
      if( !worker_active( state ) ) return false;
      if( is_irreversibility_error( e.what() ) )
      {
        LOG( debug ) << "[p2p] Ignoring already irreversible sync block at height "
                     << block.header().height() << ": " << e.what();
        expected_previous = block.id();
        ++expected_height;
        continue;
      }

      LOG( warning ) << "[p2p] Block application failed at height "
                     << block.header().height() << ": " << e.what();

      if( is_local_state_merkle_mismatch( e.what() ) )
      {
        LOG( warning ) << "[p2p] Local chain state rejected a peer sync block due to state merkle mismatch; "
                       << "keeping peer connected so local verify-blocks recovery can run";
        return false;
      }

      report_peer_error( peer, std::string( "block application: " ) + e.what(), _opts.score_block_application );
      return false;
    }

    expected_previous = block.id();
    ++expected_height;
  }

  // Update sync state
  uint64_t last_applied = blocks.back().header().height();
  is_synced = ( peer_head.height - last_applied ) < _opts.synced_block_delta;

  if( !is_synced )
  {
    LOG( info ) << "[p2p] Syncing from " << peer.id << " — applied "
                << blocks.size() << " blocks (height " << last_applied
                << "/" << peer_head.height << ", anchor LIB " << local_lib_height << ")";
  }

  return true;
}

// ---------------------------------------------------------------------------
// Gossip handlers
// ---------------------------------------------------------------------------

void P2PNode::on_gossip_block( const PeerID& from, const protocol::block& block )
{
  if( !_running )
    return;
  if( block.id().empty() || !block.has_header() )
  {
    report_peer_error( from, "invalid gossiped block", _opts.score_deserialization );
    return;
  }

  // Old traffic must not repopulate the fork watchdog after LIB cleanup.
  // Read current LIB as well as the event-derived value (restored/startup LIB).
  uint64_t lib = _lib_height.load();
  std::optional<rpc::chain::get_head_info_response> local_head;
  if( _chain )
  {
    try
    {
      local_head = _chain->get_head_info();
      lib = std::max( lib, local_head->last_irreversible_block() );
    }
    catch( const std::exception& e )
    {
      LOG( debug ) << "[p2p] Could not read LIB before gossip block apply: " << e.what();
    }
  }
  _seen_blocks.purge_irreversible( lib );
  if( block.header().height() <= lib )
    return;

  // Check fork bomb (including cached duplicates, as before).
  if( _fork_watchdog.check( block.id(), block.signature(), block.header().previous(), block.header().height() ) )
  {
    report_peer_error( from, "fork bomb via gossip", _opts.score_fork_bomb );
    return;
  }

  if( _seen_blocks.contains( block.id() ) )
    return;

  // Apply
  if( _chain )
  {
    try
    {
      if( !local_head )
        local_head = _chain->get_head_info();
      const auto local_height = local_head->head_topology().height();
      if( block.header().height() > local_height && block.header().height() - local_height > 1 )
      {
        LOG( debug ) << "[p2p] Ignoring future gossiped block from " << from.id
                     << " at height " << block.header().height()
                     << " while local head is " << local_height;
        return;
      }
    }
    catch( const std::exception& e )
    {
      LOG( debug ) << "[p2p] Could not read local head before gossip block apply: " << e.what();
    }

    try
    {
      rpc::chain::submit_block_request req;
      *req.mutable_block() = block;
      _chain->submit_block( req );
      _seen_blocks.remember( block.id(), block.header().height() );
    }
    catch( const std::bad_alloc& )
    {
      return;
    }
    catch( const std::exception& e )
    {
      report_peer_error( from, std::string( "gossip block: " ) + e.what(), _opts.score_block_application );
    }
  }
}

void P2PNode::on_gossip_transaction( const PeerID& from, const protocol::transaction& tx )
{
  if( !_running )
    return;
  if( tx.id().empty() )
  {
    report_peer_error( from, "invalid gossiped transaction", _opts.score_deserialization );
    return;
  }

  if( _chain )
  {
    if( _seen_transactions.contains( tx.id() ) || is_pending_transaction( tx.id() ) )
      return;

    try
    {
      rpc::chain::submit_transaction_request req;
      *req.mutable_transaction() = tx;
      _chain->submit_transaction( req );
      _seen_transactions.remember( tx.id() );
    }
    catch( const LocalCapacityExceeded& )
    {
      // Our local capacity is not peer misconduct. No accepted-ID insertion,
      // notification, queue or retry; later delivery gets normal validation.
      return;
    }
    catch( const std::bad_alloc& )
    {
      return; // Local allocation failure is not transaction invalidity.
    }
    catch( const std::exception& e )
    {
      // Admission by another source can race the precheck. Only a positive,
      // exact current mempool lookup suppresses a duplicate peer penalty.
      if( _seen_transactions.contains( tx.id() ) || is_pending_transaction( tx.id() ) )
        return;
      report_peer_error( from, std::string( "gossip tx: " ) + e.what(), _opts.score_transaction_application );
    }
  }
}

// ---------------------------------------------------------------------------
// EventBus handlers
// ---------------------------------------------------------------------------

void P2PNode::on_block_accepted( const broadcast::block_accepted& ba )
{
  if( !_running || !ba.has_block() )
    return;

  // Update gossip toggle head time
  if( _gossip_toggle && ba.block().has_header() )
    _gossip_toggle->update_head_time( ba.block().header().timestamp() );

  // Gossip to peers if live block
  if( ba.live() && _gossip_toggle && _gossip_toggle->is_enabled() )
    _transport->publish_block( ba.block() );
}

void P2PNode::on_block_irreversible( const broadcast::block_irreversible& bi )
{
  if( !_running )
    return;
  _lib_height.store( bi.topology().height() );
  _seen_blocks.purge_irreversible( bi.topology().height() );
  _fork_watchdog.purge_below( bi.topology().height() );
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

P2PNode::SeenStats P2PNode::seen_stats()
{
  return { _seen_blocks.stats(), _seen_transactions.stats() };
}

bool P2PNode::is_pending_transaction( const std::string& id )
{
  if( !_pending_transaction )
    return false;
  try { return _pending_transaction( id ); }
  catch( const std::exception& e )
  {
    LOG( debug ) << "[p2p] Pending-ID lookup unavailable: " << e.what();
    return false; // Unknown is not known: preserve normal validation/scoring.
  }
}

bool P2PNode::get_local_block_height( const std::string& block_id, uint64_t& height )
{
  if( !_block_store || block_id.empty() )
    return false;

  rpc::block_store::get_blocks_by_id_request request;
  request.add_block_ids( block_id );
  request.set_return_block( false );
  request.set_return_receipt( false );

  auto response = _block_store->get_blocks_by_id( request );
  if( response.block_items_size() != 1 )
    return false;

  height = response.block_items( 0 ).block_height();
  return true;
}

std::string P2PNode::get_local_ancestor_block_id( const std::string& head_block_id, uint64_t height )
{
  if( !_block_store || head_block_id.empty() || height == 0 )
    return {};

  rpc::block_store::get_blocks_by_height_request request;
  request.set_head_block_id( head_block_id );
  request.set_ancestor_start_height( height );
  request.set_num_blocks( 1 );
  request.set_return_block( false );
  request.set_return_receipt( false );

  auto response = _block_store->get_blocks_by_height( request );
  if( response.block_items_size() != 1 )
    return {};

  return response.block_items( 0 ).block_id();
}

bool P2PNode::report_peer_error( const PeerID& peer, const std::string& error, uint64_t score )
{
  LOG( debug ) << "[p2p] Peer error from " << peer.id << ": " << error << " (score: " << score << ")";

  const auto result = _error_handler.record_error( peer_score_address( peer ), score );
  if( result.local_capacity )
  {
    LOG( debug ) << "[p2p] Local peer-error bookkeeping capacity unavailable; closing connection without peer penalty";
    _transport->disconnect_peer( peer );
    return true;
  }
  const auto current_score = result.score;

  if( result.disconnect )
  {
    LOG( warning ) << "[p2p] Disconnecting peer " << peer.id
                   << " (score threshold exceeded: " << current_score
                   << "/" << _opts.error_score_disconnect_threshold
                   << ", last error: " << error << ")";
    _transport->disconnect_peer( peer );
    return true;
  }

  if( is_transport_disconnect_error( error ) )
  {
    LOG( warning ) << "[p2p] Disconnecting stale peer " << peer.id
                   << " after transport error: " << error
                   << " (score: " << current_score
                   << "/" << _opts.error_score_disconnect_threshold << ")";
    _transport->disconnect_peer( peer );
    return true;
  }

  return false;
}

uint64_t P2PNode::score_for_error( const std::string& error ) const
{
  if( contains_case_insensitive( error, "timeout" ) || contains_case_insensitive( error, "timed out" ) )
    return _opts.score_peer_rpc_timeout;
  return _opts.score_peer_rpc_error;
}

} // namespace koinos::node::p2p
