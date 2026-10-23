#include "core/owned_allocation.hpp"
#include <koinos/vm_manager/fizzy/fizzy_vm_backend.hpp>
#include <koinos/chain/controller.hpp>
#include <fizzy/module.hpp>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

// Scoped ordinary new faults/counts only; not allocator/RSS or machine OOM.
thread_local long fail_at = -1;
thread_local bool counting = false;
thread_local unsigned calls = 0;
void* operator new(std::size_t bytes) {
  if(counting) ++calls;
  if(fail_at>=0 && fail_at--==0) { fail_at=-1; throw std::bad_alloc(); }
  if(auto p=koinos::node::owned_allocation::allocate(bytes?bytes:1)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { koinos::node::owned_allocation::deallocate(p); }
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}

namespace fv=koinos::vm_manager::fizzy;
namespace koinos::vm_manager::fizzy { module_ptr parse_bytecode(const char*,std::size_t); }
void require(bool ok,const char* text) {if(!ok) throw std::runtime_error(text);}
auto module() {const std::string bytes("\0asm\1\0\0\0",8);return fv::parse_bytecode(bytes.data(),bytes.size());}

void layout() {
  fv::capacity_sum sum; sum.add(UINT64_MAX); require(sum.value==UINT64_MAX,"exact charge maximum changed");
  sum.add(1); require(!sum.value,"sum overflow accepted"); sum.add(0); require(!sum.value,"overflow state lost");
  fv::capacity_sum multiply; multiply.add(UINT64_MAX,2); require(!multiply.value,"product overflow accepted");
  require(fv::parsed_module_capacity_bytes(nullptr)==0,"null estimate changed");
  ::fizzy::Module parsed;
  auto estimate=[&] {auto n=fv::parsed_module_capacity_bytes(reinterpret_cast<const FizzyModule*>(&parsed));
    require(bool(n),"valid module footprint unavailable");return *n;};
  require(estimate()==sizeof(parsed),"empty layout charge differs");
  auto grow=[&](auto& values) {auto before=estimate();auto old=values.capacity();values.reserve(128);
    require(estimate()-before==(values.capacity()-old)*sizeof(typename std::decay_t<decltype(values)>::value_type),"vector capacity omitted");};
  grow(parsed.typesec);grow(parsed.importsec);grow(parsed.funcsec);grow(parsed.tablesec);
  grow(parsed.memorysec);grow(parsed.globalsec);grow(parsed.exportsec);grow(parsed.elementsec);
  grow(parsed.codesec);grow(parsed.datasec);grow(parsed.imported_function_types);
  grow(parsed.imported_table_types);grow(parsed.imported_memory_types);grow(parsed.imported_global_types);
  parsed.typesec.resize(1); parsed.imported_function_types.resize(1);
  grow(parsed.typesec[0].inputs);grow(parsed.typesec[0].outputs);
  grow(parsed.imported_function_types[0].inputs);grow(parsed.imported_function_types[0].outputs);
  parsed.importsec.resize(1); parsed.exportsec.resize(1); parsed.codesec.resize(1);
  parsed.elementsec.resize(1); parsed.datasec.resize(1);
  auto named=[&](auto& value) {auto before=estimate();auto old=value.capacity();value.reserve(1024);
    require(estimate()-before==value.capacity()-old,"nested string capacity omitted");};
  named(parsed.importsec[0].module);named(parsed.importsec[0].name);named(parsed.exportsec[0].name);
  grow(parsed.codesec[0].instructions);grow(parsed.elementsec[0].init);named(parsed.datasec[0].init);
}

void policy() {
  auto first=module(),second=module();
  fv::module_cache unlimited(32); unlimited.put_module("a",first);
  const auto charge=unlimited.memory().capacity_bytes;
  require(charge>first->capacity_bytes().value() && unlimited.memory().entries==1,"indexed charge incomplete");
  fv::module_cache exact(32,charge);exact.put_module("a",first);
  require(exact.get_module("a")==first && exact.memory().capacity_bytes==charge,"exact capacity refused");
  auto held=exact.get_module("a");exact.put_module("b",second);
  require(!exact.get_module("a") && exact.get_module("b")==second && held->get(),"eviction invalidates held owner");
  require(exact.memory().entries==1 && exact.memory().capacity_bytes==charge,"byte eviction accounting differs");
  exact.put_module("b",first);require(exact.get_module("b")==first && exact.memory().capacity_bytes==charge,"same-ID replacement charge differs");
  const std::string large_id(1024,'L');
  exact.put_module(large_id,second);
  require(!exact.get_module(large_id) && exact.get_module("b")==first && exact.memory().retention_bypasses==1,"oversized new entry flushed useful owner");
  exact.clear();require(!exact.memory().entries && !exact.memory().capacity_bytes && held->get(),"clear released caller owner or kept charge");
  fv::module_cache below(32,charge-1);below.put_module("a",first);
  require(!below.get_module("a") && below.memory().retention_bypasses==1,"below-exact entry retained");
  fv::module_cache disabled(0);disabled.put_module("a",first);require(!disabled.memory().entries,"zero-entry cache retained module");
  unlimited.put_module("b",first);require(unlimited.memory().entries==2,"unlimited policy changed");
  fv::module_cache maximum(32,UINT64_MAX);maximum.put_module("a",first);
  require(maximum.memory().capacity_bytes==charge,"maximum byte limit wrapped");
  calls=0;counting=true;for(int i=0;i<10000;++i)require(maximum.get_module("a")==first,"warm owner missing");counting=false;
  require(!calls,"warm hit allocates");
  fv::module_cache single(32,1);unsigned loads=0;
  for(int i=0;i<3;++i) {auto owner=single.get_or_load("a",[&]{++loads;return first;});require(owner==first,"bypass lost owned module");}
  require(loads==3 && single.memory().retention_bypasses==3,"bypass reused an unretained module");
  // A larger same-ID replacement must invalidate the stale small cached owner.
  auto large=std::make_unique<::fizzy::Module>();large->datasec.resize(1);large->datasec[0].init.resize(4096);
  auto pointer=reinterpret_cast<const FizzyModule*>(large.get());
  auto replacement=std::make_shared<const fv::module_guard>(pointer);large.release();
  exact.put_module("b",first);exact.put_module("b",replacement);
  require(!exact.get_module("b") && !exact.memory().capacity_bytes && replacement->get(),"oversized replacement left stale cache state");
  fv::module_cache large_probe(32);large_probe.put_module("b",replacement);
  const auto large_charge=large_probe.memory().capacity_bytes;
  fv::module_cache resized(32,large_charge);resized.put_module("a",first);resized.put_module("b",first);
  resized.put_module("b",replacement);
  require(!resized.get_module("a") && resized.get_module("b")==replacement
          && resized.memory().capacity_bytes==large_charge && resized.memory().entries==1,
          "larger fitting replacement failed to evict other owners or account capacity");
  resized.put_module("b",first);
  require(resized.memory().capacity_bytes==charge && resized.memory().entries==1,
          "smaller replacement did not release attributed capacity");
}

void failures() {
  auto owner=module();const std::string a(128,'a'),b(128,'b'),c(128,'c');unsigned faults=0;
  for(unsigned ordinal=0;ordinal<10;++ordinal) {
    fv::module_cache cache(2);cache.put_module(a,owner);cache.put_module(b,owner);
    const auto before=cache.memory();bool failed=false;fail_at=ordinal;
    try{cache.put_module(c,owner);}catch(const std::bad_alloc&){failed=true;}fail_at=-1;
    if(failed) {
      ++faults;require(cache.memory().capacity_bytes==before.capacity_bytes && cache.memory().entries==before.entries,"failed preparation changed charge");
      // Probe recency with successful C insertion before any A/B lookup.
      cache.put_module(c,owner);require(!cache.get_module(a) && cache.get_module(b)==owner && cache.get_module(c)==owner,"failed preparation changed recency");
    } else require(cache.memory().entries==2,"unfailed terminal insertion differs");
  }
  require(faults>=4,"expected staged key/node faults not exercised");
  std::cout<<"staged_insertion_faults="<<faults<<" warm_allocations=0\n";
}

void concurrent() {
  auto owner=module();fv::module_cache probe(2);probe.put_module("a",owner);
  const auto limit=probe.memory().capacity_bytes*2;
  fv::module_cache cache(32,limit);std::atomic<bool> failed=false;std::vector<std::thread> threads;
  for(int i=0;i<8;++i) threads.emplace_back([&,i]{try{
    for(int j=0;j<1000;++j) {auto id=std::to_string((i+j)%8);cache.put_module(id,owner);
      auto held=cache.get_module(id);if(held)require(held->get()!=nullptr,"concurrent held module invalid");
      require(cache.memory().capacity_bytes<=limit,"concurrent byte capacity exceeded");if(j%100==0)cache.clear();}
  }catch(...){failed=true;}});
  for(auto& thread:threads)thread.join();require(!failed,"concurrent retention policy failed");
}
int main() {try {
  layout();policy();failures();concurrent();
  koinos::chain::controller native(0,0,{},777);
  require(native.vm_cache_memory().available && native.vm_cache_memory().max_capacity_bytes==777,"controller budget not wired");
  auto backend=koinos::vm_manager::get_vm_backend("fizzy",999);
  require(backend->cache_memory().max_capacity_bytes==999,"factory budget not wired");
  std::cout<<"parsed capacity, limits, lifetime, allocation and concurrency passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;} }
