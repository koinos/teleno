#pragma once
#include "core/local_capacity.hpp"
#include "core/owned_memory_budget.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <stdexcept>
#include <utility>

namespace koinos::mempool {

struct pending_record_limits
{
  uint64_t max_owners = 0, max_capacity_bytes = 0;
};
struct pending_record_snapshot
{
  bool enabled = false;
  pending_record_limits limits;
  uint64_t owners = 0, capacity_bytes = 0;
  uint64_t peak_owners = 0, peak_capacity_bytes = 0, refusals = 0;
};

// Live immutable serialized records, including staged/ancestor/caller owners.
// Not all mempool heap: indexes/keys/RC/nonces/containers and encoding temporaries
// are additional. String capacity includes inline bytes, not allocator overhead.
class pending_record_budget
{
  struct state
  {
    explicit state( pending_record_limits limits ) { usage.enabled = true; usage.limits = limits; }
    std::mutex mutex;
    pending_record_snapshot usage;
  };
  static void refuse( pending_record_snapshot& u )
  {
    if( u.refusals != std::numeric_limits< uint64_t >::max() ) ++u.refusals;
    throw node::LocalCapacityExceeded( node::LocalCapacityReason::pending_record );
  }
  struct lease
  {
    lease( std::shared_ptr< state > s, uint64_t bytes ) : owner( std::move( s ) ), bytes( bytes ) {}
    ~lease()
    {
      if( !active ) return;
      std::lock_guard lock( owner->mutex );
      --owner->usage.owners; owner->usage.capacity_bytes -= bytes;
    }
    void grow( uint64_t capacity )
    {
      std::lock_guard lock( owner->mutex );
      auto& u = owner->usage;
      if( capacity < bytes ) throw std::logic_error( "pending record capacity smaller than encoding" );
      const auto extra = capacity - bytes, max = std::numeric_limits< uint64_t >::max();
      if( extra > max - u.capacity_bytes ||
          ( u.limits.max_capacity_bytes && extra > u.limits.max_capacity_bytes - u.capacity_bytes ) ) refuse( u );
      u.capacity_bytes += extra; bytes = capacity;
      u.peak_capacity_bytes = std::max( u.peak_capacity_bytes, u.capacity_bytes );
    }
    std::shared_ptr< state > owner;
    uint64_t bytes;
    bool active = false;
  };
  struct stored
  {
    stored( std::unique_ptr< lease > credit, std::string bytes ) : credit( std::move( credit ) ), value( std::move( bytes ) ) {}
    // Reverse destruction frees the string before returning credits. The const
    // field cannot acquire a mutable alias through this immutable factory.
    std::unique_ptr< lease > credit;
    const std::string value;
  };
  struct aggregate_stored
  {
    aggregate_stored( node::OwnedMemoryBudget::Lease memory, std::unique_ptr< lease > credit, std::string bytes )
      : memory( std::move( memory ) ), record( std::move( credit ), std::move( bytes ) ) {}
    node::OwnedMemoryBudget::Lease memory;
    const stored record;
  };
public:
  explicit pending_record_budget( pending_record_limits limits = {}, node::OwnedMemoryBudget memory = {} )
    : _memory( std::move( memory ) )
  {
    if( limits.max_owners || limits.max_capacity_bytes )
    {
      node::OwnedMemoryBudget::Scope scope( _memory );
      _state = node::make_owned_shared< state >( limits );
    }
  }
  pending_record_budget( const pending_record_budget& ) = delete;
  pending_record_budget& operator=( const pending_record_budget& ) = delete;

  template< typename Encode >
  std::shared_ptr< const std::string > encode( uint64_t wire_size, Encode&& encode )
  {
    node::OwnedMemoryBudget::Scope memory_scope( _memory );
    auto memory = _memory.enabled() ? _memory.reserve( node::OwnedMemoryBudget::checked_add(
      node::OwnedMemoryBudget::string_reservation( wire_size ), sizeof( lease ) + 128 ) ) : node::OwnedMemoryBudget::Lease{};
    auto credit = reserve( wire_size );
    auto bytes = encode();
    if( bytes.size() != wire_size ) throw std::logic_error( "pending record encoding size mismatch" );
    if( _memory.enabled() && bytes.capacity() + 1 > node::OwnedMemoryBudget::string_reservation( wire_size ) )
      throw std::logic_error( "pending record encoder exceeded reserved native string workspace" );
    if( _memory.enabled() )
    {
      if( credit ) credit->grow( bytes.capacity() );
      auto owner = node::make_owned_shared< const aggregate_stored >(
        std::move( memory ), std::move( credit ), std::move( bytes ) );
      return { owner, &owner->record.value };
    }
    if( !_state ) return std::make_shared< const std::string >( std::move( bytes ) );
    credit->grow( bytes.capacity() );
    auto owner = std::make_shared< const stored >( std::move( credit ), std::move( bytes ) );
    return { owner, &owner->value };
  }
  pending_record_snapshot snapshot() const
  {
    if( !_state ) return {};
    std::lock_guard lock( _state->mutex ); return _state->usage;
  }
private:
  std::unique_ptr< lease > reserve( uint64_t wire_size )
  {
    if( !_state ) return {};
    std::lock_guard lock( _state->mutex );
    auto& u = _state->usage;
    const auto max = std::numeric_limits< uint64_t >::max();
    if( u.owners == max || wire_size > max - u.capacity_bytes ||
        ( u.limits.max_owners && u.owners >= u.limits.max_owners ) ||
        ( u.limits.max_capacity_bytes && wire_size > u.limits.max_capacity_bytes - u.capacity_bytes ) ) refuse( u );
    auto credit = std::make_unique< lease >( _state, wire_size );
    ++u.owners; u.capacity_bytes += wire_size;
    u.peak_owners = std::max( u.peak_owners, u.owners );
    u.peak_capacity_bytes = std::max( u.peak_capacity_bytes, u.capacity_bytes );
    credit->active = true;
    return credit;
  }
  std::shared_ptr< state > _state;
  node::OwnedMemoryBudget _memory;
};

} // namespace koinos::mempool
