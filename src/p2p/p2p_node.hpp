#pragma once

#include <atomic>
#include <map>
#include <list>
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
#include "seen_cache.hpp"
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
  using PendingTransactionLookup = std::function< bool( const std::string& ) >;
  P2PNode( const P2POptions& opts,
           IChain* chain,
           IBlockStore* block_store,
           EventBus* event_bus,
           std::unique_ptr< ITransport > transport,
           PendingTransactionLookup pending_transaction = {} );

  ~P2PNode();

  void start();
  void stop();

  uint32_t connected_peer_count() const;
  PeerSnapshot connected_peers() const;
  PeerSnapshot known_peers() const;

  struct SeenStats { SeenCache::Stats blocks; SeenCache::Stats transactions; };
  SeenStats seen_stats();
  struct WorkerStats { size_t connected; size_t owned; size_t retired; size_t limit; uint64_t refused; };
  WorkerStats worker_stats();
  struct BookkeepingStats
  {
    size_t candidates = 0, candidate_limit = 0, candidate_effective_limit = 0, configured_seeds = 0;
    size_t protected_candidates = 0, candidate_string_capacity_bytes = 0;
    uint64_t candidate_evictions = 0, candidate_allocation_refusals = 0, discovery_allocation_refusals = 0, discovery_snapshot_refusals = 0;
    PeerErrorHandler::Stats errors{};
  };
  BookkeepingStats bookkeeping_stats();

private:
  // ── Peer sync state ──
  struct PeerState
  {
    PeerID peer;
    std::atomic< bool > cancelled{ false };
    std::atomic< bool > finished{ false };
    uint32_t response_batch_limit = 0; // Worker-owned; zero uses the configured request count.
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
  void start_runtime(); // caller holds lifecycle lock; start() owns rollback
  void on_peer_connected( const PeerID& peer );
  void on_peer_disconnected( const PeerID& peer );
  std::string handle_peer_rpc_request( const std::string& service,
                                       const std::string& method,
                                       const std::string& args );

  // ── Sync protocol (runs per peer in dedicated thread) ──
  void peer_sync_loop( const std::shared_ptr< PeerState >& state );
  bool peer_handshake( const std::shared_ptr< PeerState >& state );
  bool request_sync_blocks( const std::shared_ptr< PeerState >& state, bool& is_synced );
  bool worker_active( const std::shared_ptr< PeerState >& state ) const;
  void reap_finished_workers_locked();

  // ── Gossip handlers ──
  void on_gossip_block( const PeerID& from, const protocol::block& block );
  void on_gossip_transaction( const PeerID& from, const protocol::transaction& tx );

  // ── EventBus handlers ──
  void on_block_accepted( const broadcast::block_accepted& ba );
  void on_block_irreversible( const broadcast::block_irreversible& bi );

  // ── Helpers ──
  void log_peer_snapshot();
  void add_peer_candidate( const PeerID& peer, bool seed );
  void remember_peer_candidate_locked( const PeerID& peer, bool seed, bool replace ) noexcept;
  void trim_peer_candidates_locked() noexcept;
  void refresh_known_peer_candidates();
  void run_peer_acquisition_cycle();
  bool is_peer_connected_locked( const std::string& peer_id ) const;
  bool get_local_block_height( const std::string& block_id, uint64_t& height );
  std::string get_local_ancestor_block_id( const std::string& head_block_id, uint64_t height );
  bool report_peer_error( const PeerID& peer, const std::string& error, uint64_t score );
  uint64_t score_for_error( const std::string& error ) const;
  bool is_pending_transaction( const std::string& id );

  P2POptions _opts;
  IChain* _chain;
  IBlockStore* _block_store;
  EventBus* _event_bus;
  std::unique_ptr< ITransport > _transport;

  PeerErrorHandler _error_handler;
  ForkWatchdog _fork_watchdog;
  std::unique_ptr< GossipToggle > _gossip_toggle;

  std::mutex _peers_mutex;
  std::mutex _lifecycle_mutex; // serialize start/stop; never held by sync workers
  std::map< std::string, std::shared_ptr< PeerState > > _peers; // current connected generations
  std::list< std::shared_ptr< PeerState > > _sync_workers; // includes disconnected generations until join
  uint64_t _worker_refusals = 0;
  uint64_t _candidate_evictions = 0, _candidate_allocation_refusals = 0, _discovery_allocation_refusals = 0, _discovery_snapshot_refusals = 0;
  size_t _draining_workers = 0;
  std::map< std::string, PeerCandidate > _peer_candidates;
  std::mutex _sync_mutex;
  SeenCache _seen_blocks;
  SeenCache _seen_transactions;
  PendingTransactionLookup _pending_transaction;
  boost::signals2::scoped_connection _accepted_connection;
  boost::signals2::scoped_connection _irreversible_connection;

  std::atomic< bool > _running{ false };
  std::atomic< uint64_t > _lib_height{ 0 };
  std::string _chain_id;

  // Seed peers for reconnection
  std::vector< PeerID > _seed_peers;
  std::thread _reconnect_thread;
};

} // namespace koinos::node::p2p
