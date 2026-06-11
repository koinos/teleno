#include <core/service_registry.hpp>

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using koinos::node::ServiceRegistry;

namespace
{

void assert_events( const std::vector< std::string >& actual, const std::vector< std::string >& expected )
{
  assert( actual == expected );
}

void test_start_failure_stops_started_components()
{
  ServiceRegistry registry;
  std::vector< std::string > events;

  registry.add(
    "first",
    [&]() { events.push_back( "start:first" ); },
    [&]() { events.push_back( "stop:first" ); } );
  registry.add(
    "second",
    [&]() {
      events.push_back( "start:second" );
      throw std::runtime_error( "start failed" );
    },
    [&]() { events.push_back( "stop:second" ); } );

  bool threw = false;
  try
  {
    registry.start_all();
  }
  catch( const std::runtime_error& )
  {
    threw = true;
  }

  assert( threw );
  assert_events( events, { "start:first", "start:second", "stop:first" } );
}

void test_unknown_start_failure_stops_started_components()
{
  ServiceRegistry registry;
  std::vector< std::string > events;

  registry.add(
    "first",
    [&]() { events.push_back( "start:first" ); },
    [&]() { events.push_back( "stop:first" ); } );
  registry.add(
    "second",
    [&]() {
      events.push_back( "start:second" );
      throw 1;
    },
    [&]() { events.push_back( "stop:second" ); } );

  bool threw = false;
  try
  {
    registry.start_all();
  }
  catch( ... )
  {
    threw = true;
  }

  assert( threw );
  assert_events( events, { "start:first", "start:second", "stop:first" } );
}

void test_stop_failure_does_not_block_remaining_components()
{
  ServiceRegistry registry;
  std::vector< std::string > events;

  registry.add(
    "first",
    [&]() { events.push_back( "start:first" ); },
    [&]() { events.push_back( "stop:first" ); } );
  registry.add(
    "second",
    [&]() { events.push_back( "start:second" ); },
    [&]() {
      events.push_back( "stop:second" );
      throw std::runtime_error( "stop failed" );
    } );

  registry.start_all();
  registry.stop_all();

  assert_events( events, { "start:first", "start:second", "stop:second", "stop:first" } );
}

void test_unknown_stop_failure_does_not_block_remaining_components()
{
  ServiceRegistry registry;
  std::vector< std::string > events;

  registry.add(
    "first",
    [&]() { events.push_back( "start:first" ); },
    [&]() { events.push_back( "stop:first" ); } );
  registry.add(
    "second",
    [&]() { events.push_back( "start:second" ); },
    [&]() {
      events.push_back( "stop:second" );
      throw 1;
    } );

  registry.start_all();
  registry.stop_all();

  assert_events( events, { "start:first", "start:second", "stop:second", "stop:first" } );
}

} // namespace

int main()
{
  test_start_failure_stops_started_components();
  test_unknown_start_failure_stops_started_components();
  test_stop_failure_does_not_block_remaining_components();
  test_unknown_stop_failure_does_not_block_remaining_components();
  return 0;
}
