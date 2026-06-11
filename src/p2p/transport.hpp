#pragma once

#include <functional>
#include <string>
#include <vector>

#include <koinos/protocol/protocol.pb.h>

#include "types.hpp"

namespace koinos::node::p2p {

/**
 * Abstract transport layer for P2P networking.
 * To be implemented by cpp-libp2p when available.
 * For now, enables building and testing all P2P logic without the actual network layer.
 */
class ITransport
{
public:
  virtual ~ITransport() = default;

  // ── Lifecycle ──
  virtual void start() = 0;
  virtual void stop()  = 0;

  // ── Peer management ──
  virtual void connect_peer( const PeerID& peer )    = 0;
  virtual void disconnect_peer( const PeerID& peer )  = 0;
  virtual uint32_t connected_peer_count() const       = 0;
  virtual std::vector< PeerID > connected_peers() const = 0;
  virtual std::vector< PeerID > known_peers() const     = 0;

  // ── Peer RPC (outbound calls to a specific peer) ──
  virtual std::string peer_get_chain_id( const PeerID& peer ) = 0;
  virtual PeerHeadInfo peer_get_head_block( const PeerID& peer ) = 0;
  virtual std::string peer_get_ancestor_block_id( const PeerID& peer,
                                                   const std::string& head_id,
                                                   uint64_t height ) = 0;
  virtual std::vector< protocol::block > peer_get_blocks( const PeerID& peer,
                                                           const std::string& head_id,
                                                           uint64_t start_height,
                                                           uint32_t count ) = 0;

  // ── GossipSub ──
  virtual void publish_block( const protocol::block& block )             = 0;
  virtual void publish_transaction( const protocol::transaction& tx )    = 0;

  // ── Callbacks (set by P2PNode before start) ──
  using PeerConnectedCallback    = std::function< void( const PeerID& ) >;
  using PeerDisconnectedCallback = std::function< void( const PeerID& ) >;
  using BlockReceivedCallback    = std::function< void( const PeerID&, const protocol::block& ) >;
  using TxReceivedCallback       = std::function< void( const PeerID&, const protocol::transaction& ) >;
  using PeerRpcRequestCallback   = std::function< std::string( const std::string&,
                                                               const std::string&,
                                                               const std::string& ) >;

  virtual void on_peer_connected( PeerConnectedCallback cb )       = 0;
  virtual void on_peer_disconnected( PeerDisconnectedCallback cb ) = 0;
  virtual void on_block_received( BlockReceivedCallback cb )       = 0;
  virtual void on_transaction_received( TxReceivedCallback cb )    = 0;
  virtual void on_peer_rpc_request( PeerRpcRequestCallback cb )    = 0;
};

} // namespace koinos::node::p2p
