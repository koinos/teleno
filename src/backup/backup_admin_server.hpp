#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "backup/backup_service.hpp"

namespace koinos::node::backup {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class BackupAdminServer
{
public:
  BackupAdminServer( BackupService* backup_service,
                     const std::string& listen_address = "127.0.0.1",
                     uint16_t port = 18088,
                     unsigned int threads = 1,
                     std::string bearer_token = {} );
  ~BackupAdminServer();

  void start();
  void stop();
  uint16_t port() const;

private:
  static net::ip::address loopback_address_or_throw( const std::string& listen_address );

  void do_accept();
  void handle_session( tcp::socket socket );
  bool is_admin_target( const std::string& target ) const;
  bool request_authorized( const http::request< http::string_body >& req ) const;
  std::string status_response( const BackupOperationStatus& status ) const;
  std::string operation_not_found_response( const std::string& operation_id,
                                            const BackupOperationStatus& status ) const;
  std::string config_response();
  std::string list_response( bool remote );
  std::string create_response( const std::string& body );
  std::string upload_latest_response();
  std::string delete_response( const std::string& body );
  std::string restore_fetch_response( const std::string& body );
  std::string restore_preflight_response( const std::string& body );
  std::string cancel_response( const BackupOperationStatus& status );
  std::string restore_stage_response( const std::string& body );
  std::string restore_activate_response( const std::string& body );

  BackupService* _backup_service;
  std::string _listen_address;
  std::string _bearer_token;
  net::io_context _ioc;
  tcp::acceptor _acceptor;
  std::vector< std::thread > _threads;
  std::vector< std::thread > _session_threads;
  std::mutex _session_mutex;
  std::atomic< bool > _running{ false };
  unsigned int _thread_count;
};

} // namespace koinos::node::backup
