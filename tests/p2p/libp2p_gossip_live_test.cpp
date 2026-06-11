#include "p2p/libp2p_transport.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

using namespace koinos::node::p2p;
namespace protocol = koinos::protocol;

namespace {

std::string peer_id_from_multiaddr( const std::string& multiaddr )
{
  auto marker = multiaddr.rfind( "/p2p/" );
  if( marker == std::string::npos )
    throw std::runtime_error( "multiaddr has no /p2p/ peer ID" );
  return multiaddr.substr( marker + 5 );
}

protocol::block make_cpp_block()
{
  protocol::block block;
  block.set_id( "cpp-block-77" );
  block.set_signature( "cpp-producer" );
  block.mutable_header()->set_previous( "cpp-block-76" );
  block.mutable_header()->set_height( 77 );
  return block;
}

protocol::transaction make_cpp_transaction()
{
  protocol::transaction tx;
  tx.set_id( "cpp-tx-77" );
  return tx;
}

class GossipProbe
{
public:
  void mark_block( const protocol::block& block )
  {
    std::lock_guard lock( _mutex );
    if( block.id() == "go-block-33" && block.has_header() && block.header().height() == 33 )
    {
      _saw_go_block = true;
      _cv.notify_all();
    }
  }

  void mark_transaction( const protocol::transaction& tx )
  {
    std::lock_guard lock( _mutex );
    if( tx.id() == "go-tx-33" )
    {
      _saw_go_tx = true;
      _cv.notify_all();
    }
  }

  bool wait_for_go_messages()
  {
    std::unique_lock lock( _mutex );
    return _cv.wait_for( lock, std::chrono::seconds( 15 ), [&] {
      return _saw_go_block && _saw_go_tx;
    } );
  }

  bool wait_for_go_messages_for( std::chrono::milliseconds timeout )
  {
    std::unique_lock lock( _mutex );
    return _cv.wait_for( lock, timeout, [&] {
      return _saw_go_block && _saw_go_tx;
    } );
  }

private:
  std::mutex _mutex;
  std::condition_variable _cv;
  bool _saw_go_block = false;
  bool _saw_go_tx = false;
};

} // namespace

int main( int argc, char** argv )
{
  if( argc != 2 )
    return 2;

  const std::string peer_addr = argv[ 1 ];
  PeerID peer{ peer_id_from_multiaddr( peer_addr ), peer_addr };

  Libp2pTransport::Config config;
  config.listen_address = "/ip4/127.0.0.1/tcp/0";
  config.seed_peers = { peer_addr };

  GossipProbe probe;
  Libp2pTransport transport( config );
  transport.on_block_received( [&]( const PeerID&, const protocol::block& block ) {
    probe.mark_block( block );
  } );
  transport.on_transaction_received( [&]( const PeerID&, const protocol::transaction& tx ) {
    probe.mark_transaction( tx );
  } );
  transport.start();

  auto block = make_cpp_block();
  auto tx = make_cpp_transaction();
  auto start = std::chrono::steady_clock::now();
  while( std::chrono::steady_clock::now() - start < std::chrono::seconds( 15 ) )
  {
    transport.publish_block( block );
    transport.publish_transaction( tx );
    if( probe.wait_for_go_messages_for( std::chrono::milliseconds( 250 ) ) )
      break;
  }

  const bool saw_go_messages = probe.wait_for_go_messages();
  if( saw_go_messages )
  {
    for( int i = 0; i < 8; ++i )
    {
      transport.publish_block( block );
      transport.publish_transaction( tx );
      std::this_thread::sleep_for( std::chrono::milliseconds( 250 ) );
    }
  }
  transport.stop();
  assert( saw_go_messages );

  return 0;
}
