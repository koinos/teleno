#pragma once

#include <cstdint>
#include <filesystem>

#include "storage/rocksdb_manager.hpp"

namespace koinos::node::backup {

struct CheckpointResult
{
  std::filesystem::path checkpoint_dir;
  std::filesystem::path db_dir;
  uint64_t file_count = 0;
  uint64_t total_bytes = 0;
};

class CheckpointManager
{
public:
  CheckpointManager( std::filesystem::path basedir,
                     storage::RocksDBManager& storage_db );

  CheckpointResult create_checkpoint( const std::filesystem::path& checkpoint_dir ) const;

private:
  std::filesystem::path _basedir;
  storage::RocksDBManager& _storage_db;
};

} // namespace koinos::node::backup
