#pragma once

#include "types.hpp"
#include "local_transport_error.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace koinos::node::p2p::detail {

// This ledger follows native dial owners, including preparation, queued work
// and retained SDK callbacks. Peer string capacities are not SDK/heap/RSS bytes.
class DialBudget
{
  struct State
  {
    std::mutex mutex;
    size_t jobs = 0, bytes = 0, peak_jobs = 0, peak_bytes = 0;
    const size_t max_jobs, max_bytes;
    uint64_t refusals = 0, setup_exceptions = 0, completion_exceptions = 0, retention_bypasses = 0;
    State( size_t count, size_t bytes ) : max_jobs(count), max_bytes(bytes) {}
  };
  class Credit
  {
    std::shared_ptr<State> _state;
    size_t _bytes = 0;
  public:
    Credit( std::shared_ptr<State> state, size_t bytes ) : _state(std::move(state)), _bytes(bytes) {}
    Credit( const Credit& ) = delete;
    Credit& operator=( const Credit& ) = delete;
    Credit( Credit&& other ) noexcept : _state(std::move(other._state)), _bytes(other._bytes) {}
    ~Credit()
    {
      if( !_state ) return;
      std::lock_guard lock(_state->mutex);--_state->jobs;_state->bytes -= _bytes;
    }
    void resize( size_t bytes )
    {
      std::lock_guard lock(_state->mutex);
      if( bytes > _bytes && bytes - _bytes > _state->max_bytes - _state->bytes )
      { ++_state->refusals;throw LocalTransportUnavailable(LocalTransportUnavailable::Reason::dial_capacity); }
      _state->bytes -= _bytes;_state->bytes += bytes;_bytes = bytes;
      _state->peak_bytes = std::max(_state->peak_bytes, _state->bytes);
    }
  };
public:
  struct Stats
  {
    size_t jobs, peer_capacity_bytes, peak_jobs, peak_peer_capacity_bytes, max_jobs, max_peer_capacity_bytes;
    uint64_t capacity_refusals, setup_exceptions, completion_exceptions, retention_bypasses;
  };
  class Lease
  {
    friend class DialRegistry;
    Credit _credit;
    std::atomic<bool> _cancelled{false};
    uint64_t _ticket = 0;
  public:
    const PeerID peer;
    Lease( Credit credit, const PeerID& input ) : _credit(std::move(credit)), peer(input)
    {
      if( peer.id.capacity() > std::numeric_limits<size_t>::max() - peer.address.capacity() )
        throw LocalTransportUnavailable(LocalTransportUnavailable::Reason::dial_capacity);
      _credit.resize(peer.id.capacity() + peer.address.capacity());
    }
    bool cancelled() const noexcept { return _cancelled.load(std::memory_order_acquire); }
  };
  explicit DialBudget( size_t jobs = 32, size_t bytes = 1024 * 1024 )
    : _state(std::make_shared<State>(jobs, bytes))
  {
    if( !jobs || !bytes ) throw std::invalid_argument("Native dial count and peer-capacity limits must be positive");
  }
  std::shared_ptr<Lease> reserve( const PeerID& peer )
  {
    const auto id = std::max(peer.id.size(), std::string().capacity());
    const auto address = std::max(peer.address.size(), std::string().capacity());
    const bool overflow = id > std::numeric_limits<size_t>::max() - address;
    const auto bytes = overflow ? 0 : id + address;
    {
      std::lock_guard lock(_state->mutex);
      if( overflow || _state->jobs >= _state->max_jobs || bytes > _state->max_bytes - _state->bytes )
      { ++_state->refusals;throw LocalTransportUnavailable(LocalTransportUnavailable::Reason::dial_capacity); }
      ++_state->jobs;_state->bytes += bytes;
      _state->peak_jobs = std::max(_state->peak_jobs, _state->jobs);
      _state->peak_bytes = std::max(_state->peak_bytes, _state->bytes);
    }
    Credit credit(_state, bytes);
    // If either the shared owner or a string fails, the reservation returns.
    return std::make_shared<Lease>(std::move(credit), peer);
  }
  enum class Event { setup_exception, completion_exception, retention_bypass };
  void note( Event event ) noexcept
  {
    std::lock_guard lock(_state->mutex);
    if( event == Event::setup_exception ) ++_state->setup_exceptions;
    else if( event == Event::completion_exception ) ++_state->completion_exceptions;
    else ++_state->retention_bypasses;
  }
  Stats stats() const
  {
    std::lock_guard lock(_state->mutex);
    return {_state->jobs, _state->bytes, _state->peak_jobs, _state->peak_bytes,
            _state->max_jobs, _state->max_bytes, _state->refusals,
            _state->setup_exceptions, _state->completion_exceptions, _state->retention_bypasses};
  }
private:
  std::shared_ptr<State> _state;
};

// Each generation owns its registry, while the budget spans restart generations.
// Publication and stop admission use the same mutex. The publisher must enqueue
// work without invoking callbacks inline; it must not call this registry itself.
class DialRegistry
{
public:
  using Lease = DialBudget::Lease;
  DialRegistry( std::shared_ptr<DialBudget> budget, std::shared_ptr<std::atomic<bool>> active )
    : _budget(std::move(budget)), _active(std::move(active))
  { if( !_budget || !_active ) throw std::invalid_argument("Dial registry needs a budget and lifecycle owner"); }

  std::shared_ptr<Lease> admit( const PeerID& peer )
  {
    std::lock_guard lock(_mutex);
    if( !active_locked() ) throw LocalTransportUnavailable();
    for( auto it = _calls.begin();it != _calls.end(); )
    {
      if( auto call = it->second.lock() )
      { if( call->peer.id == peer.id ) return {}; ++it; }
      else it = _calls.erase(it);
    }
    auto lease = _budget->reserve(peer);
    while( _calls.count(++_next_ticket) ) {}
    lease->_ticket = _next_ticket;
    _calls.emplace(lease->_ticket, lease); // Strong map insertion; failed publication owns no credit.
    return lease;
  }
  bool eligible( const std::shared_ptr<Lease>& lease ) const
  { std::lock_guard lock(_mutex);return active_locked() && matches_locked(lease) && !lease->cancelled(); }

  template<typename Publisher> void publish( const std::shared_ptr<Lease>& lease, Publisher&& publisher )
  {
    std::lock_guard lock(_mutex);
    if( !active_locked() || !matches_locked(lease) || lease->cancelled() ) throw LocalTransportUnavailable();
    try { std::forward<Publisher>(publisher)(); }
    catch( ... ) { retire_locked(lease);throw; }
  }
  // Control work shares the stop barrier, but has no dial credit/byte ceiling.
  template<typename Publisher> void publish_control( Publisher&& publisher )
  {
    std::lock_guard lock(_mutex);
    if( !active_locked() ) throw LocalTransportUnavailable();
    std::forward<Publisher>(publisher)();
  }
  void retire( const std::shared_ptr<Lease>& lease ) noexcept
  { std::lock_guard lock(_mutex);retire_locked(lease); }
  void cancel_peer( std::string_view peer ) noexcept
  {
    std::lock_guard lock(_mutex);
    for( auto it = _calls.begin();it != _calls.end(); )
    {
      auto lease = it->second.lock();
      if( !lease || lease->peer.id == peer )
      { if( lease ) lease->_cancelled.store(true, std::memory_order_release);it = _calls.erase(it); }
      else ++it;
    }
  }
  void stop_accepting() noexcept
  {
    std::lock_guard lock(_mutex);_accepting = false;
    for( const auto& [ticket, weak]: _calls )
      if( auto lease = weak.lock() ) lease->_cancelled.store(true, std::memory_order_release);
    _calls.clear(); // Lease credits stay with the last preparation/queue/SDK owner.
  }
private:
  bool active_locked() const noexcept
  { return _accepting && _active->load(std::memory_order_acquire); }
  bool matches_locked( const std::shared_ptr<Lease>& lease ) const noexcept
  {
    if( !lease ) return false;
    auto it = _calls.find(lease->_ticket);
    return it != _calls.end() && it->second.lock().get() == lease.get();
  }
  void retire_locked( const std::shared_ptr<Lease>& lease ) noexcept
  {
    if( !matches_locked(lease) ) return;
    lease->_cancelled.store(true, std::memory_order_release);
    _calls.erase(lease->_ticket);
  }
  std::shared_ptr<DialBudget> _budget;
  std::shared_ptr<std::atomic<bool>> _active;
  mutable std::mutex _mutex;
  bool _accepting = true;
  uint64_t _next_ticket = 0;
  std::map<uint64_t, std::weak_ptr<Lease>> _calls;
};

} // namespace koinos::node::p2p::detail
