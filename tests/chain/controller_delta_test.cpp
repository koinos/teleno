#include "koinos/chain/controller.hpp"
#include "koinos/chain/rectify.hpp"
#include "koinos/chain/state.hpp"
#include "koinos/state_db/backends/map/map_backend.hpp"
#include "koinos/state_db/state_db.hpp"

#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/merkle_tree.hpp>
#include <koinos/protocol/protocol.pb.h>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>
#include <koinos/varint.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace koinos;

namespace {

const std::string kfs_contract_id = "1A5BmMqV5jN5zBrdkhQumAfDZBzXLPBeN9";

crypto::private_key block_signing_key()
{
  return crypto::private_key::regenerate(
    crypto::hash( crypto::multicodec::sha2_256, std::string( "teleno checked replay test seed" ) ) );
}

chain::genesis_data make_genesis()
{
  chain::genesis_data genesis;
  auto* entry = genesis.add_entries();
  *entry->mutable_space() = chain::state::space::metadata();
  entry->set_key( chain::state::key::genesis_key );
  entry->set_value( block_signing_key().get_public_key().to_address_bytes() );

  chain::resource_limit_data resource_limits;
  resource_limits.set_disk_storage_cost( 10 );
  resource_limits.set_disk_storage_limit( 409'600 );
  resource_limits.set_network_bandwidth_cost( 5 );
  resource_limits.set_network_bandwidth_limit( 1'048'576 );
  resource_limits.set_compute_bandwidth_cost( 1 );
  resource_limits.set_compute_bandwidth_limit( 100'000'000 );

  entry = genesis.add_entries();
  *entry->mutable_space() = chain::state::space::metadata();
  entry->set_key( chain::state::key::resource_limit_data );
  entry->set_value( util::converter::as< std::string >( resource_limits ) );

  chain::max_account_resources max_resources;
  max_resources.set_value( 10'000'000 );
  entry = genesis.add_entries();
  *entry->mutable_space() = chain::state::space::metadata();
  entry->set_key( chain::state::key::max_account_resources );
  entry->set_value( util::converter::as< std::string >( max_resources ) );

  const std::map< std::string, uint64_t > thunk_compute = {
    { "apply_block", 16'465 },
    { "apply_call_contract_operation", 685 },
    { "apply_set_system_call_operation", 136'081 },
    { "apply_set_system_contract_operation", 8'692 },
    { "apply_transaction", 12'542 },
    { "apply_upload_contract_operation", 3'130 },
    { "call", 3'573 },
    { "check_authority", 12'653 },
    { "check_system_authority", 12'750 },
    { "consume_account_rc", 735 },
    { "consume_block_resources", 753 },
    { "deserialize_message_per_byte", 1 },
    { "deserialize_multihash_base", 102 },
    { "deserialize_multihash_per_byte", 404 },
    { "event", 1'222 },
    { "event_per_impacted", 101 },
    { "exit", 11'636 },
    { "get_account_nonce", 821 },
    { "get_account_rc", 1'072 },
    { "get_arguments", 809 },
    { "get_block", 1'134 },
    { "get_block_field", 1'417 },
    { "get_caller", 825 },
    { "get_chain_id", 1'046 },
    { "get_contract_id", 778 },
    { "get_head_info", 2'099 },
    { "get_last_irreversible_block", 772 },
    { "get_next_object", 11'181 },
    { "get_object", 1'054 },
    { "get_operation", 1'081 },
    { "get_prev_object", 15'445 },
    { "get_resource_limits", 1'227 },
    { "get_transaction", 1'584 },
    { "get_transaction_field", 1'530 },
    { "hash", 1'570 },
    { "keccak_256_base", 1'406 },
    { "keccak_256_per_byte", 1 },
    { "log", 738 },
    { "object_serialization_per_byte", 1 },
    { "post_block_callback", 741 },
    { "post_transaction_callback", 721 },
    { "pre_block_callback", 730 },
    { "pre_transaction_callback", 729 },
    { "process_block_signature", 4'499 },
    { "put_object", 1'057 },
    { "recover_public_key", 29'630 },
    { "remove_object", 893 },
    { "ripemd_160_base", 1'343 },
    { "ripemd_160_per_byte", 1 },
    { "set_account_nonce", 749 },
    { "sha1_base", 1'151 },
    { "sha1_per_byte", 1 },
    { "sha2_256_base", 1'385 },
    { "sha2_256_per_byte", 1 },
    { "sha2_512_base", 1'445 },
    { "sha2_512_per_byte", 1 },
    { "verify_account_nonce", 822 },
    { "verify_merkle_root", 1 },
    { "verify_signature", 762 },
    { "verify_vrf_proof", 144'067 },
  };

  chain::compute_bandwidth_registry registry;
  for( const auto& [ name, compute ]: thunk_compute )
  {
    auto* compute_entry = registry.add_entries();
    compute_entry->set_name( name );
    compute_entry->set_compute( compute );
  }
  entry = genesis.add_entries();
  *entry->mutable_space() = chain::state::space::metadata();
  entry->set_key( chain::state::key::compute_bandwidth_registry );
  entry->set_value( util::converter::as< std::string >( registry ) );

  entry = genesis.add_entries();
  *entry->mutable_space() = chain::state::space::metadata();
  entry->set_key( chain::state::key::block_hash_code );
  entry->set_value( util::converter::as< std::string >(
    unsigned_varint{ std::underlying_type_t< crypto::multicodec >( crypto::multicodec::sha2_256 ) } ) );
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
  block.mutable_header()->set_timestamp( std::chrono::duration_cast< std::chrono::milliseconds >(
                                           std::chrono::system_clock::now().time_since_epoch() )
                                           .count() );
  block.set_id( util::converter::as< std::string >(
    crypto::hash( crypto::multicodec::sha2_256, block.header() ) ) );
  return block;
}

protocol::block make_signed_block( uint64_t height,
                                   const std::string& previous,
                                   const std::string& previous_root,
                                   uint64_t timestamp )
{
  protocol::block block;
  block.mutable_header()->set_height( height );
  block.mutable_header()->set_previous( previous );
  block.mutable_header()->set_previous_state_merkle_root( previous_root );
  block.mutable_header()->set_timestamp( timestamp );

  const std::vector< crypto::multihash > transaction_hashes;
  block.mutable_header()->set_transaction_merkle_root( util::converter::as< std::string >(
    crypto::merkle_tree( crypto::multicodec::sha2_256, transaction_hashes ).root()->hash() ) );
  block.set_id( util::converter::as< std::string >(
    crypto::hash( crypto::multicodec::sha2_256, block.header() ) ) );

  auto signer = block_signing_key();
  block.set_signature( util::converter::as< std::string >(
    signer.sign_compact( util::converter::to< crypto::multihash >( block.id() ) ) ) );
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

  controller.apply_block_delta( block, receipt, 1 );
  assert( controller.get_head_info().head_topology().id() == block.id() );
  assert( controller.get_head_info().head_state_merkle_root() == receipt.state_merkle_root() );
}

void test_exact_historical_root_exception()
{
  const auto block_id = util::from_hex< std::string >(
    "0x1220a97d7b0567ad55e3b04446a2bef447335cfd676668b069544b04a4719146d586" );
  const auto honest_root = util::from_hex< std::string >(
    "0x12203a22d59290a838dd49c87f57fe80319636950948f6b9aaf02287c03bb36e5f68" );
  const auto signed_root = util::from_hex< std::string >(
    "0x12209948b54dee01acd8528cf15dec02366b76e7739aedaf4487859bf6d0d182d690" );

  assert( chain::acceptable_rectified_previous_root( block_id, honest_root, signed_root ) );

  auto changed_block = block_id;
  changed_block.back() ^= 0x01;
  assert( !chain::acceptable_rectified_previous_root( changed_block, honest_root, signed_root ) );

  auto changed_honest_root = honest_root;
  changed_honest_root.back() ^= 0x01;
  assert( !chain::acceptable_rectified_previous_root( block_id, changed_honest_root, signed_root ) );

  auto changed_signed_root = signed_root;
  changed_signed_root.back() ^= 0x01;
  assert( !chain::acceptable_rectified_previous_root( block_id, honest_root, changed_signed_root ) );
}

protocol::block_receipt tampered_receipt( protocol::block_receipt receipt )
{
  receipt.clear_state_merkle_root();
  auto* extra_remove = receipt.add_state_delta_entries();
  extra_remove->mutable_object_space()->set_system( true );
  extra_remove->set_key( "checked-replay-extra-remove" );
  return receipt;
}

void test_checked_replay_clean_fallback_and_halt()
{
  chain::controller source;
  open_controller( source );

  const auto now = std::chrono::duration_cast< std::chrono::milliseconds >(
                     std::chrono::system_clock::now().time_since_epoch() )
                     .count();
  const auto zero_id = util::converter::as< std::string >(
    crypto::multihash::zero( crypto::multicodec::sha2_256 ) );

  const auto block_1 = make_signed_block(
    1,
    zero_id,
    source.get_head_info().head_state_merkle_root(),
    now );
  rpc::chain::submit_block_request request_1;
  *request_1.mutable_block() = block_1;
  const auto receipt_1       = source.submit_block( request_1, 2 ).receipt();
  assert( !receipt_1.state_merkle_root().empty() );

  const auto block_2 = make_signed_block(
    2,
    block_1.id(),
    source.get_head_info().head_state_merkle_root(),
    now + 1'000 );
  rpc::chain::submit_block_request request_2;
  *request_2.mutable_block() = block_2;
  const auto receipt_2       = source.submit_block( request_2, 2 ).receipt();
  assert( !receipt_2.state_merkle_root().empty() );

  chain::controller replay;
  open_controller( replay );
  assert( !replay.apply_block_delta_checked( block_1, receipt_1, receipt_1.state_merkle_root(), 2 ) );
  assert( replay.get_head_info().head_topology().id() == block_1.id() );
  assert( replay.get_head_info().head_state_merkle_root() == receipt_1.state_merkle_root() );

  assert( replay.apply_block_delta_checked(
    block_2,
    tampered_receipt( receipt_2 ),
    receipt_2.state_merkle_root(),
    2 ) );
  assert( replay.get_head_info().head_topology().id() == block_2.id() );
  assert( replay.get_head_info().head_state_merkle_root() == receipt_2.state_merkle_root() );

  chain::controller halt;
  open_controller( halt );
  const auto unreachable_root = util::converter::as< std::string >(
    crypto::hash( crypto::multicodec::sha2_256, std::string( "unreachable checked replay root" ) ) );
  assert_throws_with(
    [&]() {
      halt.apply_block_delta_checked( block_1, tampered_receipt( receipt_1 ), unreachable_root, 2 );
    },
    "re-executed block does not reproduce the consensus state merkle root" );
  assert( halt.get_head_info().head_topology().height() == 0 );
  assert( halt.get_head_info().head_topology().id() == zero_id );
  assert( halt.get_head_info().head_state_merkle_root() == genesis_merkle_root() );

  // The rejected writable node must be gone and the last validated parent
  // must remain usable for a later correct attempt.
  assert( !halt.apply_block_delta_checked(
    block_1,
    receipt_1,
    receipt_1.state_merkle_root(),
    2 ) );
  assert( halt.get_head_info().head_topology().id() == block_1.id() );
  assert( halt.get_head_info().head_state_merkle_root() == receipt_1.state_merkle_root() );
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
  const auto final_order_space = chain_space( final_order->object_space() );
  assert( final_order_space.system() == space.system() );
  assert( final_order_space.zone() == space.zone() );
  assert( final_order_space.id() == space.id() );

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
  test_exact_historical_root_exception();
  test_checked_replay_clean_fallback_and_halt();
  test_transient_contract_state_delta_requires_preserved_tombstone();
  test_kfs_project_order_delta_requires_preserved_tombstone();
  return 0;
}
