#include "p2p/peer_snapshot_budget.hpp"
#include <cassert>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
#include <type_traits>

namespace { thread_local long fail_after=-1; }
void* operator new(std::size_t n){if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace koinos::node::p2p;
using namespace koinos::node::p2p::detail;
const PeerID input{std::string(80,'p'),std::string(120,'a')};
PeerSnapshot make(PeerSnapshotBudget& b,size_t count=1){return b.make(count,count*PeerSnapshotBudget::strings(input),[&](auto& out){for(size_t i=0;i<count;++i)out.push_back(input);});}
template<typename F>void capacity(F&& fn){bool caught=false;try{fn();}catch(const LocalTransportUnavailable& e){caught=e.reason==LocalTransportUnavailable::Reason::peer_snapshot_capacity;}assert(caught);}
void held_owners_and_limits()
{
 static_assert(!std::is_copy_constructible_v<PeerSnapshot> && std::is_nothrow_move_constructible_v<PeerSnapshot> && std::is_nothrow_move_assignable_v<PeerSnapshot>);
 PeerSnapshotBudget b(2,4,4096);auto a=make(b,2),c=make(b,1);assert(b.stats().jobs==2&&b.stats().entries==3);capacity([&]{make(b);});
 auto moved=std::move(a);assert(b.stats().jobs==2&&moved.size()==2&&moved[0].address==input.address);moved=std::move(c);assert(b.stats().jobs==1&&b.stats().entries==1);auto* self=&moved;moved=std::move(*self);assert(b.stats().jobs==1&&moved.size()==1);moved=PeerSnapshot{};assert(b.stats().jobs==0&&b.stats().capacity_bytes==0);
 auto all=make(b,4);capacity([&]{make(b);});all=PeerSnapshot{};
 PeerSnapshotBudget probe;auto p=make(probe);const auto charge=probe.stats().capacity_bytes;p=PeerSnapshot{};PeerSnapshotBudget exact(2,8,charge);auto one=make(exact);assert(exact.stats().capacity_bytes==charge);capacity([&]{make(exact);});one=PeerSnapshot{};
 PeerSnapshotBudget small(2,8,charge-1);capacity([&]{make(small);});assert(small.stats().jobs==0&&small.stats().entries==0);
 fail_after=0;capacity([&]{b.make(100000,1,[](auto&){});});assert(fail_after==0);fail_after=-1;
 auto budget=std::make_unique<PeerSnapshotBudget>();auto retained=make(*budget);budget.reset();assert(retained[0].id==input.id);retained=PeerSnapshot{};
 for(auto limits:std::vector<size_t>{0,1,2}){bool bad=false;try{PeerSnapshotBudget invalid(limits==0?0:1,limits==1?0:4,limits==2?0:1024);}catch(const std::invalid_argument&){bad=true;}assert(bad);}
}
void rollback_and_actual_capacity()
{
 PeerSnapshotBudget b;unsigned failures=0;
 for(long site=0;site<10;++site)
 {
  fail_after=site;PeerSnapshot owner;try{owner=make(b,2);}catch(const std::bad_alloc&){++failures;}fail_after=-1;owner=PeerSnapshot{};
  assert(b.stats().jobs==0&&b.stats().entries==0&&b.stats().capacity_bytes==0);auto retry=make(b);assert(retry[0].id==input.id);
 }
 assert(failures>=6);
 PeerSnapshotBudget narrow(2,4,4096);capacity([&]{narrow.make(1,PeerSnapshotBudget::strings(input),[](auto& out){out.reserve(20);out.push_back(input);});});assert(narrow.stats().jobs==0);
 PeerSnapshotBudget bytes(2,8,1024);capacity([&]{bytes.make(1,PeerSnapshotBudget::strings(input),[](auto& out){out.push_back(input);out.back().address.reserve(2048);});});assert(bytes.stats().jobs==0&&bytes.stats().capacity_bytes==0);
 bool mismatch=false;try{make(b,0);b.make(1,20,[](auto&){});}catch(const std::logic_error&){mismatch=true;}assert(mismatch&&b.stats().jobs==0);
}
void concurrent_owners()
{
 PeerSnapshotBudget b(4,8,4096);std::atomic<unsigned> completed{0};std::vector<std::thread> workers;
 for(unsigned i=0;i<8;++i)workers.emplace_back([&]{for(unsigned j=0;j<128;++j){try{auto p=make(b);assert(p[0].id==input.id);std::this_thread::yield();++completed;}catch(const LocalTransportUnavailable&){} }});
 for(auto& w:workers)w.join();const auto s=b.stats();assert(completed>0&&s.jobs==0&&s.entries==0&&s.capacity_bytes==0&&s.peak_jobs<=4&&s.peak_entries<=8&&s.peak_capacity_bytes<=4096);
}
int main(){held_owners_and_limits();rollback_and_actual_capacity();concurrent_owners();std::cout<<"Peer snapshot lifetime, actual/provisional capacity, fault rollback and concurrent admission passed\n";}
