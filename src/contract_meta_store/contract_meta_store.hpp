#pragma once

#include <mutex>
#include <shared_mutex>
#include <string>

#include <rocksdb/db.h>

#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/contract_meta_store/contract_meta_store.pb.h>
#include <koinos/rpc/contract_meta_store/contract_meta_store_rpc.pb.h>

namespace koinos::node::contract_meta_store {

/**
 * C++ port of Go koinos-contract-meta-store.
 * Indexes contract ABI metadata extracted from UploadContract operations.
 * Key: contract_id, Value: contract_meta_item (ABI string).
 */
class ContractMetaStore
{
public:
  ContractMetaStore( rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cf );

  // ── RPC ──
  rpc::contract_meta_store::get_contract_meta_response
  get_contract_meta( const rpc::contract_meta_store::get_contract_meta_request& req );

  // ── EventBus handler ──
  void handle_block_accepted( const broadcast::block_accepted& ba );

private:
  rocksdb::DB* _db;
  rocksdb::ColumnFamilyHandle* _cf;
  mutable std::shared_mutex _mutex;
};

} // namespace koinos::node::contract_meta_store
