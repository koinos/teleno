#include "p2p/seen_cache.hpp"

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <set>
#include <thread>
#include <vector>
#if !defined(_WIN32)
#include <sys/resource.h>
#endif

// Single-threaded fault injection is disabled before the concurrent checks.
static std::atomic<int> fail_after{ -1 };
static std::atomic<size_t> allocations{ 0 };
static std::atomic<bool> counting{ false };
void* operator new( std::size_t size )
{
  if( counting ) ++allocations;
  if( fail_after >= 0 && fail_after.fetch_sub( 1 ) == 0 ) throw std::bad_alloc();
  if( auto p = std::malloc( size ? size : 1 ) ) return p;
  throw std::bad_alloc();
}
void operator delete( void* p ) noexcept { std::free( p ); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using Cache = koinos::node::p2p::SeenCache;
using namespace std::chrono_literals;

void check_bound( Cache& c )
{
  auto s = c.stats();
  assert( s.entries <= s.max_entries );
  assert( s.id_capacity_bytes <= s.max_id_capacity_bytes );
}

// Same generated keys in separate processes; does not retain an input vector.
// Legacy mode models the prior accepted-transaction std::set, not the full node.
void measure( bool legacy )
{
  Cache cache( 65536, 8 * 1024 * 1024, 600s );
  std::set<std::string> prior;
  auto start = Cache::Clock::now();
  for( size_t i = 0; i < 500000; ++i )
  {
    auto id = std::string( 34, 'x' );
    auto suffix = std::to_string( i );
    id.replace( id.size() - suffix.size(), suffix.size(), suffix );
    if( legacy ) prior.insert( id );
    else cache.remember( id );
    if( i == 99999 || i == 499999 )
    {
      auto s = cache.stats();
      std::cout << "{\"mode\":\"" << (legacy ? "legacy" : "bounded")
                << "\",\"submitted_ids\":" << i + 1
                << ",\"entries\":" << (legacy ? prior.size() : s.entries)
                << ",\"owned_id_capacity_bytes\":";
      if( legacy ) std::cout << "null";
      else std::cout << s.id_capacity_bytes;
      std::cout << ",\"capacity_evictions\":" << s.capacity_evictions
                << ",\"elapsed_us\":" << std::chrono::duration_cast<std::chrono::microseconds>(Cache::Clock::now()-start).count();
#if !defined(_WIN32)
      rusage u{};
      getrusage( RUSAGE_SELF, &u );
#if defined(__APPLE__)
      std::cout << ",\"process_peak_rss_bytes\":" << u.ru_maxrss;
#else
      std::cout << ",\"process_peak_rss_bytes\":" << u.ru_maxrss * 1024;
#endif
#endif
      std::cout << "}" << std::endl;
    }
  }
}

int main( int argc, char** argv )
{
  if( argc == 2 && (std::string(argv[1]) == "--measure-legacy" || std::string(argv[1]) == "--measure-bounded") )
  {
    measure( std::string(argv[1]) == "--measure-legacy" );
    return 0;
  }
  const auto t = Cache::Clock::now();
  Cache c( 2, 128, 10s );
  c.remember( "a", 1, t ); c.remember( "b", 2, t );
  assert( c.contains( "a", t + 1s ) );
  c.remember( "c", 3, t + 2s );
  assert( !c.contains( "a", t + 2s ) && c.contains( "b", t + 2s ) );
  assert( c.stats(t+2s).capacity_evictions == 1 );
  c.remember( "b", 2, t + 9s ); // duplicates never renew TTL/FIFO
  assert( !c.contains( "b", t + 10s ) && c.contains( "c", t + 10s ) );
  c.purge_irreversible( 3 );
  assert( c.stats(t+10s).entries == 0 && c.stats(t+10s).irreversible == 1 );
  c.remember( "late-old-sync", 2, t+10s );
  assert( c.stats(t+10s).entries == 0 );
  c.clear(); c.remember( "fresh", 1 ); assert( c.contains( "fresh" ) );
  c.clear(); assert( c.stats().id_capacity_bytes == 0 );

  Cache bytes( 100, 100, 600s );
  bytes.remember( std::string(60,'a') ); bytes.remember( std::string(60,'b') );
  assert( bytes.stats().entries == 1 ); check_bound(bytes);
  bytes.remember( std::string(1000,'z') );
  assert( bytes.stats().bypassed == 1 && bytes.stats().entries == 1 );
  Cache zero( 0, 0, 600s ); zero.remember("x"); assert(zero.stats().entries == 0);
  Cache no_time( 10, 100, 0s ); no_time.remember("x"); assert(no_time.stats().entries == 0);
  Cache huge( std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max(), 600s );
  huge.remember("ok"); assert(huge.contains("ok"));

  Cache hits( 10, 2048, 600s ); std::string key(128,'k'); hits.remember(key);
  allocations = 0; counting = true;
  for(int i=0;i<10000;++i) assert(hits.contains(key));
  counting = false; assert(allocations == 0);
  // Fail owned key, list-node and map-node allocation respectively.
  for(int fail=0;fail<3;++fail)
  {
    Cache fault( 10, 2048, 600s );
    fail_after = fail; fault.remember(key); fail_after = -1;
    assert(fault.stats().entries == 0 && fault.stats().id_capacity_bytes == 0);
    assert(fault.stats().bypassed == 1);
    fault.remember(key); assert(fault.contains(key));
  }
  Cache parallel( 64, 4096, 600s );
  std::vector<std::thread> workers;
  for(int i=0;i<4;++i) workers.emplace_back([&,i] {
    for(int j=0;j<10000;++j) {
      auto id=std::to_string(i)+":"+std::to_string(j);
      parallel.remember(id,static_cast<uint64_t>(j+1)); parallel.contains(id);
      if(j%100==0) parallel.purge_irreversible(j);
      check_bound(parallel);
    }
  });
  for(auto& w:workers) w.join();
  check_bound(parallel); parallel.clear(); assert(parallel.stats().entries == 0);
  std::cout << "seen cache invariants passed; warm hits allocate zero times\n";
}
