#pragma once

#include "monolith_client.hpp"
#include "event_bus.hpp"
#include "interfaces/i_block_store.hpp"
#include "interfaces/i_mempool.hpp"

#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/rpc/mempool/mempool_rpc.pb.h>
#include <koinos/util/services.hpp>

#include <memory>
#include <stdexcept>

namespace koinos::node {

/**
 * Concrete implementation of IRpcClient that routes RPC calls
 * directly to in-process service interfaces and broadcasts via EventBus.
 */
class MonolithRpcClient final : public IRpcClient
{
public:
  MonolithRpcClient( IBlockStore* block_store, IMempool* mempool, EventBus* event_bus )
      : _block_store( block_store ), _mempool( mempool ), _event_bus( event_bus )
  {
  }

  std::shared_future< std::string > rpc( const std::string& service,
                                          const std::string& payload,
                                          std::chrono::milliseconds /* timeout */ = std::chrono::milliseconds( 5000 ),
                                          retry_policy /* rp */ = retry_policy::none ) override
  {
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

private:
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
};

} // namespace koinos::node
