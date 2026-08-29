#include <koinos/state_db/backends/map/map_backend.hpp>
#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>
#include <koinos/state_db/state_db.hpp>

#include <koinos/crypto/multihash.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

using namespace koinos;

namespace {

std::filesystem::path unique_temp_dir()
{
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() / ( "teleno-state-db-pending-root-" + std::to_string( now ) );
}

state_db::state_node_id node_id( uint64_t value )
{
  return crypto::hash( crypto::multicodec::sha2_256, value );
}

void test_pending_root_is_fresh_and_preserves_normal_semantics()
{
  state_db::database db;
  auto unique_lock = db.get_unique_lock();
  db.open(
    std::make_shared< state_db::backends::map::map_backend >(),
    []( const state_db::state_node_ptr& ) {},
    state_db::pob_comparator,
    unique_lock );

  const state_db::object_space space;
  const std::string parent_key   = "parent-key";
  const std::string parent_value = "parent-value";

  auto parent = db.create_writable_node( db.get_head( unique_lock )->id(), node_id( 1 ), {}, unique_lock );
  parent->put_object( space, parent_key, &parent_value );
  db.finalize_node( parent->id(), unique_lock );

  auto normal = db.create_writable_node( parent->id(), node_id( 2 ), {}, unique_lock );
  normal->remove_object( space, "absent-key" );
  assert( normal->get_delta_entries().empty() );

  auto pending = db.create_writable_node( parent->id(), node_id( 3 ), {}, unique_lock );
  const std::string first_key   = "first-key";
  const std::string first_value = "first-value";
  pending->put_object( space, first_key, &first_value );

  const auto first_root = pending->pending_merkle_root();
  bool finalized_root_rejected = false;
  try
  {
    static_cast< void >( pending->merkle_root() );
  }
  catch( const koinos::exception& )
  {
    finalized_root_rejected = true;
  }
  assert( finalized_root_rejected );

  const std::string second_key   = "second-key";
  const std::string second_value = "second-value";
  pending->put_object( space, second_key, &second_value );
  const auto put_root = pending->pending_merkle_root();
  assert( put_root != first_root );

  pending->remove_object_preserve_tombstone( space, "absent-key" );
  const auto tombstone_root = pending->pending_merkle_root();
  assert( tombstone_root != put_root );
  assert( pending->get_delta_entries().size() == 3 );

  auto normal_remove = db.create_writable_node( parent->id(), node_id( 4 ), {}, unique_lock );
  normal_remove->remove_object( space, parent_key );
  auto preserved_remove = db.create_writable_node( parent->id(), node_id( 5 ), {}, unique_lock );
  preserved_remove->remove_object_preserve_tombstone( space, parent_key );
  assert( normal_remove->get_delta_entries().size() == 1 );
  assert( preserved_remove->get_delta_entries().size() == 1 );
  assert( normal_remove->pending_merkle_root() == preserved_remove->pending_merkle_root() );

  db.finalize_node( pending->id(), unique_lock );
  assert( pending->merkle_root() == tombstone_root );
  assert( pending->pending_merkle_root() == tombstone_root );
}

void test_final_root_survives_commit_and_reopen()
{
  const auto path = unique_temp_dir();
  std::filesystem::remove_all( path );
  std::filesystem::create_directories( path );

  const state_db::object_space space;
  const std::string key   = "persisted-key";
  const std::string value = "persisted-value";
  const auto child_id     = node_id( 10 );
  crypto::multihash expected_root;

  {
    auto backend = std::make_shared< state_db::backends::rocksdb::rocksdb_backend >();
    backend->open( path );
    state_db::database db;
    auto lock = db.get_unique_lock();
    db.open( backend, []( const state_db::state_node_ptr& ) {}, state_db::pob_comparator, lock );
    auto child = db.create_writable_node( db.get_head( lock )->id(), child_id, {}, lock );
    child->put_object( space, key, &value );
    child->remove_object_preserve_tombstone( space, "persisted-absent-key" );
    expected_root = child->pending_merkle_root();
    db.finalize_node( child_id, lock );
    assert( child->merkle_root() == expected_root );
    child.reset();
    db.commit_node( child_id, lock );
    assert( db.get_root( lock )->merkle_root() == expected_root );
    backend->flush();
    db.close( lock );
    backend->close();
  }

  {
    state_db::database db;
    auto lock = db.get_unique_lock();
    db.open( path, []( const state_db::state_node_ptr& ) {}, state_db::pob_comparator, lock );
    auto root = db.get_root( lock );
    assert( root->id() == child_id );
    assert( root->merkle_root() == expected_root );
    const auto* stored = root->get_object( space, key );
    assert( stored );
    assert( *stored == value );
    db.close( lock );
  }

  std::filesystem::remove_all( path );
}

void test_serialized_duplicate_key_cannot_reconstruct_remove_then_put_delta()
{
  state_db::database db;
  auto lock = db.get_unique_lock();
  db.open(
    std::make_shared< state_db::backends::map::map_backend >(),
    []( const state_db::state_node_ptr& ) {},
    state_db::pob_comparator,
    lock );

  const state_db::object_space space;
  const std::string key = "remove-then-put-key";
  const std::string old_value = "old-value";
  const std::string new_value = "new-value";

  auto parent = db.create_writable_node( db.get_head( lock )->id(), node_id( 20 ), {}, lock );
  parent->put_object( space, key, &old_value );
  db.finalize_node( parent->id(), lock );

  auto executed = db.create_writable_node( parent->id(), node_id( 21 ), {}, lock );
  executed->remove_object( space, key );
  executed->put_object( space, key, &new_value );
  const auto executed_root = executed->pending_merkle_root();
  const auto receipt_entries = executed->get_delta_entries();

  assert( receipt_entries.size() == 2 );
  assert( receipt_entries[ 0 ].key() == receipt_entries[ 1 ].key() );
  assert( receipt_entries[ 0 ].has_value() );
  assert( receipt_entries[ 1 ].has_value() );
  assert( receipt_entries[ 0 ].value() == new_value );
  assert( receipt_entries[ 1 ].value() == new_value );

  auto replayed = db.create_writable_node( parent->id(), node_id( 22 ), {}, lock );
  for( const auto& entry: receipt_entries )
    replayed->put_object( space, entry.key(), &entry.value() );

  assert( replayed->get_delta_entries().size() == 1 );
  assert( replayed->pending_merkle_root() != executed_root );
}

} // namespace

int main()
{
  test_pending_root_is_fresh_and_preserves_normal_semantics();
  test_final_root_survives_commit_and_reopen();
  test_serialized_duplicate_key_cannot_reconstruct_remove_then_put_delta();
  return 0;
}
