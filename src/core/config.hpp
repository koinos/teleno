#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace koinos::node {

struct ConfigCheckpoint
{
  uint64_t block_height = 0;
  std::string block_id;
};

struct BackupScheduleConfig
{
  bool enabled = false;
  std::string interval = "24h";
  bool run_on_startup_if_missed = true;
  uint64_t jitter_seconds = 300;
  uint64_t minimum_head_progress = 1;
  bool skip_if_syncing_from_genesis = true;
  uint64_t max_concurrent_backups = 1;
};

struct BackupLocalConfig
{
  bool enabled = false;
  std::string directory;
  uint64_t retention_count = 7;
};

struct BackupSshConfig
{
  bool enabled = false;
  std::string transport = "native";
  std::string host;
  uint64_t port = 22;
  std::string user;
  std::string auth = "password-file";
  std::string password_file;
  std::string private_key_file;
  std::string passphrase_file;
  std::string known_hosts_file;
  bool strict_host_key_checking = true;
  uint64_t connect_timeout_seconds = 15;
};

struct BackupRemoteConfig
{
  bool enabled = false;
  std::string directory;
  uint64_t retention_count = 14;
  uint64_t retention_days = 30;
  std::string upload_temp_suffix = ".partial";
};

struct BackupPublicRestoreConfig
{
  bool enabled = false;
  std::string base_url;
  std::string network = "testnet";
  bool require_https = true;
  uint64_t timeout_seconds = 30;
  uint64_t retries = 3;
  bool signature_required = false;
  std::string signature_public_key_file;
};

struct BackupPublicPublishConfig
{
  bool enabled = false;
  std::string directory;
  std::string base_url;
  std::string network = "mainnet";
  std::string observer_config_file;
  uint64_t retention_count = 1;
  std::string upload_temp_suffix = ".partial";
};

struct BackupAdminConfig
{
  bool enabled = false;
  std::string listen = "127.0.0.1:18088";
  std::string token_file;
  uint64_t jobs = 1;
};

struct BackupConfig
{
  bool enabled = false;
  std::string node_id;
  std::string workspace;
  BackupScheduleConfig schedule;
  BackupLocalConfig local;
  BackupSshConfig ssh;
  BackupRemoteConfig remote;
  BackupPublicRestoreConfig public_restore;
  BackupPublicPublishConfig public_publish;
  BackupAdminConfig admin;
};

/**
 * Unified configuration parsed from config.yml + CLI flags.
 * Replaces per-service config parsing.
 */
struct NodeConfig
{
  // ── Global ──
  std::string log_level    = "info";
  std::string instance_id  = "Koinos";
  std::string fork_algorithm = "pob";
  std::filesystem::path basedir;
  bool log_color    = true;
  bool log_datetime = true;
  std::vector< std::string > rpc_blacklist;
  std::vector< std::string > rpc_whitelist;

  // ── Chain ──
  uint64_t chain_jobs                    = 2;
  bool verify_blocks                     = false;
  uint64_t read_compute_bandwidth_limit  = 10'000'000;
  uint64_t pending_transaction_limit     = 10;
  bool disable_pending_transaction_limit = false;
  bool reset                             = false;

  // ── P2P ──
  std::string p2p_listen   = "/ip4/0.0.0.0/tcp/8888";
  std::vector< std::string > p2p_seeds;
  std::string p2p_identity_seed;
  std::vector< ConfigCheckpoint > p2p_checkpoints;
  uint64_t p2p_jobs        = 4;
  uint64_t p2p_seed_reconnect_interval_seconds = 10;
  bool p2p_peer_discovery_enabled = true;
  uint64_t p2p_target_peer_count = 20;
  uint64_t p2p_max_peer_candidates = 200;
  uint64_t p2p_max_candidate_dials_per_cycle = 3;
  uint64_t p2p_peer_acquisition_interval_seconds = 5;
  uint64_t p2p_candidate_redial_interval_seconds = 60;
  uint64_t p2p_peer_log_interval_seconds = 60;
  bool p2p_force_gossip   = false;
  bool p2p_disable_gossip = false;

  // ── JSON-RPC ──
  std::string jsonrpc_listen = "0.0.0.0:8080";
  uint64_t jsonrpc_jobs      = 4;

  // ── gRPC ──
  std::string grpc_listen = "0.0.0.0:50051";
  uint64_t grpc_jobs      = 2;

  // ── Block Producer ──
  std::string block_producer_algorithm = "pob";
  std::string block_producer_private_key_file;
  std::string block_producer_address;
  uint64_t block_producer_resources_lower_bound = 75;
  uint64_t block_producer_resources_upper_bound = 90;
  uint64_t block_producer_max_inclusion_attempts = 2'000;
  bool block_producer_gossip_production = true;
  std::vector< std::string > block_producer_approved_proposals;

  // ── Mempool ──
  uint64_t mempool_transaction_expiration = 120;

  // ── RocksDB ──
  uint64_t rocksdb_block_cache_mb                 = 256;
  uint64_t rocksdb_max_background_jobs            = 4;
  uint64_t rocksdb_bytes_per_sync                 = 1'048'576;
  uint64_t rocksdb_default_block_size             = 4 * 1024;
  uint64_t rocksdb_blocks_block_size              = 64 * 1024;
  uint64_t rocksdb_target_file_size_base          = 64 * 1024 * 1024;
  uint64_t rocksdb_max_bytes_for_level_base       = 512 * 1024 * 1024;
  uint64_t rocksdb_write_buffer_size              = 64 * 1024 * 1024;
  uint64_t rocksdb_db_write_buffer_size           = 256 * 1024 * 1024;
  uint64_t rocksdb_max_write_buffer_number        = 3;
  std::string rocksdb_compression                 = "zstd";
  std::string rocksdb_blocks_compression;
  bool rocksdb_require_compression                = false;

  // ── Backup ──
  BackupConfig backup;

  // ── Feature flags ──
  std::map< std::string, bool > features = {
    { "chain",               true  },
    { "mempool",             true  },
    { "block_store",         true  },
    { "p2p",                 true  },
    { "jsonrpc",             true  },
    { "grpc",                false },
    { "block_producer",      false },
    { "contract_meta_store", true  },
    { "transaction_store",   true  },
    { "account_history",     false }
  };

  /** Check if a component is enabled. */
  bool is_enabled( const std::string& component ) const
  {
    auto it = features.find( component );
    return it != features.end() && it->second;
  }
};

/**
 * Load configuration from YAML file and merge CLI overrides.
 *
 * @param config_path  Path to config.yml
 * @param cli_enables  Components explicitly enabled via --enable
 * @param cli_disables Components explicitly disabled via --disable
 * @return Merged configuration
 */
NodeConfig load_config( const std::filesystem::path& config_path,
                        const std::vector< std::string >& cli_enables  = {},
                        const std::vector< std::string >& cli_disables = {} );

} // namespace koinos::node
