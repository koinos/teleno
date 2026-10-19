#pragma once
#include <cstdint>
#include <cstddef>

namespace koinos::vm_manager {
// Indexed cache capacity, not total heap/RSS or caller-held module memory.
struct module_cache_memory
{
  bool available = false;
  std::size_t entries = 0;
  uint64_t capacity_bytes = 0;
  std::size_t max_entries = 0;
  uint64_t max_capacity_bytes = 0; // Zero leaves the entry limit in effect.
  uint64_t lookup_hits = 0;
  uint64_t lookup_misses = 0;
  uint64_t evictions = 0;
  uint64_t retention_bypasses = 0;
};
} // namespace koinos::vm_manager
