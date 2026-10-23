#include "owned_allocation.hpp"
#include "owned_memory_budget.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>
#include <thread>
#include <utility>

namespace koinos::node::owned_allocation {
namespace {
// SDK libraries such as RocksDB may call malloc_usable_size on a pointer
// returned by global C++ new. A prefix makes that pointer invalid to glibc.
// Keep every returned pointer at the malloc/posix_memalign base and retain only
// M1-owned leases in this malloc-backed side table. Its nodes never recurse
// through global C++ new/delete.
struct Tracked
{
  Tracked* next;
  void* pointer;
  OwnedMemoryBudget::Lease lease;
  Tracked( void* pointer, OwnedMemoryBudget::Lease lease ) noexcept
    : next( nullptr ), pointer( pointer ), lease( std::move( lease ) ) {}
};
struct Bucket
{
  std::atomic_flag lock = ATOMIC_FLAG_INIT;
  Tracked* head = nullptr;
};
// Deallocation must also check ordinary untracked pointers. A small table
// makes each such check traverse a long M1-owned chain as retained fork state
// grows. Keep the lookup depth short for the low-memory producer workload;
// the static table itself occupies about 1 MiB on 64-bit Linux.
constexpr std::size_t bucket_count = 65536;
Bucket buckets[ bucket_count ];
std::atomic< std::size_t > tracked_count{ 0 };

Bucket& bucket_for( void* pointer ) noexcept
{ return buckets[ ( reinterpret_cast< std::uintptr_t >( pointer ) >> 4 ) & ( bucket_count - 1 ) ]; }

class BucketLock
{
  Bucket& bucket;
public:
  explicit BucketLock( Bucket& bucket ) noexcept : bucket( bucket )
  { while( bucket.lock.test_and_set( std::memory_order_acquire ) ) std::this_thread::yield(); }
  ~BucketLock() { bucket.lock.clear( std::memory_order_release ); }
};

void* raw_allocate( std::size_t bytes, std::size_t alignment ) noexcept
{
  if( alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__ ) return std::malloc( bytes );
  void* pointer = nullptr;
  return posix_memalign( &pointer, alignment, bytes ) == 0 ? pointer : nullptr;
}
}

void* allocate( std::size_t bytes, std::size_t alignment )
{
  alignment = std::max( alignment, std::size_t( __STDCPP_DEFAULT_NEW_ALIGNMENT__ ) );
  if( ( alignment & ( alignment - 1 ) ) != 0 ) throw std::bad_alloc();
  const auto payload = std::max( bytes, std::size_t( 1 ) );
  // malloc implementations and sanitizers are not required to turn an
  // impossible SIZE_MAX request into a recoverable null result.
  if( payload > std::numeric_limits< std::size_t >::max() - alignment - sizeof( Tracked ) - 128 )
    throw std::bad_alloc();
  const bool tracked = OwnedMemoryBudget::capture_allocation();
  // The side-table node and conservative allocator/alignment headroom are
  // reserved before either allocation. requested excludes allocator metadata.
  auto lease = tracked ? OwnedMemoryBudget::current().reserve(
    OwnedMemoryBudget::checked_add(
      OwnedMemoryBudget::checked_add( payload, sizeof( Tracked ) ),
      OwnedMemoryBudget::checked_add( alignment, payload >= 65536 ? 16384 : 128 ) ),
    OwnedMemoryBudget::checked_add( payload, sizeof( Tracked ) ) ) : OwnedMemoryBudget::Lease{};
  void* pointer = raw_allocate( payload, alignment );
  if( !pointer ) throw std::bad_alloc();
  if( !tracked ) return pointer;
  void* raw_node = std::malloc( sizeof( Tracked ) );
  if( !raw_node ) { std::free( pointer ); throw std::bad_alloc(); }
  auto* node = new( raw_node ) Tracked( pointer, std::move( lease ) );
  auto& bucket = bucket_for( pointer );
  {
    BucketLock guard( bucket );
    node->next = bucket.head;
    bucket.head = node;
    tracked_count.fetch_add( 1, std::memory_order_release );
  }
  return pointer;
}

void deallocate( void* pointer ) noexcept
{
  if( !pointer ) return;
  Tracked* owner = nullptr;
  if( tracked_count.load( std::memory_order_acquire ) )
  {
    auto& bucket = bucket_for( pointer );
    BucketLock guard( bucket );
    auto** link = &bucket.head;
    while( *link && ( *link )->pointer != pointer ) link = &( *link )->next;
    if( *link )
    {
      owner = *link;
      *link = owner->next;
      tracked_count.fetch_sub( 1, std::memory_order_release );
    }
  }
  if( !owner ) { std::free( pointer ); return; }
  auto lease = std::move( owner->lease );
  owner->~Tracked();
  std::free( owner );
  std::free( pointer );
  // Release after physical free, including when the owner outlives its facade.
}

AllocationTableSnapshot table_snapshot() noexcept
{
  AllocationTableSnapshot result;
  result.buckets = bucket_count;
  // Each bucket is inspected under its own lock. This is a diagnostic sample,
  // not a process-wide atomic snapshot while other threads allocate.
  for( auto& bucket: buckets )
  {
    BucketLock guard( bucket );
    std::size_t depth = 0;
    for( auto* entry = bucket.head; entry; entry = entry->next ) ++depth;
    result.live_entries += depth;
    result.nonempty_buckets += depth != 0;
    result.max_bucket_depth = std::max( result.max_bucket_depth, depth );
  }
  result.live_entry_requested_bytes = result.live_entries * sizeof( Tracked );
  return result;
}
} // namespace koinos::node::owned_allocation

// Weak replacements let deterministic test executables retain their existing
// fault injection/counting wrappers. Those wrappers call the same allocate /
// deallocate primitives, so qualification exercises the production ownership.
#if defined(__GNUC__) || defined(__clang__)
#define TELENO_REPLACEABLE __attribute__((weak))
#else
#define TELENO_REPLACEABLE
#endif
namespace {
void* with_new_handler( std::size_t bytes, std::size_t alignment )
{
  for( ;; )
  {
    try { return koinos::node::owned_allocation::allocate( bytes, alignment ); }
    catch( const std::bad_alloc& )
    {
      const auto handler = std::get_new_handler();
      if( !handler ) throw;
      handler();
    }
  }
}
}
TELENO_REPLACEABLE void* operator new( std::size_t n )
{ return with_new_handler( n, __STDCPP_DEFAULT_NEW_ALIGNMENT__ ); }
TELENO_REPLACEABLE void* operator new[]( std::size_t n ) { return ::operator new( n ); }
TELENO_REPLACEABLE void operator delete( void* p ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
TELENO_REPLACEABLE void operator delete[]( void* p ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
TELENO_REPLACEABLE void operator delete( void* p, std::size_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
TELENO_REPLACEABLE void operator delete[]( void* p, std::size_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
TELENO_REPLACEABLE void* operator new( std::size_t n, std::align_val_t a ) { return with_new_handler( n, std::size_t( a ) ); }
TELENO_REPLACEABLE void* operator new[]( std::size_t n, std::align_val_t a ) { return ::operator new( n, a ); }
TELENO_REPLACEABLE void operator delete( void* p, std::align_val_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
TELENO_REPLACEABLE void operator delete[]( void* p, std::align_val_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
TELENO_REPLACEABLE void operator delete( void* p, std::size_t, std::align_val_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
TELENO_REPLACEABLE void operator delete[]( void* p, std::size_t, std::align_val_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
TELENO_REPLACEABLE void* operator new( std::size_t n, const std::nothrow_t& ) noexcept
{ try { return ::operator new( n ); } catch( ... ) { return nullptr; } }
TELENO_REPLACEABLE void* operator new[]( std::size_t n, const std::nothrow_t& ) noexcept
{ try { return ::operator new[]( n ); } catch( ... ) { return nullptr; } }
TELENO_REPLACEABLE void operator delete( void* p, const std::nothrow_t& ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
TELENO_REPLACEABLE void operator delete[]( void* p, const std::nothrow_t& ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
TELENO_REPLACEABLE void* operator new( std::size_t n, std::align_val_t a, const std::nothrow_t& ) noexcept
{ try { return ::operator new( n, a ); } catch( ... ) { return nullptr; } }
TELENO_REPLACEABLE void* operator new[]( std::size_t n, std::align_val_t a, const std::nothrow_t& ) noexcept
{ try { return ::operator new[]( n, a ); } catch( ... ) { return nullptr; } }
TELENO_REPLACEABLE void operator delete( void* p, std::align_val_t, const std::nothrow_t& ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
TELENO_REPLACEABLE void operator delete[]( void* p, std::align_val_t, const std::nothrow_t& ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
