#include "core/monolith_client.hpp"
#include "koinos/chain/controller.hpp"
#include "koinos/chain/indexer.hpp"
#include "koinos/chain/state.hpp"
#include "koinos/state_db/backends/map/map_backend.hpp"

#include <koinos/block_store/block_store.pb.h>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/merkle_tree.hpp>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/util/conversion.hpp>
#include <koinos/varint.hpp>

#include <boost/asio/io_context.hpp>

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace koinos;

namespace {

crypto::private_key block_signing_key()
{
  return crypto::private_key::regenerate(
    crypto::hash( crypto::multicodec::sha2_256, std::string( "teleno indexer lookahead test seed" ) ) );
}

chain::genesis_data make_genesis()
{
  chain::genesis_data genesis;
  auto add_metadata = [&]( const std::string& key, const std::string& value ) {
    auto* entry = genesis.add_entries();
    *entry->mutable_space() = chain::state::space::metadata();
    entry->set_key( key );
    entry->set_value( value );
  };

  add_metadata( chain::state::key::genesis_key, block_signing_key().get_public_key().to_address_bytes() );

  chain::resource_limit_data resource_limits;
  resource_limits.set_disk_storage_cost( 10 );
  resource_limits.set_disk_storage_limit( 409'600 );
  resource_limits.set_network_bandwidth_cost( 5 );
  resource_limits.set_network_bandwidth_limit( 1'048'576 );
  resource_limits.set_compute_bandwidth_cost( 1 );
  resource_limits.set_compute_bandwidth_limit( 100'000'000 );
  add_metadata( chain::state::key::resource_limit_data,
                util::converter::as< std::string >( resource_limits ) );

  chain::max_account_resources max_resources;
  max_resources.set_value( 10'000'000 );
  add_metadata( chain::state::key::max_account_resources,
                util::converter::as< std::string >( max_resources ) );

  const std::map< std::string, uint64_t > thunk_compute = {
    { "apply_block", 16'465 },
    { "consume_account_rc", 735 },
    { "consume_block_resources", 753 },
    { "deserialize_multihash_base", 102 },
    { "deserialize_multihash_per_byte", 404 },
    { "get_head_info", 2'099 },
    { "get_last_irreversible_block", 772 },
    { "get_object", 1'054 },
    { "get_resource_limits", 1'227 },
    { "hash", 1'570 },
    { "object_serialization_per_byte", 1 },
    { "post_block_callback", 741 },
    { "pre_block_callback", 730 },
    { "process_block_signature", 4'499 },
    { "recover_public_key", 29'630 },
    { "sha2_256_base", 1'385 },
    { "sha2_256_per_byte", 1 },
    { "verify_merkle_root", 1 },
  };

  chain::compute_bandwidth_registry registry;
  for( const auto& [ name, compute ]: thunk_compute )
  {
    auto* entry = registry.add_entries();
    entry->set_name( name );
    entry->set_compute( compute );
  }
  add_metadata( chain::state::key::compute_bandwidth_registry,
                util::converter::as< std::string >( registry ) );
  add_metadata( chain::state::key::block_hash_code,
                util::converter::as< std::string >(
                  unsigned_varint{
                    std::underlying_type_t< crypto::multicodec >( crypto::multicodec::sha2_256 ) } ) );
  return genesis;
}

void open_controller( chain::controller& controller )
{
  controller.open(
    std::make_shared< state_db::backends::map::map_backend >(),
    make_genesis(),
    chain::fork_resolution_algorithm::fifo,
    false );
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
  const std::vector< crypto::multihash > hashes;
  block.mutable_header()->set_transaction_merkle_root( util::converter::as< std::string >(
    crypto::merkle_tree( crypto::multicodec::sha2_256, hashes ).root()->hash() ) );
  block.set_id( util::converter::as< std::string >(
    crypto::hash( crypto::multicodec::sha2_256, block.header() ) ) );
  auto signer = block_signing_key();
  block.set_signature( util::converter::as< std::string >(
    signer.sign_compact( util::converter::to< crypto::multihash >( block.id() ) ) ) );
  return block;
}

std::vector< block_store::block_item > make_executed_chain( uint64_t count )
{
  chain::controller source;
  open_controller( source );
  std::vector< block_store::block_item > items;
  items.reserve( count );

  auto previous = util::converter::as< std::string >(
    crypto::multihash::zero( crypto::multicodec::sha2_256 ) );
  const auto start = std::chrono::duration_cast< std::chrono::milliseconds >(
                       std::chrono::system_clock::now().time_since_epoch() )
                       .count();

  for( uint64_t height = 1; height <= count; ++height )
  {
    auto block = make_signed_block(
      height,
      previous,
      source.get_head_info().head_state_merkle_root(),
      start + height );
    rpc::chain::submit_block_request request;
    *request.mutable_block() = block;
    auto receipt             = source.submit_block( request, count ).receipt();

    block_store::block_item item;
    *item.mutable_block()   = block;
    *item.mutable_receipt() = receipt;
    items.push_back( std::move( item ) );
    previous = block.id();
  }

  return items;
}

class fake_block_store_client final: public node::IRpcClient
{
public:
  fake_block_store_client( block_topology target, std::vector< block_store::block_item > items ):
      _target( std::move( target ) ),
      _items( std::move( items ) )
  {}

  std::shared_future< std::string >
  rpc( const std::string&,
       const std::string& payload,
       std::chrono::milliseconds,
       retry_policy ) override
  {
    rpc::block_store::block_store_request request;
    if( !request.ParseFromString( payload ) )
      throw std::runtime_error( "invalid fake block-store request" );

    rpc::block_store::block_store_response response;
    if( request.has_get_highest_block() )
    {
      *response.mutable_get_highest_block()->mutable_topology() = _target;
    }
    else if( request.has_get_blocks_by_height() )
    {
      ++batch_requests;
      if( on_blocks_request )
        on_blocks_request();
      const auto& by_height = request.get_blocks_by_height();
      const auto end = by_height.ancestor_start_height() + by_height.num_blocks();
      for( const auto& item: _items )
      {
        const auto height = item.block().header().height();
        if( height >= by_height.ancestor_start_height() && height < end )
          *response.mutable_get_blocks_by_height()->add_block_items() = item;
      }
    }
    else if( request.has_add_block() )
    {
      ++add_block_requests;
      response.mutable_add_block();
    }
    else
    {
      throw std::runtime_error( "unexpected fake block-store request" );
    }

    std::promise< std::string > promise;
    promise.set_value( response.SerializeAsString() );
    return promise.get_future().share();
  }

  void broadcast( const std::string&, const std::string& ) override
  {
    ++broadcasts;
  }

  uint64_t batch_requests    = 0;
  uint64_t add_block_requests = 0;
  uint64_t broadcasts        = 0;
  std::function< void() > on_blocks_request;

private:
  block_topology _target;
  std::vector< block_store::block_item > _items;
};

block_topology topology_for( const std::vector< block_store::block_item >& items )
{
  block_topology topology;
  if( items.empty() )
  {
    topology.set_id( util::converter::as< std::string >(
      crypto::multihash::zero( crypto::multicodec::sha2_256 ) ) );
    return topology;
  }
  topology.set_id( items.back().block().id() );
  topology.set_height( items.back().block().header().height() );
  topology.set_previous( items.back().block().header().previous() );
  return topology;
}

bool run_indexer( chain::indexer& indexer, boost::asio::io_context& ioc, std::size_t worker_count = 4 )
{
  auto result = indexer.index();
  std::vector< std::thread > workers;
  std::mutex worker_error_mutex;
  std::exception_ptr worker_error;
  for( std::size_t i = 0; i < worker_count; ++i )
  {
    workers.emplace_back( [&]() {
      try
      {
        ioc.run();
      }
      catch( ... )
      {
        std::lock_guard< std::mutex > lock( worker_error_mutex );
        if( !worker_error )
          worker_error = std::current_exception();
        ioc.stop();
      }
    } );
  }

  const auto status = result.wait_for( std::chrono::seconds( 10 ) );
  ioc.stop();
  for( auto& worker: workers )
    worker.join();

  if( worker_error )
    std::rethrow_exception( worker_error );
  if( status != std::future_status::ready )
    throw std::runtime_error( "indexer did not complete within the test timeout" );
  return result.get();
}

void test_cancellation_does_not_finalize_pending_item()
{
  // Hold the second block-store request while cancellation is issued from a
  // separate thread. stop() must wait for any in-flight application and no
  // pending item may advance the head after stop() returns.
  auto items = make_executed_chain( 51 );
  chain::controller controller;
  open_controller( controller );
  boost::asio::io_context ioc;
  auto client = std::make_shared< fake_block_store_client >( topology_for( items ), items );
  chain::indexer indexer( ioc, controller, client, false );
  std::mutex gate_mutex;
  std::condition_variable gate_cv;
  bool second_request  = false;
  bool release_request = false;
  client->on_blocks_request = [&]() {
    if( client->batch_requests == 2 )
    {
      std::unique_lock< std::mutex > lock( gate_mutex );
      second_request = true;
      gate_cv.notify_one();
      gate_cv.wait( lock, [&]() { return release_request; } );
    }
  };

  auto result = indexer.index();
  std::thread worker( [&]() { ioc.run(); } );
  {
    std::unique_lock< std::mutex > lock( gate_mutex );
    assert( gate_cv.wait_for( lock, std::chrono::seconds( 10 ), [&]() { return second_request; } ) );
  }

  indexer.stop();
  const auto head_at_stop = controller.get_head_info().head_topology();
  {
    std::lock_guard< std::mutex > lock( gate_mutex );
    release_request = true;
  }
  gate_cv.notify_one();

  assert( result.wait_for( std::chrono::seconds( 10 ) ) == std::future_status::ready );
  assert( !result.get() );
  ioc.stop();
  worker.join();

  const auto final_head = controller.get_head_info().head_topology();
  assert( final_head.height() == head_at_stop.height() );
  assert( final_head.id() == head_at_stop.id() );
  assert( final_head.height() < items.back().block().header().height() );
  assert( client->add_block_requests == 0 );
  assert( client->broadcasts == 0 );
}

void test_zero_and_one_missing_block()
{
  {
    chain::controller controller;
    open_controller( controller );
    boost::asio::io_context ioc;
    auto client = std::make_shared< fake_block_store_client >( controller.get_head_info().head_topology(),
                                                               std::vector< block_store::block_item >{} );
    chain::indexer indexer( ioc, controller, client, false );
    assert( run_indexer( indexer, ioc ) );
    assert( controller.get_head_info().head_topology().height() == 0 );
    assert( client->batch_requests == 0 );
  }

  {
    auto items = make_executed_chain( 1 );
    chain::controller controller;
    open_controller( controller );
    boost::asio::io_context ioc;
    auto client = std::make_shared< fake_block_store_client >( topology_for( items ), items );
    chain::indexer indexer( ioc, controller, client, false );
    assert( run_indexer( indexer, ioc ) );
    assert( controller.get_head_info().head_topology().height() == 1 );
    assert( indexer.fallback_count() == 0 );
  }
}

void test_multiple_batches_preserve_order_and_apply_target_once()
{
  auto items = make_executed_chain( 55 );
  chain::controller controller;
  open_controller( controller );
  boost::asio::io_context ioc;
  auto client = std::make_shared< fake_block_store_client >( topology_for( items ), items );
  chain::indexer indexer( ioc, controller, client, false );
  assert( run_indexer( indexer, ioc ) );
  assert( client->batch_requests == 2 );
  assert( client->add_block_requests == 0 );
  assert( client->broadcasts == 0 );
  assert( controller.get_head_info().head_topology().height() == 55 );
  assert( controller.get_head_info().head_topology().id() == items.back().block().id() );
}

void test_single_worker_preserves_backpressure_across_large_batches()
{
  // The request batch grows beyond the bounded block queue. A single
  // io_context worker must keep draining the staged response instead of
  // blocking while trying to enqueue the whole response at once.
  auto items = make_executed_chain( 275 );
  chain::controller controller;
  open_controller( controller );
  boost::asio::io_context ioc;
  auto client = std::make_shared< fake_block_store_client >( topology_for( items ), items );
  chain::indexer indexer( ioc, controller, client, false );
  assert( run_indexer( indexer, ioc, 1 ) );
  assert( client->batch_requests == 3 );
  assert( client->add_block_requests == 0 );
  assert( client->broadcasts == 0 );
  assert( controller.get_head_info().head_topology().height() == 275 );
  assert( controller.get_head_info().head_topology().id() == items.back().block().id() );
}

void test_intermediate_fallback_and_unrecoverable_stop()
{
  auto clean_items = make_executed_chain( 3 );
  auto fallback_items = clean_items;
  fallback_items[ 1 ].mutable_receipt()->clear_state_merkle_root();
  auto* extra_remove = fallback_items[ 1 ].mutable_receipt()->add_state_delta_entries();
  extra_remove->mutable_object_space()->set_system( true );
  extra_remove->set_key( "indexer-fallback-extra-remove" );

  {
    chain::controller controller;
    open_controller( controller );
    boost::asio::io_context ioc;
    auto client = std::make_shared< fake_block_store_client >( topology_for( fallback_items ), fallback_items );
    controller.set_client( client );
    chain::indexer indexer( ioc, controller, client, false );
    assert( run_indexer( indexer, ioc ) );
    assert( indexer.fallback_count() == 1 );
    assert( client->add_block_requests == 0 );
    assert( client->broadcasts == 0 );
    assert( controller.get_head_info().head_topology().height() == 3 );
    assert( controller.get_head_info().head_topology().id() == clean_items.back().block().id() );
  }

  {
    auto halt_items = fallback_items;
    halt_items[ 2 ].mutable_block()->mutable_header()->set_previous_state_merkle_root(
      util::converter::as< std::string >(
        crypto::hash( crypto::multicodec::sha2_256, std::string( "unreachable indexer root" ) ) ) );

    chain::controller controller;
    open_controller( controller );
    boost::asio::io_context ioc;
    auto client = std::make_shared< fake_block_store_client >( topology_for( halt_items ), halt_items );
    controller.set_client( client );
    chain::indexer indexer( ioc, controller, client, false );

    bool threw = false;
    try
    {
      static_cast< void >( run_indexer( indexer, ioc ) );
    }
    catch( const chain::indexer_failure_exception& e )
    {
      threw = std::string( e.what() ).find( "re-executed block does not reproduce" ) != std::string::npos;
    }
    assert( threw );
    assert( client->add_block_requests == 0 );
    assert( client->broadcasts == 0 );
    assert( controller.get_head_info().head_topology().height() == 1 );
    assert( controller.get_head_info().head_topology().id() == clean_items[ 0 ].block().id() );
  }
}

} // namespace

int main()
{
  test_zero_and_one_missing_block();
  test_multiple_batches_preserve_order_and_apply_target_once();
  test_single_worker_preserves_backpressure_across_large_batches();
  test_intermediate_fallback_and_unrecoverable_stop();
  test_cancellation_does_not_finalize_pending_item();
  return 0;
}
