#include "core/owned_allocation.hpp"
#include "jsonrpc/jsonrpc_server.hpp"
#include <koinos/util/base64.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <sys/resource.h>

// Only ordinary C++ requests on the single service worker. No aligned new,
// direct malloc, usable-size/allocator retention, driver heap or process ceiling.
namespace allocation_probe {
struct Slot { void* pointer = nullptr; std::size_t bytes = 0; };
thread_local std::array<Slot,4096> slots{};
thread_local bool enabled = false;
thread_local std::size_t live = 0, live_bytes = 0, peak_bytes = 0, total_bytes = 0;
thread_local std::size_t large_count = 0, large_bytes = 0;
constexpr std::size_t large_threshold = 32 * 1024;
}
void* operator new(std::size_t bytes) {
  void* pointer = koinos::node::owned_allocation::allocate(bytes ? bytes : 1);
  if(!pointer) throw std::bad_alloc();
  using namespace allocation_probe;
  if(enabled) {
    auto found = std::find_if(slots.begin(),slots.end(),[](const auto& s){return !s.pointer;});
    if(found==slots.end()) std::abort();
    *found = {pointer,bytes}; ++live; live_bytes+=bytes; total_bytes+=bytes;
    peak_bytes=std::max(peak_bytes,live_bytes);
    if(bytes>=large_threshold) { ++large_count; large_bytes+=bytes; }
  }
  return pointer;
}
void operator delete(void* pointer) noexcept {
  using namespace allocation_probe;
  if(pointer && live) {
    auto found=std::find_if(slots.begin(),slots.end(),[&](const auto& s){return s.pointer==pointer;});
    if(found!=slots.end()) {
      --live; live_bytes-=found->bytes;
      if(found->bytes>=large_threshold) { --large_count; large_bytes-=found->bytes; }
      *found={};
    }
  }
  koinos::node::owned_allocation::deallocate(pointer);
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }

namespace {
using namespace koinos::node;
using namespace koinos::node::jsonrpc;
using namespace std::chrono_literals;
using json = nlohmann::json;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
class Chain final: public IChain {
public:
  std::atomic<unsigned> reads{0}, submissions{0};
  std::atomic<bool> measure{false};
  bool invalid_log = false;
  std::size_t payload_bytes = 512*1024;
  std::array<std::size_t,12> prior_large_counts{}, prior_large_bytes{};
  std::size_t measured_peak = 0, measured_total = 0, final_large = 0;
  koinos::rpc::chain::read_contract_response read_contract(const koinos::rpc::chain::read_contract_request&) override {
    const auto ordinal=reads++;
    if(measure) {
      using namespace allocation_probe;
      if(!ordinal) { require(!live,"previous measured owners survive");
        live_bytes=peak_bytes=total_bytes=large_count=large_bytes=0; enabled=true; }
      require(ordinal<prior_large_counts.size(),"bounded fixture exhausted");
      prior_large_counts[ordinal]=large_count; prior_large_bytes[ordinal]=large_bytes;
    }
    koinos::rpc::chain::read_contract_response response;
    response.set_result(std::string(payload_bytes,'x'));
    if(invalid_log) response.add_logs(std::string(1,char(0xff)));
    return response;
  }
  koinos::rpc::chain::get_chain_id_response get_chain_id(const koinos::rpc::chain::get_chain_id_request&) override {
    if(measure) { measured_peak=allocation_probe::peak_bytes; measured_total=allocation_probe::total_bytes;
      final_large=allocation_probe::large_count; allocation_probe::enabled=false; }
    return {};
  }
  koinos::rpc::chain::submit_transaction_response submit_transaction(const koinos::rpc::chain::submit_transaction_request&) override {
    ++submissions; return {};
  }
#define EMPTY(method) koinos::rpc::chain::method##_response method(const koinos::rpc::chain::method##_request&) override { return {}; }
  EMPTY(submit_block) EMPTY(get_head_info) EMPTY(get_fork_heads) EMPTY(get_account_nonce)
  EMPTY(get_account_rc) EMPTY(get_resource_limits) EMPTY(invoke_system_call) EMPTY(propose_block)
#undef EMPTY
};
struct Client {
  net::io_context io;
  beast::tcp_stream stream{io};
  beast::flat_buffer buffer;
  explicit Client(uint16_t port) { stream.connect({net::ip::make_address("127.0.0.1"),port}); }
  void post(const std::string& body) {
    const auto request="POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
    net::write(stream.socket(),net::buffer(request));
  }
  std::string read(bool success=true) {
    http::response_parser<http::string_body> parser;
    parser.body_limit(32*1024*1024);
    beast::error_code error; stream.expires_after(5s); io.restart();
    http::async_read(stream,buffer,parser,[&](auto ec,std::size_t){error=ec;}); io.run();
    require(bool(error)!=success,"unexpected HTTP outcome");
    if(!success) require(!parser.got_some() && buffer.size()==0,"failed batch sent partial HTTP bytes");
    return success ? parser.release().body() : std::string();
  }
};
json request(const std::string& method,json id) { return {{"jsonrpc","2.0"},{"method",method},{"id",std::move(id)}}; }
void measure() {
  Chain chain;
  JSONRPCServer server(&chain,nullptr,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
  server.start(); Client client(server.port());
  // Warm the same conversion path before enabling worker-thread attribution.
  client.post(request("chain.read_contract",0).dump()); (void)client.read();
  chain.reads=0; chain.measure=true;
  json batch=json::array(); json expected=json::array();
  const auto encoded=koinos::util::to_base64(std::string(chain.payload_bytes,'x'));
  for(int i=0;i<12;++i) {
    batch.push_back(request("chain.read_contract",i));
    expected.push_back({{"jsonrpc","2.0"},{"id",i},{"result",{{"result",encoded}}}});
  }
  const auto start=std::chrono::steady_clock::now(); client.post(batch.dump());
  auto response=client.read();
  require(response==expected.dump(),"batch bytes/order/content changed");
  client.post(request("chain.get_chain_id",99).dump()); (void)client.read();
  server.stop();
  require(chain.reads==12 && chain.final_large==0,"batch count/large-owner cleanup changed");
  require(std::all_of(chain.prior_large_counts.begin(),chain.prior_large_counts.end(),
    [](auto count){return count<=1;}),"earlier batch result trees remain owned");
  const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
  rusage usage{}; require(!getrusage(RUSAGE_SELF,&usage),"process RSS unavailable");
#if defined(__APPLE__)
  const auto rss=uint64_t(usage.ru_maxrss);
#else
  const auto rss=uint64_t(usage.ru_maxrss)*1024;
#endif
  std::cout << json({{"format","lr2-r1b-batch"},{"items",12},{"result_bytes_per_item",chain.payload_bytes},
    {"response_bytes",response.size()},{"prior_large_counts",chain.prior_large_counts},
    {"prior_large_requested_bytes",chain.prior_large_bytes},{"peak_server_cpp_requested_bytes",chain.measured_peak},
    {"total_server_cpp_requested_bytes",chain.measured_total},{"final_large_allocations",chain.final_large},
    {"elapsed_fixture_us",elapsed},{"process_peak_rss_bytes",rss}}).dump() << '\n';
}
void compatibility() {
  Chain chain; chain.payload_bytes=0;
  JSONRPCServer server(&chain,nullptr,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
  server.start(); Client client(server.port());
  json batch=json::array(),expected=json::array();
  for(const auto& id:json::array({nullptr,42,42.25,"quote\" newline\n emoji 🦉",json::array({1,"x"})})) {
    batch.push_back(request("chain.read_contract",id));
    expected.push_back({{"jsonrpc","2.0"},{"id",id},{"result",{{"result",""}}}});
  }
  client.post(batch.dump()); require(client.read()==expected.dump(),"escaped/mixed ID bytes changed");
  client.post("[]"); require(client.read()=="[]","empty batch changed");
  batch=json::array({request("chain.submit_transaction",1),request("missing.method",2),request("chain.submit_transaction",3)});
  client.post(batch.dump()); auto actual=json::parse(client.read());
  require(chain.submissions==2 && actual.size()==3 && actual[0].contains("result") && actual[1]["error"]["code"]==-32603 && actual[2].contains("result"),"mixed call order/results changed");
  server.stop();

  // Original code dispatches all items before strict UTF-8 serialization fails.
  // A memory improvement must not skip later accepted calls on that path.
  Chain malformed; malformed.payload_bytes=0; malformed.invalid_log=true;
  JSONRPCServer invalid(&malformed,nullptr,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
  invalid.start(); Client bad(invalid.port());
  batch=json::array({request("chain.submit_transaction",1),request("chain.read_contract",2),request("chain.submit_transaction",3)});
  bad.post(batch.dump()); bad.read(false); invalid.stop();
  require(malformed.reads==1 && malformed.submissions==2,"serialization failure skipped later batch work");
  for(bool read_first:{true,false}) {
    Chain edge; edge.payload_bytes=0; edge.invalid_log=true;
    JSONRPCServer endpoint(&edge,nullptr,nullptr,nullptr,nullptr,nullptr,"127.0.0.1",0,1,{},20s);
    endpoint.start(); Client reader(endpoint.port());
    batch=json::array();
    if(read_first) batch.push_back(request("chain.read_contract",1));
    batch.push_back(request("chain.submit_transaction",2));
    batch.push_back(request("chain.submit_transaction",3));
    batch.push_back(request("chain.read_contract",4));
    reader.post(batch.dump()); reader.read(false); endpoint.stop();
    require(edge.reads==(read_first?2u:1u) && edge.submissions==2,
      "first/multiple/last serialization failure changed dispatch count");
  }
}
}
int main(int argc,char** argv) {
  try { if(argc==2 && std::string(argv[1])=="--measure") measure();
    else { compatibility(); measure(); }
    return 0;
  } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
