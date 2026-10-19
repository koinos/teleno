#include "p2p/sdk_address_repository.hpp"
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>

namespace {thread_local long fail_after=-1;}
void* operator new(std::size_t n){if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
namespace lp=libp2p;
using namespace koinos::node::p2p::detail;
using Id=lp::peer::PeerId;
using Address=lp::Multiaddress;
using Ms=std::chrono::milliseconds;
class Dns final:public lp::network::DnsaddrResolver
{
public:
 std::vector<Address> result;
 AddressesCallback pending;
 bool defer=false;
 unsigned loads=0;
 void load(Address,AddressesCallback cb)override{++loads;if(defer)pending=std::move(cb);else cb(result);}
};
Id id(unsigned n){std::array<uint8_t,34>b{};b[0]=0x12;b[1]=0x20;b[32]=n>>8;b[33]=n&255;return Id::fromBytes(b).value();}
Address addr(unsigned n){return Address::create("/ip4/127.0.0.1/tcp/"+std::to_string(10000+n)).value();}
bool has(SdkAddressRepository& r,const Id& p,const Address& a){auto rows=r.getAddresses(p);return rows&&std::find(rows.value().begin(),rows.value().end(),a)!=rows.value().end();}
bool add(SdkAddressRepository& r,const Id& p,const Address& a,Ms ttl=lp::peer::ttl::kPermanent){return r.addAddresses(p,std::span(&a,1),ttl).value();}
void limits_and_owners()
{
 auto dns=std::make_shared<Dns>();auto budget=std::make_shared<SdkAddressBudget>(SdkAddressLimits{2,4,4,4096,2});auto old=std::make_shared<SdkAddressRepository>(dns,budget);const auto a=addr(1),b=addr(2);const auto p=id(1),q=id(2),r=id(3);
 old->protect([&](const Id& x){return x==p;});assert(add(*old,p,a)&&add(*old,q,a));assert(add(*old,r,b));assert(has(*old,p,a)&&has(*old,r,b)&&!old->getAddresses(q));auto stats=budget->stats();assert(stats.peers==2&&stats.addresses==2&&stats.slots==2&&stats.evicted_peers==1);
 auto next=std::make_shared<SdkAddressRepository>(dns,budget);fail_after=0;assert(!add(*next,q,a)&&fail_after==0);fail_after=-1;old->retire();assert(!add(*old,q,a));assert(budget->stats().peers==2);old.reset();assert(budget->stats().peers==0&&budget->stats().repositories==1);assert(add(*next,q,a));next.reset();stats=budget->stats();assert(!stats.peers&&!stats.addresses&&!stats.slots&&!stats.encoded_bytes&&!stats.repositories);
 const size_t exact=p.toVector().size()+a.getBytesAddress().size()+a.getStringAddress().size();auto tiny=std::make_shared<SdkAddressBudget>(SdkAddressLimits{8,8,8,exact,2});SdkAddressRepository bounded(dns,tiny);bounded.protect([](const Id&){return true;});assert(add(bounded,p,a));assert(tiny->stats().encoded_bytes==exact);fail_after=0;assert(!add(bounded,q,a)&&fail_after==0);fail_after=-1;
 assert(!add(bounded,p,b));assert(has(bounded,p,a)&&!has(bounded,p,b));
}
void ttl_order_clear()
{
 auto budget=std::make_shared<SdkAddressBudget>();SdkAddressRepository r(std::make_shared<Dns>(),budget);const auto p=id(1);const auto a=addr(1),b=addr(2);assert(add(r,p,a));assert(add(r,p,b,Ms(0)));r.dialFailed(p,a);auto view=r.getAddresses(p).value();assert(view[0]==b&&view[1]==a);assert(!add(r,p,a,Ms(0)));r.collectGarbage();assert(has(r,p,a)&&!has(r,p,b));assert(!r.upsertAddresses(p,std::span(&a,1),Ms(0)).value());r.collectGarbage();assert(r.getPeers().empty());
 assert(add(r,p,a,Ms::max()-Ms(1)));r.collectGarbage();assert(has(r,p,a));r.clear(p);assert(r.getAddresses(p).value().empty()&&r.getPeers().contains(p));assert(budget->stats().slots==0&&budget->stats().encoded_bytes==p.toVector().size());r.collectGarbage();assert(!budget->stats().peers);
 assert(add(r,p,a,Ms::min()));r.collectGarbage();assert(r.getPeers().empty());assert(r.addAddresses(p,{},Ms(0)).value()==false);assert(r.getPeers().contains(p));r.collectGarbage();assert(r.getPeers().empty());
}
void faults_and_callbacks()
{
 const auto p=id(1),q=id(2);const auto a=addr(1),b=addr(2);unsigned failures=0,commits=0;
 for(bool upsert:{false,true})for(long site=0;site<28;++site)
 {
  auto budget=std::make_shared<SdkAddressBudget>();SdkAddressRepository r(std::make_shared<Dns>(),budget);assert(add(r,p,a));auto before=budget->stats();fail_after=site;bool accepted=(upsert?r.upsertAddresses(p,std::span(&b,1),Ms(500)):r.addAddresses(p,std::span(&b,1),Ms(500))).value();fail_after=-1;
  if(!accepted){++failures;auto after=budget->stats();assert(!has(r,p,b)&&has(r,p,a)&&after.encoded_bytes==before.encoded_bytes&&after.slots==before.slots&&after.addresses==before.addresses);assert(add(r,p,b)&&has(r,p,b));}
  else{++commits;assert(has(r,p,b)&&has(r,p,a));}
 }
 assert(failures>=6&&commits>0);
 auto budget=std::make_shared<SdkAddressBudget>();SdkAddressRepository r(std::make_shared<Dns>(),budget);unsigned callbacks=0;
 auto subscription=r.onAddressAdded([&](const Id& who,const Address& address){++callbacks;assert(has(r,who,address));assert(!add(r,q,b));bool refused=false;try{r.clear(who);}catch(const std::logic_error&){refused=true;}assert(refused);throw std::runtime_error("observer fault");});
 assert(add(r,p,a)&&has(r,p,a)&&callbacks==1);assert(budget->stats().observer_failures==1&&budget->stats().reentrant_refusals==2);subscription.disconnect();
}
void bootstrap_retirement()
{
 auto dns=std::make_shared<Dns>();auto budget=std::make_shared<SdkAddressBudget>();auto r=std::make_shared<SdkAddressRepository>(dns,budget);const auto p=id(1),q=id(2);const auto a=addr(1);assert(add(*r,p,a));dns->defer=true;dns->result={Address::create("/ip4/127.0.0.1/tcp/10002/p2p/"+q.toBase58()).value()};unsigned callbacks=0;
 r->bootstrap(Address::create("/dnsaddr/fixture.local").value(),[&](auto result){assert(result);++callbacks;});assert(dns->pending&&dns->loads==1);r->retire();r->bootstrap(Address::create("/dnsaddr/other.local").value(),[&](auto result){assert(!result);++callbacks;});assert(callbacks==1&&dns->loads==1);r.reset();assert(budget->stats().peers==1);auto pending=std::move(dns->pending);pending(dns->result);pending={};assert(callbacks==2&&budget->stats().peers==0&&budget->stats().retired_bypasses==1);
 auto fresh=std::make_shared<SdkAddressRepository>(dns,budget);dns->defer=false;fresh->bootstrap(Address::create("/dnsaddr/fresh.local").value(),[&](auto result){assert(result);++callbacks;});assert(has(*fresh,q,dns->result[0]));
}
void concurrent_repositories()
{
 auto budget=std::make_shared<SdkAddressBudget>(SdkAddressLimits{8,8,8,4096,2});auto dns=std::make_shared<Dns>();auto a=std::make_shared<SdkAddressRepository>(dns,budget),b=std::make_shared<SdkAddressRepository>(dns,budget);const auto address=addr(1);std::vector<std::thread> threads;
 for(unsigned n=0;n<8;++n)threads.emplace_back([&,n]{for(unsigned i=0;i<128;++i){auto p=id(1+n*128+i);(void)add(n%2?*a:*b,p,address);auto stats=budget->stats();assert(stats.peers<=8&&stats.addresses<=8&&stats.slots<=8&&stats.encoded_bytes<=4096);}});
 for(auto& t:threads)t.join();a.reset();b.reset();auto stats=budget->stats();assert(!stats.peers&&!stats.addresses&&!stats.slots&&!stats.encoded_bytes);
}

void staged_eviction_faults()
{
 const auto p=id(1),q=id(2),next=id(3);const auto a=addr(1),b=addr(2),c=addr(3);
 const std::array<Address,2> incoming{b,c};unsigned failures=0,successes=0;
 for(bool upsert:{false,true})for(long site=0;site<48;++site)
 {
  auto budget=std::make_shared<SdkAddressBudget>(SdkAddressLimits{2,4,4,4096,2});
  SdkAddressRepository r(std::make_shared<Dns>(),budget);r.protect([&](const Id& who){return who==p;});
  assert(add(r,p,a)&&add(r,q,a));const auto before=budget->stats();
  unsigned removed=0,added=0;
  auto removal=r.onAddressRemoved([&](const Id& who,const Address& address){++removed;assert(who==q&&address==a);});
  auto addition=r.onAddressAdded([&](const Id& who,const Address& address){++added;assert(who==next&&has(r,who,address));});
  fail_after=site;
  const auto accepted=(upsert?r.upsertAddresses(next,incoming,Ms(1000)):r.addAddresses(next,incoming,Ms(1000))).value();
  fail_after=-1;
  if(!accepted)
  {
   ++failures;const auto after=budget->stats();
   assert(!removed&&!added&&has(r,p,a)&&has(r,q,a)&&!r.getAddresses(next));
   assert(after.peers==before.peers&&after.addresses==before.addresses&&after.slots==before.slots&&after.encoded_bytes==before.encoded_bytes&&!after.evicted_peers);
   assert(r.addAddresses(next,incoming,Ms(1000)).value());
  }
  else ++successes;
  assert(has(r,p,a)&&!r.getAddresses(q)&&has(r,next,b)&&has(r,next,c));
  assert(budget->stats().peers==2&&budget->stats().addresses==3&&budget->stats().evicted_peers==1);
 }
 assert(failures>=10&&successes>0);
}

void retained_slots_and_duplicate_hits()
{
 const auto p=id(1),q=id(2);const auto a=addr(1),b=addr(2),c=addr(3);
 auto budget=std::make_shared<SdkAddressBudget>(SdkAddressLimits{4,4,2,4096,2});
 SdkAddressRepository r(std::make_shared<Dns>(),budget);r.protect([](const Id&){return true;});
 assert(add(r,p,a)&&add(r,p,b,Ms(0)));assert(budget->stats().slots==2);
 r.collectGarbage();assert(budget->stats().addresses==1&&budget->stats().slots==2);
 fail_after=0;assert(!add(r,p,a)&&fail_after==0);fail_after=-1;
 assert(!add(r,q,c)&&!r.getAddresses(q)&&has(r,p,a));
 r.clear(p);assert(budget->stats().slots==0);assert(add(r,q,c));
 auto limited=std::make_shared<SdkAddressBudget>(SdkAddressLimits{4,4,4,4096,2});
 SdkAddressRepository duplicates(std::make_shared<Dns>(),limited);
 const std::array<Address,6> input{a,a,b,a,c,b};
 assert(duplicates.addAddresses(p,input,lp::peer::ttl::kPermanent).value());
 const auto rows=duplicates.getAddresses(p).value();assert(rows.size()==2&&rows[0]==a&&rows[1]==b&&!has(duplicates,p,c));
 const std::array<Address,3> refresh{a,c,a};assert(!duplicates.upsertAddresses(p,refresh,Ms(0)).value());
 duplicates.collectGarbage();assert(!has(duplicates,p,a)&&has(duplicates,p,b)&&!has(duplicates,p,c));
}

void frozen_victims_and_optional_observers()
{
 const auto p=id(1),q=id(2),r=id(3),next=id(4);const auto a=addr(1),b=addr(2);
 auto budget=std::make_shared<SdkAddressBudget>(SdkAddressLimits{3,3,3,4096,2});
 SdkAddressRepository repo(std::make_shared<Dns>(),budget);
 assert(add(repo,p,a)&&add(repo,q,a)&&add(repo,r,a));
 bool changed=false;unsigned protected_calls=0,removed=0,added=0;
 repo.protect([&](const Id& who){++protected_calls;return who==p||changed;});
 auto removal=repo.onAddressRemoved([&](const Id& who,const Address&){
  assert(who==q||who==r);changed=true;++removed;assert(!add(repo,id(9),a));
  throw std::runtime_error("optional removal observer");
 });
 auto addition=repo.onAddressAdded([&](const Id& who,const Address& address){
  ++added;assert(who==next&&has(repo,who,address));throw std::runtime_error("optional addition observer");
 });
 const std::array<Address,2> incoming{a,b};assert(repo.addAddresses(next,incoming,Ms(1000)).value());
 assert(protected_calls==3&&removed==2&&added==2&&has(repo,p,a)&&has(repo,next,a)&&has(repo,next,b));
 assert(!repo.getAddresses(q)&&!repo.getAddresses(r));
 auto stats=budget->stats();assert(stats.observer_failures==4&&stats.reentrant_refusals==2&&stats.peers==2&&stats.addresses==3);
 removal.disconnect();addition.disconnect();
 repo.protect([](const Id&)->bool{throw std::runtime_error("protection unavailable");});
 assert(!add(repo,id(5),a));assert(has(repo,p,a)&&has(repo,next,a)&&budget->stats().protection_failures==2);
}

int main(){limits_and_owners();ttl_order_clear();faults_and_callbacks();bootstrap_retirement();concurrent_repositories();staged_eviction_faults();retained_slots_and_duplicate_hits();frozen_victims_and_optional_observers();std::cout<<"SDK address publication, limits, lifetime, TTL, bootstrap and concurrency passed\n";}
