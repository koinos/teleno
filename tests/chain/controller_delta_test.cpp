#include "koinos/chain/controller.hpp"
#include "koinos/chain/state.hpp"
#include "koinos/state_db/backends/map/map_backend.hpp"
#include "koinos/state_db/state_db.hpp"

#include <koinos/crypto/merkle_tree.hpp>
#include <koinos/protocol/protocol.pb.h>
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

protocol::state_delta_entry make_delta_entry()
{
  protocol::state_delta_entry entry;
  *entry.mutable_object_space() = protocol_space( chain::state::space::metadata() );
  entry.set_key( "test-state-key" );
  entry.set_value( "test-state-value" );
  return entry;
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

std::string state_delta_merkle_root( const protocol::state_delta_entry& entry )
{
  return merkle_root_for_entries( {
    {
      serialized_database_key( chain_space( entry.object_space() ), entry.key() ),
      entry.has_value() ? entry.value() : std::string()
    }
  } );
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

protocol::block_receipt make_receipt( const protocol::block& block, const protocol::state_delta_entry& entry )
{
  protocol::block_receipt receipt;
  receipt.set_id( block.id() );
  receipt.set_height( block.header().height() );
  *receipt.add_state_delta_entries() = entry;
  receipt.set_state_merkle_root( state_delta_merkle_root( entry ) );
  return receipt;
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

} // namespace

int main()
{
  test_apply_block_delta_rejects_parent_state_merkle_mismatch();
  test_apply_block_delta_rejects_receipt_state_merkle_mismatch();
  test_apply_block_delta_preserves_absent_remove_tombstone();
  return 0;
}
