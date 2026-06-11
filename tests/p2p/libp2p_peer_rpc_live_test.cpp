#include "p2p/libp2p_transport.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

std::string extract_peer_id( const std::string& multiaddr )
{
  const std::string marker = "/p2p/";
  const auto pos = multiaddr.rfind( marker );
  if( pos == std::string::npos )
    throw std::runtime_error( "multiaddr does not contain /p2p/: " + multiaddr );
  return multiaddr.substr( pos + marker.size() );
}

void require( bool condition, const std::string& message )
{
  if( !condition )
    throw std::runtime_error( message );
}

} // namespace

int main( int argc, char** argv )
{
  if( argc != 2 )
  {
    std::cerr << "usage: " << argv[ 0 ] << " <go-peer-multiaddr>\n";
    return 2;
  }

  const std::string peer_multiaddr = argv[ 1 ];
  const std::string peer_id = extract_peer_id( peer_multiaddr );

  koinos::node::p2p::Libp2pTransport::Config config;
  config.listen_address = "/ip4/127.0.0.1/tcp/0";

  koinos::node::p2p::Libp2pTransport transport( config );
  transport.start();

  koinos::node::p2p::PeerID peer{ peer_id, peer_multiaddr };
  transport.connect_peer( peer );

  std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );

  const auto chain_id = transport.peer_get_chain_id( peer );
  require( !chain_id.empty(), "GetChainID returned an empty chain ID" );

  const auto head = transport.peer_get_head_block( peer );
  require( !head.block_id.empty(), "GetHeadBlock returned an empty head ID" );
  require( head.height == 12, "GetHeadBlock returned unexpected height" );

  const auto ancestor_id = transport.peer_get_ancestor_block_id( peer, head.block_id, 7 );
  require( !ancestor_id.empty(), "GetAncestorBlockID returned an empty ID" );

  const auto blocks = transport.peer_get_blocks( peer, head.block_id, 7, 3 );
  require( blocks.size() == 3, "GetBlocks returned unexpected block count" );
  require( blocks[ 0 ].id() == ancestor_id, "GetBlocks first block ID does not match ancestor ID" );
  require( blocks[ 0 ].has_header() && blocks[ 0 ].header().height() == 7,
           "GetBlocks first block has unexpected height" );
  require( blocks[ 1 ].has_header() && blocks[ 1 ].header().height() == 8,
           "GetBlocks second block has unexpected height" );
  require( blocks[ 2 ].has_header() && blocks[ 2 ].header().height() == 9,
           "GetBlocks third block has unexpected height" );

  transport.stop();

  std::cout << "live peer rpc interop ok\n";
  return 0;
}
