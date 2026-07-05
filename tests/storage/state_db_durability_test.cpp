#include <koinos/state_db/backends/map/map_backend.hpp>
#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>
#include <koinos/state_db/state_delta.hpp>
#include <koinos/util/conversion.hpp>

#include <rocksdb/db.h>
#include <rocksdb/options.h>

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

using namespace koinos;

namespace {

std::filesystem::path unique_temp_dir( const std::string& prefix )
{
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  auto path      = std::filesystem::temp_directory_path() / ( prefix + "-" + std::to_string( now ) );
  std::filesystem::remove_all( path );
  std::filesystem::create_directories( path );
  return path;
}

class borrowed_rocksdb
{
public:
  explicit borrowed_rocksdb( const std::string& prefix ):
      _path( unique_temp_dir( prefix ) )
  {
    rocksdb::Options options;
    options.create_if_missing = true;

    auto status = rocksdb::DB::Open( options, _path.string(), &_db );
    assert( status.ok() );

    std::vector< rocksdb::ColumnFamilyDescriptor > descriptors = {
      { "objects", rocksdb::ColumnFamilyOptions() },
      { "metadata", rocksdb::ColumnFamilyOptions() }
    };

    status = _db->CreateColumnFamilies( descriptors, &_handles );
    assert( status.ok() );
    assert( _handles.size() == 2 );
  }

  ~borrowed_rocksdb()
  {
    for( auto* handle: _handles )
      delete handle;
    delete _db;
    std::filesystem::remove_all( _path );
  }

  rocksdb::DB& db()
  {
    return *_db;
  }

  rocksdb::ColumnFamilyHandle& default_handle()
  {
    return *_db->DefaultColumnFamily();
  }

  rocksdb::ColumnFamilyHandle& objects_handle()
  {
    return *_handles.at( 0 );
  }

  rocksdb::ColumnFamilyHandle& metadata_handle()
  {
    return *_handles.at( 1 );
  }

private:
  std::filesystem::path _path;
  rocksdb::DB* _db = nullptr;
  std::vector< rocksdb::ColumnFamilyHandle* > _handles;
};

bool read_cf( rocksdb::DB& db, rocksdb::ColumnFamilyHandle& handle, const std::string& key, std::string& value )
{
  auto status = db.Get( rocksdb::ReadOptions(), &handle, rocksdb::Slice( key ), &value );
  assert( status.ok() || status.IsNotFound() );
  return status.ok();
}

class recording_backend final: public state_db::backends::abstract_backend
{
public:
  using backend_iterator = state_db::backends::iterator;

  backend_iterator begin() override
  {
    return _map.begin();
  }

  backend_iterator end() override
  {
    return _map.end();
  }

  void put( const key_type& k, const value_type& v ) override
  {
    _map.put( k, v );
  }

  const value_type* get( const key_type& k ) const override
  {
    return _map.get( k );
  }

  void erase( const key_type& k ) override
  {
    _map.erase( k );
  }

  void clear() override
  {
    _map.clear();
  }

  size_type size() const override
  {
    return _map.size();
  }

  backend_iterator find( const key_type& k ) override
  {
    return _map.find( k );
  }

  backend_iterator lower_bound( const key_type& k ) override
  {
    return _map.lower_bound( k );
  }

  void start_write_batch() override
  {
    batch_started = true;
  }

  void end_write_batch( state_db::backends::write_durability durability ) override
  {
    assert( batch_started );
    batch_ended     = true;
    last_durability = durability;
  }

  void store_metadata() override {}

  std::shared_ptr< abstract_backend > clone() const override
  {
    return std::make_shared< recording_backend >( *this );
  }

  bool batch_started = false;
  bool batch_ended   = false;
  std::optional< state_db::backends::write_durability > last_durability;

private:
  state_db::backends::map::map_backend _map;
};

void test_rocksdb_delete_and_metadata_are_batched()
{
  borrowed_rocksdb fixture( "teleno-state-db-durability" );
  state_db::backends::rocksdb::rocksdb_backend backend;

  backend.open( fixture.db(), fixture.default_handle(), fixture.objects_handle(), fixture.metadata_handle() );

  const std::string existing_key   = "existing-object";
  const std::string existing_value = "old-value";
  const std::string new_key        = "new-object";
  const std::string new_value      = "new-value";

  backend.put( existing_key, existing_value );

  std::string value;
  assert( read_cf( fixture.db(), fixture.objects_handle(), existing_key, value ) );
  assert( value == existing_value );
  assert( read_cf( fixture.db(), fixture.metadata_handle(), "revision", value ) );
  const auto initial_revision = value;

  backend.start_write_batch();
  backend.erase( existing_key );
  backend.put( new_key, new_value );
  backend.set_revision( 7 );
  backend.store_metadata();

  assert( read_cf( fixture.db(), fixture.objects_handle(), existing_key, value ) );
  assert( value == existing_value );
  assert( !read_cf( fixture.db(), fixture.objects_handle(), new_key, value ) );
  assert( read_cf( fixture.db(), fixture.metadata_handle(), "revision", value ) );
  assert( value == initial_revision );

  backend.end_write_batch();

  assert( !read_cf( fixture.db(), fixture.objects_handle(), existing_key, value ) );
  assert( read_cf( fixture.db(), fixture.objects_handle(), new_key, value ) );
  assert( value == new_value );
  assert( read_cf( fixture.db(), fixture.metadata_handle(), "revision", value ) );
  assert( value == util::converter::as< std::string >( uint64_t( 7 ) ) );

  backend.close();
}

void initialize_crash_fixture( const std::filesystem::path& path )
{
  std::filesystem::remove_all( path );
  std::filesystem::create_directories( path );

  state_db::backends::rocksdb::rocksdb_backend backend;
  backend.open( path );
  backend.put( "existing-object", "old-value" );
  backend.set_revision( 1 );
  backend.store_metadata();
  backend.close();
}

void mutate_fixture_then_exit_without_cleanup( const std::filesystem::path& path, bool commit_batch )
{
  state_db::backends::rocksdb::rocksdb_backend backend;
  backend.open( path );
  backend.start_write_batch();
  backend.erase( "existing-object" );
  backend.put( "new-object", "new-value" );
  backend.set_revision( 2 );
  backend.store_metadata();

  if( commit_batch )
    backend.end_write_batch( state_db::backends::write_durability::sync );

  std::_Exit( commit_batch ? 78 : 77 );
}

void run_child_process_mutation( const std::filesystem::path& path, bool commit_batch )
{
  auto pid = fork();
  assert( pid >= 0 );

  if( pid == 0 )
  {
    mutate_fixture_then_exit_without_cleanup( path, commit_batch );
  }

  int status = 0;
  assert( waitpid( pid, &status, 0 ) == pid );
  assert( WIFEXITED( status ) );
  assert( WEXITSTATUS( status ) == ( commit_batch ? 78 : 77 ) );
}

void assert_fixture_state( const std::filesystem::path& path,
                           bool existing_present,
                           bool new_present,
                           uint64_t revision )
{
  state_db::backends::rocksdb::rocksdb_backend backend;
  backend.open( path );

  auto existing = backend.get( "existing-object" );
  auto next     = backend.get( "new-object" );

  assert( static_cast< bool >( existing ) == existing_present );
  if( existing )
    assert( *existing == "old-value" );

  assert( static_cast< bool >( next ) == new_present );
  if( next )
    assert( *next == "new-value" );

  assert( backend.revision() == revision );
  assert( backend.size() == 1 );
  backend.close();
}

void test_process_crash_before_batch_commit_preserves_previous_state()
{
  auto path = unique_temp_dir( "teleno-state-db-crash-before-commit" );
  initialize_crash_fixture( path );

  run_child_process_mutation( path, false );

  assert_fixture_state( path, true, false, 1 );
  std::filesystem::remove_all( path );
}

void test_process_crash_after_sync_batch_commit_preserves_new_state()
{
  auto path = unique_temp_dir( "teleno-state-db-crash-after-commit" );
  initialize_crash_fixture( path );

  run_child_process_mutation( path, true );

  assert_fixture_state( path, false, true, 2 );
  std::filesystem::remove_all( path );
}

void test_state_delta_commit_uses_sync_durability()
{
  auto backend = std::make_shared< recording_backend >();
  backend->put( "removed-object", "old-value" );

  auto root  = std::make_shared< state_db::detail::state_delta >( backend );
  auto child = root->make_child();

  child->erase( "removed-object" );
  child->put( "new-object", "new-value" );
  child->commit();

  assert( backend->batch_started );
  assert( backend->batch_ended );
  assert( backend->last_durability );
  assert( *backend->last_durability == state_db::backends::write_durability::sync );
}

} // namespace

int main()
{
  test_rocksdb_delete_and_metadata_are_batched();
  test_process_crash_before_batch_commit_preserves_previous_state();
  test_process_crash_after_sync_batch_commit_preserves_new_state();
  test_state_delta_commit_uses_sync_durability();
  return 0;
}
