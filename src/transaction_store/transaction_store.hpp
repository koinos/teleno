#pragma once

#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include <rocksdb/db.h>

#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/rpc/transaction_store/transaction_store_rpc.pb.h>
#include <koinos/transaction_store/transaction_store.pb.h>

namespace koinos::node::transaction_store {

/**
 * C++ port of Go koinos-transaction-store.
 * Indexes transactions by ID, tracking which blocks contain each transaction.
 * Key: transaction_id, Value: transaction_item (tx + containing_blocks[]).
 */
class TransactionStore
{
public:
  TransactionStore( rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cf );

  // ── RPC ──
  rpc::transaction_store::get_transactions_by_id_response
  get_transactions_by_id( const rpc::transaction_store::get_transactions_by_id_request& req );

  // ── EventBus handler ──
  void handle_block_accepted( const broadcast::block_accepted& ba );

private:
  void add_included_transaction( const protocol::transaction& tx, const std::string& block_id );

  rocksdb::DB* _db;
  rocksdb::ColumnFamilyHandle* _cf;
  mutable std::shared_mutex _mutex;
};

} // namespace koinos::node::transaction_store
