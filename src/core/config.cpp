#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <type_traits>

#include <yaml-cpp/yaml.h>

namespace koinos::node {

namespace {

YAML::Node yaml_child( const YAML::Node& node, const std::string& key )
{
  if( node.Type() != YAML::NodeType::Map )
    return {};

  for( auto it = node.begin(); it != node.end(); ++it )
  {
    try
    {
      if( it->first.as< std::string >() == key )
        return it->second;
    }
    catch( ... )
    {}
  }

  return {};
}

template< typename T >
T yaml_get( const YAML::Node& node, const std::string& key, const T& fallback, bool strict = false )
{
  auto value = strict ? ( node.IsMap() ? node[ key ] : YAML::Node( YAML::NodeType::Undefined ) )
                      : yaml_child( node, key );
  if( value.IsDefined() )
  {
    try
    {
      if constexpr( std::is_unsigned_v< T > && !std::is_same_v< T, bool > )
      {
        if( strict && value.IsScalar() && value.Scalar().find_first_not_of( "0123456789" ) != std::string::npos )
          throw std::runtime_error( "invalid unsigned integer" );
      }
      return value.as< T >();
    }
    catch( ... )
    {
      if( strict )
        throw std::invalid_argument( "Invalid configuration value for " + key );
    }
  }
  return fallback;
}

std::vector< std::string > yaml_get_string_list( const YAML::Node& node, const std::string& key )
{
  std::vector< std::string > result;
  auto value = yaml_child( node, key );
  if( value.IsDefined() && value.IsSequence() )
  {
    for( const auto& item: value )
    {
      try
      {
        result.push_back( item.as< std::string >() );
      }
      catch( ... )
      {}
    }
  }
  return result;
}

std::string yaml_get_string_scalar( const YAML::Node& node, const std::string& key, const std::string& fallback = {} )
{
  auto value = yaml_child( node, key );
  if( value.IsDefined() && value.IsScalar() )
  {
    try
    {
      return value.as< std::string >();
    }
    catch( ... )
    {}
  }
  return fallback;
}

uint8_t hex_nibble( char value )
{
  if( value >= '0' && value <= '9' )
    return static_cast< uint8_t >( value - '0' );
  if( value >= 'a' && value <= 'f' )
    return static_cast< uint8_t >( value - 'a' + 10 );
  if( value >= 'A' && value <= 'F' )
    return static_cast< uint8_t >( value - 'A' + 10 );
  throw std::runtime_error( "invalid hex character" );
}

std::string decode_hex_bytes( const std::string& value )
{
  const auto offset = value.size() >= 2 && value[ 0 ] == '0' && ( value[ 1 ] == 'x' || value[ 1 ] == 'X' ) ? 2 : 0;
  if( value.size() == offset )
    throw std::runtime_error( "block id is empty" );
  if( ( value.size() - offset ) % 2 != 0 )
    throw std::runtime_error( "block id hex string has odd length" );

  std::string bytes;
  bytes.reserve( ( value.size() - offset ) / 2 );
  for( auto i = offset; i < value.size(); i += 2 )
  {
    bytes.push_back( static_cast< char >( ( hex_nibble( value[ i ] ) << 4 ) | hex_nibble( value[ i + 1 ] ) ) );
  }
  return bytes;
}

ConfigCheckpoint parse_checkpoint( const std::string& value )
{
  const auto separator = value.find( ':' );
  if( separator == std::string::npos || separator == 0 || separator + 1 >= value.size() )
    throw std::runtime_error( "p2p.checkpoint must be in height:block_id form" );

  std::size_t parsed = 0;
  uint64_t height = 0;
  try
  {
    height = std::stoull( value.substr( 0, separator ), &parsed, 10 );
  }
  catch( const std::exception& e )
  {
    throw std::runtime_error( "invalid p2p.checkpoint height '" + value.substr( 0, separator ) + "': " + e.what() );
  }

  if( parsed != separator )
    throw std::runtime_error( "invalid p2p.checkpoint height '" + value.substr( 0, separator ) + "'" );

  ConfigCheckpoint checkpoint;
  checkpoint.block_height = height;
  try
  {
    checkpoint.block_id = decode_hex_bytes( value.substr( separator + 1 ) );
  }
  catch( const std::exception& e )
  {
    throw std::runtime_error( "invalid p2p.checkpoint block id: " + std::string( e.what() ) );
  }
  return checkpoint;
}

ConfigCheckpoint parse_checkpoint_map( const YAML::Node& value )
{
  if( value.size() != 1 )
    throw std::runtime_error( "p2p.checkpoint map entries must contain exactly one height:block_id pair" );

  auto it = value.begin();
  return parse_checkpoint( it->first.as< std::string >() + ":" + it->second.as< std::string >() );
}

std::vector< ConfigCheckpoint > yaml_get_checkpoints( const YAML::Node& node, const std::string& key )
{
  std::vector< ConfigCheckpoint > checkpoints;
  auto value = yaml_child( node, key );
  if( !value.IsDefined() || value.IsNull() )
    return checkpoints;

  if( value.IsScalar() )
  {
    checkpoints.push_back( parse_checkpoint( value.as< std::string >() ) );
    return checkpoints;
  }

  if( value.IsMap() )
  {
    for( auto it = value.begin(); it != value.end(); ++it )
      checkpoints.push_back( parse_checkpoint( it->first.as< std::string >() + ":" + it->second.as< std::string >() ) );
    return checkpoints;
  }

  if( !value.IsSequence() )
    throw std::runtime_error( "p2p.checkpoint must be a string or string list" );

  for( const auto& item: value )
  {
    if( item.IsScalar() )
      checkpoints.push_back( parse_checkpoint( item.as< std::string >() ) );
    else if( item.IsMap() )
      checkpoints.push_back( parse_checkpoint_map( item ) );
    else
      throw std::runtime_error( "p2p.checkpoint entries must be strings or height:block_id maps" );
  }
  return checkpoints;
}

} // anonymous namespace

NodeConfig load_config( const std::filesystem::path& config_path,
                        const std::vector< std::string >& cli_enables,
                        const std::vector< std::string >& cli_disables, bool strict )
{
  NodeConfig cfg;

  if( std::filesystem::exists( config_path ) )
  {
    YAML::Node root = YAML::LoadFile( config_path.string() );
    if( strict )
    {
      if( !root.IsMap() )
        throw std::invalid_argument( "Configuration must be a YAML mapping" );
      for( const auto& entry: root )
      {
        if( !entry.second.IsMap() )
          throw std::invalid_argument( "Configuration sections must be mappings" );
      }
      const YAML::Node& document = root;
      for( const auto& field: std::vector< std::pair< std::string, std::string > >{
             { "global", "blacklist" }, { "global", "whitelist" },
             { "p2p", "peer" }, { "p2p", "seed" }, { "block_producer", "approve-proposals" } } )
      {
        const auto section = document[ field.first ];
        if( !section ) continue;
        const auto value = section[ field.second ];
        if( !value ) continue;
        if( field.second == "seed" && value.IsScalar() ) continue;
        if( !value.IsSequence() ) throw std::invalid_argument( "Expected a configuration list" );
        for( const auto& item: value )
          if( !item.IsScalar() ) throw std::invalid_argument( "Expected scalar list entries" );
      }
      const auto backup = document[ "backup" ];
      if( backup )
        for( const char* section: { "schedule", "local", "ssh", "remote", "public-restore", "public-publish", "admin" } )
          if( backup[ section ] && !backup[ section ].IsMap() )
            throw std::invalid_argument( "Backup sections must be mappings" );
    }

    // ── Global ──
    if( auto g = yaml_child( root, "global" ); g.IsDefined() )
    {
      cfg.log_level      = yaml_get< std::string >( g, "log-level", cfg.log_level, strict );
      cfg.instance_id    = yaml_get< std::string >( g, "instance-id", cfg.instance_id, strict );
      cfg.fork_algorithm = yaml_get< std::string >( g, "fork-algorithm", cfg.fork_algorithm, strict );
      cfg.log_color      = yaml_get< bool >( g, "log-color", cfg.log_color, strict );
      cfg.log_datetime   = yaml_get< bool >( g, "log-datetime", cfg.log_datetime, strict );
      cfg.reset          = yaml_get< bool >( g, "reset", cfg.reset, strict );
      cfg.rpc_blacklist  = yaml_get_string_list( g, "blacklist" );
      cfg.rpc_whitelist  = yaml_get_string_list( g, "whitelist" );

      if( yaml_child( g, "jobs" ).IsDefined() )
        cfg.chain_jobs = yaml_get< uint64_t >( g, "jobs", cfg.chain_jobs, strict );
    }

    // ── Chain ──
    if( auto c = yaml_child( root, "chain" ); c.IsDefined() )
    {
      cfg.chain_jobs                    = yaml_get< uint64_t >( c, "jobs", cfg.chain_jobs, strict );
      cfg.verify_blocks                 = yaml_get< bool >( c, "verify-blocks", cfg.verify_blocks, strict );
      cfg.read_compute_bandwidth_limit  = yaml_get< uint64_t >( c, "read-compute-bandwidth-limit", cfg.read_compute_bandwidth_limit, strict );
      cfg.pending_transaction_limit     = yaml_get< uint64_t >( c, "pending-transaction-limit", cfg.pending_transaction_limit, strict );
      cfg.disable_pending_transaction_limit = yaml_get< bool >( c, "disable-pending-transaction-limit", cfg.disable_pending_transaction_limit, strict );
      // Capacity policy must never silently become unlimited after a typo.
      cfg.transaction_work_max_jobs = yaml_get< uint64_t >( c, "transaction-work-max-jobs", cfg.transaction_work_max_jobs, true );
      cfg.transaction_work_max_wire_bytes = yaml_get< uint64_t >( c, "transaction-work-max-wire-bytes", cfg.transaction_work_max_wire_bytes, true );
      cfg.vm_module_cache_max_capacity_bytes = yaml_get< uint64_t >( c, "vm-module-cache-max-capacity-bytes", cfg.vm_module_cache_max_capacity_bytes, true );
    }

    // ── P2P ──
    if( auto p = yaml_child( root, "p2p" ); p.IsDefined() )
    {
      cfg.p2p_listen = yaml_get< std::string >( p, "listen", cfg.p2p_listen, strict );
      cfg.p2p_jobs   = yaml_get< uint64_t >( p, "jobs", cfg.p2p_jobs, strict );
      cfg.p2p_seed_reconnect_interval_seconds =
        yaml_get< uint64_t >( p, "seed-reconnect-interval-seconds", cfg.p2p_seed_reconnect_interval_seconds, strict );
      cfg.p2p_peer_discovery_enabled = yaml_get< bool >( p, "peer-discovery", cfg.p2p_peer_discovery_enabled, strict );
      cfg.p2p_target_peer_count = yaml_get< uint64_t >( p, "target-peer-count", cfg.p2p_target_peer_count, strict );
      cfg.p2p_max_peer_candidates = yaml_get< uint64_t >( p, "max-peer-candidates", cfg.p2p_max_peer_candidates, strict );
      cfg.p2p_max_peer_error_records = yaml_get< uint64_t >( p, "max-peer-error-records", cfg.p2p_max_peer_error_records, true );
      cfg.p2p_peer_error_key_capacity_bytes = yaml_get< uint64_t >( p, "peer-error-key-capacity-bytes", cfg.p2p_peer_error_key_capacity_bytes, true );
      if( !cfg.p2p_max_peer_error_records || !cfg.p2p_peer_error_key_capacity_bytes )
        throw std::runtime_error( "Peer error record and key-capacity limits must be positive" );
      cfg.p2p_max_candidate_dials_per_cycle =
        yaml_get< uint64_t >( p, "max-candidate-dials-per-cycle", cfg.p2p_max_candidate_dials_per_cycle, strict );
      cfg.p2p_peer_acquisition_interval_seconds =
        yaml_get< uint64_t >( p, "peer-acquisition-interval-seconds", cfg.p2p_peer_acquisition_interval_seconds, strict );
      cfg.p2p_candidate_redial_interval_seconds =
        yaml_get< uint64_t >( p, "candidate-redial-interval-seconds", cfg.p2p_candidate_redial_interval_seconds, strict );
      cfg.p2p_peer_log_interval_seconds =
        yaml_get< uint64_t >( p, "peer-log-interval-seconds", cfg.p2p_peer_log_interval_seconds, strict );
      cfg.p2p_force_gossip = yaml_get< bool >( p, "force-gossip", cfg.p2p_force_gossip, strict );
      cfg.p2p_disable_gossip = yaml_get< bool >( p, "disable-gossip", cfg.p2p_disable_gossip, strict );
      cfg.p2p_identity_seed = yaml_get_string_scalar( p, "identity-seed", cfg.p2p_identity_seed );
      cfg.p2p_checkpoints = yaml_get_checkpoints( p, "checkpoint" );

      if( auto seed = yaml_child( p, "seed" ); seed.IsDefined() )
      {
        if( seed.IsScalar() )
          cfg.p2p_identity_seed = seed.as< std::string >();
        else if( seed.IsSequence() )
        {
          auto seeds = yaml_get_string_list( p, "seed" );
          if( !seeds.empty() )
            cfg.p2p_seeds = std::move( seeds );
        }
      }

      auto peers = yaml_get_string_list( p, "peer" );
      if( !peers.empty() )
        cfg.p2p_seeds.insert( cfg.p2p_seeds.end(), peers.begin(), peers.end() );
    }

    // ── JSON-RPC ──
    if( auto j = yaml_child( root, "jsonrpc" ); j.IsDefined() )
    {
      cfg.jsonrpc_listen = yaml_get< std::string >( j, "listen", cfg.jsonrpc_listen, strict );
      cfg.jsonrpc_jobs   = yaml_get< uint64_t >( j, "jobs", cfg.jsonrpc_jobs, strict );
      cfg.jsonrpc_io_timeout_seconds = yaml_get< uint64_t >( j, "io-timeout-seconds", cfg.jsonrpc_io_timeout_seconds, strict );
      if( cfg.jsonrpc_jobs > 64 || cfg.jsonrpc_io_timeout_seconds == 0 || cfg.jsonrpc_io_timeout_seconds > 300 )
        throw std::invalid_argument( "Invalid JSON-RPC jobs or io-timeout-seconds" );
    }

    // ── gRPC ──
    if( auto g = yaml_child( root, "grpc" ); g.IsDefined() )
    {
      cfg.grpc_listen = yaml_get< std::string >( g, "listen", cfg.grpc_listen, strict );
      cfg.grpc_listen = yaml_get< std::string >( g, "endpoint", cfg.grpc_listen, strict );
      cfg.grpc_jobs   = yaml_get< uint64_t >( g, "jobs", cfg.grpc_jobs, strict );
    }

    // ── Block Producer ──
    if( auto bp = yaml_child( root, "block_producer" ); bp.IsDefined() )
    {
      cfg.block_producer_algorithm         = yaml_get< std::string >( bp, "algorithm", cfg.block_producer_algorithm, strict );
      cfg.block_producer_private_key_file  = yaml_get< std::string >( bp, "private-key-file", cfg.block_producer_private_key_file, strict );
      cfg.block_producer_address           = yaml_get< std::string >( bp, "producer", cfg.block_producer_address, strict );
      cfg.block_producer_resources_lower_bound = yaml_get< uint64_t >( bp, "resources-lower-bound", cfg.block_producer_resources_lower_bound, strict );
      cfg.block_producer_resources_upper_bound = yaml_get< uint64_t >( bp, "resources-upper-bound", cfg.block_producer_resources_upper_bound, strict );
      cfg.block_producer_max_inclusion_attempts =
        yaml_get< uint64_t >( bp, "max-inclusion-attempts", cfg.block_producer_max_inclusion_attempts, strict );
      cfg.block_producer_gossip_production =
        yaml_get< bool >( bp, "gossip-production", cfg.block_producer_gossip_production, strict );
      cfg.block_producer_approved_proposals = yaml_get_string_list( bp, "approve-proposals" );
    }

    // ── Mempool ──
    if( auto m = yaml_child( root, "mempool" ); m.IsDefined() )
    {
      cfg.mempool_transaction_expiration = yaml_get< uint64_t >( m, "transaction-expiration", cfg.mempool_transaction_expiration, strict );
      cfg.mempool_pending_record_max_owners = yaml_get< uint64_t >( m, "pending-record-max-owners", cfg.mempool_pending_record_max_owners, true );
      cfg.mempool_pending_record_max_capacity_bytes = yaml_get< uint64_t >( m, "pending-record-max-capacity-bytes", cfg.mempool_pending_record_max_capacity_bytes, true );
      cfg.mempool_aggregate_max_accounted_bytes = yaml_get< uint64_t >( m, "aggregate-max-accounted-bytes", cfg.mempool_aggregate_max_accounted_bytes, true );
    }

    // ── RocksDB ──
    if( auto r = yaml_child( root, "rocksdb" ); r.IsDefined() )
    {
      cfg.rocksdb_block_cache_mb           = yaml_get< uint64_t >( r, "block-cache-mb", cfg.rocksdb_block_cache_mb, strict );
      cfg.rocksdb_max_background_jobs      = yaml_get< uint64_t >( r, "max-background-jobs", cfg.rocksdb_max_background_jobs, strict );
      cfg.rocksdb_bytes_per_sync           = yaml_get< uint64_t >( r, "bytes-per-sync", cfg.rocksdb_bytes_per_sync, strict );
      cfg.rocksdb_default_block_size       = yaml_get< uint64_t >( r, "default-block-size", cfg.rocksdb_default_block_size, strict );
      cfg.rocksdb_blocks_block_size        = yaml_get< uint64_t >( r, "blocks-block-size", cfg.rocksdb_blocks_block_size, strict );
      cfg.rocksdb_target_file_size_base    = yaml_get< uint64_t >( r, "target-file-size-base", cfg.rocksdb_target_file_size_base, strict );
      cfg.rocksdb_max_bytes_for_level_base = yaml_get< uint64_t >( r, "max-bytes-for-level-base", cfg.rocksdb_max_bytes_for_level_base, strict );
      cfg.rocksdb_write_buffer_size        = yaml_get< uint64_t >( r, "write-buffer-size", cfg.rocksdb_write_buffer_size, strict );
      cfg.rocksdb_db_write_buffer_size     = yaml_get< uint64_t >( r, "db-write-buffer-size", cfg.rocksdb_db_write_buffer_size, strict );
      cfg.rocksdb_max_total_wal_size       = yaml_get< uint64_t >( r, "max-total-wal-size", cfg.rocksdb_max_total_wal_size, true );
      cfg.rocksdb_max_write_buffer_number  = yaml_get< uint64_t >( r, "max-write-buffer-number", cfg.rocksdb_max_write_buffer_number, strict );
      cfg.rocksdb_compression              = yaml_get< std::string >( r, "compression", cfg.rocksdb_compression, strict );
      cfg.rocksdb_blocks_compression       = yaml_get< std::string >( r, "blocks-compression", cfg.rocksdb_blocks_compression, strict );
      cfg.rocksdb_require_compression      = yaml_get< bool >( r, "require-compression", cfg.rocksdb_require_compression, strict );
      cfg.rocksdb_blocks_zstd_level = yaml_get< uint64_t >( r, "blocks-zstd-level", cfg.rocksdb_blocks_zstd_level, true );
      cfg.rocksdb_blocks_zstd_dictionary_bytes = yaml_get< uint64_t >( r, "blocks-zstd-dictionary-bytes", cfg.rocksdb_blocks_zstd_dictionary_bytes, true );
    }

    // ── Backup ──
    if( auto b = yaml_child( root, "backup" ); b.IsDefined() )
    {
      cfg.backup.enabled = yaml_get< bool >( b, "enabled", cfg.backup.enabled, strict );
      cfg.backup.node_id = yaml_get< std::string >( b, "node-id", cfg.backup.node_id, strict );
      cfg.backup.workspace = yaml_get< std::string >( b, "workspace", cfg.backup.workspace, strict );

      if( auto schedule = yaml_child( b, "schedule" ); schedule.IsDefined() )
      {
        cfg.backup.schedule.enabled =
          yaml_get< bool >( schedule, "enabled", cfg.backup.schedule.enabled, strict );
        cfg.backup.schedule.interval =
          yaml_get< std::string >( schedule, "interval", cfg.backup.schedule.interval, strict );
        cfg.backup.schedule.run_on_startup_if_missed =
          yaml_get< bool >( schedule, "run-on-startup-if-missed", cfg.backup.schedule.run_on_startup_if_missed, strict );
        cfg.backup.schedule.jitter_seconds =
          yaml_get< uint64_t >( schedule, "jitter-seconds", cfg.backup.schedule.jitter_seconds, strict );
        cfg.backup.schedule.minimum_head_progress =
          yaml_get< uint64_t >( schedule, "minimum-head-progress", cfg.backup.schedule.minimum_head_progress, strict );
        cfg.backup.schedule.skip_if_syncing_from_genesis =
          yaml_get< bool >( schedule, "skip-if-syncing-from-genesis", cfg.backup.schedule.skip_if_syncing_from_genesis, strict );
        cfg.backup.schedule.max_concurrent_backups =
          yaml_get< uint64_t >( schedule, "max-concurrent-backups", cfg.backup.schedule.max_concurrent_backups, strict );
      }

      if( auto local = yaml_child( b, "local" ); local.IsDefined() )
      {
        cfg.backup.local.enabled =
          yaml_get< bool >( local, "enabled", cfg.backup.local.enabled, strict );
        cfg.backup.local.directory =
          yaml_get< std::string >( local, "directory", cfg.backup.local.directory, strict );
        cfg.backup.local.retention_count =
          yaml_get< uint64_t >( local, "retention-count", cfg.backup.local.retention_count, strict );
      }

      if( auto ssh = yaml_child( b, "ssh" ); ssh.IsDefined() )
      {
        cfg.backup.ssh.enabled =
          yaml_get< bool >( ssh, "enabled", cfg.backup.ssh.enabled, strict );
        cfg.backup.ssh.transport =
          yaml_get< std::string >( ssh, "transport", cfg.backup.ssh.transport, strict );
        cfg.backup.ssh.host =
          yaml_get< std::string >( ssh, "host", cfg.backup.ssh.host, strict );
        cfg.backup.ssh.port =
          yaml_get< uint64_t >( ssh, "port", cfg.backup.ssh.port, strict );
        cfg.backup.ssh.user =
          yaml_get< std::string >( ssh, "user", cfg.backup.ssh.user, strict );
        cfg.backup.ssh.auth =
          yaml_get< std::string >( ssh, "auth", cfg.backup.ssh.auth, strict );
        cfg.backup.ssh.password_file =
          yaml_get< std::string >( ssh, "password-file", cfg.backup.ssh.password_file, strict );
        cfg.backup.ssh.private_key_file =
          yaml_get< std::string >( ssh, "private-key-file", cfg.backup.ssh.private_key_file, strict );
        cfg.backup.ssh.passphrase_file =
          yaml_get< std::string >( ssh, "passphrase-file", cfg.backup.ssh.passphrase_file, strict );
        cfg.backup.ssh.known_hosts_file =
          yaml_get< std::string >( ssh, "known-hosts-file", cfg.backup.ssh.known_hosts_file, strict );
        cfg.backup.ssh.strict_host_key_checking =
          yaml_get< bool >( ssh, "strict-host-key-checking", cfg.backup.ssh.strict_host_key_checking, strict );
        cfg.backup.ssh.connect_timeout_seconds =
          yaml_get< uint64_t >( ssh, "connect-timeout-seconds", cfg.backup.ssh.connect_timeout_seconds, strict );
      }

      if( auto remote = yaml_child( b, "remote" ); remote.IsDefined() )
      {
        cfg.backup.remote.enabled =
          yaml_get< bool >( remote, "enabled", cfg.backup.remote.enabled, strict );
        cfg.backup.remote.directory =
          yaml_get< std::string >( remote, "directory", cfg.backup.remote.directory, strict );
        cfg.backup.remote.retention_count =
          yaml_get< uint64_t >( remote, "retention-count", cfg.backup.remote.retention_count, strict );
        cfg.backup.remote.retention_days =
          yaml_get< uint64_t >( remote, "retention-days", cfg.backup.remote.retention_days, strict );
        cfg.backup.remote.upload_temp_suffix =
          yaml_get< std::string >( remote, "upload-temp-suffix", cfg.backup.remote.upload_temp_suffix, strict );
      }

      if( auto public_restore = yaml_child( b, "public-restore" ); public_restore.IsDefined() )
      {
        cfg.backup.public_restore.enabled =
          yaml_get< bool >( public_restore, "enabled", cfg.backup.public_restore.enabled, strict );
        cfg.backup.public_restore.base_url =
          yaml_get< std::string >( public_restore, "base-url", cfg.backup.public_restore.base_url, strict );
        cfg.backup.public_restore.network =
          yaml_get< std::string >( public_restore, "network", cfg.backup.public_restore.network, strict );
        cfg.backup.public_restore.require_https =
          yaml_get< bool >( public_restore, "require-https", cfg.backup.public_restore.require_https, strict );
        cfg.backup.public_restore.timeout_seconds =
          yaml_get< uint64_t >( public_restore, "timeout-seconds", cfg.backup.public_restore.timeout_seconds, strict );
        cfg.backup.public_restore.retries =
          yaml_get< uint64_t >( public_restore, "retries", cfg.backup.public_restore.retries, strict );
        cfg.backup.public_restore.signature_required =
          yaml_get< bool >( public_restore, "signature-required", cfg.backup.public_restore.signature_required, strict );
        cfg.backup.public_restore.signature_public_key_file =
          yaml_get< std::string >( public_restore, "signature-public-key-file", cfg.backup.public_restore.signature_public_key_file, strict );
      }

      if( auto public_publish = yaml_child( b, "public-publish" ); public_publish.IsDefined() )
      {
        cfg.backup.public_publish.enabled =
          yaml_get< bool >( public_publish, "enabled", cfg.backup.public_publish.enabled, strict );
        cfg.backup.public_publish.directory =
          yaml_get< std::string >( public_publish, "directory", cfg.backup.public_publish.directory, strict );
        cfg.backup.public_publish.base_url =
          yaml_get< std::string >( public_publish, "base-url", cfg.backup.public_publish.base_url, strict );
        cfg.backup.public_publish.network =
          yaml_get< std::string >( public_publish, "network", cfg.backup.public_publish.network, strict );
        cfg.backup.public_publish.observer_config_file =
          yaml_get< std::string >( public_publish, "observer-config-file", cfg.backup.public_publish.observer_config_file, strict );
        cfg.backup.public_publish.retention_count =
          yaml_get< uint64_t >( public_publish, "retention-count", cfg.backup.public_publish.retention_count, strict );
        cfg.backup.public_publish.upload_temp_suffix =
          yaml_get< std::string >( public_publish, "upload-temp-suffix", cfg.backup.public_publish.upload_temp_suffix, strict );
      }

      if( auto admin = yaml_child( b, "admin" ); admin.IsDefined() )
      {
        cfg.backup.admin.enabled =
          yaml_get< bool >( admin, "enabled", cfg.backup.admin.enabled, strict );
        cfg.backup.admin.listen =
          yaml_get< std::string >( admin, "listen", cfg.backup.admin.listen, strict );
        cfg.backup.admin.token_file =
          yaml_get< std::string >( admin, "token-file", cfg.backup.admin.token_file, strict );
        cfg.backup.admin.jobs =
          yaml_get< uint64_t >( admin, "jobs", cfg.backup.admin.jobs, strict );
      }
    }

    // ── Feature flags ──
    if( auto f = yaml_child( root, "features" ); f.IsDefined() )
    {
      for( auto it = f.begin(); it != f.end(); ++it )
      {
        try
        {
          const auto name = it->first.as< std::string >();
          if( strict && cfg.features.count( name ) == 0 )
            throw std::runtime_error( "Unknown feature" );
          cfg.features[ name ] = it->second.as< bool >();
        }
        catch( ... )
        {
          if( strict ) throw std::invalid_argument( "Invalid feature configuration" );
        }
      }
    }
  }

  if( strict )
    for( const auto& group: { cli_enables, cli_disables } )
      for( const auto& name: group )
        if( cfg.features.count( name ) == 0 )
          throw std::invalid_argument( "Unknown CLI feature" );

  // Apply CLI overrides
  for( const auto& comp: cli_enables )
    cfg.features[ comp ] = true;
  for( const auto& comp: cli_disables )
    cfg.features[ comp ] = false;

  validate_rocksdb_blocks_zstd_options( cfg );
  return cfg;
}

void validate_rocksdb_blocks_zstd_options( const NodeConfig& cfg )
{
  if( cfg.rocksdb_blocks_zstd_level > 19 )
    throw std::invalid_argument( "Invalid rocksdb.blocks-zstd-level: expected 0..19" );
  const auto dictionary = cfg.rocksdb_blocks_zstd_dictionary_bytes;
  if( dictionary != 0 && dictionary != 32768 && dictionary != 65536 && dictionary != 131072 )
    throw std::invalid_argument( "Invalid rocksdb.blocks-zstd-dictionary-bytes: expected 0, 32768, 65536 or 131072" );
  if( cfg.rocksdb_blocks_zstd_level == 0 && dictionary == 0 )
    return;

  auto codec = cfg.rocksdb_blocks_compression.empty() ? cfg.rocksdb_compression : cfg.rocksdb_blocks_compression;
  if( codec.empty() )
    codec = "zstd";
  std::transform( codec.begin(), codec.end(), codec.begin(),
                  []( unsigned char ch ) { return static_cast< char >( std::tolower( ch ) ); } );
  if( codec != "zstd" && codec != "kzstd" && codec != "kzstdcompression" )
    throw std::invalid_argument( "Blocks Zstd options require rocksdb.blocks-compression: zstd" );
}

} // namespace koinos::node
