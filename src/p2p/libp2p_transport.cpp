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

template< typename T, typename Value >
void safe_set_promise_value( const std::shared_ptr< std::promise< T > >& promise,
                             Value&& value,
                             const std::string& operation )
{
  if( !promise )
    return;

  try
  {
    promise->set_value( std::forward< Value >( value ) );
  }
  catch( const std::exception& e )
  {
    log_libp2p_async_error( operation + " promise completion", e );
  }
  catch( ... )
  {
    log_libp2p_async_unknown_error( operation + " promise completion" );
  }
}

template< typename T >
void safe_set_promise_exception( const std::shared_ptr< std::promise< T > >& promise,
                                 const std::string& message,
                                 const std::string& operation )
{
  if( !promise )
    return;

  try
  {
    promise->set_exception( std::make_exception_ptr( std::runtime_error( message ) ) );
  }
  catch( const std::exception& e )
  {
    log_libp2p_async_error( operation + " promise exception", e );
  }
  catch( ... )
  {
    log_libp2p_async_unknown_error( operation + " promise exception" );
  }
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

void safe_stream_write_some( const std::shared_ptr< libp2p::connection::Stream >& stream,
                             const std::shared_ptr< libp2p::Bytes >& bytes,
                             const std::shared_ptr< std::promise< outcome::result< size_t > > >& promise,
                             const std::string& operation )
{
  if( !stream || !bytes )
  {
    safe_set_promise_exception( promise, operation + ": stream or buffer unavailable", operation );
    return;
  }

  try
  {
    stream->writeSome( *bytes, bytes->size(),
      [bytes, promise, operation]( outcome::result< size_t > result ) mutable {
        safe_set_promise_value( promise, std::move( result ), operation );
      }
    );
  }
  catch( const std::exception& e )
  {
    safe_set_promise_exception( promise, operation + ": " + e.what(), operation );
  }
  catch( ... )
  {
    safe_set_promise_exception( promise, operation + ": unknown exception", operation );
  }
}

void safe_stream_read_some( const std::shared_ptr< libp2p::connection::Stream >& stream,
                            const std::shared_ptr< libp2p::Bytes >& buffer,
                            const std::shared_ptr< std::promise< outcome::result< size_t > > >& promise,
                            const std::string& operation )
{
  if( !stream || !buffer )
  {
    safe_set_promise_exception( promise, operation + ": stream or buffer unavailable", operation );
    return;
  }

  try
  {
    stream->readSome( *buffer, buffer->size(),
      [buffer, promise, operation]( outcome::result< size_t > result ) mutable {
        safe_set_promise_value( promise, std::move( result ), operation );
      }
    );
  }
  catch( const std::exception& e )
  {
    safe_set_promise_exception( promise, operation + ": " + e.what(), operation );
  }
  catch( ... )
  {
    safe_set_promise_exception( promise, operation + ": unknown exception", operation );
  }
}

void safe_stream_write_and_close( const std::shared_ptr< libp2p::connection::Stream >& stream,
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
    stream->writeSome( *bytes, bytes->size(),
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
                           const std::string& protocol_version )
{
  // The injector owns shared-scoped cpp-libp2p dependencies, including TLS and
  // scheduler state used by async callbacks after the host has started.
  return libp2p::injector::makeHostInjector< boost::di::extension::shared_config >(
    boost::di::bind< boost::asio::io_context >.to( io )[ boost::di::override ],
    libp2p::injector::useLibp2pClientVersion( libp2p::Libp2pClientVersion{ protocol_version } ),
    libp2p::injector::useLayerAdaptors<>(),
    libp2p::injector::useMuxerAdaptors< libp2p::muxer::Yamux >(),
    libp2p::injector::useTransportAdaptors< libp2p::transport::TcpTransport >() );
}

} // namespace

namespace koinos::node::p2p {

using Libp2pInjector = decltype( make_libp2p_injector(
  std::declval< const std::shared_ptr< boost::asio::io_context >& >(),
  std::declval< const std::string& >() ) );

struct Libp2pTransport::Libp2pRuntime
{
  Libp2pInjector injector;
  libp2p::protocol::kademlia::Config kademlia_config;

  Libp2pRuntime( const std::shared_ptr< boost::asio::io_context >& io,
                 const std::string& protocol_version )
    : injector( make_libp2p_injector( io, protocol_version ) )
  {}
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Libp2pTransport::Libp2pTransport( const Config& config ) : _config( config )
{
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
  _running = true;
  _io->restart();
  _work_guard.emplace( _io->get_executor() );
  prepare_libp2p_logging();

  // Create libp2p host via a runtime-owned injector. QUIC is disabled in the
  // Koinos-compatible cpp-libp2p build, so bind the transport stack to TCP only.
  _runtime = std::make_unique< Libp2pRuntime >( _io, _config.protocol_version );
  auto& injector = _runtime->injector;

  _host = injector.create< std::shared_ptr< libp2p::Host > >();

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
    [this]( libp2p::StreamAndProtocol stream_and_proto ) {
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
    [this]( libp2p::peer::PeerInfo&& peer_info ) {
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
          auto it = _connected.find( id_str );
          inserted = it == _connected.end();
          if( inserted || it->second.address.empty() )
            _connected[ id_str ] = peer;
          if( !address.empty() )
            _known_peers[ id_str ] = peer;
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
    .subscribe( [this]( const libp2p::peer::PeerId& peer_id ) {
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
            erased = true;
          }
          _connecting.erase( id_str );
        }

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
	        _known_peers[ id ] = PeerID{ id, peer_address };
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

	  // Connect to seed peers
	  for( const auto& seed: _config.seed_peers )
  {
    DialMultiaddresses dial_addresses;
    try
    {
      dial_addresses = dial_multiaddresses_from( seed );
    }
    catch( const std::exception& e )
    {
      LOG( warning ) << "[p2p/transport] Failed to prepare seed address: " << seed
                     << " error=" << e.what();
      continue;
    }

    auto peer_id_str = dial_addresses.addresses.front().getPeerId();
    if( !peer_id_str.has_value() )
      continue;

    auto peer_id = libp2p::peer::PeerId::fromBase58( peer_id_str.value() );
    if( !peer_id.has_value() )
      continue;

    if( dial_addresses.resolved_dns )
      LOG( info ) << "[p2p/transport] Resolved seed " << seed << " to "
                  << dial_addresses.address_strings.front();

    libp2p::peer::PeerInfo peer_info{ peer_id.value(), dial_addresses.addresses };

    try
    {
      _host->connect( peer_info, [this, seed, id = peer_id_str.value()]( auto&& result ) {
        try
        {
          if( result.has_value() )
          {
            PeerID peer{ id, seed };
            {
              std::lock_guard lock( _peers_mutex );
              _connected[ id ] = peer;
              _known_peers[ id ] = peer;
            }
            if( _on_connected )
              _on_connected( peer );
            LOG( info ) << "[p2p/transport] Connected to seed: " << seed;
          }
          else
            LOG( warning ) << "[p2p/transport] Failed to connect to seed: " << seed
                           << " error=" << result.error().message();
        }
        catch( const std::exception& e )
        {
          LOG( warning ) << "[p2p/transport] Seed connect callback failed: " << e.what();
        }
        catch( ... )
        {
          LOG( warning ) << "[p2p/transport] Seed connect callback failed: unknown exception";
        }
      } );
    }
    catch( const std::exception& e )
    {
      LOG( warning ) << "[p2p/transport] Failed to start seed connection: "
                     << seed << " error=" << e.what();
    }
    catch( ... )
    {
      LOG( warning ) << "[p2p/transport] Failed to start seed connection: "
                     << seed << " error=unknown exception";
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
  for( unsigned int i = 0; i < thread_count; ++i )
    _io_threads.emplace_back( [this]() { _io->run(); } );

  LOG( info ) << "[p2p/transport] Started with " << thread_count << " IO threads";
}

void Libp2pTransport::stop()
{
  if( !_running.exchange( false ) )
    return;

  _block_sub.reset();
  _tx_sub.reset();

	  if( _gossip )
	  {
	    _gossip->stop();
	    _gossip.reset();
	  }

	  _kademlia.reset();

  _new_connection_subscription.unsubscribe();
  _peer_disconnected_subscription.unsubscribe();

	  if( _host )
  {
    _host->stop();
    _host.reset();
  }

  _work_guard.reset();
  _io->stop();
  for( auto& t: _io_threads )
    if( t.joinable() )
      t.join();
  _io_threads.clear();
  _runtime.reset();
}

// ---------------------------------------------------------------------------
// Peer management
// ---------------------------------------------------------------------------

void Libp2pTransport::connect_peer( const PeerID& peer )
{
  {
    std::lock_guard lock( _peers_mutex );
    if( _connected.count( peer.id ) || _connecting.count( peer.id ) )
      return;
    _connecting.insert( peer.id );
    if( !peer.address.empty() )
      _known_peers[ peer.id ] = peer;
  }

  DialMultiaddresses dial_addresses;
  try
  {
    dial_addresses = dial_multiaddresses_from( peer.address );
  }
  catch( const std::exception& e )
  {
    std::lock_guard lock( _peers_mutex );
    _connecting.erase( peer.id );
    throw std::runtime_error( e.what() );
  }

  auto peer_id_str = dial_addresses.addresses.front().getPeerId();
  if( !peer_id_str.has_value() )
  {
    std::lock_guard lock( _peers_mutex );
    _connecting.erase( peer.id );
    throw std::runtime_error( "Multiaddr has no peer ID: " + peer.address );
  }

  auto peer_id = libp2p::peer::PeerId::fromBase58( peer_id_str.value() );
  if( !peer_id.has_value() )
  {
    std::lock_guard lock( _peers_mutex );
    _connecting.erase( peer.id );
    throw std::runtime_error( "Invalid peer ID in multiaddr: " + peer.address );
  }

  if( dial_addresses.resolved_dns )
    LOG( info ) << "[p2p/transport] Resolved peer " << peer.address << " to "
                << dial_addresses.address_strings.front();

  libp2p::peer::PeerInfo peer_info{ peer_id.value(), dial_addresses.addresses };
  boost::asio::post( *_io, [this, host = _host, peer, peer_info = std::move( peer_info )]() mutable {
    try
    {
      host->connect( peer_info, [this, peer]( auto&& result ) {
        try
        {
          {
            std::lock_guard lock( _peers_mutex );
            _connecting.erase( peer.id );
          }

          if( result.has_value() )
          {
            {
              std::lock_guard lock( _peers_mutex );
              _connected[ peer.id ] = peer;
              if( !peer.address.empty() )
                _known_peers[ peer.id ] = peer;
            }
            if( _on_connected )
              _on_connected( peer );
          }
          else
            LOG( warning ) << "[p2p/transport] Failed to connect to peer: " << peer.address
                           << " error=" << result.error().message();
        }
        catch( const std::exception& e )
        {
          LOG( warning ) << "[p2p/transport] Peer connect callback failed: " << e.what();
          std::lock_guard lock( _peers_mutex );
          _connecting.erase( peer.id );
        }
        catch( ... )
        {
          LOG( warning ) << "[p2p/transport] Peer connect callback failed: unknown exception";
          std::lock_guard lock( _peers_mutex );
          _connecting.erase( peer.id );
        }
      } );
    }
    catch( const std::exception& e )
    {
      LOG( warning ) << "[p2p/transport] Failed to start peer connection: "
                     << peer.address << " error=" << e.what();
      std::lock_guard lock( _peers_mutex );
      _connecting.erase( peer.id );
    }
    catch( ... )
    {
      LOG( warning ) << "[p2p/transport] Failed to start peer connection: "
                     << peer.address << " error=unknown exception";
      std::lock_guard lock( _peers_mutex );
      _connecting.erase( peer.id );
    }
  } );
}

void Libp2pTransport::disconnect_peer( const PeerID& peer )
{
  auto pid = libp2p::peer::PeerId::fromBase58( peer.id );
  if( pid.has_value() && _host && _io )
  {
    boost::asio::post( *_io, [host = _host, peer, pid = pid.value()] {
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
  }

  PeerID disconnected_peer = peer;
  bool was_connected       = false;
  {
    std::lock_guard lock( _peers_mutex );
    _connecting.erase( peer.id );
    auto it = _connected.find( peer.id );
    if( it != _connected.end() )
    {
      disconnected_peer = it->second;
      _connected.erase( it );
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
  return static_cast< uint32_t >( connected_peers().size() );
}

std::vector< PeerID > Libp2pTransport::connected_peers() const
{
  std::map< std::string, PeerID > peers;
  {
    std::lock_guard lock( _peers_mutex );
    peers = _connected;
  }

  if( _host )
  {
    try
    {
      for( const auto& connection: _host->getNetwork().getConnectionManager().getConnections() )
      {
        if( !connection || connection->isClosed() )
          continue;

        auto remote_peer = connection->remotePeer();
        if( !remote_peer.has_value() )
          continue;

        auto id = remote_peer.value().toBase58();
        std::string address;
        auto remote_address = connection->remoteMultiaddr();
        if( remote_address.has_value() )
          address = with_peer_id_component( std::string( remote_address.value().getStringAddress() ), id );

        auto it = peers.find( id );
        if( it == peers.end() || ( it->second.address.empty() && !address.empty() ) )
          peers[ id ] = PeerID{ id, address };
      }
    }
    catch( const std::exception& e )
    {
      LOG( debug ) << "[p2p/transport] Failed to inspect active connections: " << e.what();
    }
    catch( ... )
    {
      LOG( debug ) << "[p2p/transport] Failed to inspect active connections: unknown exception";
    }
  }

  std::vector< PeerID > result;
  result.reserve( peers.size() );
  for( const auto& [id, peer]: peers )
    result.push_back( peer );
  return result;
}

std::vector< PeerID > Libp2pTransport::known_peers() const
{
  std::map< std::string, PeerID > known;
  {
    std::lock_guard lock( _peers_mutex );
    known = _known_peers;
  }

  if( _host )
  {
    try
    {
      const auto self_id = _host->getId();
      auto& repo = _host->getPeerRepository();
      for( const auto& peer_id: repo.getPeers() )
      {
        if( peer_id == self_id )
          continue;

        auto addresses = repo.getAddressRepository().getAddresses( peer_id );
        if( !addresses.has_value() )
          continue;

        auto id = peer_id.toBase58();
        for( const auto& address: addresses.value() )
        {
          auto address_string = with_peer_id_component( std::string( address.getStringAddress() ), id );
          if( address_string.empty() )
            continue;

          known[ id ] = PeerID{ id, address_string };
          break;
        }
      }
    }
    catch( const std::exception& e )
    {
      LOG( debug ) << "[p2p/transport] Failed to inspect known peers: " << e.what();
    }
    catch( ... )
    {
      LOG( debug ) << "[p2p/transport] Failed to inspect known peers: unknown exception";
    }
  }

  std::vector< PeerID > result;
  result.reserve( known.size() );
  for( const auto& [id, peer]: known )
    result.push_back( peer );
  return result;
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
  auto pid = libp2p::peer::PeerId::fromBase58( peer.id );
  if( !pid.has_value() )
    throw std::runtime_error( "Invalid peer ID: " + peer.id );

  auto dial_addresses = dial_multiaddresses_from( peer.address );

  if( dial_addresses.resolved_dns )
    LOG( info ) << "[p2p/transport] Resolved Peer RPC target " << peer.address
                << " to " << dial_addresses.address_strings.front();

  libp2p::peer::PeerInfo peer_info{ pid.value(), dial_addresses.addresses };

  const auto timeout = std::chrono::seconds( 6 );
  const auto open_operation = "open Peer RPC stream to peer " + peer.id;

  // Open stream (async — block via promise)
  auto stream_promise = std::make_shared< std::promise< libp2p::StreamAndProtocolOrError > >();
  auto stream_future = stream_promise->get_future();

  boost::asio::post( *_io, [host = _host, peer_info, stream_promise, open_operation]() {
    try
    {
      host->newStream(
        peer_info,
        { PEER_RPC_PROTOCOL },
        [stream_promise, open_operation]( libp2p::StreamAndProtocolOrError result ) mutable {
          safe_set_promise_value( stream_promise, std::move( result ), open_operation );
        }
      );
    }
    catch( const std::exception& e )
    {
      safe_set_promise_exception( stream_promise, open_operation + ": " + e.what(), open_operation );
    }
    catch( ... )
    {
      safe_set_promise_exception( stream_promise, open_operation + ": unknown exception", open_operation );
    }
  } );

  if( stream_future.wait_for( timeout ) != std::future_status::ready )
    throw std::runtime_error( "Timed out opening Peer RPC stream to peer: " + peer.id );

  auto stream_result = stream_future.get();
  if( !stream_result.has_value() )
    throw std::runtime_error( "Failed to open stream to peer " + peer.id + ": "
                              + stream_result.error().message() );

  auto stream = std::move( stream_result.value().stream );

  // Write request
  auto request = gorpc::encode_request( service, method, msgpack_args );
  auto req_bytes = std::make_shared< libp2p::Bytes >( request.begin(), request.end() );

  auto write_promise = std::make_shared< std::promise< outcome::result< size_t > > >();
  auto write_future = write_promise->get_future();
  const auto write_operation = "write Peer RPC request to peer " + peer.id;

  boost::asio::post( *_io, [stream, req_bytes, write_promise, write_operation]() {
    safe_stream_write_some( stream, req_bytes, write_promise, write_operation );
  } );

  if( write_future.wait_for( timeout ) != std::future_status::ready )
    throw std::runtime_error( "Timed out writing Peer RPC request" );

  auto write_result = write_future.get();
  if( !write_result.has_value() )
    throw std::runtime_error( "Failed to write RPC request: " + write_result.error().message() );
  if( write_result.value() == 0 )
    throw std::runtime_error( "Failed to write RPC request: zero bytes written" );

  // Read response
  std::string response_raw;
  constexpr size_t read_chunk_size = 64 * 1024;
  constexpr size_t max_response_size = 64 * 1024 * 1024;
  gorpc::Response decoded_response;

  while( true )
  {
    auto response_buf = std::make_shared< libp2p::Bytes >( read_chunk_size );
    auto read_promise = std::make_shared< std::promise< outcome::result< size_t > > >();
    auto read_future = read_promise->get_future();
    const auto read_operation = "read Peer RPC response from peer " + peer.id;

    boost::asio::post( *_io, [stream, response_buf, read_promise, read_operation]() {
      safe_stream_read_some( stream, response_buf, read_promise, read_operation );
    } );

    if( read_future.wait_for( timeout ) != std::future_status::ready )
      throw std::runtime_error( "Timed out reading Peer RPC response" );

    auto read_result = read_future.get();
    if( !read_result.has_value() )
      throw std::runtime_error( "Failed to read RPC response: " + read_result.error().message() );

    auto bytes_read = read_result.value();
    if( bytes_read == 0 )
      throw std::runtime_error( "Peer RPC stream closed before complete response" );

    response_raw.append( response_buf->begin(), response_buf->begin() + bytes_read );

    try
    {
      decoded_response = gorpc::decode_response( response_raw );
      break;
    }
    catch( const gorpc::DecodeError& e )
    {
      if( !e.truncated() )
        throw;
      if( response_raw.size() >= max_response_size )
        throw std::runtime_error( "Peer RPC response exceeds maximum size" );
    }
  }

  boost::asio::post( *_io, [stream, peer_id = peer.id]() {
    safe_stream_close( stream, "close Peer RPC stream to peer " + peer_id );
  } );

  if( !decoded_response.header.error.empty() )
    throw std::runtime_error( "Peer RPC error: " + decoded_response.header.error );

  return decoded_response.payload;
}

void Libp2pTransport::handle_incoming_rpc(
  std::shared_ptr< libp2p::connection::Stream > stream )
{
  auto buf = std::make_shared< libp2p::Bytes >( 64 * 1024 );
  try
  {
    stream->readSome( *buf, buf->size(),
      [this, stream, buf]( outcome::result< size_t > read_result ) {
        try
        {
          if( !read_result.has_value() )
          {
            log_libp2p_async_error( "read incoming Peer RPC request", read_result.error().message() );
            safe_stream_close( stream, "read incoming Peer RPC request" );
            return;
          }
          if( read_result.value() == 0 )
          {
            log_libp2p_async_error( "read incoming Peer RPC request", "stream closed before request" );
            safe_stream_close( stream, "read incoming Peer RPC request" );
            return;
          }

          std::string raw( buf->begin(), buf->begin() + read_result.value() );
          gorpc::DecodedRequest request;
          try
          {
            request = gorpc::decode_request( raw );
          }
          catch( const std::exception& e )
          {
            gorpc::ServiceID unknown{ "unknown", "unknown" };
            auto error = gorpc::encode_error_response( unknown, e.what(), gorpc::ErrorType::server );
            auto error_bytes = std::make_shared< libp2p::Bytes >( error.begin(), error.end() );
            safe_stream_write_and_close( stream, error_bytes, "write Peer RPC decode error response" );
            return;
          }

          LOG( debug ) << "[p2p/transport] Incoming RPC: "
                       << request.service.name << "." << request.service.method;

          std::string response;
          try
          {
            if( !_on_peer_rpc_request )
              throw std::runtime_error( "peer RPC handler is not configured" );

            auto payload = _on_peer_rpc_request( request.service.name, request.service.method, request.args );
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

          safe_stream_write_and_close( stream, resp_bytes, "write Peer RPC response" );
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
      }
    );
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
