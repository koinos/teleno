#pragma once

#include "core/owned_memory_budget.hpp"

#include <koinos/log.hpp>
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/server_interceptor.h>

#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

// Internal per-RPC ownership shared by the handler and completion interceptor.
namespace koinos::node::grpc_server::detail {

class MempoolOutputState
{
public:
  explicit MempoolOutputState( OwnedMemoryBudget budget, const char* refusal = nullptr )
    : _budget( std::move( budget ) ), _control_refusal( refusal ) {}

  OwnedMemoryBudget memory_budget() const noexcept { return _budget; }
  bool admitted() const noexcept { return !_control_refusal; }

  grpc::Status control_refusal_status()
  {
    if( admitted() ) throw std::logic_error( "No gRPC control refusal is pending" );
    return fixed_refusal( _control_refusal );
  }

  template< typename Message > void prepare_message( const Message& message )
  {
    const auto wire_bytes = static_cast< uint64_t >( message.ByteSizeLong() );
    auto lease = _budget.reserve( OwnedMemoryBudget::native_message_workspace( wire_bytes ) );
    std::lock_guard lock( _mutex );
    _lease = std::move( lease );
    _expected_wire_bytes = wire_bytes;
    _message_prepared = true;
  }

  grpc::Status prepare_status( grpc::StatusCode code, std::string_view message )
  {
    {
      std::lock_guard lock( _mutex );
      _lease.reset();
      _expected_wire_bytes = 0;
      _message_prepared = false;
    }
    try
    {
      // grpc-message percent encoding can expand each input byte threefold.
      // Cover overlapping raw/encoded SDK buffers conservatively, separately
      // from captured C++ status-field allocations. Retain this envelope until
      // RPC completion; it is policy workspace, not a heap-size equality.
      OwnedMemoryBudget::Scope status_fields( _budget, false );
      auto lease = _budget.reserve( OwnedMemoryBudget::native_message_workspace( message.size() ) );
      {
        std::lock_guard lock( _mutex );
        _lease = std::move( lease );
        _expected_wire_bytes = message.size();
        _message_prepared = false;
      }
      // exception::what() is borrowed up to this boundary. Materialize its
      // SDK-required std::string only after admission, inside this catch scope.
      return grpc::Status( code, std::string( message ) );
    }
    catch( const LocalCapacityExceeded& error )
    {
      return fixed_refusal( error.what() );
    }
    catch( const std::bad_alloc& )
    {
      // One bounded fallback preserves a useful overload result after a
      // one-time physical copy failure. A repeated failure still propagates.
      return fixed_refusal( "Local M1 RPC status allocation failed" );
    }
  }

  void serialize_message( grpc::experimental::InterceptorBatchMethods* methods )
  {
    std::lock_guard lock( _mutex );
    if( !_message_prepared ) return;
    // The explicit lease was admitted before native transaction publication
    // (or before returning a read result). It covers the SDK's malloc-backed
    // serialized ByteBuffer, which global C++ allocation capture cannot own.
    OwnedMemoryBudget::AllocationBypass sdk_serialization;
    auto* bytes = methods->GetSerializedSendMessage();
    if( !bytes || bytes->Length() != _expected_wire_bytes )
      LOG( error ) << "[grpc] Serialized response length changed after M1 admission";
  }

  void release_message() noexcept
  {
    std::lock_guard lock( _mutex );
    _lease.reset();
    _expected_wire_bytes = 0;
    _message_prepared = false;
  }

private:
  grpc::Status fixed_refusal( const char* message )
  {
    // A separate finite allowance admits the minimum refusal at a full
    // ordinary quota, including any status string held beyond this state.
    // If that allowance is exhausted, send only the status code: the empty
    // C++ status has no application payload allocation. SDK transport control
    // remains a separate R1 consumer; do not pretend it is covered here.
    try
    {
      OwnedMemoryBudget::Scope control( _budget, OwnedMemoryBudget::WorkClass::refusal );
      auto lease = _budget.reserve( 4096 );
      {
        std::lock_guard lock( _mutex );
        _lease = std::move( lease );
        _message_prepared = false;
        _expected_wire_bytes = 0;
      }
      return grpc::Status( grpc::StatusCode::RESOURCE_EXHAUSTED, message );
    }
    catch( const LocalCapacityExceeded& )
    {
      release_message();
      return grpc::Status( grpc::StatusCode::RESOURCE_EXHAUSTED, {} );
    }
  }

  OwnedMemoryBudget _budget;
  const char* _control_refusal;
  std::mutex _mutex;
  OwnedMemoryBudget::Lease _lease;
  uint64_t _expected_wire_bytes = 0;
  bool _message_prepared = false;
};

} // namespace koinos::node::grpc_server::detail
