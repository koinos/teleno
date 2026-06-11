#pragma once

#include <boost/signals2.hpp>

#include <koinos/broadcast/broadcast.pb.h>

namespace koinos::node {

/**
 * Central event bus replacing AMQP topic exchanges.
 *
 * Each signal corresponds to a former AMQP broadcast topic.
 * Handlers receive const references — zero serialization overhead.
 *
 * Thread safety: boost::signals2 is thread-safe by default.
 */
class EventBus
{
public:
  // Replaces koinos.block.accept
  boost::signals2::signal< void( const broadcast::block_accepted& ) > on_block_accepted;

  // Replaces koinos.block.irreversible
  boost::signals2::signal< void( const broadcast::block_irreversible& ) > on_block_irreversible;

  // Replaces koinos.transaction.accept
  boost::signals2::signal< void( const broadcast::transaction_accepted& ) > on_transaction_accepted;

  // Replaces koinos.transaction.fail
  boost::signals2::signal< void( const broadcast::transaction_failed& ) > on_transaction_failed;

  // Replaces koinos.gossip.status (bool = synced)
  boost::signals2::signal< void( bool ) > on_gossip_status;

  // Replaces koinos.block.forks
  boost::signals2::signal< void( const broadcast::fork_heads& ) > on_fork_heads;
};

} // namespace koinos::node
