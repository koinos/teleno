#pragma once

#include <koinos/rpc/mempool/mempool_rpc.pb.h>

namespace koinos::node {

/**
 * Abstract interface for the mempool.
 * Replaces AMQP RPC calls to the mempool service.
 */
class IMempool
{
public:
  virtual ~IMempool() = default;

  virtual rpc::mempool::get_pending_transactions_response
  get_pending_transactions( const rpc::mempool::get_pending_transactions_request& ) = 0;

  virtual rpc::mempool::check_pending_account_resources_response
  check_pending_account_resources( const rpc::mempool::check_pending_account_resources_request& ) = 0;

  virtual rpc::mempool::check_account_nonce_response
  check_account_nonce( const rpc::mempool::check_account_nonce_request& ) = 0;

  virtual rpc::mempool::get_pending_nonce_response
  get_pending_nonce( const rpc::mempool::get_pending_nonce_request& ) = 0;

  virtual rpc::mempool::get_pending_transaction_count_response
  get_pending_transaction_count( const rpc::mempool::get_pending_transaction_count_request& ) = 0;

  virtual rpc::mempool::get_reserved_account_rc_response
  get_reserved_account_rc( const rpc::mempool::get_reserved_account_rc_request& ) = 0;
};

} // namespace koinos::node
