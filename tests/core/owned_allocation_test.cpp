#include "core/owned_allocation.hpp"
#include "core/owned_memory_budget.hpp"

#include <atomic>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#ifdef __GLIBC__
#include <malloc.h>
#endif
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace koinos::node;
const std::string before_main( 8192, 'i' );
void require( bool value, const char* message )
{ if( !value ) throw std::runtime_error( message ); }

void ordinary_owner_and_cross_thread_delete()
{
  OwnedMemoryBudget memory( 1 << 20 );
  const auto bootstrap = memory.snapshot().accounted_bytes;
  std::unique_ptr< std::string > result;
  {
    OwnedMemoryBudget::Scope scope( memory );
    result = std::make_unique< std::string >( 32768, 'p' );
    require( memory.snapshot().accounted_bytes > bootstrap + 32768,
             "ordinary C++ fields did not reserve their allocation" );
  }
  const auto held = memory.snapshot().accounted_bytes;
  require( held > bootstrap && result->size() == 32768,
           "escaped owner returned its credits when its scope ended" );
  std::thread consumer( [value = std::move( result )]() mutable {
    require( value->front() == 'p', "cross-thread escaped value changed" );
    value.reset();
  } );
  consumer.join();
  require( memory.snapshot().accounted_bytes == bootstrap &&
           !memory.snapshot().allocator_requested_bytes,
           "last cross-thread delete did not return exactly its own credits" );
}

void copied_container_fields()
{
  OwnedMemoryBudget memory( 1 << 20 );
  using Values = std::vector< std::string, OwnedMemoryBudget::Allocator< std::string > >;
  Values original{ OwnedMemoryBudget::Allocator< std::string >{ memory } };
  original.emplace_back( 32768, 's' );
  const auto before = memory.snapshot().accounted_bytes;
  {
    // No caller scope: the container allocator must also attribute copied
    // strings before their constructor allocates an independent buffer.
    auto copied = original;
    require( copied.front() == original.front() &&
             memory.snapshot().accounted_bytes > before + 32768,
             "external container copy omitted its independently owned fields" );
  }
  require( memory.snapshot().accounted_bytes == before, "container copy leaked or returned another owner's credits" );
}

struct alignas( 256 ) Aligned { unsigned char bytes[ 257 ]{}; };
void allocation_forms()
{
  OwnedMemoryBudget memory( 1 << 20 );
  const auto before = memory.snapshot().accounted_bytes;
  {
    OwnedMemoryBudget::Scope scope( memory );
    void* empty = ::operator new( 0 );
    require( empty != nullptr, "zero-byte new returned null" );
    ::operator delete( empty );
    auto value = std::make_unique< Aligned >();
    require( reinterpret_cast< uintptr_t >( value.get() ) % 256 == 0, "overaligned new is not aligned" );
    auto values = std::make_unique< Aligned[] >( 3 );
    require( reinterpret_cast< uintptr_t >( values.get() ) % 256 == 0, "overaligned array is not aligned" );
    auto* nothrow_value = new( std::nothrow ) Aligned;
    require( nothrow_value, "nothrow aligned new unexpectedly failed" );
    delete nothrow_value;
    auto* array = new( std::nothrow ) unsigned char[ 5000 ];
    require( array, "nothrow array unexpectedly failed" );
    array[ 17 ] = 42;
    delete[] array;
  }
  require( memory.snapshot().accounted_bytes == before, "allocation form leaked credits" );
  bool overflow = false;
  try { owned_allocation::allocate( std::numeric_limits< size_t >::max() ); }
  catch( const std::bad_alloc& ) { overflow = true; }
  require( overflow && before_main.size() == 8192, "allocation overflow or static initialization failed" );
}

void malloc_base_compatibility()
{
#ifdef __GLIBC__
  OwnedMemoryBudget memory( 1 << 20 );
  void* ordinary = ::operator new( 513 );
  require( malloc_usable_size( ordinary ) >= 513,
           "ordinary new did not return a glibc malloc base pointer" );
  ::operator delete( ordinary );
  {
    OwnedMemoryBudget::Scope scope( memory );
    void* tracked = ::operator new( 8193 );
    require( malloc_usable_size( tracked ) >= 8193,
             "tracked new did not return a glibc malloc base pointer" );
    ::operator delete( tracked );
    void* aligned = ::operator new( 257, std::align_val_t( 256 ) );
    require( reinterpret_cast< uintptr_t >( aligned ) % 256 == 0 &&
             malloc_usable_size( aligned ) >= 257,
             "aligned tracked new lost malloc-base compatibility" );
    ::operator delete( aligned, std::align_val_t( 256 ) );
  }
  require( memory.snapshot().accounted_bytes == memory.snapshot().bootstrap_accounted_bytes,
           "malloc-compatible allocations leaked aggregate credits" );
#endif
}

void table_snapshot_lifetime()
{
  OwnedMemoryBudget memory( 1 << 20 );
  const auto before = owned_allocation::table_snapshot();
  void* first = nullptr;
  void* second = nullptr;
  {
    OwnedMemoryBudget::Scope scope( memory );
    first = ::operator new( 4096 );
    second = ::operator new( 8192 );
  }
  const auto held = owned_allocation::table_snapshot();
  require( held.live_entries == before.live_entries + 2 &&
           held.live_entry_requested_bytes > before.live_entry_requested_bytes &&
           held.buckets == before.buckets && held.nonempty_buckets <= held.buckets,
           "lease table did not report the two escaped allocation owners" );
  ::operator delete( first );
  ::operator delete( second );
  const auto released = owned_allocation::table_snapshot();
  require( released.live_entries == before.live_entries &&
           released.live_entry_requested_bytes == before.live_entry_requested_bytes,
           "lease table retained entries after final physical deletion" );
}

void table_snapshot_during_cross_thread_release()
{
  OwnedMemoryBudget memory( 1 << 20 );
  const auto before = owned_allocation::table_snapshot();
  std::atomic< bool > finished = false;
  std::thread worker( [&] {
    OwnedMemoryBudget::Scope scope( memory );
    for( unsigned n = 0; n < 1000; ++n )
    {
      auto* value = ::operator new( 256 );
      ::operator delete( value );
    }
    finished = true;
  } );
  do
  {
    const auto sample = owned_allocation::table_snapshot();
    require( sample.max_bucket_depth <= sample.live_entries &&
             sample.nonempty_buckets <= sample.buckets,
             "concurrent lease-table sample has invalid occupancy" );
  } while( !finished.load() );
  worker.join();
  require( owned_allocation::table_snapshot().live_entries == before.live_entries &&
           memory.snapshot().accounted_bytes == memory.snapshot().bootstrap_accounted_bytes,
           "concurrent lease-table sampling retained allocation credits" );
}

void refusal_protected_work_and_retry()
{
  OwnedMemoryBudget memory( 32768 );
  const auto bootstrap = memory.snapshot().accounted_bytes;
  auto occupied = memory.reserve( 32768 - bootstrap );
  bool refused = false;
  {
    OwnedMemoryBudget::Scope scope( memory );
    try { auto* p = ::operator new( 4096 ); ::operator delete( p ); }
    catch( const LocalCapacityExceeded& e )
    { refused = std::strcmp( e.reason_code(), "mempool_memory_capacity" ) == 0; }
  }
  require( refused && memory.snapshot().accounted_bytes == 32768,
           "local refusal allocated, returned held credits or lost its classification" );
  {
    OwnedMemoryBudget::Scope recovery( memory, true );
    auto value = std::make_unique< std::string >( 4096, 'c' );
    require( memory.snapshot().accounted_bytes > 32768, "protected work was unaccounted or rejected" );
  }
  require( memory.snapshot().accounted_bytes == 32768, "protected allocation did not release its credits" );
  occupied.reset();
  {
    OwnedMemoryBudget::Scope scope( memory );
    auto value = std::make_unique< std::string >( 4096, 'r' );
    require( value->back() == 'r', "retry changed its result" );
  }
  require( memory.snapshot().accounted_bytes == bootstrap, "successful retry leaked credits" );
}

void concurrent_admission()
{
  OwnedMemoryBudget memory( OwnedMemoryBudget::bootstrap_reservation() + 8192 );
  std::atomic< unsigned > attempted = 0, accepted = 0, refused = 0;
  std::atomic< bool > failed = false;
  std::vector< std::thread > workers;
  for( unsigned n = 0; n < 8; ++n ) workers.emplace_back( [&] {
    OwnedMemoryBudget::Scope scope( memory );
    void* value = nullptr;
    try { value = ::operator new( 4096 ); ++accepted; }
    catch( const LocalCapacityExceeded& ) { ++refused; }
    catch( ... ) { failed = true; }
    ++attempted;
    while( attempted.load() < 8 ) std::this_thread::yield();
    ::operator delete( value );
  } );
  for( auto& worker: workers ) worker.join();
  const auto result = memory.snapshot();
  require( !failed && accepted == 1 && refused == 7 &&
           result.peak_accounted_bytes <= result.limit_bytes &&
           result.accounted_bytes == result.bootstrap_accounted_bytes,
           "concurrent C++ admission exceeded capacity, lost classification or leaked ownership" );
}
}

int main( int argc, char** argv )
{
  try
  {
    // Explicit diagnostic controls, never normal CTest modes. The indices are
    // supplied at runtime so the compiler cannot replace the intended access.
    if( argc == 3 && std::strcmp( argv[ 1 ], "--known-index" ) == 0 )
    {
      OwnedMemoryBudget memory( 1 << 20 );
      OwnedMemoryBudget::Scope scope( memory );
      auto value = std::make_unique< char[] >( 17 );
      const auto index = std::strtol( argv[ 2 ], nullptr, 10 );
      static_cast< volatile char* >( value.get() )[ index ] = 'x';
      return 0;
    }
    if( argc == 2 && std::strcmp( argv[ 1 ], "--known-uaf" ) == 0 )
    {
      OwnedMemoryBudget memory( 1 << 20 );
      OwnedMemoryBudget::Scope scope( memory );
      auto* value = new char[ 17 ];
      delete[] value;
      static_cast< volatile char* >( value )[ 0 ] = 'x';
      return 0;
    }
    if( argc == 2 && std::strcmp( argv[ 1 ], "--known-ub" ) == 0 )
    {
      volatile int maximum = std::numeric_limits< int >::max();
      volatile int overflow = maximum + 1;
      (void)overflow;
      return 0;
    }
    if( argc == 2 && std::strcmp( argv[ 1 ], "--known-race" ) == 0 )
    {
      OwnedMemoryBudget memory( 1 << 20 );
      OwnedMemoryBudget::Scope scope( memory );
      auto value = std::make_unique< int >( 0 );
      volatile int* shared = value.get();
      std::atomic< unsigned > ready = 0;
      std::atomic< bool > start = false;
      auto write = [&] {
        ++ready;
        while( !start.load() ) std::this_thread::yield();
        for( int n = 0; n < 1000; ++n ) *shared = n;
      };
      std::thread first( write ), second( write );
      while( ready.load() != 2 ) std::this_thread::yield();
      start = true;
      first.join(); second.join();
      return 0;
    }
    ordinary_owner_and_cross_thread_delete(); copied_container_fields();
    allocation_forms(); malloc_base_compatibility(); table_snapshot_lifetime();
    table_snapshot_during_cross_thread_release();
    refusal_protected_work_and_retry(); concurrent_admission();
  }
  catch( const std::exception& e ) { std::cerr << e.what() << '\n'; return 1; }
  std::cout << "Native C++ allocation ownership: fields, copies, cross-thread release, forms, refusal, recovery and concurrency passed\n";
}
