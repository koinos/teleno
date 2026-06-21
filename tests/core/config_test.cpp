#include "core/config.hpp"
#include "core/rpc_access_policy.hpp"

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
