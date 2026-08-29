#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <future>
#include <mutex>
#include <optional>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/thread/sync_bounded_queue.hpp>

#include <koinos/block_store/block_store.pb.h>
#include <koinos/chain/controller.hpp>
#include <koinos/chain/exceptions.hpp>
#include "core/monolith_client.hpp"

namespace koinos::chain {

class indexer final
{
public:
  indexer( boost::asio::io_context& ioc, controller& c, std::shared_ptr< node::IRpcClient > mc, bool verify_blocks );

  std::future< bool > index();
  void stop();
  uint64_t fallback_count() const { return _fallback_count; }

private:
  void prepare_index();
  void send_requests( uint64_t last_height, uint64_t batch_size );
  void process_requests( uint64_t last_height, uint64_t batch_size );
  void drain_response_blocks();
  void process_block();

  void handle_error( const std::string& msg );

  boost::asio::io_context& _ioc;
  controller& _controller;
  std::shared_ptr< node::IRpcClient > _client;
  bool _verify_blocks = false;

  boost::asio::signal_set _signals;
  std::atomic_bool _stopped = false;

  boost::concurrent::sync_bounded_queue< std::shared_future< std::string > > _request_queue;
  std::atomic< bool > _requests_complete           = false;
  std::atomic< bool > _request_processing_complete = false;

  boost::concurrent::sync_bounded_queue< block_store::block_item > _block_queue;

  // A block-store response can exceed the bounded consumer queue. Keep at
  // most one response staged and move it into the queue without blocking an
  // io_context worker. The next request starts only after the staged response
  // has been drained, preserving bounded backpressure even with one worker.
  std::mutex _response_mutex;
  std::deque< block_store::block_item > _response_blocks;
  bool _response_active       = false;
  uint64_t _response_height   = 0;
  uint64_t _response_batch_size = 0;

  // An empty block queue is a transient producer/consumer condition. Retry
  // through the io_context timer instead of either blocking its worker or
  // continuously reposting a hot polling loop.
  boost::asio::steady_timer _block_queue_retry;
  std::mutex _block_queue_retry_mutex;

  // In fast replay, H stays writable until H+1 supplies the signed expectation
  // for H's state-delta root. This remains O(1) in retained block items.
  std::optional< block_store::block_item > _pending_item;
  uint64_t _fallback_count = 0;

  block_topology _target_head;
  rpc::chain::get_head_info_response _start_head_info;
  const std::chrono::time_point< std::chrono::system_clock > _start_time = std::chrono::system_clock::now();

  std::mutex _completion_mutex;
  std::optional< std::promise< bool > > _complete = std::promise< bool >();
};

} // namespace koinos::chain
