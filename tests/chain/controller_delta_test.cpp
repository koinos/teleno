#include "koinos/chain/controller.hpp"
#include "koinos/chain/state.hpp"
#include "koinos/state_db/backends/map/map_backend.hpp"
#include "koinos/state_db/state_db.hpp"

#include <koinos/crypto/merkle_tree.hpp>
#include <koinos/protocol/protocol.pb.h>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>

#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace koinos;

namespace {

const std::string kfs_contract_id = "1A5BmMqV5jN5zBrdkhQumAfDZBzXLPBeN9";

chain::genesis_data make_genesis()
{
  chain::genesis_data genesis;
  auto* entry = genesis.add_entries();
  *entry->mutable_space() = chain::state::space::metadata();
  entry->set_key( chain::state::key::genesis_key );
  entry->set_value( "test-genesis-key" );
  return genesis;
}

void open_controller( chain::controller& controller )
{
  controller.open(
    std::make_shared< state_db::backends::map::map_backend >(),
    make_genesis(),
    chain::fork_resolution_algorithm::pob,
    false );
}

protocol::object_space protocol_space( const chain::object_space& space )
{
  protocol::object_space result;
  result.set_system( space.system() );
  result.set_zone( space.zone() );
  result.set_id( space.id() );
  return result;
}

chain::object_space chain_space( const protocol::object_space& space )
{
  chain::object_space result;
  result.set_system( space.system() );
  result.set_zone( space.zone() );
  result.set_id( space.id() );
  return result;
}

chain::object_space kfs_contract_space( uint32_t id )
{
  chain::object_space result;
  result.set_system( true );
  result.set_zone( util::from_base58< std::string >( kfs_contract_id ) );
  result.set_id( id );
  return result;
}

protocol::state_delta_entry make_delta_entry()
{
  protocol::state_delta_entry entry;
  *entry.mutable_object_space() = protocol_space( chain::state::space::metadata() );
  entry.set_key( "test-state-key" );
  entry.set_value( "test-state-value" );
  return entry;
}

const protocol::state_delta_entry*
find_delta_entry( const std::vector< protocol::state_delta_entry >& entries, const std::string& key )
{
  const auto itr = std::find_if( entries.begin(), entries.end(), [&]( const auto& entry ) {
    return entry.key() == key;
  } );
  return itr == entries.end() ? nullptr : &( *itr );
}

std::string serialized_database_key( const chain::object_space& space, const std::string& key )
{
  chain::database_key db_key;
  *db_key.mutable_space() = space;
  db_key.set_key( key );
  return util::converter::as< std::string >( db_key );
}

std::string merkle_root_for_entries( std::vector< std::pair< std::string, std::string > > entries )
{
  std::sort( entries.begin(), entries.end(), []( const auto& lhs, const auto& rhs ) {
    return lhs.first < rhs.first;
  } );

  std::vector< crypto::multihash > leaves;
  leaves.reserve( entries.size() * 2 );
  for( const auto& [ key, value ]: entries )
  {
    leaves.emplace_back( crypto::hash( crypto::multicodec::sha2_256, key ) );
    leaves.emplace_back( crypto::hash( crypto::multicodec::sha2_256, value ) );
  }

  return util::converter::as< std::string >( crypto::merkle_tree( crypto::multicodec::sha2_256, leaves ).root()->hash() );
}

std::string genesis_merkle_root()
{
  const auto genesis = make_genesis();
  state_db::database db;
  auto lock = db.get_unique_lock();
  db.open(
    std::make_shared< state_db::backends::map::map_backend >(),
    [ genesis ]( state_db::state_node_ptr root ) {
      for( const auto& entry: genesis.entries() )
        root->put_object( entry.space(), entry.key(), &entry.value() );

      auto chain_id = util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, genesis ) );
      root->put_object( chain::state::space::metadata(), chain::state::key::chain_id, &chain_id );
    },
    state_db::pob_comparator,
    lock );
  return util::converter::as< std::string >( db.get_root( lock )->merkle_root() );
}

std::string state_delta_merkle_root( const std::vector< protocol::state_delta_entry >& entries )
{
  std::vector< std::pair< std::string, std::string > > merkle_entries;
  merkle_entries.reserve( entries.size() );
  for( const auto& entry: entries )
  {
    merkle_entries.emplace_back(
      serialized_database_key( chain_space( entry.object_space() ), entry.key() ),
      entry.has_value() ? entry.value() : std::string() );
  }
  return merkle_root_for_entries( merkle_entries );
}

std::string state_delta_merkle_root( const protocol::state_delta_entry& entry )
{
  return state_delta_merkle_root( std::vector< protocol::state_delta_entry >{ entry } );
}

protocol::block make_block()
{
  protocol::block block;
  block.mutable_header()->set_height( 1 );
  block.mutable_header()->set_previous(
    util::converter::as< std::string >( crypto::multihash::zero( crypto::multicodec::sha2_256 ) ) );
  block.mutable_header()->set_previous_state_merkle_root( genesis_merkle_root() );
  block.mutable_header()->set_timestamp( 1'000 );
  block.set_id( util::converter::as< std::string >(
    crypto::hash( crypto::multicodec::sha2_256, block.header() ) ) );
  return block;
}

protocol::block_receipt make_receipt( const protocol::block& block,
                                      const std::vector< protocol::state_delta_entry >& entries )
{
  protocol::block_receipt receipt;
  receipt.set_id( block.id() );
  receipt.set_height( block.header().height() );
  for( const auto& entry: entries )
    *receipt.add_state_delta_entries() = entry;
  receipt.set_state_merkle_root( state_delta_merkle_root( entries ) );
  return receipt;
}

protocol::block_receipt make_receipt( const protocol::block& block, const protocol::state_delta_entry& entry )
{
  return make_receipt( block, std::vector< protocol::state_delta_entry >{ entry } );
}

std::string pending_root_string( const state_db::abstract_state_node_ptr& node )
{
  return util::converter::as< std::string >( node->pending_merkle_root() );
}

void apply_delta_entries( const state_db::abstract_state_node_ptr& node,
                          const std::vector< protocol::state_delta_entry >& entries,
                          bool preserve_remove_tombstones )
{
  for( const auto& entry: entries )
  {
    const auto space = chain_space( entry.object_space() );
    if( entry.has_value() )
    {
      const auto& value = entry.value();
      node->put_object( space, entry.key(), &value );
    }
    else if( preserve_remove_tombstones )
    {
      node->remove_object_preserve_tombstone( space, entry.key() );
    }
    else
    {
      node->remove_object( space, entry.key() );
    }
  }
}

state_db::state_node_ptr make_parent_state_with_existing_contract_key( state_db::database& db )
{
  auto lock = db.get_unique_lock();
  db.open(
    std::make_shared< state_db::backends::map::map_backend >(),
    []( state_db::state_node_ptr root ) {
      const auto genesis = make_genesis();
      for( const auto& entry: genesis.entries() )
        root->put_object( entry.space(), entry.key(), &entry.value() );

      auto chain_id = util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, genesis ) );
      root->put_object( chain::state::space::metadata(), chain::state::key::chain_id, &chain_id );

      const std::string value = "parent-contract-value";
      root->put_object( chain::state::space::metadata(), "contract-key-A", &value );
    },
    state_db::pob_comparator,
    lock );
  return db.get_root( lock );
}

state_db::state_node_ptr make_parent_state_with_existing_kfs_project_order( state_db::database& db )
{
  auto lock = db.get_unique_lock();
  db.open(
    std::make_shared< state_db::backends::map::map_backend >(),
    []( state_db::state_node_ptr root ) {
      const auto genesis = make_genesis();
      for( const auto& entry: genesis.entries() )
        root->put_object( entry.space(), entry.key(), &entry.value() );

      auto chain_id = util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, genesis ) );
      root->put_object( chain::state::space::metadata(), chain::state::key::chain_id, &chain_id );

      const std::string current_order_entry = "fund.project id=7 status=active total_votes=100";
      root->put_object( kfs_contract_space( 2 ), "active/by_votes/0000000100/project/0000000007", &current_order_entry );
    },
    state_db::pob_comparator,
    lock );
  return db.get_root( lock );
}

void assert_throws_with( const std::function< void() >& fn, const std::string& expected )
{
  bool threw = false;
  try
  {
    fn();
  }
  catch( const std::exception& e )
  {
    threw = true;
    assert( std::string( e.what() ).find( expected ) != std::string::npos );
  }
  assert( threw );
}

void test_apply_block_delta_rejects_parent_state_merkle_mismatch()
{
  chain::controller controller;
  open_controller( controller );
  const auto entry = make_delta_entry();
  auto block = make_block();
  block.mutable_header()->set_previous_state_merkle_root( "wrong-parent-state-root" );
  const auto receipt = make_receipt( block, entry );

  assert_throws_with(
    [&]() {
      controller.apply_block_delta( block, receipt, 1 );
    },
    "block previous state merkle mismatch" );
}

void test_apply_block_delta_rejects_receipt_state_merkle_mismatch()
{
  chain::controller controller;
  open_controller( controller );
  const auto entry = make_delta_entry();
  const auto block = make_block();
  auto receipt = make_receipt( block, entry );
  receipt.set_state_merkle_root( "wrong-receipt-state-root" );

  assert_throws_with(
    [&]() {
      controller.apply_block_delta( block, receipt, 1 );
    },
    "block receipt state merkle mismatch" );
}

void test_apply_block_delta_preserves_absent_remove_tombstone()
{
  chain::controller controller;
  open_controller( controller );

  protocol::state_delta_entry entry;
  *entry.mutable_object_space() = protocol_space( chain::state::space::metadata() );
  entry.set_key( "absent-state-key" );

  const auto block   = make_block();
  const auto receipt = make_receipt( block, entry );

  assert_throws_with(
    [&]() {
      controller.apply_block_delta( block, receipt, 1 );
    },
    "compute bandwidth registry does not exist" );
}

void test_transient_contract_state_delta_requires_preserved_tombstone()
{
  state_db::database db;
  auto parent = make_parent_state_with_existing_contract_key( db );

  auto expected_node = parent->create_anonymous_node();
  const auto space   = chain::state::space::metadata();

  const std::string value_b = "intermediate-contract-value";
  const std::string value_c = "final-contract-value";

  expected_node->remove_object( space, "contract-key-A" );
  expected_node->put_object( space, "contract-key-B", &value_b );
  expected_node->remove_object( space, "contract-key-B" );
  expected_node->put_object( space, "contract-key-C", &value_c );

  const auto receipt_entries = expected_node->get_delta_entries();
  const auto expected_root   = pending_root_string( expected_node );

  assert( receipt_entries.size() == 3 );

  const auto* entry_a = find_delta_entry( receipt_entries, "contract-key-A" );
  const auto* entry_b = find_delta_entry( receipt_entries, "contract-key-B" );
  const auto* entry_c = find_delta_entry( receipt_entries, "contract-key-C" );

  assert( entry_a );
  assert( entry_b );
  assert( entry_c );
  assert( !entry_a->has_value() );
  assert( !entry_b->has_value() );
  assert( entry_c->has_value() );
  assert( entry_c->value() == value_c );

  auto normal_replay_node = parent->create_anonymous_node();
  apply_delta_entries( normal_replay_node, receipt_entries, false );
  assert( pending_root_string( normal_replay_node ) != expected_root );

  auto preserved_tombstone_replay_node = parent->create_anonymous_node();
  apply_delta_entries( preserved_tombstone_replay_node, receipt_entries, true );
  assert( pending_root_string( preserved_tombstone_replay_node ) == expected_root );
}

void test_kfs_project_order_delta_requires_preserved_tombstone()
{
  state_db::database db;
  auto parent = make_parent_state_with_existing_kfs_project_order( db );

  auto expected_node = parent->create_anonymous_node();
  const auto space   = kfs_contract_space( 2 );

  const std::string intermediate_order_entry = "fund.project id=7 status=active total_votes=175";
  const std::string final_order_entry        = "fund.project id=7 status=active total_votes=250";

  expected_node->remove_object( space, "active/by_votes/0000000100/project/0000000007" );
  expected_node->put_object( space, "active/by_votes/0000000175/project/0000000007", &intermediate_order_entry );
  expected_node->remove_object( space, "active/by_votes/0000000175/project/0000000007" );
  expected_node->put_object( space, "active/by_votes/0000000250/project/0000000007", &final_order_entry );

  const auto receipt_entries = expected_node->get_delta_entries();
  const auto expected_root   = pending_root_string( expected_node );

  assert( receipt_entries.size() == 3 );

  const auto* old_order =
    find_delta_entry( receipt_entries, "active/by_votes/0000000100/project/0000000007" );
  const auto* transient_order =
    find_delta_entry( receipt_entries, "active/by_votes/0000000175/project/0000000007" );
  const auto* final_order =
    find_delta_entry( receipt_entries, "active/by_votes/0000000250/project/0000000007" );

  assert( old_order );
  assert( transient_order );
  assert( final_order );
  assert( !old_order->has_value() );
  assert( !transient_order->has_value() );
  assert( final_order->has_value() );
  assert( final_order->value() == final_order_entry );
  assert( chain_space( final_order->object_space() ) == space );

  auto normal_replay_node = parent->create_anonymous_node();
  apply_delta_entries( normal_replay_node, receipt_entries, false );
  assert( pending_root_string( normal_replay_node ) != expected_root );

  auto preserved_tombstone_replay_node = parent->create_anonymous_node();
  apply_delta_entries( preserved_tombstone_replay_node, receipt_entries, true );
  assert( pending_root_string( preserved_tombstone_replay_node ) == expected_root );
}

} // namespace

int main()
{
  test_apply_block_delta_rejects_parent_state_merkle_mismatch();
  test_apply_block_delta_rejects_receipt_state_merkle_mismatch();
  test_apply_block_delta_preserves_absent_remove_tombstone();
  test_transient_contract_state_delta_requires_preserved_tombstone();
  test_kfs_project_order_delta_requires_preserved_tombstone();
  return 0;
}
