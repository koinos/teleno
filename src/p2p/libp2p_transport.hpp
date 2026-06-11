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

namespace libp2p::protocol::kademlia {
class Kademlia;
}

namespace koinos::node::p2p {

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
  std::vector< PeerID > connected_peers() const override;
  std::vector< PeerID > known_peers() const override;

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

  Config _config;
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
  std::set< std::string > _connecting;
  std::map< std::string, PeerID > _known_peers;
};

} // namespace koinos::node::p2p

#endif // KOINOS_HAS_LIBP2P
