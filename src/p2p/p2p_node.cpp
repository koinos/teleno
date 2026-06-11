#include "p2p_node.hpp"

#include "p2p/gorpc_codec.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iterator>
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
                  std::unique_ptr< ITransport > transport )
    : _opts( opts ),
      _chain( chain ),
      _block_store( block_store ),
      _event_bus( event_bus ),
      _transport( std::move( transport ) ),
      _error_handler( opts )
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
  _running = true;

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
    _event_bus->on_block_accepted.connect( [this]( const broadcast::block_accepted& ba ) { on_block_accepted( ba ); } );
    _event_bus->on_block_irreversible.connect(
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

    while( _running )
    {
      refresh_known_peer_candidates();

      const auto now = std::chrono::steady_clock::now();
      if( now - last_seed_cycle >= _opts.seed_reconnect_interval )
      {
        last_seed_cycle = now;
        for( const auto& seed: _seed_peers )
        {
          if( !_running )
            break;
          if( !_error_handler.can_connect( seed.address ) )
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
      log_peer_snapshot();

      auto loop_interval = _opts.seed_reconnect_interval;
      if( _opts.peer_discovery_enabled )
        loop_interval = std::min( loop_interval, _opts.peer_acquisition_interval );
      sleep_while_running( _running, loop_interval );
    }
  } );

  LOG( info ) << "[p2p] Started with " << _seed_peers.size()
              << " seed peers, target peer count " << _opts.target_peer_count;
}

void P2PNode::stop()
{
  if( !_running.exchange( false ) )
    return;

  if( _gossip_toggle )
    _gossip_toggle->stop();

  // Cancel all peer sync threads.
  std::vector< std::thread > sync_threads;
  {
    std::lock_guard lock( _peers_mutex );
    for( auto& [id, state]: _peers )
    {
      if( state->sync_thread.joinable() )
        sync_threads.emplace_back( std::move( state->sync_thread ) );
    }
    _peers.clear();
  }

  for( auto& thread: sync_threads )
    if( thread.joinable() )
      thread.join();

  if( _reconnect_thread.joinable() )
    _reconnect_thread.join();

  _transport->stop();
}

uint32_t P2PNode::connected_peer_count() const
{
  return _transport->connected_peer_count();
}

void P2PNode::log_peer_snapshot()
{
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
  if( !has_dialable_address( peer ) )
    return;

  std::lock_guard lock( _peers_mutex );
  auto [it, inserted] = _peer_candidates.emplace( peer.id, PeerCandidate{} );
  if( inserted || it->second.peer.address.empty() || seed )
    it->second.peer = peer;
  it->second.seed = it->second.seed || seed;

  if( _opts.max_peer_candidates > 0
      && _peer_candidates.size() > _opts.max_peer_candidates )
  {
    for( auto itr = _peer_candidates.begin(); itr != _peer_candidates.end(); )
    {
      if( itr->second.seed || _peers.count( itr->first ) )
      {
        ++itr;
        continue;
      }

      itr = _peer_candidates.erase( itr );
      if( _peer_candidates.size() <= _opts.max_peer_candidates )
        break;
    }
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
      if( !_error_handler.can_connect( candidate.peer.address ) )
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
  std::lock_guard lock( _peers_mutex );

  if( _peers.count( peer.id ) )
    return; // Already connected

  if( has_dialable_address( peer ) )
  {
    auto [candidate_it, inserted] = _peer_candidates.emplace( peer.id, PeerCandidate{} );
    candidate_it->second.peer = peer;
  }

  auto state = std::make_unique< PeerState >();
  state->peer = peer;

  // Start sync loop in dedicated thread
  auto* raw_state = state.get();
  state->sync_thread = std::thread( [this, p = peer]() { peer_sync_loop( p ); } );

  _peers[ peer.id ] = std::move( state );
  LOG( info ) << "[p2p] Peer connected: " << peer.id;
}

void P2PNode::on_peer_disconnected( const PeerID& peer )
{
  std::lock_guard lock( _peers_mutex );

  auto it = _peers.find( peer.id );
  if( it == _peers.end() )
    return;

  if( it->second->sync_thread.joinable() )
    it->second->sync_thread.detach();

  _peers.erase( it );
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

void P2PNode::peer_sync_loop( PeerID peer )
{
  // Handshake with retries
  while( _running )
  {
    try
    {
      if( peer_handshake( peer ) )
        break;
      if( _error_handler.get_score( peer.address ) >= _opts.error_score_disconnect_threshold )
        return;
    }
    catch( const std::exception& e )
    {
      if( report_peer_error( peer, e.what(), score_for_error( e.what() ) ) )
        return;
    }

    // Backoff
    for( int i = 0; i < 60 && _running; ++i )
      std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
  }

  // Sync loop
  bool is_synced = false;
  while( _running )
  {
    try
    {
      request_sync_blocks( peer, is_synced );
      if( _error_handler.get_score( peer.address ) >= _opts.error_score_disconnect_threshold )
        return;
    }
    catch( const std::exception& e )
    {
      if( report_peer_error( peer, e.what(), score_for_error( e.what() ) ) )
        return;
    }

    auto sleep_time = is_synced ? _opts.sync_check_interval : _opts.syncing_check_interval;
    auto sleep_ms = std::chrono::duration_cast< std::chrono::milliseconds >( sleep_time ).count();
    for( int64_t i = 0; i < sleep_ms / 100 && _running; ++i )
      std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
  }
}

bool P2PNode::peer_handshake( const PeerID& peer )
{
  if( !_chain )
    return false;

  // Verify chain ID matches
  auto peer_chain_id = _transport->peer_get_chain_id( peer );
  if( peer_chain_id != _chain_id )
  {
    report_peer_error( peer, "chain ID mismatch", _opts.score_chain_id_mismatch );
    return false;
  }

  auto peer_head = _transport->peer_get_head_block( peer );
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
    if( peer_checkpoint != checkpoint.block_id )
    {
      report_peer_error( peer, "checkpoint mismatch", _opts.score_checkpoint_mismatch );
      return false;
    }
  }

  LOG( info ) << "[p2p] Handshake complete with " << peer.id;
  return true;
}

bool P2PNode::request_sync_blocks( const PeerID& peer, bool& is_synced )
{
  if( !_chain || !_block_store )
    return false;

  std::lock_guard sync_lock( _sync_mutex );

  auto local_head_info = _chain->get_head_info();
  auto local_head = local_head_info.head_topology();
  uint64_t local_height = local_head.height();
  uint64_t local_lib_height = std::min( local_head_info.last_irreversible_block(), local_height );

  // Get peer's head
  auto peer_head = _transport->peer_get_head_block( peer );
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
    if( ancestor_id != sync_anchor_id )
    {
      report_peer_error( peer, "chain not connected at LIB", _opts.score_chain_not_connected );
      return false;
    }
  }

  // Calculate batch
  uint64_t blocks_needed = peer_head.height - local_lib_height;
  uint32_t batch = std::min( static_cast< uint64_t >( _opts.block_request_batch_size ), blocks_needed );

  // Fetch blocks
  auto blocks = _transport->peer_get_blocks( peer, peer_head.block_id, local_lib_height + 1, batch );

  if( blocks.empty() )
    return true;

  // Apply sequentially
  uint64_t expected_height = local_lib_height + 1;
  std::string expected_previous = sync_anchor_id;
  for( const auto& block: blocks )
  {
    if( !_running )
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
      {
        std::lock_guard lock( _seen_mutex );
        _seen_blocks.insert( block.id() );
      }
    }
    catch( const std::exception& e )
    {
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
  if( block.id().empty() || !block.has_header() )
  {
    report_peer_error( from, "invalid gossiped block", _opts.score_deserialization );
    return;
  }

  // Check fork bomb
  if( _fork_watchdog.check( block.id(), block.signature(), block.header().previous(), block.header().height() ) )
  {
    report_peer_error( from, "fork bomb via gossip", _opts.score_fork_bomb );
    return;
  }

  // Check height >= LIB
  if( block.header().height() <= _lib_height.load() )
    return;

  {
    std::lock_guard lock( _seen_mutex );
    if( _seen_blocks.count( block.id() ) )
      return;
  }

  // Apply
  if( _chain )
  {
    try
    {
      const auto local_head = _chain->get_head_info();
      const auto local_height = local_head.head_topology().height();
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
      {
        std::lock_guard lock( _seen_mutex );
        _seen_blocks.insert( block.id() );
      }
    }
    catch( const std::exception& e )
    {
      report_peer_error( from, std::string( "gossip block: " ) + e.what(), _opts.score_block_application );
    }
  }
}

void P2PNode::on_gossip_transaction( const PeerID& from, const protocol::transaction& tx )
{
  if( tx.id().empty() )
  {
    report_peer_error( from, "invalid gossiped transaction", _opts.score_deserialization );
    return;
  }

  if( _chain )
  {
    {
      std::lock_guard lock( _seen_mutex );
      if( _seen_transactions.count( tx.id() ) )
        return;
    }

    try
    {
      rpc::chain::submit_transaction_request req;
      *req.mutable_transaction() = tx;
      _chain->submit_transaction( req );
      {
        std::lock_guard lock( _seen_mutex );
        _seen_transactions.insert( tx.id() );
      }
    }
    catch( const std::exception& e )
    {
      report_peer_error( from, std::string( "gossip tx: " ) + e.what(), _opts.score_transaction_application );
    }
  }
}

// ---------------------------------------------------------------------------
// EventBus handlers
// ---------------------------------------------------------------------------

void P2PNode::on_block_accepted( const broadcast::block_accepted& ba )
{
  if( !ba.has_block() )
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
  _lib_height.store( bi.topology().height() );
  _fork_watchdog.purge_below( bi.topology().height() );
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

  const bool threshold_exceeded = _error_handler.record_error( peer.address, score );
  const auto current_score      = _error_handler.get_score( peer.address );

  if( threshold_exceeded )
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
