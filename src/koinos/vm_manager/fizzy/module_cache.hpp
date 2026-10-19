#pragma once
#include <fizzy/fizzy.h>
#include <koinos/vm_manager/fizzy/module_memory.hpp>
#include <koinos/vm_manager/vm_memory.hpp>

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <utility>

namespace koinos::vm_manager::fizzy {

class module_guard
{
private:
  const FizzyModule* _module;
  const std::optional< uint64_t > _capacity;

public:
  explicit module_guard( const FizzyModule* m ) noexcept:
      _module( m ),
      _capacity( parsed_module_capacity_bytes( m ) )
  {}

  module_guard( const module_guard& ) = delete;
  module_guard& operator=( const module_guard& ) = delete;

  ~module_guard() noexcept
  {
    fizzy_free_module( _module );
  }

  const FizzyModule* get() const
  {
    return _module;
  }

  std::optional< uint64_t > capacity_bytes() const noexcept
  {
    if( !_capacity ) return {};
    capacity_sum sum;
    sum.add( *_capacity );
    sum.add( sizeof( *this ) );
    return sum.value;
  }
};

using module_ptr = std::shared_ptr< const module_guard >;

class module_cache
{
private:
  using lru_list_type = std::list< std::string >;

  struct entry
  {
    module_ptr module;
    lru_list_type::iterator lru;
    uint64_t charge = 0;
  };
  using module_map_type = std::map< std::string, entry >;

  lru_list_type _lru_list;
  module_map_type _module_map;
  mutable std::mutex _mutex;
  // Serialize cold parsing only. Warm hits do not wait for this mutex, and
  // no parser/cache lock may be held while executing a module or host callback.
  std::mutex _parse_mutex;
  const std::size_t _cache_size;
  const uint64_t _cache_capacity;
  module_cache_memory _memory;
  void retire( module_map_type::iterator position );
  std::optional< uint64_t > charge( const std::string& key, const entry& value ) const noexcept;

public:
  module_cache( std::size_t size, uint64_t max_capacity_bytes = 0 );
  ~module_cache();

  module_ptr get_module( const std::string& id );
  void put_module( const std::string& id, module_ptr module );
  module_cache_memory memory() const;
  void clear();

  template< typename Loader >
  module_ptr get_or_load( const std::string& id, Loader&& loader )
  {
    if( !id.empty() )
      if( auto cached = get_module( id ) )
        return cached;

    std::lock_guard< std::mutex > parse_lock( _parse_mutex );
    // Another cold caller may have populated this ID while we waited.
    if( !id.empty() )
      if( auto cached = get_module( id ) )
        return cached;

    auto parsed = std::forward< Loader >( loader )();
    if( !id.empty() )
    {
      try
      {
        put_module( id, parsed );
      }
      catch( const std::bad_alloc& )
      {
        // Retention is optional; a fully owned parsed module remains usable.
        // Parse/instance/execution failures are not swallowed here.
      }
    }
    return parsed;
  }
};

} // namespace koinos::vm_manager::fizzy
