#include "core/owned_allocation.hpp"
#include "p2p/peer_dial_budget.hpp"
#include <cassert>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <iostream>
#include <new>
#include <thread>
#include <vector>

namespace { thread_local long fail_after = -1; }
void* operator new(std::size_t n)
{
  if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;
  if(auto p=koinos::node::owned_allocation::allocate(n?n:1))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{koinos::node::owned_allocation::deallocate(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}

namespace {
using namespace koinos::node::p2p;
using namespace koinos::node::p2p::detail;
using namespace std::chrono_literals;
PeerID input(size_t n){return {std::string(80,'p')+std::to_string(n),std::string(120,'a')+std::to_string(n)};}
template<typename F>void capacity(F&& fn)
{
  bool caught=false;try{fn();}catch(const LocalTransportUnavailable&e){caught=e.reason==LocalTransportUnavailable::Reason::dial_capacity;}assert(caught);
}
void counts_and_bytes()
{
  auto active=std::make_shared<std::atomic<bool>>(true);auto budget=std::make_shared<DialBudget>(2,1024);
  DialRegistry registry(budget,active);auto a=registry.admit(input(1));auto b=registry.admit(input(2));
  assert(a&&b&&registry.eligible(a));assert(!registry.admit(input(1)));assert(budget->stats().jobs==2);
  const auto charge=a->peer.id.capacity()+a->peer.address.capacity()+b->peer.id.capacity()+b->peer.address.capacity();
  assert(budget->stats().peer_capacity_bytes==charge);capacity([&]{registry.admit(input(3));});
  registry.retire(a);assert(!registry.eligible(a));assert(budget->stats().jobs==2);
  capacity([&]{registry.admit(input(3));});a.reset();assert(budget->stats().jobs==1);
  auto c=registry.admit(input(3));registry.retire(b);b.reset();registry.retire(c);c.reset();
  assert(budget->stats().jobs==0&&budget->stats().peer_capacity_bytes==0&&budget->stats().peak_jobs==2);
  const auto peer=input(4);auto probe=std::make_shared<DialBudget>(1,4096);auto owner=probe->reserve(peer);
  const auto exact=owner->peer.id.capacity()+owner->peer.address.capacity();owner.reset();
  auto narrow=std::make_shared<DialBudget>(2,exact);DialRegistry bytes(narrow,active);auto d=bytes.admit(peer);
  assert(narrow->stats().peer_capacity_bytes==exact);capacity([&]{bytes.admit(input(5));});
  bytes.retire(d);d.reset();assert(narrow->stats().peer_capacity_bytes==0);
  const auto initial=std::max(peer.id.size(),std::string().capacity())+std::max(peer.address.size(),std::string().capacity());
  if(exact>initial)
  {
    auto growth=std::make_shared<DialBudget>(1,initial);DialRegistry checked(growth,active);
    capacity([&]{checked.admit(peer);});assert(growth->stats().jobs==0&&growth->stats().peer_capacity_bytes==0);
  }
  // Oversize borrowed input must be refused before an owned key/control allocation.
  PeerID huge{std::string(4096,'x'),"y"};fail_after=0;capacity([&]{bytes.admit(huge);});assert(fail_after==0);fail_after=-1;
}
void failures_and_publication()
{
  auto active=std::make_shared<std::atomic<bool>>(true);auto budget=std::make_shared<DialBudget>(2,4096);DialRegistry registry(budget,active);const auto peer=input(6);
  // Shared owner, both strings, registry node: each actual allocation must roll back.
  size_t failures=0;
  for(long site=0;site<10;++site)
  {
    fail_after=site;std::shared_ptr<DialBudget::Lease> lease;
    try{lease=registry.admit(peer);}catch(const std::bad_alloc&){++failures;}
    fail_after=-1;if(lease)registry.retire(lease);lease.reset();
    assert(budget->stats().jobs==0&&budget->stats().peer_capacity_bytes==0);
    auto retry=registry.admit(peer);assert(retry&&registry.eligible(retry));registry.retire(retry);retry.reset();
  }
  assert(failures>=4);
  auto lease=registry.admit(peer);bool threw=false;
  try{registry.publish(lease,[]{throw std::bad_alloc();});}catch(const std::bad_alloc&){threw=true;}
  assert(threw&&!registry.eligible(lease)&&budget->stats().jobs==1);lease.reset();assert(budget->stats().jobs==0);
}
void generations_and_late_owners()
{
  auto budget=std::make_shared<DialBudget>(2,4096);auto active=std::make_shared<std::atomic<bool>>(true);auto old=std::make_shared<DialRegistry>(budget,active);
  auto lease=old->admit(input(7));auto late=lease;old->cancel_peer(lease->peer.id);
  auto fresh=old->admit(input(7));assert(fresh&&old->eligible(fresh));old->retire(lease);assert(old->eligible(fresh));
  old->retire(fresh);fresh.reset();old->stop_accepting();active->store(false);lease.reset();old.reset();
  assert(budget->stats().jobs==1);auto next_active=std::make_shared<std::atomic<bool>>(true);DialRegistry next(budget,next_active);
  auto now=next.admit(input(8));capacity([&]{next.admit(input(9));});late.reset();auto after=next.admit(input(9));assert(now&&after);
  next.stop_accepting();assert(!next.eligible(now));bool published=false,caught=false;
  try{next.publish(now,[&]{published=true;});}catch(const LocalTransportUnavailable&e){caught=e.reason==LocalTransportUnavailable::Reason::lifecycle;}
  assert(caught&&!published);caught=false;
  try{next.publish_control([&]{published=true;});}catch(const LocalTransportUnavailable&){caught=true;}
  assert(caught&&!published);now.reset();after.reset();assert(budget->stats().jobs==0);
}
bool foreign_registry_isolation()
{
  auto budget=std::make_shared<DialBudget>(2,4096);auto active=std::make_shared<std::atomic<bool>>(true);
  DialRegistry left(budget,active),right(budget,active);auto a=left.admit(input(123));auto b=right.admit(input(123));
  left.retire(b);bool unchanged=left.eligible(a)&&right.eligible(b);
  left.retire(a);right.retire(b);a.reset();b.reset();assert(budget->stats().jobs==0);
  if(!unchanged)std::cerr<<"A foreign registry changed another reservation with an equal ticket\n";
  return unchanged;
}
void stop_linearizes_publication()
{
  auto budget=std::make_shared<DialBudget>(2,4096);auto active=std::make_shared<std::atomic<bool>>(true);DialRegistry registry(budget,active);auto lease=registry.admit(input(10));
  std::promise<void> entered,release;auto entered_future=entered.get_future();auto release_future=release.get_future();std::atomic<bool> stopped{false};
  auto publishing=std::async(std::launch::async,[&]{registry.publish(lease,[&]{entered.set_value();release_future.wait();});});
  assert(entered_future.wait_for(2s)==std::future_status::ready);
  std::promise<void> stop_entered;auto entering=stop_entered.get_future();
  auto stopping=std::async(std::launch::async,[&]{stop_entered.set_value();registry.stop_accepting();stopped=true;});
  assert(entering.wait_for(2s)==std::future_status::ready);
  assert(stopping.wait_for(30ms)==std::future_status::timeout&&!stopped.load());release.set_value();publishing.get();stopping.get();assert(stopped.load()&&!registry.eligible(lease));
  bool called=false;try{registry.publish(lease,[&]{called=true;});}catch(const LocalTransportUnavailable&){}assert(!called);lease.reset();assert(budget->stats().jobs==0);
}
void concurrent_capacity()
{
  auto budget=std::make_shared<DialBudget>(8,8192);auto active=std::make_shared<std::atomic<bool>>(true);DialRegistry registry(budget,active);std::atomic<size_t> admitted{0},refused{0};std::vector<std::thread> workers;
  for(size_t i=0;i<8;++i)workers.emplace_back([&,i]{
    for(size_t j=0;j<128;++j)
    {
      try{auto lease=registry.admit(input(i*128+j));if(lease){++admitted;assert(registry.eligible(lease));registry.retire(lease);}}
      catch(const LocalTransportUnavailable&e){assert(e.reason==LocalTransportUnavailable::Reason::dial_capacity);++refused;}
      auto stats=budget->stats();assert(stats.jobs<=8&&stats.peer_capacity_bytes<=8192);
    }
  });
  for(auto&worker:workers)worker.join();assert(admitted+refused==1024);assert(budget->stats().jobs==0&&budget->stats().peer_capacity_bytes==0);
}
}
int main(int argc,char**argv)
{
  if(argc>1&&std::string(argv[1])=="foreign-registry")return foreign_registry_isolation()?0:1;
  assert(foreign_registry_isolation());
  counts_and_bytes();failures_and_publication();generations_and_late_owners();stop_linearizes_publication();concurrent_capacity();
  for(auto limits:std::vector<std::pair<size_t,size_t>>{{0,1},{1,0}})
  {bool caught=false;try{DialBudget invalid(limits.first,limits.second);}catch(const std::invalid_argument&){caught=true;}assert(caught);}
  std::cout<<"Dial reservation, allocation rollback, publication/stop ordering and late-owner generations passed\n";
}
