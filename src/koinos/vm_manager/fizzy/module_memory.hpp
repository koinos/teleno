#pragma once
#include <fizzy/fizzy.h>
#include <cstdint>
#include <limits>
#include <optional>

namespace koinos::vm_manager::fizzy {
class capacity_sum
{
public:
  void add( uint64_t count, uint64_t element_bytes = 1 ) noexcept
  {
    const auto maximum = std::numeric_limits< uint64_t >::max();
    if( !value || ( element_bytes && count > ( maximum - *value ) / element_bytes ) )
      value.reset();
    else
      *value += count * element_bytes;
  }
  std::optional< uint64_t > value = 0;
};
// Pinned immutable Module object, vector capacities and nested string capacities
// (including terminators; inline strings conservatively charged). Excludes
// allocator overhead, shared control blocks and mutable instances.
std::optional< uint64_t > parsed_module_capacity_bytes( const FizzyModule* module ) noexcept;
} // namespace koinos::vm_manager::fizzy
