#include "core/config.hpp"
#include "core/rpc_access_policy.hpp"
#include "core/restore_startup.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace koinos::node;

namespace {

std::filesystem::path write_config( const std::string& content )
{
  auto path = std::filesystem::temp_directory_path()
              / ( "koinos-node-config-test-" + std::to_string( std::rand() ) + ".yml" );
  std::ofstream out( path );
  out << content;
  out.close();
  return path;
}

} // namespace

int main()
{
  {
    NodeConfig defaults;
    assert( defaults.rocksdb_blocks_zstd_level == 0 );
    assert( defaults.rocksdb_blocks_zstd_dictionary_bytes == 0 );
    assert( defaults.rocksdb_max_total_wal_size == 0 );
    for( bool strict: { false, true } )
    {
      for( const auto bytes: { uint64_t( 0 ), uint64_t( 67108864 ), uint64_t( UINT64_MAX ) } )
      {
        auto path = write_config( "rocksdb:\n  max-total-wal-size: " + std::to_string( bytes ) + "\n" );
        assert( load_config( path, {}, {}, strict ).rocksdb_max_total_wal_size == bytes );
        std::filesystem::remove( path );
      }
      for( const auto& codec: { "zstd", "kZSTD", "kZSTDCompression" } )
      {
        auto path = write_config( std::string( "rocksdb:\n  compression: none\n  blocks-compression: " )
                                  + codec + "\n  blocks-zstd-level: 9\n  blocks-zstd-dictionary-bytes: 65536\n" );
        const auto cfg = load_config( path, {}, {}, strict );
        assert( cfg.rocksdb_blocks_zstd_level == 9 && cfg.rocksdb_blocks_zstd_dictionary_bytes == 65536 );
        std::filesystem::remove( path );
      }
      for( const auto& valid: {
             "rocksdb:\n  blocks-zstd-level: 19\n",
             "rocksdb:\n  blocks-zstd-dictionary-bytes: 32768\n",
             "rocksdb:\n  blocks-zstd-level: 15\n  blocks-zstd-dictionary-bytes: 131072\n",
             "rocksdb:\n  compression: none\n  blocks-zstd-level: 0\n  blocks-zstd-dictionary-bytes: 0\n" } )
      {
        auto path = write_config( valid );
        load_config( path, {}, {}, strict );
        std::filesystem::remove( path );
      }
      for( const auto& invalid: {
             "rocksdb:\n  blocks-zstd-level: 20\n",
             "rocksdb:\n  blocks-zstd-dictionary-bytes: 1\n",
             "rocksdb:\n  blocks-zstd-dictionary-bytes: 65537\n",
             "rocksdb:\n  blocks-zstd-dictionary-bytes: 131073\n",
             "rocksdb:\n  blocks-zstd-dictionary-bytes: 262144\n",
             "rocksdb:\n  compression: none\n  blocks-zstd-level: 9\n",
             "rocksdb:\n  blocks-compression: snappy\n  blocks-zstd-dictionary-bytes: 65536\n" } )
      {
        auto path = write_config( invalid );
        bool threw = false;
        try { load_config( path, {}, {}, strict ); } catch( const std::invalid_argument& ) { threw = true; }
        assert( threw );
        std::filesystem::remove( path );
      }
      for( const auto& key: { "blocks-zstd-level", "blocks-zstd-dictionary-bytes", "max-total-wal-size" } )
        for( const auto& value: { "-1", "-0", "1.5", "true", "[]", "null", "18446744073709551616", "abc" } )
        {
          auto path = write_config( std::string( "rocksdb:\n  " ) + key + ": " + value + "\n" );
          bool threw = false;
          try { load_config( path, {}, {}, strict ); } catch( const std::invalid_argument& ) { threw = true; }
          assert( threw );
          std::filesystem::remove( path );
        }
    }
  }
  {
    NodeConfig defaults;
    assert( defaults.p2p_max_peer_error_records == 4096 );
    assert( defaults.p2p_peer_error_key_capacity_bytes == 1048576 );
    auto path = write_config( "p2p:\n  max-peer-error-records: 17\n  peer-error-key-capacity-bytes: 4096\n" );
    const auto cfg = load_config( path );
    assert( cfg.p2p_max_peer_error_records == 17 && cfg.p2p_peer_error_key_capacity_bytes == 4096 );
    std::filesystem::remove( path );
    for( const auto& key: { "max-peer-error-records", "peer-error-key-capacity-bytes" } )
      for( const auto& value: { "0", "-1", "-0", "1.5", "true", "[]", "null", "18446744073709551616", "abc" } )
      {
        path = write_config( std::string( "p2p:\n  " ) + key + ": " + value + "\n" );
        bool threw = false;
        try { load_config( path ); } catch( const std::exception& ) { threw = true; }
        assert( threw ); std::filesystem::remove( path );
      }
  }
  {
    NodeConfig defaults;
    assert( defaults.vm_module_cache_max_capacity_bytes == 0 );
    auto path = write_config( "chain:\n  vm-module-cache-max-capacity-bytes: 1048576\n" );
    assert( load_config( path ).vm_module_cache_max_capacity_bytes == 1048576 );
    std::filesystem::remove( path );
    for( const auto& value: { "-1", "-0", "1.5", "true", "[]", "null", "18446744073709551616", "abc" } )
    {
      path = write_config( std::string( "chain:\n  vm-module-cache-max-capacity-bytes: " ) + value + "\n" );
      bool threw = false;
      try { load_config( path ); } catch( const std::invalid_argument& ) { threw = true; }
      assert( threw ); std::filesystem::remove( path );
    }
    path = write_config( "chain:\n  vm-module-cache-max-capacity-bytes: 18446744073709551615\n" );
    assert( load_config( path ).vm_module_cache_max_capacity_bytes == UINT64_MAX );
    std::filesystem::remove( path );
  }
  {
    NodeConfig defaults;
    assert( !defaults.mempool_pending_record_max_owners && !defaults.mempool_pending_record_max_capacity_bytes );
    auto path = write_config( "mempool:\n  pending-record-max-owners: 2\n  pending-record-max-capacity-bytes: 1048576\n" );
    const auto cfg = load_config( path );
    assert( cfg.mempool_pending_record_max_owners == 2 && cfg.mempool_pending_record_max_capacity_bytes == 1048576 );
    std::filesystem::remove( path );
    path = write_config( "mempool:\n  pending-record-max-owners: 0\n  pending-record-max-capacity-bytes: 1\n" );
    const auto independent = load_config( path );
    assert( !independent.mempool_pending_record_max_owners && independent.mempool_pending_record_max_capacity_bytes == 1 );
    std::filesystem::remove( path );
    for( const auto& key : { "pending-record-max-owners", "pending-record-max-capacity-bytes" } )
      for( const auto& value : { "-1", "-0", "1.5", "true", "[]", "null", "18446744073709551616", "abc" } )
      {
        path = write_config( std::string( "mempool:\n  " ) + key + ": " + value + "\n" );
        bool threw = false;
        try { load_config( path ); } catch( const std::invalid_argument& ) { threw = true; }
        assert( threw ); std::filesystem::remove( path );
      }
  }
  {
    NodeConfig defaults;
    assert( !defaults.transaction_work_max_jobs && !defaults.transaction_work_max_wire_bytes );
    auto path = write_config( "chain:\n  transaction-work-max-jobs: 2\n  transaction-work-max-wire-bytes: 1048576\n" );
    const auto cfg = load_config( path );
    assert( cfg.transaction_work_max_jobs == 2 && cfg.transaction_work_max_wire_bytes == 1048576 );
    std::filesystem::remove( path );
    for( const auto& key : { "transaction-work-max-jobs", "transaction-work-max-wire-bytes" } )
      for( const auto& value : { "-1", "-0", "1.5", "true", "[]", "null", "18446744073709551616", "abc" } )
      {
        path = write_config( std::string( "chain:\n  " ) + key + ": " + value + "\n" );
        bool threw = false;
        try { load_config( path ); } catch( const std::invalid_argument& ) { threw = true; }
        assert( threw ); std::filesystem::remove( path );
      }
  }
  {
    auto path = write_config("jsonrpc:\n  jobs: 2\n  io-timeout-seconds: 5\n");
    auto cfg = load_config(path);
    assert(cfg.jsonrpc_jobs == 2 && cfg.jsonrpc_io_timeout_seconds == 5);
    std::filesystem::remove(path);
    for(const auto& invalid: {"jsonrpc:\n  jobs: 65\n", "jsonrpc:\n  io-timeout-seconds: 0\n", "jsonrpc:\n  io-timeout-seconds: 301\n"}) {
      path=write_config(invalid); bool threw=false;
      try { load_config(path); } catch(const std::exception&) { threw=true; }
      assert(threw); std::filesystem::remove(path);
    }
  }
  {
    const auto dir = std::filesystem::temp_directory_path()
                     / ( "teleno-restore-startup-test-" + std::to_string( std::rand() ) );
    std::filesystem::create_directories( dir );

    NodeConfig cfg;
    cfg.verify_blocks = false;
    cfg.features[ "block_producer" ] = true;

    auto result = apply_restore_startup_policy( cfg, dir );
    assert( !result.restore_marker_found );
    assert( cfg.is_enabled( "block_producer" ) );
    assert( !cfg.verify_blocks );

    std::ofstream( dir / ".backup-just-restored" ).close();
    result = apply_restore_startup_policy( cfg, dir );
    assert( result.restore_marker_found );
    assert( result.producer_recovery_hold_active );
    assert( result.block_producer_was_enabled );
    assert( !result.verify_blocks );
    assert( !cfg.is_enabled( "block_producer" ) );
    assert( !cfg.verify_blocks );
    assert( !std::filesystem::exists( dir / ".backup-just-restored" ) );
    assert( std::filesystem::exists( dir / ".backup-observer-recovery" ) );

    cfg.features[ "block_producer" ] = true;
    result = apply_restore_startup_policy( cfg, dir );
    assert( !result.restore_marker_found );
    assert( result.producer_recovery_hold_active );
    assert( !result.producer_recovery_hold_released );
    assert( !cfg.is_enabled( "block_producer" ) );

    cfg.features[ "block_producer" ] = true;
    result = apply_restore_startup_policy( cfg, dir, true );
    assert( !result.restore_marker_found );
    assert( !result.producer_recovery_hold_active );
    assert( result.producer_recovery_hold_released );
    assert( cfg.is_enabled( "block_producer" ) );
    assert( !std::filesystem::exists( dir / ".backup-observer-recovery" ) );

    cfg.verify_blocks = true;
    cfg.features[ "block_producer" ] = true;
    std::ofstream( dir / ".backup-just-restored" ).close();
    result = apply_restore_startup_policy( cfg, dir, true );
    assert( result.restore_marker_found );
    assert( result.producer_recovery_hold_active );
    assert( !result.producer_recovery_hold_released );
    assert( result.block_producer_was_enabled );
    assert( result.verify_blocks );
    assert( !cfg.is_enabled( "block_producer" ) );
    assert( cfg.verify_blocks );

    std::filesystem::remove_all( dir );
  }

  {
    auto path = write_config( R"(
global:
  blacklist:
    - block_store
    - chain.propose_block
  whitelist:
    - chain.get_head_info
p2p:
  identity-seed: stable-seed
  peer-log-interval-seconds: 60
  checkpoint:
    - "7:0x1220abcd"
  peer:
    - /dns4/seed.example/tcp/8888/p2p/QmSeed
)" );

    auto cfg = load_config( path );
    assert( cfg.rpc_blacklist.size() == 2 );
    assert( cfg.rpc_whitelist.size() == 1 );
    assert( cfg.p2p_identity_seed == "stable-seed" );
    assert( cfg.p2p_seeds.size() == 1 );
    assert( cfg.p2p_peer_log_interval_seconds == 60 );
    assert( cfg.p2p_checkpoints.size() == 1 );
    assert( cfg.p2p_checkpoints[ 0 ].block_height == 7 );
    assert( cfg.p2p_checkpoints[ 0 ].block_id == std::string( "\x12\x20\xab\xcd", 4 ) );

    std::filesystem::remove( path );
  }

  {
    auto path = write_config( R"(
p2p:
  peer-log-interval-seconds: 120
)" );

    auto cfg = load_config( path );
    assert( cfg.p2p_peer_log_interval_seconds == 120 );

    std::filesystem::remove( path );
  }

  {
    auto path = write_config( R"(
backup:
  enabled: true
  node-id: testnet-producer-1
  workspace: /tmp/teleno-backup-work
  schedule:
    enabled: true
    interval: 6h
    run-on-startup-if-missed: true
    jitter-seconds: 120
    minimum-head-progress: 2
    skip-if-syncing-from-genesis: true
    max-concurrent-backups: 1
  local:
    enabled: true
    directory: /tmp/teleno-local-backups
    retention-count: 3
  ssh:
    enabled: true
    transport: native
    host: 10.0.0.2
    port: 2222
    user: teleno-backup
    auth: password-file
    password-file: /tmp/teleno-backup-password
    known-hosts-file: /tmp/teleno-known-hosts
    strict-host-key-checking: true
    connect-timeout-seconds: 20
  remote:
    enabled: true
    directory: /srv/teleno-backups
    retention-count: 14
    retention-days: 30
    upload-temp-suffix: .uploading
  public-restore:
    enabled: true
    base-url: https://testnet.koinosfoundation.org/backups/testnet/teleno-bootstrap
    network: testnet
    require-https: true
    timeout-seconds: 45
    retries: 4
    signature-required: true
    signature-public-key-file: /tmp/teleno-public-bootstrap.pub
  public-publish:
    enabled: true
    directory: /srv/teleno-backups/testnet/public/teleno-bootstrap
    base-url: https://testnet.koinosfoundation.org/backups/testnet/teleno-bootstrap
    network: testnet
    observer-config-file: /tmp/teleno-public-bootstrap-observer.yml
    retention-count: 1
    upload-temp-suffix: .public-partial
  admin:
    enabled: true
    listen: 127.0.0.1:18089
    token-file: /tmp/teleno-backup-admin-token
    jobs: 2
)" );

    auto cfg = load_config( path );
    assert( cfg.backup.enabled );
    assert( cfg.backup.node_id == "testnet-producer-1" );
    assert( cfg.backup.workspace == "/tmp/teleno-backup-work" );
    assert( cfg.backup.schedule.enabled );
    assert( cfg.backup.schedule.interval == "6h" );
    assert( cfg.backup.schedule.run_on_startup_if_missed );
    assert( cfg.backup.schedule.jitter_seconds == 120 );
    assert( cfg.backup.schedule.minimum_head_progress == 2 );
    assert( cfg.backup.schedule.skip_if_syncing_from_genesis );
    assert( cfg.backup.local.enabled );
    assert( cfg.backup.local.directory == "/tmp/teleno-local-backups" );
    assert( cfg.backup.local.retention_count == 3 );
    assert( cfg.backup.ssh.enabled );
    assert( cfg.backup.ssh.host == "10.0.0.2" );
    assert( cfg.backup.ssh.port == 2222 );
    assert( cfg.backup.ssh.user == "teleno-backup" );
    assert( cfg.backup.ssh.password_file == "/tmp/teleno-backup-password" );
    assert( cfg.backup.ssh.known_hosts_file == "/tmp/teleno-known-hosts" );
    assert( cfg.backup.ssh.connect_timeout_seconds == 20 );
    assert( cfg.backup.remote.enabled );
    assert( cfg.backup.remote.directory == "/srv/teleno-backups" );
    assert( cfg.backup.remote.retention_count == 14 );
    assert( cfg.backup.remote.retention_days == 30 );
    assert( cfg.backup.remote.upload_temp_suffix == ".uploading" );
    assert( cfg.backup.public_restore.enabled );
    assert( cfg.backup.public_restore.base_url == "https://testnet.koinosfoundation.org/backups/testnet/teleno-bootstrap" );
    assert( cfg.backup.public_restore.network == "testnet" );
    assert( cfg.backup.public_restore.require_https );
    assert( cfg.backup.public_restore.timeout_seconds == 45 );
    assert( cfg.backup.public_restore.retries == 4 );
    assert( cfg.backup.public_restore.signature_required );
    assert( cfg.backup.public_restore.signature_public_key_file == "/tmp/teleno-public-bootstrap.pub" );
    assert( cfg.backup.public_publish.enabled );
    assert( cfg.backup.public_publish.directory == "/srv/teleno-backups/testnet/public/teleno-bootstrap" );
    assert( cfg.backup.public_publish.base_url == "https://testnet.koinosfoundation.org/backups/testnet/teleno-bootstrap" );
    assert( cfg.backup.public_publish.network == "testnet" );
    assert( cfg.backup.public_publish.observer_config_file == "/tmp/teleno-public-bootstrap-observer.yml" );
    assert( cfg.backup.public_publish.retention_count == 1 );
    assert( cfg.backup.public_publish.upload_temp_suffix == ".public-partial" );
    assert( cfg.backup.admin.enabled );
    assert( cfg.backup.admin.listen == "127.0.0.1:18089" );
    assert( cfg.backup.admin.token_file == "/tmp/teleno-backup-admin-token" );
    assert( cfg.backup.admin.jobs == 2 );

    std::filesystem::remove( path );
  }

  {
    auto path = write_config( R"(
p2p:
  seed: legacy-identity-seed
)" );

    auto cfg = load_config( path );
    assert( cfg.p2p_identity_seed == "legacy-identity-seed" );
    assert( cfg.p2p_peer_log_interval_seconds == 60 );
    assert( cfg.p2p_seeds.empty() );

    std::filesystem::remove( path );
  }

  {
    auto path = write_config( R"(
p2p:
  seed:
    - /dns4/legacy-seed.example/tcp/8888/p2p/QmSeed
)" );

    auto cfg = load_config( path );
    assert( cfg.p2p_identity_seed.empty() );
    assert( cfg.p2p_seeds.size() == 1 );

    std::filesystem::remove( path );
  }

  {
    auto path = write_config( R"(
p2p:
  checkpoint:
    - not-a-checkpoint
)" );

    bool threw = false;
    try
    {
      (void)load_config( path );
    }
    catch( const std::runtime_error& )
    {
      threw = true;
    }
    assert( threw );

    std::filesystem::remove( path );
  }

  {
    RpcAccessPolicy policy;
    assert( policy.allows( "chain", "get_head_info" ) );

    policy.blacklist = { "block_store", "chain.propose_block" };
    assert( policy.allows( "chain", "get_head_info" ) );
    assert( !policy.allows( "chain", "propose_block" ) );
    assert( !policy.allows( "block_store", "get_blocks_by_id" ) );

    policy.whitelist = { "koinos.rpc.chain.get_head_info" };
    assert( policy.allows( "chain", "get_head_info" ) );
    assert( !policy.allows( "chain", "propose_block" ) );
    assert( !policy.allows( "block_store", "get_blocks_by_id" ) );
  }

  return 0;
}
