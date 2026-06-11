#pragma once

#include "interfaces/i_mempool.hpp"
#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/crypto/multihash.hpp>
#include <koinos/mempool/mempool.hpp>
#include <koinos/util/conversion.hpp>

#include <chrono>
#include <optional>

namespace koinos::node {

/**
 * Adapter that wraps koinos::mempool::mempool behind the IMempool interface.
 * Translates protobuf RPC request/response types to mempool library calls.
 */
class MempoolAdapter final : public IMempool
{
public:
  explicit MempoolAdapter( koinos::mempool::mempool& impl ) : _impl( impl ) {}

  rpc::mempool::get_pending_transactions_response
  get_pending_transactions( const rpc::mempool::get_pending_transactions_request& req ) override
  {
    rpc::mempool::get_pending_transactions_response resp;

    auto limit = req.limit() > 0 ? req.limit() : koinos::mempool::constants::max_request_limit;
    std::optional< koinos::crypto::multihash > block_id;
    if( !req.block_id().empty() )
      block_id = koinos::util::converter::to< koinos::crypto::multihash >( req.block_id() );

    auto txns  = _impl.get_pending_transactions( limit, block_id );

    for( auto& ptx: txns )
      *resp.add_pending_transactions() = std::move( ptx );

    return resp;
  }

  rpc::mempool::check_pending_account_resources_response
  check_pending_account_resources(
    const rpc::mempool::check_pending_account_resources_request& req ) override
  {
    rpc::mempool::check_pending_account_resources_response resp;

    std::optional< koinos::crypto::multihash > block_id;
    if( !req.block_id().empty() )
      block_id = koinos::util::converter::to< koinos::crypto::multihash >( req.block_id() );

    bool ok = _impl.check_pending_account_resources(
      req.payer(),
      req.max_payer_rc(),
      req.rc_limit(),
      block_id );

    resp.set_success( ok );
    return resp;
  }

  rpc::mempool::check_account_nonce_response
  check_account_nonce( const rpc::mempool::check_account_nonce_request& req ) override
  {
    rpc::mempool::check_account_nonce_response resp;

    std::optional< koinos::crypto::multihash > block_id;
    if( !req.block_id().empty() )
      block_id = koinos::util::converter::to< koinos::crypto::multihash >( req.block_id() );

    resp.set_success( _impl.check_account_nonce( req.payee(), req.nonce(), block_id ) );
    return resp;
  }

  rpc::mempool::get_pending_nonce_response
  get_pending_nonce( const rpc::mempool::get_pending_nonce_request& req ) override
  {
    rpc::mempool::get_pending_nonce_response resp;

    std::optional< koinos::crypto::multihash > block_id;
    if( !req.block_id().empty() )
      block_id = koinos::util::converter::to< koinos::crypto::multihash >( req.block_id() );

    resp.set_nonce( _impl.get_pending_nonce( req.payee(), block_id ) );
    return resp;
  }

  rpc::mempool::get_pending_transaction_count_response
  get_pending_transaction_count( const rpc::mempool::get_pending_transaction_count_request& req ) override
  {
    rpc::mempool::get_pending_transaction_count_response resp;

    std::optional< koinos::crypto::multihash > block_id;
    if( !req.block_id().empty() )
      block_id = koinos::util::converter::to< koinos::crypto::multihash >( req.block_id() );

    resp.set_count( _impl.get_pending_transaction_count( req.payee(), block_id ) );
    return resp;
  }

  rpc::mempool::get_reserved_account_rc_response
  get_reserved_account_rc( const rpc::mempool::get_reserved_account_rc_request& req ) override
  {
    rpc::mempool::get_reserved_account_rc_response resp;
    resp.set_rc( _impl.get_reserved_account_rc( req.account() ) );
    return resp;
  }

  uint64_t add_transaction_accepted(
    const broadcast::transaction_accepted& accepted,
    std::chrono::system_clock::time_point accepted_at = std::chrono::system_clock::now() )
  {
    koinos::mempool::pending_transaction pending;
    *pending.mutable_transaction() = accepted.transaction();
    pending.set_disk_storage_used( accepted.receipt().disk_storage_used() );
    pending.set_network_bandwidth_used( accepted.receipt().network_bandwidth_used() );
    pending.set_compute_bandwidth_used( accepted.receipt().compute_bandwidth_used() );
    pending.set_system_disk_storage_used( accepted.system_disk_storage_used() );
    pending.set_system_network_bandwidth_used( accepted.system_network_bandwidth_used() );
    pending.set_system_compute_bandwidth_used( accepted.system_compute_bandwidth_used() );

    return _impl.add_pending_transaction( pending, accepted_at, accepted.receipt().max_payer_rc() );
  }

  uint64_t prune(
    std::chrono::seconds expiration,
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now() )
  {
    return _impl.prune( expiration, now );
  }

private:
  koinos::mempool::mempool& _impl;
};

} // namespace koinos::node
