#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <koinos/log.hpp>

#include "interfaces/i_block_store.hpp"
#include "interfaces/i_chain.hpp"
#include "core/event_bus.hpp"

#include "error_handler.hpp"
#include "fork_watchdog.hpp"
#include "gossip_toggle.hpp"
#include "transport.hpp"
#include "types.hpp"

namespace koinos::node::p2p {

/**
 * P2P node — manages peer connections, block sync, gossip.
 * Replaces the Go koinos-p2p service.
 *
 * Architecture:
 * - ITransport: abstract network layer (cpp-libp2p implementation pending)
 * - PeerErrorHandler: scoring with exponential decay
 * - ForkWatchdog: fork bomb detection
 * - GossipToggle: enable/disable based on sync state
 * - Sync protocol: batch fetch from peers, sequential application
 */
class P2PNode
{
public:
  P2PNode( const P2POptions& opts,
           IChain* chain,
           IBlockStore* block_store,
           EventBus* event_bus,
           std::unique_ptr< ITransport > transport );

  ~P2PNode();

  void start();
  void stop();

  uint32_t connected_peer_count() const;
  std::vector< PeerID > connected_peers() const;
  std::vector< PeerID > known_peers() const;

private:
  // ── Peer sync state ──
  struct PeerState
  {
    PeerID peer;
    bool synced           = false;
    bool handshake_done   = false;
    std::thread sync_thread;
  };

  struct PeerCandidate
  {
    PeerID peer;
    bool seed = false;
    uint32_t attempts = 0;
    std::chrono::steady_clock::time_point last_dial{};
  };

  // ── Peer lifecycle ──
  void on_peer_connected( const PeerID& peer );
  void on_peer_disconnected( const PeerID& peer );
  std::string handle_peer_rpc_request( const std::string& service,
                                       const std::string& method,
                                       const std::string& args );

  // ── Sync protocol (runs per peer in dedicated thread) ──
  void peer_sync_loop( PeerID peer );
  bool peer_handshake( const PeerID& peer );
  bool request_sync_blocks( const PeerID& peer, bool& is_synced );

  // ── Gossip handlers ──
  void on_gossip_block( const PeerID& from, const protocol::block& block );
  void on_gossip_transaction( const PeerID& from, const protocol::transaction& tx );

  // ── EventBus handlers ──
  void on_block_accepted( const broadcast::block_accepted& ba );
  void on_block_irreversible( const broadcast::block_irreversible& bi );

  // ── Helpers ──
  void log_peer_snapshot();
  void add_peer_candidate( const PeerID& peer, bool seed );
  void refresh_known_peer_candidates();
  void run_peer_acquisition_cycle();
  bool is_peer_connected_locked( const std::string& peer_id ) const;
  bool get_local_block_height( const std::string& block_id, uint64_t& height );
  std::string get_local_ancestor_block_id( const std::string& head_block_id, uint64_t height );
  bool report_peer_error( const PeerID& peer, const std::string& error, uint64_t score );
  uint64_t score_for_error( const std::string& error ) const;

  P2POptions _opts;
  IChain* _chain;
  IBlockStore* _block_store;
  EventBus* _event_bus;
  std::unique_ptr< ITransport > _transport;

  PeerErrorHandler _error_handler;
  ForkWatchdog _fork_watchdog;
  std::unique_ptr< GossipToggle > _gossip_toggle;

  std::mutex _peers_mutex;
  std::map< std::string, std::unique_ptr< PeerState > > _peers; // peer.id → state
  std::map< std::string, PeerCandidate > _peer_candidates;
  std::mutex _sync_mutex;
  std::mutex _seen_mutex;
  std::set< std::string > _seen_blocks;
  std::set< std::string > _seen_transactions;

  std::atomic< bool > _running{ false };
  std::atomic< uint64_t > _lib_height{ 0 };
  std::string _chain_id;

  // Seed peers for reconnection
  std::vector< PeerID > _seed_peers;
  std::thread _reconnect_thread;
};

} // namespace koinos::node::p2p
