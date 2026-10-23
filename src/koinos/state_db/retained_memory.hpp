#pragma once

#include <koinos/state_db/state_db_types.hpp>
#include "core/owned_memory_budget.hpp"

#include <limits>
#include <unordered_set>

namespace koinos::state_db::detail {

// Short-lived scan workspace. It retains pointer identities, not value owners,
// and must not outlive the caller's exclusion of all backend mutations. Scan
// allocations/failures are outside the reported retained inventory. When an
// owned domain is active, its physical identity-set requests are separately
// charged for the scan's lifetime; they are not retained application payload.
class retained_memory_accumulator
{
public:
  retained_memory_usage usage;

  static void add( uint64_t& total, uint64_t amount )
  {
    KOINOS_ASSERT( amount <= std::numeric_limits< uint64_t >::max() - total,
                   internal_error, "retained-memory inventory overflow" );
    total += amount;
  }

  bool first_backend( const void* backend ) { return _backends.insert( backend ).second; }

  void map_entry( const object_key& key, const object_value_ptr& value )
  {
    add( usage.map_entries, 1 );
    add( usage.map_key_size_bytes, key.size() );
    add( usage.map_key_capacity_bytes, key.capacity() );
    if( value )
    {
      add( usage.value_references, 1 );
      if( _values.insert( value.get() ).second )
      {
        add( usage.unique_values, 1 );
        add( usage.unique_value_size_bytes, value->size() );
        add( usage.unique_value_capacity_bytes, value->capacity() );
      }
    }
  }

  void tombstone( const object_key& key )
  {
    add( usage.tombstone_entries, 1 );
    add( usage.tombstone_key_size_bytes, key.size() );
    add( usage.tombstone_key_capacity_bytes, key.capacity() );
  }

private:
  template< typename T > using identity_set = std::unordered_set< T, std::hash< T >,
    std::equal_to< T >, node::OwnedMemoryBudget::Allocator< T > >;
  identity_set< const void* > _backends;
  identity_set< const object_value* > _values;
};

} // namespace koinos::state_db::detail
