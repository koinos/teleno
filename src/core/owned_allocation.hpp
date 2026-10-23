#pragma once

#include <cstddef>

namespace koinos::node::owned_allocation {

// The native executable replaces the replaceable C++ allocation functions.
// Returned pointers remain malloc/posix_memalign base pointers for native SDK
// compatibility. An allocation made within an active M1 domain has a side-table
// lease until physical deletion, including deletion on another thread.
// Direct dependency malloc/realloc, stacks and allocator arenas are separate.
void* allocate( std::size_t bytes,
                std::size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__ );
void deallocate( void* pointer ) noexcept;

// Diagnostic view of the global lease table. Counts describe live table nodes
// and their requested bytes; allocator rounding and arenas remain separate.
struct AllocationTableSnapshot
{
  std::size_t live_entries = 0;
  std::size_t live_entry_requested_bytes = 0;
  std::size_t buckets = 0;
  std::size_t nonempty_buckets = 0;
  std::size_t max_bucket_depth = 0;
};
AllocationTableSnapshot table_snapshot() noexcept;

} // namespace koinos::node::owned_allocation
