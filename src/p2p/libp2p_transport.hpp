#pragma once

/**
 * Concrete ITransport implementation using cpp-libp2p.
 *
 * Provides:
 * - libp2p Host for peer connections (Noise encryption, mplex/yamux muxing)
 * - GossipSub for block/transaction gossip (topics: koinos.blocks, koinos.transactions)
 * - Custom peer RPC protocol (/koinos/peerrpc/1.0.0) for sync operations
 * - NAT traversal (UPnP, AutoRelay, hole punching)
 * - Kademlia DHT for peer discovery
 *
 * Build requirement: cpp-libp2p (https://github.com/libp2p/cpp-libp2p)
 * Enable with CMake: -DKOINOS_ENABLE_LIBP2P=ON
 */

#ifdef KOINOS_HAS_LIBP2P

#include "transport.hpp"
#include "types.hpp"
#include "peer_dial_budget.hpp"
#include "peer_advice_store.hpp"
#include "peer_snapshot_budget.hpp"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

// cpp-libp2p headers
#include <libp2p/host/host.hpp>
#include <libp2p/protocol/gossip/gossip.hpp>
#include <libp2p/protocol/common/subscription.hpp>
#include <libp2p/peer/peer_id.hpp>
#include <libp2p/multi/multiaddress.hpp>
#include "sdk_address_repository.hpp"

namespace libp2p::protocol::kademlia {
class Kademlia;
}

namespace koinos::node::p2p {

namespace detail { class RpcRequestRead; }

class Libp2pTransport final : public ITransport
{
public:
	  struct Config
	  {
	    std::string listen_address = "/ip4/0.0.0.0/tcp/8888";
	    std::vector< std::string > seed_peers;
	    std::vector< std::string > discovery_peers;
	    std::string identity_key_path; // Ed25519 key file
	    bool enable_dht = true;
	    bool enable_dht_local = false;
	    std::string protocol_version = "koinos/p2p/1.0.0";
	    unsigned int requested_io_threads = 1;
        size_t max_pending_dials = 32;
        size_t pending_dial_peer_capacity_bytes = 1024 * 1024;
        size_t max_known_peer_advice = 1024;
        size_t known_peer_advice_capacity_bytes = 1024 * 1024;
        size_t max_peer_snapshots = 4;
        size_t peer_snapshot_entries = 4096;
        size_t peer_snapshot_capacity_bytes = 4 * 1024 * 1024;
        detail::SdkAddressLimits sdk_address_limits;
        uint32_t sdk_metadata_collection_interval_ms = 60'000;
	  };

  explicit Libp2pTransport( const Config& config );
  ~Libp2pTransport() override;

  // ── ITransport lifecycle ──
  void start() override;
  void stop() override;

  // ── Peer management ──
  void connect_peer( const PeerID& peer ) override;
  void disconnect_peer( const PeerID& peer ) override;
  uint32_t connected_peer_count() const override;
  PeerSnapshot connected_peers() const override;
  PeerSnapshot known_peers() const override;
  detail::DialBudget::Stats dial_stats() const { return _dial_budget->stats(); }
  detail::PeerSnapshotBudget::Stats peer_snapshot_stats() const { return _peer_snapshot_budget->stats(); }
  detail::PeerAdviceStore::Stats peer_advice_stats() const { std::lock_guard lock(_peers_mutex);return _known_peers.stats(); }
  detail::SdkAddressBudget::Stats sdk_address_stats() const { return _sdk_address_budget->stats(); }

  // ── Peer RPC (custom protocol /koinos/peerrpc/1.0.0) ──
  std::string peer_get_chain_id( const PeerID& peer ) override;
  PeerHeadInfo peer_get_head_block( const PeerID& peer ) override;
  std::string peer_get_ancestor_block_id( const PeerID& peer,
                                           const std::string& head_id,
                                           uint64_t height ) override;
  std::vector< protocol::block > peer_get_blocks( const PeerID& peer,
                                                   const std::string& head_id,
                                                   uint64_t start_height,
                                                   uint32_t count ) override;

  // ── GossipSub ──
  void publish_block( const protocol::block& block ) override;
  void publish_transaction( const protocol::transaction& tx ) override;

  // ── Callbacks ──
  void on_peer_connected( PeerConnectedCallback cb ) override;
  void on_peer_disconnected( PeerDisconnectedCallback cb ) override;
  void on_block_received( BlockReceivedCallback cb ) override;
  void on_transaction_received( TxReceivedCallback cb ) override;
  void on_peer_rpc_request( PeerRpcRequestCallback cb ) override;

private:
  // ── GossipSub topic names ──
  static constexpr const char* BLOCK_TOPIC       = "koinos.blocks";
  static constexpr const char* TRANSACTION_TOPIC  = "koinos.transactions";

  // ── Peer RPC protocol ID ──
  static constexpr const char* PEER_RPC_PROTOCOL = "/koinos/peerrpc/1.0.0";

  /** Send an RPC request to a peer over a libp2p stream. */
  std::string send_peer_rpc( const PeerID& peer,
                              const std::string& service,
                              const std::string& method,
                              const std::string& msgpack_args );

  /** Handle incoming peer RPC requests (server side). */
  void handle_incoming_rpc( std::shared_ptr< libp2p::connection::Stream > stream );

  /** Set up GossipSub topics and message handlers. */
  void setup_gossipsub();

  /** Handle received gossip message. */
  void on_gossip_message( const std::string& topic, const std::string& data,
                           const libp2p::peer::PeerId& from );

  /** Convert between PeerID types. */
  static PeerID to_peer_id( const libp2p::peer::PeerId& pid );

  // Caller holds _peers_mutex; optional advice must not suppress connection notification.
  void remember_known_peer_locked( const PeerID& peer, bool seed = false ) noexcept;
  bool publish_connected_peer_locked( const PeerID& peer );

  Config _config;
  std::shared_ptr< detail::DialBudget > _dial_budget;
  std::shared_ptr< detail::PeerSnapshotBudget > _peer_snapshot_budget;
  std::shared_ptr< detail::SdkAddressBudget > _sdk_address_budget;
  struct Libp2pRuntime;
  std::unique_ptr< Libp2pRuntime > _runtime;
	  std::shared_ptr< libp2p::Host > _host;
	  std::shared_ptr< libp2p::protocol::gossip::Gossip > _gossip;
	  std::shared_ptr< libp2p::protocol::kademlia::Kademlia > _kademlia;

  // GossipSub subscriptions (RAII handles — cancel on destruction)
  std::optional< libp2p::protocol::Subscription > _block_sub;
  std::optional< libp2p::protocol::Subscription > _tx_sub;

  // Callbacks
  PeerConnectedCallback _on_connected;
  PeerDisconnectedCallback _on_disconnected;
  BlockReceivedCallback _on_block;
  TxReceivedCallback _on_tx;
  PeerRpcRequestCallback _on_peer_rpc_request;
  std::shared_ptr< std::atomic< bool > > _rpc_read_active;
  struct RpcClientContext;
  // Published/retired atomically; outbound callers never race mutable _host.
  std::shared_ptr< RpcClientContext > _rpc_client_context;
  // Accessed by the serialized I/O runner, or after that runner has joined.
  std::vector< std::weak_ptr< detail::RpcRequestRead > > _rpc_reads;

  // IO
  std::shared_ptr< boost::asio::io_context > _io;
  using IoWorkGuard = boost::asio::executor_work_guard< boost::asio::io_context::executor_type >;
  std::optional< IoWorkGuard > _work_guard;
  std::vector< std::thread > _io_threads;
  std::atomic< bool > _running{ false };

  libp2p::event::Handle _new_connection_subscription;
  libp2p::event::Handle _peer_disconnected_subscription;

  mutable std::mutex _peers_mutex;
  std::map< std::string, PeerID > _connected;
  detail::PeerAdviceStore _known_peers;
};

} // namespace koinos::node::p2p

#endif // KOINOS_HAS_LIBP2P
