#pragma once

#include <cmath>
#include <chrono>
#include <map>
#include <mutex>
#include <string>

#include "types.hpp"

namespace koinos::node::p2p {

/**
 * Peer error scoring with exponential decay.
 * Peers exceeding the disconnect threshold are dropped.
 * Peers exceeding the reconnect threshold are blocked from reconnecting.
 */
class PeerErrorHandler
{
public:
  explicit PeerErrorHandler( const P2POptions& opts = {} ) : _opts( opts ) {}

  /** Record an error for a peer. Returns true if peer should be disconnected. */
  bool record_error( const std::string& peer_addr, uint64_t score )
  {
    std::lock_guard lock( _mutex );

    auto now = std::chrono::steady_clock::now();
    auto& record = _scores[ peer_addr ];

    // Decay existing score
    if( record.score > 0 )
    {
      auto elapsed = std::chrono::duration_cast< std::chrono::seconds >( now - record.last_update ).count();
      double halflife_secs = std::chrono::duration_cast< std::chrono::seconds >( _opts.error_score_halflife ).count();
      double decay = std::exp( -( std::log( 2.0 ) / halflife_secs ) * elapsed );
      record.score = static_cast< uint64_t >( record.score * decay );
    }

    record.score += score;
    record.last_update = now;

    return record.score >= _opts.error_score_disconnect_threshold;
  }

  /** Check if a peer address is allowed to connect. */
  bool can_connect( const std::string& peer_addr ) const
  {
    std::lock_guard lock( _mutex );

    auto it = _scores.find( peer_addr );
    if( it == _scores.end() )
      return true;

    return it->second.score < _opts.error_score_reconnect_threshold;
  }

  /** Get current score for a peer. */
  uint64_t get_score( const std::string& peer_addr ) const
  {
    std::lock_guard lock( _mutex );
    auto it = _scores.find( peer_addr );
    return it != _scores.end() ? it->second.score : 0;
  }

private:
  struct ScoreRecord
  {
    uint64_t score = 0;
    std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now();
  };

  P2POptions _opts;
  mutable std::mutex _mutex;
  std::map< std::string, ScoreRecord > _scores;
};

} // namespace koinos::node::p2p
