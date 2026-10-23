#include "core/owned_allocation.hpp"
#include "core/owned_memory_budget.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// One-shot calling-thread failures. Dependency malloc and machine OOM are
// separate instrumentation axes. Aligned allocation is exercised explicitly.
static thread_local bool fail_next = false;
static thread_local bool watch_new = false;
static thread_local uint64_t new_calls = 0, requested_bytes = 0;
void* operator new( size_t n )
{
  if( watch_new ) { ++new_calls; requested_bytes += n; }
  if( fail_next ) { fail_next = false; throw std::bad_alloc(); }
  if( auto p = koinos::node::owned_allocation::allocate( n ? n : 1 ) ) return p;
  throw std::bad_alloc();
}
void* operator new[]( size_t n ) { return ::operator new( n ); }
void operator delete( void* p ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
void operator delete[]( void* p ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
void operator delete( void* p, size_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
void operator delete[]( void* p, size_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
void* operator new( size_t n, std::align_val_t alignment )
{
  if( watch_new ) { ++new_calls; requested_bytes += n; }
  if( fail_next ) { fail_next = false; throw std::bad_alloc(); }
  return koinos::node::owned_allocation::allocate( n, size_t( alignment ) );
}
void operator delete( void* p, std::align_val_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
void operator delete( void* p, size_t, std::align_val_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }

namespace {
using namespace koinos::node;
void require( bool ok, const char* message ) { if( !ok ) throw std::runtime_error( message ); }
template< typename F > void refused( F&& f )
{
  bool rejected = false;
  try { f(); }
  catch( const LocalCapacityExceeded& e ) { rejected = std::strcmp( e.reason_code(), "mempool_memory_capacity" ) == 0; }
  require( rejected, "aggregate refusal lost its local classification" );
}

void bootstrap_and_construction()
{
  const auto bootstrap = OwnedMemoryBudget::bootstrap_reservation();
  fail_next = true;
  refused( [&] { OwnedMemoryBudget too_small( bootstrap - 1 ); } );
  require( fail_next, "undersized budget allocated before reserving its counter state" );
  fail_next = false;
  watch_new = true; new_calls = requested_bytes = 0;
  OwnedMemoryBudget exact( bootstrap + 1 );
  watch_new = false;
  const auto usage = exact.snapshot();
  require( new_calls > 0 && requested_bytes <= bootstrap &&
           usage.bootstrap_accounted_bytes == bootstrap && usage.accounted_bytes == bootstrap &&
           !usage.allocator_requested_bytes,
           "budget bootstrap allocation escaped its declared supplementary reservation" );
  auto byte = exact.reserve( 1 );
  refused( [&] { exact.reserve( 1 ); } );
  byte.reset();
  require( exact.snapshot().accounted_bytes == bootstrap, "bootstrap credit returned while state remained alive" );
  fail_next = true;
  bool failed = false;
  try { OwnedMemoryBudget allocation_failure( bootstrap ); } catch( const std::bad_alloc& ) { failed = true; }
  require( failed, "bootstrap physical allocation failure was not exercised" );
  OwnedMemoryBudget retry( bootstrap );
  require( retry.snapshot().accounted_bytes == bootstrap, "bootstrap retry was not usable" );
}

void subordinate_state_last_owner()
{
  OwnedMemoryBudget parent( 1024 * 1024 );
  const auto initial = parent.snapshot().accounted_bytes;
  OwnedMemoryBudget::Lease retained;
  {
    OwnedMemoryBudget::Scope scope( parent );
    OwnedMemoryBudget child( 1024, true );
    require( parent.snapshot().accounted_bytes > initial,
             "subordinate counter state escaped its parent domain" );
    retained = child.reserve( 1 );
  }
  require( parent.snapshot().accounted_bytes > initial,
           "subordinate state released parent credit before its last permit" );
  std::thread release( [&] { retained.reset(); } );
  release.join();
  require( parent.snapshot().accounted_bytes == initial,
           "subordinate final owner did not release its parent credit" );
}

void boundary_and_scope()
{
  OwnedMemoryBudget disabled;
  fail_next = true;
  auto untracked = disabled.reserve( UINT64_MAX );
  require( fail_next && !disabled.snapshot().enabled, "disabled reservation allocated or claimed measurement" );
  fail_next = false;
  const auto bootstrap = OwnedMemoryBudget::bootstrap_reservation();
  OwnedMemoryBudget budget( bootstrap + 100 );
  auto a = budget.reserve( 60 ), b = budget.reserve( 40 );
  refused( [&] { budget.reserve( 1 ); } );
  require( budget.snapshot().accounted_bytes == bootstrap + 100, "exact byte boundary changed" );
  b.reset(); auto c = budget.reserve( 40 ); a.reset(); c.reset();
  require( budget.snapshot().accounted_bytes == bootstrap, "moved/reset lease leaked credits" );
  {
    OwnedMemoryBudget::Scope scope( budget, true );
    auto protected_work = budget.reserve( 101 );
    OwnedMemoryBudget other( bootstrap + 1 );
    refused( [&] { other.reserve( 2 ); } );
    {
      OwnedMemoryBudget::Scope other_scope( other );
      refused( [&] { other.reserve( 2 ); } );
    }
    require( budget.snapshot().accounted_bytes == bootstrap + 101, "protected work was lost or limited by local admission" );
  }
  refused( [&] { budget.reserve( 101 ); } );
  require( budget.snapshot().protected_allocations == 1 && budget.snapshot().accounted_bytes == bootstrap,
           "protected scope leaked across return" );
  bool overflow = false;
  try { OwnedMemoryBudget::checked_multiply( UINT64_MAX, 2 ); } catch( const std::bad_alloc& ) { overflow = true; }
  require( overflow, "reservation arithmetic wrapped" );
  for( size_t n: { 0, 1, 15, 16, 23, 31, 32, 127, 128, 8192, 65536 } )
    require( std::string( n, 's' ).capacity() + 1 <= OwnedMemoryBudget::string_reservation( n ),
             "native string capacity exceeded its declared envelope" );
}

struct alignas( 256 ) Aligned { unsigned char bytes[ 256 ]{}; };

template< typename T > void allocator_request_attribution( size_t count )
{
  OwnedMemoryBudget budget( 1 << 20 );
  const auto before = budget.snapshot();
  OwnedMemoryBudget::Allocator< T > allocator( budget );
  new_calls = requested_bytes = 0; watch_new = true;
  auto* value = allocator.allocate( count );
  watch_new = false;
  const auto held = budget.snapshot();
  require( new_calls == 1 && held.allocator_requested_bytes - before.allocator_requested_bytes == requested_bytes,
           "raw allocator requests include supplementary prefix/alignment policy" );
  require( held.accounted_bytes - before.accounted_bytes > requested_bytes,
           "conservative supplementary allowance was removed from admission" );
  allocator.deallocate( value, count );
  require( budget.snapshot().accounted_bytes == before.accounted_bytes &&
           budget.snapshot().allocator_requested_bytes == before.allocator_requested_bytes,
           "raw request accounting outlived final physical deletion" );
}

void allocator_and_last_owner()
{
  OwnedMemoryBudget budget( 1 << 20 );
  std::shared_ptr< const std::string > survivor;
  std::weak_ptr< const std::string > weak;
  {
    OwnedMemoryBudget::Scope scope( budget, false );
    auto first = make_owned_shared_extra< const std::string >( 4096, 1024, 'x' );
    const auto usage = budget.snapshot();
    auto alias = first;
    require( usage.accounted_bytes > 4096 && usage.allocator_requested_bytes > sizeof( std::string ),
             "shared control block/header not accounted" );
    require( budget.snapshot().accounted_bytes == usage.accounted_bytes, "shared alias double counted" );
    survivor = first; weak = first;
    first.reset(); alias.reset();
    require( budget.snapshot().accounted_bytes == usage.accounted_bytes, "caller ownership returned credits early" );
    auto aligned = make_owned_shared< Aligned >();
    require( reinterpret_cast< uintptr_t >( aligned.get() ) % alignof( Aligned ) == 0, "overaligned owner misaligned" );
    aligned.reset();
    fail_next = true;
    bool failed = false;
    try { make_owned_shared< Aligned >(); } catch( const std::bad_alloc& ) { failed = true; }
    require( failed && budget.snapshot().accounted_bytes == usage.accounted_bytes,
             "failed physical allocation retained a reservation" );
    auto retry = make_owned_shared< Aligned >(); retry.reset();
  }
  require( *survivor == std::string( 1024, 'x' ), "owner escaped scope with invalid bytes" );
  survivor.reset();
  require( budget.snapshot().accounted_bytes > budget.snapshot().bootstrap_accounted_bytes,
           "weak control-block allocation returned credits before physical free" );
  weak.reset();
  require( budget.snapshot().accounted_bytes == budget.snapshot().bootstrap_accounted_bytes &&
           !budget.snapshot().allocator_requested_bytes, "last owner leaked credits" );
  { OwnedMemoryBudget temporary( 1 << 20 ); OwnedMemoryBudget::Scope scope( temporary ); survivor = make_owned_shared< const std::string >( "outlives budget" ); }
  require( *survivor == "outlives budget", "allocation outliving facade became invalid" ); survivor.reset();
}

void transfer_and_concurrency()
{
  OwnedMemoryBudget budget( 1 << 20 );
  {
    OwnedMemoryBudget::Scope scope( budget );
    using Alloc = OwnedMemoryBudget::Allocator< std::pair< const std::string, int > >;
    std::map< std::string, int, std::less< std::string >, Alloc > first, second;
    { OwnedMemoryBudget::KeyScope key( 200 ); first.emplace( std::string( 200, 'k' ), 7 ); }
    const auto charged = budget.snapshot().accounted_bytes;
    require( charged > 400, "container key reservation missing" );
    second.insert( first.extract( first.begin() ) );
    require( budget.snapshot().accounted_bytes == charged && second.begin()->second == 7,
             "node transfer changed physical ownership credits" );
  }
  const auto bootstrap = OwnedMemoryBudget::bootstrap_reservation();
  require( budget.snapshot().accounted_bytes == bootstrap, "container destruction leaked credits" );
  OwnedMemoryBudget concurrent( bootstrap + 512 );
  std::atomic< bool > failed = false;
  std::atomic< unsigned > attempts = 0;
  std::vector< std::thread > threads;
  for( unsigned n = 0; n < 8; ++n ) threads.emplace_back( [&] {
    for( unsigned i = 0; i < 2000; ++i )
    {
      try
      {
        auto lease = concurrent.reserve( 128 );
        if( concurrent.snapshot().accounted_bytes > bootstrap + 512 ) failed = true;
        std::this_thread::yield();
      }
      catch( const LocalCapacityExceeded& ) {}
      ++attempts;
    }
  } );
  for( auto& thread: threads ) thread.join();
  const auto usage = concurrent.snapshot();
  require( !failed && attempts == 16000 && usage.accounted_bytes == bootstrap &&
           usage.peak_accounted_bytes <= bootstrap + 512,
           "concurrent aggregate accounting escaped its boundary" );
}

void bounded_refusal_owners()
{
  require( sizeof( OwnedMemoryBudget::Lease ) <= sizeof( std::shared_ptr< void > ) + 2 * sizeof( uint64_t ),
           "Bounded refusal enlarged every ordinary allocation owner" );
  OwnedMemoryBudget budget( 1 << 20, false, 512 );
  const auto bootstrap = budget.snapshot().accounted_bytes;
  auto full = budget.reserve( budget.snapshot().limit_bytes - bootstrap );
  OwnedMemoryBudget::Lease survivor;
  {
    OwnedMemoryBudget::Scope refusal_scope( budget, OwnedMemoryBudget::WorkClass::refusal );
    survivor = budget.reserve( 512 );
    refused( [&] { budget.reserve( 1 ); } );
    {
      OwnedMemoryBudget::Scope inherited( budget );
      refused( [&] { budget.reserve( 1 ); } );
    }
    {
      OwnedMemoryBudget::Scope canonical( budget, true );
      auto progress = budget.reserve( 1024 );
      require( budget.snapshot().refusal_accounted_bytes == 512,
               "Canonical progress entered the bounded refusal allowance" );
    }
  }
  require( budget.snapshot().refusal_accounted_bytes == 512,
           "Refusal scope returned a still-live owner's credits" );
  std::thread release( [&] { survivor.reset(); } ); release.join();
  require( budget.snapshot().refusal_accounted_bytes == 0 &&
           budget.snapshot().accounted_bytes == budget.snapshot().limit_bytes,
           "Cross-thread refusal release lost its accounting class" );
  std::atomic< bool > failed{ false };
  std::vector< std::thread > threads;
  for( unsigned n = 0; n < 8; ++n ) threads.emplace_back( [&] {
    OwnedMemoryBudget::Scope scope( budget, OwnedMemoryBudget::WorkClass::refusal );
    for( unsigned i = 0; i < 1000; ++i )
      try
      {
        auto credit = budget.reserve( 128 );
        if( budget.snapshot().refusal_accounted_bytes > 512 ) failed = true;
        std::this_thread::yield();
      }
      catch( const LocalCapacityExceeded& ) {}
  } );
  for( auto& thread: threads ) thread.join();
  auto snapshot = budget.snapshot();
  require( !failed && snapshot.refusal_accounted_bytes == 0 &&
           snapshot.peak_refusal_accounted_bytes == 512 && snapshot.refusal_capacity_refusals >= 2,
           "Concurrent refusal owners exceeded their shared allowance" );
  bool impossible = false;
  try { budget.reserve( uint64_t( 1 ) << 63, uint64_t( 1 ) << 63 ); }
  catch( const std::bad_alloc& ) { impossible = true; }
  require( impossible && budget.snapshot().accounted_bytes == snapshot.accounted_bytes,
           "Impossible tagged physical request changed credits" );
  full.reset();
  require( budget.snapshot().accounted_bytes == bootstrap, "Bounded refusal final credits leaked" );

  OwnedMemoryBudget captured( 1 << 20, false, 8192 );
  const auto captured_bootstrap = captured.snapshot().accounted_bytes;
  std::shared_ptr< const std::string > caller;
  {
    OwnedMemoryBudget::Scope scope( captured, OwnedMemoryBudget::WorkClass::refusal );
    caller = make_owned_shared< const std::string >( 128, 'r' );
  }
  require( captured.snapshot().refusal_accounted_bytes > 128,
           "Escaped refusal allocation lost its final-owner class" );
  caller.reset();
  require( captured.snapshot().refusal_accounted_bytes == 0 &&
           captured.snapshot().accounted_bytes == captured_bootstrap,
           "Escaped refusal allocation leaked credits" );
}
} // namespace

int main()
{
  try { bootstrap_and_construction(); subordinate_state_last_owner(); boundary_and_scope();
    allocator_request_attribution< char >( 37 ); allocator_request_attribution< Aligned >( 1 );
    allocator_and_last_owner(); transfer_and_concurrency(); bounded_refusal_owners(); }
  catch( const std::exception& e ) { std::cerr << e.what() << '\n'; return 1; }
  std::cout << "Owned memory reservations: boundaries, allocation failure, alignment, last owner, transfer and concurrency passed\n";
}
