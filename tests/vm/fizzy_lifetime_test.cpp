#include <koinos/vm_manager/fizzy/fizzy_vm_backend.hpp>
#include <koinos/vm_manager/fizzy/exceptions.hpp>
#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <new>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <sys/resource.h>

// Ordinary C++ allocation requests, not allocator usable size/retention, aligned
// new, malloc-backed execution stacks, dependency races or total heap/RSS.
static thread_local bool track=false;
static thread_local long fail_at=-1;
static thread_local bool fired=false;
struct Allocation { void* ptr; size_t bytes; };
static thread_local Allocation tracked[65536]{};
static thread_local size_t live=0, live_bytes=0, peak_bytes=0, requested=0, allocation_calls=0;
void* operator new(size_t n) {
  if(track)++allocation_calls;
  if(fail_at>=0 && fail_at--==0){fail_at=-1;fired=true;throw std::bad_alloc();}
  auto p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();
  if(track){bool stored=false;for(auto& slot:tracked)if(!slot.ptr){slot={p,n};stored=true;break;}
    if(!stored)std::abort();++live;live_bytes+=n;requested+=n;peak_bytes=std::max(peak_bytes,live_bytes);}
  return p;
}
void operator delete(void* p)noexcept{if(p&&live)for(auto& slot:tracked)if(slot.ptr==p){--live;live_bytes-=slot.bytes;slot={};break;}std::free(p);}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,size_t)noexcept{::operator delete(p);}

namespace koinos::vm_manager::fizzy {
module_ptr parse_bytecode(const char*,size_t);
}
namespace {
namespace vm=koinos::vm_manager;
namespace fv=vm::fizzy;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
const std::string empty_wasm("\0asm\1\0\0\0",8);
void reset_tracking(){require(!live,"previous measured allocations still alive");live_bytes=peak_bytes=requested=allocation_calls=0;}
void parse_fault(size_t ordinal){
  reset_tracking();track=true;fail_at=static_cast<long>(ordinal);fired=false;bool failed=false;
  try{auto module=fv::parse_bytecode(empty_wasm.data(),empty_wasm.size());}
  catch(...){failed=true;}
  fail_at=-1;track=false;
  std::cout<<"parse_ordinal="<<ordinal<<" injected="<<fired<<" rejected="<<failed<<" live_allocations="<<live<<" live_requested_bytes="<<live_bytes<<"\n";
  require(!live,"parsed module leaked when shared ownership allocation failed");
}
void uleb(std::string& out,uint32_t value){do{auto byte=uint8_t(value&127);value>>=7;out.push_back(char(byte|(value?128:0)));}while(value);}
void section(std::string& out,uint8_t id,const std::string& data){out.push_back(char(id));uleb(out,data.size());out+=data;}
std::string bytecode(uint32_t data_bytes=0){
  auto out=empty_wasm;
  section(out,1,std::string("\1\x60\0\0",4));
  section(out,3,std::string("\1\0",2));
  std::string memory("\1\0",2);uleb(memory,std::max<uint32_t>(1,(data_bytes+65535)/65536));section(out,5,memory);
  section(out,7,std::string("\1\6_start\0\0",10));
  section(out,10,std::string("\1\5\0\x41\7\x1a\x0b",7));
  if(data_bytes){std::string data("\1\0\x41\0\x0b",5);uleb(data,data_bytes);data.append(data_bytes,'d');section(out,11,data);}
  return out;
}
class Host:public vm::abstract_host_api {
public:
  int64_t ticks=1'000'000;uint64_t spent=0;
  int32_t invoke_thunk(uint32_t,char*,uint32_t,const char*,uint32_t,uint32_t*)override{throw std::runtime_error("unexpected thunk");}
  int32_t invoke_system_call(uint32_t,char*,uint32_t,const char*,uint32_t,uint32_t*)override{throw std::runtime_error("unexpected syscall");}
  int64_t get_meter_ticks()const override{return ticks;}
  void use_meter_ticks(uint64_t n)override{require(n<=uint64_t(ticks),"meter underflow");ticks-=n;spent+=n;}
};
void measure(const std::string& mode,uint64_t capacity=0){
  reset_tracking();auto code=bytecode(128*1024);track=true;
  const auto start=std::chrono::steady_clock::now();size_t cached_live=0,cached_bytes=0;uint64_t ticks=0;
  vm::module_cache_memory cache_memory;
  {fv::fizzy_vm_backend backend(capacity);
    for(int i=0;i<16;++i){Host host;backend.run(host,code,mode=="uncached"?"":"contract-"+std::to_string(i));ticks+=host.spent;}
    cached_live=live;cached_bytes=live_bytes;
    if(mode=="warm")for(int i=0;i<80;++i){Host host;backend.run(host,code,"contract-"+std::to_string(i%16));ticks+=host.spent;}
    cache_memory=backend.cache_memory();
    require(!capacity || cache_memory.capacity_bytes<=capacity,"measured cache exceeded byte budget");
  }
  track=false;require(!live,"backend retained measured allocations after destruction");
  const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
  rusage rss{};require(!getrusage(RUSAGE_SELF,&rss),"RSS measurement failed");
#if defined(__APPLE__)
  const auto peak_rss=uint64_t(rss.ru_maxrss);
#else
  const auto peak_rss=uint64_t(rss.ru_maxrss)*1024;
#endif
  std::cout<<"{\"format\":\"lr2-v1a-vm\",\"mode\":\""<<mode<<"\",\"contracts\":16,\"data_bytes\":131072,\"meter_ticks\":"<<ticks<<",\"retained_cpp_allocations\":"<<cached_live<<",\"retained_cpp_requested_bytes\":"<<cached_bytes<<",\"peak_cpp_requested_bytes\":"<<peak_bytes<<",\"total_cpp_requested_bytes\":"<<requested<<",\"final_cpp_allocations\":"<<live<<",\"elapsed_us\":"<<elapsed<<",\"process_peak_rss_bytes\":"<<peak_rss
    <<",\"cache_entries\":"<<cache_memory.entries<<",\"cache_capacity_bytes\":"<<cache_memory.capacity_bytes
    <<",\"cache_max_capacity_bytes\":"<<capacity<<",\"lookup_hits\":"<<cache_memory.lookup_hits
    <<",\"lookup_misses\":"<<cache_memory.lookup_misses<<",\"evictions\":"<<cache_memory.evictions
    <<",\"retention_bypasses\":"<<cache_memory.retention_bypasses<<"}\n";
}
void test_execution(){auto code=bytecode(65536);uint64_t expected=0;
  for(uint64_t capacity:{uint64_t(0),uint64_t(1),uint64_t(262144)})
  {fv::fizzy_vm_backend backend(capacity);for(int i=0;i<20;++i){Host h;backend.run(h,code,"same");if(!i)expected=h.spent;require(h.spent==expected&&expected>0,"warm/bypassed execution changed metering");}}
  fv::fizzy_vm_backend backend;std::atomic<bool> failed=false;std::vector<std::thread> threads;
  try{for(int i=0;i<8;++i)threads.emplace_back([&]{try{for(int n=0;n<100;++n){Host h;backend.run(h,code,"shared");require(h.spent==expected,"concurrent execution changed metering");}}catch(...){failed=true;}});}
  catch(...){for(auto& t:threads)t.join();throw;}for(auto& t:threads)t.join();require(!failed,"concurrent VM execution failed");
}

void context_allocations()
{
  for (bool metered : {false, true})
  {
    reset_tracking(); track = true; fail_at = 0; fired = false;
    auto* context = metered ? fizzy_create_metered_execution_context(251, 1000)
                            : fizzy_create_execution_context(251);
    fail_at = -1; track = false;
    require(fired && !context && !live, "context allocation failure did not return null cleanly");
    context = metered ? fizzy_create_metered_execution_context(251, 1000)
                      : fizzy_create_execution_context(251);
    require(context && *fizzy_get_execution_context_depth(context) == 251,
            "context retry changed depth");
    require(*fizzy_get_execution_context_ticks(context) == (metered ? 1000 : INT64_MAX),
            "context retry changed metering initialization");
    fizzy_free_execution_context(context);
  }
  std::cout << "context_constructors=2 allocation_failure=null retry=passed\n";
}

void execution_allocation_sweep()
{
  const auto code = bytecode(65536);
  size_t boundaries = 0;
  uint64_t expected_ticks = 0;
  reset_tracking(); track = true;
  {
    fv::fizzy_vm_backend backend; Host host;
    backend.run(host, code, ""); expected_ticks = host.spent;
  }
  track = false; boundaries = allocation_calls;
  require(!live && boundaries && boundaries < 1000, "invalid native allocation baseline");
  size_t refused = 0;
  for (size_t ordinal = 0; ordinal <= boundaries; ++ordinal)
  {
    reset_tracking(); track = true; fired = false;
    {
      fv::fizzy_vm_backend backend; Host host;
      fail_at = static_cast<long>(ordinal);
      bool allocation_failure = false;
      try { backend.run(host, code, ""); }
      catch (const std::bad_alloc&) { allocation_failure = true; }
      catch (...) { fail_at = -1; track = false; throw; }
      fail_at = -1;
      if (fired)
      {
        ++refused;
        require(allocation_failure && !host.spent,
                "local allocation failure became a contract reversion or charged execution");
      }
      else require(!allocation_failure && host.spent == expected_ticks, "unfailed execution changed ticks");
      Host retry; backend.run(retry, code, "");
      require(retry.spent == expected_ticks, "allocation failure corrupted retry or metering");
    }
    track = false;
    require(!live, "native execution allocation failure leaked an owned C++ request");
  }
  require(refused == boundaries, "native allocation sweep missed a boundary");
  // Invalid inputs remain deterministic VM reversions, not local capacity errors.
  bool invalid = false;
  try { fv::parse_bytecode("invalid", 7); }
  catch (const fv::module_parse_exception&) { invalid = true; }
  require(invalid, "invalid bytecode changed exception classification");
  std::cout << "native_execution_allocation_boundaries=" << boundaries
            << " local_failure_cleanup_retry=passed\n";
}

void cache_faults()
{
  auto module = fv::parse_bytecode(empty_wasm.data(), empty_wasm.size());
  const std::string a(80, 'a'), b(80, 'b'), c(80, 'c');
  size_t rejections = 0;
  for (long ordinal = 0; ordinal < 5; ++ordinal)
  {
    reset_tracking();
    {
      fv::module_cache cache(2);
      cache.put_module(a, module);
      cache.put_module(b, module);
      track = true; fail_at = ordinal; fired = false; bool rejected = false;
      try { cache.put_module(c, module); }
      catch (const std::bad_alloc&) { rejected = true; }
      fail_at = -1; track = false;
      if (rejected)
      {
        ++rejections;
        require(fired && cache.get_module(a) == module && cache.get_module(b) == module,
                "failed cache insertion evicted an existing owner");
        require(!cache.get_module(c), "failed cache insertion published a partial entry");
        cache.put_module(c, module);
        require(!cache.get_module(a) && cache.get_module(b) && cache.get_module(c),
                "cache retry changed LRU behavior");
      }
      else
        require(!fired && !cache.get_module(a) && cache.get_module(b) && cache.get_module(c),
                "successful cache insertion changed capacity");
    }
    require(!live, "cache insertion retained allocations after destruction");

    // Fail retention after the loader has returned a valid owned module. It
    // must remain executable even when the disposable cache cannot allocate.
    fv::module_cache optional(1);
    optional.put_module(a, module);
    fired = false;
    auto held = optional.get_or_load(c, [&] { fail_at = ordinal; return module; });
    fail_at = -1;
    require(held == module, "optional retention changed the loaded module");
    if (fired)
      require(optional.get_module(a) == module && !optional.get_module(c),
              "optional retention failure changed existing cache membership");
  }
  require(rejections == 4, "cache allocation sweep did not cover all four boundaries");
  std::cout << "cache_allocation_rejections=" << rejections << " optional_retention=passed\n";
}

void loader_lifetimes()
{
  auto module = fv::parse_bytecode(empty_wasm.data(), empty_wasm.size());
  fv::module_cache cache(2);
  cache.put_module("warm", module);
  std::mutex mutex;
  std::condition_variable changed;
  bool loading = false, release = false, warm_done = false;
  std::atomic<unsigned> loads{0};
  std::atomic<bool> failed{false};
  std::vector<std::thread> cold;
  std::thread warm;
  auto cleanup = [&] {
    { std::lock_guard lock(mutex); release = true; }
    changed.notify_all();
    for (auto& thread : cold) thread.join();
    if (warm.joinable()) warm.join();
  };
  try
  {
    for (int i = 0; i < 8; ++i)
      cold.emplace_back([&] {
        try
        {
          auto held = cache.get_or_load("cold", [&] {
            ++loads;
            std::unique_lock lock(mutex);
            loading = true; changed.notify_all();
            changed.wait(lock, [&] { return release; });
            return module;
          });
          if (held != module) failed = true;
        }
        catch (...) { failed = true; }
      });
    {
      std::unique_lock lock(mutex);
      require(changed.wait_for(lock, std::chrono::seconds(3), [&] { return loading; }),
              "cold loader did not start");
    }
    warm = std::thread([&] {
      try
      {
        auto held = cache.get_or_load("warm", [&]() -> fv::module_ptr {
          throw std::runtime_error("warm hit called the parser");
        });
        if (held != module) failed = true;
      }
      catch (...) { failed = true; }
      { std::lock_guard lock(mutex); warm_done = true; }
      changed.notify_all();
    });
    {
      std::unique_lock lock(mutex);
      require(changed.wait_for(lock, std::chrono::seconds(3), [&] { return warm_done; }),
              "warm hit was blocked by a cold parser");
    }
  }
  catch (...) { cleanup(); throw; }
  cleanup();
  require(!failed && loads == 1, "same-ID cold callers did not share one parsed owner");
  cache.put_module("other", module);
  require(module->get() != nullptr, "eviction destroyed a caller-held module");
  bool rejected = false;
  try { cache.get_or_load("bad", []() -> fv::module_ptr { throw std::runtime_error("parse failure"); }); }
  catch (const std::runtime_error&) { rejected = true; }
  require(rejected && cache.get_or_load("bad", [&] { return module; }) == module,
          "loader failure retained a lock or prevented retry");
  fv::module_cache disabled(0);
  unsigned uncached_loads = 0;
  for (int i = 0; i < 2; ++i)
    require(disabled.get_or_load("id", [&] { ++uncached_loads; return module; }) == module,
            "disabled cache lost the caller's owner");
  require(uncached_loads == 2 && !disabled.get_module("id"), "zero cache retained a module");
  std::cout << "cold_callers=8 parse_loads=" << loads << " warm_during_parse=passed\n";
}

std::string callback_bytecode()
{
  auto out = empty_wasm;
  std::string types("\2\x60\6", 3); types.append(6, '\x7f');
  types.append("\1\x7f\x60\0\0", 5); section(out, 1, types);
  std::string imports("\1\3env\x0cinvoke_thunk\0\0", 20); section(out, 2, imports);
  section(out, 3, std::string("\1\1", 2));
  section(out, 5, std::string("\1\0\1", 3));
  section(out, 7, std::string("\1\6_start\0\1", 10));
  std::string body(1, '\0');
  for (uint8_t arg : {1, 0, 8, 8, 8, 16}) { body.push_back('\x41'); body.push_back(char(arg)); }
  body.append("\x10\0\x1a\x0b", 4);
  std::string code(1, '\1'); uleb(code, body.size()); code += body;
  section(out, 10, code); return out;
}

class NestedHost : public Host
{
public:
  fv::fizzy_vm_backend& backend;
  const std::string child = bytecode();
  bool fail = false;
  unsigned calls = 0;
  explicit NestedHost(fv::fizzy_vm_backend& backend) : backend(backend) {}
  int32_t invoke_thunk(uint32_t id, char* ret, uint32_t ret_size,
                       const char*, uint32_t, uint32_t* written) override
  {
    require(id == 1 && ret_size == 8 && ret[0] == 0, "mutable VM instance was reused");
    ++calls; ret[0] = 'x'; *written = 1;
    if (fail) throw std::runtime_error("fixture host failure");
    backend.run(*this, child, "nested-child");
    return 0;
  }
};

void nested_and_failure_cleanup()
{
  auto code = callback_bytecode(); reset_tracking(); track = true;
  uint64_t expected = 0;
  for (uint64_t capacity : {uint64_t(0), uint64_t(1), uint64_t(262144)})
  {
    fv::fizzy_vm_backend backend(capacity);
    for (int i = 0; i < 20; ++i)
    {
      NestedHost host(backend); backend.run(host, code, "parent");
      if (!expected) expected = host.spent;
      require(host.calls == 1 && expected > 0 && host.spent == expected,
              "nested call changed metering or instance isolation");
    }
    NestedHost host(backend); host.fail = true; bool failed = false;
    try { backend.run(host, code, "parent"); }
    catch (const std::runtime_error& error) { failed = std::string(error.what()) == "fixture host failure"; }
    require(failed && host.calls == 1 && host.spent > 0, "host exception was lost or unmetered");
    Host no_start; failed = false;
    try { backend.run(no_start, empty_wasm, "no-start"); }
    catch (...) { failed = true; }
    require(failed, "module without _start was accepted");
    // The owned module outlives each borrowing instance, including exceptions.
  }
  track = false; require(!live, "nested/failure path leaked an ordinary C++ allocation");
  std::cout << "nested_calls=60 cache_policies=3 instance_isolation=passed exception_cleanup=passed\n";
}

void measure_burst(const std::string& strategy)
{
  require(strategy == "before" || strategy == "after" || strategy == "diverse",
          "unknown burst strategy");
  const auto code = bytecode(2 * 1024 * 1024);
  fv::module_cache cache(32);
  std::vector<fv::module_ptr> owners(8);
  std::vector<std::thread> workers;
  std::mutex mutex;
  std::condition_variable changed;
  bool release = false;
  unsigned ready = 0, loads = 0;
  std::atomic<unsigned> active{0}, peak{0};
  std::atomic<bool> failed{false};
  auto join = [&] {
    { std::lock_guard lock(mutex); release = true; }
    changed.notify_all();
    for (auto& worker : workers) worker.join();
  };
  try
  {
    for (unsigned i = 0; i < owners.size(); ++i)
      workers.emplace_back([&, i] {
        try
        {
          const auto id = strategy == "diverse" ? "contract-" + std::to_string(i) : "same";
          { std::lock_guard lock(mutex); ++ready; }
          changed.notify_all();
          auto parse = [&] {
            {
              std::unique_lock lock(mutex); ++loads; changed.notify_all();
              changed.wait(lock, [&] { return release; });
            }
            const auto count = ++active;
            auto high = peak.load();
            while (count > high && !peak.compare_exchange_weak(high, count)) {}
            fv::module_ptr parsed;
            try { parsed = fv::parse_bytecode(code.data(), code.size()); }
            catch (...) { --active; throw; }
            --active;
            return parsed;
          };
          if (strategy == "before")
          {
            auto parsed = cache.get_module(id);
            if (!parsed) { parsed = parse(); cache.put_module(id, parsed); }
            owners[i] = std::move(parsed);
          }
          else owners[i] = cache.get_or_load(id, parse);
        }
        catch (...) { failed = true; changed.notify_all(); }
      });
    {
      std::unique_lock lock(mutex);
      require(changed.wait_for(lock, std::chrono::seconds(3), [&] {
        return failed || (ready == owners.size() && loads >= (strategy == "before" ? 8u : 1u));
      }), "burst callers did not reach the controlled cold boundary");
      require(!failed, "burst preparation failed");
    }
  }
  catch (...) { join(); throw; }
  const auto start = std::chrono::steady_clock::now();
  join();
  require(!failed && !active, "burst parser failed or stayed active");
  const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now() - start).count();
  unsigned unique = 0;
  for (unsigned i = 0; i < owners.size(); ++i)
  {
    require(bool(owners[i]), "burst owner missing");
    bool duplicate = false;
    for (unsigned j = 0; j < i; ++j) duplicate |= owners[j] == owners[i];
    if (!duplicate) ++unique;
  }
  require(loads == (strategy == "after" ? 1u : 8u) && unique == loads,
          "cold parse count does not match distinct caller-held owners");
  rusage usage{}; require(!getrusage(RUSAGE_SELF, &usage), "burst RSS unavailable");
#if defined(__APPLE__)
  const auto rss = uint64_t(usage.ru_maxrss);
#else
  const auto rss = uint64_t(usage.ru_maxrss) * 1024;
#endif
  std::cout << "{\"format\":\"lr2-v1a-controlled-burst\",\"strategy\":\"" << strategy
            << "\",\"callers\":8,\"data_bytes\":2097152,\"parse_loads\":" << loads
            << ",\"peak_active_parsers\":" << peak << ",\"unique_caller_modules\":" << unique
            << ",\"elapsed_after_gate_us\":" << elapsed << ",\"process_peak_rss_bytes\":" << rss << "}\n";
}
}
int main(int argc,char** argv){try{
  if(argc==3&&std::string(argv[1])=="--parse-fault"){parse_fault(std::stoul(argv[2]));return 0;}
  if((argc==3||argc==4)&&std::string(argv[1])=="--measure"){measure(argv[2],argc==4?std::stoull(argv[3]):0);return 0;}
  if(argc==3&&std::string(argv[1])=="--burst"){measure_burst(argv[2]);return 0;}
  if(argc==2&&std::string(argv[1])=="--context-fault"){
    std::set_terminate([]{std::_Exit(77);});context_allocations();return 0;
  }
  context_allocations();execution_allocation_sweep();
  for(size_t i=0;i<3;++i)parse_fault(i);cache_faults();loader_lifetimes();test_execution();nested_and_failure_cleanup();
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
