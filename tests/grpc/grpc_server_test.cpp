#include "grpc_server/grpc_server.hpp"
#include "core/transaction_work_budget.hpp"
#include "core/owned_allocation.hpp"

#include <koinos/rpc/services.grpc.pb.h>

#include <grpcpp/grpcpp.h>

#include <cassert>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>

using namespace koinos;
using namespace koinos::node;

namespace control_fault {
std::atomic<bool> fail_next_captured{false};
std::atomic<unsigned> failures{0};
std::atomic<unsigned> target{1}, seen{0};
}
void* operator new(std::size_t bytes) {
  if(OwnedMemoryBudget::capture_allocation() && control_fault::fail_next_captured.load() &&
     ++control_fault::seen==control_fault::target.load() &&
     control_fault::fail_next_captured.exchange(false)) {
    ++control_fault::failures;
    throw std::bad_alloc();
  }
  return owned_allocation::allocate(bytes);
}

namespace {

class FakeChain final : public IChain
{
public:
  std::atomic< bool > full = true;
  std::atomic< LocalCapacityReason > reason = LocalCapacityReason::transaction_work;
  std::atomic< size_t > receipt_bytes = 0, error_bytes = 0;
  std::atomic< unsigned > accepted = 0, prepared = 0;
  rpc::chain::submit_block_response
  submit_block( const rpc::chain::submit_block_request& ) override
  {
    throw std::runtime_error( "submit_block failed intentionally" );
  }

  rpc::chain::submit_transaction_response
  submit_transaction( const rpc::chain::submit_transaction_request& ) override
  {
    if( error_bytes ) throw std::runtime_error( std::string( error_bytes.load(), 'e' ) );
    if( full ) throw LocalCapacityExceeded( reason.load() );
    rpc::chain::submit_transaction_response response;
    if( receipt_bytes ) response.mutable_receipt()->add_logs( std::string( receipt_bytes.load(), 'r' ) );
    return response;
  }

  rpc::chain::submit_transaction_response
  submit_transaction_with_prepared_response( const rpc::chain::submit_transaction_request& request,
                                             TransactionResponsePreparer prepare ) override
  {
    auto response = submit_transaction( request );
    prepare( response );
    ++prepared;
    if( request.broadcast() ) ++accepted;
    return response;
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
  OwnedMemoryBudget budget;
  std::atomic< size_t > pending_bytes = 0;
  std::atomic<unsigned> calls{0};
  std::atomic<unsigned> resource_calls{0};
  std::atomic<uint64_t> entry_accounted{0};
  std::mutex gate_mutex;
  std::condition_variable gate_cv;
  bool gated=false, entered=false, released=false;
  unsigned entered_calls = 0; // Protected by gate_mutex.
  OwnedMemoryBudget memory_budget() const noexcept override { return budget; }

  rpc::mempool::get_pending_transactions_response
  get_pending_transactions( const rpc::mempool::get_pending_transactions_request& req ) override
  {
    last_pending_limit = req.limit();
    ++calls;
    entry_accounted=budget.snapshot().accounted_bytes;
    if(gated) {
      std::unique_lock lock(gate_mutex);entered=true;++entered_calls;gate_cv.notify_all();
      if(!gate_cv.wait_for(lock,std::chrono::seconds(3),[&]{return released;}))
        throw std::runtime_error("M1 gRPC fixture gate did not release");
    }
    rpc::mempool::get_pending_transactions_response response;
    if( pending_bytes )
      response.add_pending_transactions()->mutable_transaction()->add_signatures(
        std::string( pending_bytes.load(), 'p' ) );
    return response;
  }

  rpc::mempool::check_pending_account_resources_response
  check_pending_account_resources( const rpc::mempool::check_pending_account_resources_request& req ) override
  {
    ++resource_calls;
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

  std::atomic< uint64_t > last_pending_limit = 0;
};

template< typename Response, typename Request, typename Call >
grpc::Status call( Call&& fn, const Request& req, Response& resp )
{
  grpc::ClientContext ctx;
  return fn( &ctx, req, &resp );
}

} // anonymous namespace

int control_ownership(bool require_owned)
{
  FakeChain chain;chain.full=false;
  FakeMempool mempool;mempool.budget=OwnedMemoryBudget(1024*1024);mempool.gated=true;
  koinos::node::grpc_server::GRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1:0");
  server.start();
  auto channel=grpc::CreateChannel("127.0.0.1:"+std::to_string(server.bound_port()),grpc::InsecureChannelCredentials());
  assert(channel->WaitForConnected(std::chrono::system_clock::now()+std::chrono::seconds(5)));
  auto stub=services::koinos::NewStub(channel);
  const auto bootstrap=mempool.budget.snapshot().bootstrap_accounted_bytes;
  const auto startup=mempool.budget.snapshot().accounted_bytes;
  auto pending=std::async(std::launch::async,[&] {
    grpc::ClientContext context;context.set_deadline(std::chrono::system_clock::now()+std::chrono::seconds(5));
    rpc::mempool::get_pending_transactions_response response;
    return stub->get_pending_transactions(&context,{},&response);
  });
  uint64_t held=0;
  { std::unique_lock lock(mempool.gate_mutex);
    assert(mempool.gate_cv.wait_for(lock,std::chrono::seconds(3),[&]{return mempool.entered;}));
    held=mempool.entry_accounted;
    mempool.released=true;mempool.gate_cv.notify_all(); }
  assert(pending.get().ok());
  mempool.gated=false;
  // Registry buckets are real retained owners until stop, not leaked credits.
  server.stop();
  assert(mempool.budget.snapshot().accounted_bytes==bootstrap);
  if(require_owned) {
    auto occupied=mempool.budget.reserve(mempool.budget.snapshot().limit_bytes-bootstrap);
    server.start();
    channel=grpc::CreateChannel("127.0.0.1:"+std::to_string(server.bound_port()),grpc::InsecureChannelCredentials());
    assert(channel->WaitForConnected(std::chrono::system_clock::now()+std::chrono::seconds(5)));
    stub=services::koinos::NewStub(channel);
    const auto before=mempool.calls.load();
    { grpc::ClientContext context;context.set_deadline(std::chrono::system_clock::now()+std::chrono::seconds(5));
      rpc::mempool::get_pending_transactions_response response;
      const auto status=stub->get_pending_transactions(&context,{},&response);
      assert(status.error_code()==grpc::StatusCode::RESOURCE_EXHAUSTED); }
    assert(mempool.calls==before); // Admission must precede native work.
    occupied.reset();
    server.stop();
    assert(mempool.budget.snapshot().accounted_bytes==bootstrap);
    // Each fresh registry exercises state, map node, bucket and interceptor
    // allocation failure, including partial publication before the last new.
    for(unsigned ordinal=1;ordinal<=4;++ordinal) {
      server.start();
      channel=grpc::CreateChannel("127.0.0.1:"+std::to_string(server.bound_port()),grpc::InsecureChannelCredentials());
      assert(channel->WaitForConnected(std::chrono::system_clock::now()+std::chrono::seconds(5)));
      stub=services::koinos::NewStub(channel);
      const auto failures=control_fault::failures.load();
      control_fault::seen=0;control_fault::target=ordinal;
      control_fault::fail_next_captured=true;
      { grpc::ClientContext context;context.set_deadline(std::chrono::system_clock::now()+std::chrono::seconds(5));
        rpc::mempool::get_pending_transactions_response response;
        const auto status=stub->get_pending_transactions(&context,{},&response);
        assert(status.error_code()==grpc::StatusCode::RESOURCE_EXHAUSTED); }
      control_fault::fail_next_captured=false;
      assert(control_fault::failures==failures+1 && mempool.calls==before);
      server.stop();
      assert(mempool.budget.snapshot().accounted_bytes==bootstrap);
    }
    server.start();
    channel=grpc::CreateChannel("127.0.0.1:"+std::to_string(server.bound_port()),grpc::InsecureChannelCredentials());
    assert(channel->WaitForConnected(std::chrono::system_clock::now()+std::chrono::seconds(5)));
    stub=services::koinos::NewStub(channel);
    { grpc::ClientContext context;context.set_deadline(std::chrono::system_clock::now()+std::chrono::seconds(5));
      rpc::chain::submit_transaction_request request;request.set_broadcast(true);
      rpc::chain::submit_transaction_response response;
      assert(stub->submit_transaction(&context,request,&response).ok()); }
    assert(chain.prepared==1 && chain.accepted==1);
  }
  server.stop();
  const auto after=mempool.budget.snapshot().accounted_bytes;
  std::cout<<"M1_GRPC_CONTROL bootstrap="<<bootstrap<<" startup="<<startup
    <<" held="<<held<<" after="<<after<<'\n';
  assert(after==bootstrap);
  if(require_owned) assert(startup>bootstrap && held>startup);
  else assert(startup==bootstrap && held==bootstrap);
  std::cout<<"gRPC control owner lifetime and admission check completed\n";
  return 0;
}

int control_concurrency()
{
  FakeChain chain;
  FakeMempool mempool;
  mempool.budget = OwnedMemoryBudget( 1024 * 1024 );
  mempool.gated = true;
  koinos::node::grpc_server::GRPCServer server(
    &chain, &mempool, nullptr, nullptr, nullptr, nullptr, "127.0.0.1:0" );
  const auto bootstrap = mempool.budget.snapshot().accounted_bytes;
  server.start();
  const auto startup = mempool.budget.snapshot().accounted_bytes;
  auto channel = grpc::CreateChannel(
    "127.0.0.1:" + std::to_string( server.bound_port() ), grpc::InsecureChannelCredentials() );
  assert( channel->WaitForConnected( std::chrono::system_clock::now() + std::chrono::seconds( 5 ) ) );
  auto stub = services::koinos::NewStub( channel );
  grpc::ClientContext first, second;
  first.set_deadline( std::chrono::system_clock::now() + std::chrono::seconds( 5 ) );
  second.set_deadline( std::chrono::system_clock::now() + std::chrono::seconds( 5 ) );
  auto read = [&]( grpc::ClientContext& context ) {
    rpc::mempool::get_pending_transactions_response response;
    return stub->get_pending_transactions( &context, {}, &response );
  };
  auto pending_first = std::async( std::launch::async, [&] { return read( first ); } );
  auto held = [&]( unsigned count ) {
    std::unique_lock lock( mempool.gate_mutex );
    assert( mempool.gate_cv.wait_for( lock, std::chrono::seconds( 3 ),
      [&] { return mempool.entered_calls == count; } ) );
    return mempool.budget.snapshot().accounted_bytes;
  };
  const auto one = held( 1 );
  auto pending_second = std::async( std::launch::async, [&] { return read( second ); } );
  const auto two = held( 2 );
  assert( startup > bootstrap && one > startup && two > one );

  first.TryCancel();
  assert( pending_first.wait_for( std::chrono::seconds( 3 ) ) == std::future_status::ready );
  assert( pending_first.get().error_code() == grpc::StatusCode::CANCELLED );
  // Cancelling the client cannot free the native handler's still-live owner.
  assert( mempool.budget.snapshot().accounted_bytes > one );
  auto stopping = std::async( std::launch::async, [&] { server.stop(); } );
  assert( stopping.wait_for( std::chrono::milliseconds( 50 ) ) == std::future_status::timeout );
  assert( mempool.budget.snapshot().accounted_bytes > one );
  {
    std::lock_guard lock( mempool.gate_mutex );
    mempool.released = true;
    mempool.gate_cv.notify_all();
  }
  assert( stopping.wait_for( std::chrono::seconds( 3 ) ) == std::future_status::ready );
  stopping.get();
  const auto status = pending_second.get();
  assert( status.ok() || status.error_code() == grpc::StatusCode::CANCELLED ||
    status.error_code() == grpc::StatusCode::UNAVAILABLE );
  assert( mempool.calls == 2 && chain.accepted == 0 );
  assert( mempool.budget.snapshot().accounted_bytes == bootstrap );
  std::cout << "M1_GRPC_CONTROL_CONCURRENT bootstrap=" << bootstrap << " startup=" << startup
    << " one=" << one << " two=" << two << '\n';
  std::cout << "gRPC concurrent control cancellation and stop check completed\n";
  return 0;
}

int refusal_control_capacity()
{
  FakeChain chain; chain.full = false;
  FakeMempool mempool; mempool.budget = OwnedMemoryBudget( 1024 * 1024, false, 8192 );
  const auto bootstrap = mempool.budget.snapshot().accounted_bytes;
  koinos::node::grpc_server::GRPCServer server(
    &chain, &mempool, nullptr, nullptr, nullptr, nullptr, "127.0.0.1:0" );
  server.start();
  auto channel = grpc::CreateChannel( "127.0.0.1:" + std::to_string( server.bound_port() ),
                                      grpc::InsecureChannelCredentials() );
  assert( channel->WaitForConnected( std::chrono::system_clock::now() + std::chrono::seconds( 5 ) ) );
  auto stub = services::koinos::NewStub( channel );
  auto occupied = mempool.budget.reserve(
    mempool.budget.snapshot().limit_bytes - mempool.budget.snapshot().accounted_bytes );
  OwnedMemoryBudget::Lease refusal_occupied;
  {
    OwnedMemoryBudget::Scope scope( mempool.budget, OwnedMemoryBudget::WorkClass::refusal );
    refusal_occupied = mempool.budget.reserve( 8192 );
  }
  for( unsigned method = 0; method < 3; ++method )
  {
    grpc::ClientContext context;
    context.set_deadline( std::chrono::system_clock::now() + std::chrono::seconds( 5 ) );
    grpc::Status status;
    if( method == 0 )
    {
      rpc::mempool::get_pending_transactions_response response;
      status = stub->get_pending_transactions( &context, {}, &response );
    }
    else if( method == 1 )
    {
      rpc::mempool::check_pending_account_resources_response response;
      status = stub->check_pending_account_resources( &context, {}, &response );
    }
    else
    {
      rpc::chain::submit_transaction_response response;
      status = stub->submit_transaction( &context, {}, &response );
    }
    assert( status.error_code() == grpc::StatusCode::CANCELLED );
  }
  assert( mempool.calls == 0 && mempool.resource_calls == 0 && chain.accepted == 0 );
  auto saturated = mempool.budget.snapshot();
  assert( saturated.refusal_transport_cancellations == 3 && saturated.refusal_capacity_refusals >= 3 &&
          saturated.peak_refusal_accounted_bytes == 8192 );
  refusal_occupied.reset();
  {
    grpc::ClientContext context;
    context.set_deadline( std::chrono::system_clock::now() + std::chrono::seconds( 5 ) );
    rpc::mempool::get_pending_transactions_response response;
    assert( stub->get_pending_transactions( &context, {}, &response ).error_code() ==
            grpc::StatusCode::RESOURCE_EXHAUSTED );
    assert( mempool.calls == 0 );
  }
  server.stop();
  const auto stopped_occupied = mempool.budget.snapshot().accounted_bytes;
  // The ordinary allocation refuses first. Fail each subsequent fallback
  // state/node/bucket/interceptor allocation in a fresh registry, so partial
  // fallback setup must cancel safely and return every live-owner credit.
  for( unsigned ordinal = 2; ordinal <= 5; ++ordinal )
  {
    server.start();
    channel = grpc::CreateChannel( "127.0.0.1:" + std::to_string( server.bound_port() ),
                                    grpc::InsecureChannelCredentials() );
    assert( channel->WaitForConnected( std::chrono::system_clock::now() + std::chrono::seconds( 5 ) ) );
    stub = services::koinos::NewStub( channel );
    const auto failures = control_fault::failures.load();
    control_fault::seen = 0; control_fault::target = ordinal; control_fault::fail_next_captured = true;
    grpc::ClientContext context;
    context.set_deadline( std::chrono::system_clock::now() + std::chrono::seconds( 5 ) );
    rpc::mempool::get_pending_transactions_response response;
    const auto status = stub->get_pending_transactions( &context, {}, &response );
    control_fault::fail_next_captured = false;
    assert( status.error_code() == grpc::StatusCode::CANCELLED && control_fault::failures == failures + 1 );
    assert( mempool.calls == 0 && mempool.resource_calls == 0 && chain.accepted == 0 );
    server.stop();
    assert( mempool.budget.snapshot().accounted_bytes == stopped_occupied &&
            mempool.budget.snapshot().refusal_accounted_bytes == 0 );
  }
  server.start();
  channel = grpc::CreateChannel( "127.0.0.1:" + std::to_string( server.bound_port() ),
                                  grpc::InsecureChannelCredentials() );
  assert( channel->WaitForConnected( std::chrono::system_clock::now() + std::chrono::seconds( 5 ) ) );
  stub = services::koinos::NewStub( channel );
  occupied.reset();
  {
    grpc::ClientContext context;
    context.set_deadline( std::chrono::system_clock::now() + std::chrono::seconds( 5 ) );
    rpc::chain::submit_transaction_request request; request.set_broadcast( true );
    rpc::chain::submit_transaction_response response;
    assert( stub->submit_transaction( &context, request, &response ).ok() );
  }
  assert( chain.prepared == 1 && chain.accepted == 1 );
  server.stop();
  const auto final = mempool.budget.snapshot();
  assert( final.accounted_bytes == bootstrap && final.refusal_accounted_bytes == 0 &&
          final.refusal_transport_cancellations == 7 );
  std::cout << "M1_GRPC_REFUSAL_CAPACITY cancellations=" << final.refusal_transport_cancellations
            << " refusal_peak=" << final.peak_refusal_accounted_bytes << " final=" << final.accounted_bytes << '\n';
  return 0;
}

int main(int argc,char** argv)
{
  if(argc==2 && std::string(argv[1])=="--m1-control-owners-gap") return control_ownership(false);
  if(argc==2 && std::string(argv[1])=="--m1-control-owners-owned") return control_ownership(true);
  if(argc==2 && std::string(argv[1])=="--m1-control-concurrency") return control_concurrency();
  if(argc==2 && std::string(argv[1])=="--m1-refusal-control-capacity") return refusal_control_capacity();
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

  rpc::chain::submit_transaction_response transaction_resp;
  auto overload = call< rpc::chain::submit_transaction_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->submit_transaction( ctx, req, resp ); },
    rpc::chain::submit_transaction_request{}, transaction_resp );
  assert( overload.error_code() == grpc::StatusCode::RESOURCE_EXHAUSTED );
  chain.reason = LocalCapacityReason::pending_record;
  overload = call< rpc::chain::submit_transaction_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->submit_transaction( ctx, req, resp ); },
    rpc::chain::submit_transaction_request{}, transaction_resp );
  assert( overload.error_code() == grpc::StatusCode::RESOURCE_EXHAUSTED );
  assert( overload.error_message() == LocalCapacityExceeded( LocalCapacityReason::pending_record ).what() );
  chain.reason = LocalCapacityReason::mempool_memory;
  overload = call< rpc::chain::submit_transaction_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->submit_transaction( ctx, req, resp ); },
    rpc::chain::submit_transaction_request{}, transaction_resp );
  assert( overload.error_code() == grpc::StatusCode::RESOURCE_EXHAUSTED );
  assert( overload.error_message() == LocalCapacityExceeded( LocalCapacityReason::mempool_memory ).what() );
  chain.full = false;
  overload = call< rpc::chain::submit_transaction_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return stub->submit_transaction( ctx, req, resp ); },
    rpc::chain::submit_transaction_request{}, transaction_resp );
  assert( overload.ok() );

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

  // The server reserves SDK serialization/status ownership in the shared M1
  // domain, while the exact wire payload remains complete. submit_transaction
  // prepares that reservation before the fixture records acceptance.
  FakeChain output_chain;
  output_chain.full = false;
  output_chain.receipt_bytes = 1024 * 1024;
  FakeMempool output_mempool;
  output_mempool.budget = OwnedMemoryBudget( 128 * 1024 * 1024 );
  output_mempool.pending_bytes = 1024 * 1024;
  koinos::node::grpc_server::GRPCServer output_server(
    &output_chain, &output_mempool, nullptr, nullptr, nullptr, nullptr, "127.0.0.1:0" );
  output_server.start();
  const auto output_endpoint = "127.0.0.1:" + std::to_string( output_server.bound_port() );
  // Large status text is carried in trailing metadata, independently of the
  // ordinary receive-message limit. This fixture deliberately receives 1 MiB;
  // preserve the SDK's default peer-limit rejection as a separate control.
  grpc::ChannelArguments output_arguments;
  output_arguments.SetInt( GRPC_ARG_MAX_METADATA_SIZE, 2 * 1024 * 1024 );
  auto output_channel = grpc::CreateCustomChannel(
    output_endpoint, grpc::InsecureChannelCredentials(), output_arguments );
  assert( output_channel->WaitForConnected( std::chrono::system_clock::now() + std::chrono::seconds( 5 ) ) );
  auto output_stub = services::koinos::NewStub( output_channel );

  rpc::mempool::get_pending_transactions_response large_pending;
  status = call< rpc::mempool::get_pending_transactions_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return output_stub->get_pending_transactions( ctx, req, resp ); },
    rpc::mempool::get_pending_transactions_request{}, large_pending );
  assert( status.ok() );
  assert( large_pending.pending_transactions_size() == 1 );
  assert( large_pending.pending_transactions( 0 ).transaction().signatures( 0 ) ==
          std::string( 1024 * 1024, 'p' ) );

  rpc::chain::submit_transaction_request output_request;
  output_request.set_broadcast( true );
  rpc::chain::submit_transaction_response large_receipt;
  status = call< rpc::chain::submit_transaction_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return output_stub->submit_transaction( ctx, req, resp ); },
    output_request, large_receipt );
  assert( status.ok() );
  assert( large_receipt.receipt().logs( 0 ) == std::string( 1024 * 1024, 'r' ) );
  assert( output_chain.prepared == 1 && output_chain.accepted == 1 );

  output_chain.error_bytes = 1024 * 1024;
  rpc::chain::submit_transaction_response ignored;
  {
    auto default_channel = grpc::CreateChannel( output_endpoint, grpc::InsecureChannelCredentials() );
    assert( default_channel->WaitForConnected(
      std::chrono::system_clock::now() + std::chrono::seconds( 5 ) ) );
    auto default_stub = services::koinos::NewStub( default_channel );
    status = call< rpc::chain::submit_transaction_response >(
      [&]( auto* ctx, const auto& req, auto* resp ) { return default_stub->submit_transaction( ctx, req, resp ); },
      output_request, ignored );
    assert( status.error_code() == grpc::StatusCode::RESOURCE_EXHAUSTED );
    assert( status.error_message().find( "metadata" ) != std::string::npos );
    assert( output_chain.accepted == 1 );
  }
  status = call< rpc::chain::submit_transaction_response >(
    [&]( auto* ctx, const auto& req, auto* resp ) { return output_stub->submit_transaction( ctx, req, resp ); },
    output_request, ignored );
  assert( status.error_code() == grpc::StatusCode::INTERNAL );
  assert( status.error_message() == std::string( 1024 * 1024, 'e' ) );
  assert( output_chain.accepted == 1 );
  const auto output_usage = output_mempool.budget.snapshot();
  assert( output_usage.peak_accounted_bytes >= 16 * 1024 * 1024 );
  output_server.stop();
  assert( output_mempool.budget.snapshot().accounted_bytes ==
          output_mempool.budget.snapshot().bootstrap_accounted_bytes );
  return 0;
}
