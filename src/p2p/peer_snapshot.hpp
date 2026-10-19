#pragma once

#include "types.hpp"
#include <memory>
#include <utility>

namespace koinos::node::p2p {
namespace detail { class PeerSnapshotBudget; }

// Native projections keep their admission owner until all their storage dies.
// Other transports/test doubles may construct their own unbudgeted projection.
class PeerSnapshot
{
  friend class detail::PeerSnapshotBudget;
  // Declaration order matters: destroy peer storage before returning credits.
  std::shared_ptr<void> _owner;
  std::vector<PeerID> _peers;
  PeerSnapshot( std::shared_ptr<void> owner, std::vector<PeerID> peers ) noexcept
    : _owner(std::move(owner)), _peers(std::move(peers)) {}
public:
  PeerSnapshot() = default;
  PeerSnapshot( std::vector<PeerID> peers ) : _peers(std::move(peers)) {}
  PeerSnapshot( const PeerSnapshot& ) = delete;
  PeerSnapshot& operator=( const PeerSnapshot& ) = delete;
  PeerSnapshot( PeerSnapshot&& ) noexcept = default;
  PeerSnapshot& operator=( PeerSnapshot&& other ) noexcept
  {
    if( this != &other ) { PeerSnapshot incoming(std::move(other));swap(incoming); }
    return *this;
  }
  void swap( PeerSnapshot& other ) noexcept { _owner.swap(other._owner);_peers.swap(other._peers); }
  auto begin() const noexcept { return _peers.begin(); }
  auto end() const noexcept { return _peers.end(); }
  size_t size() const noexcept { return _peers.size(); }
  bool empty() const noexcept { return _peers.empty(); }
  const PeerID& operator[]( size_t index ) const noexcept { return _peers[index]; }
};
} // namespace koinos::node::p2p
