#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <rocksdb/db.h>

#include "core/config.hpp"

namespace koinos::node::storage {

enum class ColumnFamily
{
  default_state = 0,
  blocks,
  block_meta,
  contract_meta,
  transaction_index,
  account_history,
  chain_state,
  chain_metadata,
  storage_metadata
};

struct ColumnFamilyStats
{
  std::string name;
  uint64_t estimated_keys = 0;
  uint64_t total_sst_file_size = 0;
  uint64_t estimated_live_data_size = 0;
};

struct CompressionStatus
{
  std::string requested_default;
  std::string selected_default;
  std::string requested_blocks;
  std::string selected_blocks;
  std::string fallback_note;
  std::vector< std::string > supported_compressions;
};

class RocksDBManager
{
public:
  RocksDBManager() = default;
  ~RocksDBManager();

  RocksDBManager( const RocksDBManager& ) = delete;
  RocksDBManager& operator=( const RocksDBManager& ) = delete;
  RocksDBManager( RocksDBManager&& ) = delete;
  RocksDBManager& operator=( RocksDBManager&& ) = delete;

  void open( const std::filesystem::path& basedir, const NodeConfig& cfg );
  void close();

  rocksdb::DB* db() const;
  rocksdb::ColumnFamilyHandle* handle( ColumnFamily cf ) const;
  const std::filesystem::path& path() const;
  std::size_t column_family_count() const;
  std::vector< ColumnFamilyStats > column_family_stats() const;
  const CompressionStatus& compression_status() const;

  std::string read_metadata( const std::string& key ) const;
  void write_metadata( const std::string& key, const std::string& value );
  void compact_column_family( ColumnFamily cf );
  void compact_all_column_families();

private:
  void initialize_storage_metadata( const std::filesystem::path& basedir );

  std::filesystem::path _path;
  std::unique_ptr< rocksdb::DB > _db;
  std::vector< rocksdb::ColumnFamilyHandle* > _handles;
  CompressionStatus _compression_status;
};

const char* column_family_name( ColumnFamily cf );
ColumnFamily column_family_from_name( const std::string& name );
std::string compression_name( rocksdb::CompressionType compression );

} // namespace koinos::node::storage
