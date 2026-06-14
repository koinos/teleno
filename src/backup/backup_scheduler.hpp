#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>

#include "backup/backup_service.hpp"
#include "core/config.hpp"

namespace koinos::node::backup {

using BackupHeadHeightProvider = std::function< uint64_t() >;

std::chrono::milliseconds parse_backup_schedule_interval( const std::string& value );

class BackupScheduler
{
public:
  BackupScheduler( BackupService* backup_service,
                   NodeConfig cfg,
                   BackupHeadHeightProvider head_height_provider );
  ~BackupScheduler();

  void start();
  void stop();

private:
  bool wait_for_stop_or_timeout( std::chrono::milliseconds timeout );
  bool stop_requested() const;
  std::chrono::milliseconds jitter_delay();
  bool should_run_at_height( uint64_t head_height );
  void run_loop();
  void run_once();

  BackupService* _backup_service;
  NodeConfig _cfg;
  BackupHeadHeightProvider _head_height_provider;
  std::chrono::milliseconds _interval;
  std::mt19937_64 _rng;

  mutable std::mutex _mutex;
  std::condition_variable _cv;
  bool _stop_requested = false;
  bool _started = false;
  std::thread _thread;

  std::optional< uint64_t > _last_successful_backup_height;
};

} // namespace koinos::node::backup
