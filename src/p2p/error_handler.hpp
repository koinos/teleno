#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include "types.hpp"

namespace koinos::node::p2p {

// A ban belongs to the first network component, as in the reference Go node.
// Ports and trailing peer IDs cannot split one address into unrelated scores.
// Normalize numeric IP spellings without DNS, heap allocation or a borrowed
// view into a moved temporary. Other native TCP/DNS address values stay exact.
class PeerErrorAddress
{
public:
  explicit PeerErrorAddress( std::string_view address ) noexcept : _view( address )
  {
    if( address.empty() || address.front() != '/' ) return;
    const auto protocol_end = address.substr( 0, 16 ).find( '/', 1 );
    if( protocol_end == std::string_view::npos ) return;
    const auto protocol = address.substr( 1, protocol_end - 1 );
    if( protocol != "ip4" && protocol != "ip6" && protocol != "dns"
      && protocol != "dns4" && protocol != "dns6" && protocol != "dnsaddr" ) return;
    const auto end = address.find( '/', protocol_end + 1 );
    _view = address.substr( 0, end );
    if( protocol != "ip4" && protocol != "ip6" ) return;
    const auto value = _view.substr( protocol_end + 1 );
    if( value.empty() || value.size() >= INET6_ADDRSTRLEN || value.find( '\0' ) != std::string_view::npos ) return;
    std::array< char, INET6_ADDRSTRLEN > text{};
    std::memcpy( text.data(), value.data(), value.size() );
    alignas(16) std::array< unsigned char, 16 > bytes{};
    const auto family = protocol == "ip4" ? AF_INET : AF_INET6;
    if( inet_pton( family, text.data(), bytes.data() ) != 1 ) return;
    std::array< char, INET6_ADDRSTRLEN > canonical{};
    if( !inet_ntop( family, bytes.data(), canonical.data(), canonical.size() ) ) return;
    constexpr size_t prefix_size = 5; // /ip4/ or /ip6/
    std::memcpy( _canonical.data(), _view.data(), prefix_size );
    const auto length = std::strlen( canonical.data() );
    std::memcpy( _canonical.data() + prefix_size, canonical.data(), length );
    _canonical_size = prefix_size + length;
  }
  std::string_view value() const noexcept
  { return _canonical_size ? std::string_view( _canonical.data(), _canonical_size ) : _view; }
private:
  std::string_view _view;
  std::array< char, INET6_ADDRSTRLEN + 5 > _canonical{};
  size_t _canonical_size = 0;
};

// Count and attributed retained key-capacity limits do not bound SDK, caller,
// pending connection or whole-node memory. Meaningful scores are never evicted
// to make room: new unknown addresses receive local admission backpressure.
class PeerErrorHandler
{
public:
  using Clock = std::chrono::steady_clock;
  using Now = std::function< Clock::time_point() >;
  static constexpr size_t sweep_work_limit = 16;
  enum class Admission { allowed, score_blocked, local_capacity };
  struct ErrorResult { bool disconnect = false; bool local_capacity = false; uint64_t score = 0; };
  struct Stats
  {
    size_t records, key_capacity_bytes, max_records, max_key_capacity_bytes;
    uint64_t expired, capacity_refusals, allocation_refusals, saturated_additions;
    size_t last_sweep_work, maximum_sweep_work;
  };

  explicit PeerErrorHandler( const P2POptions& opts = {}, Now now = Clock::now )
    : _disconnect_threshold( opts.error_score_disconnect_threshold ),
      _reconnect_threshold( opts.error_score_reconnect_threshold ),
      _max_records( opts.max_peer_error_records ),
      _max_key_bytes( opts.peer_error_key_capacity_bytes ),
      _decay_constant( std::log( 2.0 ) / std::chrono::duration< double, std::nano >( opts.error_score_halflife ).count() ),
      _now( std::move( now ) ), _sweep( _scores.end() )
  {
    if( opts.error_score_halflife <= std::chrono::minutes::zero() || !std::isfinite( _decay_constant ) || !_now )
      throw std::invalid_argument( "Peer error score half-life and clock must be valid" );
    if( !_max_records || !_max_key_bytes )
      throw std::invalid_argument( "Peer error record and key-capacity limits must be positive" );
  }

  ErrorResult record_error( const std::string& peer_address, uint64_t score )
  {
    PeerErrorAddress key( peer_address );
    std::lock_guard lock( _mutex );
    const auto now = _now();
    sweep_expired( now );
    auto it = _scores.find( key.value() );
    if( it == _scores.end() )
    {
      if( !score && _disconnect_threshold > 0 && _reconnect_threshold > 0 ) return {};
      if( !can_reserve( key.value() ) ) return refuse();
      try
      {
        std::string owned( key.value() );
        const auto capacity = owned.capacity();
        if( capacity > _max_key_bytes - _key_bytes ) return refuse();
        auto inserted = _scores.emplace( std::move( owned ), ScoreRecord{ score, now } );
        it = inserted.first;
        _key_bytes += it->first.capacity();
      }
      catch( const std::bad_alloc& )
      {
        ++_allocation_refusals;
        return { true, true, 0 };
      }
    }
    else
    {
      if( !score )
      {
        const auto current = projected( it->second, now );
        if( disposable( current ) ) erase( it );
        return { current >= _disconnect_threshold, false, current };
      }
      decay( it->second, now );
      if( score > std::numeric_limits< uint64_t >::max() - it->second.score )
      {
        it->second.score = std::numeric_limits< uint64_t >::max();
        ++_saturated_additions;
      }
      else it->second.score += score;
    }
    const auto current = it->second.score;
    return { current >= _disconnect_threshold, false, current };
  }

  Admission admission( const std::string& peer_address ) const
  {
    PeerErrorAddress key( peer_address );
    std::lock_guard lock( _mutex );
    const auto now = _now();
    sweep_expired( now );
    auto it = _scores.find( key.value() );
    if( it == _scores.end() )
    {
      if( can_reserve( key.value() ) ) return Admission::allowed;
      ++_capacity_refusals;
      return Admission::local_capacity;
    }
    // Keep the last score-changing event as the anchor. Repeated reads must
    // not compound integer truncation and prematurely erase a meaningful ban.
    const auto current = projected( it->second, now );
    if( disposable( current ) ) erase( it );
    return current < _reconnect_threshold ? Admission::allowed : Admission::score_blocked;
  }

  bool can_connect( const std::string& peer_address ) const
  { return admission( peer_address ) == Admission::allowed; }

  uint64_t get_score( const std::string& peer_address ) const
  {
    PeerErrorAddress key( peer_address );
    std::lock_guard lock( _mutex );
    const auto now = _now();
    sweep_expired( now );
    auto it = _scores.find( key.value() );
    if( it == _scores.end() ) return 0;
    // Keep the last score-changing event as the anchor. Repeated reads must
    // not compound integer truncation and prematurely erase a meaningful ban.
    const auto current = projected( it->second, now );
    if( disposable( current ) ) erase( it );
    return current;
  }

  void collect_expired() const
  { std::lock_guard lock( _mutex ); sweep_expired( _now() ); }

  Stats stats() const
  {
    std::lock_guard lock( _mutex );
    return { _scores.size(), _key_bytes, _max_records, _max_key_bytes, _expired,
      _capacity_refusals, _allocation_refusals, _saturated_additions,
      _last_sweep_work, _maximum_sweep_work };
  }

private:
  struct ScoreRecord { uint64_t score; Clock::time_point last_update; };
  using Records = std::map< std::string, ScoreRecord, std::less<> >;
  bool can_reserve( std::string_view key ) const noexcept
  {
    // Check the borrowed value before constructing any owned key. The actual
    // string capacity is checked again before publishing a new map entry.
    return !key.empty() && _scores.size() < _max_records
      && std::max( key.size(), std::string().capacity() ) <= _max_key_bytes - _key_bytes;
  }
  ErrorResult refuse() noexcept
  { ++_capacity_refusals; return { true, true, 0 }; }
  uint64_t projected( const ScoreRecord& record, Clock::time_point now ) const noexcept
  {
    if( now <= record.last_update || !record.score ) return record.score;
    // Same exponential expression and nanosecond units as the Go reference.
    const auto elapsed = std::chrono::duration< double, std::nano >( now - record.last_update ).count();
    const double factor = std::exp( -_decay_constant * elapsed );
    if( factor >= 1 ) return record.score;
    const double scaled = static_cast< double >( record.score ) * factor;
    // double(UINT64_MAX) rounds to 2^64 on these targets. Clamp before the
    // floating-to-integer conversion so even an extreme score remains defined.
    if( scaled >= static_cast< double >( std::numeric_limits< uint64_t >::max() ) )
      return std::numeric_limits< uint64_t >::max();
    return static_cast< uint64_t >( scaled );
  }
  void decay( ScoreRecord& record, Clock::time_point now ) const noexcept
  {
    record.score = projected( record, now );
    if( now > record.last_update ) record.last_update = now;
  }
  bool disposable( uint64_t score ) const noexcept
  { return score == 0 && _reconnect_threshold > 0 && _disconnect_threshold > 0; }
  void erase( Records::iterator it ) const noexcept
  {
    if( _sweep == it ) ++_sweep;
    _key_bytes -= it->first.capacity();
    _scores.erase( it );
    ++_expired;
  }
  void sweep_expired( Clock::time_point now ) const noexcept
  {
    _last_sweep_work = 0;
    const auto work = std::min( sweep_work_limit, _scores.size() );
    while( _last_sweep_work < work && !_scores.empty() )
    {
      if( _sweep == _scores.end() ) _sweep = _scores.begin();
      auto it = _sweep++;
      ++_last_sweep_work;
      // Do not round or refresh unrelated meaningful records during GC.
      if( disposable( projected( it->second, now ) ) ) erase( it );
    }
    _maximum_sweep_work = std::max( _maximum_sweep_work, _last_sweep_work );
  }

  const uint64_t _disconnect_threshold, _reconnect_threshold;
  const size_t _max_records, _max_key_bytes;
  const double _decay_constant;
  Now _now;
  mutable std::mutex _mutex;
  mutable Records _scores;
  mutable Records::iterator _sweep;
  mutable size_t _key_bytes = 0;
  mutable uint64_t _expired = 0, _capacity_refusals = 0, _allocation_refusals = 0;
  uint64_t _saturated_additions = 0;
  mutable size_t _last_sweep_work = 0, _maximum_sweep_work = 0;
};

} // namespace koinos::node::p2p
