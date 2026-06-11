#include "transaction_store.hpp"

#include <koinos/log.hpp>

#include <stdexcept>

namespace koinos::node::transaction_store {

TransactionStore::TransactionStore( rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cf )
    : _db( db ), _cf( cf )
{
}

rpc::transaction_store::get_transactions_by_id_response
TransactionStore::get_transactions_by_id( const rpc::transaction_store::get_transactions_by_id_request& req )
{
  std::shared_lock lock( _mutex );

  rpc::transaction_store::get_transactions_by_id_response resp;

  if( req.transaction_ids_size() == 0 )
    throw std::runtime_error( "expected field transaction_ids was nil" );

  for( const auto& tx_id: req.transaction_ids() )
  {
    if( tx_id.empty() )
      continue;

    std::string value;
    auto s = _db->Get( rocksdb::ReadOptions(), _cf, tx_id, &value );
    if( !s.ok() || value.empty() )
      continue;

    ::koinos::transaction_store::transaction_item item;
    if( item.ParseFromString( value ) )
      *resp.add_transactions() = item;
  }

  return resp;
}

void TransactionStore::handle_block_accepted( const broadcast::block_accepted& ba )
{
  if( !ba.has_block() )
    return;

  const auto& block = ba.block();

  for( const auto& tx: block.transactions() )
  {
    if( tx.id().empty() )
      continue;

    try
    {
      add_included_transaction( tx, block.id() );
    }
    catch( const std::exception& e )
    {
      LOG( warning ) << "[transaction_store] Failed to index tx: " << e.what();
    }
  }

  if( ba.live() )
  {
    LOG( debug ) << "[transaction_store] Indexed " << block.transactions_size()
                 << " txs from block height=" << block.header().height();
  }
  else if( block.header().height() % 1000 == 0 )
  {
    LOG( info ) << "[transaction_store] Sync progress height=" << block.header().height();
  }
}

void TransactionStore::add_included_transaction( const protocol::transaction& tx,
                                                  const std::string& block_id )
{
  std::unique_lock lock( _mutex );

  std::string existing_value;
  auto s = _db->Get( rocksdb::ReadOptions(), _cf, tx.id(), &existing_value );

  if( s.ok() && !existing_value.empty() )
  {
    // Existing transaction — append block ID if not already present
    ::koinos::transaction_store::transaction_item item;
    if( !item.ParseFromString( existing_value ) )
      return;

    // Duplicate check
    for( const auto& bid: item.containing_blocks() )
    {
      if( bid == block_id )
        return; // Already indexed for this block
    }

    item.add_containing_blocks( block_id );

    std::string value;
    if( !item.SerializeToString( &value ) )
      return;

    _db->Put( rocksdb::WriteOptions(), _cf, tx.id(), value );
  }
  else
  {
    // New transaction
    ::koinos::transaction_store::transaction_item item;
    *item.mutable_transaction() = tx;
    item.add_containing_blocks( block_id );

    std::string value;
    if( !item.SerializeToString( &value ) )
      return;

    _db->Put( rocksdb::WriteOptions(), _cf, tx.id(), value );
  }
}

} // namespace koinos::node::transaction_store
