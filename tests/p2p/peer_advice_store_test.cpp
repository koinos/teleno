#include "p2p/peer_advice_store.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>

namespace { thread_local long fail_after=-1; }
void* operator new(std::size_t n){if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
using namespace koinos::node::p2p;
using namespace koinos::node::p2p::detail;
PeerID peer(unsigned n){return {std::string(80,'p')+std::to_string(n),std::string(120,'a')+std::to_string(n)};}
std::vector<PeerID> view(const PeerAdviceStore& s){std::vector<PeerID> out;s.visit([&](const auto& p){out.push_back(p);});return out;}
bool contains(const PeerAdviceStore& s,const PeerID& p){bool yes=false;s.visit([&](const auto& row){yes|=row.id==p.id&&row.address==p.address;});return yes;}
void protected_churn()
{
 PeerAdviceStore s(3,4096);const auto a=peer(1),b=peer(2);assert(s.remember(a,true));assert(s.remember(b,false,true));
 for(unsigned i=3;i<2000;++i){assert(s.remember(peer(i)));assert(s.stats().entries==3&&contains(s,a)&&contains(s,b));assert(s.stats().peer_capacity_bytes<=4096);}
 assert(s.stats().evictions==1996);s.disconnected(b.id);assert(s.remember(peer(2000)));assert(!contains(s,b)&&contains(s,a));
 s.disconnected_all();assert(contains(s,a));
 PeerAdviceStore full(2,4096);assert(full.remember(a,true)&&full.remember(b,false,true));const auto before=full.stats();assert(!full.remember(peer(3)));assert(!full.remember(peer(4),true,true));assert(full.stats().entries==before.entries&&full.stats().peer_capacity_bytes==before.peer_capacity_bytes&&contains(full,a)&&contains(full,b));
 const auto refused=peer(5);fail_after=0;assert(!full.remember(refused)&&fail_after==0);assert(full.remember(a,true)&&fail_after==0);fail_after=-1;
 // Protection belongs to the authoritative connection even if advice replacement fails.
 PeerAdviceStore pinned(1,4096);assert(pinned.remember(a));pinned.protect(a.id,false,true);auto changed=a;changed.address+=std::string(100,'x');fail_after=0;assert(!pinned.remember(changed));fail_after=-1;assert(contains(pinned,a)&&!pinned.remember(b));pinned.disconnected(a.id);assert(pinned.remember(b));
}
void bytes_and_failures()
{
 const auto a=peer(1),b=peer(2),c=peer(3);PeerAdviceStore probe(3,4096);assert(probe.remember(a));const auto charge=probe.stats().peer_capacity_bytes;PeerAdviceStore exact(8,charge);assert(exact.remember(a));assert(exact.remember(b));assert(exact.stats().entries==1&&!contains(exact,a)&&contains(exact,b));exact.protect(b.id,true,false);assert(!exact.remember(c));
 const PeerID huge{std::string(4096,'x'),"y"};fail_after=0;assert(!exact.remember(huge)&&fail_after==0);fail_after=-1;
 unsigned failed=0;
 for(long site=0;site<9;++site)
 {
  PeerAdviceStore s(2,4096);assert(s.remember(a)&&s.remember(b));const auto before=s.stats();fail_after=site;const bool added=s.remember(c);fail_after=-1;
  if(!added){++failed;assert(s.stats().entries==2&&s.stats().peer_capacity_bytes==before.peer_capacity_bytes&&contains(s,a)&&contains(s,b)&&!contains(s,c));assert(s.remember(c));}
  assert(s.stats().entries==2&&contains(s,b)&&contains(s,c)&&!contains(s,a));
 }
 assert(failed>=4);
 for(long site=0;site<3;++site)
 {
  PeerAdviceStore s(2,4096);assert(s.remember(a));auto changed=a;changed.address+=std::string(100,'z');fail_after=site;const bool updated=s.remember(changed);fail_after=-1;assert(s.stats().entries==1);
  if(!updated)assert(contains(s,a));else assert(contains(s,changed));assert(s.remember(changed)&&contains(s,changed));
 }
 PeerAdviceStore small(1,2*std::string().capacity());assert(small.remember({"a","b"}));assert(small.stats().peer_capacity_bytes==2*std::string().capacity());
 for(auto limits:std::vector<std::pair<size_t,size_t>>{{0,1024},{1,0}}){bool bad=false;try{PeerAdviceStore invalid(limits.first,limits.second);}catch(const std::invalid_argument&){bad=true;}assert(bad);}
}
int main(){protected_churn();bytes_and_failures();std::cout<<"Peer advice capacity, protected owners, recency/index lifetime and allocation rollback passed\n";}
