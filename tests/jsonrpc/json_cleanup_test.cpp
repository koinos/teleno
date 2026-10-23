#include "jsonrpc/json_cleanup.hpp"
#include "core/owned_allocation.hpp"
#include "core/owned_memory_budget.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>

namespace fault {
thread_local bool fail_allocations = false;
thread_local std::size_t attempted = 0;
}

void* operator new( std::size_t bytes )
{
  if( fault::fail_allocations )
  {
    ++fault::attempted;
    throw std::bad_alloc();
  }
  return koinos::node::owned_allocation::allocate( bytes );
}
void* operator new[]( std::size_t bytes ) { return ::operator new( bytes ); }

namespace {
using nlohmann::json;
using koinos::node::OwnedMemoryBudget;
using koinos::node::jsonrpc::JsonCleanup;
using koinos::node::jsonrpc::clear_json_without_allocation;
using koinos::node::jsonrpc::dump_owned_json;

void require( bool ok, const char* message )
{
  if( !ok ) throw std::runtime_error( message );
}

json nested( unsigned depth, bool branched, bool object, bool reverse = false )
{
  json value = std::string( 4096, 'x' );
  for( unsigned i = 0; i < depth; ++i )
  {
    json next = object ? json::object() : json::array();
    if( object )
    {
      if( branched ) next[ reverse ? "z" : "a" ] = std::string( 32, 's' );
      next[ reverse ? "a" : "z" ] = std::move( value );
    }
    else
    {
      if( branched && !reverse ) next.push_back( std::string( 32, 's' ) );
      next.push_back( std::move( value ) );
      if( branched && reverse ) next.push_back( std::string( 32, 's' ) );
    }
    value = std::move( next );
  }
  return value;
}

json wide()
{
  json value = json::object();
  for( unsigned i = 0; i < 2048; ++i )
    value[ std::to_string( i ) ] = json::array( { std::string( 128, 'w' ),
      json::object( { { "id", i }, { "binary", json::binary( { 0, 1, 255 } ) } } ) } );
  return value;
}

template< typename Factory >
void saturated_cleanup( const char* name, Factory&& factory )
{
  OwnedMemoryBudget budget( 128 * 1024 * 1024 );
  OwnedMemoryBudget::Scope scope( budget, false );
  const auto bootstrap = budget.snapshot().accounted_bytes;
  auto value = factory();
  const auto held = budget.snapshot();
  auto full = budget.reserve( held.limit_bytes - held.accounted_bytes );
  const auto before_attempts = fault::attempted;
  fault::fail_allocations = true;
  clear_json_without_allocation( value );
  clear_json_without_allocation( value ); // Idempotent, even while saturated.
  fault::fail_allocations = false;
  require( value.is_null(), "cleanup did not empty the exclusively owned root" );
  require( fault::attempted == before_attempts, "cleanup attempted dynamic allocation" );
  full.reset();
  require( budget.snapshot().accounted_bytes == bootstrap &&
           budget.snapshot().allocator_requested_bytes == 0,
           "cleanup retained an allocation or released another owner's credit" );
  std::cout << "JSON_CLEANUP shape=" << name << " allocations=0 released="
            << held.accounted_bytes - bootstrap << '\n';
}

json guarded_return()
{
  json response = { { "id", 71 }, { "result", { { "logs", { "complete" } } } } };
  JsonCleanup cleanup( response );
  return std::move( response ); // Deliberately avoid an NRVO alias to the guard.
}

void guard_return_and_unwind()
{
  auto response = guarded_return();
  require( response[ "id" ] == 71 && response[ "result" ][ "logs" ][ 0 ] == "complete",
           "cleanup guard erased the returned response" );
  auto copy = response;
  clear_json_without_allocation( response );
  require( copy[ "result" ][ "logs" ][ 0 ] == "complete", "cleanup changed another owner" );
  clear_json_without_allocation( copy );
  const auto before = fault::attempted;
  bool caught = false;
  try
  {
    auto value = nested( 128, true, true );
    JsonCleanup cleanup( value );
    fault::fail_allocations = true;
    throw std::bad_alloc();
  }
  catch( const std::bad_alloc& ) { caught = true; }
  fault::fail_allocations = false;
  require( caught && before == fault::attempted,
           "exception cleanup attempted allocation or lost the original exception" );
}

void serialization_refusal_cleanup()
{
  const auto expected = guarded_return().dump();
  require( dump_owned_json( guarded_return() ) == expected,
           "guarded serialization changed response bytes" );
  for( bool physical_fault : { false, true } )
  {
    OwnedMemoryBudget budget( 1024 * 1024 );
    OwnedMemoryBudget::Scope scope( budget, false );
    const auto bootstrap = budget.snapshot().accounted_bytes;
    auto value = guarded_return();
    auto full = budget.reserve( budget.snapshot().limit_bytes - budget.snapshot().accounted_bytes );
    const auto before_attempts = fault::attempted;
    const auto before_refusals = budget.snapshot().refusals;
    bool rejected = false;
    fault::fail_allocations = physical_fault;
    try { (void)dump_owned_json( std::move( value ) ); }
    catch( const koinos::node::LocalCapacityExceeded& ) { rejected = !physical_fault; }
    catch( const std::bad_alloc& ) { rejected = physical_fault; }
    fault::fail_allocations = false;
    require( rejected && value.is_null(), "serialization refusal lost classification or ownership" );
    require( physical_fault ? fault::attempted == before_attempts + 1 :
             budget.snapshot().refusals == before_refusals + 1,
             "serialization cleanup attempted another allocation at full capacity" );
    full.reset();
    require( budget.snapshot().accounted_bytes == bootstrap &&
             !budget.snapshot().allocator_requested_bytes,
             "serialization failure retained response credits" );
  }
}
} // namespace

int main( int argc, char** argv )
{
  std::set_terminate( [] { std::_Exit( 86 ); } );
  if( argc == 2 && std::string( argv[ 1 ] ) == "--default-destructor-fault" )
  {
    const auto before = fault::attempted;
    { json value = { { "params", { { "broadcast", true } } } };
      fault::fail_allocations = true; }
#ifdef TELENO_NLOHMANN_ALLOCATION_FREE_DESTROY
    fault::fail_allocations = false;
    require( fault::attempted == before, "backported default destructor attempted allocation" );
    return 0;
#else
    std::_Exit( 87 ); // Known-fault control must reach terminate, not this line.
#endif
  }
  if( argc == 2 && std::string( argv[ 1 ] ) == "--default-destructor-clean" )
  {
    { json value = { { "params", { { "broadcast", true } } } }; }
    return 0;
  }
  try
  {
    saturated_cleanup( "null", [] { return json(); } );
    saturated_cleanup( "scalar", [] { return json( std::string( 4096, 's' ) ); } );
    saturated_cleanup( "empty-array", [] { return json::array(); } );
    saturated_cleanup( "empty-object", [] { return json::object(); } );
    saturated_cleanup( "wide-mixed", wide );
    saturated_cleanup( "deep-unary-array", [] { return nested( 8192, false, false ); } );
    saturated_cleanup( "deep-unary-object", [] { return nested( 8192, false, true ); } );
    saturated_cleanup( "deep-branched-array", [] { return nested( 384, true, false ); } );
    saturated_cleanup( "deep-branched-object", [] { return nested( 384, true, true ); } );
    saturated_cleanup( "deep-reverse-branched-array", [] { return nested( 384, true, false, true ); } );
    saturated_cleanup( "deep-reverse-branched-object", [] { return nested( 384, true, true, true ); } );
    guard_return_and_unwind();
    serialization_refusal_cleanup();
    std::cout << "JSON allocation-free cleanup and unwinding passed\n";
    return 0;
  }
  catch( const std::exception& error )
  {
    fault::fail_allocations = false;
    std::cerr << error.what() << '\n';
    return 1;
  }
}
