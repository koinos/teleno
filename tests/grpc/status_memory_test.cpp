#include "grpc_server/mempool_output_state.hpp"
#include "core/owned_allocation.hpp"

#include <cstddef>
#include <iostream>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using koinos::node::OwnedMemoryBudget;
using koinos::node::grpc_server::detail::MempoolOutputState;

namespace status_fault {
thread_local std::size_t copy_bytes = 0;
thread_local unsigned remaining = 0, failures = 0, skip = 0;
}

void* operator new( std::size_t bytes )
{
  if( status_fault::remaining &&
      ( !status_fault::copy_bytes || bytes == status_fault::copy_bytes ) )
  {
    if( status_fault::skip ) --status_fault::skip;
    else
    {
      --status_fault::remaining;
      ++status_fault::failures;
      throw std::bad_alloc();
    }
  }
  return koinos::node::owned_allocation::allocate( bytes );
}

namespace {
void require( bool value, const char* message )
{
  if( !value ) throw std::runtime_error( message );
}

void refusal()
{
  OwnedMemoryBudget budget( 1024 * 1024 );
  const auto bootstrap = budget.snapshot().accounted_bytes;
  auto occupied = budget.reserve( budget.snapshot().limit_bytes - bootstrap );
  const auto full = budget.snapshot().accounted_bytes;
  std::optional< grpc::Status > status;
  uint64_t held = 0, after_state = 0;
  {
    MempoolOutputState state( budget );
    status.emplace( state.prepare_status( grpc::StatusCode::INTERNAL, "native failure" ) );
    const auto snapshot = budget.snapshot();
    held = snapshot.accounted_bytes;
    require( status->error_code() == grpc::StatusCode::RESOURCE_EXHAUSTED,
             "Full ordinary quota must return a local resource refusal" );
    require( held >= full + 4096, "Fixed refusal SDK workspace is unaccounted" );
    require( snapshot.allocator_requested_bytes >= status->error_message().capacity() + 1,
             "Refusal string allocation is unaccounted" );
  }
  after_state = budget.snapshot().accounted_bytes;
  require( after_state > full, "Caller-held refusal lost its last-owner credit" );
  status.reset();
  require( budget.snapshot().accounted_bytes == full, "Refusal final-owner credit leaked" );
  occupied.reset();
  {
    MempoolOutputState state( budget );
    const std::string message( 256, 'e' );
    const auto healthy = state.prepare_status( grpc::StatusCode::INTERNAL, message );
    require( healthy.error_code() == grpc::StatusCode::INTERNAL && healthy.error_message() == message,
             "Refusal prevented a later healthy retry" );
  }
  require( budget.snapshot().accounted_bytes == bootstrap, "Retry left retained credits" );
  std::cout << "GRPC_STATUS_REFUSAL full=" << full << " held=" << held
            << " after_state=" << after_state << " final=" << bootstrap << '\n';
}

void encoding()
{
  OwnedMemoryBudget budget( 64 * 1024 * 1024 );
  const auto bootstrap = budget.snapshot().accounted_bytes;
  uint64_t held = 0, minimum = 0;
  {
    MempoolOutputState state( budget );
    // '%' must be percent-escaped to three wire bytes by gRPC. Raw SDK and
    // encoded buffers can overlap the caller-visible status string. This
    // lower bound is independent of the selected reservation multiplier.
    const std::string message( 16384, '%' );
    const auto status = state.prepare_status( grpc::StatusCode::INTERNAL, message );
    require( status.error_code() == grpc::StatusCode::INTERNAL && status.error_message() == message,
             "Error bytes changed during status preparation" );
    minimum = 4 * message.size() + status.error_message().capacity() + 1;
    const auto snapshot = budget.snapshot();
    held = snapshot.accounted_bytes - bootstrap;
    require( held >= minimum, "Overlapping percent-encoded error workspace is under-reserved" );
    require( snapshot.allocator_requested_bytes >= status.error_message().capacity() + 1,
             "Application status fields have no lifetime allocation ownership" );
    state.release_message();
    require( budget.snapshot().accounted_bytes > bootstrap,
             "Releasing SDK workspace released a still-live status string" );
  }
  require( budget.snapshot().accounted_bytes == bootstrap, "Encoded error final credits leaked" );
  std::cout << "GRPC_STATUS_ENCODING held=" << held << " minimum=" << minimum << '\n';
}

void copy_failure()
{
  OwnedMemoryBudget budget( 64 * 1024 * 1024 );
  const auto bootstrap = budget.snapshot().accounted_bytes;
  const std::string message( 16384, 'e' );
  status_fault::failures = 0;
  for( unsigned copy = 0; copy < 2; ++copy )
  {
    MempoolOutputState state( budget );
    status_fault::copy_bytes = message.size() + 1;
    status_fault::remaining = 1;
    status_fault::skip = copy;
    const auto refused = state.prepare_status( grpc::StatusCode::INTERNAL, message );
    status_fault::remaining = 0;
    require( status_fault::failures == copy + 1 && refused.error_code() == grpc::StatusCode::RESOURCE_EXHAUSTED,
             "One-shot status-copy failure escaped its local resource refusal" );
    const auto healthy = state.prepare_status( grpc::StatusCode::INTERNAL, message );
    require( healthy.error_code() == grpc::StatusCode::INTERNAL && healthy.error_message() == message,
             "Status-copy failure prevented a healthy retry" );
  }
  require( budget.snapshot().accounted_bytes == bootstrap, "One-shot failure/retry leaked credits" );
  {
    MempoolOutputState state( budget );
    status_fault::copy_bytes = 0;
    status_fault::remaining = 2;
    bool exposed = false;
    try { (void)state.prepare_status( grpc::StatusCode::INTERNAL, message ); }
    catch( const std::bad_alloc& ) { exposed = true; }
    status_fault::remaining = 0;
    require( exposed && status_fault::failures == 4, "Repeated physical failure was hidden or retried indefinitely" );
    const auto healthy = state.prepare_status( grpc::StatusCode::INTERNAL, message );
    require( healthy.error_code() == grpc::StatusCode::INTERNAL, "Repeated-failure cleanup prevented retry" );
  }
  require( budget.snapshot().accounted_bytes == bootstrap, "Repeated failure leaked credits" );
  std::cout << "GRPC_STATUS_COPY_FAILURE injected=" << status_fault::failures << " final=" << bootstrap << '\n';
}

void argument_failure()
{
  OwnedMemoryBudget budget( 64 * 1024 * 1024 );
  const auto bootstrap = budget.snapshot().accounted_bytes;
  const std::string message( 16384, 'e' );
  status_fault::failures = 0;
  {
    MempoolOutputState state( budget );
    status_fault::copy_bytes = message.size() + 1;
    status_fault::remaining = 1;
    // Match invoke_service's exception::what() call. Argument materialization
    // must happen inside the same admitted/caught status-preparation boundary.
    const auto refused = state.prepare_status( grpc::StatusCode::INTERNAL, message.c_str() );
    status_fault::remaining = 0;
    require( status_fault::failures == 1 && refused.error_code() == grpc::StatusCode::RESOURCE_EXHAUSTED,
             "C-string argument copy escaped status admission/failure handling" );
    const auto healthy = state.prepare_status( grpc::StatusCode::INTERNAL, message.c_str() );
    require( healthy.error_code() == grpc::StatusCode::INTERNAL && healthy.error_message() == message,
             "C-string argument failure prevented a healthy retry" );
  }
  require( budget.snapshot().accounted_bytes == bootstrap, "Argument failure/retry leaked credits" );
  std::cout << "GRPC_STATUS_ARGUMENT_FAILURE injected=" << status_fault::failures
            << " final=" << bootstrap << '\n';
}

void bounded_refusal()
{
  OwnedMemoryBudget budget( 1024 * 1024, false, 16384 );
  const auto bootstrap = budget.snapshot().accounted_bytes;
  auto occupied = budget.reserve( budget.snapshot().limit_bytes - bootstrap );
  std::vector< std::optional< grpc::Status > > statuses( 64 );
  unsigned detailed = 0, code_only = 0;
  for( auto& status: statuses )
  {
    MempoolOutputState state( budget );
    status.emplace( state.prepare_status( grpc::StatusCode::INTERNAL, "native failure" ) );
    require( status->error_code() == grpc::StatusCode::RESOURCE_EXHAUSTED,
             "Bounded refusal changed local-overload classification" );
    if( status->error_message().empty() ) ++code_only; else ++detailed;
    require( budget.snapshot().refusal_accounted_bytes <= 16384,
             "Held statuses exceeded their shared refusal allowance" );
  }
  const auto held = budget.snapshot();
  require( detailed && code_only && held.refusal_capacity_refusals &&
           held.peak_refusal_accounted_bytes <= 16384,
           "Concurrent caller statuses did not exercise the refusal boundary" );
  statuses.clear();
  require( budget.snapshot().refusal_accounted_bytes == 0 &&
           budget.snapshot().accounted_bytes == budget.snapshot().limit_bytes,
           "Bounded caller statuses leaked final-owner credits" );
  {
    OwnedMemoryBudget::Lease refusal_occupied;
    {
      OwnedMemoryBudget::Scope scope( budget, OwnedMemoryBudget::WorkClass::refusal );
      refusal_occupied = budget.reserve( 16384 );
    }
    MempoolOutputState state( budget );
    status_fault::copy_bytes = 0; status_fault::skip = 0; status_fault::remaining = 1;
    const auto empty = state.prepare_status( grpc::StatusCode::INTERNAL, "native failure" );
    const bool allocation_free = status_fault::remaining == 1;
    status_fault::remaining = 0;
    require( allocation_free && empty.error_code() == grpc::StatusCode::RESOURCE_EXHAUSTED &&
             empty.error_message().empty(), "Exhausted refusal allowance allocated an application payload" );
  }
  occupied.reset();
  {
    MempoolOutputState state( budget );
    const auto retry = state.prepare_status( grpc::StatusCode::INTERNAL, "healthy retry" );
    require( retry.error_code() == grpc::StatusCode::INTERNAL,
             "Exhausted refusal allowance prevented a healthy retry" );
  }
  require( budget.snapshot().accounted_bytes == bootstrap && budget.snapshot().refusal_accounted_bytes == 0,
           "Refusal allowance did not return to bootstrap" );
  std::cout << "GRPC_STATUS_BOUNDED detailed=" << detailed << " code_only=" << code_only
            << " peak=" << held.peak_refusal_accounted_bytes << " limit=" << held.refusal_limit_bytes << '\n';
}
}

int main( int argc, char** argv )
{
  try
  {
    require( argc == 2, "Expected refusal, encoding, copy-failure, argument-failure or bounded-refusal" );
    const std::string mode( argv[ 1 ] );
    if( mode == "refusal" ) refusal();
    else if( mode == "encoding" ) encoding();
    else if( mode == "copy-failure" ) copy_failure();
    else if( mode == "argument-failure" ) argument_failure();
    else if( mode == "bounded-refusal" ) bounded_refusal();
    else throw std::runtime_error( "Unknown gRPC status-memory case" );
    std::cout << "GRPC_STATUS_MEMORY_COMPLETED mode=" << mode << std::endl;
    return 0;
  }
  catch( const std::exception& error )
  {
    status_fault::remaining = 0;
    std::cerr << error.what() << std::endl;
    return 1;
  }
}
