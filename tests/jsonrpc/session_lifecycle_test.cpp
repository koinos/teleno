#include "jsonrpc/jsonrpc_server.hpp"
#include "core/transaction_work_budget.hpp"
#include <koinos/util/base64.hpp>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <source_location>
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
  http::response<http::string_body> read(bool success=true) {
    http::response_parser<http::string_body> parser;
    parser.body_limit(32*1024*1024); // Test client receives complete large responses.
    beast::error_code error;
    stream.expires_after(2s); io.restart();
    http::async_read(stream,buffer,parser,[&](beast::error_code ec,std::size_t){error=ec;});
    io.run(); check(bool(error)!=success); return parser.release();
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
int main(int argc,char** argv) {
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
