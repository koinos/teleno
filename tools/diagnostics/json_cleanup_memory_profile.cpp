#include "core/owned_allocation.hpp"
#include "core/owned_memory_budget.hpp"
#include "jsonrpc/json_cleanup.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <time.h>

namespace probe {
thread_local bool enabled = false;
thread_local std::uint64_t requests = 0, requested_bytes = 0;
}

void* operator new( std::size_t bytes )
{
  auto* pointer = koinos::node::owned_allocation::allocate( bytes );
  if( probe::enabled ) { ++probe::requests; probe::requested_bytes += bytes; }
  return pointer;
}
void* operator new[]( std::size_t bytes ) { return ::operator new( bytes ); }

namespace {
using nlohmann::json;
using koinos::node::OwnedMemoryBudget;

std::uint64_t thread_cpu_ns()
{
  timespec value{};
  if( clock_gettime( CLOCK_THREAD_CPUTIME_ID, &value ) )
    throw std::runtime_error( "thread CPU measurement unavailable" );
  return std::uint64_t( value.tv_sec ) * 1000000000 + value.tv_nsec;
}

json input( const std::string& shape )
{
  if( shape == "wide" )
  {
    json tree = json::object();
    for( unsigned i = 0; i < 8192; ++i )
      tree[ std::to_string( i ) ] = json::array( { std::string( 128, 'w' ),
        json::object( { { "id", i }, { "binary", json::binary( { 0, 1, 255 } ) } } ) } );
    return tree;
  }
  const bool branched = shape == "branched";
  if( !branched && shape != "unary" ) throw std::runtime_error( "unknown profile shape" );
  const auto depth = branched ? 1024u : 8192u;
  json tree = std::string( 4096, 'x' );
  for( unsigned i = 0; i < depth; ++i )
  {
    auto parent = json::array();
    if( branched ) parent.push_back( 42 );
    parent.push_back( std::move( tree ) );
    tree = std::move( parent );
  }
  return tree;
}
} // namespace

int main( int argc, char** argv )
{
  try
  {
    if( argc != 3 ) throw std::runtime_error( "usage: json_cleanup_memory_profile baseline|candidate wide|unary|branched" );
    const std::string variant = argv[ 1 ], shape = argv[ 2 ];
    if( variant != "baseline" && variant != "candidate" ) throw std::runtime_error( "unknown profile variant" );
    constexpr unsigned iterations = 16;
    std::uint64_t elapsed_ns = 0, cpu_ns = 0, released = 0;
    std::uint64_t peak_increment = 0, peak_accounted = 0, retained_before = 0;
    for( unsigned i = 0; i < iterations; ++i )
    {
      OwnedMemoryBudget budget( 128 * 1024 * 1024 );
      OwnedMemoryBudget::Scope scope( budget, false );
      auto tree = input( shape );
      const auto before = budget.snapshot();
      retained_before = std::max( retained_before, before.accounted_bytes );
      const auto cpu_start = thread_cpu_ns();
      const auto start = std::chrono::steady_clock::now();
      probe::enabled = true;
      if( variant == "candidate" )
        koinos::node::jsonrpc::clear_json_without_allocation( tree );
      else
      {
        json retiring;
        tree.swap( retiring );
      } // Measure the ordinary destructor, including its allocation/deletion.
      probe::enabled = false;
      const auto finish = std::chrono::steady_clock::now();
      cpu_ns += thread_cpu_ns() - cpu_start;
      elapsed_ns += std::chrono::duration_cast< std::chrono::nanoseconds >( finish - start ).count();
      const auto after = budget.snapshot();
      if( !tree.is_null() || after.accounted_bytes != after.bootstrap_accounted_bytes || after.allocator_requested_bytes )
        throw std::runtime_error( "profile changed owner release semantics" );
      released += before.accounted_bytes - after.accounted_bytes;
      peak_increment = std::max( peak_increment, after.peak_accounted_bytes - before.peak_accounted_bytes );
      peak_accounted = std::max( peak_accounted, after.peak_accounted_bytes );
    }
    rusage usage{};
    if( getrusage( RUSAGE_SELF, &usage ) ) throw std::runtime_error( "process resource measurement unavailable" );
    const auto seconds = []( const timeval& t ) { return double( t.tv_sec ) + double( t.tv_usec ) / 1000000; };
#ifdef __APPLE__
    const auto peak_rss_bytes = std::uint64_t( usage.ru_maxrss );
#else
    const auto peak_rss_bytes = std::uint64_t( usage.ru_maxrss ) * 1024;
#endif
    std::cout << "{\"variant\":\"" << variant << "\",\"shape\":\"" << shape
              << "\",\"iterations\":" << iterations << ",\"cleanup_allocation_requests\":" << probe::requests
              << ",\"cleanup_requested_bytes\":" << probe::requested_bytes
              << ",\"cleanup_elapsed_ns\":" << elapsed_ns << ",\"cleanup_thread_cpu_ns\":" << cpu_ns
              << ",\"maximum_retained_accounted_before_bytes\":" << retained_before
              << ",\"maximum_accounted_peak_bytes\":" << peak_accounted
              << ",\"maximum_new_accounted_peak_bytes\":" << peak_increment
              << ",\"released_accounted_bytes_sum\":" << released
              << ",\"process_peak_rss_bytes\":" << peak_rss_bytes
              << ",\"process_user_seconds\":" << seconds( usage.ru_utime )
              << ",\"process_system_seconds\":" << seconds( usage.ru_stime )
              << ",\"physical_input_512b_blocks\":" << usage.ru_inblock
              << ",\"physical_output_512b_blocks\":" << usage.ru_oublock << "}\n";
    return 0;
  }
  catch( const std::exception& error )
  {
    probe::enabled = false;
    std::cerr << error.what() << '\n';
    return 1;
  }
}
