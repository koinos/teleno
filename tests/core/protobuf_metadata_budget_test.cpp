#include "core/owned_allocation.hpp"
#include "core/owned_memory_budget.hpp"

#include <koinos/rpc/chain/chain_rpc.pb.h>

#include <atomic>
#include <barrier>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using koinos::node::LocalCapacityExceeded;
using koinos::node::OwnedMemoryBudget;
using Reply = koinos::rpc::chain::submit_transaction_response;

void require( bool value, const char* message )
{
  if( !value ) throw std::runtime_error( message );
}

void cold_metadata_at_full_capacity( bool concurrent )
{
  // Construct fields without warming reflection or SpaceUsedLong. Lazy field
  // descriptors must not be charged to whichever request happens to size first.
  Reply reply;
  reply.mutable_receipt()->set_id( std::string( 32, 'i' ) );
  reply.mutable_receipt()->add_logs( std::string( 16384, 'r' ) );
  OwnedMemoryBudget budget( 1024 * 1024 );
  const auto bootstrap = budget.snapshot().accounted_bytes;
  auto occupied = budget.reserve( 1024 * 1024 - bootstrap );
  std::atomic< unsigned > sized = 0, failures = 0;
  const auto inspect = [&] {
    try
    {
      OwnedMemoryBudget::Scope scope( budget, false );
      for( unsigned n = 0; n < 16; ++n )
      {
        require( OwnedMemoryBudget::protobuf_space_used( reply ) >= 16384,
                 "sizing lost the nested receipt payload" );
        ++sized;
      }
    }
    catch( ... ) { ++failures; }
  };
  if( concurrent )
  {
    std::barrier start( 8 );
    std::vector< std::thread > workers;
    for( unsigned n = 0; n < 8; ++n )
      workers.emplace_back( [&] { start.arrive_and_wait(); inspect(); } );
    for( auto& worker : workers ) worker.join();
  }
  else inspect();
  require( failures == 0 && sized == ( concurrent ? 128u : 16u ),
           "cold immutable Protobuf metadata used the exhausted request quota" );
  const auto full = budget.snapshot();
  require( full.accounted_bytes == full.limit_bytes && !full.allocator_requested_bytes,
           "metadata sizing changed request-owned credits" );

  // Excluding immutable descriptors must not exclude the actual reply copy.
  bool refused = false;
  {
    OwnedMemoryBudget::Scope scope( budget, false );
    try { Reply copy( reply ); }
    catch( const LocalCapacityExceeded& error )
    { refused = std::strcmp( error.reason_code(), "mempool_memory_capacity" ) == 0; }
  }
  require( refused && reply.receipt().logs( 0 ).size() == 16384,
           "metadata initialization bypassed message-copy admission or changed input" );
  occupied.reset();
  {
    OwnedMemoryBudget::Scope scope( budget, false );
    auto copy = std::make_unique< Reply >( reply );
    require( copy->SerializeAsString() == reply.SerializeAsString() &&
             budget.snapshot().accounted_bytes > bootstrap + 16384,
             "retry lost bytes or message-owned accounting" );
  }
  require( budget.snapshot().accounted_bytes == bootstrap &&
           !budget.snapshot().allocator_requested_bytes,
           "reply destruction did not return its own credits" );
}
} // namespace

int main( int argc, char** argv )
{
  try
  {
    const bool concurrent = argc == 2 && std::strcmp( argv[ 1 ], "--concurrent" ) == 0;
    cold_metadata_at_full_capacity( concurrent );
    std::cout << "Cold Protobuf metadata and admitted copy retry completed\n";
    return 0;
  }
  catch( const std::exception& error )
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
