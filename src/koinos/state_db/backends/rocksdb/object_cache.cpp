#include <koinos/state_db/backends/rocksdb/object_cache.hpp>

#include <cassert>
#include <limits>
#include <stdexcept>

namespace koinos::state_db::backends::rocksdb {

object_cache::object_cache( std::size_t size ):
    _cache_max_size( size )
{}

object_cache::~object_cache() {}

std::pair< bool, std::shared_ptr< const object_cache::value_type > > object_cache::get( const key_type& k )
{
  auto itr = _object_map.find( k );
  if( itr == _object_map.end() )
    return std::make_pair( false, std::shared_ptr< const object_cache::value_type >() );

  // Move the existing node: a warm hit must not allocate or duplicate its key.
  _lru_list.splice( _lru_list.begin(), _lru_list, itr->second.second );
  auto val = itr->second.first;

  assert( _object_map.size() == _lru_list.size() );

  return std::make_pair( true, val );
}

std::shared_ptr< const object_cache::value_type >
object_cache::put( const key_type& k, std::shared_ptr< const object_cache::value_type > v )
{
  // Min 1 byte for key and 1 byte for value
  const auto value_size = v ? v->size() : 0;
  if( k.size() > std::numeric_limits< std::size_t >::max() - value_size )
    throw std::overflow_error( "object cache payload size overflow" );
  const auto entry_size = std::max( k.size() + value_size, std::size_t( 2 ) );
  remove( k );

  // Readers/iterators now carry the returned immutable owner to consumption.
  // Oversized entries can bypass retention safely, without clearing unrelated
  // warm entries or exporting a raw pointer to a destroyed temporary.
  if( entry_size > _cache_max_size )
    return v;
  while( _cache_size > _cache_max_size - entry_size )
    remove( _lru_list.back() );

  _lru_list.push_front( k );
  try
  {
    _object_map.emplace( k, std::make_pair( v, _lru_list.begin() ) );
  }
  catch( ... )
  {
    _lru_list.pop_front();
    throw;
  }
  _cache_size      += entry_size;

  assert( _object_map.size() == _lru_list.size() );

  return v;
}

void object_cache::remove( const key_type& k )
{
  auto itr = _object_map.find( k );
  if( itr != _object_map.end() )
  {
    _cache_size -= std::max( k.size() + ( itr->second.first ? itr->second.first->size() : 0 ), std::size_t( 2 ) );
    _lru_list.erase( itr->second.second );
    _object_map.erase( itr );
  }

  assert( _object_map.size() == _lru_list.size() );
}

void object_cache::clear()
{
  _object_map.clear();
  _lru_list.clear();
  _cache_size = 0;
}

std::mutex& object_cache::get_mutex()
{
  return _mutex;
}

} // namespace koinos::state_db::backends::rocksdb
