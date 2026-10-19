#include "core/monolith_rpc_client.hpp"

#include <atomic>
#include <cstdlib>
#include <future>
#include <iostream>
#include <new>
#include <thread>
#include <vector>

// Calling-thread C++ allocation failure, not dependency malloc/machine OOM.
static thread_local bool fail_next = false;
void* operator new( std::size_t n )
{
  if( fail_next ) { fail_next = false; throw std::bad_alloc(); }
  if( auto p = std::malloc( n ? n : 1 ) ) return p;
  throw std::bad_alloc();
}
void* operator new[]( std::size_t n ) { return ::operator new( n ); }
void operator delete( void* p ) noexcept { std::free( p ); }
void operator delete[]( void* p ) noexcept { std::free( p ); }
void operator delete( void* p, std::size_t ) noexcept { std::free( p ); }
void operator delete[]( void* p, std::size_t ) noexcept { std::free( p ); }

using namespace koinos::node;
void require( bool ok ) { if( !ok ) throw std::runtime_error( "transaction work budget assertion failed" ); }
template< typename F > void refused( F f )
{
  bool caught = false;
  try { f(); } catch( const LocalCapacityExceeded& ) { caught = true; }
  require( caught );
}

int main()
{
  TransactionWorkBudget disabled;
  fail_next = true;
  require( !disabled.reserve( UINT64_MAX ) );
  require( fail_next ); fail_next = false;
  require( !disabled.snapshot().enabled );

  TransactionWorkBudget jobs( { 2, 0 } );
  auto a = jobs.reserve( 0 ), b = jobs.reserve( 100 );
  fail_next = true;
  refused( [&] { jobs.reserve( 0 ); } );
  require( fail_next ); fail_next = false; // refusal message needs no C++ allocation
  require( jobs.snapshot().jobs == 2 && jobs.snapshot().wire_bytes == 100 );
  b.reset(); a.reset();
  fail_next = true;
  bool allocation_failed = false;
  try { jobs.reserve( 1 ); } catch( const std::bad_alloc& ) { allocation_failed = true; }
  require( allocation_failed && jobs.snapshot().jobs == 0 && jobs.snapshot().wire_bytes == 0 );
  a = jobs.reserve( 1 ); a.reset();

  TransactionWorkBudget bytes( { 0, 100 } );
  a = bytes.reserve( 60 ); b = bytes.reserve( 40 );
  refused( [&] { bytes.reserve( 1 ); } );
  auto empty = bytes.reserve( 0 ); // byte-only does not secretly impose job limits
  require( bytes.snapshot().jobs == 3 && bytes.snapshot().wire_bytes == 100 );
  a.reset(); b.reset(); empty.reset();
  refused( [&] { bytes.reserve( 101 ); } );
  a = bytes.reserve( 100 ); a.reset();

  TransactionWorkBudget combined( { 2, 100 } );
  a = combined.reserve( 70 );
  refused( [&] { combined.reserve( 31 ); } );
  require( combined.snapshot().jobs == 1 && combined.snapshot().wire_bytes == 70 );
  b = combined.reserve( 30 ); refused( [&] { combined.reserve( 0 ); } );
  a.reset(); b.reset();

  TransactionWorkBudget overflow( { 3, 0 } );
  a = overflow.reserve( UINT64_MAX );
  refused( [&] { overflow.reserve( 1 ); } );
  b = overflow.reserve( 0 ); b.reset(); a.reset();
  require( overflow.snapshot().wire_bytes == 0 );

  // A moved lease may outlive its budget; release is allocation-free.
  std::unique_ptr< TransactionWorkReservation > survivor;
  { TransactionWorkBudget owner( { 1, 10 } ); survivor = owner.reserve( 10 ); }
  auto moved = std::move( survivor );
  fail_next = true; moved.reset(); require( fail_next ); fail_next = false;

  // Aggregate across distinct accounts; wire size is measured, not heap size.
  MonolithRpcClient native( nullptr, nullptr, nullptr, {}, { 2'000, 0 } );
  std::vector< std::unique_ptr< TransactionWorkReservation > > leases;
  uint64_t expected = 0;
  for( unsigned i = 0; i < 2'000; ++i )
  {
    koinos::protocol::transaction tx;
    tx.mutable_header()->set_payer( "synthetic-account-" + std::to_string( i ) );
    tx.add_signatures( std::string( 1024, 's' ) );
    expected += tx.ByteSizeLong();
    leases.push_back( native.reserve_transaction_work( tx ) );
  }
  koinos::protocol::transaction tx;
  refused( [&] { native.reserve_transaction_work( tx ); } );
  require( native.transaction_work_snapshot().wire_bytes == expected );
  leases.clear(); require( native.transaction_work_snapshot().jobs == 0 );

  TransactionWorkBudget concurrent( { 3, 30 } );
  std::vector< std::future< void > > workers;
  std::atomic< unsigned > successes = 0, rejections = 0;
  for( unsigned thread = 0; thread < 8; ++thread )
    workers.push_back( std::async( std::launch::async, [&] {
      for( unsigned i = 0; i < 500; ++i )
      {
        try
        {
          auto lease = concurrent.reserve( 10 );
          const auto s = concurrent.snapshot();
          require( s.jobs <= 3 && s.wire_bytes <= 30 );
          std::this_thread::yield(); ++successes;
        }
        catch( const LocalCapacityExceeded& ) { ++rejections; }
      }
    } ) );
  for( auto& worker : workers ) worker.get();
  const auto done = concurrent.snapshot();
  require( !done.jobs && !done.wire_bytes && done.peak_jobs <= 3 && done.peak_wire_bytes <= 30 );
  require( successes + rejections == 4'000 && rejections == done.refusals );
  std::cout << "Transaction work reservations passed: concurrent_attempts=4000 distinct_accounts=2000"
    << " account_input_wire_bytes=" << expected << " peak_concurrent_jobs=" << done.peak_jobs
    << " peak_concurrent_wire_bytes=" << done.peak_wire_bytes
    << " capacity_refusals=" << done.refusals << " final_jobs=" << done.jobs
    << " final_wire_bytes=" << done.wire_bytes << '\n';
}
