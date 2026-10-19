#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <koinos/chain/chain.pb.h>
#include <koinos/crypto/multihash.hpp>
#include <koinos/exception.hpp>

namespace koinos::state_db {

using state_node_id = crypto::multihash;
using object_space  = chain::object_space;
using object_key    = std::string;
using object_value  = std::string;
using object_value_ptr = std::shared_ptr<const object_value>;

// On-demand inventory of indexed state deltas, not process/allocator memory.
// String capacities include inline storage and exclude terminators; do not add
// them to object sizes or present their sum as heap bytes. Metadata, allocator
// overhead, non-map backends, anonymous overlays and caller-only owners are not
// measured. Immutable values shared by map clones are counted only once.
struct retained_memory_usage
{
  uint64_t state_nodes = 0;
  uint64_t finalized_state_nodes = 0;
  uint64_t writable_state_nodes = 0;
  uint64_t map_backends = 0;
  uint64_t unmeasured_backends = 0;
  uint64_t map_entries = 0;
  uint64_t map_key_size_bytes = 0;
  uint64_t map_key_capacity_bytes = 0;
  uint64_t value_references = 0;
  uint64_t unique_values = 0;
  uint64_t unique_value_size_bytes = 0;
  uint64_t unique_value_capacity_bytes = 0;
  uint64_t tombstone_entries = 0;
  uint64_t tombstone_key_size_bytes = 0;
  uint64_t tombstone_key_capacity_bytes = 0;
  uint64_t tombstone_bucket_count = 0;
};

KOINOS_DECLARE_DERIVED_EXCEPTION( state_db_exception, chain::reversion_exception );

KOINOS_DECLARE_DERIVED_EXCEPTION( database_not_open, state_db_exception );

/**
 * An attempt was made to modify a finalized node.
 */
KOINOS_DECLARE_DERIVED_EXCEPTION( node_finalized, state_db_exception );

/**
 * An argument is out of range or otherwise invalid.
 *
 * If IllegalArgument is thrown, it likely indicates a programming error
 * in the caller.
 */
KOINOS_DECLARE_DERIVED_EXCEPTION( illegal_argument, state_db_exception );

/**
 * The given NodeId cannot be discarded.
 *
 * Due to limitations of chainbase, the only sessions that can be discarded are:
 * - The oldest session.
 * - The session before the newest session.
 *
 * Furthermore the last node cannot be discarded.
 */
KOINOS_DECLARE_DERIVED_EXCEPTION( cannot_discard, state_db_exception );

/**
 * An internal invariant has been violated.
 *
 * This is most likely caused by a programming error in state_db.
 */
KOINOS_DECLARE_DERIVED_EXCEPTION( internal_error, state_db_exception );

} // namespace koinos::state_db
