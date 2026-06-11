#pragma once

#include "interfaces/i_chain.hpp"
#include "interfaces/i_mempool.hpp"

#include <koinos/contracts/pob/pob.pb.h>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/protocol/protocol.pb.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace koinos::node::block_production {

struct ProducerConfig
{
  std::string algorithm = "pob";
  std::string producer_address;
  uint64_t resources_lower_bound  = 75;
  uint64_t resources_upper_bound  = 90;
  uint64_t max_inclusion_attempts = 2'000;
  std::vector< std::string > approved_proposals;
};

struct ProductionResult
{
  enum class Status
  {
    produced,
    not_ready,
    not_our_turn
  };

  Status status = Status::not_ready;
  uint64_t height = 0;
  std::string id;
  uint64_t transaction_count = 0;
  uint64_t removed_failed_transactions = 0;
  std::chrono::milliseconds retry_after = std::chrono::seconds( 5 );
};

crypto::private_key load_private_key_file( const std::filesystem::path& path );

class BlockProducer final
{
public:
  BlockProducer( IChain& chain, IMempool& mempool, crypto::private_key signing_key, ProducerConfig config );

  std::string public_address() const;
  std::string public_key_base64() const;
  void write_public_key_file( const std::filesystem::path& path ) const;

  ProductionResult produce_once();

private:
  struct PobAuxiliaryData
  {
    std::string pob_contract_id;
    std::string vhp_contract_id;
    std::string vhp_symbol;
    uint32_t vhp_precision = 0;
    uint32_t target_block_interval = 0;
    uint32_t quantum_length = 0;
    uint32_t quanta_per_block_interval = 0;
  };

  struct PobBundle
  {
    protocol::block block;
    std::string seed;
    std::string difficulty;
    uint64_t vhp_balance = 0;
    std::chrono::system_clock::time_point time_quantum;
  };

  ProductionResult produce_federated_once();
  ProductionResult produce_pob_once();

  protocol::block next_block( const std::string& signer );
  void fill_block( protocol::block& block );
  void set_merkle_roots( protocol::block& block ) const;
  bool submit_block( protocol::block& block, ProductionResult& result );

  std::chrono::system_clock::time_point next_time_quantum( std::chrono::system_clock::time_point time ) const;
  uint64_t now_ms() const;

  void refresh_pob_auxiliary_data();
  PobBundle next_pob_bundle();
  bool difficulty_met( const crypto::multihash& proof_hash, uint64_t vhp_balance, const std::string& difficulty ) const;

  std::string get_contract_address( const std::string& name );
  std::string get_vhp_symbol();
  uint32_t get_vhp_decimals();
  uint64_t get_vhp_balance();
  contracts::pob::metadata get_pob_metadata();

  IChain& _chain;
  IMempool& _mempool;
  crypto::private_key _signing_key;
  ProducerConfig _config;

  std::optional< PobAuxiliaryData > _pob_auxiliary_data;
  std::optional< PobBundle > _pob_bundle;
};

} // namespace koinos::node::block_production
