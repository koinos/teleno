#pragma once

#include <koinos/chain/execution_context.hpp>

#include <koinos/protocol/protocol.pb.h>

namespace koinos::chain {

void maybe_rectify_state( execution_context&, const protocol::block&, protocol::block_receipt& );

/**
 * Return true only for the exact mainnet parent/root triple whose historically
 * signed previous root differs from the root produced by honest execution.
 */
bool acceptable_rectified_previous_root( const std::string& parent_block_id,
                                         const std::string& computed_parent_root,
                                         const std::string& claimed_previous_root );

} // namespace koinos::chain
