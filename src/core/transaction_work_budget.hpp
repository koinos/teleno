#pragma once
#include "local_capacity.hpp"
#include "owned_memory_budget.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

namespace koinos::node {

struct TransactionWorkLimits
{
  uint64_t max_jobs = 0;
  uint64_t max_wire_bytes = 0;
};

struct TransactionWorkSnapshot
{
  bool enabled = false;
  TransactionWorkLimits limits;
  uint64_t jobs = 0, wire_bytes = 0;
  uint64_t peak_jobs = 0, peak_wire_bytes = 0, refusals = 0;
};

class TransactionWorkReservation
{
public:
  virtual ~TransactionWorkReservation() = default;
  virtual OwnedMemoryBudget memory_budget() const noexcept { return {}; }
};

// Bounds only simultaneous submit_transaction jobs and their serialized input
// bytes, not protobuf/VM/receipt/fork/transport memory or total heap/RSS.
// Zero disables an axis; both zero preserve the legacy allocation-free path.
class TransactionWorkBudget
{
  struct State
  {
    explicit State( TransactionWorkLimits limits ) { usage.enabled = true; usage.limits = limits; }
    std::mutex mutex;
    TransactionWorkSnapshot usage;
  };

  struct Lease final : TransactionWorkReservation
  {
    Lease( std::shared_ptr< State > owner, uint64_t bytes ) : owner( std::move( owner ) ), bytes( bytes ) {}
    ~Lease() override
    {
      if( !active ) return;
      std::lock_guard lock( owner->mutex );
      --owner->usage.jobs;
      owner->usage.wire_bytes -= bytes;
    }
    std::shared_ptr< State > owner;
    uint64_t bytes;
    bool active = false;
  };

public:
  explicit TransactionWorkBudget( TransactionWorkLimits limits = {}, OwnedMemoryBudget memory = {} )
  {
    if( limits.max_jobs || limits.max_wire_bytes )
    {
      OwnedMemoryBudget::Scope scope( memory );
      _state = make_owned_shared< State >( limits );
    }
  }
  TransactionWorkBudget( const TransactionWorkBudget& ) = delete;
  TransactionWorkBudget& operator=( const TransactionWorkBudget& ) = delete;

  bool enabled() const noexcept { return bool( _state ); }

  std::unique_ptr< TransactionWorkReservation > reserve( uint64_t wire_bytes )
  {
    if( !_state ) return {};
    std::lock_guard lock( _state->mutex );
    auto& u = _state->usage;
    const auto max = std::numeric_limits< uint64_t >::max();
    if( u.jobs == max || wire_bytes > max - u.wire_bytes
        || ( u.limits.max_jobs && u.jobs >= u.limits.max_jobs )
        || ( u.limits.max_wire_bytes && wire_bytes > u.limits.max_wire_bytes - u.wire_bytes ) )
    {
      if( u.refusals != max ) ++u.refusals;
      throw LocalCapacityExceeded();
    }

    // The only fallible allocation precedes accounting. Failure cannot leak
    // credits; no queue, retry or work starts before a successful reservation.
    auto lease = std::make_unique< Lease >( _state, wire_bytes );
    ++u.jobs;
    u.wire_bytes += wire_bytes;
    u.peak_jobs = std::max( u.peak_jobs, u.jobs );
    u.peak_wire_bytes = std::max( u.peak_wire_bytes, u.wire_bytes );
    lease->active = true;
    return lease;
  }

  TransactionWorkSnapshot snapshot() const
  {
    if( !_state ) return {};
    std::lock_guard lock( _state->mutex );
    return _state->usage;
  }

private:
  std::shared_ptr< State > _state;
};

} // namespace koinos::node
