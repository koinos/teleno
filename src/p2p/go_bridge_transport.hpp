#pragma once

/**
 * Go P2P Bridge Transport — fallback ITransport using the existing Go P2P binary.
 *
 * When cpp-libp2p cannot be compiled (e.g., Xcode 26 beta + Hunter incompatibility),
 * this transport runs the Go koinos-p2p binary as a child process and communicates
 * with it via a local JSON-RPC bridge on a Unix socket or TCP loopback.
 *
 * Architecture:
 *   koinos_node (C++) ←→ local IPC ←→ koinos-p2p (Go binary)
 *                                      ↕
 *                                   mainnet peers
 *
 * The Go binary handles all libp2p networking (GossipSub, peer RPC, NAT).
 * The C++ monolith handles chain, block_store, mempool, etc.
 *
 * This is the same architecture as the current multi-service setup but with
 * only 2 processes instead of 12, and no AMQP broker.
 *
 * Enable with: -DKOINOS_USE_GO_P2P=ON
 */

#include "transport.hpp"
#include "types.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

namespace koinos::node::p2p {

class GoBridgeTransport final : public ITransport
{
public:
  struct Config
  {
    std::string go_p2p_binary;  // Path to koinos-p2p Go binary
    std::string basedir;        // Shared basedir with the monolith
    std::string amqp_url;       // Not used — placeholder for config compat
    std::string listen_address = "/ip4/0.0.0.0/tcp/8888";
    std::vector< std::string > seed_peers;
    uint16_t bridge_port = 0;   // Local bridge port (0 = auto)
  };

  explicit GoBridgeTransport( const Config& config );
  ~GoBridgeTransport() override;

  void start() override;
  void stop() override;

  void connect_peer( const PeerID& peer ) override;
  void disconnect_peer( const PeerID& peer ) override;
  uint32_t connected_peer_count() const override;
  std::vector< PeerID > connected_peers() const override;
  std::vector< PeerID > known_peers() const override;

  std::string peer_get_chain_id( const PeerID& peer ) override;
  PeerHeadInfo peer_get_head_block( const PeerID& peer ) override;
  std::string peer_get_ancestor_block_id( const PeerID& peer,
                                           const std::string& head_id,
                                           uint64_t height ) override;
  std::vector< protocol::block > peer_get_blocks( const PeerID& peer,
                                                   const std::string& head_id,
                                                   uint64_t start_height,
                                                   uint32_t count ) override;

  void publish_block( const protocol::block& block ) override;
  void publish_transaction( const protocol::transaction& tx ) override;

  void on_peer_connected( PeerConnectedCallback cb ) override;
  void on_peer_disconnected( PeerDisconnectedCallback cb ) override;
  void on_block_received( BlockReceivedCallback cb ) override;
  void on_transaction_received( TxReceivedCallback cb ) override;
  void on_peer_rpc_request( PeerRpcRequestCallback cb ) override;

private:
  /** Spawn the Go P2P binary as a child process. */
  void spawn_go_process();

  /** Monitor the Go process and restart if it crashes. */
  void monitor_loop();

  /** Send a command to the Go bridge via local IPC. */
  std::string bridge_call( const std::string& method, const std::string& params );

  Config _config;
  std::atomic< bool > _running{ false };

  // Child process
  pid_t _go_pid = 0;
  std::thread _monitor_thread;

  // Callbacks
  PeerConnectedCallback _on_connected;
  PeerDisconnectedCallback _on_disconnected;
  BlockReceivedCallback _on_block;
  TxReceivedCallback _on_tx;
  PeerRpcRequestCallback _on_peer_rpc_request;

  // Bridge IO
  std::shared_ptr< boost::asio::io_context > _io;
  uint32_t _peer_count = 0;
};

} // namespace koinos::node::p2p
