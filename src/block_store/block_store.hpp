#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include <rocksdb/db.h>

#include <koinos/block_store/block_store.pb.h>
#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>

#include "interfaces/i_block_store.hpp"

namespace koinos::node::block_store {

/**
 * C++ block store implementation replacing the Go koinos-block-store.
 *
 * Uses a RocksDB column family instead of Badger DB.
 * Implements the same skip-list ancestor system for O(log n) height lookups.
 *
 * Thread safety: concurrent reads via shared_mutex, exclusive writes.
 */
class BlockStore final : public IBlockStore
{
public:
  static constexpr uint32_t max_block_request = 1000;

  /**
   * Open block store backed by a RocksDB column family.
   *
   * @param db          Shared RocksDB instance (owned externally)
   * @param cf_handle   Column family handle for block data
   * @param cf_meta     Column family handle for metadata (highest block)
   */
  BlockStore( rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cf_handle, rocksdb::ColumnFamilyHandle* cf_meta );
  ~BlockStore() override = default;

  // ── IBlockStore interface ──

  rpc::block_store::get_blocks_by_height_response
  get_blocks_by_height( const rpc::block_store::get_blocks_by_height_request& req ) override;

  rpc::block_store::get_blocks_by_id_response
  get_blocks_by_id( const rpc::block_store::get_blocks_by_id_request& req ) override;

  rpc::block_store::get_highest_block_response
  get_highest_block( const rpc::block_store::get_highest_block_request& req ) override;

  rpc::block_store::add_block_response
  add_block( const rpc::block_store::add_block_request& req ) override;

  // ── Broadcast handler ──

  /** Called by EventBus when a block is accepted. */
  void handle_block_accepted( const broadcast::block_accepted& ba );

  // ── Lifecycle ──

  /** Initialize metadata if not present (genesis). */
  void initialize();

private:
  /** Get raw bytes from block CF by key. */
  std::string get_record_bytes( const std::string& block_id ) const;

  /** Put raw bytes into block CF. */
  void put_record_bytes( const std::string& block_id, const std::string& value );

  /** Get metadata (highest block topology). */
  bool get_highest_block_topology( koinos::block_topology& topo ) const;

  /** Update metadata if new height > current. */
  void update_highest_block( const koinos::block_topology& topo );

  /**
   * Walk the skip-list to find the ancestor block ID at a target height.
   * Caller must hold at least a shared lock.
   */
  std::string get_ancestor_id_at_height( const std::string& block_id, uint64_t target_height ) const;

  /**
   * Fill consecutive blocks walking forward from a starting block ID.
   * @param start_id  Block ID to start from
   * @param count     Max blocks to return
   * @param return_block   Include block data
   * @param return_receipt Include receipt data
   */
  std::vector< koinos::block_store::block_item >
  fill_blocks( const std::string& start_id,
               uint32_t count,
               bool return_block,
               bool return_receipt ) const;

  rocksdb::DB* _db;
  rocksdb::ColumnFamilyHandle* _cf_blocks;
  rocksdb::ColumnFamilyHandle* _cf_meta;

  mutable std::shared_mutex _mutex;

  static const std::string META_KEY;
};

} // namespace koinos::node::block_store
