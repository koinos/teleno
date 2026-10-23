#include "p2p/libp2p_transport.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using koinos::node::p2p::Libp2pTransport;

namespace {
constexpr uint16_t port = 19088;

koinos::protocol::block make_block( unsigned lane, unsigned index )
{
  koinos::protocol::block block;
  block.set_id( "fixture-block-" + std::to_string( lane ) + "-" + std::to_string( index ) );
  block.mutable_header()->set_height( index + 1 );
  return block;
}

void incoming_loop( std::atomic< unsigned >& accepted )
{
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons( port );
  address.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
  for( unsigned i = 0; i < 200; ++i )
  {
    int fd = ::socket( AF_INET, SOCK_STREAM, 0 );
    if( fd < 0 ) continue;
    if( ::connect( fd, reinterpret_cast< const sockaddr* >( &address ),
                   sizeof( address ) ) == 0 )
    {
      ++accepted;
      static constexpr char bytes[] = "invalid-noise-fixture";
      (void)::send( fd, bytes, sizeof( bytes ), MSG_NOSIGNAL );
    }
    ::close( fd );
    if( i % 4 == 0 ) std::this_thread::yield();
  }
}

void concurrent_publication( Libp2pTransport& transport, unsigned round )
{
  std::barrier start( 5 );
  std::atomic< unsigned > incoming{ 0 };
  std::vector< std::thread > publishers;
  for( unsigned lane = 0; lane < 4; ++lane )
    publishers.emplace_back( [&, lane] {
      start.arrive_and_wait();
      for( unsigned i = 0; i < 5000; ++i )
      {
        auto block = make_block( lane + 4 * round, i );
        transport.publish_block( block );
        if( i % 5 == 0 )
          std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
      }
    } );
  std::thread ingress( incoming_loop, std::ref( incoming ) );
  start.arrive_and_wait();
  for( auto& thread: publishers ) thread.join();
  ingress.join();
  if( incoming.load() == 0 ) throw std::runtime_error( "no loopback ingress reached the listener" );
  std::this_thread::sleep_for( std::chrono::milliseconds( 1250 ) );
  std::cout << "PUBLICATION_ROUND " << round << " ingress=" << incoming.load() << std::endl;
}

void stop_during_publication( Libp2pTransport& transport )
{
  std::atomic< bool > publish{ true };
  std::atomic< unsigned > calls{ 0 };
  std::thread caller( [&] {
    auto block = make_block( 99, 1 );
    while( publish.load( std::memory_order_acquire ) )
    {
      transport.publish_block( block );
      calls.fetch_add( 1, std::memory_order_relaxed );
      if( calls.load( std::memory_order_relaxed ) % 10 == 0 )
        std::this_thread::yield();
    }
  } );
  auto until = std::chrono::steady_clock::now() + std::chrono::seconds( 2 );
  while( calls.load( std::memory_order_relaxed ) < 1000
         && std::chrono::steady_clock::now() < until )
    std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
  transport.stop();
  publish.store( false, std::memory_order_release );
  caller.join();
  if( calls.load() < 1000 ) throw std::runtime_error( "publication/stop overlap was not exercised" );
  std::cout << "STOP_OVERLAP calls=" << calls.load() << std::endl;
}
} // namespace

int main( int argc, char** argv )
{
  if( argc != 2 ) return 2;
  const std::string mode = argv[ 1 ];
  if( mode != "publish-heartbeat" && mode != "stop-race" && mode != "no-publish" )
    return 2;

  Libp2pTransport::Config config;
  config.listen_address = "/ip4/127.0.0.1/tcp/19088";
  config.enable_dht = false;
  config.seed_peers.clear();
  config.discovery_peers.clear();
  Libp2pTransport transport( config );
  transport.start();
  if( mode == "publish-heartbeat" )
  {
    concurrent_publication( transport, 0 );
    transport.stop();
    transport.start();
    concurrent_publication( transport, 1 );
    transport.stop();
  }
  else if( mode == "stop-race" )
  {
    stop_during_publication( transport );
    transport.start();
    transport.stop();
  }
  else
  {
    std::atomic< unsigned > incoming{ 0 };
    std::thread ingress( incoming_loop, std::ref( incoming ) );
    ingress.join();
    if( incoming.load() == 0 ) throw std::runtime_error( "no loopback ingress reached the listener" );
    transport.stop();
    transport.start();
    transport.stop();
    std::cout << "NO_PUBLICATION_CONTROL ingress=" << incoming.load() << std::endl;
  }
  return 0;
}
