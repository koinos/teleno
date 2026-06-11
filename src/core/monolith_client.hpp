#pragma once

/**
 * IRpcClient — abstract interface replacing mq::client for the monolith.
 *
 * The existing chain controller/indexer call:
 *   _client->rpc(service_name, serialized_request) → returns future<string>
 *   _client->broadcast(topic, serialized_message)
 *
 * This interface is implemented by MonolithRpcClient which routes
 * directly to IBlockStore/IMempool/EventBus.
 */

#include <chrono>
#include <future>
#include <string>

namespace koinos::node {

class IRpcClient
{
public:
  virtual ~IRpcClient() = default;

  /** Retry policy placeholder — ignored in monolith (no AMQP retries needed). */
  enum class retry_policy { none, exponential_backoff };

  virtual std::shared_future< std::string >
  rpc( const std::string& service,
       const std::string& payload,
       std::chrono::milliseconds timeout = std::chrono::milliseconds( 5000 ),
       retry_policy = retry_policy::none ) = 0;

  virtual void broadcast( const std::string& topic, const std::string& payload ) = 0;

  virtual void connect( const std::string& /* url */ ) {}
};

} // namespace koinos::node
