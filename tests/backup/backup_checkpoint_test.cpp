#include "backup/checkpoint_manager.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <rocksdb/db.h>
#include <rocksdb/options.h>

using namespace koinos::node;
using namespace koinos::node::backup;

namespace {

std::filesystem::path unique_temp_dir( const std::string& prefix )
{
  auto path = std::filesystem::temp_directory_path()
              / ( prefix + "-" + std::to_string( std::rand() ) );
  std::filesystem::remove_all( path );
  std::filesystem::create_directories( path );
  return path;
}

bool checkpoint_opens_and_contains_metadata( const std::filesystem::path& db_dir )
{
  rocksdb::Options list_options;
  std::vector< std::string > names;
  auto status = rocksdb::DB::ListColumnFamilies( list_options, db_dir.string(), &names );
  if( !status.ok() )
    return false;

  const auto metadata_it = std::find( names.begin(), names.end(), "storage_metadata" );
  if( metadata_it == names.end() )
    return false;
  const auto metadata_index = static_cast< std::size_t >( std::distance( names.begin(), metadata_it ) );

  std::vector< rocksdb::ColumnFamilyDescriptor > descriptors;
  descriptors.reserve( names.size() );
  for( const auto& name: names )
    descriptors.push_back( { name, rocksdb::ColumnFamilyOptions{} } );

  rocksdb::DB* raw_db = nullptr;
  std::vector< rocksdb::ColumnFamilyHandle* > handles;
  rocksdb::DBOptions db_options;
  status = rocksdb::DB::OpenForReadOnly( db_options, db_dir.string(), descriptors, &handles, &raw_db );
  if( !status.ok() )
    return false;

  std::string layout_version;
  status = raw_db->Get( rocksdb::ReadOptions(), handles.at( metadata_index ), "layout.version", &layout_version );

  for( auto* handle: handles )
    delete handle;
  delete raw_db;

  return status.ok() && layout_version == "1";
}

} // namespace

int main()
{
  {
    auto root = unique_temp_dir( "teleno-backup-checkpoint" );
    auto basedir = root / "basedir";
    auto checkpoint_dir = root / "checkpoint";

    NodeConfig cfg;
    cfg.rocksdb_compression = "none";
    cfg.rocksdb_blocks_compression = "none";

    storage::RocksDBManager manager;
    manager.open( basedir, cfg );
    manager.write_metadata( "backup.test", "present" );

    CheckpointManager checkpoint_manager( basedir, manager );
    auto result = checkpoint_manager.create_checkpoint( checkpoint_dir );

    assert( result.checkpoint_dir == checkpoint_dir );
    assert( result.db_dir == checkpoint_dir / "db" );
    assert( result.file_count > 0 );
    assert( result.total_bytes > 0 );
    assert( checkpoint_opens_and_contains_metadata( result.db_dir ) );

    manager.close();
    std::filesystem::remove_all( root );
  }

  {
    auto root = unique_temp_dir( "teleno-backup-checkpoint-nonempty" );
    auto basedir = root / "basedir";
    auto checkpoint_dir = root / "checkpoint";
    std::filesystem::create_directories( checkpoint_dir );
    std::ofstream( checkpoint_dir / "existing" ) << "not empty";

    NodeConfig cfg;
    cfg.rocksdb_compression = "none";
    cfg.rocksdb_blocks_compression = "none";

    storage::RocksDBManager manager;
    manager.open( basedir, cfg );

    bool threw = false;
    try
    {
      CheckpointManager checkpoint_manager( basedir, manager );
      (void)checkpoint_manager.create_checkpoint( checkpoint_dir );
    }
    catch( const std::runtime_error& )
    {
      threw = true;
    }

    assert( threw );
    assert( std::filesystem::exists( checkpoint_dir / "existing" ) );

    manager.close();
    std::filesystem::remove_all( root );
  }

  return 0;
}
