#pragma once

#include <bit>
#include <cstdint>
#include <vector>

namespace koinos::node::block_store {

/**
 * Deterministic power-of-2 skip-list for O(log n) ancestor lookups.
 *
 * Block at height h stores pointers to ancestors at heights:
 *   h-1, h-2, h-4, h-8, ... h-2^trailing_zeros(h)
 *
 * Average ~2 pointers per block. This is the same algorithm as
 * the Go block_store (koinos-block-store/internal/bstore/reqhandler.go).
 */

/**
 * Compute the ancestor heights that a block at the given height should store.
 *
 * @param height Block height (0 returns empty)
 * @return Vector of ancestor heights in order [h-1, h-2, h-4, ...]
 */
inline std::vector< uint64_t > get_previous_heights( uint64_t height )
{
  if( height == 0 )
    return {};

  int zeros = std::countr_zero( height );
  std::vector< uint64_t > result;
  result.reserve( zeros + 1 );

  for( int i = 0; i <= zeros; ++i )
    result.push_back( height - ( uint64_t( 1 ) << i ) );

  return result;
}

/**
 * Find the best skip-list index to jump toward a goal height.
 *
 * @param goal    Target height to reach
 * @param current Height of the current block
 * @return {index, height_at_index} — the skip-list entry to follow
 * @throws std::invalid_argument if goal >= current
 */
struct skip_index_result
{
  int index;
  uint64_t height;
};

inline skip_index_result get_previous_height_index( uint64_t goal, uint64_t current )
{
  if( goal >= current )
    throw std::invalid_argument( "goal height must be less than current height" );

  int zeros    = std::countr_zero( current );
  int last_idx = 0;
  uint64_t last_h = 0;

  for( int i = 0; i <= zeros; ++i )
  {
    uint64_t h = current - ( uint64_t( 1 ) << i );
    if( h < goal )
      return { i - 1, last_h };
    last_idx = i;
    last_h   = h;
  }

  return { last_idx, last_h };
}

} // namespace koinos::node::block_store
