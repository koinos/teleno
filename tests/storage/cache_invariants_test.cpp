#include <koinos/state_db/backends/rocksdb/object_cache.hpp>
#include <koinos/vm_manager/fizzy/module_cache.hpp>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>
#if !defined(_WIN32)
#include <sys/resource.h>
#endif

static std::atomic<size_t> allocations{0};
static std::atomic<bool> counting{false};
void* operator new(std::size_t size) {
  if (counting) ++allocations;
  if (auto p = std::malloc(size ? size : 1)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
using Cache = koinos::state_db::backends::rocksdb::object_cache;
using Modules = koinos::vm_manager::fizzy::module_cache;
void check(bool value) { if (!value) throw std::runtime_error("cache invariant failed"); }
auto value(size_t size) { return std::make_shared<const std::string>(size, 'x'); }
auto module() {
  const uint8_t bytes[] = {0,97,115,109,1,0,0,0};
  FizzyError error{};
  auto parsed = fizzy_parse(bytes, sizeof(bytes), &error);
  check(parsed != nullptr);
  return std::make_shared<const koinos::vm_manager::fizzy::module_guard>(parsed);
}
void measure() {
  Cache cache(8*1024*1024);
  std::vector<std::string> keys;
  size_t payload=0;
  for (int i=0; i<50000; ++i) {
    keys.push_back(std::string(64,'k')+std::to_string(i));
    cache.put(keys.back(),value(64));
    payload+=keys.back().size()+64;
  }
  for (const auto& key:keys) check(cache.get(key).first);
  allocations=0; counting=true;
  auto start=std::chrono::steady_clock::now();
  for (int i=0;i<100000;++i) check(cache.get(keys[i%keys.size()]).first);
  counting=false;
  auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
  std::string peak="null";
#if !defined(_WIN32)
  rusage usage{}; getrusage(RUSAGE_SELF,&usage); peak=std::to_string(usage.ru_maxrss);
#endif
  std::cout << "{\"entries\":50000,\"hits\":100000,\"hit_allocations\":" << allocations
            << ",\"elapsed_us\":" << elapsed << ",\"peak_rss_native\":" << peak
            << ",\"logical_payload_bytes\":" << payload << "}" << std::endl;
}
int main(int argc,char** argv) {
  if(argc>1 && std::string(argv[1])=="--measure") { measure(); return 0; }
  Cache c(64); c.put("a",value(30)); c.put("b",value(30));
  auto held=c.get("a").second;
  c.put("c",value(30)); check(!c.get("b").first); check(*held==std::string(30,'x'));
  c.put("a",value(40)); check(c.get("a").first); check(!c.get("c").first);
  c.clear(); c.put("a",value(60)); check(c.get("a").first);
  auto oversized = c.put("large",value(100)); check(!c.get("large").first); check(c.get("a").first);
  check(oversized && *oversized==std::string(100,'x'));
  check(c.entry_count()==1 && c.payload_bytes()==61 && c.capacity_bytes()==64);
  c.put("a",value(1)); check(!c.get("large").first); check(c.get("a").first);
  Cache zero(0); auto uncached = zero.put("large",value(100)); check(!zero.get("large").first && *uncached==std::string(100,'x'));
  zero.put("missing",{}); check(!zero.get("missing").first && zero.entry_count()==0 && zero.payload_bytes()==0);
  zero.clear(); check(!zero.get("missing").first);
  Cache huge(std::numeric_limits<size_t>::max()); huge.put("ok",value(100)); check(huge.get("ok").first);
  std::string key(128,'k'); Cache warm(1024); warm.put(key,value(1));
  counting=true; allocations=0; for(int i=0;i<10000;++i) check(warm.get(key).first); counting=false;
  check(allocations==0);
  std::vector<std::thread> cache_workers;
  for(int i=0;i<4;++i) cache_workers.emplace_back([&,i] {
    for(int j=0;j<10000;++j) {
      std::shared_ptr<const std::string> held;
      { std::lock_guard lock(c.get_mutex()); auto key=std::to_string(i)+":"+std::to_string(j%7);
        c.put(key,value(30)); held=c.get(key).second; }
      check(held && *held==std::string(30,'x'));
    }
  });
  for(auto& worker:cache_workers) worker.join();
  auto m=module(); Modules modules(2);
  modules.put_module("A",m); modules.put_module("A",m); modules.put_module("B",m);
  check(modules.get_module("A") && modules.get_module("B"));
  modules.get_module("A"); modules.put_module("C",m); check(!modules.get_module("B"));
  Modules empty(0); empty.put_module("A",m); check(!empty.get_module("A"));
  std::string module_key(128,'m'); modules.put_module(module_key,m);
  counting=true; allocations=0;
  for(int i=0;i<10000;++i) check(bool(modules.get_module(module_key)));
  counting=false; check(allocations==0);
  std::vector<std::thread> workers;
  for(int i=0;i<4;++i) workers.emplace_back([&] {
    for(int j=0;j<10000;++j) { modules.put_module("same",m); auto held=modules.get_module("same"); if(held) check(held->get()!=nullptr); }
  });
  for(auto& worker:workers) worker.join();
  check(bool(modules.get_module("same")));
  std::cout << "cache invariants passed\n";
}
