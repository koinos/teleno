#pragma once

#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include <rocksdb/db.h>

#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/rpc/account_history/account_history_rpc.pb.h>

namespace koinos::node::account_history {

/**
 * C++ account history service.
 * Indexes blockchain events by account address for efficient history queries.
 *
 * Storage schema (RocksDB column family "account_history"):
 *   Key: address + sequence_number (8 bytes BE)
 *   Value: account_history_record protobuf
 *
 * Metadata key per address:
 *   Key: "meta:" + address
 *   Value: uint64 (next sequence number)
 *
 * Subscribes to on_block_accepted to extract impacted addresses from:
 *   - Block signer
 *   - Transaction payer/payee
 *   - Event sources and impacted addresses
 */
class AccountHistory
{
public:
  static constexpr uint32_t max_records_per_query = 500;

  AccountHistory( rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cf );

  // ── RPC ──
  rpc::account_history::get_account_history_response
  get_account_history( const rpc::account_history::get_account_history_request& req );

  // ── EventBus handler ──
  void handle_block_accepted( const broadcast::block_accepted& ba );

private:
  /** Get or create the next sequence number for an address. */
  uint64_t next_sequence( const std::string& address );

  /** Store a history record for an address. */
  void store_record( const std::string& address,
                     uint64_t seq,
                     const std::string& trx_id,
                     const std::string& block_id,
                     uint64_t block_height );

  /** Encode a sequence number as 8-byte big-endian for lexicographic ordering. */
  static std::string encode_seq( uint64_t seq );

  /** Decode a sequence number from 8-byte big-endian. */
  static uint64_t decode_seq( const std::string& s );

  rocksdb::DB* _db;
  rocksdb::ColumnFamilyHandle* _cf;
  mutable std::shared_mutex _mutex;
};

} // namespace koinos::node::account_history
