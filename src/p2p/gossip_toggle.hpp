#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>

#include <koinos/log.hpp>

#include "types.hpp"

namespace koinos::node::p2p {

/**
 * Enables/disables gossip based on sync state.
 * Gossip is enabled when head block is within 45s of wall clock
 * AND at least 1 peer is connected.
 */
class GossipToggle
{
public:
  using EnableCallback     = std::function< void( bool ) >;
  using PeerCountCallback  = std::function< uint32_t() >;

  GossipToggle( const P2POptions& opts,
                EnableCallback enable_cb,
                PeerCountCallback peer_count_cb )
      : _opts( opts ),
        _enable_cb( std::move( enable_cb ) ),
        _peer_count_cb( std::move( peer_count_cb ) )
  {
  }

  void start()
  {
    if( _opts.always_enable_gossip )
    {
      set_enabled( true );
      return;
    }
    if( _opts.always_disable_gossip )
    {
      set_enabled( false );
      return;
    }

    _running = true;
    _thread = std::thread( [this]() { run_loop(); } );
  }

  void stop()
  {
    _running = false;
    if( _thread.joinable() )
      _thread.join();
  }

  /** Called by the node when a new head block is received. */
  void update_head_time( uint64_t block_timestamp_ms )
  {
    std::lock_guard lock( _head_mutex );
    _head_time_ms = block_timestamp_ms;
  }

  bool is_enabled() const { return _enabled; }

private:
  void run_loop()
  {
    while( _running )
    {
      std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
      if( !_running )
        break;

      uint32_t peers = _peer_count_cb ? _peer_count_cb() : 0;
      if( peers == 0 )
      {
        if( _enabled )
          set_enabled( false );
        continue;
      }

      uint64_t head_ms;
      {
        std::lock_guard lock( _head_mutex );
        head_ms = _head_time_ms;
      }

      if( head_ms == 0 )
        continue;

      auto head_time = std::chrono::system_clock::time_point(
        std::chrono::milliseconds( head_ms ) );
      auto now = std::chrono::system_clock::now();
      auto lag = std::chrono::duration_cast< std::chrono::seconds >( now - head_time );

      if( lag < _opts.gossip_head_threshold )
      {
        if( !_enabled )
          set_enabled( true );
      }
      else
      {
        if( _enabled )
          set_enabled( false );
      }
    }
  }

  void set_enabled( bool enabled )
  {
    _enabled = enabled;
    if( _enable_cb )
      _enable_cb( enabled );
    LOG( info ) << "[p2p] Gossip " << ( enabled ? "enabled" : "disabled" );
  }

  P2POptions _opts;
  EnableCallback _enable_cb;
  PeerCountCallback _peer_count_cb;

  std::atomic< bool > _running{ false };
  std::atomic< bool > _enabled{ false };
  std::thread _thread;

  std::mutex _head_mutex;
  uint64_t _head_time_ms = 0;
};

} // namespace koinos::node::p2p
