#include "config.hpp"

#include <stdexcept>

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
T yaml_get( const YAML::Node& node, const std::string& key, const T& fallback )
{
  auto value = yaml_child( node, key );
  if( value.IsDefined() )
  {
    try
    {
      return value.as< T >();
    }
    catch( ... )
    {}
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
                        const std::vector< std::string >& cli_disables )
{
  NodeConfig cfg;

  if( std::filesystem::exists( config_path ) )
  {
    YAML::Node root = YAML::LoadFile( config_path.string() );

    // ── Global ──
    if( auto g = yaml_child( root, "global" ); g.IsDefined() )
    {
      cfg.log_level      = yaml_get< std::string >( g, "log-level", cfg.log_level );
      cfg.instance_id    = yaml_get< std::string >( g, "instance-id", cfg.instance_id );
      cfg.fork_algorithm = yaml_get< std::string >( g, "fork-algorithm", cfg.fork_algorithm );
      cfg.log_color      = yaml_get< bool >( g, "log-color", cfg.log_color );
      cfg.log_datetime   = yaml_get< bool >( g, "log-datetime", cfg.log_datetime );
      cfg.reset          = yaml_get< bool >( g, "reset", cfg.reset );
      cfg.rpc_blacklist  = yaml_get_string_list( g, "blacklist" );
      cfg.rpc_whitelist  = yaml_get_string_list( g, "whitelist" );

      if( yaml_child( g, "jobs" ).IsDefined() )
        cfg.chain_jobs = yaml_get< uint64_t >( g, "jobs", cfg.chain_jobs );
    }

    // ── Chain ──
    if( auto c = yaml_child( root, "chain" ); c.IsDefined() )
    {
      cfg.chain_jobs                    = yaml_get< uint64_t >( c, "jobs", cfg.chain_jobs );
      cfg.verify_blocks                 = yaml_get< bool >( c, "verify-blocks", cfg.verify_blocks );
      cfg.read_compute_bandwidth_limit  = yaml_get< uint64_t >( c, "read-compute-bandwidth-limit", cfg.read_compute_bandwidth_limit );
      cfg.pending_transaction_limit     = yaml_get< uint64_t >( c, "pending-transaction-limit", cfg.pending_transaction_limit );
      cfg.disable_pending_transaction_limit = yaml_get< bool >( c, "disable-pending-transaction-limit", cfg.disable_pending_transaction_limit );
    }

    // ── P2P ──
    if( auto p = yaml_child( root, "p2p" ); p.IsDefined() )
    {
      cfg.p2p_listen = yaml_get< std::string >( p, "listen", cfg.p2p_listen );
      cfg.p2p_jobs   = yaml_get< uint64_t >( p, "jobs", cfg.p2p_jobs );
      cfg.p2p_seed_reconnect_interval_seconds =
        yaml_get< uint64_t >( p, "seed-reconnect-interval-seconds", cfg.p2p_seed_reconnect_interval_seconds );
      cfg.p2p_peer_discovery_enabled = yaml_get< bool >( p, "peer-discovery", cfg.p2p_peer_discovery_enabled );
      cfg.p2p_target_peer_count = yaml_get< uint64_t >( p, "target-peer-count", cfg.p2p_target_peer_count );
      cfg.p2p_max_peer_candidates = yaml_get< uint64_t >( p, "max-peer-candidates", cfg.p2p_max_peer_candidates );
      cfg.p2p_max_candidate_dials_per_cycle =
        yaml_get< uint64_t >( p, "max-candidate-dials-per-cycle", cfg.p2p_max_candidate_dials_per_cycle );
      cfg.p2p_peer_acquisition_interval_seconds =
        yaml_get< uint64_t >( p, "peer-acquisition-interval-seconds", cfg.p2p_peer_acquisition_interval_seconds );
      cfg.p2p_candidate_redial_interval_seconds =
        yaml_get< uint64_t >( p, "candidate-redial-interval-seconds", cfg.p2p_candidate_redial_interval_seconds );
      cfg.p2p_peer_log_interval_seconds =
        yaml_get< uint64_t >( p, "peer-log-interval-seconds", cfg.p2p_peer_log_interval_seconds );
      cfg.p2p_force_gossip = yaml_get< bool >( p, "force-gossip", cfg.p2p_force_gossip );
      cfg.p2p_disable_gossip = yaml_get< bool >( p, "disable-gossip", cfg.p2p_disable_gossip );
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
      cfg.jsonrpc_listen = yaml_get< std::string >( j, "listen", cfg.jsonrpc_listen );
      cfg.jsonrpc_jobs   = yaml_get< uint64_t >( j, "jobs", cfg.jsonrpc_jobs );
    }

    // ── gRPC ──
    if( auto g = yaml_child( root, "grpc" ); g.IsDefined() )
    {
      cfg.grpc_listen = yaml_get< std::string >( g, "listen", cfg.grpc_listen );
      cfg.grpc_listen = yaml_get< std::string >( g, "endpoint", cfg.grpc_listen );
      cfg.grpc_jobs   = yaml_get< uint64_t >( g, "jobs", cfg.grpc_jobs );
    }

    // ── Block Producer ──
    if( auto bp = yaml_child( root, "block_producer" ); bp.IsDefined() )
    {
      cfg.block_producer_algorithm         = yaml_get< std::string >( bp, "algorithm", cfg.block_producer_algorithm );
      cfg.block_producer_private_key_file  = yaml_get< std::string >( bp, "private-key-file", cfg.block_producer_private_key_file );
      cfg.block_producer_address           = yaml_get< std::string >( bp, "producer", cfg.block_producer_address );
      cfg.block_producer_resources_lower_bound = yaml_get< uint64_t >( bp, "resources-lower-bound", cfg.block_producer_resources_lower_bound );
      cfg.block_producer_resources_upper_bound = yaml_get< uint64_t >( bp, "resources-upper-bound", cfg.block_producer_resources_upper_bound );
      cfg.block_producer_max_inclusion_attempts =
        yaml_get< uint64_t >( bp, "max-inclusion-attempts", cfg.block_producer_max_inclusion_attempts );
      cfg.block_producer_gossip_production =
        yaml_get< bool >( bp, "gossip-production", cfg.block_producer_gossip_production );
      cfg.block_producer_approved_proposals = yaml_get_string_list( bp, "approve-proposals" );
    }

    // ── Mempool ──
    if( auto m = yaml_child( root, "mempool" ); m.IsDefined() )
    {
      cfg.mempool_transaction_expiration = yaml_get< uint64_t >( m, "transaction-expiration", cfg.mempool_transaction_expiration );
    }

    // ── RocksDB ──
    if( auto r = yaml_child( root, "rocksdb" ); r.IsDefined() )
    {
      cfg.rocksdb_block_cache_mb           = yaml_get< uint64_t >( r, "block-cache-mb", cfg.rocksdb_block_cache_mb );
      cfg.rocksdb_max_background_jobs      = yaml_get< uint64_t >( r, "max-background-jobs", cfg.rocksdb_max_background_jobs );
      cfg.rocksdb_bytes_per_sync           = yaml_get< uint64_t >( r, "bytes-per-sync", cfg.rocksdb_bytes_per_sync );
      cfg.rocksdb_default_block_size       = yaml_get< uint64_t >( r, "default-block-size", cfg.rocksdb_default_block_size );
      cfg.rocksdb_blocks_block_size        = yaml_get< uint64_t >( r, "blocks-block-size", cfg.rocksdb_blocks_block_size );
      cfg.rocksdb_target_file_size_base    = yaml_get< uint64_t >( r, "target-file-size-base", cfg.rocksdb_target_file_size_base );
      cfg.rocksdb_max_bytes_for_level_base = yaml_get< uint64_t >( r, "max-bytes-for-level-base", cfg.rocksdb_max_bytes_for_level_base );
      cfg.rocksdb_write_buffer_size        = yaml_get< uint64_t >( r, "write-buffer-size", cfg.rocksdb_write_buffer_size );
      cfg.rocksdb_db_write_buffer_size     = yaml_get< uint64_t >( r, "db-write-buffer-size", cfg.rocksdb_db_write_buffer_size );
      cfg.rocksdb_max_write_buffer_number  = yaml_get< uint64_t >( r, "max-write-buffer-number", cfg.rocksdb_max_write_buffer_number );
      cfg.rocksdb_compression              = yaml_get< std::string >( r, "compression", cfg.rocksdb_compression );
      cfg.rocksdb_blocks_compression       = yaml_get< std::string >( r, "blocks-compression", cfg.rocksdb_blocks_compression );
      cfg.rocksdb_require_compression      = yaml_get< bool >( r, "require-compression", cfg.rocksdb_require_compression );
    }

    // ── Backup ──
    if( auto b = yaml_child( root, "backup" ); b.IsDefined() )
    {
      cfg.backup.enabled = yaml_get< bool >( b, "enabled", cfg.backup.enabled );
      cfg.backup.node_id = yaml_get< std::string >( b, "node-id", cfg.backup.node_id );
      cfg.backup.workspace = yaml_get< std::string >( b, "workspace", cfg.backup.workspace );

      if( auto schedule = yaml_child( b, "schedule" ); schedule.IsDefined() )
      {
        cfg.backup.schedule.enabled =
          yaml_get< bool >( schedule, "enabled", cfg.backup.schedule.enabled );
        cfg.backup.schedule.interval =
          yaml_get< std::string >( schedule, "interval", cfg.backup.schedule.interval );
        cfg.backup.schedule.run_on_startup_if_missed =
          yaml_get< bool >( schedule, "run-on-startup-if-missed", cfg.backup.schedule.run_on_startup_if_missed );
        cfg.backup.schedule.jitter_seconds =
          yaml_get< uint64_t >( schedule, "jitter-seconds", cfg.backup.schedule.jitter_seconds );
        cfg.backup.schedule.minimum_head_progress =
          yaml_get< uint64_t >( schedule, "minimum-head-progress", cfg.backup.schedule.minimum_head_progress );
        cfg.backup.schedule.skip_if_syncing_from_genesis =
          yaml_get< bool >( schedule, "skip-if-syncing-from-genesis", cfg.backup.schedule.skip_if_syncing_from_genesis );
        cfg.backup.schedule.max_concurrent_backups =
          yaml_get< uint64_t >( schedule, "max-concurrent-backups", cfg.backup.schedule.max_concurrent_backups );
      }

      if( auto local = yaml_child( b, "local" ); local.IsDefined() )
      {
        cfg.backup.local.enabled =
          yaml_get< bool >( local, "enabled", cfg.backup.local.enabled );
        cfg.backup.local.directory =
          yaml_get< std::string >( local, "directory", cfg.backup.local.directory );
        cfg.backup.local.retention_count =
          yaml_get< uint64_t >( local, "retention-count", cfg.backup.local.retention_count );
      }

      if( auto ssh = yaml_child( b, "ssh" ); ssh.IsDefined() )
      {
        cfg.backup.ssh.enabled =
          yaml_get< bool >( ssh, "enabled", cfg.backup.ssh.enabled );
        cfg.backup.ssh.transport =
          yaml_get< std::string >( ssh, "transport", cfg.backup.ssh.transport );
        cfg.backup.ssh.host =
          yaml_get< std::string >( ssh, "host", cfg.backup.ssh.host );
        cfg.backup.ssh.port =
          yaml_get< uint64_t >( ssh, "port", cfg.backup.ssh.port );
        cfg.backup.ssh.user =
          yaml_get< std::string >( ssh, "user", cfg.backup.ssh.user );
        cfg.backup.ssh.auth =
          yaml_get< std::string >( ssh, "auth", cfg.backup.ssh.auth );
        cfg.backup.ssh.password_file =
          yaml_get< std::string >( ssh, "password-file", cfg.backup.ssh.password_file );
        cfg.backup.ssh.private_key_file =
          yaml_get< std::string >( ssh, "private-key-file", cfg.backup.ssh.private_key_file );
        cfg.backup.ssh.passphrase_file =
          yaml_get< std::string >( ssh, "passphrase-file", cfg.backup.ssh.passphrase_file );
        cfg.backup.ssh.known_hosts_file =
          yaml_get< std::string >( ssh, "known-hosts-file", cfg.backup.ssh.known_hosts_file );
        cfg.backup.ssh.strict_host_key_checking =
          yaml_get< bool >( ssh, "strict-host-key-checking", cfg.backup.ssh.strict_host_key_checking );
        cfg.backup.ssh.connect_timeout_seconds =
          yaml_get< uint64_t >( ssh, "connect-timeout-seconds", cfg.backup.ssh.connect_timeout_seconds );
      }

      if( auto remote = yaml_child( b, "remote" ); remote.IsDefined() )
      {
        cfg.backup.remote.enabled =
          yaml_get< bool >( remote, "enabled", cfg.backup.remote.enabled );
        cfg.backup.remote.directory =
          yaml_get< std::string >( remote, "directory", cfg.backup.remote.directory );
        cfg.backup.remote.retention_count =
          yaml_get< uint64_t >( remote, "retention-count", cfg.backup.remote.retention_count );
        cfg.backup.remote.retention_days =
          yaml_get< uint64_t >( remote, "retention-days", cfg.backup.remote.retention_days );
        cfg.backup.remote.upload_temp_suffix =
          yaml_get< std::string >( remote, "upload-temp-suffix", cfg.backup.remote.upload_temp_suffix );
      }

      if( auto admin = yaml_child( b, "admin" ); admin.IsDefined() )
      {
        cfg.backup.admin.enabled =
          yaml_get< bool >( admin, "enabled", cfg.backup.admin.enabled );
        cfg.backup.admin.listen =
          yaml_get< std::string >( admin, "listen", cfg.backup.admin.listen );
        cfg.backup.admin.token_file =
          yaml_get< std::string >( admin, "token-file", cfg.backup.admin.token_file );
        cfg.backup.admin.jobs =
          yaml_get< uint64_t >( admin, "jobs", cfg.backup.admin.jobs );
      }
    }

    // ── Feature flags ──
    if( auto f = yaml_child( root, "features" ); f.IsDefined() )
    {
      for( auto it = f.begin(); it != f.end(); ++it )
      {
        try
        {
          cfg.features[ it->first.as< std::string >() ] = it->second.as< bool >();
        }
        catch( ... )
        {}
      }
    }
  }

  // Apply CLI overrides
  for( const auto& comp: cli_enables )
    cfg.features[ comp ] = true;
  for( const auto& comp: cli_disables )
    cfg.features[ comp ] = false;

  return cfg;
}

} // namespace koinos::node
