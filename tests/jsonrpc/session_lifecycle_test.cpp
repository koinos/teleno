#include "jsonrpc/jsonrpc_server.hpp"
#include "core/owned_allocation.hpp"
#include "core/transaction_work_budget.hpp"
#include <koinos/util/base64.hpp>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <source_location>
#include <utility>

namespace response_fault {
thread_local bool fail_next_allocation=false;
std::atomic<unsigned> failures{0};
std::atomic<std::size_t> normalization_size{0};
std::atomic<unsigned> normalization_nth{0}, normalization_seen{0}, normalization_failures{0};
std::atomic<bool> normalization_bad_alloc{false};
}
// Keep the production ownership primitive while allowing one deterministic
// allocation failure after the fake service's acceptance effect.
namespace refusal_probe {
std::atomic<bool> enabled{false};
thread_local bool after_actual_quota_refusal=false;
std::atomic<uint64_t> marked_refusals{0}, untracked_large_requests{0}, untracked_large_bytes{0}, tracked_large_requests{0};
std::atomic<bool> pause_large{false}, release_large{false};
std::atomic<unsigned> paused{0};
std::atomic<unsigned> fault_nth{0}, fault_seen{0}, faults{0};
}
namespace conversion_fault {
thread_local bool active=false;
std::atomic<unsigned> nth{0}, seen{0}, fired{0};
}
namespace input_conversion_fault {
std::atomic<bool> enabled{false};std::atomic<unsigned> nth{0},seen{0},fired{0};
}
void* operator new(std::size_t bytes) {
  if(input_conversion_fault::enabled && koinos::node::OwnedMemoryBudget::capture_allocation() &&
     ++input_conversion_fault::seen==input_conversion_fault::nth) {
    input_conversion_fault::enabled=false;++input_conversion_fault::fired;
    throw koinos::node::LocalCapacityExceeded(koinos::node::LocalCapacityReason::mempool_memory);
  }

  if(conversion_fault::active && koinos::node::OwnedMemoryBudget::capture_allocation() &&
     conversion_fault::nth && ++conversion_fault::seen==conversion_fault::nth) {
    conversion_fault::active=false;++conversion_fault::fired;
    throw koinos::node::LocalCapacityExceeded(koinos::node::LocalCapacityReason::mempool_memory);
  }
  if(refusal_probe::enabled && refusal_probe::after_actual_quota_refusal &&
     koinos::node::OwnedMemoryBudget::capture_allocation() && refusal_probe::fault_nth &&
     ++refusal_probe::fault_seen==refusal_probe::fault_nth) {
    ++refusal_probe::faults;throw std::bad_alloc();
  }
  if(refusal_probe::enabled && refusal_probe::after_actual_quota_refusal && bytes>=65536) {
    if(koinos::node::OwnedMemoryBudget::capture_allocation()) {
      ++refusal_probe::tracked_large_requests;
      if(refusal_probe::pause_large) {
        ++refusal_probe::paused;
        while(!refusal_probe::release_large) std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
    else { ++refusal_probe::untracked_large_requests;refusal_probe::untracked_large_bytes+=bytes; }
  }

  if(std::exchange(response_fault::fail_next_allocation,false)) {
    ++response_fault::failures;
    throw std::bad_alloc();
  }
  const auto matching_size=response_fault::normalization_size.load();
  if(matching_size && bytes==matching_size && koinos::node::OwnedMemoryBudget::capture_allocation() &&
     ++response_fault::normalization_seen==response_fault::normalization_nth.load()) {
    ++response_fault::normalization_failures;
    if(response_fault::normalization_bad_alloc) throw std::bad_alloc();
    throw koinos::node::LocalCapacityExceeded(koinos::node::LocalCapacityReason::mempool_memory);
  }
  const bool was_captured=koinos::node::OwnedMemoryBudget::capture_allocation();
  try { return koinos::node::owned_allocation::allocate(bytes); }
  catch(const koinos::node::LocalCapacityExceeded&) {
    if(refusal_probe::enabled && was_captured) {
      refusal_probe::after_actual_quota_refusal=true;
      ++refusal_probe::marked_refusals;
    }
    throw;
  }
}
using namespace koinos::node;
using namespace koinos::node::jsonrpc;
using namespace std::chrono_literals;
void check(bool value, std::source_location location=std::source_location::current()) {
  if(!value) throw std::runtime_error("RPC lifecycle assertion failed at line "+std::to_string(location.line()));
}
template<typename F> void wait_until(F f) {
  auto deadline=std::chrono::steady_clock::now()+2s;
  while(!f()) { check(std::chrono::steady_clock::now()<deadline); std::this_thread::sleep_for(5ms); }
}
class Chain: public IChain {
public:
  std::atomic< bool > full = true;
  std::atomic< LocalCapacityReason > reason = LocalCapacityReason::transaction_work;
  koinos::rpc::chain::submit_transaction_response submit_transaction(const koinos::rpc::chain::submit_transaction_request&) override {
    if(full) throw LocalCapacityExceeded(reason.load()); return {};
  }
  std::mutex mutex; std::condition_variable cv; bool entered=false, released=false;
  koinos::rpc::chain::get_head_info_response get_head_info(const koinos::rpc::chain::get_head_info_request&) override {
    std::unique_lock lock(mutex); entered=true; cv.notify_all();
    check(cv.wait_for(lock,2s,[&]{return released;})); return {};
  }
#define EMPTY(method) koinos::rpc::chain::method##_response method(const koinos::rpc::chain::method##_request&) override { return {}; }
  EMPTY(submit_block) EMPTY(get_chain_id) EMPTY(get_fork_heads)
  koinos::rpc::chain::read_contract_response read_contract(const koinos::rpc::chain::read_contract_request&) override {
    koinos::rpc::chain::read_contract_response result; result.set_result(std::string(8*1024*1024,'x')); return result;
  }
  EMPTY(get_account_nonce) EMPTY(get_account_rc) EMPTY(get_resource_limits)
  EMPTY(invoke_system_call) EMPTY(propose_block)
#undef EMPTY
};
struct Client {
  net::io_context io; beast::tcp_stream stream{io}; beast::flat_buffer buffer;
  explicit Client(uint16_t port) { stream.connect(tcp::endpoint(net::ip::make_address("127.0.0.1"),port)); }
  void send(const std::string& text) {
    beast::error_code ec; net::write(stream.socket(),net::buffer(text),ec); check(!ec);
  }
  http::response<http::string_body> read(bool success=true,bool verify_result=true) {
    http::response_parser<http::string_body> parser;
    parser.body_limit(32*1024*1024); // Test client receives complete large responses.
    beast::error_code error;
    stream.expires_after(2s); io.restart();
    http::async_read(stream,buffer,parser,[&](beast::error_code ec,std::size_t){error=ec;});
    io.run(); if(verify_result) check(bool(error)!=success); return parser.release();
  }
  std::optional<http::response<http::string_body>> read_or_disconnect() {
    http::response_parser<http::string_body> parser;
    parser.body_limit(32*1024*1024);
    beast::error_code error;
    stream.expires_after(2s); io.restart();
    http::async_read(stream,buffer,parser,[&](beast::error_code ec,std::size_t){error=ec;});
    io.run();
    if(!error) return parser.release();
    check(error==net::error::eof || error==net::error::connection_reset ||
      error==http::error::end_of_stream || error==http::error::partial_message);
    return std::nullopt;
  }
  auto health() { send("GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n"); return read(); }
  void post(const std::string& body) {
    send("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body);
  }
};
void body_lifetime(bool expect_retained=false) {
  Chain chain;
  JSONRPCServer server(&chain,nullptr,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,2,{},20s);
  server.start();
  Client client(server.port());
  std::string body=R"({"jsonrpc":"2.0","method":"chain.read_contract","id":11,"params":{}})";
  client.send("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body);
  auto response=client.read();
  auto result=nlohmann::json::parse(response.body());
  check(result["id"]==11);
  check(result["result"]["result"]==koinos::util::to_base64(std::string(8*1024*1024,'x')));
  wait_until([&]{return server.http_body_memory().peak_combined_capacity>=response.body().size();});
  if(!expect_retained) wait_until([&]{return server.http_body_memory().materialized_response_capacity==0;});
  const auto idle=server.http_body_memory();
  std::cout << "completed_response_bytes=" << response.body().size()
            << " idle_request_capacity=" << idle.completed_request_capacity
            << " idle_response_capacity=" << idle.materialized_response_capacity
            << " peak_combined_capacity=" << idle.peak_combined_capacity << std::endl;
  if(expect_retained) check(idle.materialized_response_capacity>=response.body().size());
  else check(idle.completed_request_capacity==0 && idle.materialized_response_capacity==0);
  check(server.active_sessions()==1); // The healthy connection stays open.
  check(client.health().result_int()==200);
  server.stop();
  const auto stopped=server.http_body_memory();
  check(stopped.completed_request_capacity==0 && stopped.materialized_response_capacity==0);
}
void body_transitions() {
  Chain chain;
  JSONRPCServer server(&chain,nullptr,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,2,{},20s);
  server.start();
  Client gated(server.port());
  const auto request=nlohmann::json({{"jsonrpc","2.0"},{"method","chain.get_head_info"},
    {"id",std::string(512*1024,'i')}}).dump();
  gated.post(request);
  { std::unique_lock lock(chain.mutex); check(chain.cv.wait_for(lock,1s,[&]{return chain.entered;})); }
  check(server.http_body_memory().completed_request_capacity>=request.size());
  auto stopping=std::async(std::launch::async,[&]{server.stop();});
  check(stopping.wait_for(50ms)==std::future_status::timeout);
  // Dispatch still owns the request even after stop, until the service returns.
  check(server.http_body_memory().completed_request_capacity>=request.size());
  { std::lock_guard lock(chain.mutex); chain.released=true; chain.cv.notify_all(); }
  check(stopping.wait_for(1s)==std::future_status::ready); stopping.get();
  check(server.http_body_memory().completed_request_capacity==0);
  check(server.http_body_memory().materialized_response_capacity==0);
  gated.read(false);

  server.start();
  { Client blocked(server.port());
    blocked.stream.socket().set_option(net::socket_base::receive_buffer_size(1024));
    const auto body=nlohmann::json({{"jsonrpc","2.0"},{"method","chain.read_contract"},
      {"id",std::string(512*1024,'r')},{"params",nlohmann::json::object()}}).dump();
    blocked.post(body);
    wait_until([&]{const auto m=server.http_body_memory();
      return m.materialized_response_capacity>8*1024*1024 && m.completed_request_capacity==0;});
    check(server.http_body_memory().completed_request_capacity==0);
    const auto first_capacity=server.http_body_memory().materialized_response_capacity;
    check(server.http_body_memory().peak_combined_capacity>=body.size()+first_capacity);
    Client second(server.port());
    second.stream.socket().set_option(net::socket_base::receive_buffer_size(1024));
    second.post(body);
    wait_until([&]{const auto m=server.http_body_memory();
      return m.materialized_response_capacity>first_capacity && m.completed_request_capacity==0;});
    check(server.active_sessions()==2);
    check(server.http_body_memory().materialized_response_capacity==2*first_capacity);
    check(server.http_body_memory().completed_request_capacity==0);
    check(server.http_body_memory().peak_combined_capacity>=body.size()+2*first_capacity);
    // The write still borrows its complete body: stop must cancel/drain before releasing it.
    server.stop();
    check(server.http_body_memory().completed_request_capacity==0);
    check(server.http_body_memory().materialized_response_capacity==0);
  }
  server.start();
  { Client pipeline(server.port());
    std::string body=R"({"jsonrpc":"2.0","method":"chain.read_contract","id":12,"params":{}})";
    pipeline.send("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: "+std::to_string(body.size())+
      "\r\n\r\n"+body+"GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n");
    auto response=pipeline.read();
    check(nlohmann::json::parse(response.body())["result"]["result"]==koinos::util::to_base64(std::string(8*1024*1024,'x')));
    check(response[http::field::access_control_allow_origin]=="*");
    check(pipeline.read().result_int()==200); // Unread pipelined bytes were not discarded.
    wait_until([&]{return server.http_body_memory().materialized_response_capacity==0;});
    pipeline.send("OPTIONS / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    check(pipeline.read().result_int()==204);
    pipeline.send("PUT / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n");
    check(pipeline.read().result_int()==405);
    pipeline.post("{"); check(nlohmann::json::parse(pipeline.read().body())["error"]["code"]==-32700);
    body=R"([{"jsonrpc":"2.0","method":"chain.get_chain_id","id":1},{"jsonrpc":"2.0","method":"chain.submit_transaction","id":2,"params":{}}])";
    pipeline.post(body); const auto batch=nlohmann::json::parse(pipeline.read().body());
    check(batch.size()==2 && batch[0].contains("result") && batch[1]["error"]["code"]==-32005);
    wait_until([&]{return server.http_body_memory().materialized_response_capacity==0;});
    check(server.http_body_memory().completed_request_capacity==0);
    check(pipeline.health().result_int()==200);
  }
  { Client closing(server.port());
    closing.send("GET /health HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    check(closing.read().result_int()==200); closing.read(false);
    wait_until([&]{return server.active_sessions()==0;});
    check(server.http_body_memory().completed_request_capacity==0);
    check(server.http_body_memory().materialized_response_capacity==0);
  }
  server.stop();
  check(server.http_body_memory().completed_request_capacity==0);
  check(server.http_body_memory().materialized_response_capacity==0);
  std::cout << "HTTP body dispatch/write/pipeline/cancellation transitions passed\n";
}
class LargeMempool: public IMempool {
public:
  explicit LargeMempool(uint64_t limit=64 * 1024 * 1024, size_t bytes=4 * 1024 * 1024):
    budget(limit), signature_bytes(bytes) {}
  OwnedMemoryBudget budget;
  size_t signature_bytes;
  OwnedMemoryBudget memory_budget() const noexcept override { return budget; }
  koinos::rpc::mempool::get_pending_transactions_response
  get_pending_transactions(const koinos::rpc::mempool::get_pending_transactions_request&) override {
    koinos::rpc::mempool::get_pending_transactions_response result;
    result.add_pending_transactions()->mutable_transaction()->add_signatures(std::string(signature_bytes, 'x'));
    return result;
  }
#define EMPTY_MEMPOOL(method) koinos::rpc::mempool::method##_response method(const koinos::rpc::mempool::method##_request&) override { return {}; }
  EMPTY_MEMPOOL(check_pending_account_resources)
  EMPTY_MEMPOOL(check_account_nonce)
  EMPTY_MEMPOOL(get_pending_nonce)
  EMPTY_MEMPOOL(get_pending_transaction_count)
  EMPTY_MEMPOOL(get_reserved_account_rc)
#undef EMPTY_MEMPOOL
};
void mempool_output_lifetime(bool expect_owned, bool mixed_batch=false) {
  LargeMempool mempool;
  Chain chain;
  JSONRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,2,{},20s);
  server.start();
  Client client(server.port());
  client.stream.socket().set_option(net::socket_base::receive_buffer_size(1024));
  client.post(mixed_batch ?
    R"([{"jsonrpc":"2.0","method":"mempool.get_pending_transactions","id":1,"params":{"limit":1}},{"jsonrpc":"2.0","method":"chain.get_chain_id","id":2,"params":{}}])" :
    R"({"jsonrpc":"2.0","method":"mempool.get_pending_transactions","id":1,"params":{"limit":1}})");
  const auto deadline=std::chrono::steady_clock::now()+15s;
  while(server.http_body_memory().materialized_response_capacity < 4 * 1024 * 1024) {
    check(std::chrono::steady_clock::now() < deadline);
    std::this_thread::sleep_for(5ms);
  }
  const auto body=server.http_body_memory();
  const auto usage=mempool.budget.snapshot();
  std::cout << "M1_JSON_OUTPUT_GAP response_capacity=" << body.materialized_response_capacity
            << " accounted_bytes=" << usage.accounted_bytes
            << " bootstrap_bytes=" << usage.bootstrap_accounted_bytes << '\n';
  check(body.materialized_response_capacity > 4 * 1024 * 1024);
  if(expect_owned) check(usage.accounted_bytes >= 4 * 1024 * 1024);
  else check(body.materialized_response_capacity > usage.accounted_bytes + 4 * 1024 * 1024);
  server.stop();
  const auto released=mempool.budget.snapshot();
  std::cout << "M1_JSON_OUTPUT_RELEASED accounted_bytes=" << released.accounted_bytes << '\n';
  if(expect_owned) check(usage.accounted_bytes > released.accounted_bytes + 4 * 1024 * 1024);
}
void mempool_mixed_batch_capacity() {
  LargeMempool mempool(16 * 1024 * 1024);
  Chain chain;
  JSONRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,2,{},20s);
  server.start();
  Client client(server.port());
  client.post(R"([{"jsonrpc":"2.0","method":"mempool.get_pending_transactions","id":1,"params":{"limit":1}},{"jsonrpc":"2.0","method":"chain.get_chain_id","id":2,"params":{}}])");
  const auto response=client.read();
  const auto body=nlohmann::json::parse(response.body());
  check(body.is_array() && body.size()==2);
  check(body[0]["id"]==1 && body[0]["error"]["code"]==-32005);
  check(body[0]["error"]["data"]["reason"]=="mempool_memory_capacity");
  check(body[0]["error"]["data"]["retryable"]==true);
  check(body[1]["id"]==2 && body[1].contains("result") && !body[1].contains("error"));
  check(mempool.budget.snapshot().refusals>0);
  server.stop();
}
void mempool_output_roundtrip() {
  LargeMempool mempool(64 * 1024 * 1024, 1024);
  Chain chain;
  JSONRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,2,{},20s);
  server.start();
  Client client(server.port());
  client.post(R"({"jsonrpc":"2.0","method":"mempool.get_pending_transactions","id":1,"params":{"limit":1}})");
  const auto single=nlohmann::json::parse(client.read().body());
  check(single["id"]==1 && single.contains("result") && !single.contains("error"));
  check(single["result"]["pending_transactions"].size()==1);
  client.post(R"([{"jsonrpc":"2.0","method":"mempool.get_pending_transactions","id":2,"params":{"limit":1}},{"jsonrpc":"2.0","method":"chain.get_chain_id","id":3,"params":{}}])");
  const auto mixed=nlohmann::json::parse(client.read().body());
  check(mixed.is_array() && mixed.size()==2);
  check(mixed[0]["id"]==2 && mixed[0]["result"]["pending_transactions"].size()==1);
  check(mixed[1]["id"]==3 && mixed[1].contains("result") && !mixed[1].contains("error"));
  server.stop();
}
void mempool_output_concurrent() {
  LargeMempool mempool(128 * 1024 * 1024);
  JSONRPCServer server(nullptr,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,4,{},20s);
  server.start();
  Client first(server.port()), second(server.port());
  first.stream.socket().set_option(net::socket_base::receive_buffer_size(1024));
  second.stream.socket().set_option(net::socket_base::receive_buffer_size(1024));
  const auto query=R"({"jsonrpc":"2.0","method":"mempool.get_pending_transactions","id":1,"params":{"limit":1}})";
  first.post(query); second.post(query);
  const auto deadline=std::chrono::steady_clock::now()+15s;
  while(server.http_body_memory().materialized_response_capacity < 16 * 1024 * 1024) {
    check(std::chrono::steady_clock::now() < deadline);
    std::this_thread::sleep_for(5ms);
  }
  const auto held=mempool.budget.snapshot();
  check(held.accounted_bytes >= 8 * 1024 * 1024);
  server.stop();
  check(held.accounted_bytes > mempool.budget.snapshot().accounted_bytes + 8 * 1024 * 1024);
}

// This interface fixture models the native controller's documented ordering;
// the real signed controller/admission interleaving has a separate regression.
class ReceiptChain: public Chain {
public:
  ReceiptChain(OwnedMemoryBudget budget, size_t bytes): budget(std::move(budget)), bytes(bytes) {}
  OwnedMemoryBudget budget;
  const size_t bytes;
  std::atomic<unsigned> attempts=0, prepared=0, accepted=0, blocks=0;
  std::atomic<bool> fill_before_preparation=false, refuse_after_preparation=false;
  std::atomic<bool> allocation_failure=false;
  std::atomic<bool> fail_allocation_after_acceptance=false;
  std::atomic<uint64_t> preparation_headroom=0;
  std::atomic<size_t> error_bytes=0;

  koinos::rpc::chain::submit_transaction_response submit_transaction(
    const koinos::rpc::chain::submit_transaction_request& request) override {
    return submit(request, {});
  }
  koinos::rpc::chain::submit_transaction_response submit_transaction_with_prepared_response(
    const koinos::rpc::chain::submit_transaction_request& request,
    TransactionResponsePreparer prepare) override {
    return submit(request, prepare);
  }
  koinos::rpc::chain::submit_transaction_response submit(
    const koinos::rpc::chain::submit_transaction_request& request,
    TransactionResponsePreparer prepare) {
    OwnedMemoryBudget::Scope scope(budget, false);
    ++attempts;
    if(error_bytes) throw std::runtime_error(std::string(error_bytes.load(),'e'));
    koinos::rpc::chain::submit_transaction_response response;
    response.mutable_receipt()->set_id(std::string(32, 'i'));
    response.mutable_receipt()->add_logs(std::string(bytes, 'r'));
    OwnedMemoryBudget::Lease occupied;
    if(fill_before_preparation) {
      const auto usage=budget.snapshot();
      const auto available=usage.limit_bytes-usage.accounted_bytes;
      occupied=budget.reserve(available-std::min(available,preparation_headroom.load()));
    }
    if(allocation_failure) throw std::bad_alloc();
    if(prepare) { prepare(response); ++prepared; }
    if(refuse_after_preparation) throw LocalCapacityExceeded(LocalCapacityReason::mempool_memory);
    if(request.broadcast()) {
      ++accepted;
      if(fail_allocation_after_acceptance) response_fault::fail_next_allocation=true;
    }
    return response;
  }
  koinos::rpc::chain::submit_block_response submit_block(
    const koinos::rpc::chain::submit_block_request&) override {
    check(!OwnedMemoryBudget::capture_allocation());
    ++blocks; return {};
  }
};
const std::string receipt_query=R"({"jsonrpc":"2.0","method":"chain.submit_transaction","id":71,"params":{"broadcast":true}})";
const std::string receipt_batch=R"([{"jsonrpc":"2.0","method":"chain.submit_transaction","id":71,"params":{"broadcast":true}},{"jsonrpc":"2.0","method":"chain.get_chain_id","id":72}])";

class InputLifetimeChain: public ReceiptChain {
public:
  explicit InputLifetimeChain(OwnedMemoryBudget budget):ReceiptChain(std::move(budget),4096) {}
  std::string expected_args;
  std::atomic<uint64_t> entry_accounted_bytes=0;
  koinos::rpc::chain::submit_transaction_response submit_transaction_with_prepared_response(
    const koinos::rpc::chain::submit_transaction_request& request,
    TransactionResponsePreparer prepare) override {
    check(request.transaction().operations_size()==1);
    check(request.transaction().operations(0).call_contract().args()==expected_args);
    entry_accounted_bytes=budget.snapshot().accounted_bytes;
    return ReceiptChain::submit_transaction_with_prepared_response(request,prepare);
  }
};

void transaction_input_lifetime(bool require_released) {
  constexpr size_t payload=256*1024; // Encoded request stays below the unchanged 1-MiB HTTP limit.
  LargeMempool mempool(64*1024*1024);
  InputLifetimeChain chain(mempool.budget);
  JSONRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
  server.start(); Client client(server.port());
  auto request=[&](size_t bytes) {
    chain.expected_args.assign(bytes,'a');
    return nlohmann::json{{"jsonrpc","2.0"},{"id",81},{"method","chain.submit_transaction"},
      {"params",{{"broadcast",true},{"transaction",{{"operations",nlohmann::json::array({
        {{"call_contract",{{"args",koinos::util::to_base64(chain.expected_args)}}}}
      })}}}}}}.dump();
  };
  // Warm the same generated input/output types on the single I/O worker.
  client.post(request(16));
  check(nlohmann::json::parse(client.read().body()).contains("result"));
  check(client.health().result_int()==200);
  wait_until([&]{return server.http_body_memory().materialized_response_capacity==0;});
  const auto before=mempool.budget.snapshot().accounted_bytes;
  client.post(request(payload));
  const auto reply=nlohmann::json::parse(client.read().body());
  check(reply["id"]==81 && reply["result"]["receipt"]["logs"][0]==std::string(4096,'r'));
  check(chain.prepared==2 && chain.accepted==2);
  const auto at_entry=chain.entry_accounted_bytes.load();
  check(at_entry>=before+payload); // Decoded native input still owns its bytes.
  const auto delta=at_entry-before;
  if(require_released) check(delta<2*payload); // No two redundant encoded JSON copies at service entry.
  server.stop();
  const auto after=mempool.budget.snapshot().accounted_bytes;
  std::cout<<"M1_JSON_INPUT_LIFETIME payload="<<payload<<" before="<<before
    <<" entry="<<at_entry<<" delta="<<delta<<" after="<<after<<'\n';
  check(after==before);
  std::cout<<"Transaction input ownership, exact native bytes and cleanup completed\n";
}

class NormalizationChain: public ReceiptChain {
public:
  explicit NormalizationChain(OwnedMemoryBudget budget):ReceiptChain(std::move(budget),4096) {}
  std::string expected_payer=std::string(64,'\0');
  std::atomic<unsigned> mismatches{0}, correct_inputs{0};
  koinos::rpc::chain::submit_transaction_response submit_transaction_with_prepared_response(
    const koinos::rpc::chain::submit_transaction_request& request,
    TransactionResponsePreparer prepare) override {
    if(request.transaction().header().payer()!=expected_payer) {
      ++mismatches;
      throw std::logic_error("Normalization changed native payer bytes");
    }
    ++correct_inputs;
    return ReceiptChain::submit_transaction_with_prepared_response(request,prepare);
  }
};

void transaction_normalization_refusal(bool require_safe) {
  LargeMempool mempool(64*1024*1024);
  NormalizationChain chain(mempool.budget);
  JSONRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
  server.start();
  // A synthetic base58 payer also consists of valid base64 characters. Keeping
  // it unchanged after conversion refusal would decode to different bytes.
  const auto request=nlohmann::json{{"jsonrpc","2.0"},{"id",82},{"method","chain.submit_transaction"},
    {"params",{{"broadcast",true},{"transaction",{{"header",{{"payer",std::string(64,'1')}}}}}}}}.dump();
  { Client warm(server.port()); warm.post(request);
    check(nlohmann::json::parse(warm.read().body()).contains("result")); }
  wait_until([&]{return server.active_sessions()==0;});
  const auto before=mempool.budget.snapshot().accounted_bytes;
  unsigned injected=0, misclassified_refusals=0, disconnected=0;
  for(bool bad_alloc:{false,true}) for(unsigned nth=1;nth<=12;++nth) {
    response_fault::normalization_seen=0;
    response_fault::normalization_nth=nth;
    response_fault::normalization_bad_alloc=bad_alloc;
    const auto faults=response_fault::normalization_failures.load();
    response_fault::normalization_size=65;
    { Client faulted(server.port()); faulted.post(request);
      const auto wire=faulted.read_or_disconnect();
      response_fault::normalization_size=0;
      const auto refused=response_fault::normalization_failures.load()-faults;
      injected+=refused;
      if(wire) {
        const auto reply=nlohmann::json::parse(wire->body());
        check(reply["id"]==82 && (reply.contains("result") || reply.contains("error")));
        if(refused && !bad_alloc && reply.contains("error") && reply["error"]["code"]!=-32005)
          ++misclassified_refusals;
      } else { check(refused==1); ++disconnected; }
    }
    wait_until([&]{return server.active_sessions()==0;});
    // Failure before envelope creation can close the connection. It must not
    // change native bytes, hide a timeout or prevent a healthy later request.
    const auto valid_before=chain.correct_inputs.load();
    { Client later(server.port()); later.post(request);
      check(nlohmann::json::parse(later.read().body()).contains("result")); }
    check(chain.correct_inputs==valid_before+1);
    wait_until([&]{return server.active_sessions()==0;});
  }
  // Preserve the existing fallback for valid protobuf base64 that cannot be
  // decoded as base58. Only resource failures must escape that fallback.
  chain.expected_payer.assign(64,'\xff');
  auto base64_request=nlohmann::json::parse(request);
  base64_request["params"]["transaction"]["header"]["payer"]=koinos::util::to_base64(chain.expected_payer);
  { Client later(server.port()); later.post(base64_request.dump());
    check(nlohmann::json::parse(later.read().body()).contains("result")); }
  server.stop();
  const auto after=mempool.budget.snapshot().accounted_bytes;
  std::cout<<"M1_JSON_NORMALIZATION injected="<<injected<<" mismatches="<<chain.mismatches
    <<" misclassified_refusals="<<misclassified_refusals
    <<" disconnected="<<disconnected
    <<" correct_inputs="<<chain.correct_inputs<<" before="<<before<<" after="<<after<<'\n';
  check(injected>=4 && after==before);
  if(require_safe) check(chain.mismatches==0 && misclassified_refusals==0);
  else check(chain.mismatches>0 || misclassified_refusals>0);
  std::cout<<"Normalization allocation refusal and successful retry completed\n";
}

void transaction_output_lifetime(bool expect_owned, bool mixed=false, bool concurrent=false) {
  constexpr auto payload=4*1024*1024;
  // Two overlapping conservative conversion workspaces must fit for this
  // two-success lifetime oracle. Capacity refusal has its separate fixture.
  LargeMempool mempool((concurrent ? 512 : 256)*1024*1024);
  ReceiptChain chain(mempool.budget, payload);
  JSONRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,2,{},20s);
  server.start();
  Client first(server.port());
  first.stream.socket().set_option(net::socket_base::receive_buffer_size(1024));
  first.post(mixed ? receipt_batch : receipt_query);
  std::unique_ptr<Client> second;
  if(concurrent) {
    second=std::make_unique<Client>(server.port());
    second->stream.socket().set_option(net::socket_base::receive_buffer_size(1024));
    second->post(receipt_query);
  }
  const unsigned count=concurrent ? 2 : 1;
  const auto deadline=std::chrono::steady_clock::now()+15s;
  while(server.http_body_memory().materialized_response_capacity < count*payload) {
    check(std::chrono::steady_clock::now()<deadline);
    std::this_thread::sleep_for(5ms);
  }
  const auto held=mempool.budget.snapshot();
  check(chain.accepted==count);
  if(expect_owned) {
    check(chain.prepared==count);
    check(held.accounted_bytes>=count*payload);
  } else {
    check(chain.prepared==0);
    check(server.http_body_memory().materialized_response_capacity>held.accounted_bytes+payload);
  }
  server.stop(); // Cancel pending writes while retaining every accepted call.
  const auto released=mempool.budget.snapshot();
  check(chain.accepted==count);
  check(server.http_body_memory().materialized_response_capacity==0);
  if(expect_owned) check(held.accounted_bytes>released.accounted_bytes+count*payload);
  std::cout << "M1_TRANSACTION_OUTPUT mixed=" << mixed << " concurrent=" << concurrent
            << " held_bytes=" << held.accounted_bytes << " released_bytes=" << released.accounted_bytes
            << " accepted=" << chain.accepted << '\n';
}

void transaction_error_output_lifetime(bool mixed=false) {
  constexpr auto payload=4*1024*1024;
  LargeMempool mempool(256*1024*1024);
  ReceiptChain chain(mempool.budget,0);
  chain.error_bytes=payload;
  JSONRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,2,{},20s);
  server.start(); Client client(server.port());
  client.stream.socket().set_option(net::socket_base::receive_buffer_size(1024));
  client.post(mixed ? receipt_batch : receipt_query);
  const auto deadline=std::chrono::steady_clock::now()+15s;
  while(server.http_body_memory().materialized_response_capacity<payload) {
    check(std::chrono::steady_clock::now()<deadline);
    std::this_thread::sleep_for(5ms);
  }
  const auto held=mempool.budget.snapshot();
  check(chain.accepted==0 && held.accounted_bytes>=payload);
  server.stop();
  const auto released=mempool.budget.snapshot();
  check(held.accounted_bytes>released.accounted_bytes+payload);
  std::cout << "M1_TRANSACTION_ERROR_OUTPUT mixed=" << mixed
            << " held_bytes=" << held.accounted_bytes
            << " released_bytes=" << released.accounted_bytes << '\n';
}

void transaction_error_output_capacity() {
  LargeMempool mempool(8*1024*1024);
  ReceiptChain chain(mempool.budget,0);
  chain.error_bytes=16*1024*1024;
  JSONRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
  server.start(); Client client(server.port());
  for(bool mixed:{false,true}) {
    client.post(mixed ? receipt_batch : receipt_query);
    const auto body=nlohmann::json::parse(client.read().body());
    const auto& response=mixed ? body.at(0) : body;
    check(response["error"]["code"]==-32005);
    check(response["error"]["data"]["reason"]=="mempool_memory_capacity");
    check(response["error"]["data"]["retryable"]==true);
    check(chain.accepted==0);
    if(mixed) check(body.at(1)["id"]==72 && body.at(1).contains("result"));
  }
  server.stop();
}

void transaction_output_capacity_and_retry() {
  LargeMempool mempool(16*1024*1024);
  ReceiptChain chain(mempool.budget, 16*1024);
  JSONRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,2,{},20s);
  server.start(); Client client(server.port());
  for(bool mixed:{false,true}) {
    const auto accepted=chain.accepted.load();
    const auto prepared=chain.prepared.load();
    chain.fill_before_preparation=true;
    client.post(mixed ? receipt_batch : receipt_query);
    auto body=nlohmann::json::parse(client.read().body());
    const auto& response=mixed ? body.at(0) : body;
    check(response["id"]==71 && response["error"]["code"]==-32005);
    check(response["error"]["data"]["retryable"]==true);
    check(response["error"]["data"]["reason"]=="mempool_memory_capacity");
    check(chain.accepted==accepted && chain.prepared==prepared);
    if(mixed) check(body[1]["id"]==72 && body[1].contains("result"));

    chain.fill_before_preparation=false;
    chain.refuse_after_preparation=true;
    client.post(mixed ? receipt_batch : receipt_query);
    body=nlohmann::json::parse(client.read().body());
    check((mixed ? body.at(0) : body)["error"]["code"]==-32005);
    check(chain.accepted==accepted && chain.prepared==prepared+1);
    chain.refuse_after_preparation=false;

    chain.allocation_failure=true;
    client.post(mixed ? receipt_batch : receipt_query);
    body=nlohmann::json::parse(client.read().body());
    check((mixed ? body.at(0) : body)["error"]["code"]==-32603);
    check(chain.accepted==accepted);
    chain.allocation_failure=false;

    client.post(mixed ? receipt_batch : receipt_query);
    body=nlohmann::json::parse(client.read().body());
    const auto& success=mixed ? body.at(0) : body;
    check(success["id"]==71 && success["result"]["receipt"]["logs"][0]==std::string(16*1024,'r'));
    check(chain.accepted==accepted+1);
  }
  // Exercise refusal at different conversion/encoding stages. Any successful
  // reply must keep Koinos byte encoding; resource refusal cannot silently use
  // the raw protobuf-JSON fallback or occur after the acceptance effect.
  std::string expected_id="0x";
  for(unsigned i=0;i<32;++i) expected_id+="69";
  for(uint64_t headroom:{0u,256u,1024u,4096u,16384u,32768u,65536u,131072u,262144u}) {
    chain.fill_before_preparation=true;
    chain.preparation_headroom=headroom;
    const auto before=chain.accepted.load();
    client.post(receipt_query);
    const auto response=nlohmann::json::parse(client.read().body());
    if(response.contains("error")) {
      check(response["error"]["code"]==-32005 && chain.accepted==before);
    } else {
      check(chain.accepted==before+1);
      check(response["result"]["receipt"]["logs"][0]==std::string(16*1024,'r'));
      check(response["result"]["receipt"]["id"]==expected_id);
    }
  }
  chain.fill_before_preparation=false;
  chain.preparation_headroom=0;
  wait_until([&]{return server.http_body_memory().materialized_response_capacity==0;});
  auto usage=mempool.budget.snapshot();
  auto full=mempool.budget.reserve(usage.limit_bytes-usage.accounted_bytes);
  const auto before=chain.accepted.load();
  client.post(R"([{"jsonrpc":"2.0","method":"chain.submit_transaction","id":1,"params":{"broadcast":true}},{"jsonrpc":"2.0","method":"chain.submit_block","id":2}])");
  const auto saturated=nlohmann::json::parse(client.read().body());
  check(saturated[0]["error"]["code"]==-32005);
  check(saturated[1].contains("result") && chain.blocks==1 && chain.accepted==before);
  full.reset(); server.stop();
  check(mempool.budget.snapshot().refusals>0);

  // A foreign adapter cannot silently run the ordinary mutating method and
  // discover after acceptance that it lacks pre-admission response support.
  Chain unsupported; unsupported.full=false;
  JSONRPCServer foreign(&unsupported,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
  foreign.start(); Client other(foreign.port()); other.post(receipt_query);
  const auto refused=nlohmann::json::parse(other.read().body());
  check(refused["error"]["code"]==-32603);
  check(refused["error"]["message"]=="Pre-admission transaction response preparation is unavailable");
  foreign.stop();
  std::cout << "M1 transaction capacity and retry completed\n";
}

void transaction_output_roundtrip() {
  nlohmann::json expected_single, expected_batch;
  for(bool enabled:{false,true}) {
    LargeMempool mempool(enabled ? 64*1024*1024 : 0);
    ReceiptChain chain(mempool.budget,1024);
    JSONRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
    server.start(); Client client(server.port());
    client.post(receipt_query); const auto single=nlohmann::json::parse(client.read().body());
    client.post(receipt_batch); const auto batch=nlohmann::json::parse(client.read().body());
    check(chain.accepted==2 && chain.prepared==(enabled ? 2u : 0u));
    check(single["id"]==71 && single["result"]["receipt"]["logs"][0]==std::string(1024,'r'));
    check(batch[0]==single && batch[1]["id"]==72 && batch[1].contains("result"));
    if(!enabled) { expected_single=single; expected_batch=batch; }
    else { check(single==expected_single); check(batch==expected_batch); }
    server.stop();
  }
}

void transaction_post_acceptance_failure() {
  for(bool mixed:{false,true}) {
    LargeMempool mempool(64*1024*1024);
    ReceiptChain chain(mempool.budget,64*1024);
    JSONRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
    server.start();
    // Warm fixed SDK state before selecting the precise effect/fault boundary.
    { Client warm(server.port()); warm.post(receipt_query);
      check(nlohmann::json::parse(warm.read().body()).contains("result")); }
    wait_until([&]{return server.http_body_memory().materialized_response_capacity==0;});
    const auto before=chain.accepted.load();
    const auto faults=response_fault::failures.load();
    chain.fail_allocation_after_acceptance=true;
    { Client faulted(server.port()); faulted.post(mixed ? receipt_batch : receipt_query);
      faulted.read(false); }
    wait_until([&]{return server.active_sessions()==0;});
    check(chain.accepted==before+1 && response_fault::failures==faults+1);
    check(server.http_body_memory().materialized_response_capacity==0);
    chain.fail_allocation_after_acceptance=false;
    { Client later(server.port()); later.post(receipt_query);
      check(nlohmann::json::parse(later.read().body()).contains("result")); }
    check(chain.accepted==before+2);
    server.stop();
  }
}

class RefusalProbeMempool:public LargeMempool {
public:
  RefusalProbeMempool():LargeMempool(64*1024*1024,32) {}
  std::atomic<unsigned> calls{0};
  koinos::rpc::mempool::get_pending_transactions_response get_pending_transactions(
    const koinos::rpc::mempool::get_pending_transactions_request& request) override {
    ++calls;return LargeMempool::get_pending_transactions(request);
  }
};
void method_format_compatibility()
{
  RefusalProbeMempool mempool;
  JSONRPCServer server(nullptr,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
  server.start();Client client(server.port());
  unsigned accepted=0;
  for(const auto& [method,valid]:std::vector<std::pair<std::string,bool>>{
      {"mempool.get_pending_transactions",true},
      {"koinos.rpc.mempool.get_pending_transactions",true},
      {".mempool.get_pending_transactions",true},
      {"mempool.get_pending_transactions.",true},
      {"",false},{".",false},{"..",false},{"mempool",false},
      {"mempool.",false},{"mempool..",false},
      {"mempool..get_pending_transactions",false},
      {"mempool.get_pending_transactions..",false}})
  {
    client.post(nlohmann::json{{"jsonrpc","2.0"},{"id",92},{"method",method},
      {"params",{{"limit",1}}}}.dump());
    const auto response=nlohmann::json::parse(client.read().body());
    check(response["id"]==92);
    if(valid) {
      ++accepted;
      check(response["result"]["pending_transactions"][0]["transaction"]["signatures"][0]==
        koinos::util::to_base64(std::string(32,'x')));
    }
    else check(response["error"]["code"]==-32601);
    check(mempool.calls==accepted);
  }
  server.stop();
  std::cout<<"M1_METHOD_FORMAT legacy_delimiter_cases=12 exact_native_results=4 invalid_dispatches=0\n";
}
int refusal_owner_probe(bool require_owned) {
  RefusalProbeMempool mempool;
  JSONRPCServer server(nullptr,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
  server.start();Client client(server.port());check(client.health().result_int()==200);
  client.post(R"({"jsonrpc":"2.0","id":50,"method":"mempool.get_pending_transactions","params":{"limit":1}})");
  check(nlohmann::json::parse(client.read().body()).contains("result") && mempool.calls==1);
  wait_until([&]{return server.http_body_memory().materialized_response_capacity==0;});
  mempool.calls=0;
  const auto before=mempool.budget.snapshot().accounted_bytes;
  auto occupied=mempool.budget.reserve(mempool.budget.snapshot().limit_bytes-before);
  const std::string id(128*1024,'q');
  const auto request=nlohmann::json{{"jsonrpc","2.0"},{"id",id},{"method","mempool.get_pending_transactions"},{"params",{{"limit",1}}}}.dump();
  refusal_probe::enabled=true;client.post(request);
  const auto response=nlohmann::json::parse(client.read().body());refusal_probe::enabled=false;
  check(response["id"]==id && response["error"]["code"]==-32005 &&
    response["error"]["data"]["reason"]=="mempool_memory_capacity" && response["error"]["data"]["retryable"]==true);
  check(mempool.calls==0 && refusal_probe::marked_refusals>0);
  const auto untracked=refusal_probe::untracked_large_requests.load();
  const auto untracked_bytes=refusal_probe::untracked_large_bytes.load();
  const auto tracked=refusal_probe::tracked_large_requests.load();
  const auto held=mempool.budget.snapshot().accounted_bytes;
  occupied.reset();
  client.post(R"({"jsonrpc":"2.0","id":51,"method":"mempool.get_pending_transactions","params":{"limit":1}})");
  check(nlohmann::json::parse(client.read().body()).contains("result") && mempool.calls==1);
  server.stop();const auto after=mempool.budget.snapshot().accounted_bytes;
  std::cout<<"M1_HTTP_REFUSAL_ACCOUNTING id_bytes="<<id.size()<<" actual_quota_refusals="<<refusal_probe::marked_refusals
    <<" untracked_large_requests="<<untracked<<" untracked_large_requested_bytes="<<untracked_bytes
    <<" tracked_large_requests="<<tracked<<" before="<<before<<" held="<<held<<" after="<<after
    <<" local_classification=1 refused_service_calls=0 successful_retry_calls=1\n";
  check(after==before);
  if(require_owned && untracked) return 91;
  if(!require_owned && !untracked) return 92;
  return 0;
}

void refusal_shapes()
{
  RefusalProbeMempool mempool;
  Chain chain;
  chain.released=true;
  JSONRPCServer server(&chain,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,2,{},20s);
  server.start(); Client client(server.port());
  const auto before=mempool.budget.snapshot().accounted_bytes;
  const auto gate_before=server.refusal_workspace_memory().accounted_bytes;
  auto occupied=mempool.budget.reserve(mempool.budget.snapshot().limit_bytes-before);
  std::vector<nlohmann::json> ids={nullptr,17,1.25,"q\"\n\\",std::string(192*1024,'x'),
    nlohmann::json{{"nested",nlohmann::json::array({1,"two",nullptr})}}};
  for(const auto& id:ids)
  {
    auto item=nlohmann::json{{"jsonrpc","2.0"},{"id",id},{"method","mempool.get_pending_transactions"},{"params",{{"limit",1}}}};
    auto expected=nlohmann::json{{"error",{{"code",-32005},{"data",{{"reason","mempool_memory_capacity"},{"retryable",true},{"scope","local"}}},{"message",LocalCapacityExceeded(LocalCapacityReason::mempool_memory).what()}}},{"id",id},{"jsonrpc","2.0"}};
    client.post(item.dump()); check(client.read().body()==expected.dump());
    auto transaction=item;transaction["method"]="chain.submit_transaction";
    auto other=nlohmann::json{{"jsonrpc","2.0"},{"id",71},{"method","chain.get_head_info"}};
    client.post(nlohmann::json::array({item,transaction,other}).dump());
    auto response=nlohmann::json::parse(client.read().body());
    check(response.size()==3 && response[0]==expected && response[1]==expected && response[2].contains("result"));
  }
  check(mempool.calls==0);
  wait_until([&]{return server.http_body_memory().materialized_response_capacity==0 &&
    server.refusal_workspace_memory().accounted_bytes==gate_before;});
  occupied.reset();server.stop();check(mempool.budget.snapshot().accounted_bytes==before);
  std::cout<<"M1_HTTP_REFUSAL_SHAPES exact_single_bytes=6 mixed_batches=6 rejected_service_calls=0 final_credits_exact=1\n";
}

void refusal_gate_concurrency()
{
  RefusalProbeMempool mempool;
  JSONRPCServer server(nullptr,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,4,{},20s);
  server.start();
  // All admitted fallback allocators pause after obtaining their whole-request
  // permit. A third request must close locally without entering the service;
  // releasing either completed output makes that capacity reusable.
  const std::string id(700*1024,'p');
  const auto request=nlohmann::json{{"jsonrpc","2.0"},{"id",id},{"method","mempool.get_pending_transactions"},{"params",{{"limit",1}}}}.dump();
  const auto before=mempool.budget.snapshot().accounted_bytes;
  const auto gate_before=server.refusal_workspace_memory().accounted_bytes;
  auto occupied=mempool.budget.reserve(mempool.budget.snapshot().limit_bytes-before);
  refusal_probe::enabled=true; refusal_probe::pause_large=true;
  refusal_probe::release_large=false; refusal_probe::paused=0;
  Client first(server.port()),second(server.port()),third(server.port());
  try
  {
    first.post(request);wait_until([&]{return refusal_probe::paused>=1;});
    second.post(request);wait_until([&]{return refusal_probe::paused>=2;});
    const auto held=server.refusal_workspace_memory();
    check(held.accounted_bytes==gate_before+2*(32*request.size()+65536) && held.accounted_bytes<=held.limit_bytes);
    third.post(request);third.read(false);
    check(server.refusal_workspace_memory().refusals==1 && mempool.calls==0);
    refusal_probe::pause_large=false;refusal_probe::release_large=true;
    for(auto* client:{&first,&second}) {
      auto response=nlohmann::json::parse(client->read().body());
      check(response["id"]==id && response["error"]["code"]==-32005);
    }
    wait_until([&]{return server.refusal_workspace_memory().accounted_bytes==gate_before;});
    refusal_probe::enabled=false;occupied.reset();
    first.post(R"({"jsonrpc":"2.0","id":51,"method":"mempool.get_pending_transactions","params":{"limit":1}})");
    check(nlohmann::json::parse(first.read().body()).contains("result") && mempool.calls==1);
    server.stop();
  }
  catch(...)
  {
    refusal_probe::pause_large=false;refusal_probe::release_large=true;refusal_probe::enabled=false;
    server.stop();throw;
  }
  // The retry may initialize process-global conversion metadata. Its attribution
  // is deliberately not mistaken for retained fallback bytes in this cold test.
  check(server.refusal_workspace_memory().accounted_bytes==gate_before);
  std::cout<<"M1_HTTP_REFUSAL_GATE simultaneous_permits=2 third_refused=1 service_calls_before_retry=0 successful_retry=1 permits_released=1\n";
}

void refusal_fault_cleanup()
{
  unsigned injected=0;bool completed=false;
  for(unsigned nth=1;nth<=128;++nth)
  {
    RefusalProbeMempool mempool;
    JSONRPCServer server(nullptr,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
    server.start();Client client(server.port());
    client.post(R"({"jsonrpc":"2.0","id":50,"method":"mempool.get_pending_transactions","params":{"limit":1}})");
    check(nlohmann::json::parse(client.read().body()).contains("result"));
    wait_until([&]{return server.http_body_memory().materialized_response_capacity==0;});
    const auto before=mempool.budget.snapshot().accounted_bytes;
    const auto gate_before=server.refusal_workspace_memory().accounted_bytes;
    auto occupied=mempool.budget.reserve(mempool.budget.snapshot().limit_bytes-before);
    const auto request=nlohmann::json{{"jsonrpc","2.0"},{"id",std::string(128*1024,'f')},
      {"method","mempool.get_pending_transactions"},{"params",{{"limit",1}}}}.dump();
    refusal_probe::fault_seen=0;refusal_probe::faults=0;refusal_probe::fault_nth=nth;refusal_probe::enabled=true;
    client.post(request);const auto response=client.read(false,false);
    refusal_probe::enabled=false;refusal_probe::fault_nth=0;
    const bool faulted=refusal_probe::faults!=0;
    if(!faulted) check(nlohmann::json::parse(response.body())["error"]["code"]==-32005);
    wait_until([&]{return server.refusal_workspace_memory().accounted_bytes==gate_before;});
    occupied.reset();
    // This fixture permits one session. A successful terminal refusal keeps
    // its connection alive; retire it before checking a new connection's retry.
    beast::error_code ignored;
    client.stream.socket().shutdown(tcp::socket::shutdown_both,ignored);
    client.stream.socket().close(ignored);
    wait_until([&]{return server.active_sessions()==0;});
    // Every injected fallback allocation failure must leave this same service
    // usable. Use a new connection because failure may close the old one.
    const auto calls=mempool.calls.load();
    { Client retry(server.port());
      retry.post(R"({"jsonrpc":"2.0","id":51,"method":"mempool.get_pending_transactions","params":{"limit":1}})");
      check(nlohmann::json::parse(retry.read().body()).contains("result")); }
    check(mempool.calls==calls+1);
    server.stop();check(server.refusal_workspace_memory().accounted_bytes==gate_before);
    check(mempool.budget.snapshot().accounted_bytes==before);
    if(faulted) ++injected;
    else {completed=true;break;}
  }
  check(injected>0 && completed);
  std::cout<<"M1_HTTP_REFUSAL_FAULTS injected="<<injected<<" successful_terminal=1 final_credits_exact=1\n";
}
class ConversionFaultMempool:public LargeMempool {
public:
  ConversionFaultMempool():LargeMempool(64*1024*1024,32) {}
  bool enable_fault=false;
  koinos::rpc::mempool::get_pending_transactions_response get_pending_transactions(
    const koinos::rpc::mempool::get_pending_transactions_request& request) override {
    auto result=LargeMempool::get_pending_transactions(request);
    if(enable_fault) conversion_fault::active=true;
    return result;
  }
};
int generic_json_fault_probe() {
  unsigned injected=0;
  for(unsigned nth=1;nth<=2048;++nth) {
    ConversionFaultMempool mempool;
    JSONRPCServer server(nullptr,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
    server.start();Client client(server.port());
    const std::string request=R"({"jsonrpc":"2.0","id":91,"method":"mempool.get_pending_transactions","params":{"limit":1}})";
    client.post(request);check(nlohmann::json::parse(client.read().body()).contains("result"));
    wait_until([&]{return server.http_body_memory().materialized_response_capacity==0;});
    const auto before=mempool.budget.snapshot().accounted_bytes;
    mempool.signature_bytes=32768;mempool.enable_fault=true;
    conversion_fault::nth=nth;conversion_fault::seen=0;conversion_fault::fired=0;
    client.post(request);auto response=nlohmann::json::parse(client.read().body());
    const auto fired=conversion_fault::fired.load();const auto seen=conversion_fault::seen.load();
    conversion_fault::nth=0;mempool.enable_fault=false;
    // Reuse the same service after each refused conversion. The native result
    // must keep its exact bytes; a successful retry alone cannot hide a leak.
    client.post(request);const auto retry=nlohmann::json::parse(client.read().body());
    check(retry["result"]["pending_transactions"][0]["transaction"]["signatures"][0]==
      koinos::util::to_base64(std::string(32768,'x')));
    server.stop();const auto after=mempool.budget.snapshot().accounted_bytes;
    if(after!=before) {
      std::cout<<"GENERIC_JSON_FAULT_RESULT outcome=unreleased-credit ordinal="<<nth
        <<" faults="<<fired<<" seen="<<seen<<" before="<<before<<" after="<<after
        <<" delta="<<(int64_t(after)-int64_t(before))<<"\n"<<std::flush;
      return 81;
    }
    if(fired) {
      ++injected;
      if(!response.contains("error") || response["error"]["code"]!=-32005) {
        std::cout<<"GENERIC_JSON_FAULT_RESULT outcome=wrong-classification ordinal="<<nth
          <<" faults="<<fired<<" final_credits_exact=1\n"<<std::flush;return 82;
      }
    }
    else {
      check(response.contains("result") && injected>0);
      std::cout<<"GENERIC_JSON_FAULT_RESULT outcome=clean-complete injected="<<injected
        <<" final_credits_exact=1 terminal_ordinal="<<nth<<" seen="<<seen<<"\n"<<std::flush;return 0;
    }
  }
  throw std::runtime_error("Generic JSON bounded sweep did not reach an unfaulted terminal path");
}


class InputConversionMempool:public LargeMempool {
public:
  InputConversionMempool():LargeMempool(64*1024*1024,32) {}
  std::mutex expected_mutex;std::string expected_nonce;
  std::atomic<unsigned> calls{0},wrong_inputs{0};
  void expect(std::string value) {std::lock_guard lock(expected_mutex);expected_nonce=std::move(value);}
  koinos::rpc::mempool::get_reserved_account_rc_response get_reserved_account_rc(
    const koinos::rpc::mempool::get_reserved_account_rc_request& request) override {
    std::lock_guard lock(expected_mutex);++calls;
    if(request.account()!=expected_nonce) ++wrong_inputs;
    return {};
  }
};
int input_json_fault_probe() {
  unsigned injected=0;
  for(unsigned nth=1;nth<=2048;++nth) {
    InputConversionMempool mempool;mempool.expect(std::string(16,'n'));
    JSONRPCServer server(nullptr,&mempool,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
    server.start();Client client(server.port());
    auto request=[](size_t bytes){return nlohmann::json{{"jsonrpc","2.0"},{"id",92},
      {"method","mempool.get_reserved_account_rc"},{"params",{{"account",koinos::util::to_base64(std::string(bytes,'n'))}}}}.dump();};
    client.post(request(16));check(nlohmann::json::parse(client.read().body()).contains("result") && mempool.wrong_inputs==0);
    wait_until([&]{return server.http_body_memory().materialized_response_capacity==0;});
    const auto before=mempool.budget.snapshot().accounted_bytes;mempool.expect(std::string(32768,'n'));
    const auto large=request(32768);
    input_conversion_fault::nth=nth;input_conversion_fault::seen=0;input_conversion_fault::fired=0;input_conversion_fault::enabled=true;
    client.post(large);auto response=nlohmann::json::parse(client.read().body());
    input_conversion_fault::enabled=false;const auto fired=input_conversion_fault::fired.load();
    if(mempool.wrong_inputs) {server.stop();std::cout<<"GENERIC_JSON_INPUT_FAULT_RESULT outcome=wrong-native-input ordinal="<<nth<<"\n"<<std::flush;return 83;}
    if(fired && (!response.contains("error") || response["error"]["code"]!=-32005)) {
      server.stop();std::cout<<"GENERIC_JSON_INPUT_FAULT_RESULT outcome=wrong-classification ordinal="<<nth<<"\n"<<std::flush;return 82;
    }
    client.post(large);check(nlohmann::json::parse(client.read().body()).contains("result") && mempool.wrong_inputs==0);
    server.stop();const auto after=mempool.budget.snapshot().accounted_bytes;
    if(after!=before) {
      std::cout<<"GENERIC_JSON_INPUT_FAULT_RESULT outcome=unreleased-credit ordinal="<<nth<<" faults="<<fired
        <<" before="<<before<<" after="<<after<<" delta="<<(int64_t(after)-int64_t(before))<<" successful_retry=1\n"<<std::flush;return 81;
    }
    if(fired) ++injected;
    else {check(response.contains("result") && injected>0);std::cout<<"GENERIC_JSON_INPUT_FAULT_RESULT outcome=clean-complete injected="<<injected
      <<" terminal_ordinal="<<nth<<" final_credits_exact=1 successful_retry=1 wrong_native_inputs=0\n"<<std::flush;return 0;}
  }
  throw std::runtime_error("Generic JSON input sweep did not reach an unfaulted terminal path");
}


int main(int argc,char** argv) {
  if(argc==2 && std::string(argv[1])=="--m1-method-format") {
    try {method_format_compatibility();return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
  }
  if(argc==2 && std::string(argv[1])=="--generic-json-input-faults") {
    try {return input_json_fault_probe();}
    catch(const std::exception& error) {input_conversion_fault::enabled=false;std::cerr<<error.what()<<'\n';return 1;}
  }

  if(argc==2 && std::string(argv[1])=="--generic-json-faults") {
    try {return generic_json_fault_probe();}
    catch(const std::exception& error) {conversion_fault::nth=0;std::cerr<<error.what()<<'\n';return 1;}
  }

  if(argc==2 && std::string(argv[1])=="--http-refusal-faults") {
    try { refusal_fault_cleanup();return 0; }
    catch(const std::exception& e) {refusal_probe::enabled=false;refusal_probe::fault_nth=0;std::cerr<<e.what()<<'\n';return 1;}
  }
  if(argc==2 && std::string(argv[1])=="--http-refusal-shapes") {
    try { refusal_shapes();return 0; }
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
  }
  if(argc==2 && std::string(argv[1])=="--http-refusal-gate") {
    try { refusal_gate_concurrency();return 0; }
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
  }
  if(argc==2 && (std::string(argv[1])=="--http-refusal-owner-gap" || std::string(argv[1])=="--http-refusal-owner-owned")) {
    try { return refusal_owner_probe(std::string(argv[1])=="--http-refusal-owner-owned"); }
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
  }

  if(argc==2 && (std::string(argv[1])=="--m1-transaction-normalization-refusal" ||
                std::string(argv[1])=="--m1-transaction-normalization-gap")) {
    try { transaction_normalization_refusal(std::string(argv[1])=="--m1-transaction-normalization-refusal"); return 0; }
    catch(const std::exception& e) { response_fault::normalization_size=0; std::cerr<<e.what()<<'\n'; return 1; }
  }
  if(argc==2 && (std::string(argv[1])=="--m1-transaction-input-lifetime" ||
                std::string(argv[1])=="--m1-transaction-input-lifetime-gap")) {
    try { transaction_input_lifetime(std::string(argv[1])=="--m1-transaction-input-lifetime"); return 0; }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-transaction-output-owned") {
    try { transaction_output_lifetime(true); transaction_output_lifetime(true,true); return 0; }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-transaction-output-gap") {
    try { transaction_output_lifetime(false); transaction_output_lifetime(false,true); return 0; }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-transaction-output-concurrent") {
    try { transaction_output_lifetime(true,false,true); return 0; }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-transaction-capacity") {
    try { transaction_output_capacity_and_retry(); return 0; }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-transaction-error-output-owned") {
    try { transaction_error_output_lifetime(); transaction_error_output_lifetime(true); return 0; }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-transaction-error-capacity") {
    try { transaction_error_output_capacity(); return 0; }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-transaction-roundtrip") {
    try { transaction_output_roundtrip(); return 0; }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-transaction-post-acceptance-failure") {
    try { transaction_post_acceptance_failure(); return 0; }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-output-concurrent") {
    try { mempool_output_concurrent(); return 0; }
    catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-output-roundtrip") {
    try { mempool_output_roundtrip(); return 0; }
    catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-mixed-batch-capacity") {
    try { mempool_mixed_batch_capacity(); return 0; }
    catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-mixed-batch-owned") {
    try { mempool_output_lifetime(true,true); return 0; }
    catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
  }
  if(argc==2 && std::string(argv[1])=="--m1-mixed-batch-gap") {
    try { mempool_output_lifetime(false,true); return 0; }
    catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
  }
  if(argc==2 && (std::string(argv[1])=="--m1-output-gap" || std::string(argv[1])=="--m1-output-owned")) {
    try { mempool_output_lifetime(std::string(argv[1])=="--m1-output-owned"); return 0; }
    catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
  }
  if(argc==2 && (std::string(argv[1])=="--body-lifetime-only" || std::string(argv[1])=="--expect-retained")) {
    try { body_lifetime(std::string(argv[1])=="--expect-retained"); return 0; }
    catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
  }
  JSONRPCServer server(nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},200ms);
  for(int i=0;i<20;++i) {
    server.start(); server.start();
    Client first(server.port()); check(first.health().result_int()==200);
    wait_until([&]{return server.active_sessions()==1;});
    { Client overloaded(server.port()); check(overloaded.read().result_int()==503); }
    check(first.health().result_int()==200);
    wait_until([&]{return server.active_sessions()==0;});
    { Client next(server.port()); check(next.health().result_int()==200); }
    wait_until([&]{return server.active_sessions()==0;});
    Client partial(server.port()); partial.send("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\n{");
    wait_until([&]{return server.active_sessions()==1;});
    auto start=std::chrono::steady_clock::now(); server.stop(); server.stop();
    check(std::chrono::steady_clock::now()-start<1s); check(server.active_sessions()==0);
    partial.read(false);
  }
  // No HTTP byte has arrived: an idle connection must also expire.
  server.start(); { Client idle(server.port()); wait_until([&]{return server.active_sessions()==1;});
    wait_until([&]{return server.active_sessions()==0;}); idle.read(false); }
  // Deadline is absolute over the complete body, not refreshed by each byte.
  { Client partial(server.port()); partial.send("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\n{");
    wait_until([&]{return server.active_sessions()==1;}); wait_until([&]{return server.active_sessions()==0;}); partial.read(false); }
  { Client limit(server.port()); limit.send("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1048577\r\n\r\n"); limit.read(false); }
  { Client rpc(server.port()); std::string body=R"({"jsonrpc":"2.0","method":"missing.method","id":1})";
    rpc.send("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body);
    auto response=rpc.read(); check(response.result_int()==200);
    check(nlohmann::json::parse(response.body())["error"]["code"]==-32603); }
  server.stop(); check(server.active_sessions()==0);
  Chain chain;
  JSONRPCServer slow(&chain,nullptr,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,2,{},200ms);
  slow.start(); Client call(slow.port());
  std::string body=R"({"jsonrpc":"2.0","method":"chain.get_head_info","id":1})";
  call.send("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body);
  { std::unique_lock lock(chain.mutex); check(chain.cv.wait_for(lock,1s,[&]{return chain.entered;})); }
  auto stopped=std::async(std::launch::async,[&]{slow.stop();});
  check(stopped.wait_for(50ms)==std::future_status::timeout);
  { std::lock_guard lock(chain.mutex); chain.released=true; chain.cv.notify_all(); }
  check(stopped.wait_for(1s)==std::future_status::ready); stopped.get(); check(slow.active_sessions()==0); call.read(false);
  slow.start();
  { Client retry(slow.port());
    body=R"({"jsonrpc":"2.0","method":"chain.submit_transaction","id":73,"params":{}})";
    auto send=[&]{retry.send("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body);};
    send(); auto result=nlohmann::json::parse(retry.read().body());
    check(result["id"]==73 && result["error"]["code"]==-32005);
    check(result["error"]["data"]["retryable"]==true && result["error"]["data"]["scope"]=="local");
    check(!result.contains("result"));
    check(result["error"]["data"]["reason"]=="transaction_work_capacity");
    chain.reason=LocalCapacityReason::pending_record; send(); result=nlohmann::json::parse(retry.read().body());
    check(result["id"]==73 && result["error"]["code"]==-32005 && !result.contains("result"));
    check(result["error"]["data"]["reason"]=="pending_record_capacity" && result["error"]["data"]["retryable"]==true);
    chain.reason=LocalCapacityReason::mempool_memory; send(); result=nlohmann::json::parse(retry.read().body());
    check(result["id"]==73 && result["error"]["code"]==-32005 && !result.contains("result"));
    check(result["error"]["data"]["reason"]=="mempool_memory_capacity" && result["error"]["data"]["retryable"]==true);
    chain.full=false; send(); result=nlohmann::json::parse(retry.read().body());
    check(result.contains("result") && !result.contains("error"));
  }
  { Client blocked(slow.port()); blocked.stream.socket().set_option(net::socket_base::receive_buffer_size(1024));
    body=R"({"jsonrpc":"2.0","method":"chain.read_contract","id":2,"params":{}})";
    blocked.send("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body);
    wait_until([&]{return slow.active_sessions()==1;}); wait_until([&]{return slow.active_sessions()==0;}); }
  slow.stop();
  // Destruction also owns/drains an idle socket.
  auto owned=std::make_unique<JSONRPCServer>(nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,2);
  owned->start(); Client idle(owned->port()); wait_until([&]{return owned->active_sessions()==1;}); owned.reset(); idle.read(false);
  std::cout << "RPC lifecycle passed\n";
  body_lifetime();
  body_transitions();
}
