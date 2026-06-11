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
#include <chrono>
#include <future>
#include <string>

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

  assert( adapter.prune( std::chrono::seconds{ 120 }, inserted_at + std::chrono::seconds{ 120 } ) == 1 );
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

} // anonymous namespace

int main()
{
  test_pending_resources_and_expiration();
  test_monolith_rpc_client_mempool_methods();
  return 0;
}
