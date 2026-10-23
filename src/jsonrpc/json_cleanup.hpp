#pragma once

#include <array>
#include <cstddef>
#include <iterator>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace koinos::node::jsonrpc {

namespace detail {

inline bool json_container( const nlohmann::json& value ) noexcept
{
  return value.is_array() || value.is_object();
}

// Only primitives and empty containers may reach their ordinary destructor:
// nlohmann 3.11.2 allocates a flattening vector for nonempty containers.
inline void reset_json_leaf( nlohmann::json& value ) noexcept
{
  nlohmann::json empty;
  value.swap( empty );
}

inline void erase_last_json_child( nlohmann::json& value ) noexcept
{
  value.erase( std::prev( value.end() ) );
}

inline void collapse_only_json_child( nlohmann::json& value ) noexcept
{
  nlohmann::json child( std::move( value.back() ) );
  value.clear(); // Its only child was moved to null; no nested cleanup remains.
  value.swap( child ); // child now owns only an empty container.
}

inline bool json_leaf( const nlohmann::json& value ) noexcept
{
  return !json_container( value ) || value.empty();
}

// Consume a leaf sibling before descending again. Swapping values is safe for
// this exclusively retiring tree and avoids array-front erase/element shifts.
// In particular, deep comb-shaped trees become unary and collapse in one pass.
inline bool erase_edge_json_leaf( nlohmann::json& value ) noexcept
{
  if( json_leaf( value.back() ) )
  {
    erase_last_json_child( value );
    return true;
  }
  if( json_leaf( value.front() ) )
  {
    value.front().swap( value.back() );
    erase_last_json_child( value );
    return true;
  }
  return false;
}

// Emergency depth overflow uses rescans rather than heap or recursive stack.
// Unary chains collapse in place. Highly branched deep trees can take extra
// traversal time; the ordinary path below retains its parent pointers.
inline void clear_deep_json_without_allocation( nlohmann::json& root ) noexcept
{
  for( ;; )
  {
    auto* current = &root;
    nlohmann::json* parent = nullptr;
    while( json_container( *current ) && !current->empty() )
    {
      if( current->size() == 1 )
        collapse_only_json_child( *current );
      else if( erase_edge_json_leaf( *current ) )
        continue;
      else
      {
        parent = current;
        current = &current->back();
      }
    }
    reset_json_leaf( *current );
    if( !parent ) return;
    erase_last_json_child( *parent );
  }
}

} // namespace detail

// Empty an exclusively owned JSON tree before its noexcept destructor runs.
// Uses a fixed 512-byte parent stack, no dynamic allocation and no recursive
// C++ calls. Leaves are erased from the back, avoiding array element shifts.
// This changes cleanup only; never use it while another caller borrows the tree.
inline void clear_json_without_allocation( nlohmann::json& root ) noexcept
{
  std::array< nlohmann::json*, 64 > parents{};
  std::size_t depth = 1;
  parents[ 0 ] = &root;
  while( depth )
  {
    auto& current = *parents[ depth - 1 ];
    if( detail::json_container( current ) && !current.empty() )
    {
      if( current.size() == 1 )
      {
        detail::collapse_only_json_child( current );
        continue;
      }
      if( depth < parents.size() )
      {
        parents[ depth++ ] = &current.back();
        continue;
      }
      detail::clear_deep_json_without_allocation( current );
    }
    else
      detail::reset_json_leaf( current );
    --depth;
    if( depth ) detail::erase_last_json_child( *parents[ depth - 1 ] );
  }
}

class JsonCleanup final
{
  nlohmann::json& value;
public:
  explicit JsonCleanup( nlohmann::json& value ) noexcept : value( value ) {}
  JsonCleanup( const JsonCleanup& ) = delete;
  JsonCleanup& operator=( const JsonCleanup& ) = delete;
  ~JsonCleanup() noexcept { clear_json_without_allocation( value ); }
};

// A temporary response must stay owned through serialization failure. Its
// ordinary destructor could otherwise ask the same exhausted quota for scratch.
inline std::string dump_owned_json( nlohmann::json value )
{
  JsonCleanup cleanup( value );
  return value.dump();
}

} // namespace koinos::node::jsonrpc
