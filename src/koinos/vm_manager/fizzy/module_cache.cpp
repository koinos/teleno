#include <koinos/vm_manager/fizzy/module_cache.hpp>

namespace koinos::vm_manager::fizzy {

namespace {
void increment( uint64_t& value ) noexcept
{
  if( value != std::numeric_limits< uint64_t >::max() ) ++value;
}
} // namespace

module_cache::module_cache( std::size_t size, uint64_t max_capacity_bytes ):
    _cache_size( size ), _cache_capacity( max_capacity_bytes )
{
  _memory.available = true;
  _memory.max_entries = size;
  _memory.max_capacity_bytes = max_capacity_bytes;
}

module_cache::~module_cache() { clear(); }

void module_cache::clear()
{
  std::lock_guard< std::mutex > lock( _mutex );
  _module_map.clear();
  _lru_list.clear();
  _memory.entries = 0;
  _memory.capacity_bytes = 0;
}

module_cache_memory module_cache::memory() const
{
  std::lock_guard< std::mutex > lock( _mutex );
  return _memory;
}

module_ptr module_cache::get_module( const std::string& id )
{
  std::lock_guard< std::mutex > lock( _mutex );

  auto itr = _module_map.find( id );
  if( itr == _module_map.end() )
  {
    increment( _memory.lookup_misses );
    return {};
  }

  increment( _memory.lookup_hits );
  _lru_list.splice( _lru_list.begin(), _lru_list, itr->second.lru );
  return itr->second.module;
}

std::optional< uint64_t > module_cache::charge( const std::string& key, const entry& value ) const noexcept
{
  const auto module_capacity = value.module->capacity_bytes();
  if( !module_capacity ) return {};
  capacity_sum sum;
  sum.add( *module_capacity );
  sum.add( sizeof( module_map_type::value_type ) );
  sum.add( sizeof( lru_list_type::value_type ) );
  sum.add( key.capacity() ); sum.add( 1 );
  sum.add( value.lru->capacity() ); sum.add( 1 );
  return sum.value;
}

void module_cache::retire( module_map_type::iterator position )
{
  _memory.capacity_bytes -= position->second.charge;
  --_memory.entries;
  increment( _memory.evictions );
  _lru_list.erase( position->second.lru );
  _module_map.erase( position );
}

void module_cache::put_module( const std::string& id, module_ptr module )
{
  std::lock_guard< std::mutex > lock( _mutex );

  if( _cache_size == 0 || !module )
    return; // The execution caller still owns its parsed module.

  auto existing = _module_map.find( id );
  if( existing != _module_map.end() )
  {
    entry replacement{ std::move( module ), existing->second.lru };
    const auto bytes = charge( existing->first, replacement );
    if( !bytes || ( _cache_capacity && *bytes > _cache_capacity ) )
    {
      // Never leave a stale same-ID owner after an unretained replacement.
      retire( existing );
      increment( _memory.retention_bypasses );
      return;
    }
    _memory.capacity_bytes -= existing->second.charge;
    replacement.charge = *bytes;
    existing->second = std::move( replacement );
    _lru_list.splice( _lru_list.begin(), _lru_list, existing->second.lru );
    while( _memory.capacity_bytes > ( _cache_capacity ? _cache_capacity : UINT64_MAX ) - *bytes )
      retire( _module_map.find( _lru_list.back() ) );
    _memory.capacity_bytes += *bytes;
    return;
  }

  // Prepare both same-allocator nodes before retiring any old owner. Failed
  // allocation leaves membership, capacity and recency unchanged. Publication
  // transfers prepared nodes and splices strings without allocating.
  lru_list_type staged_lru;
  staged_lru.push_front( id );
  module_map_type staged_map;
  auto staged = staged_map.emplace( id, entry{ std::move( module ), staged_lru.begin() } ).first;
  const auto bytes = charge( staged->first, staged->second );
  if( !bytes || ( _cache_capacity && *bytes > _cache_capacity ) )
  {
    increment( _memory.retention_bypasses );
    return;
  }
  staged->second.charge = *bytes;
  auto node = staged_map.extract( staged );
  while( _memory.entries >= _cache_size
         || _memory.capacity_bytes > ( _cache_capacity ? _cache_capacity : UINT64_MAX ) - *bytes )
    retire( _module_map.find( _lru_list.back() ) );
  _lru_list.splice( _lru_list.begin(), staged_lru );
  _module_map.insert( std::move( node ) );
  ++_memory.entries;
  _memory.capacity_bytes += *bytes;
}

} // namespace koinos::vm_manager::fizzy
