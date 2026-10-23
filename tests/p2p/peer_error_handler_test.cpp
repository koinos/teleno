#include "core/owned_allocation.hpp"
#include "p2p/error_handler.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
thread_local int allocation_failure = -1;
void require( bool valid, const char* message ) { if( !valid ) throw std::runtime_error( message ); }
}
void* operator new( std::size_t size )
{
  if( allocation_failure == 0 ) { allocation_failure = -1; throw std::bad_alloc(); }
  if( allocation_failure > 0 ) --allocation_failure;
  if( auto result = koinos::node::owned_allocation::allocate( size ? size : 1 ) ) return result;
  throw std::bad_alloc();
}
void* operator new[]( std::size_t size ) { return ::operator new( size ); }
void operator delete( void* pointer ) noexcept { koinos::node::owned_allocation::deallocate( pointer ); }
void operator delete[]( void* pointer ) noexcept { koinos::node::owned_allocation::deallocate( pointer ); }
void operator delete( void* pointer, std::size_t ) noexcept { koinos::node::owned_allocation::deallocate( pointer ); }
void operator delete[]( void* pointer, std::size_t ) noexcept { koinos::node::owned_allocation::deallocate( pointer ); }

using namespace koinos::node::p2p;
namespace {
using Handler = PeerErrorHandler;
using Admission = Handler::Admission;
struct ManualClock
{
  std::atomic< int64_t > nanoseconds{ 3600000000000LL };
  Handler::Clock::time_point now() const
  { return Handler::Clock::time_point( std::chrono::duration_cast< Handler::Clock::duration >( std::chrono::nanoseconds( nanoseconds.load() ) ) ); }
  void advance( std::chrono::nanoseconds amount ) { nanoseconds.fetch_add( amount.count() ); }
  Handler::Now source() { return [this] { return now(); }; }
};
P2POptions options()
{
  P2POptions o;o.error_score_disconnect_threshold=100;o.error_score_reconnect_threshold=50;
  o.error_score_halflife=std::chrono::minutes(1);return o;
}

void addresses()
{
  ManualClock clock; Handler h(options(),clock.source());
  h.record_error("/ip4/192.0.2.1/tcp/10000/p2p/a",100);
  require(!h.can_connect("/ip4/192.0.2.1/tcp/10001/p2p/b"),"port/peer-ID rotation bypassed a network-address ban");
  require(h.get_score("/ip4/192.0.2.1/udp/10002")==100,"one network address must share its score");
  require(h.can_connect("/ip4/192.0.2.2/tcp/10000"),"unrelated network address inherited a ban");
  h.record_error("/ip6/2001:0DB8:0:0:0:0:0:1/tcp/10000",100);
  require(!h.can_connect("/ip6/2001:db8::1/tcp/10001/p2p/c"),"equivalent IPv6 spellings bypassed a ban");
  h.record_error("/dns4/example.invalid/tcp/10000/p2p/d",100);
  require(!h.can_connect("/dns4/example.invalid/tcp/10001/p2p/e"),"DNS port rotation bypassed a ban");
  require(h.stats().records==3,"network-address aliases created redundant records");
  h.record_error("identity-without-address",100);
  require(!h.can_connect("identity-without-address")&&h.can_connect("different-identity"),"addressless identities must remain separate");
  require(h.admission("")==Admission::local_capacity,"unattributable empty identity must fail locally");
  auto copy=PeerErrorAddress("/ip6/2001:0db8::1/tcp/80");auto moved=std::move(copy);
  require(moved.value()=="/ip6/2001:db8::1","address key view refers to moved stack storage");
}

void query_decay()
{
  ManualClock clock;auto o=options();o.error_score_reconnect_threshold=100;
  Handler h(o,clock.source());const std::string address="/ip4/192.0.2.3/tcp/10000";
  h.record_error(address,200);clock.advance(std::chrono::seconds(62));
  require(h.can_connect(address)&&h.get_score(address)==97,"idle score did not decay on admission query");
  clock.advance(std::chrono::minutes(20));require(h.get_score(address)==0,"expired score remained nonzero");
  require(h.stats().records==0&&h.stats().key_capacity_bytes==0,"expired key credits were not released");
  h.record_error(address,100);clock.advance(-std::chrono::seconds(20));
  require(h.get_score(address)==100,"backwards test clock increased or changed the score");
  clock.advance(std::chrono::seconds(82));require(h.get_score(address)==48,"backwards clock extended the decay interval");
}

void saturation_and_zero_threshold()
{
  ManualClock clock; Handler h(options(),clock.source());const std::string address="/ip4/192.0.2.4/tcp/80";
  const auto maximum=std::numeric_limits<uint64_t>::max();h.record_error(address,maximum);
  const auto result=h.record_error(address,1);
  require(result.score==maximum&&result.disconnect&&!result.local_capacity,"score overflow cleared a severe ban");
  require(h.stats().saturated_additions==1&&h.get_score(address)==maximum,"extreme score conversion is not defined/saturated");
  clock.advance(std::chrono::nanoseconds(1));require(h.get_score(address)>maximum/2,"extreme score decay wrapped");
  auto o=options();o.error_score_disconnect_threshold=0;o.error_score_reconnect_threshold=0;
  Handler zero(o,clock.source());const auto event=zero.record_error(address,0);
  require(event.disconnect&&!event.local_capacity,"zero thresholds lost their literal scoring semantics");
  clock.advance(std::chrono::hours(24));zero.collect_expired();
  require(zero.stats().records==1&&zero.admission(address)==Admission::score_blocked,"GC discarded a meaningful zero-threshold record");
}

void bounded_records_and_work()
{
  ManualClock clock;auto o=options();o.max_peer_error_records=33;o.peer_error_key_capacity_bytes=8192;
  Handler h(o,clock.source());
  for(unsigned i=0;i<33;++i)h.record_error("/ip4/192.0.2."+std::to_string(i)+"/tcp/80",1000);
  const auto full=h.stats();require(full.records==33,"record count did not fill exactly");
  require(h.admission("/ip4/198.51.100.1/tcp/80")==Admission::local_capacity,"full score inventory admitted a new address");
  const auto refused=h.record_error("/ip4/198.51.100.1/tcp/80",1);
  require(refused.local_capacity&&refused.disconnect&&h.stats().records==33,"count exhaustion published an untracked score");
  require(h.get_score("/ip4/192.0.2.0/tcp/81")==1000,"capacity pressure evicted or weakened a meaningful score");
  clock.advance(std::chrono::minutes(30));h.collect_expired();
  require(h.stats().records==17&&h.stats().last_sweep_work==16,"GC exceeded its fixed per-call work budget");
  require(!h.record_error("/ip4/198.51.100.1/tcp/80",5).local_capacity,"expired capacity was not reusable");
  h.collect_expired();require(h.stats().records==1&&h.stats().expired==33,"bounded cursor GC did not complete or corrupted its iterator");
  require(h.get_score("/ip4/198.51.100.1/tcp/81")==5,"GC rounded or erased a fresh score");
  require(h.stats().maximum_sweep_work<=Handler::sweep_work_limit,"GC traversal became unbounded");
  ManualClock small_clock;Handler small(options(),small_clock.source());small.record_error("/ip4/203.0.113.1/tcp/80",1000);
  small_clock.advance(std::chrono::nanoseconds(1));for(unsigned i=0;i<100;++i)small.collect_expired();
  require(small.get_score("/ip4/203.0.113.1/tcp/80")==999,"sweeps repeatedly rounded an unrelated live score");
}

void byte_capacity_and_allocation_failure()
{
  ManualClock clock;const std::string a="/ip4/192.0.2.1/tcp/80",b="/ip4/192.0.2.2/tcp/80";
  Handler sample(options(),clock.source());sample.record_error(a,10);
  auto o=options();o.peer_error_key_capacity_bytes=sample.stats().key_capacity_bytes;
  Handler limited(o,clock.source());limited.record_error(a,10);
  require(limited.record_error(b,10).local_capacity,"retained-key byte capacity allowed an overshoot");
  require(limited.can_connect(a)&&limited.get_score(a)==10,"byte capacity penalized an existing address");
  require(limited.stats().records==1&&limited.stats().key_capacity_bytes<=o.peer_error_key_capacity_bytes,"attributed key-byte accounting is wrong");
  const std::string oversized(4096,'x');allocation_failure=0;
  const auto refusal=limited.record_error(oversized,10);const auto untouched=allocation_failure;allocation_failure=-1;
  require(refusal.local_capacity&&untouched==0,"oversized borrowed key allocated before rejection");
  Handler fault(options(),clock.source());fault.record_error(a,100);
  const auto before=fault.stats();const std::string address="/dns4/"+std::string(200,'a')+"/tcp/10000";
  unsigned observed=0;bool success=false;
  for(int fail=0;fail<8;++fail)
  {
    allocation_failure=fail;const auto result=fault.record_error(address,5);allocation_failure=-1;
    if(!result.local_capacity){success=true;break;}
    ++observed;const auto current=fault.stats();
    require(current.records==before.records&&current.key_capacity_bytes==before.key_capacity_bytes,"allocation failure published a partial score or leaked capacity");
    require(fault.get_score(a)==100,"allocation failure changed a meaningful ban");
  }
  require(success&&observed>=2,"key/map allocation fault sites were not exercised");
  require(fault.stats().records==2&&fault.stats().allocation_refusals==observed,"allocation failure classification is wrong");
  clock.advance(std::chrono::hours(24));fault.collect_expired();
  require(fault.stats().records==0&&fault.stats().key_capacity_bytes==0,"failed/new key credits did not release");
}

void concurrent_bookkeeping()
{
  ManualClock clock;auto o=options();o.max_peer_error_records=32;o.peer_error_key_capacity_bytes=8192;
  Handler h(o,clock.source());std::atomic<bool> valid{true};std::vector<std::thread> threads;
  for(unsigned t=0;t<8;++t)threads.emplace_back([&,t]{
    for(unsigned i=0;i<512;++i)
    {
      const std::string address="/ip6/2001:db8::"+std::to_string(t*512+i+1)+"/tcp/10000";
      const auto result=h.record_error(address,1);
      if(!result.local_capacity&&h.get_score(address)!=1)valid=false;
      (void)h.admission(address);h.collect_expired();const auto s=h.stats();
      if(s.records>o.max_peer_error_records||s.key_capacity_bytes>o.peer_error_key_capacity_bytes||s.maximum_sweep_work>16)valid=false;
    }
  });for(auto&thread:threads)thread.join();
  require(valid&&h.stats().records==32&&h.stats().capacity_refusals>0,"concurrent score/GC/admission bounds failed");
  clock.advance(std::chrono::hours(24));for(unsigned i=0;i<3;++i)h.collect_expired();
  require(h.stats().records==0&&h.stats().key_capacity_bytes==0,"concurrent bookkeeping did not release after expiry");
}

void invalid_options()
{
  for(unsigned mode=0;mode<4;++mode)
  {
    auto o=options();if(mode==0)o.max_peer_error_records=0;if(mode==1)o.peer_error_key_capacity_bytes=0;
    if(mode==2)o.error_score_halflife=std::chrono::minutes(0);bool failed=false;
    try{if(mode==3){Handler h(o,Handler::Now{});}else{Handler h(o);}}catch(const std::invalid_argument&){failed=true;}
    require(failed,"invalid score capacity/clock/half-life was accepted");
  }
}

void query_frequency_does_not_shorten_ban()
{
  ManualClock clock;Handler h(options(),clock.source());const std::string address="/ip4/203.0.113.2/tcp/80";
  h.record_error(address,1000);
  for(unsigned i=0;i<1000;++i)
  {
    clock.advance(std::chrono::nanoseconds(1));(void)h.can_connect(address);
    (void)h.get_score(address);(void)h.record_error(address,0);
  }
  require(h.get_score(address)==999&&h.admission(address)==Admission::score_blocked,
    "query/zero-event frequency shortened a ban through repeated integer rounding");
}

void reference_vectors()
{
  for(const uint64_t score:{uint64_t(1000),uint64_t(200000)})
    for(const int64_t elapsed:{int64_t(0),int64_t(1),int64_t(1000000000),int64_t(30000000000),int64_t(60000000000),int64_t(62000000000),int64_t(89000000000),int64_t(3600000000000)})
    {
      ManualClock clock;Handler h(options(),clock.source());const std::string address="/ip4/192.0.2.8/tcp/80";
      h.record_error(address,score);clock.advance(std::chrono::nanoseconds(elapsed));
      std::cout<<"{\"score\":"<<score<<",\"elapsed_ns\":"<<elapsed<<",\"decayed\":"<<h.get_score(address)<<"}"<<std::endl;
    }
}

void realtime_decay()
{
  auto o=options();o.error_score_reconnect_threshold=100;Handler h(o);
  const std::string address="/ip4/192.0.2.3/tcp/10000";h.record_error(address,200);
  std::this_thread::sleep_for(std::chrono::seconds(62));
  require(h.can_connect(address),"idle real-clock ban did not decay below its reconnect threshold");
}
}
int main(int argc,char**argv)
{
  try
  {
    if(argc==2&&std::string(argv[1])=="realtime-decay"){realtime_decay();return 0;}
    if(argc==2&&std::string(argv[1])=="reference-vectors"){reference_vectors();return 0;}
    if(argc==2&&std::string(argv[1])=="query-frequency"){query_frequency_does_not_shorten_ban();return 0;}
    require(argc==1,"unexpected test arguments");addresses();query_decay();saturation_and_zero_threshold();
    bounded_records_and_work();byte_capacity_and_allocation_failure();concurrent_bookkeeping();invalid_options();
    query_frequency_does_not_shorten_ban();
    std::cout<<"Peer error handler bounds, expiry, faults and concurrency passed"<<std::endl;return 0;
  }
  catch(const std::exception&e){allocation_failure=-1;std::cerr<<e.what()<<std::endl;return 1;}
}
