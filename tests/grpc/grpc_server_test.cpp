#include "grpc_server/grpc_server.hpp"

#include <koinos/rpc/services.grpc.pb.h>

#include <grpcpp/grpcpp.h>

#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>

using namespace koinos;
using namespace koinos::node;

namespace {

class FakeChain final : public IChain
{
public:
  rpc::chain::submit_block_response
  submit_block( const rpc::chain::submit_block_request& ) override
  {
    throw std::runtime_error( "submit_block failed intentionally" );
  }

  rpc::chain::submit_transaction_response
  submit_transaction( const rpc::chain::submit_transaction_request& ) override
  {
    return {};
  }

  rpc::chain::get_head_info_response
  get_head_info( const rpc::chain::get_head_info_request& ) override
  {
    rpc::chain::get_head_info_response resp;
    resp.mutable_head_topology()->set_height( 123 );
    resp.mutable_head_topology()->set_id( "head-block" );
    resp.mutable_head_topology()->set_previous( "previous-block" );
    resp.set_last_irreversible_block( 120 );
    resp.set_head_state_merkle_root( "state-root" );
    resp.set_head_block_time( 456 );
    return resp;
  }

  rpc::chain::get_chain_id_response
  get_chain_id( const rpc::chain::get_chain_id_request& ) override
  {
    rpc::chain::get_chain_id_response resp;
    resp.set_chain_id( "chain-id" );
    return resp;
  }

  rpc::chain::get_fork_heads_response
  get_fork_heads( const rpc::chain::get_fork_heads_request& ) override
  {
    return {};
  }

  rpc::chain::read_contract_response
  read_contract( const rpc::chain::read_contract_request& ) override
  {
    return {};
  }

  rpc::chain::get_account_nonce_response
  get_account_nonce( const rpc::chain::get_account_nonce_request& req ) override
  {
    if( req.account().empty() )
      throw std::runtime_error( "expected field account was nil" );
    rpc::chain::get_account_nonce_response resp;
    resp.set_nonce( "nonce" );
    return resp;
  }

  rpc::chain::get_account_rc_response
  get_account_rc( const rpc::chain::get_account_rc_request& ) override
  {
    rpc::chain::get_account_rc_response resp;
    resp.set_rc( 777 );
    return resp;
  }

  rpc::chain::get_resource_limits_response
  get_resource_limits( const rpc::chain::get_resource_limits_request& ) override
  {
    return {};
  }

  rpc::chain::invoke_system_call_response
  invoke_system_call( const rpc::chain::invoke_system_call_request& ) override
  {
    return {};
  }

  rpc::chain::propose_block_response
  propose_block( const rpc::chain::propose_block_request& ) override
  {
    return {};
  }
};

class FakeBlockStore final : public IBlockStore
{
public:
  rpc::block_store::get_blocks_by_height_response
  get_blocks_by_height( const rpc::block_store::get_blocks_by_height_request& req ) override
  {
    last_height_request_count = req.num_blocks();
    return {};
  }

  rpc::block_store::get_blocks_by_id_response
  get_blocks_by_id( const rpc::block_store::get_blocks_by_id_request& req ) override
  {
    last_id_request_count = static_cast< uint32_t >( req.block_ids_size() );
    return {};
  }

  rpc::block_store::get_highest_block_response
  get_highest_block( const rpc::block_store::get_highest_block_request& ) override
  {
    rpc::block_store::get_highest_block_response resp;
    resp.mutable_topology()->set_height( 123 );
    resp.mutable_topology()->set_id( "head-block" );
    resp.mutable_topology()->set_previous( "previous-block" );
    return resp;
  }

  rpc::block_store::add_block_response
  add_block( const rpc::block_store::add_block_request& ) override
  {
    return {};
  }

  uint32_t last_height_request_count = 0;
  uint32_t last_id_request_count     = 0;
};

class FakeMempool final : public IMempool
{
public:
  rpc::mempool::get_pending_transactions_response
  get_pending_transactions( const rpc::mempool::get_pending_transactions_request& req ) override
  {
    last_pending_limit = req.limit();
    return {};
  }

  rpc::mempool::check_pending_account_resources_response
  check_pending_account_resources( const rpc::mempool::check_pending_account_resources_request& req ) override
  {
    rpc::mempool::check_pending_account_resources_response resp;
    resp.set_success( !req.payer().empty() );
    return resp;
  }

  rpc::mempool::check_account_nonce_response
  check_account_nonce( const rpc::mempool::check_account_nonce_request& ) override
  {
    return {};
  }

  rpc::mempool::get_pending_nonce_response
  get_pending_nonce( const rpc::mempool::get_pending_nonce_request& ) override
  {
    return {};
  }

  rpc::mempool::get_pending_transaction_count_response
  get_pending_transaction_count( const rpc::mempool::get_pending_transaction_count_request& ) override
  {
    return {};
  }

  rpc::mempool::get_reserved_account_rc_response
  get_reserved_account_rc( const rpc::mempool::get_reserved_account_rc_request& ) override
  {
    return {};
  }

  uint64_t last_pending_limit = 0;
};

template< typename Response, typename Request, typename Call >
grpc::Status call( Call&& fn, const Request& req, Response& resp )
{
  grpc::ClientContext ctx;
  return fn( &ctx, req, &resp );
}

} // anonymous namespace

int main()
{
  FakeChain chain;
  FakeBlockStore block_store;
  FakeMempool mempool;

  koinos::node::grpc_server::GRPCServer server(
    &chain, &mempool, &block_store, nullptr, nullptr, nullptr, "127.0.0.1:0" );
  server.start();
  assert( server.bound_port() > 0 );

  const auto endpoint = "127.0.0.1:" + std::to_string( server.bound_port() );
  auto channel        = grpc::CreateChannel( endpoint, grpc::InsecureChannelCredentials() );
  assert( channel->WaitForConnected( std::chrono::system_clock::now() + std::chrono::seconds( 5 ) ) );

  auto stub = services::koinos::NewStub( channel );

  rpc::chain::get_head_info_response head_resp;
  auto status = call< rpc::chain::get_head_info_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->get_head_info( ctx, req, resp ); },
    rpc::chain::get_head_info_request{},
    head_resp );
  assert( status.ok() );
  assert( head_resp.head_topology().height() == 123 );
  assert( head_resp.head_topology().id() == "head-block" );

  rpc::chain::get_chain_id_response chain_id_resp;
  status = call< rpc::chain::get_chain_id_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->get_chain_id( ctx, req, resp ); },
    rpc::chain::get_chain_id_request{},
    chain_id_resp );
  assert( status.ok() );
  assert( chain_id_resp.chain_id() == "chain-id" );

  rpc::block_store::get_highest_block_response highest_resp;
  status = call< rpc::block_store::get_highest_block_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->get_highest_block( ctx, req, resp ); },
    rpc::block_store::get_highest_block_request{},
    highest_resp );
  assert( status.ok() );
  assert( highest_resp.topology().height() == 123 );

  rpc::mempool::get_pending_transactions_request pending_req;
  pending_req.set_limit( 50 );
  rpc::mempool::get_pending_transactions_response pending_resp;
  status = call< rpc::mempool::get_pending_transactions_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->get_pending_transactions( ctx, req, resp ); },
    pending_req,
    pending_resp );
  assert( status.ok() );
  assert( mempool.last_pending_limit == 50 );

  rpc::mempool::check_pending_account_resources_request resources_req;
  resources_req.set_payer( "payer" );
  rpc::mempool::check_pending_account_resources_response resources_resp;
  status = call< rpc::mempool::check_pending_account_resources_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->check_pending_account_resources( ctx, req, resp ); },
    resources_req,
    resources_resp );
  assert( status.ok() );
  assert( resources_resp.success() );

  rpc::contract_meta_store::get_contract_meta_response meta_resp;
  status = call< rpc::contract_meta_store::get_contract_meta_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->get_contract_meta( ctx, req, resp ); },
    rpc::contract_meta_store::get_contract_meta_request{},
    meta_resp );
  assert( status.error_code() == grpc::StatusCode::UNAVAILABLE );

  rpc::p2p::get_gossip_status_response gossip_resp;
  status = call< rpc::p2p::get_gossip_status_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->get_gossip_status( ctx, req, resp ); },
    rpc::p2p::get_gossip_status_request{},
    gossip_resp );
  assert( status.ok() );
  assert( !gossip_resp.enabled() );

  rpc::chain::submit_block_response submit_resp;
  status = call< rpc::chain::submit_block_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->submit_block( ctx, req, resp ); },
    rpc::chain::submit_block_request{},
    submit_resp );
  assert( status.error_code() == grpc::StatusCode::INTERNAL );
  assert( status.error_message().find( "submit_block failed intentionally" ) != std::string::npos );

  rpc::chain::get_account_nonce_response nonce_resp;
  status = call< rpc::chain::get_account_nonce_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->get_account_nonce( ctx, req, resp ); },
    rpc::chain::get_account_nonce_request{},
    nonce_resp );
  assert( status.error_code() == grpc::StatusCode::INVALID_ARGUMENT );
  assert( status.error_message().find( "expected field account was nil" ) != std::string::npos );

  server.stop();
  return 0;
}
