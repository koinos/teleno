#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <list>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace koinos::node::p2p {

// Disposable accepted-ID FIFO, not a validity oracle or a persistent index.
// Hits do not refresh age/order. All state and counters are protected internally.
class SeenCache
{
public:
  using Clock = std::chrono::steady_clock;
  struct Stats
  {
    size_t entries = 0;
    size_t id_capacity_bytes = 0; // owned string capacity + terminator, not RSS
    size_t max_entries = 0;
    size_t max_id_capacity_bytes = 0;
    uint64_t hits = 0;
    uint64_t capacity_evictions = 0;
    uint64_t expired = 0;
    uint64_t irreversible = 0;
    uint64_t bypassed = 0;
  };

  SeenCache( size_t max_entries, size_t max_bytes, std::chrono::seconds lifetime )
      : _max_entries( max_entries ), _max_bytes( max_bytes ), _lifetime( lifetime ) {}
  SeenCache( const SeenCache& ) = delete;
  SeenCache& operator=( const SeenCache& ) = delete;

  bool contains( std::string_view id, Clock::time_point now = Clock::now() )
  {
    std::lock_guard lock( _mutex );
    expire( now );
    if( _index.find( id ) == _index.end() )
      return false;
    ++_stats.hits;
    return true;
  }

  // Oversized IDs still go through normal validation; only caching is bypassed.
  void remember( std::string_view id, uint64_t height = 0, Clock::time_point now = Clock::now() )
  {
    std::lock_guard lock( _mutex );
    expire( now );
    if( _index.find( id ) != _index.end() )
      return;
    if( ( height != 0 && height <= _irreversible_height )
        || id.empty() || !_max_entries || !_max_bytes || _lifetime.count() <= 0
        || id.size() >= _max_bytes )
    {
      ++_stats.bypassed;
      return;
    }
    try
    {
      std::string owned( id );
      // Subtraction rather than size+1 avoids wraparound at SIZE_MAX.
      if( owned.capacity() >= _max_bytes )
      {
        ++_stats.bypassed;
        return;
      }
      const size_t bytes = owned.capacity() + 1;
      while( _fifo.size() >= _max_entries || bytes > _max_bytes - _stats.id_capacity_bytes )
      {
        erase( _fifo.begin() );
        ++_stats.capacity_evictions;
      }
      // Concurrent callers may sample their default time before taking the lock.
      // Keep FIFO timestamps monotonic so a delayed insertion cannot hide expiry.
      if( !_fifo.empty() && now < _fifo.back().accepted_at )
        now = _fifo.back().accepted_at;
      _fifo.push_back( { std::move( owned ), height, now } );
      auto entry = std::prev( _fifo.end() );
      try
      {
        // Views borrow the one stable list-owned key; erase index before owner.
        _index.emplace( std::string_view( entry->id ), entry );
      }
      catch( ... )
      {
        _fifo.pop_back();
        throw;
      }
      _stats.id_capacity_bytes += bytes;
    }
    catch( const std::bad_alloc& )
    {
      // Failure of an optional cache must not turn successful validation into
      // a chain/peer error. Prior evictions remain harmless cache misses.
      ++_stats.bypassed;
    }
  }

  void purge_irreversible( uint64_t height )
  {
    std::lock_guard lock( _mutex );
    if( height <= _irreversible_height )
      return;
    _irreversible_height = height;
    for( auto it = _fifo.begin(); it != _fifo.end(); )
    {
      if( it->height != 0 && it->height <= height )
      {
        it = erase( it );
        ++_stats.irreversible;
      }
      else
        ++it;
    }
  }

  Stats stats( Clock::time_point now = Clock::now() )
  {
    std::lock_guard lock( _mutex );
    expire( now );
    auto result = _stats;
    result.entries = _fifo.size();
    result.max_entries = _max_entries;
    result.max_id_capacity_bytes = _max_bytes;
    return result;
  }

  void clear()
  {
    std::lock_guard lock( _mutex );
    _index.clear();
    _fifo.clear();
    _stats.id_capacity_bytes = 0;
    _irreversible_height = 0;
  }

private:
  struct Entry
  {
    std::string id;
    uint64_t height;
    Clock::time_point accepted_at;
  };
  using Queue = std::list< Entry >;

  Queue::iterator erase( Queue::iterator it )
  {
    _stats.id_capacity_bytes -= it->id.capacity() + 1;
    _index.erase( std::string_view( it->id ) );
    return _fifo.erase( it );
  }

  void expire( Clock::time_point now )
  {
    while( !_fifo.empty() && now - _fifo.front().accepted_at >= _lifetime )
    {
      erase( _fifo.begin() );
      ++_stats.expired;
    }
  }

  const size_t _max_entries;
  const size_t _max_bytes;
  const std::chrono::seconds _lifetime;
  std::mutex _mutex;
  Queue _fifo;
  std::map< std::string_view, Queue::iterator > _index;
  Stats _stats;
  uint64_t _irreversible_height = 0;
};

} // namespace koinos::node::p2p
