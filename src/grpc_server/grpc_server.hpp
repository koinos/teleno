#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "interfaces/i_block_store.hpp"
#include "interfaces/i_chain.hpp"
#include "interfaces/i_mempool.hpp"
#include "account_history/account_history.hpp"
#include "contract_meta_store/contract_meta_store.hpp"
#include "transaction_store/transaction_store.hpp"

namespace koinos::node::grpc_server {

/**
 * gRPC server gateway that routes requests to internal service interfaces.
 * Replaces the Go koinos-grpc service.
 *
 * Uses the same protobuf service definitions from koinos_proto.
 * Routes chain/block_store/mempool/account_history/contract_meta_store/
 * transaction_store requests directly to C++ implementations.
 *
 * NOTE: Requires gRPC::grpc++ library. When the transport layer is
 * available, this will serve on the configured endpoint (default: 50051).
 */
class GRPCServer
{
public:
  GRPCServer( IChain* chain,
              IMempool* mempool,
              IBlockStore* block_store,
              contract_meta_store::ContractMetaStore* contract_meta = nullptr,
              transaction_store::TransactionStore* tx_store         = nullptr,
              account_history::AccountHistory* acct_history         = nullptr,
              const std::string& listen_address = "0.0.0.0:50051",
              unsigned int threads               = 2,
              const std::atomic< bool >* gossip_status = nullptr );

  ~GRPCServer();

  void start();
  void stop();
  int bound_port() const;

private:
  IChain* _chain;
  IMempool* _mempool;
  IBlockStore* _block_store;
  contract_meta_store::ContractMetaStore* _contract_meta;
  transaction_store::TransactionStore* _tx_store;
  account_history::AccountHistory* _acct_history;
  const std::atomic< bool >* _gossip_status;

  std::string _listen_address;
  unsigned int _thread_count;
  std::atomic< bool > _running{ false };

  // gRPC server instance — opaque until gRPC service impl is wired
  struct Impl;
  std::unique_ptr< Impl > _impl;
};

} // namespace koinos::node::grpc_server
