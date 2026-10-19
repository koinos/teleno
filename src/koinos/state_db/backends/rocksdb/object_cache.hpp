#pragma once

#include <koinos/state_db/backends/types.hpp>

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace koinos::state_db::backends::rocksdb {

class object_cache
{
public:
  using key_type   = detail::key_type;
  using value_type = detail::value_type;

private:
  using lru_list_type = std::list< key_type >;
  using value_map_type =
    std::map< key_type, std::pair< std::shared_ptr< const value_type >, typename lru_list_type::iterator > >;

  lru_list_type _lru_list;
  value_map_type _object_map;
  std::size_t _cache_size = 0;
  const std::size_t _cache_max_size;
  std::mutex _mutex;

public:
  object_cache( std::size_t size );
  ~object_cache();

  std::pair< bool, std::shared_ptr< const value_type > > get( const key_type& k );
  std::shared_ptr< const value_type > put( const key_type& k, std::shared_ptr< const value_type > v );

  void remove( const key_type& k );

  void clear();

  // Caller holds get_mutex(), as for the existing get/put/clear API. Payload
  // excludes map/list nodes, key copies, shared_ptr/string headers and allocator
  // retention and caller-held/evicted owners. Oversized/zero-budget values
  // return an owner without retention; payload_bytes never exceeds capacity.
  std::size_t payload_bytes() const { return _cache_size; }
  std::size_t entry_count() const { return _object_map.size(); }
  std::size_t capacity_bytes() const { return _cache_max_size; }

  std::mutex& get_mutex();
};

} // namespace koinos::state_db::backends::rocksdb
