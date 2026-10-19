#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>

#include <koinos/state_db/backends/rocksdb/exceptions.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>
#include <koinos/util/random.hpp>

#include <rocksdb/convenience.h>
#include <rocksdb/filter_policy.h>
#include <iostream>

namespace koinos::state_db::backends::rocksdb {

namespace constants {
constexpr std::size_t max_open_files = 64;

constexpr std::size_t default_column_index  = 0;
const std::string objects_column_name       = "objects";
constexpr std::size_t objects_column_index  = 1;
const std::string metadata_column_name      = "metadata";
constexpr std::size_t metadata_column_index = 2;

const std::string size_key         = "size";
const std::string revision_key     = "revision";
const std::string id_key           = "id";
const std::string merkle_root_key  = "merkle_root";
const std::string block_header_key = "block_header";

constexpr rocksdb_backend::size_type size_default     = 0;
constexpr rocksdb_backend::size_type revision_default = 0;
const crypto::multihash id_default                    = crypto::multihash::zero( crypto::multicodec::sha2_256 );
const crypto::multihash merkle_root_default           = crypto::multihash::zero( crypto::multicodec::sha2_256 );
const protocol::block_header block_header_default     = protocol::block_header();
} // namespace constants

bool write_default_metadata( ::rocksdb::DB& db, ::rocksdb::ColumnFamilyHandle& metadata_handle )
{
  ::rocksdb::WriteBatch batch;
  for(const auto& [key,value]: std::vector<std::pair<std::string,std::string>>{
    {constants::size_key,util::converter::as<std::string>(constants::size_default)},
    {constants::revision_key,util::converter::as<std::string>(constants::revision_default)},
    {constants::id_key,util::converter::as<std::string>(constants::id_default)},
    {constants::merkle_root_key,util::converter::as<std::string>(constants::merkle_root_default)},
    {constants::block_header_key,util::converter::as<std::string>(constants::block_header_default)}})
    if(!batch.Put(&metadata_handle,key,value).ok()) return false;
  return db.Write(::rocksdb::WriteOptions{},&batch).ok();
}

bool setup_database( const std::filesystem::path& p )
{
  constexpr std::size_t setup_metadata_handle_index = 1;

  std::vector< ::rocksdb::ColumnFamilyDescriptor > defs;
  defs.emplace_back( constants::objects_column_name, ::rocksdb::ColumnFamilyOptions() );
  defs.emplace_back( constants::metadata_column_name, ::rocksdb::ColumnFamilyOptions() );

  ::rocksdb::Options options;
  options.create_if_missing = true;

  ::rocksdb::DB* db;
  auto status = ::rocksdb::DB::Open( options, p.string(), &db );

  KOINOS_ASSERT( status.ok(),
                 rocksdb_open_exception,
                 "unable to open rocksdb database"
                   + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

  auto db_ptr = std::shared_ptr< ::rocksdb::DB >( db );

  std::vector< ::rocksdb::ColumnFamilyHandle* > handles;
  status = db->CreateColumnFamilies( defs, &handles );

  if( !status.ok() )
  {
    return false;
  }

  std::vector< std::shared_ptr< ::rocksdb::ColumnFamilyHandle > > handle_ptrs;

  for( auto* h: handles )
    handle_ptrs.emplace_back( h );

  auto ok = write_default_metadata( *db_ptr, *handle_ptrs[ setup_metadata_handle_index ] );

  handle_ptrs.clear();
  db_ptr.reset();

  return ok;
}

bool setup_borrowed_database( ::rocksdb::DB& db, ::rocksdb::ColumnFamilyHandle& metadata_handle )
{
  std::string value;
  auto status = db.Get( ::rocksdb::ReadOptions(), &metadata_handle, ::rocksdb::Slice( constants::size_key ), &value );
  if( status.ok() )
    return true;
  if( !status.IsNotFound() )
    return false;

  std::unique_ptr<::rocksdb::Iterator> it(db.NewIterator(::rocksdb::ReadOptions{},&metadata_handle));
  it->SeekToFirst();
  if(!it->status().ok() || it->Valid()) return false; // Never initialize over partial metadata.

  return write_default_metadata( db, metadata_handle );
}

rocksdb_backend::rocksdb_backend(std::size_t object_cache_bytes):
    _cache( std::make_shared< object_cache >( object_cache_bytes ) ),
    _ropts( std::make_shared< ::rocksdb::ReadOptions >() )
{}

rocksdb_backend::~rocksdb_backend()
{
  try { close(); }
  catch( ... ) { std::cerr << "[state_db] close failed; preserve database and validate recovery before restart\n"; }
}

void rocksdb_backend::open( const std::filesystem::path& p )
{
  KOINOS_ASSERT( !_db, rocksdb_open_exception, "database already open" );
  _failed = false;
  KOINOS_ASSERT( p.is_absolute(), rocksdb_open_exception, "path must be absolute, ${p}", ( "p", p.string() ) );
  KOINOS_ASSERT( std::filesystem::exists( p ),
                 rocksdb_open_exception,
                 "path does not exist, ${p}",
                 ( "p", p.string() ) );

  std::vector< ::rocksdb::ColumnFamilyDescriptor > defs;
  defs.emplace_back( ::rocksdb::kDefaultColumnFamilyName, ::rocksdb::ColumnFamilyOptions() );
  defs.emplace_back( constants::objects_column_name, ::rocksdb::ColumnFamilyOptions() );
  defs.emplace_back( constants::metadata_column_name, ::rocksdb::ColumnFamilyOptions() );

  std::vector< ::rocksdb::ColumnFamilyHandle* > handles;

  ::rocksdb::Options options;
  options.max_open_files = constants::max_open_files;
  ::rocksdb::DB* db;

  const bool initially_empty = std::filesystem::is_empty(p);
  auto status = ::rocksdb::DB::Open( options, p.string(), defs, &handles, &db );

  if( !status.ok() )
  {
    KOINOS_ASSERT( initially_empty,
                   rocksdb_open_exception, "existing database failed to open; preserve it for diagnosis" );
    KOINOS_ASSERT( setup_database( p ), rocksdb_setup_exception, "unable to configure rocksdb database" );

    status = ::rocksdb::DB::Open( options, p.string(), defs, &handles, &db );
    KOINOS_ASSERT( status.ok(),
                   rocksdb_open_exception,
                   "unable to open rocksdb database"
                     + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );
  }

  _db = std::shared_ptr< ::rocksdb::DB >( db );

  for( auto* h: handles )
    _handles.emplace_back( h );

  try
  {
    load_metadata();
  }
  catch( ... )
  {
    _handles.clear();
    _db.reset();
    throw;
  }
}

void rocksdb_backend::open( ::rocksdb::DB& db,
                            ::rocksdb::ColumnFamilyHandle& default_handle,
                            ::rocksdb::ColumnFamilyHandle& objects_handle,
                            ::rocksdb::ColumnFamilyHandle& metadata_handle )
{
  KOINOS_ASSERT( !_db, rocksdb_open_exception, "database already open" );
  _failed = false;
  KOINOS_ASSERT( setup_borrowed_database( db, metadata_handle ),
                 rocksdb_setup_exception,
                 "unable to configure borrowed rocksdb database" );

  _borrowed = true;
  _db = std::shared_ptr< ::rocksdb::DB >( &db, []( ::rocksdb::DB* ) {} );
  _handles.emplace_back( &default_handle, []( ::rocksdb::ColumnFamilyHandle* ) {} );
  _handles.emplace_back( &objects_handle, []( ::rocksdb::ColumnFamilyHandle* ) {} );
  _handles.emplace_back( &metadata_handle, []( ::rocksdb::ColumnFamilyHandle* ) {} );

  try
  {
    load_metadata();
  }
  catch( ... )
  {
    _handles.clear();
    _db.reset();
    _borrowed = false;
    throw;
  }
}

void rocksdb_backend::close()
{
  if( _db )
  {
    try
    {
      check_usable();
      KOINOS_ASSERT( !_write_batch, rocksdb_session_in_progress, "cannot close an unfinished write session" );
      store_metadata(); flush();
    }
    catch( ... ) { release_database(); throw; }
    release_database();
  }
}

void rocksdb_backend::release_database() noexcept
{
  if( _db && !_borrowed ) ::rocksdb::CancelAllBackgroundWork( &*_db, true );
  _write_batch.reset(); _handles.clear(); _db.reset(); _borrowed = false;
  std::lock_guard lock( _cache->get_mutex() ); _cache->clear();
}

void rocksdb_backend::check_usable() const
{
  KOINOS_ASSERT( _db, rocksdb_database_not_open_exception, "database not open" );
  KOINOS_ASSERT( !_failed, rocksdb_write_exception, "failed state session; close and validate persisted data before reuse" );
}

void rocksdb_backend::abort_write_batch() noexcept
{ _failed = true; _write_batch.reset(); }

void rocksdb_backend::flush()
{
  check_usable();

  static const ::rocksdb::FlushOptions flush_options;

  for( auto column: {constants::objects_column_index,constants::metadata_column_index} )
  {
    const auto status = _db->Flush( flush_options, &*_handles[column] );
    KOINOS_ASSERT( status.ok(), rocksdb_write_exception, "state flush failed: ${status}", ("status",status.ToString()) );
  }
}

void rocksdb_backend::force_async_writes( bool force )
{
  _force_async_writes = force;
}

void rocksdb_backend::start_write_batch()
{
  check_usable();
  KOINOS_ASSERT( !_write_batch, rocksdb_session_in_progress, "session already in progress" );
  _write_batch.emplace();
}

void rocksdb_backend::end_write_batch( write_durability durability )
{
  check_usable();
  if( _write_batch )
  {
    auto write_options = _wopts;
    if( durability == write_durability::sync && !_force_async_writes )
      write_options.sync = true;

    auto status = _db->Write( write_options, &*_write_batch );
    if( !status.ok() ) abort_write_batch();
    KOINOS_ASSERT( status.ok(),
                   rocksdb_write_exception,
                   "unable to write session to rocksdb database"
                     + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );
    _write_batch.reset();
  }
}

iterator rocksdb_backend::begin()
{
  check_usable();

  auto itr   = std::make_unique< rocksdb_iterator >( _db, _handles[ constants::objects_column_index ], _ropts, _cache );
  itr->_iter = std::unique_ptr< ::rocksdb::Iterator >(
    _db->NewIterator( *_ropts, &*_handles[ constants::objects_column_index ] ) );
  itr->_iter->SeekToFirst();

  return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

iterator rocksdb_backend::end()
{
  check_usable();

  auto itr   = std::make_unique< rocksdb_iterator >( _db, _handles[ constants::objects_column_index ], _ropts, _cache );
  itr->_iter = std::unique_ptr< ::rocksdb::Iterator >(
    _db->NewIterator( *_ropts, &*_handles[ constants::objects_column_index ] ) );

  return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

void rocksdb_backend::put( const key_type& k, const value_type& v )
{
  check_usable();
  bool exists = static_cast<bool>( get( k ) );

  ::rocksdb::Status status;

  if( _write_batch )
  {
    status =
      _write_batch->Put( &*_handles[ constants::objects_column_index ], ::rocksdb::Slice( k ), ::rocksdb::Slice( v ) );
  }
  else
  {
    status =
      _db->Put( _wopts, &*_handles[ constants::objects_column_index ], ::rocksdb::Slice( k ), ::rocksdb::Slice( v ) );
  }

  KOINOS_ASSERT( status.ok(),
                 rocksdb_write_exception,
                 "unable to write to rocksdb database"
                   + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

  if( !exists )
  {
    _size++;
  }

  std::lock_guard lock( _cache->get_mutex() );
  _cache->put( k, std::make_shared< const object_cache::value_type >( v ) );
}

rocksdb_backend::value_ptr rocksdb_backend::get( const key_type& k ) const
{
  check_usable();

  std::lock_guard lock( _cache->get_mutex() );
  auto [ cache_hit, ptr ] = _cache->get( k );
  if( cache_hit )
  {
    return ptr;
  }

  value_type value;
  auto status = _db->Get( *_ropts, &*_handles[ constants::objects_column_index ], ::rocksdb::Slice( k ), &value );

  if( status.ok() )
    return _cache->put( k, std::make_shared< const object_cache::value_type >( std::move(value) ) );
  else if( status.IsNotFound() )
    _cache->put( k, std::shared_ptr< const object_cache::value_type >() );
  else
    KOINOS_THROW( rocksdb_read_exception, "state read failed: ${status}", ("status",status.ToString()) );

  return nullptr;
}

void rocksdb_backend::erase( const key_type& k )
{
  check_usable();

  bool exists = static_cast<bool>( get( k ) );
  ::rocksdb::Status status;

  if( _write_batch )
  {
    status = _write_batch->Delete( &*_handles[ constants::objects_column_index ], ::rocksdb::Slice( k ) );
  }
  else
  {
    status = _db->Delete( _wopts, &*_handles[ constants::objects_column_index ], ::rocksdb::Slice( k ) );
  }

  KOINOS_ASSERT( status.ok(),
                 rocksdb_write_exception,
                 "unable to write to rocksdb database"
                   + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

  if( exists )
  {
    _size--;
  }

  std::lock_guard lock( _cache->get_mutex() );
  _cache->put( k, std::shared_ptr< const object_cache::value_type >() );
}

void rocksdb_backend::clear()
{
  check_usable();
  KOINOS_ASSERT( !_borrowed,
                 rocksdb_internal_exception,
                 "cannot clear borrowed rocksdb column families through state_db backend" );

  for( auto h: _handles )
  {
    _db->DropColumnFamily( &*h );
  }

  _handles.clear();
  _db.reset();
  std::lock_guard lock( _cache->get_mutex() );
  _cache->clear();
}

rocksdb_backend::size_type rocksdb_backend::size() const
{
  check_usable();

  return _size;
}

iterator rocksdb_backend::find( const key_type& k )
{
  check_usable();

  auto itr = std::make_unique< rocksdb_iterator >( _db, _handles[ constants::objects_column_index ], _ropts, _cache );
  auto itr_ptr = std::unique_ptr< ::rocksdb::Iterator >(
    _db->NewIterator( *_ropts, &*_handles[ constants::objects_column_index ] ) );

  itr_ptr->Seek( ::rocksdb::Slice( k ) );

  if( itr_ptr->Valid() )
  {
    auto key_slice = itr_ptr->key();

    if( k.size() == key_slice.size() && memcmp( k.data(), key_slice.data(), k.size() ) == 0 )
    {
      itr->_iter = std::move( itr_ptr );
    }
  }

  return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

iterator rocksdb_backend::lower_bound( const key_type& k )
{
  check_usable();

  auto itr   = std::make_unique< rocksdb_iterator >( _db, _handles[ constants::objects_column_index ], _ropts, _cache );
  itr->_iter = std::unique_ptr< ::rocksdb::Iterator >(
    _db->NewIterator( *_ropts, &*_handles[ constants::objects_column_index ] ) );

  itr->_iter->Seek( ::rocksdb::Slice( k ) );

  return iterator( std::unique_ptr< abstract_iterator >( std::move( itr ) ) );
}

void rocksdb_backend::load_metadata()
{
  check_usable();

  std::string value;
  auto status = _db->Get( *_ropts,
                          &*_handles[ constants::metadata_column_index ],
                          ::rocksdb::Slice( constants::size_key ),
                          &value );

  KOINOS_ASSERT( status.ok(),
                 rocksdb_read_exception,
                 "unable to read from rocksdb database"
                   + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

  _size = util::converter::to< size_type >( value );

  status = _db->Get( *_ropts,
                     &*_handles[ constants::metadata_column_index ],
                     ::rocksdb::Slice( constants::revision_key ),
                     &value );

  KOINOS_ASSERT( status.ok(),
                 rocksdb_read_exception,
                 "unable to read from rocksdb database"
                   + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

  set_revision( util::converter::to< size_type >( value ) );

  status =
    _db->Get( *_ropts, &*_handles[ constants::metadata_column_index ], ::rocksdb::Slice( constants::id_key ), &value );

  KOINOS_ASSERT( status.ok(),
                 rocksdb_read_exception,
                 "unable to read from rocksdb database"
                   + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

  set_id( util::converter::to< crypto::multihash >( value ) );

  status = _db->Get( *_ropts,
                     &*_handles[ constants::metadata_column_index ],
                     ::rocksdb::Slice( constants::merkle_root_key ),
                     &value );

  KOINOS_ASSERT( status.ok(),
                 rocksdb_read_exception,
                 "unable to read from rocksdb database"
                   + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

  set_merkle_root( util::converter::to< crypto::multihash >( value ) );

  status = _db->Get( *_ropts,
                     &*_handles[ constants::metadata_column_index ],
                     ::rocksdb::Slice( constants::block_header_key ),
                     &value );

  KOINOS_ASSERT( status.ok(),
                 rocksdb_read_exception,
                 "unable to read from rocksdb database"
                   + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );

  set_block_header( util::converter::to< protocol::block_header >( value ) );
}

void rocksdb_backend::put_metadata_value( const std::string& key, const std::string& value )
{
  check_usable();

  ::rocksdb::Status status;

  if( _write_batch )
  {
    status = _write_batch->Put( &*_handles[ constants::metadata_column_index ],
                                ::rocksdb::Slice( key ),
                                ::rocksdb::Slice( value ) );
  }
  else
  {
    status = _db->Put( _wopts,
                       &*_handles[ constants::metadata_column_index ],
                       ::rocksdb::Slice( key ),
                       ::rocksdb::Slice( value ) );
  }

  KOINOS_ASSERT( status.ok(),
                 rocksdb_write_exception,
                 "unable to write to rocksdb database"
                   + ( status.getState() ? ", " + std::string( status.getState() ) : "" ) );
}

void rocksdb_backend::store_metadata()
{
  check_usable();

  put_metadata_value( constants::size_key, util::converter::as< std::string >( _size ) );
  put_metadata_value( constants::revision_key, util::converter::as< std::string >( revision() ) );
  put_metadata_value( constants::id_key, util::converter::as< std::string >( id() ) );
  put_metadata_value( constants::merkle_root_key, util::converter::as< std::string >( merkle_root() ) );
  put_metadata_value( constants::block_header_key, util::converter::as< std::string >( block_header() ) );
}

std::shared_ptr< abstract_backend > rocksdb_backend::clone() const
{
  KOINOS_THROW( internal_exception, "rocksdb_backend, 'clone' not implemented" );
}

} // namespace koinos::state_db::backends::rocksdb
