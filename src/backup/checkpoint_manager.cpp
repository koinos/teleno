#include "backup/checkpoint_manager.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include <rocksdb/utilities/checkpoint.h>

namespace koinos::node::backup {
namespace {

std::pair< uint64_t, uint64_t > directory_file_stats( const std::filesystem::path& root )
{
  uint64_t file_count = 0;
  uint64_t total_bytes = 0;
  std::error_code ec;

  if( !std::filesystem::exists( root, ec ) )
    return { file_count, total_bytes };

  for( const auto& entry: std::filesystem::recursive_directory_iterator( root, ec ) )
  {
    if( ec )
      break;
    if( !entry.is_regular_file( ec ) )
      continue;
    ++file_count;
    total_bytes += entry.file_size( ec );
    ec.clear();
  }

  return { file_count, total_bytes };
}

bool directory_is_empty_or_missing( const std::filesystem::path& path )
{
  std::error_code ec;
  if( !std::filesystem::exists( path, ec ) )
    return true;
  return std::filesystem::is_directory( path, ec )
         && std::filesystem::directory_iterator( path, ec ) == std::filesystem::directory_iterator{};
}

void remove_all_best_effort( const std::filesystem::path& path )
{
  std::error_code ec;
  std::filesystem::remove_all( path, ec );
}

} // anonymous namespace

CheckpointManager::CheckpointManager( std::filesystem::path basedir,
                                      storage::RocksDBManager& storage_db )
  : _basedir( std::move( basedir ) ),
    _storage_db( storage_db )
{}

CheckpointResult CheckpointManager::create_checkpoint( const std::filesystem::path& checkpoint_dir ) const
{
  if( !_storage_db.db() )
    throw std::runtime_error( "RocksDB is not open; cannot create backup checkpoint" );
  if( checkpoint_dir.empty() )
    throw std::runtime_error( "checkpoint output directory is required" );
  if( !directory_is_empty_or_missing( checkpoint_dir ) )
    throw std::runtime_error( "checkpoint output directory must be empty or missing: " + checkpoint_dir.string() );

  const auto db_checkpoint_dir = checkpoint_dir / "db";

  try
  {
    std::filesystem::create_directories( checkpoint_dir );

    auto wal_status = _storage_db.db()->FlushWAL( true );
    if( !wal_status.ok() )
      throw std::runtime_error( "failed to flush RocksDB WAL before checkpoint: " + wal_status.ToString() );

    rocksdb::Checkpoint* raw_checkpoint = nullptr;
    auto status = rocksdb::Checkpoint::Create( _storage_db.db(), &raw_checkpoint );
    if( !status.ok() )
      throw std::runtime_error( "failed to create RocksDB checkpoint object: " + status.ToString() );

    std::unique_ptr< rocksdb::Checkpoint > checkpoint( raw_checkpoint );
    status = checkpoint->CreateCheckpoint( db_checkpoint_dir.string() );
    if( !status.ok() )
      throw std::runtime_error( "failed to create RocksDB checkpoint at " + db_checkpoint_dir.string()
                                + ": " + status.ToString() );

    auto [file_count, total_bytes] = directory_file_stats( db_checkpoint_dir );
    if( file_count == 0 )
      throw std::runtime_error( "RocksDB checkpoint contains no files: " + db_checkpoint_dir.string() );

    CheckpointResult result;
    result.checkpoint_dir = checkpoint_dir;
    result.db_dir = db_checkpoint_dir;
    result.file_count = file_count;
    result.total_bytes = total_bytes;
    return result;
  }
  catch( ... )
  {
    remove_all_best_effort( checkpoint_dir );
    throw;
  }
}

} // namespace koinos::node::backup
