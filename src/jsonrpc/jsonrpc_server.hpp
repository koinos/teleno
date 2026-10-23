#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>

#include "core/rpc_access_policy.hpp"
#include "interfaces/i_block_store.hpp"
#include "interfaces/i_chain.hpp"
#include "interfaces/i_mempool.hpp"
#include "contract_meta_store/contract_meta_store.hpp"
#include "transaction_store/transaction_store.hpp"
#include "account_history/account_history.hpp"

namespace koinos::node::jsonrpc {

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp       = net::ip::tcp;

// Each mixed-batch mempool/transaction result retains its attribution until the
// complete HTTP body is no longer borrowed by an asynchronous write.
using MempoolOutputLeases = std::vector< OwnedMemoryBudget::Lease,
  OwnedMemoryBudget::Allocator< OwnedMemoryBudget::Lease > >;

/**
 * HTTP JSON-RPC 2.0 server replacing the Go koinos-jsonrpc service.
 *
 * Routes method calls directly to C++ service interfaces — no AMQP,
 * no protobuf serialization, no message broker.
 *
 * Method format: "chain.get_head_info" or "koinos.rpc.chain.get_head_info"
 * Supports single requests and batch (JSON array).
 */
class JSONRPCServer
{
public:
  JSONRPCServer( IChain* chain,
                 IMempool* mempool,
                 IBlockStore* block_store,
                 contract_meta_store::ContractMetaStore* contract_meta = nullptr,
                 transaction_store::TransactionStore* tx_store         = nullptr,
                 account_history::AccountHistory* acct_history         = nullptr,
                 const std::string& listen_address = "0.0.0.0",
                 uint16_t port                     = 8080,
                 unsigned int threads               = 4,
                 koinos::node::RpcAccessPolicy access_policy = {},
                 std::chrono::milliseconds io_timeout = std::chrono::seconds( 30 ) );

  ~JSONRPCServer();

  void start();
  void stop();
  uint16_t port() const { return _endpoint.port(); }
  std::size_t active_sessions() const;

  struct HTTPBodyMemory
  {
    std::size_t completed_request_capacity = 0;
    std::size_t materialized_response_capacity = 0;
    std::size_t peak_combined_capacity = 0;
  };
  // Attributed std::string capacities after read/response materialization,
  // including pending handlers after registry removal. Not a heap/RSS limit:
  // partial ingress, headers, transport buffers and service/JSON work are extra.
  HTTPBodyMemory http_body_memory() const;

  // Independent, bounded workspace permits for replies after ordinary M1
  // admission is full. Actual allocations remain attributed to the M1 owner;
  // do not add these conservative permits to its physical allocation counters.
  OwnedMemorySnapshot refusal_workspace_memory() const { return _refusal_workspace.snapshot(); }

private:
  /** Accept incoming TCP connections. */
  void do_accept();

  struct Session;
  void release_session( Session* session );
  void replace_body_capacities( std::size_t old_request, std::size_t old_response,
                               std::size_t new_request, std::size_t new_response );

  /** Process an HTTP request body and return the JSON-RPC response. */
  std::string process_body( const std::string& body, MempoolOutputLeases& mempool_output_leases,
                            OwnedMemoryBudget::Lease& refusal_workspace );

  /** Process a single JSON-RPC request object. */
  struct PreparedTransactionReply;
  nlohmann::json process_single_request( const nlohmann::json& request,
                                         PreparedTransactionReply* prepared = nullptr );

  /**
   * Parse method string → (service, method).
   * "chain.get_head_info" → ("chain", "get_head_info")
   * "koinos.rpc.chain.get_head_info" → ("chain", "get_head_info")
   */
  static std::pair< std::string, std::string > parse_method( const std::string& method_str );

  /** Dispatch a parsed method call to the appropriate service interface. */
  nlohmann::json dispatch( const std::string& service,
                           const std::string& method,
                           const nlohmann::json& params );

  // ── Chain dispatch ──
  nlohmann::json dispatch_chain( const std::string& method, const nlohmann::json& params );

  // ── Block store dispatch ──
  nlohmann::json dispatch_block_store( const std::string& method, const nlohmann::json& params );

  // ── Mempool dispatch ──
  nlohmann::json dispatch_mempool( const std::string& method, const nlohmann::json& params );

  // ── Contract meta store dispatch ──
  nlohmann::json dispatch_contract_meta_store( const std::string& method, const nlohmann::json& params );

  // ── Transaction store dispatch ──
  nlohmann::json dispatch_transaction_store( const std::string& method, const nlohmann::json& params );

  // ── Account history dispatch ──
  nlohmann::json dispatch_account_history( const std::string& method, const nlohmann::json& params );

  /** Build a JSON-RPC error response. */
  static nlohmann::json make_error( int code, const std::string& message, const nlohmann::json& id = nullptr );
  static nlohmann::json make_local_capacity_error( const LocalCapacityExceeded& error,
                                                   const nlohmann::json& id );

  /** Build a JSON-RPC success response. */
  static nlohmann::json make_result( const nlohmann::json& id );

  /** Convert a protobuf message to JSON (proto3 JSON mapping). */
  static std::string proto_to_json( const google::protobuf::Message& msg );

  /** Parse JSON params into a protobuf message (proto3 JSON mapping). */
  static bool json_to_proto( const nlohmann::json& params, google::protobuf::Message& msg );

  // ── JSON-RPC 2.0 error codes ──
  static constexpr int PARSE_ERROR      = -32700;
  static constexpr int INVALID_REQUEST  = -32600;
  static constexpr int METHOD_NOT_FOUND = -32601;
  static constexpr int INVALID_PARAMS   = -32602;
  static constexpr int INTERNAL_ERROR   = -32603;
  static constexpr int APP_ERROR        = -32001;

  IChain* _chain;
  IMempool* _mempool;
  IBlockStore* _block_store;
  contract_meta_store::ContractMetaStore* _contract_meta;
  transaction_store::TransactionStore* _tx_store;
  account_history::AccountHistory* _acct_history;

  OwnedMemoryBudget _refusal_workspace;

  net::io_context _ioc;
  tcp::acceptor _acceptor;
  tcp::endpoint _endpoint;
  net::strand< net::io_context::executor_type > _accept_strand;
  std::vector< std::thread > _threads;
  std::atomic< bool > _running{ false };
  mutable std::mutex _sessions_mutex;
  std::map< Session*, std::shared_ptr< Session > > _sessions;
  mutable std::mutex _http_body_mutex;
  HTTPBodyMemory _http_body_memory;
  std::mutex _lifecycle_mutex;
  unsigned int _thread_count;
  koinos::node::RpcAccessPolicy _access_policy;
  std::chrono::milliseconds _io_timeout;
};

} // namespace koinos::node::jsonrpc
