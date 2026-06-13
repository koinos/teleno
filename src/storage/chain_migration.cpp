#include "storage/chain_migration.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <openssl/evp.h>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/write_batch.h>

namespace koinos::node::storage {
namespace {

constexpr uint64_t max_batch_records = 10'000;
constexpr uint64_t max_batch_bytes   = 64 * 1024 * 1024;
constexpr std::size_t sha256_digest_length = 32;

class Sha256Hasher
{
public:
  Sha256Hasher()
  {
    _ctx = EVP_MD_CTX_new();
    if( !_ctx || EVP_DigestInit_ex( _ctx, EVP_sha256(), nullptr ) != 1 )
      throw std::runtime_error( "failed to initialize SHA-256 context" );
  }

  Sha256Hasher( const Sha256Hasher& ) = delete;
  Sha256Hasher& operator=( const Sha256Hasher& ) = delete;

  ~Sha256Hasher()
  {
    if( _ctx )
      EVP_MD_CTX_free( _ctx );
  }

  void update( const void* data, std::size_t size )
  {
    if( size == 0 )
      return;
    if( EVP_DigestUpdate( _ctx, data, size ) != 1 )
      throw std::runtime_error( "failed to update SHA-256 context" );
  }

  std::array< unsigned char, sha256_digest_length > final()
  {
    std::array< unsigned char, sha256_digest_length > digest{};
    unsigned int digest_size = 0;
    if( EVP_DigestFinal_ex( _ctx, digest.data(), &digest_size ) != 1 || digest_size != digest.size() )
      throw std::runtime_error( "failed to finalize SHA-256 context" );
    return digest;
  }

private:
  EVP_MD_CTX* _ctx = nullptr;
};

struct OpenedLegacyDB
{
  OpenedLegacyDB() = default;
  OpenedLegacyDB( const OpenedLegacyDB& ) = delete;
  OpenedLegacyDB& operator=( const OpenedLegacyDB& ) = delete;
  OpenedLegacyDB( OpenedLegacyDB&& ) noexcept = default;
  OpenedLegacyDB& operator=( OpenedLegacyDB&& ) noexcept = default;

  ~OpenedLegacyDB()
  {
    for( auto* handle: handles )
      delete handle;
    handles.clear();
    db.reset();
  }

  std::unique_ptr< rocksdb::DB > db;
  std::vector< rocksdb::ColumnFamilyHandle* > handles;
};

std::string timestamp_suffix()
{
  const auto now  = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t( now );
  std::tm tm{};
#if defined( _WIN32 )
  gmtime_s( &tm, &time );
#else
  gmtime_r( &time, &tm );
#endif
  std::ostringstream out;
  out << std::put_time( &tm, "%Y%m%dT%H%M%SZ" );
  return out.str();
}

void update_uint64( Sha256Hasher& hasher, uint64_t value )
{
  std::array< unsigned char, 8 > bytes{};
  for( std::size_t i = 0; i < bytes.size(); ++i )
    bytes[ i ] = static_cast< unsigned char >( ( value >> ( i * 8 ) ) & 0xff );
  hasher.update( bytes.data(), bytes.size() );
}

std::string hex_digest( const std::array< unsigned char, sha256_digest_length >& digest )
{
  std::ostringstream out;
  out << std::hex << std::setfill( '0' );
  for( auto byte: digest )
    out << std::setw( 2 ) << static_cast< unsigned int >( byte );
  return out.str();
}

MigratedColumnFamily hash_column_family( rocksdb::DB& db,
                                         rocksdb::ColumnFamilyHandle& handle,
                                         const std::string& name )
{
  MigratedColumnFamily result;
  result.name = name;

  Sha256Hasher hasher;

  rocksdb::ReadOptions read_options;
  std::unique_ptr< rocksdb::Iterator > it( db.NewIterator( read_options, &handle ) );
  for( it->SeekToFirst(); it->Valid(); it->Next() )
  {
    const auto key = it->key();
    const auto value = it->value();
    update_uint64( hasher, key.size() );
    hasher.update( key.data(), key.size() );
    update_uint64( hasher, value.size() );
    hasher.update( value.data(), value.size() );
    ++result.record_count;
    result.byte_count += key.size() + value.size();
  }

  auto status = it->status();
  if( !status.ok() )
    throw std::runtime_error( "failed to iterate " + name + ": " + status.ToString() );

  result.source_hash = hex_digest( hasher.final() );
  result.target_hash = result.source_hash;
  return result;
}

void write_batch_checked( rocksdb::DB& db, rocksdb::WriteBatch& batch )
{
  if( batch.Count() == 0 )
    return;

  rocksdb::WriteOptions write_options;
  auto status = db.Write( write_options, &batch );
  if( !status.ok() )
    throw std::runtime_error( "failed to write migration batch: " + status.ToString() );
  batch.Clear();
}

void clear_column_family( rocksdb::DB& db, rocksdb::ColumnFamilyHandle& handle )
{
  rocksdb::ReadOptions read_options;
  rocksdb::WriteBatch batch;
  uint64_t batch_records = 0;

  std::unique_ptr< rocksdb::Iterator > it( db.NewIterator( read_options, &handle ) );
  for( it->SeekToFirst(); it->Valid(); it->Next() )
  {
    batch.Delete( &handle, it->key() );
    if( ++batch_records >= max_batch_records )
    {
      write_batch_checked( db, batch );
      batch_records = 0;
    }
  }

  auto status = it->status();
  if( !status.ok() )
    throw std::runtime_error( "failed to clear migration target: " + status.ToString() );

  write_batch_checked( db, batch );
}

void copy_column_family( rocksdb::DB& source_db,
                         rocksdb::ColumnFamilyHandle& source_handle,
                         rocksdb::DB& target_db,
                         rocksdb::ColumnFamilyHandle& target_handle )
{
  clear_column_family( target_db, target_handle );

  rocksdb::ReadOptions read_options;
  rocksdb::WriteBatch batch;
  uint64_t batch_records = 0;
  uint64_t batch_bytes = 0;

  std::unique_ptr< rocksdb::Iterator > it( source_db.NewIterator( read_options, &source_handle ) );
  for( it->SeekToFirst(); it->Valid(); it->Next() )
  {
    batch.Put( &target_handle, it->key(), it->value() );
    ++batch_records;
    batch_bytes += it->key().size() + it->value().size();
    if( batch_records >= max_batch_records || batch_bytes >= max_batch_bytes )
    {
      write_batch_checked( target_db, batch );
      batch_records = 0;
      batch_bytes = 0;
    }
  }

  auto status = it->status();
  if( !status.ok() )
    throw std::runtime_error( "failed to copy migration source: " + status.ToString() );

  write_batch_checked( target_db, batch );

  rocksdb::FlushOptions flush_options;
  status = target_db.Flush( flush_options, &target_handle );
  if( !status.ok() )
    throw std::runtime_error( "failed to flush migration target: " + status.ToString() );
}

OpenedLegacyDB open_legacy_chain_db( const std::filesystem::path& source_path )
{
  rocksdb::Options options;
  options.create_if_missing = false;

  std::vector< rocksdb::ColumnFamilyDescriptor > descriptors = {
    { rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions() },
    { "objects", rocksdb::ColumnFamilyOptions() },
    { "metadata", rocksdb::ColumnFamilyOptions() }
  };

  rocksdb::DB* raw_db = nullptr;
  OpenedLegacyDB opened;
  auto status = rocksdb::DB::OpenForReadOnly( options, source_path.string(), descriptors, &opened.handles, &raw_db );
  if( !status.ok() )
    throw std::runtime_error( "failed to open legacy chain DB read-only at "
                              + source_path.string() + ": " + status.ToString() );
  opened.db.reset( raw_db );
  return opened;
}

void verify_copy( const MigratedColumnFamily& source, const MigratedColumnFamily& target )
{
  if( source.record_count != target.record_count || source.byte_count != target.byte_count
      || source.source_hash != target.source_hash )
  {
    throw std::runtime_error( "migration verification failed for " + source.name );
  }
}

} // anonymous namespace

ChainMigrationResult migrate_legacy_chain_db_to_unified( const std::filesystem::path& basedir,
                                                         RocksDBManager& storage_db )
{
  if( storage_db.read_metadata( "layout.chain_storage" ) == "unified" )
    throw std::runtime_error( "chain storage is already unified" );
  if( storage_db.read_metadata( "layout.chain_storage" ) != "legacy" )
    throw std::runtime_error( "chain storage must be legacy before migration" );

  const auto source_path = basedir / "chain" / "blockchain";
  if( !std::filesystem::exists( source_path ) )
    throw std::runtime_error( "legacy chain DB does not exist: " + source_path.string() );

  const auto backup_path = source_path.parent_path()
                         / ( "blockchain.legacy-pre-unified-" + timestamp_suffix() );
  if( std::filesystem::exists( backup_path ) )
    throw std::runtime_error( "migration backup path already exists: " + backup_path.string() );

  storage_db.write_metadata( "layout.chain_storage", "migration-in-progress" );
  storage_db.write_metadata( "migration.source_path", source_path.string() );

  ChainMigrationResult result;
  result.source_path = source_path;
  result.backup_path = backup_path;

  bool renamed_legacy_db = false;
  try
  {
    {
      auto legacy = open_legacy_chain_db( source_path );
      auto& target_db = *storage_db.db();

      auto source_objects = hash_column_family( *legacy.db, *legacy.handles[ 1 ], "objects" );
      auto source_metadata = hash_column_family( *legacy.db, *legacy.handles[ 2 ], "metadata" );

      copy_column_family( *legacy.db,
                          *legacy.handles[ 1 ],
                          target_db,
                          *storage_db.handle( ColumnFamily::chain_state ) );
      copy_column_family( *legacy.db,
                          *legacy.handles[ 2 ],
                          target_db,
                          *storage_db.handle( ColumnFamily::chain_metadata ) );

      auto target_objects = hash_column_family( target_db,
                                                *storage_db.handle( ColumnFamily::chain_state ),
                                                "objects" );
      auto target_metadata = hash_column_family( target_db,
                                                 *storage_db.handle( ColumnFamily::chain_metadata ),
                                                 "metadata" );

      verify_copy( source_objects, target_objects );
      verify_copy( source_metadata, target_metadata );

      result.objects = source_objects;
      result.objects.target_hash = target_objects.source_hash;
      result.metadata = source_metadata;
      result.metadata.target_hash = target_metadata.source_hash;
    }

    std::filesystem::rename( source_path, backup_path );
    renamed_legacy_db = true;

    storage_db.write_metadata( "layout.chain_storage", "unified" );
    storage_db.write_metadata( "migration.backup_path", backup_path.string() );
    storage_db.write_metadata( "migration.objects.count", std::to_string( result.objects.record_count ) );
    storage_db.write_metadata( "migration.objects.sha256", result.objects.source_hash );
    storage_db.write_metadata( "migration.metadata.count", std::to_string( result.metadata.record_count ) );
    storage_db.write_metadata( "migration.metadata.sha256", result.metadata.source_hash );
    storage_db.write_metadata( "migration.completed_at", timestamp_suffix() );
  }
  catch( ... )
  {
    try
    {
      storage_db.write_metadata( "layout.chain_storage", renamed_legacy_db ? "unified" : "legacy" );
    }
    catch( ... )
    {}
    throw;
  }

  return result;
}

ChainMigrationRollbackResult rollback_unified_chain_db_migration( const std::filesystem::path& basedir,
                                                                  RocksDBManager& storage_db )
{
  if( storage_db.read_metadata( "layout.chain_storage" ) != "unified" )
    throw std::runtime_error( "chain storage must be unified before rollback" );

  const auto restored_path = basedir / "chain" / "blockchain";
  const auto backup_value = storage_db.read_metadata( "migration.backup_path" );
  if( backup_value.empty() )
    throw std::runtime_error( "migration.backup_path metadata is missing; cannot determine rollback source" );

  const auto backup_path = std::filesystem::path( backup_value );
  if( !std::filesystem::exists( backup_path ) )
    throw std::runtime_error( "migration backup path does not exist: " + backup_path.string() );
  if( std::filesystem::exists( restored_path ) )
    throw std::runtime_error( "legacy chain DB already exists; refusing to overwrite: " + restored_path.string() );

  ChainMigrationRollbackResult result;
  result.restored_path = restored_path;
  result.backup_path = backup_path;

  storage_db.write_metadata( "layout.chain_storage", "migration-in-progress" );
  storage_db.write_metadata( "migration.rollback_started_at", timestamp_suffix() );

  bool restored_legacy_db = false;
  try
  {
    std::filesystem::create_directories( restored_path.parent_path() );
    std::filesystem::rename( backup_path, restored_path );
    restored_legacy_db = true;
    storage_db.write_metadata( "layout.chain_storage", "legacy" );
    storage_db.write_metadata( "migration.rollback_restored_path", restored_path.string() );
    storage_db.write_metadata( "migration.rollback_backup_path", backup_path.string() );
    storage_db.write_metadata( "migration.rollback_completed_at", timestamp_suffix() );
  }
  catch( ... )
  {
    try
    {
      storage_db.write_metadata( "layout.chain_storage", restored_legacy_db ? "legacy" : "unified" );
    }
    catch( ... )
    {}
    throw;
  }

  return result;
}

} // namespace koinos::node::storage
