#include "core/event_bus.hpp"
#include "core/monolith_rpc_client.hpp"
#include "mempool/mempool_adapter.hpp"

#include <koinos/chain/value.pb.h>
#include <koinos/mempool/mempool.hpp>
#include <koinos/protocol/protocol.pb.h>
#include <koinos/rpc/mempool/mempool_rpc.pb.h>
#include <koinos/util/conversion.hpp>
#include <koinos/util/services.hpp>

#include <cassert>
#include <atomic>
#include <barrier>
#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <sys/resource.h>

using namespace koinos;
using namespace koinos::node;

namespace {

std::string nonce_bytes( uint64_t value )
{
  chain::value_type nonce;
  nonce.set_uint64_value( value );
  return util::converter::as< std::string >( nonce );
}

protocol::transaction make_transaction( const std::string& id,
                                        const std::string& payer,
                                        uint64_t nonce,
                                        uint64_t rc_limit )
{
  protocol::transaction transaction;
  transaction.set_id( id );
  transaction.mutable_header()->set_payer( payer );
  transaction.mutable_header()->set_nonce( nonce_bytes( nonce ) );
  transaction.mutable_header()->set_rc_limit( rc_limit );
  transaction.mutable_header()->set_chain_id( "chain-id" );
  transaction.add_signatures( "signature-" + id );
  return transaction;
}

broadcast::transaction_accepted make_accepted_transaction(
  const std::string& id,
  const std::string& payer,
  uint64_t nonce,
  uint64_t rc_limit,
  uint64_t max_payer_rc )
{
  broadcast::transaction_accepted accepted;
  *accepted.mutable_transaction() = make_transaction( id, payer, nonce, rc_limit );
  accepted.mutable_receipt()->set_max_payer_rc( max_payer_rc );
  accepted.mutable_receipt()->set_rc_limit( rc_limit );
  accepted.mutable_receipt()->set_disk_storage_used( 11 );
  accepted.mutable_receipt()->set_network_bandwidth_used( 13 );
  accepted.mutable_receipt()->set_compute_bandwidth_used( 17 );
  accepted.set_system_disk_storage_used( 2 );
  accepted.set_system_network_bandwidth_used( 3 );
  accepted.set_system_compute_bandwidth_used( 5 );
  return accepted;
}

rpc::mempool::mempool_response call_mempool(
  MonolithRpcClient& client,
  const rpc::mempool::mempool_request& request )
{
  auto future = client.rpc( util::service::mempool, request.SerializeAsString() );
  rpc::mempool::mempool_response response;
  response.ParseFromString( future.get() );
  return response;
}

void test_pending_resources_and_expiration()
{
  koinos::mempool::mempool impl;
  MempoolAdapter adapter( impl );
  EventBus event_bus;

  event_bus.on_transaction_accepted.connect(
    [&adapter]( const broadcast::transaction_accepted& accepted ) {
      adapter.add_transaction_accepted(
        accepted,
        std::chrono::system_clock::time_point{ std::chrono::seconds{ 1'000 } } );
    }
  );

  const std::string payer = "payer-account";
  auto accepted = make_accepted_transaction( "tx-1", payer, 7, 400, 1'000 );
  event_bus.on_transaction_accepted( accepted );
  // P2P's eviction fallback is exact current ID membership, not a nonce guess.
  assert( impl.has_pending_transaction( "tx-1" ) );
  assert( !impl.has_pending_transaction( "tx-2" ) );

  rpc::mempool::get_pending_transactions_request pending_req;
  auto pending = adapter.get_pending_transactions( pending_req );
  assert( pending.pending_transactions_size() == 1 );
  assert( pending.pending_transactions( 0 ).transaction().id() == "tx-1" );
  assert( pending.pending_transactions( 0 ).disk_storage_used() == 11 );
  assert( pending.pending_transactions( 0 ).system_compute_bandwidth_used() == 5 );

  rpc::mempool::get_reserved_account_rc_request reserved_req;
  reserved_req.set_account( payer );
  auto reserved = adapter.get_reserved_account_rc( reserved_req );
  assert( reserved.rc() == 400 );

  rpc::mempool::check_pending_account_resources_request resources_req;
  resources_req.set_payer( payer );
  resources_req.set_max_payer_rc( 1'000 );
  resources_req.set_rc_limit( 600 );
  assert( adapter.check_pending_account_resources( resources_req ).success() );

  resources_req.set_rc_limit( 601 );
  assert( !adapter.check_pending_account_resources( resources_req ).success() );

  rpc::mempool::check_account_nonce_request nonce_req;
  nonce_req.set_payee( payer );
  nonce_req.set_nonce( nonce_bytes( 7 ) );
  assert( !adapter.check_account_nonce( nonce_req ).success() );

  nonce_req.set_nonce( nonce_bytes( 8 ) );
  assert( adapter.check_account_nonce( nonce_req ).success() );

  rpc::mempool::get_pending_nonce_request pending_nonce_req;
  pending_nonce_req.set_payee( payer );
  auto pending_nonce = adapter.get_pending_nonce( pending_nonce_req );
  assert( pending_nonce.nonce() == nonce_bytes( 7 ) );

  rpc::mempool::get_pending_transaction_count_request count_req;
  count_req.set_payee( payer );
  assert( adapter.get_pending_transaction_count( count_req ).count() == 1 );

  const auto inserted_at = std::chrono::system_clock::time_point{ std::chrono::seconds{ 1'000 } };
  assert( adapter.prune( std::chrono::seconds{ 120 }, inserted_at + std::chrono::seconds{ 119 } ) == 0 );
  assert( adapter.get_pending_transactions( pending_req ).pending_transactions_size() == 1 );

  assert( impl.has_pending_transaction( "tx-1" ) );

  assert( adapter.prune( std::chrono::seconds{ 120 }, inserted_at + std::chrono::seconds{ 120 } ) == 1 );
  assert( !impl.has_pending_transaction( "tx-1" ) );
  assert( adapter.get_pending_transactions( pending_req ).pending_transactions_size() == 0 );
  assert( adapter.get_reserved_account_rc( reserved_req ).rc() == 0 );
  assert( adapter.get_pending_transaction_count( count_req ).count() == 0 );
  resources_req.set_rc_limit( 1'000 );
  assert( adapter.check_pending_account_resources( resources_req ).success() );
}

void test_monolith_rpc_client_mempool_methods()
{
  koinos::mempool::mempool impl;
  MempoolAdapter adapter( impl );
  MonolithRpcClient client( nullptr, &adapter, nullptr );

  const std::string payer = "rpc-payer";
  adapter.add_transaction_accepted(
    make_accepted_transaction( "tx-rpc", payer, 3, 900, 1'000 ),
    std::chrono::system_clock::now() );

  rpc::mempool::mempool_request req;
  auto* resources = req.mutable_check_pending_account_resources();
  resources->set_payer( payer );
  resources->set_max_payer_rc( 1'000 );
  resources->set_rc_limit( 101 );
  auto resources_resp = call_mempool( client, req );
  assert( resources_resp.has_check_pending_account_resources() );
  assert( !resources_resp.check_pending_account_resources().success() );

  req.Clear();
  auto* nonce = req.mutable_check_account_nonce();
  nonce->set_payee( payer );
  nonce->set_nonce( nonce_bytes( 3 ) );
  auto nonce_resp = call_mempool( client, req );
  assert( nonce_resp.has_check_account_nonce() );
  assert( !nonce_resp.check_account_nonce().success() );

  req.Clear();
  req.mutable_get_pending_nonce()->set_payee( payer );
  auto pending_nonce_resp = call_mempool( client, req );
  assert( pending_nonce_resp.has_get_pending_nonce() );
  assert( pending_nonce_resp.get_pending_nonce().nonce() == nonce_bytes( 3 ) );

  req.Clear();
  req.mutable_get_pending_transaction_count()->set_payee( payer );
  auto count_resp = call_mempool( client, req );
  assert( count_resp.has_get_pending_transaction_count() );
  assert( count_resp.get_pending_transaction_count().count() == 1 );
}

// Characterization baseline for LR2 M1: a response-size limit is not a
// retained-record or byte budget. These synthetic library inputs bypass chain
// execution and must not be described as mainnet-valid signed transactions.
void test_request_limit_does_not_bound_global_retention()
{
  koinos::mempool::mempool impl;
  MempoolAdapter adapter( impl );
  const auto inserted_at = std::chrono::system_clock::time_point{ std::chrono::seconds{ 1'000 } };
  const uint64_t records = koinos::mempool::constants::max_request_limit + 1;
  assert( impl.pending_transaction_count() == 0 );
  auto suffix = []( uint64_t i ) {
    auto digits = std::to_string( i );
    return std::string( 12 - digits.size(), '0' ) + digits;
  };

  for( uint64_t i = 0; i < records; ++i )
  {
    const auto id = "aggregate-tx-" + suffix( i );
    const auto payer = "aggregate-payer-" + suffix( i );
    adapter.add_transaction_accepted( make_accepted_transaction( id, payer, 7, 1, 1'000 ), inserted_at );
    assert( impl.has_pending_transaction( id ) );
    assert( impl.get_pending_transaction_count( payer ) == 1 );
  }

  rpc::mempool::get_pending_transactions_request request;
  assert( impl.pending_transaction_count() == records );
  // The previous native metrics expression always returned zero: limit=0
  // requests no records. Correct diagnostics cannot use that expression.
  assert( impl.get_pending_transactions( 0 ).empty() );
  const auto response = adapter.get_pending_transactions( request );
  assert( response.pending_transactions_size() == koinos::mempool::constants::max_request_limit );
  const auto last_id = "aggregate-tx-" + suffix( records - 1 );
  const auto last_payer = "aggregate-payer-" + suffix( records - 1 );
  const auto last = impl.get_pending_transactions( std::vector< std::string >{ last_id } );
  assert( last.size() == 1 && last.front().transaction().id() == last_id );

  request.set_limit( records );
  bool request_overflow = false;
  try { adapter.get_pending_transactions( request ); }
  catch( const koinos::mempool::pending_transaction_request_overflow& ) { request_overflow = true; }
  assert( request_overflow );
  assert( impl.has_pending_transaction( last_id ) );

  assert( adapter.prune( std::chrono::seconds{ 120 }, inserted_at + std::chrono::seconds{ 119 } ) == 0 );
  assert( adapter.prune( std::chrono::seconds{ 120 }, inserted_at + std::chrono::seconds{ 120 } ) == records );
  assert( !impl.has_pending_transaction( last_id ) );
  assert( impl.get_pending_transaction_count( last_payer ) == 0 );
  assert( impl.get_reserved_account_rc( last_payer ) == 0 );
  assert( adapter.get_pending_transactions( {} ).pending_transactions_size() == 0 );
  assert( impl.pending_transaction_count() == 0 );
  // Logical cleanup is asserted; allocator/delta/fork physical retention is
  // intentionally not inferred from an empty pending response.
}

void test_failed_native_admission_is_not_published()
{
  koinos::mempool::mempool impl;
  MempoolAdapter adapter( impl );
  adapter.add_transaction_accepted( make_accepted_transaction( "reserved", "bounded-payer", 7, 400, 1'000 ) );
  EventBus event_bus;
  size_t notifications = 0;
  event_bus.on_transaction_accepted.connect( [&]( const broadcast::transaction_accepted& ) { ++notifications; } );
  MonolithRpcClient client( nullptr, &adapter, &event_bus,
                          [&adapter]( const broadcast::transaction_accepted& accepted ) {
                            adapter.add_transaction_accepted( accepted );
                          } );
  auto rejected = make_accepted_transaction( "over-reserved", "bounded-payer", 8, 700, 1'000 );
  bool refused = false;
  try { client.broadcast( "koinos.transaction.accept", rejected.SerializeAsString() ); }
  catch( const koinos::mempool::pending_transaction_exceeds_resources& ) { refused = true; }
  assert( refused );
  assert( notifications == 0 );
  assert( !impl.has_pending_transaction( "over-reserved" ) );
  assert( impl.get_reserved_account_rc( "bounded-payer" ) == 400 );
  assert( client.transaction_notification_failures() == 0 );

  // A resource refusal must not leave nonce/index residue preventing retry.
  assert( impl.remove_pending_transactions( { "reserved" } ) == 1 );
  client.broadcast( "koinos.transaction.accept", rejected.SerializeAsString() );
  assert( notifications == 1 );
  assert( impl.has_pending_transaction( "over-reserved" ) );
  assert( impl.get_reserved_account_rc( "bounded-payer" ) == 700 );
  // Duplicate insertion is not a second accepted-work notification or RC debit.
  bool duplicate = false;
  try { client.broadcast( "koinos.transaction.accept", rejected.SerializeAsString() ); }
  catch( const koinos::mempool::pending_transaction_exceeds_resources& ) { duplicate = true; }
  catch( const koinos::mempool::pending_transaction_insertion_failure& ) { duplicate = true; }
  assert( duplicate && notifications == 1 );
  assert( impl.pending_transaction_count() == 1 );
  assert( impl.get_reserved_account_rc( "bounded-payer" ) == 700 );
}

void test_post_admission_notification_failure_is_observable()
{
  koinos::mempool::mempool impl;
  MempoolAdapter adapter( impl );
  EventBus event_bus;
  auto failing = event_bus.on_transaction_accepted.connect( [&]( const broadcast::transaction_accepted& accepted ) {
    assert( impl.has_pending_transaction( accepted.transaction().id() ) );
    throw std::runtime_error( "synthetic notification failure" );
  } );
  MonolithRpcClient client( nullptr, &adapter, &event_bus,
                          [&adapter]( const broadcast::transaction_accepted& accepted ) {
                            adapter.add_transaction_accepted( accepted );
                          } );
  client.broadcast( "koinos.transaction.accept",
                    make_accepted_transaction( "retained", "notify-payer", 1, 100, 1'000 ).SerializeAsString() );
  assert( impl.has_pending_transaction( "retained" ) );
  assert( impl.get_reserved_account_rc( "notify-payer" ) == 100 );
  assert( client.transaction_notification_failures() == 1 );
  failing.disconnect();
  size_t notified = 0;
  event_bus.on_transaction_accepted.connect( [&]( const broadcast::transaction_accepted& accepted ) {
    assert( impl.has_pending_transaction( accepted.transaction().id() ) );
    ++notified;
  } );
  client.broadcast( "koinos.transaction.accept",
                    make_accepted_transaction( "retained-next", "notify-payer", 2, 100, 1'000 ).SerializeAsString() );
  assert( notified == 1 && client.transaction_notification_failures() == 1 );
  assert( impl.get_pending_transaction_count( "notify-payer" ) == 2 );
}

void test_admission_message_and_disabled_mode()
{
  EventBus event_bus;
  size_t admissions = 0, notifications = 0;
  event_bus.on_transaction_accepted.connect( [&]( const broadcast::transaction_accepted& ) { ++notifications; } );
  MonolithRpcClient configured( nullptr, nullptr, &event_bus,
                               [&]( const broadcast::transaction_accepted& ) { ++admissions; } );
  for( const auto& malformed: { std::string{}, std::string( 1, '\xff' ) } )
  {
    bool refused = false;
    try { configured.broadcast( "koinos.transaction.accept", malformed ); }
    catch( const std::invalid_argument& ) { refused = true; }
    assert( refused && admissions == 0 && notifications == 0 );
  }

  const auto accepted = make_accepted_transaction( "disabled", "disabled-payer", 1, 100, 1'000 );
  MonolithRpcClient disabled( nullptr, nullptr, &event_bus );
  disabled.broadcast( "koinos.transaction.accept", accepted.SerializeAsString() );
  assert( admissions == 0 && notifications == 1 );
  // Nontransaction events retain their original exception propagation.
  event_bus.on_block_accepted.connect( []( const broadcast::block_accepted& ) { throw std::runtime_error( "block notification" ); } );
  bool threw = false;
  try { configured.broadcast( "koinos.block.accept", broadcast::block_accepted{}.SerializeAsString() ); }
  catch( const std::runtime_error& ) { threw = true; }
  assert( threw && configured.transaction_notification_failures() == 0 );

  koinos::mempool::mempool impl;
  MempoolAdapter adapter( impl );
  MonolithRpcClient without_observers( nullptr, &adapter, nullptr,
                                     [&adapter]( const broadcast::transaction_accepted& value ) {
                                       adapter.add_transaction_accepted( value );
                                     } );
  without_observers.broadcast( "koinos.transaction.accept", accepted.SerializeAsString() );
  assert( impl.has_pending_transaction( "disabled" ) );
}

void test_concurrent_native_resource_admission()
{
  koinos::mempool::mempool impl;
  MempoolAdapter adapter( impl );
  adapter.add_transaction_accepted( make_accepted_transaction( "initial", "concurrent-payer", 1, 400, 1'000 ) );
  EventBus event_bus;
  std::atomic< size_t > notifications{ 0 };
  event_bus.on_transaction_accepted.connect( [&]( const broadcast::transaction_accepted& accepted ) {
    assert( impl.has_pending_transaction( accepted.transaction().id() ) );
    ++notifications;
  } );
  MonolithRpcClient client( nullptr, &adapter, &event_bus,
                          [&adapter]( const broadcast::transaction_accepted& accepted ) {
                            adapter.add_transaction_accepted( accepted );
                          } );
  std::barrier start( 3 );
  auto submit = [&]( const std::string& id, uint64_t nonce ) {
    const auto bytes = make_accepted_transaction( id, "concurrent-payer", nonce, 400, 1'000 ).SerializeAsString();
    start.arrive_and_wait();
    try { client.broadcast( "koinos.transaction.accept", bytes ); return true; }
    catch( const koinos::mempool::pending_transaction_exceeds_resources& ) { return false; }
  };
  auto first = std::async( std::launch::async, submit, "race-first", 2 );
  auto second = std::async( std::launch::async, submit, "race-second", 3 );
  start.arrive_and_wait();
  const bool first_ok = first.get(), second_ok = second.get();
  assert( first_ok != second_ok );
  assert( notifications == 1 && client.transaction_notification_failures() == 0 );
  assert( impl.has_pending_transaction( "race-first" ) == first_ok );
  assert( impl.has_pending_transaction( "race-second" ) == second_ok );
  assert( impl.get_reserved_account_rc( "concurrent-payer" ) == 800 );
  assert( impl.get_pending_transaction_count( "concurrent-payer" ) == 2 );
  assert( impl.pending_transaction_count() == 2 );
}

void measure_diagnostic_count( bool materialized )
{
  koinos::mempool::mempool impl;
  MempoolAdapter adapter( impl );
  const auto inserted_at = std::chrono::system_clock::time_point{ std::chrono::seconds{ 1'000 } };
  constexpr size_t records = 128, payload_bytes = 128 * 1024, repetitions = 100;
  for( size_t i = 0; i < records; ++i )
  {
    auto accepted = make_accepted_transaction( "count-tx-" + std::to_string( i ), "count-payer", i + 1, 1, 1'000 );
    accepted.mutable_transaction()->set_signatures( 0, std::string( payload_bytes, 'x' ) );
    adapter.add_transaction_accepted( accepted, inserted_at );
  }
  // Compare two correct count paths for this <2,000-record fixture. The
  // materialized API is NOT the previous native metrics expression (limit=0),
  // which always returned zero and did no payload work.
  auto count = [&] { return materialized ? impl.get_pending_transactions( koinos::mempool::constants::max_request_limit ).size()
                                         : impl.pending_transaction_count(); };
  assert( count() == records ); // equal warm-up before timed execution
  const auto start = std::chrono::steady_clock::now();
  for( size_t i = 0; i < repetitions; ++i ) assert( count() == records );
  const auto elapsed = std::chrono::duration_cast< std::chrono::microseconds >( std::chrono::steady_clock::now() - start ).count();
  struct rusage usage{};
  assert( getrusage( RUSAGE_SELF, &usage ) == 0 );
#if defined( __APPLE__ )
  const uint64_t peak = usage.ru_maxrss;
#else
  const uint64_t peak = static_cast< uint64_t >( usage.ru_maxrss ) * 1024;
#endif
  std::cout << "{\"format\":\"lr2-diagnostic-count\",\"materialized\":" << ( materialized ? "true" : "false" )
            << ",\"records\":" << records << ",\"signature_bytes_per_record\":" << payload_bytes
            << ",\"repetitions\":" << repetitions << ",\"elapsed_microseconds\":" << elapsed
            << ",\"process_peak_rss_bytes\":" << peak << "}\n";
}

} // anonymous namespace

int main( int argc, char** argv )
{
  if( argc == 2 && ( std::string( argv[1] ) == "--measure-count-materialized" || std::string( argv[1] ) == "--measure-count" ) )
  {
    measure_diagnostic_count( std::string( argv[1] ) == "--measure-count-materialized" );
    return 0;
  }
  test_pending_resources_and_expiration();
  test_monolith_rpc_client_mempool_methods();
  test_request_limit_does_not_bound_global_retention();
  test_failed_native_admission_is_not_published();
  test_post_admission_notification_failure_is_observable();
  test_admission_message_and_disabled_mode();
  test_concurrent_native_resource_admission();
  return 0;
}
