#pragma once

#include <koinos/mempool/mempool.hpp>
#include <koinos/chain/value.pb.h>
#include <koinos/util/conversion.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

namespace mempool_memory_fixture {
using namespace koinos;

inline void require( bool ok, const char* message )
{ if( !ok ) throw std::runtime_error( message ); }

inline auto id( uint64_t n ) { return crypto::hash( crypto::multicodec::sha2_256, n ); }
inline auto at() { return std::chrono::system_clock::time_point{ std::chrono::seconds{ 1000 } }; }

inline broadcast::block_accepted accepted( uint64_t n, uint64_t parent, uint64_t height,
                                          const std::string& included = {} )
{
  broadcast::block_accepted b;
  b.mutable_block()->set_id( util::converter::as< std::string >( id( n ) ) );
  b.mutable_block()->mutable_header()->set_previous( util::converter::as< std::string >(
    parent ? id( parent ) : crypto::multihash::zero( crypto::multicodec::sha2_256 ) ) );
  b.mutable_block()->mutable_header()->set_height( height );
  if( !included.empty() ) b.mutable_block()->add_transactions()->set_id( included );
  return b;
}

inline void block( mempool::mempool& p, uint64_t n, uint64_t parent, uint64_t height )
{ require( p.handle_block( accepted( n, parent, height ) ), "fixture block not linked" ); }

inline void lib( mempool::mempool& p, uint64_t n )
{
  broadcast::block_irreversible b;
  b.mutable_topology()->set_id( util::converter::as< std::string >( id( n ) ) );
  p.handle_irreversibility( b );
}

inline auto transaction( uint64_t n, size_t signature_bytes = 8192 )
{
  mempool::pending_transaction p;
  auto* t = p.mutable_transaction();
  t->set_id( std::string( 40, 'i' ) + std::to_string( n ) );
  t->mutable_header()->set_payer( std::string( 40, 'p' ) + std::to_string( n ) );
  t->mutable_header()->set_rc_limit( 100 );
  chain::value_type nonce;
  nonce.set_uint64_value( 1 );
  t->mutable_header()->set_nonce( util::converter::as< std::string >( nonce ) );
  t->add_signatures( std::string( signature_bytes, 's' ) );
  return p;
}

inline void initialize( mempool::mempool& p )
{ block( p, 1, 0, 1 ); block( p, 2, 1, 2 ); block( p, 3, 1, 2 ); }

inline auto snapshot( mempool::mempool& p )
{
  std::vector< std::string > result;
  for( const auto& point: { crypto::multihash::zero( crypto::multicodec::sha2_256 ), id( 1 ), id( 2 ), id( 3 ) } )
  {
    for( const auto& value: p.get_pending_transactions( 2000, point ) ) result.push_back( value.SerializeAsString() );
    for( unsigned n = 1; n <= 3; ++n )
    {
      const auto payer = transaction( n, 0 ).transaction().header().payer();
      result.push_back( p.get_pending_nonce( payer, point ) );
      result.push_back( std::to_string( p.get_pending_transaction_count( payer, point ) ) );
    }
  }
  // Detect a partially published head whose corresponding pending state is
  // absent, even if explicitly selected old forks still contain their records.
  for( const auto& value: p.get_pending_transactions( 2000 ) ) result.push_back( value.SerializeAsString() );
  return result;
}
} // namespace mempool_memory_fixture
