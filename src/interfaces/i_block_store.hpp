#pragma once

#include <koinos/rpc/block_store/block_store_rpc.pb.h>

namespace koinos::node {

/**
 * Abstract interface for block storage operations.
 * Replaces AMQP RPC calls to the block_store service.
 */
class IBlockStore
{
public:
  virtual ~IBlockStore() = default;

  virtual rpc::block_store::get_blocks_by_height_response
  get_blocks_by_height( const rpc::block_store::get_blocks_by_height_request& ) = 0;

  virtual rpc::block_store::get_blocks_by_id_response
  get_blocks_by_id( const rpc::block_store::get_blocks_by_id_request& ) = 0;

  virtual rpc::block_store::get_highest_block_response
  get_highest_block( const rpc::block_store::get_highest_block_request& ) = 0;

  virtual rpc::block_store::add_block_response
  add_block( const rpc::block_store::add_block_request& ) = 0;
};

} // namespace koinos::node
