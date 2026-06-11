#pragma once

#include <koinos/rpc/chain/chain_rpc.pb.h>

namespace koinos::node {

/**
 * Abstract interface for the chain engine.
 * Replaces AMQP RPC calls to the chain service.
 */
class IChain
{
public:
  virtual ~IChain() = default;

  virtual rpc::chain::submit_block_response
  submit_block( const rpc::chain::submit_block_request& ) = 0;

  virtual rpc::chain::submit_transaction_response
  submit_transaction( const rpc::chain::submit_transaction_request& ) = 0;

  virtual rpc::chain::get_head_info_response
  get_head_info( const rpc::chain::get_head_info_request& = {} ) = 0;

  virtual rpc::chain::get_chain_id_response
  get_chain_id( const rpc::chain::get_chain_id_request& = {} ) = 0;

  virtual rpc::chain::get_fork_heads_response
  get_fork_heads( const rpc::chain::get_fork_heads_request& = {} ) = 0;

  virtual rpc::chain::read_contract_response
  read_contract( const rpc::chain::read_contract_request& ) = 0;

  virtual rpc::chain::get_account_nonce_response
  get_account_nonce( const rpc::chain::get_account_nonce_request& ) = 0;

  virtual rpc::chain::get_account_rc_response
  get_account_rc( const rpc::chain::get_account_rc_request& ) = 0;

  virtual rpc::chain::get_resource_limits_response
  get_resource_limits( const rpc::chain::get_resource_limits_request& ) = 0;

  virtual rpc::chain::invoke_system_call_response
  invoke_system_call( const rpc::chain::invoke_system_call_request& ) = 0;

  virtual rpc::chain::propose_block_response
  propose_block( const rpc::chain::propose_block_request& ) = 0;
};

} // namespace koinos::node
