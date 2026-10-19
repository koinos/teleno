/**
 * cpp-libp2p transport implementation.
 *
 * Wire-compatible with Go koinos-p2p:
 * - GossipSub topics: koinos.blocks, koinos.transactions
 * - Peer RPC protocol: /koinos/peerrpc/1.0.0 with gorpc framing
 *   (two MessagePack objects: ServiceID header followed by args/body)
 * - Noise encryption, mplex/yamux muxing
 * - Ed25519 identity keys
 *
 * Compile with -DKOINOS_ENABLE_LIBP2P=ON
 */

#ifdef KOINOS_HAS_LIBP2P

#include "p2p/gorpc_codec.hpp"
#include "p2p/peer_rpc_read.hpp"
#include "p2p/peer_rpc_write.hpp"
#include "p2p/peer_rpc_client.hpp"
#include "p2p/sdk_metadata_maintenance.hpp"
#include "libp2p_transport.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <koinos/log.hpp>
#include <koinos/protocol/protocol.pb.h>

// cpp-libp2p
#include <boost/di.hpp>
#include <boost/di/extension/scopes/shared.hpp>
#include <libp2p/common/literals.hpp>
#include <libp2p/common/types.hpp>
#include <libp2p/crypto/key_marshaller/key_marshaller_impl.hpp>
#include <libp2p/crypto/random_generator.hpp>
#include <libp2p/event/bus.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <libp2p/log/logger.hpp>
#include <libp2p/muxer/yamux/yamux.hpp>
#include <libp2p/peer/identity_manager.hpp>
#include <libp2p/protocol/kademlia/config.hpp>
#include <libp2p/protocol/kademlia/impl/content_routing_table_impl.hpp>
#include <libp2p/protocol/kademlia/impl/kademlia_impl.hpp>
#include <libp2p/protocol/kademlia/impl/peer_routing_table_impl.hpp>
#include <libp2p/protocol/kademlia/impl/storage_backend_default.hpp>
#include <libp2p/protocol/kademlia/impl/storage_impl.hpp>
#include <libp2p/protocol/kademlia/impl/validator_default.hpp>
#include <soralog/impl/fallback_configurator.hpp>

namespace {

struct DialMultiaddresses
{
  std::vector< libp2p::multi::Multiaddress > addresses;
  std::vector< std::string > address_strings;
  bool resolved_dns = false;
};

std::vector< std::string > split_multiaddress( const std::string& address )
{
  std::vector< std::string > parts;
  std::size_t pos = 0;
  while( pos < address.size() )
  {
    if( address[ pos ] == '/' )
      ++pos;

    auto next = address.find( '/', pos );
    auto part = address.substr( pos, next == std::string::npos ? std::string::npos : next - pos );
    if( !part.empty() )
      parts.push_back( part );

    if( next == std::string::npos )
      break;
    pos = next + 1;
  }
  return parts;
}

std::string join_multiaddress( const std::vector< std::string >& parts )
{
  std::ostringstream out;
  for( const auto& part: parts )
    out << '/' << part;
  return out.str();
}

std::optional< std::string > protocol_value( const std::vector< std::string >& parts,
                                             const std::string& protocol )
{
  for( std::size_t i = 0; i + 1 < parts.size(); ++i )
    if( parts[ i ] == protocol )
      return parts[ i + 1 ];
  return std::nullopt;
}

bool has_peer_id_component( const std::string& address )
{
  auto parts = split_multiaddress( address );
  return std::find( parts.begin(), parts.end(), "p2p" ) != parts.end();
}

std::string with_peer_id_component( const std::string& address, const std::string& peer_id )
{
  if( address.empty() || peer_id.empty() || has_peer_id_component( address ) )
    return address;
  return address + "/p2p/" + peer_id;
}

std::vector< std::string > resolve_dns_multiaddress_strings( const std::string& address )
{
  auto parts = split_multiaddress( address );
  if( parts.size() < 2 || ( parts[ 0 ] != "dns4" && parts[ 0 ] != "dns6" ) )
    return { address };

  auto port = protocol_value( parts, "tcp" );
  if( !port.has_value() )
    throw std::runtime_error( "DNS multiaddr has no TCP port: " + address );

  boost::asio::io_context io;
  boost::asio::ip::tcp::resolver resolver( io );
  boost::system::error_code ec;
  const bool wants_ipv6 = parts[ 0 ] == "dns6";
  auto results = resolver.resolve(
    wants_ipv6 ? boost::asio::ip::tcp::v6() : boost::asio::ip::tcp::v4(),
    parts[ 1 ],
    port.value(),
    ec );
  if( ec )
    throw std::runtime_error( "Failed to resolve DNS multiaddr " + address + ": " + ec.message() );

  std::vector< std::string > resolved;
  std::set< std::string > seen;
  for( const auto& result: results )
  {
    auto ip = result.endpoint().address();
    if( wants_ipv6 != ip.is_v6() )
      continue;

    boost::system::error_code to_string_ec;
    auto ip_string = ip.to_string( to_string_ec );
    if( to_string_ec || !seen.insert( ip_string ).second )
      continue;

    auto resolved_parts = parts;
    resolved_parts[ 0 ] = wants_ipv6 ? "ip6" : "ip4";
    resolved_parts[ 1 ] = ip_string;
    resolved.push_back( join_multiaddress( resolved_parts ) );
  }

  if( resolved.empty() )
    throw std::runtime_error( "DNS multiaddr resolved no usable addresses: " + address );

  return resolved;
}

DialMultiaddresses dial_multiaddresses_from( const std::string& address )
{
  DialMultiaddresses result;
  result.address_strings = resolve_dns_multiaddress_strings( address );
  result.resolved_dns    = result.address_strings.size() != 1 || result.address_strings.front() != address;

  for( const auto& address_string: result.address_strings )
  {
    auto ma = libp2p::multi::Multiaddress::create( address_string );
    if( !ma.has_value() )
      throw std::runtime_error( "Invalid multiaddr: " + address_string );
    result.addresses.push_back( ma.value() );
  }

  return result;
}

std::optional< libp2p::peer::PeerInfo > peer_info_from_multiaddress( const std::string& address )
{
  auto dial_addresses = dial_multiaddresses_from( address );
  if( dial_addresses.addresses.empty() )
    return std::nullopt;

  auto peer_id_str = dial_addresses.addresses.front().getPeerId();
  if( !peer_id_str.has_value() )
    return std::nullopt;

  auto peer_id = libp2p::peer::PeerId::fromBase58( peer_id_str.value() );
  if( !peer_id.has_value() )
    return std::nullopt;

  return libp2p::peer::PeerInfo{ peer_id.value(), dial_addresses.addresses };
}

void log_libp2p_async_error( const std::string& operation, const std::string& message )
{
  LOG( debug ) << "[p2p/transport] " << operation << " failed: " << message;
}

void log_libp2p_async_error( const std::string& operation, const std::exception& e )
{
  log_libp2p_async_error( operation, e.what() );
}

void log_libp2p_async_unknown_error( const std::string& operation )
{
  log_libp2p_async_error( operation, "unknown exception" );
}

void safe_stream_close( const std::shared_ptr< libp2p::connection::Stream >& stream,
                        const std::string& operation )
{
  if( !stream )
    return;

  try
  {
    stream->close( []( auto&& ) {} );
  }
  catch( const std::exception& e )
  {
    log_libp2p_async_error( operation + " close", e );
  }
  catch( ... )
  {
    log_libp2p_async_unknown_error( operation + " close" );
  }
}

void safe_stream_write_and_close( boost::asio::io_context& io,
                                  const std::shared_ptr< libp2p::connection::Stream >& stream,
                                  const std::shared_ptr< libp2p::Bytes >& bytes,
                                  const std::string& operation )
{
  if( !stream )
    return;
  if( !bytes )
  {
    log_libp2p_async_error( operation, "buffer unavailable" );
    safe_stream_close( stream, operation );
    return;
  }

  try
  {
    koinos::node::p2p::detail::write_rpc_frame( io, stream, bytes,
      [stream, bytes, operation]( outcome::result< size_t > result ) mutable {
        try
        {
          if( !result.has_value() )
            log_libp2p_async_error( operation, result.error().message() );
          else if( result.value() == 0 )
            log_libp2p_async_error( operation, "zero bytes written" );
        }
        catch( const std::exception& e )
        {
          log_libp2p_async_error( operation + " callback", e );
        }
        catch( ... )
        {
          log_libp2p_async_unknown_error( operation + " callback" );
        }

        safe_stream_close( stream, operation );
      }
    );
  }
  catch( const std::exception& e )
  {
    log_libp2p_async_error( operation, e );
    safe_stream_close( stream, operation );
  }
  catch( ... )
  {
    log_libp2p_async_unknown_error( operation );
    safe_stream_close( stream, operation );
  }
}

void prepare_libp2p_logging()
{
  static std::once_flag initialized;
  std::call_once( initialized, [] {
    auto logging_system = std::make_shared< soralog::LoggingSystem >(
      std::make_shared< soralog::FallbackConfigurator >( soralog::Level::ERROR ) );
    auto result = logging_system->configure();
    if( result.has_error )
      throw std::runtime_error( "Failed to configure cpp-libp2p logging: " + result.message );
    if( !result.message.empty() )
      std::cerr << result.message << std::endl;

    libp2p::log::setLoggingSystem( logging_system );
  } );

  libp2p::log::setLevelOfGroup(
    libp2p::log::defaultGroupName,
    std::getenv( "KOINOS_LIBP2P_TRACE" ) ? soralog::Level::TRACE : soralog::Level::ERROR );
}

auto make_libp2p_injector( const std::shared_ptr< boost::asio::io_context >& io,
                           const std::string& protocol_version,
                           const std::shared_ptr<koinos::node::p2p::detail::SdkAddressBudget>& address_budget )
{
  // The injector owns shared-scoped cpp-libp2p dependencies, including TLS and
  // scheduler state used by async callbacks after the host has started.
  return libp2p::injector::makeHostInjector< boost::di::extension::shared_config >(
    boost::di::bind< boost::asio::io_context >.to( io )[ boost::di::override ],
    boost::di::bind< koinos::node::p2p::detail::SdkAddressBudget >.to( address_budget )[ boost::di::override ],
    boost::di::bind< libp2p::peer::AddressRepository >.to< koinos::node::p2p::detail::SdkAddressRepository >()[ boost::di::override ],
    libp2p::injector::useLibp2pClientVersion( libp2p::Libp2pClientVersion{ protocol_version } ),
    libp2p::injector::useLayerAdaptors<>(),
    libp2p::injector::useMuxerAdaptors< libp2p::muxer::Yamux >(),
    libp2p::injector::useTransportAdaptors< libp2p::transport::TcpTransport >() );
}

} // namespace

namespace koinos::node::p2p {

using Libp2pInjector = decltype( make_libp2p_injector(
  std::declval< const std::shared_ptr< boost::asio::io_context >& >(),
  std::declval< const std::string& >(),
  std::declval< const std::shared_ptr<detail::SdkAddressBudget>& >() ) );

struct Libp2pTransport::Libp2pRuntime
{
  Libp2pInjector injector;
  libp2p::protocol::kademlia::Config kademlia_config;
  boost::signals2::scoped_connection address_advice;
  std::shared_ptr<boost::asio::steady_timer> metadata_timer;

  Libp2pRuntime( const std::shared_ptr< boost::asio::io_context >& io,
                 const std::string& protocol_version,
                 const std::shared_ptr<detail::SdkAddressBudget>& address_budget )
    : injector( make_libp2p_injector( io, protocol_version, address_budget ) )
  {}
};

struct Libp2pTransport::RpcClientContext
{
  std::shared_ptr< boost::asio::io_context > io;
  std::shared_ptr< libp2p::Host > host;
  std::shared_ptr< detail::RpcClientRegistry > calls;
  std::shared_ptr< detail::DialRegistry > dials;
  std::shared_ptr< std::atomic< bool > > active;
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Libp2pTransport::Libp2pTransport( const Config& config ) : _config( config ),
  _dial_budget(std::make_shared<detail::DialBudget>(config.max_pending_dials, config.pending_dial_peer_capacity_bytes)),
  _peer_snapshot_budget(std::make_shared<detail::PeerSnapshotBudget>(config.max_peer_snapshots, config.peer_snapshot_entries, config.peer_snapshot_capacity_bytes)),
  _sdk_address_budget(std::make_shared<detail::SdkAddressBudget>(config.sdk_address_limits)),
  _known_peers(config.max_known_peer_advice, config.known_peer_advice_capacity_bytes)
{
  if(!config.sdk_metadata_collection_interval_ms)
    throw std::invalid_argument("SDK metadata collection interval must be positive");
  _io = std::make_shared< boost::asio::io_context >();
}

Libp2pTransport::~Libp2pTransport()
{
  stop();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void Libp2pTransport::start()
{
  if( _running ) return;
  std::shared_ptr<std::promise<void>> runner_ready;
  try
  {
  auto active = std::make_shared< std::atomic< bool > >( true );
  _rpc_read_active = std::move( active );
  _running = true;
  _rpc_reads.clear();
  _io->restart();
  _work_guard.emplace( _io->get_executor() );
  prepare_libp2p_logging();

  // Create libp2p host via a runtime-owned injector. QUIC is disabled in the
  // Koinos-compatible cpp-libp2p build, so bind the transport stack to TCP only.
  _runtime = std::make_unique< Libp2pRuntime >( _io, _config.protocol_version, _sdk_address_budget );
  auto& injector = _runtime->injector;

  _host = injector.create< std::shared_ptr< libp2p::Host > >();

  auto& addresses=dynamic_cast<detail::SdkAddressRepository&>(_host->getPeerRepository().getAddressRepository());
  std::vector<libp2p::peer::PeerId> roots;
  for(const auto* configured:{&_config.seed_peers,&_config.discovery_peers})
    for(const auto& address:*configured)
      if(auto peer=peer_info_from_multiaddress(address))roots.push_back(peer->id);
  addresses.protect([roots=std::move(roots),self=_host->getId(),weak=std::weak_ptr(_host)](const auto& id){
    if(id==self||std::find(roots.begin(),roots.end(),id)!=roots.end())return true;
    if(auto host=weak.lock())return host->getNetwork().getConnectionManager().getBestConnectionForPeer(id)!=nullptr;
    return false;
  });
  _runtime->metadata_timer=std::make_shared<boost::asio::steady_timer>(*_io);
  detail::arm_sdk_metadata_maintenance(_runtime->metadata_timer,std::weak_ptr(_host),_rpc_read_active,
    _sdk_address_budget,std::chrono::milliseconds(_config.sdk_metadata_collection_interval_ms));

  // Observe SDK advice on its serialized mutation path. Projection readers never
  // inspect the SDK's unsynchronized repositories or mutable host pointer.
  _runtime->address_advice = _host->getPeerRepository().getAddressRepository().onAddressAdded(
    [this, active = _rpc_read_active, self = _host->getId()](const auto& id, const auto& address) noexcept {
      if(!active->load(std::memory_order_acquire)||id==self)return;
      try
      {
        const auto& raw=address.getStringAddress();const auto limit=_config.known_peer_advice_capacity_bytes;
        // Conservative base58/conversion workspace check before making strings.
        if(limit<10||raw.size()>limit-10||id.toVector().size()>(limit-10-raw.size())/4)
        {std::lock_guard lock(_peers_mutex);_known_peers.note_capacity_refusal();return;}
        auto peer_id=id.toBase58();auto dial_address=with_peer_id_component(std::string(raw),peer_id);
        PeerID peer{std::move(peer_id),std::move(dial_address)};std::lock_guard lock(_peers_mutex);
        if(!active->load(std::memory_order_acquire))return;
        _known_peers.note_sdk_observation();
        auto current=_connected.find(peer.id);
        if(current!=_connected.end()&&current->second.address.empty())
        {std::string ready(peer.address);current->second.address.swap(ready);}
        remember_known_peer_locked(peer);
      }
      catch(const std::bad_alloc&){std::lock_guard lock(_peers_mutex);_known_peers.note_allocation_refusal();}
      catch(...){/* Optional observation must never unwind an SDK mutation. */}
    });

  LOG( info ) << "[p2p/transport] Host peer ID: " << _host->getId().toBase58();
  LOG( info ) << "[p2p/transport] Advertised protocol version: " << _config.protocol_version;

  // Parse listen address
  if( _config.listen_address.empty() )
  {
    LOG( info ) << "[p2p/transport] Inbound listener disabled";
  }
  else
  {
    auto listen_ma = libp2p::multi::Multiaddress::create( _config.listen_address );
    if( listen_ma.has_value() )
    {
      auto listen_result = _host->listen( listen_ma.value() );
      if( listen_result.has_value() )
      {
        LOG( info ) << "[p2p/transport] Listening on " << _config.listen_address;
      }
      else
      {
        LOG( warning ) << "[p2p/transport] Failed to listen on " << _config.listen_address
                       << " error=" << listen_result.error().message();
      }
    }
    else
    {
      LOG( warning ) << "[p2p/transport] Invalid listen address: " << _config.listen_address;
    }
  }

  // Register peer RPC protocol handler (server side)
  _host->setProtocolHandler(
    { PEER_RPC_PROTOCOL },
    [this, active = _rpc_read_active]( libp2p::StreamAndProtocol stream_and_proto ) {
      if( !active->load( std::memory_order_acquire ) ) return;
      try
      {
        handle_incoming_rpc( std::move( stream_and_proto.stream ) );
      }
      catch( const std::exception& e )
      {
        LOG( warning ) << "[p2p/transport] Incoming Peer RPC handler failed: " << e.what();
      }
      catch( ... )
      {
        LOG( warning ) << "[p2p/transport] Incoming Peer RPC handler failed: unknown exception";
      }
    }
  );

  // Set up connection handlers via Host. The returned event handles must stay
  // alive for the subscriptions to remain active.
  _new_connection_subscription = _host->setOnNewConnectionHandler(
    [this, active = _rpc_read_active]( libp2p::peer::PeerInfo&& peer_info ) {
      if( !active->load(std::memory_order_acquire) ) return;
      try
      {
        auto id_str = peer_info.id.toBase58();
        std::string address;
        if( !peer_info.addresses.empty() )
          address = with_peer_id_component( std::string( peer_info.addresses.front().getStringAddress() ), id_str );
        PeerID peer{ id_str, address };
        bool inserted = false;
        {
          std::lock_guard lock( _peers_mutex );
          inserted = publish_connected_peer_locked(peer);
        }
        if( inserted && _on_connected )
          _on_connected( peer );
      }
      catch( const std::exception& e )
      {
        LOG( warning ) << "[p2p/transport] New connection callback failed: " << e.what();
      }
      catch( ... )
      {
        LOG( warning ) << "[p2p/transport] New connection callback failed: unknown exception";
      }
    }
  );

  _peer_disconnected_subscription = _host->getBus()
    .getChannel< libp2p::event::network::OnPeerDisconnectedChannel >()
    .subscribe( [this, active = _rpc_read_active]( const libp2p::peer::PeerId& peer_id ) {
      if( !active->load(std::memory_order_acquire) ) return;
      try
      {
        auto id_str = peer_id.toBase58();
        PeerID peer{ id_str, "" };
        bool erased = false;
        {
          std::lock_guard lock( _peers_mutex );
          auto it = _connected.find( id_str );
          if( it != _connected.end() )
          {
            peer = it->second;
            _connected.erase( it );
            _known_peers.disconnected(id_str);
            erased = true;
          }

        }

        if( auto client = std::atomic_load(&_rpc_client_context) ) client->dials->cancel_peer(id_str);
        if( erased && _on_disconnected )
          _on_disconnected( peer );
      }
      catch( const std::exception& e )
      {
        LOG( warning ) << "[p2p/transport] Peer disconnected callback failed: " << e.what();
      }
      catch( ... )
      {
        LOG( warning ) << "[p2p/transport] Peer disconnected callback failed: unknown exception";
      }
    } );

  // Create GossipSub
  auto gossip_config = libp2p::protocol::gossip::Config{};
  gossip_config.sign_messages = true;

  _gossip = libp2p::protocol::gossip::create(
    injector.create< std::shared_ptr< libp2p::basic::Scheduler > >(),
    _host,
    injector.create< std::shared_ptr< libp2p::peer::IdentityManager > >(),
    injector.create< std::shared_ptr< libp2p::crypto::CryptoProvider > >(),
    injector.create< std::shared_ptr< libp2p::crypto::marshaller::KeyMarshaller > >(),
    gossip_config );

  for( const auto& seed: _config.seed_peers )
  {
    auto result = _gossip->addBootstrapPeer( seed );
    if( !result.has_value() )
      LOG( debug ) << "[p2p/transport] Failed to add GossipSub bootstrap peer: "
                   << seed << " error=" << result.error().message();
  }

  // Subscribe to block topic
  _block_sub = _gossip->subscribe(
    { BLOCK_TOPIC },
    [this]( libp2p::protocol::gossip::Gossip::SubscriptionData msg ) {
      try
      {
        if( msg )
        {
          auto& m = msg.value();
          auto peer_id = libp2p::peer::PeerId::fromBytes( m.from );
          if( peer_id.has_value() )
            on_gossip_message( std::string( m.topic ),
                               std::string( m.data.begin(), m.data.end() ),
                               peer_id.value() );
        }
      }
      catch( const std::exception& e )
      {
        LOG( warning ) << "[p2p/transport] Block gossip callback failed: " << e.what();
      }
      catch( ... )
      {
        LOG( warning ) << "[p2p/transport] Block gossip callback failed: unknown exception";
      }
    }
  );

  // Subscribe to transaction topic
  _tx_sub = _gossip->subscribe(
    { TRANSACTION_TOPIC },
    [this]( libp2p::protocol::gossip::Gossip::SubscriptionData msg ) {
      try
      {
        if( msg )
        {
          auto& m = msg.value();
          auto peer_id = libp2p::peer::PeerId::fromBytes( m.from );
          if( peer_id.has_value() )
            on_gossip_message( std::string( m.topic ),
                               std::string( m.data.begin(), m.data.end() ),
                               peer_id.value() );
        }
      }
      catch( const std::exception& e )
      {
        LOG( warning ) << "[p2p/transport] Transaction gossip callback failed: " << e.what();
      }
      catch( ... )
      {
        LOG( warning ) << "[p2p/transport] Transaction gossip callback failed: unknown exception";
      }
    }
  );

	  _gossip->start();

	  // Start host
	  _host->start();

	  if( _config.enable_dht )
	  {
	    try
	    {
	      auto scheduler = injector.create< std::shared_ptr< libp2p::basic::Scheduler > >();
	      auto identity_manager = injector.create< std::shared_ptr< libp2p::peer::IdentityManager > >();
	      auto bus = injector.create< std::shared_ptr< libp2p::event::Bus > >();
	      auto random_generator =
	        injector.create< std::shared_ptr< libp2p::crypto::random::RandomGenerator > >();

	      auto storage_backend =
	        std::make_shared< libp2p::protocol::kademlia::StorageBackendDefault >();
	      auto storage = std::make_shared< libp2p::protocol::kademlia::StorageImpl >(
	        _runtime->kademlia_config,
	        storage_backend,
	        scheduler );
	      auto peer_table = std::make_shared< libp2p::protocol::kademlia::PeerRoutingTableImpl >(
	        _runtime->kademlia_config,
	        identity_manager,
	        bus );
	      auto content_table =
	        std::make_shared< libp2p::protocol::kademlia::ContentRoutingTableImpl >(
	          _runtime->kademlia_config,
	          *scheduler,
	          bus );
	      auto validator =
	        std::make_shared< libp2p::protocol::kademlia::ValidatorDefault >();

	      _kademlia = std::make_shared< libp2p::protocol::kademlia::KademliaImpl >(
	        _runtime->kademlia_config,
	        _host,
	        storage,
	        content_table,
	        peer_table,
	        validator,
	        scheduler,
	        bus,
	        random_generator );

	      _kademlia->start();

	      for( const auto& peer_address: _config.discovery_peers )
	      {
	        auto peer_info = peer_info_from_multiaddress( peer_address );
	        if( !peer_info.has_value() )
	          continue;

	        _kademlia->addPeer( peer_info.value(), true, false );
	        auto id = peer_info->id.toBase58();
	        std::lock_guard lock( _peers_mutex );
	        remember_known_peer_locked(PeerID{ id, peer_address },true);
	      }

	      auto bootstrap_result = _kademlia->bootstrap();
	      if( bootstrap_result.has_value() )
	      {
	        LOG( info ) << "[p2p/transport] Kademlia DHT started with "
	                    << _config.discovery_peers.size() << " bootstrap peer(s)";
	      }
	      else
	      {
	        LOG( warning ) << "[p2p/transport] Kademlia bootstrap failed: "
	                       << bootstrap_result.error().message();
	      }
	    }
	    catch( const std::exception& e )
	    {
	      LOG( warning ) << "[p2p/transport] Kademlia DHT startup failed: " << e.what();
	      _kademlia.reset();
	    }
	  }

  // cpp-libp2p connection and muxer state is not safe to drive from multiple
  // io_context runners; keep all libp2p callbacks serialized on one thread.
  unsigned int thread_count = 1;
  if( _config.requested_io_threads != thread_count )
  {
    LOG( info ) << "[p2p/transport] Requested " << _config.requested_io_threads
                << " IO threads; using serialized cpp-libp2p runner count "
                << thread_count;
  }
  runner_ready = std::make_shared<std::promise<void>>();
  auto readiness = runner_ready->get_future().share();
  for( unsigned int i = 0; i < thread_count; ++i )
    _io_threads.emplace_back( [this, readiness]() { readiness.wait();_io->run(); } );

  auto client = std::make_shared< RpcClientContext >();
  client->io = _io;client->host = _host;client->active = _rpc_read_active;
  client->calls = std::make_shared< detail::RpcClientRegistry >( *_io, _rpc_read_active );
  client->dials = std::make_shared< detail::DialRegistry >(_dial_budget, _rpc_read_active);
  std::atomic_store( &_rpc_client_context, std::move( client ) );
  // Seed and later reconnect work share the same native admission/ownership.
  for( const auto& seed: _config.seed_peers )
  {
    try
    {
      // Use the pinned SDK's syntax/identity rules, including legacy /ipfs/.
      // This trusted-config identity prepass is outside the dial lease ledger;
      // native admission still precedes DNS and explicit dial preparation.
      auto address = libp2p::multi::Multiaddress::create(seed);
      if( !address.has_value() ) throw std::runtime_error("Invalid seed multiaddress: " + seed);
      auto id = address.value().getPeerId();
      if( id )
      {
        PeerID target{*id,seed};
        {std::lock_guard lock(_peers_mutex);remember_known_peer_locked(target,true);}
        connect_peer(target);
      }
    }
    catch( const std::bad_alloc& ) {} // Optional startup advice; ordinary reconnect can try later.
    catch( const LocalTransportUnavailable& ) {}
    catch( const std::exception& e )
    { LOG(warning) << "[p2p/transport] Failed to prepare seed connection: " << e.what(); }
  }
  // Seed publication/context initialization precedes SDK callbacks, matching
  // the historical startup order; implicit GossipSub bootstrap has a separate budget gap.
  runner_ready->set_value();
  LOG( info ) << "[p2p/transport] Started with " << thread_count << " IO threads";
  }
  catch( ... )
  {
    auto failure = std::current_exception();
    // A partially initialized runner must wake before stop posts its drain barrier.
    try { if(runner_ready)runner_ready->set_value(); } catch( ... ) {}
    try { stop(); } catch( ... ) {}
    std::rethrow_exception(failure);
  }
}

void Libp2pTransport::stop()
{
  if( _io->get_executor().running_in_this_thread() )
    throw std::logic_error( "Libp2pTransport::stop requires the external lifecycle owner" );
  if( !_running.exchange( false ) ) return;
  auto client = std::atomic_exchange( &_rpc_client_context, std::shared_ptr< RpcClientContext >() );
  if( client ) { client->dials->stop_accepting();client->calls->stop_accepting(); }
  _rpc_read_active->store( false, std::memory_order_release );

  auto cleanup = [this, client]() noexcept {
    auto attempt = []( const char* label, auto&& action ) noexcept {
      try { action(); }
      catch( ... )
      {
        try { LOG( warning ) << "[p2p/transport] Shutdown cleanup failed: " << label; }
        catch( ... ) {}
      }
    };
    if(_runtime&&_runtime->metadata_timer)
      attempt("metadata timer",[this]{boost::system::error_code error;_runtime->metadata_timer->cancel(error);});
    if(_host)
      attempt("address retirement",[this]{dynamic_cast<detail::SdkAddressRepository&>(_host->getPeerRepository().getAddressRepository()).retire();});
    if( client ) client->calls->cancel_all();
    for( const auto& weak: _rpc_reads )
      if( auto read = weak.lock() )
        read->cancel( detail::RpcRequestReadError::Reason::cancelled, true );
    std::vector< std::weak_ptr< detail::RpcRequestRead > >().swap( _rpc_reads );
    _block_sub.reset();
    _tx_sub.reset();
    if( _gossip )
    {
      attempt( "gossip", [this] { _gossip->stop(); } );
      _gossip.reset();
    }
    _kademlia.reset();
    attempt( "connection subscription", [this] { _new_connection_subscription.unsubscribe(); } );
    attempt( "disconnection subscription", [this] { _peer_disconnected_subscription.unsubscribe(); } );
    if( _runtime ) _runtime->address_advice.disconnect();
    if( _host )
    {
      attempt( "host", [this] { _host->stop(); } );
      // The pinned SDK's Host::stop stops listeners only. Established Yamux
      // connections otherwise retain pending reads, their scheduler and I/O
      // context across stop/restart. Close them on their serialized runner.
      attempt( "connection inventory", [this, &attempt] {
        auto& manager = _host->getNetwork().getConnectionManager();
        for( const auto& connection: manager.getConnections() )
          if( connection && !connection->isClosed() )
            attempt( "connection", [&] {
              auto peer = connection->remotePeer();
              if( peer.has_value() ) manager.closeConnectionsToPeer( peer.value() );
              else (void)connection->close();
            } );
      } );
      _host.reset();
    }
  };

  bool cleaned = false;
  // Cancel RPC reads and stop SDK services on their serialized runner. The
  // acknowledgement follows immediately queued cancellation completions.
  if( !_io_threads.empty() && !_io->stopped() )
  {
    try
    {
      auto done = std::make_shared< std::promise< void > >();
      auto drained = done->get_future();
      boost::asio::post( *_io, [this, cleanup, done] {
        cleanup();
        try { boost::asio::post( *_io, [done] { done->set_value(); } ); }
        catch( ... ) { done->set_value(); }
      } );
      drained.get();
      cleaned = true;
    }
    catch( ... ) {} // Join the runner before allocation-free fallback cleanup.
  }

  _work_guard.reset();
  _io->stop();
  for( auto& thread: _io_threads )
    if( thread.joinable() ) thread.join();
  _io_threads.clear();
  if( !cleaned ) cleanup();
  {
    std::lock_guard lock( _peers_mutex );
    _connected.clear();
    _known_peers.disconnected_all();
  }
  _runtime.reset();
}

// ---------------------------------------------------------------------------
// Peer management
// ---------------------------------------------------------------------------

void Libp2pTransport::remember_known_peer_locked( const PeerID& peer, bool seed ) noexcept
{
  if(peer.address.empty())return;
  _known_peers.protect(peer.id,seed,_connected.count(peer.id)!=0);
  if(!_known_peers.remember(peer,seed,_connected.count(peer.id)!=0))
    _dial_budget->note(detail::DialBudget::Event::retention_bypass);
}

bool Libp2pTransport::publish_connected_peer_locked( const PeerID& peer )
{
  auto it = _connected.find(peer.id);
  const bool inserted = it == _connected.end();
  if( inserted ) _connected.emplace(peer.id, peer);
  else if( it->second.address.empty() && !peer.address.empty() )
  {
    PeerID ready(peer);it->second.id.swap(ready.id);it->second.address.swap(ready.address);
  }
  remember_known_peer_locked(peer);
  return inserted;
}

void Libp2pTransport::connect_peer( const PeerID& peer )
{
  auto client = std::atomic_load(&_rpc_client_context);
  if( !client || !client->active->load(std::memory_order_acquire) ) throw LocalTransportUnavailable();
  std::shared_ptr<detail::DialBudget::Lease> lease;
  {
    std::lock_guard lock(_peers_mutex);
    if( _connected.count(peer.id) ) return;
    lease = client->dials->admit(peer);
  }
  if( !lease ) return; // This generation already owns this peer's pending attempt.
  try
  {
    // Admission precedes owned copies, DNS and queue publication. DNS and SDK
    // representations remain separate unbounded byte axes, not this ledger.
    auto addresses = dial_multiaddresses_from(lease->peer.address);
    if( addresses.addresses.empty() ) throw std::runtime_error("Dial multiaddress has no addresses");
    auto id = addresses.addresses.front().getPeerId();
    if( !id ) throw std::runtime_error("Multiaddr has no peer ID: " + lease->peer.address);
    auto parsed = libp2p::peer::PeerId::fromBase58(*id);
    if( !parsed.has_value() ) throw std::runtime_error("Invalid peer ID in multiaddr: " + lease->peer.address);
    if( *id != lease->peer.id ) throw std::invalid_argument("Peer identity does not match dial multiaddress");
    libp2p::peer::PeerInfo info{parsed.value(), std::move(addresses.addresses)};
    auto work = [this, client, lease, info = std::move(info)]() mutable {
      if( !client->dials->eligible(lease) ) { client->dials->retire(lease);return; }
      try
      {
        { std::lock_guard lock(_peers_mutex);remember_known_peer_locked(lease->peer); }
        // The SDK stores this callback. A weak context avoids a host -> callback
        // -> context -> host cycle; the lease alone retains its budget credits.
        client->host->connect(info, [this, weak = std::weak_ptr<RpcClientContext>(client), lease](auto&& result) {
          auto current = weak.lock();
          if( !current || !current->active->load(std::memory_order_acquire) )
          {
            if( result.has_value() && result.value() )
              try { (void)result.value()->close(); } catch( ... ) {}
            return; // No access to the transport owner after generation retirement.
          }
          if( !current->dials->eligible(lease) ) return;
          struct Retire
          {
            std::shared_ptr<detail::DialRegistry> registry;
            std::shared_ptr<detail::DialBudget::Lease> lease;
            ~Retire() { registry->retire(lease); }
          } done{current->dials, lease};
          try
          {
            if( result.has_value() && result.value() && !result.value()->isClosed() )
            {
              bool inserted;
              { std::lock_guard lock(_peers_mutex);inserted = publish_connected_peer_locked(lease->peer); }
              if( inserted && _on_connected ) _on_connected(lease->peer);
            }
            else if( !result.has_value() )
              LOG(warning) << "[p2p/transport] Failed to connect to peer: " << lease->peer.address
                           << " error=" << result.error().message();
          }
          catch( const std::bad_alloc& )
          { _dial_budget->note(detail::DialBudget::Event::completion_exception); }
          catch( ... )
          { _dial_budget->note(detail::DialBudget::Event::completion_exception); }
          // An external notification may already have effects; never invoke it twice.
        });
      }
      catch( ... )
      { _dial_budget->note(detail::DialBudget::Event::setup_exception);client->dials->retire(lease); }
    };
    // Stop closes admission under this same mutex before posting its drain barrier.
    client->dials->publish(lease, [&] { boost::asio::post(*client->io, std::move(work)); });
  }
  catch( ... )
  { _dial_budget->note(detail::DialBudget::Event::setup_exception);client->dials->retire(lease);throw; }
}

void Libp2pTransport::disconnect_peer( const PeerID& peer )
{
  auto client = std::atomic_load(&_rpc_client_context);
  if( !client ) return;
  client->dials->cancel_peer(peer.id);
  auto pid = libp2p::peer::PeerId::fromBase58( peer.id );
  if( pid.has_value() && client && client->active->load(std::memory_order_acquire) )
  {
    try
    {
    client->dials->publish_control([&] {
    boost::asio::post( *client->io, [client, peer, pid = pid.value()] {
      if( !client->active->load(std::memory_order_acquire) ) return;
      auto& host = client->host;
      try
      {
        host->disconnect( pid );
      }
      catch( const std::exception& e )
      {
        LOG( debug ) << "[p2p/transport] Disconnect failed for peer "
                     << peer.id << ": " << e.what();
      }
      catch( ... )
      {
        LOG( debug ) << "[p2p/transport] Disconnect failed for peer "
                     << peer.id << ": unknown exception";
      }
    } );
    });
    }
    catch( const LocalTransportUnavailable& ) { return; }
  }

  PeerID disconnected_peer;
  bool was_connected       = false;
  {
    std::lock_guard lock( _peers_mutex );
    if( !client->active->load(std::memory_order_acquire)
        || std::atomic_load(&_rpc_client_context).get() != client.get() ) return;
    auto it = _connected.find( peer.id );
    if( it != _connected.end() )
    {
      disconnected_peer = std::move(it->second);
      _connected.erase( it );
      _known_peers.disconnected(peer.id);
      was_connected = true;
    }
  }

  if( was_connected && _on_disconnected )
  {
    try
    {
      _on_disconnected( disconnected_peer );
    }
    catch( const std::exception& e )
    {
      LOG( warning ) << "[p2p/transport] Peer disconnected callback failed: " << e.what();
    }
    catch( ... )
    {
      LOG( warning ) << "[p2p/transport] Peer disconnected callback failed: unknown exception";
    }
  }
}

uint32_t Libp2pTransport::connected_peer_count() const
{
  std::lock_guard lock(_peers_mutex);
  return static_cast<uint32_t>(std::min<size_t>(_connected.size(),std::numeric_limits<uint32_t>::max()));
}

PeerSnapshot Libp2pTransport::connected_peers() const
{
  std::lock_guard lock(_peers_mutex);size_t bytes=0;
  for(const auto& [id,peer]:_connected)bytes=detail::PeerSnapshotBudget::add(bytes,detail::PeerSnapshotBudget::strings(peer));
  return _peer_snapshot_budget->make(_connected.size(),bytes,[&](auto& result){
    for(const auto& [id,peer]:_connected)result.push_back(peer);
  });
}

PeerSnapshot Libp2pTransport::known_peers() const
{
  std::lock_guard lock(_peers_mutex);
  // Merge two already-owned sorted sources directly into the result. No
  // intermediate map, SDK peer/address vectors or external host access.
  auto project=[&](auto&& emit){
    auto connected=_connected.begin();
    _known_peers.visit([&](const PeerID& advice){
      while(connected!=_connected.end()&&connected->first<advice.id){emit(connected->second);++connected;}
      if(connected!=_connected.end()&&connected->first==advice.id)
      {emit(connected->second.address.empty()?advice:connected->second);++connected;}
      else emit(advice);
    });
    while(connected!=_connected.end()){emit(connected->second);++connected;}
  };
  size_t entries=0,bytes=0;project([&](const PeerID& peer){entries=detail::PeerSnapshotBudget::add(entries,1);bytes=detail::PeerSnapshotBudget::add(bytes,detail::PeerSnapshotBudget::strings(peer));});
  return _peer_snapshot_budget->make(entries,bytes,[&](auto& result){project([&](const PeerID& peer){result.push_back(peer);});});
}

// ---------------------------------------------------------------------------
// GossipSub publish
// ---------------------------------------------------------------------------

void Libp2pTransport::publish_block( const protocol::block& block )
{
  if( !_gossip || !_running )
    return;

  std::string data;
  if( block.SerializeToString( &data ) )
  {
    try
    {
      _gossip->publish( BLOCK_TOPIC, libp2p::Bytes( data.begin(), data.end() ) );
    }
    catch( const std::exception& e )
    {
      LOG( warning ) << "[p2p/transport] Failed to publish block gossip: " << e.what();
    }
    catch( ... )
    {
      LOG( warning ) << "[p2p/transport] Failed to publish block gossip: unknown exception";
    }
  }
}

void Libp2pTransport::publish_transaction( const protocol::transaction& tx )
{
  if( !_gossip || !_running )
    return;

  std::string data;
  if( tx.SerializeToString( &data ) )
  {
    try
    {
      _gossip->publish( TRANSACTION_TOPIC, libp2p::Bytes( data.begin(), data.end() ) );
    }
    catch( const std::exception& e )
    {
      LOG( warning ) << "[p2p/transport] Failed to publish transaction gossip: " << e.what();
    }
    catch( ... )
    {
      LOG( warning ) << "[p2p/transport] Failed to publish transaction gossip: unknown exception";
    }
  }
}

void Libp2pTransport::on_gossip_message( const std::string& topic,
                                          const std::string& data,
                                          const libp2p::peer::PeerId& from )
{
  PeerID pid = to_peer_id( from );

  if( topic == BLOCK_TOPIC && _on_block )
  {
    ::koinos::protocol::block block;
    if( block.ParseFromString( data ) )
      _on_block( pid, block );
  }
  else if( topic == TRANSACTION_TOPIC && _on_tx )
  {
    ::koinos::protocol::transaction tx;
    if( tx.ParseFromString( data ) )
      _on_tx( pid, tx );
  }
}

// ---------------------------------------------------------------------------
// GossipSub setup (now inlined into start())
// ---------------------------------------------------------------------------

void Libp2pTransport::setup_gossipsub()
{
  // Intentionally empty — gossip is set up in start() where injector is available
}

// ---------------------------------------------------------------------------
// Peer RPC (gorpc-compatible framing over libp2p streams)
// ---------------------------------------------------------------------------

std::string Libp2pTransport::send_peer_rpc( const PeerID& peer,
                                              const std::string& service,
                                              const std::string& method,
                                              const std::string& msgpack_args )
{
  auto client = std::atomic_load( &_rpc_client_context );
  if( !client || !client->active->load( std::memory_order_acquire )
    || client->io->get_executor().running_in_this_thread() )
    throw LocalTransportUnavailable();
  auto pid = libp2p::peer::PeerId::fromBase58( peer.id );
  if( !pid.has_value() ) throw std::runtime_error( "Invalid peer ID: " + peer.id );
  auto addresses = dial_multiaddresses_from( peer.address );
  libp2p::peer::PeerInfo target{ pid.value(), std::move( addresses.addresses ) };
  auto request = gorpc::encode_request( service, method, msgpack_args );
  auto bytes = std::make_shared< libp2p::Bytes >( request.begin(), request.end() );
  auto promise = std::make_shared< std::promise< gorpc::Response > >();
  auto future = promise->get_future();
  detail::RpcClientOpener opener = [host = client->host, target = std::move( target )]
    ( detail::RpcClientOpenResult done ) {
      host->newStream( target, { PEER_RPC_PROTOCOL },
        [done = std::move( done )]( libp2p::StreamAndProtocolOrError result ) {
          std::shared_ptr< libp2p::connection::Stream > stream;
          try
          {
            if( !result.has_value() )
            {
              if( result.error() == std::errc::not_enough_memory ) throw std::bad_alloc();
              throw std::runtime_error( "Failed to open stream: " + result.error().message() );
            }
            stream = std::move( result.value().stream );
            detail::RpcClientChannel channel{ stream, stream,
              [stream] { safe_stream_close( stream, "complete outgoing Peer RPC" ); },
              [stream] { stream->reset(); } };
            done( {}, std::move( channel ) );
          }
          catch( ... )
          {
            if( stream ) try { stream->reset(); } catch( ... ) {}
            done( std::current_exception(), {} );
          }
        } );
    };
  auto call = std::make_shared< detail::RpcClientCall >( *client->io, std::move( opener ), bytes,
    [promise]( std::exception_ptr error, gorpc::Response response ) {
      if( error ) promise->set_exception( error );
      else promise->set_value( std::move( response ) );
    }, client->active );
  client->calls->submit( call );
  // Timers cannot run while an unrelated synchronous SDK handler monopolizes
  // the I/O runner. Bound the external wait too, without touching I/O-owned
  // fields or blaming a peer for local scheduling failure. Never replay work.
  if( future.wait_for( std::chrono::seconds( 19 ) ) != std::future_status::ready )
  {
    client->calls->request_cancel( call );
    throw LocalTransportUnavailable();
  }
  auto response = future.get();
  if( !response.header.error.empty() ) throw std::runtime_error( "Peer RPC error: " + response.header.error );
  return std::move( response.payload );
}

void Libp2pTransport::handle_incoming_rpc(
  std::shared_ptr< libp2p::connection::Stream > stream )
{
  try
  {
    auto active = _rpc_read_active;
    auto read = std::make_shared< detail::RpcRequestRead >( *_io, stream,
      [weak_io = std::weak_ptr( _io ), stream, active, handler = _on_peer_rpc_request]
      ( std::exception_ptr read_error, gorpc::DecodedRequest request ) {
        if( !active->load( std::memory_order_acquire ) ) return;
        auto io = weak_io.lock();
        if( !io ) return;
        try
        {
          try
          {
            if( read_error )
              std::rethrow_exception( read_error );
          }
          catch( const detail::RpcRequestReadError& e )
          {
            if( e.reason != detail::RpcRequestReadError::Reason::capacity )
            {
              log_libp2p_async_error( "read incoming Peer RPC request", e );
              safe_stream_close( stream, "read incoming Peer RPC request" );
              return;
            }
            gorpc::ServiceID unknown{ "unknown", "unknown" };
            auto error = gorpc::encode_error_response( unknown, e.what(), gorpc::ErrorType::server );
            auto bytes = std::make_shared< libp2p::Bytes >( error.begin(), error.end() );
            safe_stream_write_and_close( *io, stream, bytes, "write Peer RPC capacity error response" );
            return;
          }
          catch( const gorpc::DecodeError& e )
          {
            gorpc::ServiceID unknown{ "unknown", "unknown" };
            auto error = gorpc::encode_error_response( unknown, e.what(), gorpc::ErrorType::server );
            auto error_bytes = std::make_shared< libp2p::Bytes >( error.begin(), error.end() );
            safe_stream_write_and_close( *io, stream, error_bytes, "write Peer RPC decode error response" );
            return;
          }

          LOG( debug ) << "[p2p/transport] Incoming RPC: "
                       << request.service.name << "." << request.service.method;

          std::string response;
          try
          {
            if( !handler )
              throw std::runtime_error( "peer RPC handler is not configured" );

            auto payload = handler( request.service.name, request.service.method, request.args );
            response = gorpc::encode_success_response( request.service, payload );
          }
          catch( const std::exception& e )
          {
            response = gorpc::encode_error_response( request.service, e.what(), gorpc::ErrorType::server );
          }
          catch( ... )
          {
            response = gorpc::encode_error_response(
              request.service, "unknown peer RPC handler exception", gorpc::ErrorType::server );
          }

          auto resp_bytes = std::make_shared< libp2p::Bytes >( response.begin(), response.end() );

          safe_stream_write_and_close( *io, stream, resp_bytes, "write Peer RPC response" );
        }
        catch( const std::exception& e )
        {
          LOG( warning ) << "[p2p/transport] Incoming Peer RPC callback failed: " << e.what();
          safe_stream_close( stream, "incoming Peer RPC callback failure" );
        }
        catch( ... )
        {
          LOG( warning ) << "[p2p/transport] Incoming Peer RPC callback failed: unknown exception";
          safe_stream_close( stream, "incoming Peer RPC callback failure" );
        }
      },
      [stream] { stream->reset(); }, std::chrono::seconds( 6 ), active
    );
    _rpc_reads.erase( std::remove_if( _rpc_reads.begin(), _rpc_reads.end(),
      []( const auto& weak ) { return weak.expired(); } ), _rpc_reads.end() );
    _rpc_reads.emplace_back( read ); // Admit tracking before I/O/handler work.
    read->begin();
  }
  catch( const std::exception& e )
  {
    LOG( warning ) << "[p2p/transport] Failed to start incoming Peer RPC read: " << e.what();
    safe_stream_close( stream, "start incoming Peer RPC read" );
  }
  catch( ... )
  {
    LOG( warning ) << "[p2p/transport] Failed to start incoming Peer RPC read: unknown exception";
    safe_stream_close( stream, "start incoming Peer RPC read" );
  }
}

// ---------------------------------------------------------------------------
// Peer RPC methods
// ---------------------------------------------------------------------------

std::string Libp2pTransport::peer_get_chain_id( const PeerID& peer )
{
  auto payload = send_peer_rpc( peer, "PeerRPCService", "GetChainID", gorpc::encode_empty_request() );
  return gorpc::decode_id_response( payload );
}

PeerHeadInfo Libp2pTransport::peer_get_head_block( const PeerID& peer )
{
  auto payload = send_peer_rpc( peer, "PeerRPCService", "GetHeadBlock", gorpc::encode_empty_request() );
  auto decoded = gorpc::decode_head_block_response( payload );

  PeerHeadInfo info;
  info.block_id = std::move( decoded.id );
  info.height = decoded.height;
  return info;
}

std::string Libp2pTransport::peer_get_ancestor_block_id( const PeerID& peer,
                                                          const std::string& head_id,
                                                          uint64_t height )
{
  auto request = gorpc::encode_get_ancestor_block_id_request( head_id, height );
  auto payload = send_peer_rpc( peer, "PeerRPCService", "GetAncestorBlockID", request );
  return gorpc::decode_id_response( payload );
}

std::vector< ::koinos::protocol::block >
Libp2pTransport::peer_get_blocks( const PeerID& peer,
                                   const std::string& head_id,
                                   uint64_t start_height,
                                   uint32_t count )
{
  auto request = gorpc::encode_get_blocks_request( head_id, start_height, count );
  auto payload = send_peer_rpc( peer, "PeerRPCService", "GetBlocks", request );
  auto block_payloads = gorpc::decode_blocks_response( payload );
  if( block_payloads.size() != count )
    throw std::runtime_error( "Peer returned unexpected number of blocks" );

  std::vector< ::koinos::protocol::block > blocks;
  blocks.reserve( block_payloads.size() );
  for( const auto& block_payload: block_payloads )
  {
    ::koinos::protocol::block block;
    if( !block.ParseFromString( block_payload ) )
      throw std::runtime_error( "Failed to deserialize peer RPC block response" );
    blocks.push_back( std::move( block ) );
  }
  return blocks;
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void Libp2pTransport::on_peer_connected( PeerConnectedCallback cb ) { _on_connected = std::move( cb ); }
void Libp2pTransport::on_peer_disconnected( PeerDisconnectedCallback cb ) { _on_disconnected = std::move( cb ); }
void Libp2pTransport::on_block_received( BlockReceivedCallback cb ) { _on_block = std::move( cb ); }
void Libp2pTransport::on_transaction_received( TxReceivedCallback cb ) { _on_tx = std::move( cb ); }
void Libp2pTransport::on_peer_rpc_request( PeerRpcRequestCallback cb ) { _on_peer_rpc_request = std::move( cb ); }

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

PeerID Libp2pTransport::to_peer_id( const libp2p::peer::PeerId& pid )
{
  return { pid.toBase58(), "" };
}

} // namespace koinos::node::p2p

#endif // KOINOS_HAS_LIBP2P
