#include "core/owned_allocation.hpp"
#include "core/owned_memory_budget.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>

#ifndef TELENO_NLOHMANN_ALLOCATION_FREE_DESTROY
#error "This regression requires the verified build-owned nlohmann cleanup backport"
#endif

namespace fault {
thread_local bool fail = false;
thread_local unsigned attempted = 0;
}

void* operator new( std::size_t bytes )
{
  if( fault::fail ) { ++fault::attempted; throw std::bad_alloc(); }
  return koinos::node::owned_allocation::allocate( bytes );
}
void* operator new[]( std::size_t bytes ) { return ::operator new( bytes ); }

namespace {
using koinos::node::OwnedMemoryBudget;
using nlohmann::json;

void require( bool condition, const char* message )
{
  if( !condition ) throw std::runtime_error( message );
}

template< typename Json > Json deep( bool object, bool branch, bool reverse )
{
  Json value = std::string( 4096, 'x' );
  const auto depth = branch ? 384u : 8192u;
  for( unsigned i = 0; i < depth; ++i )
  {
    Json parent = object ? Json::object() : Json::array();
    if( object )
    {
      if( branch ) parent[ reverse ? "z" : "a" ] = std::string( 32, 's' );
      parent[ reverse ? "a" : "z" ] = std::move( value );
    }
    else
    {
      if( branch && !reverse ) parent.push_back( std::string( 32, 's' ) );
      parent.push_back( std::move( value ) );
      if( branch && reverse ) parent.push_back( std::string( 32, 's' ) );
    }
    value = std::move( parent );
  }
  return value;
}

template< typename Factory > void default_cleanup( Factory factory )
{
  OwnedMemoryBudget budget( 128 * 1024 * 1024 );
  OwnedMemoryBudget::Scope scope( budget, false );
  const auto bootstrap = budget.snapshot().accounted_bytes;
  auto value = factory();
  const auto held = budget.snapshot();
  auto full = budget.reserve( held.limit_bytes - held.accounted_bytes );
  const auto before = fault::attempted;
  {
    auto retiring = std::move( value );
    fault::fail = true;
  } // Exercise the actual dependency destructor, without Teleno's outer guard.
  fault::fail = false;
  require( fault::attempted == before, "default cleanup attempted ordinary new/new[]" );
  full.reset();
  require( budget.snapshot().accounted_bytes == bootstrap &&
           !budget.snapshot().allocator_requested_bytes, "default cleanup retained credits" );
}

void partial_copy_refusal_and_retry()
{
  const auto input = std::string( R"({"receipt":{"id":"test","logs":[")" ) +
    std::string( 8192, 'x' ) + R"("],"events":[{"name":"event","impacted":["a","b"]}]}})";
  const auto original = json::parse( input );
  const auto expected = original.dump();
  unsigned refused = 0, copied = 0;
  for( unsigned limit = 512; limit < 32768; ++limit )
  {
    OwnedMemoryBudget budget( limit );
    bool equal = true;
    {
      OwnedMemoryBudget::Scope scope( budget, false );
      try
      {
        auto copy = original;
        equal = copy == original;
        ++copied;
      }
      catch( const koinos::node::LocalCapacityExceeded& ) { ++refused; }
    }
    const auto after = budget.snapshot();
    require( equal, "successful JSON copy changed source bytes" );
    require( after.accounted_bytes == after.bootstrap_accounted_bytes &&
             !after.allocator_requested_bytes, "partial copy retained credits" );
    require( original.dump() == expected, "failed JSON copy changed its original owner" );
  }
  require( refused && copied, "copy sweep did not cover refusal and successful retry" );
  std::cout << "JSON_DEFAULT_COPY refusals=" << refused << " successes=" << copied << '\n';
}

void cross_thread_final_owner()
{
  OwnedMemoryBudget budget( 1024 * 1024 );
  json held;
  {
    OwnedMemoryBudget::Scope scope( budget, false );
    held = { { "logs", { std::string( 16384, 'r' ) } } };
  }
  const auto before = budget.snapshot();
  auto full = budget.reserve( before.limit_bytes - before.accounted_bytes );
  std::thread consumer( [value = std::move( held )]() mutable {
    const auto attempts = fault::attempted;
    fault::fail = true;
    value = nullptr;
    fault::fail = false;
    require( attempts == fault::attempted, "cross-thread default cleanup allocated" );
  } );
  consumer.join();
  full.reset();
  require( budget.snapshot().accounted_bytes == before.bootstrap_accounted_bytes &&
           !budget.snapshot().allocator_requested_bytes, "final JSON owner did not release its credits" );
}

void partial_initializer_refusal_and_retry()
{
  const json payload = { { "logs", { std::string( 8192, 'r' ) } }, { "nonce", 17 } };
  const json expected = { { "error", payload }, { "id", 51 }, { "jsonrpc", "2.0" } };
  unsigned refused = 0, completed = 0;
  for( unsigned limit = 512; limit < 32768; ++limit )
  {
    OwnedMemoryBudget budget( limit );
    {
      OwnedMemoryBudget::Scope scope( budget, false );
      try
      {
        json value = { { "error", payload }, { "id", 51 }, { "jsonrpc", "2.0" } };
        require( value == expected, "initializer publication changed JSON values" );
        ++completed;
      }
      catch( const koinos::node::LocalCapacityExceeded& ) { ++refused; }
    }
    const auto after = budget.snapshot();
    require( after.accounted_bytes == after.bootstrap_accounted_bytes &&
             !after.allocator_requested_bytes, "partial initializer retained credits" );
  }
  require( refused && completed, "initializer sweep missed refusal or successful retry" );
  std::cout << "JSON_DEFAULT_INITIALIZER refusals=" << refused
            << " successes=" << completed << '\n';
}
} // namespace

int main()
{
  std::set_terminate( [] { std::_Exit( 86 ); } );
  try
  {
    default_cleanup( [] { return json(); } );
    default_cleanup( [] { return json( std::string( 4096, 's' ) ); } );
    default_cleanup( [] { return json::array(); } );
    default_cleanup( [] { return json::object(); } );
    default_cleanup( [] { return json::binary( { 0, 1, 255 } ); } );
    default_cleanup( [] { return json::parse( "{", nullptr, false ); } );
    default_cleanup( [] {
      auto value = json::object();
      for( unsigned i = 0; i < 2048; ++i )
        value[ std::to_string( i ) ] = json::array( { std::string( 128, 'w' ), i } );
      return value;
    } );
    for( bool object : { false, true } )
    {
      default_cleanup( [=] { return deep< json >( object, false, false ); } );
      for( bool reverse : { false, true } )
        default_cleanup( [=] { return deep< json >( object, true, reverse ); } );
    }
    default_cleanup( [] { return deep< nlohmann::ordered_json >( true, true, false ); } );
    default_cleanup( [] { return deep< nlohmann::ordered_json >( true, true, true ); } );
    partial_copy_refusal_and_retry();
    partial_initializer_refusal_and_retry();
    cross_thread_final_owner();
    std::cout << "Default JSON cleanup, partial-copy refusal and final-owner release completed\n";
    return 0;
  }
  catch( const std::exception& error )
  {
    fault::fail = false;
    std::cerr << error.what() << '\n';
    return 1;
  }
}
