#include <koinos/state_db/backends/map/map_iterator.hpp>

#include <koinos/state_db/backends/exceptions.hpp>

namespace koinos::state_db::backends::map {

map_iterator::map_iterator( std::unique_ptr< iterator_impl > itr,
                            const map_impl& map ):
    _itr( std::move( itr ) ),
    _map( map )
{}

map_iterator::~map_iterator() {}

const map_iterator::value_type& map_iterator::operator*() const
{
  KOINOS_ASSERT( valid(), iterator_exception, "iterator operation is invalid" );
  return *( *_itr )->second;
}

map_iterator::value_ptr map_iterator::share_value() const
{
  KOINOS_ASSERT( valid(), iterator_exception, "iterator operation is invalid" );
  return ( *_itr )->second;
}

const map_iterator::key_type& map_iterator::key() const
{
  KOINOS_ASSERT( valid(), iterator_exception, "iterator operation is invalid" );
  return ( *_itr )->first;
}

abstract_iterator& map_iterator::operator++()
{
  KOINOS_ASSERT( valid(), iterator_exception, "iterator operation is invalid" );
  ++( *_itr );
  return *this;
}

abstract_iterator& map_iterator::operator--()
{
  KOINOS_ASSERT( *_itr != _map.begin(), iterator_exception, "iterator operation is invalid" );
  --( *_itr );
  return *this;
}

bool map_iterator::valid() const
{
  return _itr && *_itr != _map.end();
}

std::unique_ptr< abstract_iterator > map_iterator::copy() const
{
  return std::make_unique< map_iterator >(
    std::make_unique< iterator_impl >( *_itr ),
    _map );
}

} // namespace koinos::state_db::backends::map
