#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <tuple>

namespace koinos::node::p2p {

/**
 * Tracks blocks per (signer, parent) pair to detect fork bombs.
 * A peer producing >3 forks from the same parent is flagged.
 */
class ForkWatchdog
{
public:
  static constexpr int max_forks_per_parent = 3;

  /** Returns true if this block is a fork bomb (too many forks from same signer+parent). */
  bool check( const std::string& block_id,
              const std::string& signer,
              const std::string& parent_id,
              uint64_t height )
  {
    std::lock_guard lock( _mutex );

    auto key = std::make_tuple( signer, parent_id );
    auto& blocks = _forks[ key ];
    blocks.insert( block_id );

    _height_index[ height ].emplace( key );

    return static_cast< int >( blocks.size() ) > max_forks_per_parent;
  }

  /** Remove all entries at or below the given height (LIB cleanup). */
  void purge_below( uint64_t height )
  {
    std::lock_guard lock( _mutex );

    auto it = _height_index.begin();
    while( it != _height_index.end() && it->first <= height )
    {
      for( const auto& key: it->second )
        _forks.erase( key );
      it = _height_index.erase( it );
    }
  }

private:
  using fork_key = std::tuple< std::string, std::string >;

  std::mutex _mutex;
  std::map< fork_key, std::set< std::string > > _forks;
  std::map< uint64_t, std::set< fork_key > > _height_index;
};

} // namespace koinos::node::p2p
