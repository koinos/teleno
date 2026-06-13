#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "storage/rocksdb_manager.hpp"

namespace koinos::node::storage {

struct MigratedColumnFamily
{
  std::string name;
  uint64_t record_count = 0;
  uint64_t byte_count = 0;
  std::string source_hash;
  std::string target_hash;
};

struct ChainMigrationResult
{
  std::filesystem::path source_path;
  std::filesystem::path backup_path;
  MigratedColumnFamily objects;
  MigratedColumnFamily metadata;
};

struct ChainMigrationRollbackResult
{
  std::filesystem::path restored_path;
  std::filesystem::path backup_path;
};

ChainMigrationResult migrate_legacy_chain_db_to_unified( const std::filesystem::path& basedir,
                                                         RocksDBManager& storage_db );

ChainMigrationRollbackResult rollback_unified_chain_db_migration( const std::filesystem::path& basedir,
                                                                  RocksDBManager& storage_db );

} // namespace koinos::node::storage
