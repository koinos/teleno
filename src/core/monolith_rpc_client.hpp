#pragma once

#include "monolith_client.hpp"
#include "event_bus.hpp"
#include "interfaces/i_block_store.hpp"
#include "interfaces/i_mempool.hpp"

#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/rpc/mempool/mempool_rpc.pb.h>
#include <koinos/util/services.hpp>
#include <koinos/log.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace koinos::node {

/**
 * Concrete implementation of IRpcClient that routes RPC calls
 * directly to in-process service interfaces and broadcasts via EventBus.
 */
class MonolithRpcClient final : public IRpcClient
{
public:
  // Native admission must finish before notifying accepted-work observers.
  // No new protobuf/RPC surface is introduced; disabled mempool keeps the
  // legacy notification-only behavior through an empty callback.
  using TransactionAdmission = std::function< void( const ::koinos::broadcast::transaction_accepted& ) >;
  MonolithRpcClient( IBlockStore* block_store, IMempool* mempool, EventBus* event_bus,
                     TransactionAdmission transaction_admission = {},
                     TransactionWorkLimits work_limits = {}, OwnedMemoryBudget transaction_memory = {} )
      : _block_store( block_store ), _mempool( mempool ), _event_bus( event_bus ),
        _transaction_admission( std::move( transaction_admission ) ), _transaction_work( work_limits, transaction_memory ),
        _transaction_memory( std::move( transaction_memory ) )
  {
  }

  std::unique_ptr< TransactionWorkReservation >
  reserve_transaction_work( const protocol::transaction& transaction ) override
  {
    if( !_transaction_memory.enabled() )
    {
      if( !_transaction_work.enabled() ) return {};
      return _transaction_work.reserve( transaction.ByteSizeLong() );
    }
    OwnedMemoryBudget::Scope scope( _transaction_memory, false );
    // Supplementary workspace for native protobuf copies/serialization, not
    // a claim about arbitrary VM, receipt, ingress or output allocation. The
    // controller's anonymous map allocations are charged individually below.
    auto workspace = _transaction_memory.reserve( OwnedMemoryBudget::native_message_workspace(
      OwnedMemoryBudget::protobuf_space_used( transaction ) ) );
    auto job = _transaction_work.reserve( transaction.ByteSizeLong() );
    return std::make_unique< AggregateWork >( _transaction_memory, std::move( workspace ), std::move( job ) );
  }

  TransactionWorkSnapshot transaction_work_snapshot() const { return _transaction_work.snapshot(); }

  uint64_t transaction_notification_failures() const noexcept
  {
    return _transaction_notification_failures.load();
  }

  std::shared_future< std::string > rpc( const std::string& service,
                                          const std::string& payload,
                                          std::chrono::milliseconds /* timeout */ = std::chrono::milliseconds( 5000 ),
                                          retry_policy /* rp */ = retry_policy::none ) override
  {
    OwnedMemoryBudget::Scope response_memory_scope(
      service == util::service::mempool ? _transaction_memory : OwnedMemoryBudget::current() );
    auto promise = std::make_shared< std::promise< std::string > >();
    try
    {
      std::string response_bytes;
      if( service == util::service::block_store && _block_store )
        response_bytes = handle_block_store_rpc( payload );
      else if( service == util::service::mempool && _mempool )
        response_bytes = handle_mempool_rpc( payload );
      else
      {
        promise->set_exception(
          std::make_exception_ptr( std::runtime_error( "MonolithRpcClient: unknown service " + service ) ) );
        return promise->get_future().share();
      }
      promise->set_value( std::move( response_bytes ) );
    }
    catch( ... )
    {
      promise->set_exception( std::current_exception() );
    }
    return promise->get_future().share();
  }

  void broadcast( const std::string& topic, const std::string& payload ) override
  {
    if( topic == "koinos.transaction.accept" && _transaction_admission )
    {
      OwnedMemoryBudget::Scope memory_scope( _transaction_memory, false );
      auto parse_workspace = _transaction_memory.enabled() ? _transaction_memory.reserve(
        OwnedMemoryBudget::checked_add( OwnedMemoryBudget::checked_multiply( payload.size(), 128 ), 65536 ) )
        : OwnedMemoryBudget::Lease{};
      ::koinos::broadcast::transaction_accepted ta;
      if( !ta.ParseFromString( payload ) || !ta.has_transaction() || !ta.has_receipt() )
        throw std::invalid_argument( "Malformed native transaction admission message" );

      // Admission failures propagate to submit_transaction, before any
      // accepted-work notification. Never acknowledge a swallowed refusal.
      _transaction_admission( ta );
      if( _event_bus )
      {
        try { _event_bus->on_transaction_accepted( ta ); }
        catch( const std::exception& e )
        {
          ++_transaction_notification_failures;
          try { LOG( warning ) << "[mempool] Transaction retained, but accepted-work notification failed: " << e.what(); }
          catch( ... ) {}
        }
        catch( ... )
        {
          ++_transaction_notification_failures;
          try { LOG( warning ) << "[mempool] Transaction retained, but accepted-work notification failed: unknown exception"; }
          catch( ... ) {}
        }
      }
      return;
    }
    if( !_event_bus )
      return;

    if( topic == "koinos.block.accept" )
    {
      ::koinos::broadcast::block_accepted ba;
      if( ba.ParseFromString( payload ) )
        _event_bus->on_block_accepted( ba );
    }
    else if( topic == "koinos.block.irreversible" )
    {
      ::koinos::broadcast::block_irreversible bi;
      if( bi.ParseFromString( payload ) )
        _event_bus->on_block_irreversible( bi );
    }
    else if( topic == "koinos.transaction.accept" )
    {
      ::koinos::broadcast::transaction_accepted ta;
      if( ta.ParseFromString( payload ) )
        _event_bus->on_transaction_accepted( ta );
    }
    else if( topic == "koinos.transaction.fail" )
    {
      ::koinos::broadcast::transaction_failed tf;
      if( tf.ParseFromString( payload ) )
        _event_bus->on_transaction_failed( tf );
    }
    else if( topic == "koinos.block.forks" )
    {
      ::koinos::broadcast::fork_heads fh;
      if( fh.ParseFromString( payload ) )
        _event_bus->on_fork_heads( fh );
    }
  }

  void broadcast_block_accepted( const ::koinos::broadcast::block_accepted& event ) override
  {
    if( _event_bus ) _event_bus->on_block_accepted( event );
  }

private:
  struct AggregateWork final : TransactionWorkReservation
  {
    AggregateWork( OwnedMemoryBudget memory, OwnedMemoryBudget::Lease workspace,
                   std::unique_ptr< TransactionWorkReservation > job )
      : memory( std::move( memory ) ), workspace( std::move( workspace ) ), job( std::move( job ) ) {}
    OwnedMemoryBudget memory;
    OwnedMemoryBudget::Lease workspace;
    std::unique_ptr< TransactionWorkReservation > job;
    OwnedMemoryBudget memory_budget() const noexcept override { return memory; }
  };
  std::string handle_block_store_rpc( const std::string& payload )
  {
    rpc::block_store::block_store_request req;
    if( !req.ParseFromString( payload ) )
      throw std::runtime_error( "Failed to parse block_store request" );

    rpc::block_store::block_store_response resp;
    if( req.has_get_blocks_by_height() )
      *resp.mutable_get_blocks_by_height() = _block_store->get_blocks_by_height( req.get_blocks_by_height() );
    else if( req.has_get_blocks_by_id() )
      *resp.mutable_get_blocks_by_id() = _block_store->get_blocks_by_id( req.get_blocks_by_id() );
    else if( req.has_get_highest_block() )
      *resp.mutable_get_highest_block() = _block_store->get_highest_block( req.get_highest_block() );
    else if( req.has_add_block() )
      *resp.mutable_add_block() = _block_store->add_block( req.add_block() );

    return resp.SerializeAsString();
  }

  std::string handle_mempool_rpc( const std::string& payload )
  {
    rpc::mempool::mempool_request req;
    if( !req.ParseFromString( payload ) )
      throw std::runtime_error( "Failed to parse mempool request" );

    rpc::mempool::mempool_response resp;
    if( req.has_get_pending_transactions() )
      *resp.mutable_get_pending_transactions() = _mempool->get_pending_transactions( req.get_pending_transactions() );
    else if( req.has_check_pending_account_resources() )
      *resp.mutable_check_pending_account_resources() =
        _mempool->check_pending_account_resources( req.check_pending_account_resources() );
    else if( req.has_check_account_nonce() )
      *resp.mutable_check_account_nonce() = _mempool->check_account_nonce( req.check_account_nonce() );
    else if( req.has_get_pending_nonce() )
      *resp.mutable_get_pending_nonce() = _mempool->get_pending_nonce( req.get_pending_nonce() );
    else if( req.has_get_pending_transaction_count() )
      *resp.mutable_get_pending_transaction_count() =
        _mempool->get_pending_transaction_count( req.get_pending_transaction_count() );
    else if( req.has_get_reserved_account_rc() )
      *resp.mutable_get_reserved_account_rc() = _mempool->get_reserved_account_rc( req.get_reserved_account_rc() );

    return resp.SerializeAsString();
  }

  IBlockStore* _block_store;
  IMempool* _mempool;
  EventBus* _event_bus;
  TransactionAdmission _transaction_admission;
  TransactionWorkBudget _transaction_work;
  OwnedMemoryBudget _transaction_memory;
  std::atomic< uint64_t > _transaction_notification_failures{ 0 };
};

} // namespace koinos::node
