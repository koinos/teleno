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

void test_staged_start_defers_external_components_and_does_not_restart_core()
{
  ServiceRegistry registry;
  std::vector< std::string > events;

  for( const auto* name: { "block_store", "chain", "p2p", "jsonrpc" } )
  {
    registry.add(
      name,
      [&, name]() { events.push_back( std::string( "start:" ) + name ); },
      [&, name]() { events.push_back( std::string( "stop:" ) + name ); } );
  }

  registry.start( "block_store" );
  registry.start( "chain" );
  registry.start( "chain" );
  assert_events( events, { "start:block_store", "start:chain" } );

  registry.start_all();
  assert_events(
    events,
    { "start:block_store", "start:chain", "start:p2p", "start:jsonrpc" } );

  registry.stop_all();
  assert_events(
    events,
    { "start:block_store",
      "start:chain",
      "start:p2p",
      "start:jsonrpc",
      "stop:jsonrpc",
      "stop:p2p",
      "stop:chain",
      "stop:block_store" } );
}

void test_staged_start_rejects_unknown_component()
{
  ServiceRegistry registry;
  bool threw = false;
  try
  {
    registry.start( "missing" );
  }
  catch( const std::invalid_argument& )
  {
    threw = true;
  }
  assert( threw );
}

void test_staged_external_failure_stops_started_core()
{
  ServiceRegistry registry;
  std::vector< std::string > events;

  registry.add(
    "block_store",
    [&]() { events.push_back( "start:block_store" ); },
    [&]() { events.push_back( "stop:block_store" ); } );
  registry.add(
    "chain",
    [&]() { events.push_back( "start:chain" ); },
    [&]() { events.push_back( "stop:chain" ); } );
  registry.add(
    "p2p",
    [&]() {
      events.push_back( "start:p2p" );
      throw std::runtime_error( "external start failed" );
    },
    [&]() { events.push_back( "stop:p2p" ); } );

  registry.start( "block_store" );
  registry.start( "chain" );

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
  assert_events(
    events,
    { "start:block_store", "start:chain", "start:p2p", "stop:chain", "stop:block_store" } );
}

} // namespace

int main()
{
  test_start_failure_stops_started_components();
  test_unknown_start_failure_stops_started_components();
  test_stop_failure_does_not_block_remaining_components();
  test_unknown_stop_failure_does_not_block_remaining_components();
  test_staged_start_defers_external_components_and_does_not_restart_core();
  test_staged_start_rejects_unknown_component();
  test_staged_external_failure_stops_started_core();
  return 0;
}
