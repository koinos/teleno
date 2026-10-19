#include <koinos/state_db/backends/map/map_backend.hpp>
#include <koinos/state_db/retained_memory.hpp>
#include <stdexcept>

namespace koinos::state_db::backends::map {

map_backend::map_backend() {}

map_backend::~map_backend() {}

iterator map_backend::begin()
{
  return iterator(
    std::make_unique< map_iterator >( std::make_unique< map_iterator::iterator_impl >( _map.begin() ), _map ) );
}

iterator map_backend::end()
{
  return iterator(
    std::make_unique< map_iterator >( std::make_unique< map_iterator::iterator_impl >( _map.end() ), _map ) );
}

void map_backend::put( const key_type& k, const value_type& v )
{
  put_owned( k, std::make_shared<const value_type>(v) );
}

void map_backend::put_owned( const key_type& k, value_ptr v )
{
  if( !v ) throw std::invalid_argument( "owned state value is null" );
  _map.insert_or_assign( k, std::move( v ) );
}

map_backend::value_ptr map_backend::get( const key_type& key ) const
{
  auto itr = _map.find( key );
  if( itr == _map.end() )
  {
    return nullptr;
  }

  return itr->second;
}

void map_backend::erase( const key_type& k )
{
  _map.erase( k );
}

void map_backend::clear() noexcept
{
  _map.clear();
}

map_backend::size_type map_backend::size() const noexcept
{
  return _map.size();
}

iterator map_backend::find( const key_type& k )
{
  return iterator(
    std::make_unique< map_iterator >( std::make_unique< map_iterator::iterator_impl >( _map.find( k ) ), _map ) );
}

iterator map_backend::lower_bound( const key_type& k )
{
  return iterator(
    std::make_unique< map_iterator >( std::make_unique< map_iterator::iterator_impl >( _map.lower_bound( k ) ),
                                      _map ) );
}

void map_backend::start_write_batch() {}

void map_backend::end_write_batch( write_durability ) {}

void map_backend::store_metadata() {}

std::shared_ptr< abstract_backend > map_backend::clone() const
{
  return std::make_shared< map_backend >( *this );
}

void map_backend::merge_writes_from( map_backend& source, std::unordered_set< key_type >& removed ) noexcept
{
  // No virtual iterator wrappers, new map nodes, string copies or control
  // blocks here. Both maps have the same allocator; string comparison/hash,
  // shared_ptr moves and erasure do not allocate or invoke user callbacks.
  while( !source._map.empty() )
  {
    auto entry = source._map.extract( source._map.begin() );
    removed.erase( entry.key() );
    auto current = _map.find( entry.key() );
    if( current != _map.end() )
      current->second = std::move( entry.mapped() );
    else
      _map.insert( std::move( entry ) );
  }
}

void map_backend::inventory_retained_memory( ::koinos::state_db::detail::retained_memory_accumulator& inventory ) const
{
  for( const auto& [ key, value ]: _map )
    inventory.map_entry( key, value );
}

} // namespace koinos::state_db::backends::map
