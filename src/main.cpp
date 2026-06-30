/**
 * teleno_node — Monolithic Koinos blockchain node.
 *
 * Replaces 12 separate microservices + AMQP broker with a single binary.
 * Inter-service communication uses direct C++ function calls + EventBus
 * (boost::signals2) instead of serialized AMQP messages.
 *
 * Phase 0: Skeleton with EventBus, interfaces, config, lifecycle.
 * Phase 1: Chain, mempool, block_producer integrated via direct calls.
 */

#include <csignal>
#include <cstdlib>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>

#include <koinos/log.hpp>
#include <nlohmann/json.hpp>

#include "core/config.hpp"
#include "core/event_bus.hpp"
#include "core/node_version.hpp"
#include "core/service_registry.hpp"

#include "interfaces/i_block_store.hpp"
#include "interfaces/i_chain.hpp"
#include "interfaces/i_mempool.hpp"

// Phase 1: chain controller (internal, AMQP replaced by MonolithClient)
#include <koinos/chain/controller.hpp>
#include <koinos/chain/indexer.hpp>
#include "core/monolith_client.hpp"
#include "core/monolith_rpc_client.hpp"

// Phase 2: C++ block store
#include "block_store/block_store.hpp"

// Phase 3: JSON-RPC gateway
#include "jsonrpc/jsonrpc_server.hpp"

// Mempool
#include <koinos/mempool/mempool.hpp>
#include "mempool/mempool_adapter.hpp"

// Phase 5: P2P
#include "p2p/p2p_node.hpp"
#ifdef KOINOS_HAS_LIBP2P
#include "p2p/libp2p_transport.hpp"
#endif
#include "p2p/go_bridge_transport.hpp"

// Phase 6: gRPC + Account History
#include "grpc_server/grpc_server.hpp"
#include "account_history/account_history.hpp"

// Phase 4: Contract meta store + Transaction store
#include "contract_meta_store/contract_meta_store.hpp"
#include "transaction_store/transaction_store.hpp"

#include "block_production/block_producer.hpp"
#include "backup/backup_admin_server.hpp"
#include "backup/backup_scheduler.hpp"
#include "backup/backup_service.hpp"
#include "backup/checkpoint_manager.hpp"
#include "backup/backup_plan.hpp"
#include "backup/public_restore.hpp"
#include "backup/restore_activation_supervisor.hpp"
#include "backup/sftp_uploader.hpp"
#include "backup/snapshot_repository.hpp"
#include "storage/chain_migration.hpp"
#include "storage/rocksdb_manager.hpp"

#if defined( __APPLE__ )
#include <mach/mach_init.h>
#include <mach/task.h>
#elif defined( __linux__ )
#include <unistd.h>
#endif

// Protobuf
#include <koinos/broadcast/broadcast.pb.h>
#include <koinos/rpc/chain/chain_rpc.pb.h>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/rpc/mempool/mempool_rpc.pb.h>
#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>
#include <koinos/util/hex.hpp>

#include <google/protobuf/util/json_util.h>

#define HELP_OPTION    "help"
#define VERSION_OPTION "version"
#define BASEDIR_OPTION "basedir"
#define CONFIG_OPTION  "config"
#define LOG_LEVEL_OPTION "log-level"
#define ENABLE_OPTION  "enable"
#define DISABLE_OPTION "disable"
#define JOBS_OPTION    "jobs"
#define P2P_LISTEN_OPTION     "p2p-listen"
#define JSONRPC_LISTEN_OPTION "jsonrpc-listen"
#define STORAGE_REPORT_OPTION "storage-report"
#define MIGRATE_CHAIN_DB_OPTION "migrate-chain-db-to-unified-rocksdb"
#define ROLLBACK_CHAIN_DB_MIGRATION_OPTION "rollback-unified-chain-db-migration"
#define REQUIRE_ROCKSDB_COMPRESSION_OPTION "require-rocksdb-compression"
#define COMPACT_DB_OPTION "compact-db"
#define COMPACT_CF_OPTION "compact-cf"
#define BACKUP_DRY_RUN_OPTION "backup-dry-run"
#define BACKUP_CHECKPOINT_OPTION "backup-checkpoint"
#define BACKUP_CREATE_OPTION "backup-create"
#define BACKUP_CREATE_LOCAL_OPTION "backup-create-local"
#define BACKUP_UPLOAD_LATEST_OPTION "backup-upload-latest"
#define BACKUP_LIST_OPTION "backup-list"
#define BACKUP_LIST_REMOTE_OPTION "backup-list-remote"
#define BACKUP_PUBLIC_LIST_OPTION "backup-public-list"
#define BACKUP_PUBLIC_FETCH_OPTION "backup-public-fetch"
#define BACKUP_PUBLIC_RESTORE_OPTION "backup-public-restore"
#define BACKUP_PUBLIC_URL_OPTION "backup-public-url"
#define BACKUP_DELETE_OPTION "backup-delete"
#define BACKUP_SCOPE_OPTION "backup-scope"
#define BACKUP_DELETE_CONFIRM_OPTION "backup-delete-confirm"
#define BACKUP_RESTORE_OPTION "backup-restore"
#define BACKUP_RESTORE_PREFLIGHT_OPTION "backup-restore-preflight"
#define BACKUP_RESTORE_STAGE_OPTION "backup-restore-stage"
#define BACKUP_RESTORE_ACTIVATE_OPTION "backup-restore-activate"
#define BACKUP_RESTORE_FETCH_OPTION "backup-restore-fetch"
#define BACKUP_OUTPUT_OPTION "backup-output"
#define BACKUP_ID_OPTION "backup-id"
#define BACKUP_JSON_OPTION "backup-json"
#define ALL_OPTION "all"

namespace po = boost::program_options;
using namespace koinos;

namespace {

uint64_t current_process_rss_bytes()
{
#if defined( __APPLE__ )
  task_basic_info_data_t info;
  mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
  if( task_info( mach_task_self(), TASK_BASIC_INFO, reinterpret_cast< task_info_t >( &info ), &count ) == KERN_SUCCESS )
    return static_cast< uint64_t >( info.resident_size );
#elif defined( __linux__ )
  std::ifstream statm( "/proc/self/statm" );
  uint64_t pages = 0;
  if( statm >> pages )
  {
    const auto page_size = sysconf( _SC_PAGESIZE );
    if( page_size > 0 )
      return pages * static_cast< uint64_t >( page_size );
  }
#endif
  return 0;
}

std::pair< std::string, uint16_t > parse_jsonrpc_listen( const std::string& listen )
{
  if( listen.empty() )
    return { "0.0.0.0", 8080 };

  auto tcp_pos = listen.rfind( "/tcp/" );
  if( tcp_pos != std::string::npos )
  {
    auto host = std::string( "0.0.0.0" );
    auto ip4_pos = listen.find( "/ip4/" );
    if( ip4_pos != std::string::npos && ip4_pos < tcp_pos )
    {
      auto host_begin = ip4_pos + 5;
      host = listen.substr( host_begin, tcp_pos - host_begin );
    }

    return {
      host,
      static_cast< uint16_t >( std::stoi( listen.substr( tcp_pos + 5 ) ) )
    };
  }

  auto colon = listen.rfind( ':' );
  if( colon != std::string::npos )
  {
    return {
      listen.substr( 0, colon ),
      static_cast< uint16_t >( std::stoi( listen.substr( colon + 1 ) ) )
    };
  }

  return { "0.0.0.0", static_cast< uint16_t >( std::stoi( listen ) ) };
}

std::string backup_snapshot_list_cli_text(
  const node::backup::BackupSnapshotListResult& result,
  const std::string& source,
  const std::optional< std::string >& remote_reference = std::nullopt )
{
  const auto body = node::backup::backup_snapshot_list_result_to_text( result );
  const auto header_end = body.find( '\n' );

  std::ostringstream out;
  out << "Native backup snapshots\n";
  out << "source: " << source << "\n";
  if( remote_reference )
    out << "remote_reference: " << *remote_reference << "\n";
  if( header_end == std::string::npos )
    return out.str();
  out << body.substr( header_end + 1 );
  return out.str();
}

std::string backup_snapshot_list_cli_json(
  const node::backup::BackupSnapshotListResult& result,
  const std::string& source,
  const std::optional< std::string >& remote_reference = std::nullopt )
{
  auto json = nlohmann::ordered_json::parse(
    node::backup::backup_snapshot_list_result_to_json( result ) );
  json[ "source" ] = source;
  if( remote_reference )
    json[ "remote_reference" ] = *remote_reference;
  return json.dump( 2 ) + "\n";
}

std::string backup_delete_results_cli_text(
  const std::string& scope,
  const std::vector< node::backup::BackupDeleteResult >& results )
{
  std::ostringstream out;
  out << "Native backup delete results\n";
  out << "scope: " << scope << "\n";
  out << "result_count: " << results.size() << "\n";
  for( std::size_t i = 0; i < results.size(); ++i )
  {
    if( i )
      out << "\n";
    out << node::backup::backup_delete_result_to_text( results[ i ] );
  }
  return out.str();
}

std::string backup_delete_results_cli_json(
  const std::string& scope,
  const std::vector< node::backup::BackupDeleteResult >& results )
{
  nlohmann::ordered_json json;
  json[ "scope" ] = scope;
  json[ "result_count" ] = results.size();
  json[ "results" ] = nlohmann::ordered_json::array();
  for( const auto& result: results )
    json[ "results" ].push_back(
      nlohmann::ordered_json::parse( node::backup::backup_delete_result_to_json( result ) ) );
  return json.dump( 2 ) + "\n";
}

std::string join_strings( const std::vector< std::string >& values, const std::string& separator )
{
  std::ostringstream out;
  for( std::size_t i = 0; i < values.size(); ++i )
  {
    if( i )
      out << separator;
    out << values[ i ];
  }
  return out.str();
}

std::string read_backup_admin_token_file( const std::filesystem::path& token_file )
{
  if( token_file.empty() )
    throw std::runtime_error( "backup.admin.token-file is required when backup.admin.enabled is true" );

  std::ifstream input( token_file, std::ios::binary );
  if( !input )
    throw std::runtime_error( "failed to read backup admin token file: " + token_file.string() );

  std::string token( ( std::istreambuf_iterator< char >( input ) ),
                     std::istreambuf_iterator< char >() );
  const auto is_token_whitespace = []( char ch ) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
  };
  while( !token.empty() && is_token_whitespace( token.front() ) )
    token.erase( token.begin() );
  while( !token.empty() && is_token_whitespace( token.back() ) )
    token.pop_back();

  if( token.empty() )
    throw std::runtime_error( "backup admin token file is empty: " + token_file.string() );

  return token;
}

void print_storage_report( std::ostream& os,
                           const std::filesystem::path& basedir,
                           const std::filesystem::path& state_dir,
                           const node::storage::RocksDBManager& storage_db )
{
  os << "Teleno storage report\n";
  os << "basedir: " << basedir.string() << "\n";
  os << "shared_db: " << storage_db.path().string() << "\n";
  os << "chain_state_db: " << state_dir.string() << "\n";
  os << "chain_state_db_exists: " << ( std::filesystem::exists( state_dir ) ? "true" : "false" ) << "\n";
  os << "layout.version: " << storage_db.read_metadata( "layout.version" ) << "\n";
  os << "layout.chain_storage: " << storage_db.read_metadata( "layout.chain_storage" ) << "\n";
  os << "layout.network: " << storage_db.read_metadata( "layout.network" ) << "\n";
  os << "layout.created_by: " << storage_db.read_metadata( "layout.created_by" ) << "\n";
  const auto& compression = storage_db.compression_status();
  os << "compression.requested_default: " << compression.requested_default << "\n";
  os << "compression.selected_default: " << compression.selected_default << "\n";
  os << "compression.requested_blocks: " << compression.requested_blocks << "\n";
  os << "compression.selected_blocks: " << compression.selected_blocks << "\n";
  os << "compression.supported: " << join_strings( compression.supported_compressions, "," ) << "\n";
  if( !compression.fallback_note.empty() )
    os << "compression.fallback: " << compression.fallback_note << "\n";
  os << "column_families:\n";
  for( const auto& row: storage_db.column_family_stats() )
  {
    os << "  - name: " << row.name
       << " estimated_keys: " << row.estimated_keys
       << " total_sst_file_size: " << row.total_sst_file_size
       << " estimated_live_data_size: " << row.estimated_live_data_size
       << "\n";
  }
}

std::string configured_backup_result_to_text(
  const node::backup::LocalSnapshotResult& snapshot,
  const std::optional< node::backup::SftpUploadResult >& remote_upload )
{
  std::ostringstream out;
  out << "Created configured Teleno backup\n";
  out << node::backup::local_snapshot_result_to_text( snapshot );
  out << "remote_enabled: " << ( remote_upload.has_value() ? "true" : "false" ) << "\n";
  if( remote_upload )
    out << node::backup::sftp_upload_result_to_text( *remote_upload );
  else
    out << "remote_upload: skipped\n";
  return out.str();
}

std::string configured_backup_result_to_json(
  const node::backup::LocalSnapshotResult& snapshot,
  const std::optional< node::backup::SftpUploadResult >& remote_upload )
{
  nlohmann::json result;
  result[ "ok" ] = true;
  result[ "local_snapshot" ] = nlohmann::json::parse(
    node::backup::local_snapshot_result_to_json( snapshot ) );
  result[ "remote_enabled" ] = remote_upload.has_value();
  if( remote_upload )
    result[ "remote_upload" ] = nlohmann::json::parse(
      node::backup::sftp_upload_result_to_json( *remote_upload ) );
  else
    result[ "remote_upload" ] = nullptr;
  return result.dump( 2 ) + "\n";
}

node::backup::SftpTransferOptions cli_sftp_transfer_options( bool emit_json_progress )
{
  node::backup::SftpTransferOptions options;
  if( !emit_json_progress )
    return options;

  options.progress = []( const node::backup::SftpTransferProgress& progress ) {
    nlohmann::json event;
    event[ "event" ] = "backup-progress";
    event[ "phase" ] = progress.phase;
    event[ "backup_id" ] = progress.backup_id;
    event[ "completed_batches" ] = progress.completed_batches;
    event[ "total_batches" ] = progress.total_batches;
    event[ "attempt" ] = progress.attempt;
    event[ "file_count" ] = progress.file_count;
    event[ "completed_bytes" ] = progress.completed_bytes;
    event[ "total_bytes" ] = progress.total_bytes;
    std::cerr << event.dump() << std::endl;
  };
  return options;
}

node::backup::PublicRestoreOptions cli_public_restore_options( bool emit_json_progress )
{
  node::backup::PublicRestoreOptions options;
  if( !emit_json_progress )
    return options;

  options.progress = []( const node::backup::PublicRestoreProgress& progress ) {
    nlohmann::json event;
    event[ "event" ] = "backup-progress";
    event[ "phase" ] = progress.phase;
    event[ "backup_id" ] = progress.backup_id;
    event[ "completed_batches" ] = progress.completed_batches;
    event[ "total_batches" ] = progress.total_batches;
    event[ "attempt" ] = progress.attempt;
    event[ "file_count" ] = progress.file_count;
    event[ "completed_bytes" ] = progress.completed_bytes;
    event[ "total_bytes" ] = progress.total_bytes;
    std::cerr << event.dump() << std::endl;
  };
  return options;
}

node::backup::RestoreStageProgressCallback cli_restore_stage_progress(
  bool emit_json_progress,
  std::string phase )
{
  if( !emit_json_progress )
    return {};

  return [phase = std::move( phase )]( const node::backup::RestoreStageProgress& progress ) {
    nlohmann::json event;
    event[ "event" ] = "backup-progress";
    event[ "phase" ] = phase;
    event[ "backup_id" ] = progress.backup_id;
    event[ "completed_batches" ] = progress.completed_files;
    event[ "total_batches" ] = progress.total_files;
    event[ "attempt" ] = 1;
    event[ "file_count" ] = progress.total_files;
    event[ "completed_bytes" ] = progress.completed_bytes;
    event[ "total_bytes" ] = progress.total_bytes;
    std::cerr << event.dump() << std::endl;
  };
}

std::string shell_quote( const std::filesystem::path& path )
{
  std::string value = path.string();
  std::string out = "'";
  for( char ch: value )
  {
    if( ch == '\'' )
      out += "'\\''";
    else
      out.push_back( ch );
  }
  out += "'";
  return out;
}

struct ConfiguredRestoreResult
{
  bool ok = false;
  bool remote_enabled = false;
  bool public_restore_enabled = false;
  std::optional< node::backup::SftpRestoreFetchResult > remote_fetch;
  std::optional< node::backup::PublicRestoreFetchResult > public_fetch;
  node::backup::RestorePreflightResult preflight;
  std::optional< node::backup::RestoreStageResult > stage;
  std::optional< node::backup::RestoreActivationResult > activation;
  std::string observer_start_command;
};

std::string configured_restore_result_to_text( const ConfiguredRestoreResult& result )
{
  std::ostringstream out;
  out << ( result.ok ? "Restored configured Teleno backup\n"
                     : "Teleno backup restore did not complete\n" );
  out << "remote_enabled: " << ( result.remote_enabled ? "true" : "false" ) << "\n";
  out << "public_restore_enabled: " << ( result.public_restore_enabled ? "true" : "false" ) << "\n";
  if( result.remote_fetch )
    out << node::backup::sftp_restore_fetch_result_to_text( *result.remote_fetch );
  if( result.public_fetch )
    out << node::backup::public_restore_fetch_result_to_text( *result.public_fetch );
  out << node::backup::restore_preflight_result_to_text( result.preflight );
  if( result.stage )
    out << node::backup::restore_stage_result_to_text( *result.stage );
  if( result.activation )
  {
    out << node::backup::restore_activation_result_to_text( *result.activation );
    out << "observer_start_command: " << result.observer_start_command << "\n";
  }
  return out.str();
}

std::string configured_restore_result_to_json( const ConfiguredRestoreResult& result )
{
  nlohmann::json json;
  json[ "ok" ] = result.ok;
  json[ "remote_enabled" ] = result.remote_enabled;
  json[ "public_restore_enabled" ] = result.public_restore_enabled;
  json[ "remote_fetch" ] = result.remote_fetch
    ? nlohmann::json::parse( node::backup::sftp_restore_fetch_result_to_json( *result.remote_fetch ) )
    : nlohmann::json( nullptr );
  json[ "public_fetch" ] = result.public_fetch
    ? nlohmann::json::parse( node::backup::public_restore_fetch_result_to_json( *result.public_fetch ) )
    : nlohmann::json( nullptr );
  json[ "preflight" ] = nlohmann::json::parse(
    node::backup::restore_preflight_result_to_json( result.preflight ) );
  json[ "stage" ] = result.stage
    ? nlohmann::json::parse( node::backup::restore_stage_result_to_json( *result.stage ) )
    : nlohmann::json( nullptr );
  json[ "activation" ] = result.activation
    ? nlohmann::json::parse( node::backup::restore_activation_result_to_json( *result.activation ) )
    : nlohmann::json( nullptr );
  json[ "observer_start_command" ] = result.observer_start_command;
  return json.dump( 2 ) + "\n";
}

bool config_file_exists( const std::filesystem::path& config_path )
{
  std::error_code ec;
  return std::filesystem::is_regular_file( config_path, ec );
}

void ensure_public_restore_local_repository( node::NodeConfig& cfg,
                                             const std::filesystem::path& basedir )
{
  cfg.backup.local.enabled = true;
  if( cfg.backup.local.directory.empty() )
    cfg.backup.local.directory = ( basedir / ".teleno-native-backups" / "repository" ).string();
  std::filesystem::create_directories( cfg.backup.local.directory );
}

void validate_public_restore_config( const node::NodeConfig& cfg )
{
  if( !cfg.backup.public_restore.enabled )
    throw std::runtime_error( "public restore requires backup.public-restore.enabled=true or --backup-public-url" );
  if( cfg.backup.public_restore.base_url.empty() )
    throw std::runtime_error( "public restore requires backup.public-restore.base-url or --backup-public-url" );
  if( cfg.backup.local.directory.empty() )
    throw std::runtime_error( "public restore requires backup.local.directory" );
}

void write_observer_config_for_public_restore( const std::filesystem::path& config_path,
                                               const node::NodeConfig& cfg,
                                               const std::filesystem::path& basedir )
{
  if( config_file_exists( config_path ) )
    return;

  std::filesystem::create_directories( config_path.parent_path() );
  std::ofstream out( config_path, std::ios::binary | std::ios::trunc );
  if( !out )
    throw std::runtime_error( "failed to write observer config after public restore: " + config_path.string() );

  const auto jsonrpc_listen = cfg.backup.public_restore.network == "testnet" ? "127.0.0.1:18122" : "127.0.0.1:8080";
  const auto p2p_listen = cfg.backup.public_restore.network == "testnet" ? "/ip4/0.0.0.0/tcp/18888"
                                                                          : "/ip4/0.0.0.0/tcp/8888";
  out << "global:\n";
  out << "  log-level: info\n";
  out << "  fork-algorithm: pob\n";
  out << "chain:\n";
  out << "  verify-blocks: true\n";
  out << "p2p:\n";
  out << "  listen: " << p2p_listen << "\n";
  out << "  peer-log-interval-seconds: 60\n";
  if( cfg.backup.public_restore.network == "testnet" )
  {
    out << "  peer:\n";
    out << "    - /dns4/testnet.koinosfoundation.org/tcp/8888/p2p/QmYV414G6xRzkSUytntEsBsCSjXrVGubfYJn4vpeER2s2W\n";
  }
  out << "jsonrpc:\n";
  out << "  listen: " << jsonrpc_listen << "\n";
  out << "features:\n";
  out << "  chain: true\n";
  out << "  mempool: true\n";
  out << "  block_store: true\n";
  out << "  p2p: true\n";
  out << "  jsonrpc: true\n";
  out << "  grpc: false\n";
  out << "  block_producer: false\n";
  out << "  contract_meta_store: true\n";
  out << "  transaction_store: false\n";
  out << "  account_history: false\n";
  out << "backup:\n";
  out << "  enabled: true\n";
  out << "  node-id: teleno-public-" << cfg.backup.public_restore.network << "\n";
  out << "  workspace: " << ( basedir / ".teleno-native-backups" / "workspace" ).string() << "\n";
  out << "  local:\n";
  out << "    enabled: true\n";
  out << "    directory: " << cfg.backup.local.directory << "\n";
  out << "    retention-count: " << cfg.backup.local.retention_count << "\n";
  out << "  public-restore:\n";
  out << "    enabled: true\n";
  out << "    base-url: " << cfg.backup.public_restore.base_url << "\n";
  out << "    network: " << cfg.backup.public_restore.network << "\n";
  out << "    require-https: " << ( cfg.backup.public_restore.require_https ? "true" : "false" ) << "\n";
  out << "    timeout-seconds: " << cfg.backup.public_restore.timeout_seconds << "\n";
  out << "    retries: " << cfg.backup.public_restore.retries << "\n";
  out << "    signature-required: " << ( cfg.backup.public_restore.signature_required ? "true" : "false" ) << "\n";
  if( !cfg.backup.public_restore.signature_public_key_file.empty() )
    out << "    signature-public-key-file: " << cfg.backup.public_restore.signature_public_key_file << "\n";
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// ChainAdapter — wraps chain::controller to implement IChain
// ---------------------------------------------------------------------------

class ChainAdapter final : public node::IChain
{
public:
  explicit ChainAdapter( chain::controller& ctrl ) : _ctrl( ctrl ) {}

  rpc::chain::submit_block_response
  submit_block( const rpc::chain::submit_block_request& req ) override
  {
    return _ctrl.submit_block( req );
  }

  rpc::chain::submit_transaction_response
  submit_transaction( const rpc::chain::submit_transaction_request& req ) override
  {
    return _ctrl.submit_transaction( req );
  }

  rpc::chain::get_head_info_response
  get_head_info( const rpc::chain::get_head_info_request& req ) override
  {
    return _ctrl.get_head_info( req );
  }

  rpc::chain::get_chain_id_response
  get_chain_id( const rpc::chain::get_chain_id_request& req ) override
  {
    return _ctrl.get_chain_id( req );
  }

  rpc::chain::get_fork_heads_response
  get_fork_heads( const rpc::chain::get_fork_heads_request& req ) override
  {
    return _ctrl.get_fork_heads( req );
  }

  rpc::chain::read_contract_response
  read_contract( const rpc::chain::read_contract_request& req ) override
  {
    return _ctrl.read_contract( req );
  }

  rpc::chain::get_account_nonce_response
  get_account_nonce( const rpc::chain::get_account_nonce_request& req ) override
  {
    return _ctrl.get_account_nonce( req );
  }

  rpc::chain::get_account_rc_response
  get_account_rc( const rpc::chain::get_account_rc_request& req ) override
  {
    return _ctrl.get_account_rc( req );
  }

  rpc::chain::get_resource_limits_response
  get_resource_limits( const rpc::chain::get_resource_limits_request& req ) override
  {
    return _ctrl.get_resource_limits( req );
  }

  rpc::chain::invoke_system_call_response
  invoke_system_call( const rpc::chain::invoke_system_call_request& req ) override
  {
    return _ctrl.invoke_system_call( req );
  }

  rpc::chain::propose_block_response
  propose_block( const rpc::chain::propose_block_request& req ) override
  {
    return _ctrl.propose_block( req );
  }

private:
  chain::controller& _ctrl;
};


// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main( int argc, char** argv )
{
  try
  {
    // ── CLI argument parsing ──
    po::options_description desc( "teleno_node options" );
    desc.add_options()
      ( HELP_OPTION ",h",    "Print help" )
      ( VERSION_OPTION ",v", "Print version" )
      ( BASEDIR_OPTION ",d", po::value< std::string >()->default_value( "" ),
        "Base data directory (default: ~/.koinos)" )
      ( CONFIG_OPTION ",c",  po::value< std::string >()->default_value( "" ),
        "Config file path (default: {basedir}/config.yml)" )
      ( LOG_LEVEL_OPTION ",l", po::value< std::string >()->default_value( "" ),
        "Log level override" )
      ( ENABLE_OPTION,  po::value< std::vector< std::string > >()->multitoken(),
        "Enable a component (repeatable)" )
      ( DISABLE_OPTION, po::value< std::vector< std::string > >()->multitoken(),
        "Disable a component (repeatable)" )
      ( JOBS_OPTION ",j", po::value< uint64_t >()->default_value( 0 ),
        "Chain worker threads" )
      ( P2P_LISTEN_OPTION,     po::value< std::string >()->default_value( "" ),
        "P2P listen address override" )
      ( JSONRPC_LISTEN_OPTION, po::value< std::string >()->default_value( "" ),
        "JSON-RPC listen address override" )
      ( STORAGE_REPORT_OPTION,
        "Print storage layout metadata and RocksDB column-family estimates, then exit" )
      ( MIGRATE_CHAIN_DB_OPTION,
        "Migrate legacy chain state into the shared RocksDB layout, then exit" )
      ( ROLLBACK_CHAIN_DB_MIGRATION_OPTION,
        "Restore the preserved legacy chain DB after unified storage migration, then exit" )
      ( REQUIRE_ROCKSDB_COMPRESSION_OPTION,
        "Fail if configured RocksDB compression is not exactly supported by the linked library" )
      ( COMPACT_DB_OPTION,
        "Compact the shared RocksDB database, then exit" )
      ( COMPACT_CF_OPTION, po::value< std::vector< std::string > >()->multitoken(),
        "Column family to compact; repeat or use with multiple names" )
      ( BACKUP_DRY_RUN_OPTION,
        "Validate native backup configuration and print the backup plan, then exit without opening RocksDB" )
      ( BACKUP_CHECKPOINT_OPTION,
        "Create a local RocksDB checkpoint of unified BASEDIR/db, then exit without starting node services" )
      ( BACKUP_CREATE_OPTION,
        "Create the configured native backup: local hot snapshot, plus remote upload when backup.remote.enabled=true" )
      ( BACKUP_CREATE_LOCAL_OPTION,
        "Create a local object-store backup snapshot from unified BASEDIR/db, then exit without starting node services" )
      ( BACKUP_UPLOAD_LATEST_OPTION,
        "Upload backup.local.directory latest snapshot to backup.remote.directory over native libssh SFTP, then exit" )
      ( BACKUP_LIST_OPTION,
        "List completed snapshots in backup.local.directory, then exit without opening RocksDB" )
      ( BACKUP_LIST_REMOTE_OPTION,
        "Fetch and list completed snapshots from backup.remote.directory over native libssh SFTP, caching metadata locally" )
      ( BACKUP_PUBLIC_LIST_OPTION,
        "Fetch and list the latest or selected public read-only backup snapshot metadata over HTTP(S), caching metadata locally" )
      ( BACKUP_PUBLIC_FETCH_OPTION,
        "Fetch latest or selected public read-only backup metadata and missing objects over HTTP(S), then exit without opening RocksDB" )
      ( BACKUP_PUBLIC_RESTORE_OPTION,
        "Restore latest or selected public read-only backup over HTTP(S), preflight, stage, activate, then exit" )
      ( BACKUP_PUBLIC_URL_OPTION, po::value< std::string >()->default_value( "" ),
        "Public read-only backup base URL override for --backup-public-list/fetch/restore" )
      ( BACKUP_DELETE_OPTION,
        "Delete the selected native backup snapshot. Dry-run by default; requires --backup-delete-confirm=<backup-id> to mutate" )
      ( BACKUP_SCOPE_OPTION, po::value< std::string >()->default_value( "local" ),
        "Backup delete scope: local, remote, or both" )
      ( BACKUP_DELETE_CONFIRM_OPTION, po::value< std::string >()->default_value( "" ),
        "Exact backup ID confirmation required to execute --backup-delete; omit for dry-run" )
      ( BACKUP_RESTORE_OPTION,
        "Restore the configured native backup: fetch remote data when enabled, preflight, stage, activate, then exit" )
      ( BACKUP_RESTORE_PREFLIGHT_OPTION,
        "Validate the latest local backup snapshot and target disk space before restore, then exit without opening RocksDB" )
      ( BACKUP_RESTORE_STAGE_OPTION,
        "Rebuild the latest local backup snapshot into a restore staging directory, then exit without opening RocksDB" )
      ( BACKUP_RESTORE_ACTIVATE_OPTION,
        "Activate a staged local backup restore while the node is stopped, then exit without opening RocksDB" )
      ( BACKUP_RESTORE_FETCH_OPTION,
        "Fetch selected or latest remote backup metadata and missing objects over native libssh SFTP, then exit without opening RocksDB" )
      ( BACKUP_OUTPUT_OPTION, po::value< std::string >()->default_value( "" ),
        "Output directory for --backup-checkpoint, staging directory for --backup-restore/--backup-restore-stage, or staged dir for --backup-restore-activate" )
      ( BACKUP_ID_OPTION, po::value< std::string >()->default_value( "" ),
        "Backup ID to use with backup list/fetch/restore/preflight/stage commands; omit or use latest by default" )
      ( BACKUP_JSON_OPTION,
        "Print backup command output as JSON when used with backup command modes" )
      ( ALL_OPTION,
        "When used with --compact-db, compact all shared RocksDB column families" );

    po::variables_map vm;
    po::store( po::parse_command_line( argc, argv, desc ), vm );
    po::notify( vm );

    if( vm.count( HELP_OPTION ) )
    {
      std::cout << desc << std::endl;
      return EXIT_SUCCESS;
    }

    if( vm.count( VERSION_OPTION ) )
    {
      std::cout << node::node_name() << " " << node::build_version() << std::endl;
      return EXIT_SUCCESS;
    }

    // ── Resolve basedir ──
    std::filesystem::path basedir = vm[ BASEDIR_OPTION ].as< std::string >();
    if( basedir.empty() )
    {
      const char* home = std::getenv( "HOME" );
      basedir = home ? std::filesystem::path( home ) / ".koinos"
                     : std::filesystem::current_path() / ".koinos";
    }
    std::filesystem::create_directories( basedir );

    // ── Load config ──
    std::filesystem::path config_path = vm[ CONFIG_OPTION ].as< std::string >();
    if( config_path.empty() )
      config_path = basedir / "config.yml";

    auto enables  = vm.count( ENABLE_OPTION )  ? vm[ ENABLE_OPTION ].as< std::vector< std::string > >()  : std::vector< std::string >{};
    auto disables = vm.count( DISABLE_OPTION ) ? vm[ DISABLE_OPTION ].as< std::vector< std::string > >() : std::vector< std::string >{};

    node::NodeConfig cfg = node::load_config( config_path, enables, disables );
    cfg.basedir = basedir;

    // CLI overrides
    if( auto ll = vm[ LOG_LEVEL_OPTION ].as< std::string >(); !ll.empty() )
      cfg.log_level = ll;
    if( auto jobs = vm[ JOBS_OPTION ].as< uint64_t >(); jobs > 0 )
      cfg.chain_jobs = jobs;
    if( auto p2p = vm[ P2P_LISTEN_OPTION ].as< std::string >(); !p2p.empty() )
      cfg.p2p_listen = p2p;
    if( auto jrpc = vm[ JSONRPC_LISTEN_OPTION ].as< std::string >(); !jrpc.empty() )
      cfg.jsonrpc_listen = jrpc;
    if( vm.count( REQUIRE_ROCKSDB_COMPRESSION_OPTION ) )
      cfg.rocksdb_require_compression = true;
    if( auto public_url = vm[ BACKUP_PUBLIC_URL_OPTION ].as< std::string >(); !public_url.empty() )
    {
      cfg.backup.public_restore.enabled = true;
      cfg.backup.public_restore.base_url = public_url;
    }

    const bool public_backup_mode = vm.count( BACKUP_PUBLIC_LIST_OPTION )
                                 || vm.count( BACKUP_PUBLIC_FETCH_OPTION )
                                 || vm.count( BACKUP_PUBLIC_RESTORE_OPTION );
    if( public_backup_mode )
    {
      ensure_public_restore_local_repository( cfg, basedir );
      validate_public_restore_config( cfg );
    }

    if( vm.count( BACKUP_JSON_OPTION )
        && !vm.count( BACKUP_DRY_RUN_OPTION )
        && !vm.count( BACKUP_CHECKPOINT_OPTION )
        && !vm.count( BACKUP_CREATE_OPTION )
        && !vm.count( BACKUP_CREATE_LOCAL_OPTION )
        && !vm.count( BACKUP_UPLOAD_LATEST_OPTION )
        && !vm.count( BACKUP_LIST_OPTION )
        && !vm.count( BACKUP_LIST_REMOTE_OPTION )
        && !vm.count( BACKUP_PUBLIC_LIST_OPTION )
        && !vm.count( BACKUP_PUBLIC_FETCH_OPTION )
        && !vm.count( BACKUP_PUBLIC_RESTORE_OPTION )
        && !vm.count( BACKUP_DELETE_OPTION )
        && !vm.count( BACKUP_RESTORE_OPTION )
        && !vm.count( BACKUP_RESTORE_PREFLIGHT_OPTION )
        && !vm.count( BACKUP_RESTORE_STAGE_OPTION )
        && !vm.count( BACKUP_RESTORE_ACTIVATE_OPTION )
        && !vm.count( BACKUP_RESTORE_FETCH_OPTION ) )
      throw std::runtime_error( "--backup-json requires a backup command mode" );

    if( vm.count( BACKUP_DRY_RUN_OPTION ) )
    {
      auto plan = node::backup::build_backup_dry_run_plan( cfg, basedir, config_path );
      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << node::backup::backup_dry_run_plan_to_json( plan );
      else
        std::cout << node::backup::backup_dry_run_plan_to_text( plan );
      return plan.has_errors() ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    if( vm.count( BACKUP_LIST_OPTION ) )
    {
      if( !cfg.backup.local.enabled )
        throw std::runtime_error( "--backup-list requires backup.local.enabled=true" );
      if( cfg.backup.local.directory.empty() )
        throw std::runtime_error( "--backup-list requires backup.local.directory" );

      auto result = node::backup::list_local_backup_snapshots( cfg.backup.local.directory );
      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << backup_snapshot_list_cli_json( result, "local" );
      else
        std::cout << backup_snapshot_list_cli_text( result, "local" );
      return EXIT_SUCCESS;
    }

    if( vm.count( BACKUP_PUBLIC_LIST_OPTION ) )
    {
      const auto backup_id = vm[ BACKUP_ID_OPTION ].as< std::string >();
      auto result = node::backup::list_public_backup_snapshots(
        cfg.backup.local.directory,
        cfg.backup.public_restore,
        backup_id,
        cli_public_restore_options( vm.count( BACKUP_JSON_OPTION ) > 0 ) );
      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << backup_snapshot_list_cli_json( result, "public_http", cfg.backup.public_restore.base_url );
      else
        std::cout << backup_snapshot_list_cli_text( result, "public_http", cfg.backup.public_restore.base_url );
      return EXIT_SUCCESS;
    }

    if( vm.count( BACKUP_LIST_REMOTE_OPTION ) )
    {
      if( !cfg.backup.local.enabled )
        throw std::runtime_error(
          "--backup-list-remote requires backup.local.enabled=true because remote metadata is cached locally" );
      if( cfg.backup.local.directory.empty() )
        throw std::runtime_error( "--backup-list-remote requires backup.local.directory" );
      if( !cfg.backup.remote.enabled )
        throw std::runtime_error( "--backup-list-remote requires backup.remote.enabled=true" );
      if( cfg.backup.remote.directory.empty() )
        throw std::runtime_error( "--backup-list-remote requires backup.remote.directory" );
      if( !cfg.backup.ssh.enabled )
        throw std::runtime_error( "--backup-list-remote requires backup.ssh.enabled=true" );

      auto result = node::backup::list_remote_backup_snapshots_with_managed_sftp(
        cfg.backup.local.directory,
        cfg.backup.ssh,
        cfg.backup.remote );
      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << backup_snapshot_list_cli_json( result, "remote_sftp", cfg.backup.remote.directory );
      else
        std::cout << backup_snapshot_list_cli_text( result, "remote_sftp", cfg.backup.remote.directory );
      return EXIT_SUCCESS;
    }

    if( vm.count( BACKUP_PUBLIC_FETCH_OPTION ) )
    {
      const auto backup_id = vm[ BACKUP_ID_OPTION ].as< std::string >();
      auto result = node::backup::fetch_public_restore_snapshot(
        cfg.backup.local.directory,
        basedir,
        cfg.backup.public_restore,
        backup_id,
        cli_public_restore_options( vm.count( BACKUP_JSON_OPTION ) > 0 ) );
      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << node::backup::public_restore_fetch_result_to_json( result );
      else
        std::cout << node::backup::public_restore_fetch_result_to_text( result );
      return result.ready_to_stage ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if( vm.count( BACKUP_DELETE_OPTION ) )
    {
      const auto backup_id = vm[ BACKUP_ID_OPTION ].as< std::string >();
      if( backup_id.empty() || backup_id == "latest" )
        throw std::runtime_error( "--backup-delete requires an exact --backup-id; 'latest' is not accepted" );

      const auto scope = vm[ BACKUP_SCOPE_OPTION ].as< std::string >();
      if( scope != "local" && scope != "remote" && scope != "both" )
        throw std::runtime_error( "--backup-scope must be local, remote, or both" );

      const auto confirm = vm[ BACKUP_DELETE_CONFIRM_OPTION ].as< std::string >();
      if( !confirm.empty() && confirm != backup_id )
        throw std::runtime_error( "--backup-delete-confirm must exactly match --backup-id" );
      const bool dry_run = confirm != backup_id;

      std::vector< node::backup::BackupDeleteResult > results;
      const auto delete_local = [&]() {
        if( !cfg.backup.local.enabled )
          throw std::runtime_error( "--backup-delete local scope requires backup.local.enabled=true" );
        if( cfg.backup.local.directory.empty() )
          throw std::runtime_error( "--backup-delete local scope requires backup.local.directory" );
        results.push_back( node::backup::delete_local_backup_snapshot(
          cfg.backup.local.directory,
          backup_id,
          dry_run ) );
      };

      const auto delete_remote = [&]() {
        if( !cfg.backup.local.enabled )
          throw std::runtime_error(
            "--backup-delete remote scope requires backup.local.enabled=true for temporary metadata" );
        if( cfg.backup.local.directory.empty() )
          throw std::runtime_error( "--backup-delete remote scope requires backup.local.directory" );
        if( !cfg.backup.remote.enabled )
          throw std::runtime_error( "--backup-delete remote scope requires backup.remote.enabled=true" );
        if( cfg.backup.remote.directory.empty() )
          throw std::runtime_error( "--backup-delete remote scope requires backup.remote.directory" );
        if( !cfg.backup.ssh.enabled )
          throw std::runtime_error( "--backup-delete remote scope requires backup.ssh.enabled=true" );
        results.push_back( node::backup::delete_remote_backup_snapshot_with_managed_sftp(
          cfg.backup.local.directory,
          cfg.backup.ssh,
          cfg.backup.remote,
          backup_id,
          dry_run ) );
      };

      if( scope == "both" && !dry_run )
      {
        delete_remote();
        delete_local();
      }
      else
      {
        if( scope == "local" || scope == "both" )
          delete_local();
        if( scope == "remote" || scope == "both" )
          delete_remote();
      }

      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << backup_delete_results_cli_json( scope, results );
      else
        std::cout << backup_delete_results_cli_text( scope, results );
      return EXIT_SUCCESS;
    }

    if( vm.count( BACKUP_UPLOAD_LATEST_OPTION ) )
    {
      auto plan = node::backup::build_backup_dry_run_plan( cfg, basedir, config_path );
      if( plan.has_errors() )
      {
        if( vm.count( BACKUP_JSON_OPTION ) )
          std::cout << node::backup::backup_dry_run_plan_to_json( plan );
        else
          std::cout << node::backup::backup_dry_run_plan_to_text( plan );
        return EXIT_FAILURE;
      }
      if( !cfg.backup.local.enabled )
        throw std::runtime_error( "--backup-upload-latest requires backup.local.enabled=true" );
      if( !cfg.backup.remote.enabled )
        throw std::runtime_error( "--backup-upload-latest requires backup.remote.enabled=true" );
      if( !cfg.backup.ssh.enabled )
        throw std::runtime_error( "--backup-upload-latest requires backup.ssh.enabled=true" );

      auto result = node::backup::upload_latest_snapshot_with_managed_sftp(
        cfg.backup.local.directory,
        cfg.backup.ssh,
        cfg.backup.remote,
        cli_sftp_transfer_options( vm.count( BACKUP_JSON_OPTION ) > 0 ) );
      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << node::backup::sftp_upload_result_to_json( result );
      else
        std::cout << node::backup::sftp_upload_result_to_text( result );
      return EXIT_SUCCESS;
    }

    if( vm.count( BACKUP_PUBLIC_RESTORE_OPTION ) )
    {
      const auto backup_id = vm[ BACKUP_ID_OPTION ].as< std::string >();
      ConfiguredRestoreResult result;
      result.public_restore_enabled = true;
      result.public_fetch = node::backup::fetch_public_restore_snapshot(
        cfg.backup.local.directory,
        basedir,
        cfg.backup.public_restore,
        backup_id,
        cli_public_restore_options( vm.count( BACKUP_JSON_OPTION ) > 0 ) );
      result.preflight = result.public_fetch->preflight;
      if( !result.public_fetch->ready_to_stage )
      {
        if( vm.count( BACKUP_JSON_OPTION ) )
          std::cout << configured_restore_result_to_json( result );
        else
          std::cout << configured_restore_result_to_text( result );
        return EXIT_FAILURE;
      }

      const auto output = vm[ BACKUP_OUTPUT_OPTION ].as< std::string >();
      result.stage = node::backup::stage_local_restore_snapshot(
        cfg.backup.local.directory,
        basedir,
        backup_id == "latest" ? std::string{} : backup_id,
        output,
        cli_restore_stage_progress( vm.count( BACKUP_JSON_OPTION ) > 0, "public-restore-stage" ) );
      result.activation = node::backup::activate_staged_restore_snapshot(
        result.stage->staging_dir,
        basedir );
      write_observer_config_for_public_restore( config_path, cfg, basedir );
      result.ok = true;
      std::ostringstream start_command;
      start_command << "teleno_node --basedir " << shell_quote( basedir )
                    << " --config " << shell_quote( config_path )
                    << " --disable block_producer";
      result.observer_start_command = start_command.str();

      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << configured_restore_result_to_json( result );
      else
        std::cout << configured_restore_result_to_text( result );
      return EXIT_SUCCESS;
    }

    if( vm.count( BACKUP_RESTORE_OPTION ) )
    {
      const auto backup_id = vm[ BACKUP_ID_OPTION ].as< std::string >();
      if( !cfg.backup.local.enabled )
        throw std::runtime_error(
          "--backup-restore requires backup.local.enabled=true because restore uses the local object repository as its verified cache" );
      if( cfg.backup.local.directory.empty() )
        throw std::runtime_error( "--backup-restore requires backup.local.directory" );
      if( cfg.backup.remote.enabled )
      {
        if( cfg.backup.remote.directory.empty() )
          throw std::runtime_error( "--backup-restore remote fetch requires backup.remote.directory" );
        if( !cfg.backup.ssh.enabled )
          throw std::runtime_error( "--backup-restore remote fetch requires backup.ssh.enabled=true" );
      }

      ConfiguredRestoreResult result;
      result.remote_enabled = cfg.backup.remote.enabled;

      if( cfg.backup.remote.enabled )
      {
        result.remote_fetch = node::backup::fetch_restore_snapshot_with_managed_sftp(
          cfg.backup.local.directory,
          basedir,
          cfg.backup.ssh,
          cfg.backup.remote,
          backup_id,
          cli_sftp_transfer_options( vm.count( BACKUP_JSON_OPTION ) > 0 ) );
        result.preflight = result.remote_fetch->preflight;
        if( !result.remote_fetch->ready_to_stage )
        {
          if( vm.count( BACKUP_JSON_OPTION ) )
            std::cout << configured_restore_result_to_json( result );
          else
            std::cout << configured_restore_result_to_text( result );
          return EXIT_FAILURE;
        }
      }
      else
      {
        result.preflight = node::backup::build_local_restore_preflight(
          cfg.backup.local.directory,
          basedir,
          backup_id == "latest" ? std::string{} : backup_id );
        if( !result.preflight.ready_to_restore )
        {
          if( vm.count( BACKUP_JSON_OPTION ) )
            std::cout << configured_restore_result_to_json( result );
          else
            std::cout << configured_restore_result_to_text( result );
          return EXIT_FAILURE;
        }
      }

      const auto output = vm[ BACKUP_OUTPUT_OPTION ].as< std::string >();
      result.stage = node::backup::stage_local_restore_snapshot(
        cfg.backup.local.directory,
        basedir,
        backup_id == "latest" ? std::string{} : backup_id,
        output,
        cli_restore_stage_progress( vm.count( BACKUP_JSON_OPTION ) > 0, "restore-stage" ) );
      result.activation = node::backup::activate_staged_restore_snapshot(
        result.stage->staging_dir,
        basedir );
      result.ok = true;
      std::ostringstream start_command;
      start_command << "teleno_node --basedir " << shell_quote( basedir )
                    << " --config " << shell_quote( config_path )
                    << " --disable block_producer";
      result.observer_start_command = start_command.str();

      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << configured_restore_result_to_json( result );
      else
        std::cout << configured_restore_result_to_text( result );
      return EXIT_SUCCESS;
    }

    if( vm.count( BACKUP_RESTORE_PREFLIGHT_OPTION ) )
    {
      const auto backup_id = vm[ BACKUP_ID_OPTION ].as< std::string >();
      if( !cfg.backup.local.enabled )
        throw std::runtime_error( "--backup-restore-preflight requires backup.local.enabled=true" );
      if( cfg.backup.local.directory.empty() )
        throw std::runtime_error( "--backup-restore-preflight requires backup.local.directory" );

      auto result = node::backup::build_local_restore_preflight(
        cfg.backup.local.directory,
        basedir,
        backup_id == "latest" ? std::string{} : backup_id );
      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << node::backup::restore_preflight_result_to_json( result );
      else
        std::cout << node::backup::restore_preflight_result_to_text( result );
      return result.ready_to_restore ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if( vm.count( BACKUP_RESTORE_STAGE_OPTION ) )
    {
      const auto backup_id = vm[ BACKUP_ID_OPTION ].as< std::string >();
      if( !cfg.backup.local.enabled )
        throw std::runtime_error( "--backup-restore-stage requires backup.local.enabled=true" );
      if( cfg.backup.local.directory.empty() )
        throw std::runtime_error( "--backup-restore-stage requires backup.local.directory" );

      const auto output = vm[ BACKUP_OUTPUT_OPTION ].as< std::string >();
      auto result = node::backup::stage_local_restore_snapshot(
        cfg.backup.local.directory,
        basedir,
        backup_id == "latest" ? std::string{} : backup_id,
        output,
        cli_restore_stage_progress( vm.count( BACKUP_JSON_OPTION ) > 0, "restore-stage" ) );
      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << node::backup::restore_stage_result_to_json( result );
      else
        std::cout << node::backup::restore_stage_result_to_text( result );
      return EXIT_SUCCESS;
    }

    if( vm.count( BACKUP_RESTORE_ACTIVATE_OPTION ) )
    {
      auto staging_dir = std::filesystem::path( vm[ BACKUP_OUTPUT_OPTION ].as< std::string >() );
      if( staging_dir.empty() )
      {
        if( !cfg.backup.local.enabled )
          throw std::runtime_error( "--backup-restore-activate without --backup-output requires backup.local.enabled=true" );
        if( cfg.backup.local.directory.empty() )
          throw std::runtime_error( "--backup-restore-activate without --backup-output requires backup.local.directory" );
        auto preflight = node::backup::build_local_restore_preflight( cfg.backup.local.directory, basedir );
        staging_dir = basedir / ".teleno-restore-staging" / preflight.backup_id;
      }

      auto result = node::backup::activate_staged_restore_snapshot( staging_dir, basedir );
      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << node::backup::restore_activation_result_to_json( result );
      else
        std::cout << node::backup::restore_activation_result_to_text( result );
      return EXIT_SUCCESS;
    }

    if( vm.count( BACKUP_RESTORE_FETCH_OPTION ) )
    {
      if( !cfg.backup.local.enabled )
        throw std::runtime_error( "--backup-restore-fetch requires backup.local.enabled=true" );
      if( cfg.backup.local.directory.empty() )
        throw std::runtime_error( "--backup-restore-fetch requires backup.local.directory" );
      if( !cfg.backup.remote.enabled )
        throw std::runtime_error( "--backup-restore-fetch requires backup.remote.enabled=true" );
      if( cfg.backup.remote.directory.empty() )
        throw std::runtime_error( "--backup-restore-fetch requires backup.remote.directory" );
      if( !cfg.backup.ssh.enabled )
        throw std::runtime_error( "--backup-restore-fetch requires backup.ssh.enabled=true" );

      const auto backup_id = vm[ BACKUP_ID_OPTION ].as< std::string >();
      auto result = node::backup::fetch_restore_snapshot_with_managed_sftp(
        cfg.backup.local.directory,
        basedir,
        cfg.backup.ssh,
        cfg.backup.remote,
        backup_id,
        cli_sftp_transfer_options( vm.count( BACKUP_JSON_OPTION ) > 0 ) );
      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << node::backup::sftp_restore_fetch_result_to_json( result );
      else
        std::cout << node::backup::sftp_restore_fetch_result_to_text( result );
      return result.ready_to_stage ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // ── Initialize logging ──
    koinos::initialize_logging( node::node_name(), {}, cfg.log_level );
    LOG( info ) << "[node] " << node::node_name() << " v" << node::build_version() << " starting";
    LOG( info ) << "[node] basedir: " << basedir.string();
    LOG( info ) << "[node] config: " << config_path.string();

    if( node::backup::has_pending_restore_activation_request( basedir ) )
    {
      const auto intent = node::backup::read_pending_restore_activation_request( basedir );
      LOG( info ) << "[backup_restore] Pending restore activation found before DB open: "
                  << intent->intent_path.string();
      auto result = node::backup::activate_pending_restore_activation_request( basedir );
      LOG( info ) << "[backup_restore] Activated pending restore before startup"
                  << " backup_id=" << result.backup_id
                  << " pre_restore_dir=" << result.pre_restore_dir.string()
                  << " observer_first=" << ( result.start_as_observer_first ? "true" : "false" );
    }

    // Log enabled features
    for( const auto& [name, enabled]: cfg.features )
    {
      if( enabled )
        LOG( info ) << "[features] " << name << ": enabled";
    }

    LOG( info ) << "[runtime] Thread topology: main_ioc=1"
                << " chain_jobs=" << std::max< uint64_t >( 1, cfg.chain_jobs )
                << " jsonrpc_session_limit=" << std::max< uint64_t >( 1, cfg.jsonrpc_jobs )
                << " grpc_pollers=" << std::max< uint64_t >( 1, cfg.grpc_jobs )
                << " p2p_requested_io_threads=" << std::max< uint64_t >( 1, cfg.p2p_jobs )
                << " p2p_effective_io_threads=1"
                << " p2p_sync_threads=per-peer"
                << " p2p_peer_log_interval_seconds=" << cfg.p2p_peer_log_interval_seconds
                << " rocksdb_background_jobs=" << std::max< uint64_t >( 1, cfg.rocksdb_max_background_jobs );

    // ── Core objects ──
    node::EventBus event_bus;
    node::ServiceRegistry registry;

    // ── io_context instances (Phase 0 threading model) ──
    boost::asio::io_context main_ioc;
    boost::asio::io_context chain_ioc;

    // ── Phase 1: Chain component (requires chain library) ──

    chain::fork_resolution_algorithm fork_algo = chain::fork_resolution_algorithm::fifo;
    if( cfg.fork_algorithm == "pob" )
      fork_algo = chain::fork_resolution_algorithm::pob;
    else if( cfg.fork_algorithm == "block-time" )
      fork_algo = chain::fork_resolution_algorithm::block_time;

    chain::genesis_data genesis;
    {
      auto genesis_path = basedir / "chain" / "genesis_data.json";
      if( !std::filesystem::exists( genesis_path ) )
        genesis_path = basedir / "genesis_data.json";

      if( std::filesystem::exists( genesis_path ) )
      {
        std::ifstream ifs( genesis_path );
        std::string json_str( ( std::istreambuf_iterator< char >( ifs ) ),
                                std::istreambuf_iterator< char >() );
        google::protobuf::util::JsonStringToMessage( json_str, &genesis );
        LOG( info ) << "[chain] Loaded genesis data from " << genesis_path.string();
      }
      else
      {
        LOG( warning ) << "[chain] Genesis data not found at " << genesis_path.string();
      }
    }

    std::optional< uint64_t > pending_limit;
    if( !cfg.disable_pending_transaction_limit )
      pending_limit = cfg.pending_transaction_limit;

    node::storage::RocksDBManager storage_db;
    chain::controller controller(
      cfg.read_compute_bandwidth_limit,
      64'000,
      pending_limit
    );

    auto state_dir = basedir / "chain" / "blockchain";

    // ── Mempool ──
    koinos::mempool::mempool mempool_impl;
    node::MempoolAdapter mempool_adapter( mempool_impl );

    // ── Phase 2: RocksDB + Block Store ──
    storage_db.open( basedir, cfg );
    auto* raw_db = storage_db.db();

    const auto migration_modes = static_cast< int >( vm.count( MIGRATE_CHAIN_DB_OPTION ) > 0 )
                               + static_cast< int >( vm.count( ROLLBACK_CHAIN_DB_MIGRATION_OPTION ) > 0 )
                               + static_cast< int >( vm.count( COMPACT_DB_OPTION ) > 0 )
                               + static_cast< int >( vm.count( BACKUP_CHECKPOINT_OPTION ) > 0 )
                               + static_cast< int >( vm.count( BACKUP_CREATE_OPTION ) > 0 )
                               + static_cast< int >( vm.count( BACKUP_CREATE_LOCAL_OPTION ) > 0 );
    if( migration_modes > 1 )
      throw std::runtime_error( "choose only one storage mutation command" );

    if( vm.count( BACKUP_CHECKPOINT_OPTION ) )
    {
      const auto output = vm[ BACKUP_OUTPUT_OPTION ].as< std::string >();
      if( output.empty() )
        throw std::runtime_error( "--backup-checkpoint requires --backup-output <directory>" );

      const auto chain_layout = storage_db.read_metadata( "layout.chain_storage" );
      if( chain_layout != "unified" )
        throw std::runtime_error( "--backup-checkpoint currently requires unified chain storage; current layout is "
                                  + ( chain_layout.empty() ? std::string( "unknown" ) : chain_layout ) );

      node::backup::CheckpointManager checkpoint_manager( basedir, storage_db );
      auto checkpoint_result = checkpoint_manager.create_checkpoint( output );

      if( vm.count( BACKUP_JSON_OPTION ) )
      {
        std::cout << "{\n"
                  << "  \"checkpoint_dir\": \"" << checkpoint_result.checkpoint_dir.string() << "\",\n"
                  << "  \"db_dir\": \"" << checkpoint_result.db_dir.string() << "\",\n"
                  << "  \"file_count\": " << checkpoint_result.file_count << ",\n"
                  << "  \"total_bytes\": " << checkpoint_result.total_bytes << "\n"
                  << "}\n";
      }
      else
      {
        std::cout << "Created RocksDB checkpoint\n"
                  << "checkpoint_dir: " << checkpoint_result.checkpoint_dir.string() << "\n"
                  << "db_dir: " << checkpoint_result.db_dir.string() << "\n"
                  << "file_count: " << checkpoint_result.file_count << "\n"
                  << "total_bytes: " << checkpoint_result.total_bytes << "\n";
      }
      return EXIT_SUCCESS;
    }

    if( vm.count( BACKUP_CREATE_OPTION ) )
    {
      auto plan = node::backup::build_backup_dry_run_plan( cfg, basedir, config_path );
      if( plan.has_errors() )
      {
        if( vm.count( BACKUP_JSON_OPTION ) )
          std::cout << node::backup::backup_dry_run_plan_to_json( plan );
        else
          std::cout << node::backup::backup_dry_run_plan_to_text( plan );
        return EXIT_FAILURE;
      }
      if( !cfg.backup.local.enabled )
        throw std::runtime_error(
          "--backup-create currently requires backup.local.enabled=true because remote upload is staged from the local object repository" );
      if( cfg.backup.local.directory.empty() )
        throw std::runtime_error( "--backup-create requires backup.local.directory" );
      if( cfg.backup.remote.enabled )
      {
        if( !cfg.backup.ssh.enabled )
          throw std::runtime_error( "--backup-create remote upload requires backup.ssh.enabled=true" );
        if( cfg.backup.remote.directory.empty() )
          throw std::runtime_error( "--backup-create remote upload requires backup.remote.directory" );
      }

      node::backup::LocalSnapshotResult snapshot_result;
      {
        node::backup::BackupService backup_service( cfg, basedir, config_path, storage_db );
        snapshot_result = backup_service.create_local_snapshot();
      }

      // Remote upload works from the immutable local object repository. Keep the
      // RocksDB lock only while the checkpoint/local snapshot is being created.
      storage_db.close();

      std::optional< node::backup::SftpUploadResult > remote_upload;
      if( cfg.backup.remote.enabled )
      {
        remote_upload = node::backup::upload_latest_snapshot_with_managed_sftp(
          cfg.backup.local.directory,
          cfg.backup.ssh,
          cfg.backup.remote,
          cli_sftp_transfer_options( vm.count( BACKUP_JSON_OPTION ) > 0 ) );
      }

      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << configured_backup_result_to_json( snapshot_result, remote_upload );
      else
        std::cout << configured_backup_result_to_text( snapshot_result, remote_upload );

      return EXIT_SUCCESS;
    }

    if( vm.count( BACKUP_CREATE_LOCAL_OPTION ) )
    {
      auto plan = node::backup::build_backup_dry_run_plan( cfg, basedir, config_path );
      if( plan.has_errors() )
      {
        if( vm.count( BACKUP_JSON_OPTION ) )
          std::cout << node::backup::backup_dry_run_plan_to_json( plan );
        else
          std::cout << node::backup::backup_dry_run_plan_to_text( plan );
        return EXIT_FAILURE;
      }
      node::backup::LocalSnapshotResult snapshot_result;
      {
        node::backup::BackupService backup_service( cfg, basedir, config_path, storage_db );
        snapshot_result = backup_service.create_local_snapshot();
      }
      storage_db.close();
      if( vm.count( BACKUP_JSON_OPTION ) )
        std::cout << node::backup::local_snapshot_result_to_json( snapshot_result );
      else
        std::cout << node::backup::local_snapshot_result_to_text( snapshot_result );

      return EXIT_SUCCESS;
    }

    if( vm.count( MIGRATE_CHAIN_DB_OPTION ) )
    {
      auto result = node::storage::migrate_legacy_chain_db_to_unified( basedir, storage_db );
      std::cout << "Migrated legacy chain DB to unified RocksDB\n";
      std::cout << "source: " << result.source_path.string() << "\n";
      std::cout << "backup: " << result.backup_path.string() << "\n";
      std::cout << "objects: count=" << result.objects.record_count
                << " bytes=" << result.objects.byte_count
                << " sha256=" << result.objects.source_hash << "\n";
      std::cout << "metadata: count=" << result.metadata.record_count
                << " bytes=" << result.metadata.byte_count
                << " sha256=" << result.metadata.source_hash << "\n";
      return EXIT_SUCCESS;
    }

    if( vm.count( ROLLBACK_CHAIN_DB_MIGRATION_OPTION ) )
    {
      auto result = node::storage::rollback_unified_chain_db_migration( basedir, storage_db );
      std::cout << "Rolled back unified chain DB migration\n";
      std::cout << "restored: " << result.restored_path.string() << "\n";
      std::cout << "from backup: " << result.backup_path.string() << "\n";
      return EXIT_SUCCESS;
    }

    if( vm.count( COMPACT_CF_OPTION ) && !vm.count( COMPACT_DB_OPTION ) )
      throw std::runtime_error( "--compact-cf requires --compact-db" );
    if( vm.count( ALL_OPTION ) && !vm.count( COMPACT_DB_OPTION ) )
      throw std::runtime_error( "--all requires --compact-db" );

    if( vm.count( COMPACT_DB_OPTION ) )
    {
      if( vm.count( ALL_OPTION ) && vm.count( COMPACT_CF_OPTION ) )
        throw std::runtime_error( "choose either --all or --compact-cf, not both" );
      if( !vm.count( ALL_OPTION ) && !vm.count( COMPACT_CF_OPTION ) )
        throw std::runtime_error( "--compact-db requires --all or at least one --compact-cf" );

      if( vm.count( ALL_OPTION ) )
      {
        storage_db.compact_all_column_families();
        std::cout << "Compacted all shared RocksDB column families\n";
      }
      else
      {
        for( const auto& cf_name: vm[ COMPACT_CF_OPTION ].as< std::vector< std::string > >() )
        {
          const auto cf = node::storage::column_family_from_name( cf_name );
          storage_db.compact_column_family( cf );
          std::cout << "Compacted RocksDB column family: " << node::storage::column_family_name( cf ) << "\n";
        }
      }

      if( vm.count( STORAGE_REPORT_OPTION ) )
        print_storage_report( std::cout, basedir, state_dir, storage_db );
      return EXIT_SUCCESS;
    }

    if( vm.count( STORAGE_REPORT_OPTION ) )
    {
      print_storage_report( std::cout, basedir, state_dir, storage_db );
      return EXIT_SUCCESS;
    }

    // Block store using RocksDB column families
    node::block_store::BlockStore block_store_impl(
      raw_db,
      storage_db.handle( node::storage::ColumnFamily::blocks ),
      storage_db.handle( node::storage::ColumnFamily::block_meta ) );

    // Phase 4: Contract meta store + Transaction store
    node::contract_meta_store::ContractMetaStore contract_meta_impl(
      raw_db,
      storage_db.handle( node::storage::ColumnFamily::contract_meta ) );
    node::transaction_store::TransactionStore transaction_store_impl(
      raw_db,
      storage_db.handle( node::storage::ColumnFamily::transaction_index ) );
    node::account_history::AccountHistory account_history_impl(
      raw_db,
      storage_db.handle( node::storage::ColumnFamily::account_history ) );

    // Chain adapter implements IChain
    ChainAdapter chain_adapter( controller );
    const auto chain_storage_layout = storage_db.read_metadata( "layout.chain_storage" );

    // Register block store component
    if( cfg.is_enabled( "block_store" ) )
    {
      registry.add(
        "block_store",
        [&]() {
          block_store_impl.initialize();
          LOG( info ) << "[block_store] Initialized";
        },
        [&]() {
          LOG( info ) << "[block_store] Stopped";
        }
      );
    }

    // Backup restore detection — temporarily force verify-blocks
    auto backup_marker = basedir / ".backup-just-restored";
    bool force_verify  = false;
    if( std::filesystem::exists( backup_marker ) )
    {
      LOG( info ) << "[chain] Backup restore detected — enabling verify-blocks for merkle correction";
      if( cfg.is_enabled( "block_producer" ) )
        LOG( warning ) << "[block_producer] Backup restore first start detected — disabling block production for observer recovery";
      cfg.features[ "block_producer" ] = false;
      force_verify = true;
      std::filesystem::remove( backup_marker );
    }

    bool effective_verify = cfg.verify_blocks || force_verify;

    // Register chain component
    if( cfg.is_enabled( "chain" ) )
    {
      registry.add(
        "chain",
        [&]() {
          if( chain_storage_layout == "unified" )
          {
            auto backend = std::make_shared< state_db::backends::rocksdb::rocksdb_backend >();
            backend->open(
              *raw_db,
              *storage_db.handle( node::storage::ColumnFamily::default_state ),
              *storage_db.handle( node::storage::ColumnFamily::chain_state ),
              *storage_db.handle( node::storage::ColumnFamily::chain_metadata ) );
            controller.open( std::move( backend ), genesis, fork_algo, cfg.reset );
            LOG( info ) << "[chain] State DB opened from shared RocksDB column families";
          }
          else
          {
            std::filesystem::create_directories( state_dir );
            controller.open( state_dir, genesis, fork_algo, cfg.reset );
            LOG( info ) << "[chain] State DB opened at " << state_dir.string();
          }
        },
        [&]() {
          controller.close();
        }
      );
    }

    // Register block producer (optional)
    // The monolith assembles and signs blocks locally, then submits them
    // through chain.propose_block(), which applies the block and broadcasts
    // block acceptance through the in-process EventBus.
    std::atomic< bool > producer_running{ false };
    std::atomic< bool > producer_gossip_enabled{ false };
    std::thread producer_thread;
    std::unique_ptr< node::block_production::BlockProducer > block_producer;

    if( cfg.is_enabled( "block_producer" ) )
    {
      // Validate producer config
      std::filesystem::path key_file = cfg.block_producer_private_key_file;
      if( key_file.empty() )
      {
        key_file = basedir / "block_producer" / "private.key";
      }
      else if( key_file.is_relative() )
      {
        if( key_file.has_parent_path() )
        {
          key_file = basedir / key_file;
        }
        else
        {
          auto block_producer_key_file = basedir / "block_producer" / key_file;
          auto basedir_key_file = basedir / key_file;
          key_file = std::filesystem::exists( block_producer_key_file ) ? block_producer_key_file
                   : std::filesystem::exists( basedir_key_file ) ? basedir_key_file
                   : block_producer_key_file;
        }
      }

      node::block_production::ProducerConfig producer_cfg;
      producer_cfg.algorithm                = cfg.block_producer_algorithm;
      producer_cfg.producer_address         = cfg.block_producer_address;
      producer_cfg.resources_lower_bound    = cfg.block_producer_resources_lower_bound;
      producer_cfg.resources_upper_bound    = cfg.block_producer_resources_upper_bound;
      producer_cfg.max_inclusion_attempts   = cfg.block_producer_max_inclusion_attempts;

      for( const auto& proposal: cfg.block_producer_approved_proposals )
        producer_cfg.approved_proposals.push_back( util::from_hex< std::string >( proposal ) );

      auto signing_key = node::block_production::load_or_create_private_key_file( key_file );
      block_producer = std::make_unique< node::block_production::BlockProducer >(
        chain_adapter, mempool_adapter, std::move( signing_key ), producer_cfg );
      block_producer->write_public_key_file( basedir / "block_producer" / "public.key" );

      LOG( info ) << "[block_producer] Private key: " << key_file;
      LOG( info ) << "[block_producer] Public address: " << block_producer->public_address();
      if( !cfg.block_producer_address.empty() )
        LOG( info ) << "[block_producer] Producer address: " << cfg.block_producer_address;
      LOG( info ) << "[block_producer] Algorithm: " << cfg.block_producer_algorithm;
      LOG( info ) << "[block_producer] Resource utilization lower/upper bounds: "
                  << cfg.block_producer_resources_lower_bound << "%/"
                  << cfg.block_producer_resources_upper_bound << "%";
      LOG( info ) << "[block_producer] Max inclusion attempts: "
                  << cfg.block_producer_max_inclusion_attempts;
      LOG( info ) << "[block_producer] Gossip production gate: "
                  << ( cfg.block_producer_gossip_production ? "enabled" : "disabled" );

      registry.add(
        "block_producer",
        [&]() {
          producer_running = true;
          producer_gossip_enabled = !cfg.block_producer_gossip_production;
          producer_thread  = std::thread( [&]() {
              LOG( info ) << "[block_producer] Production loop started";
              uint64_t blocks_produced = 0;
              while( producer_running )
              {
                std::chrono::milliseconds retry_after{ 5000 };

                if( cfg.block_producer_gossip_production && !producer_gossip_enabled )
                {
                  std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
                  continue;
                }

                try
                {
                  auto result = block_producer->produce_once();
                  retry_after = result.retry_after;

                  if( result.removed_failed_transactions > 0 )
                  {
                    LOG( info ) << "[block_producer] Removed "
                                << result.removed_failed_transactions
                                << " failed transaction(s) before acceptance";
                  }

                  if( result.status == node::block_production::ProductionResult::Status::produced )
                  {
                    ++blocks_produced;
                    LOG( info ) << "[block_producer] Produced block #" << blocks_produced
                                << " at height " << result.height;
                  }
                }
                catch( const std::exception& e )
                {
                  retry_after = std::chrono::seconds( 5 );
                  LOG( warning ) << "[block_producer] " << e.what();
                }

                auto sleep_ms = std::max< int64_t >( 10, retry_after.count() );
                for( int64_t slept = 0; slept < sleep_ms && producer_running; slept += 100 )
                {
                  auto chunk = std::min< int64_t >( 100, sleep_ms - slept );
                  std::this_thread::sleep_for( std::chrono::milliseconds( chunk ) );
                }
              }
              LOG( info ) << "[block_producer] Stopped (produced " << blocks_produced << " blocks)";
            } );
          },
          [&]() {
            producer_running = false;
            if( producer_thread.joinable() )
              producer_thread.join();
        }
      );
    }

    // ── EventBus wiring ──

    // Block accepted → block store, mempool (future), p2p (future)
    event_bus.on_block_accepted.connect(
      [&block_store_impl]( const broadcast::block_accepted& ba ) {
        block_store_impl.handle_block_accepted( ba );
      }
    );

    // Contract meta store subscribes to block events
    if( cfg.is_enabled( "contract_meta_store" ) )
    {
      event_bus.on_block_accepted.connect(
        [&contract_meta_impl]( const broadcast::block_accepted& ba ) {
          contract_meta_impl.handle_block_accepted( ba );
        }
      );
    }

    // Transaction store subscribes to block events
    if( cfg.is_enabled( "transaction_store" ) )
    {
      event_bus.on_block_accepted.connect(
        [&transaction_store_impl]( const broadcast::block_accepted& ba ) {
          transaction_store_impl.handle_block_accepted( ba );
        }
      );
    }

    // Account history subscribes to block events
    if( cfg.is_enabled( "account_history" ) )
    {
      event_bus.on_block_accepted.connect(
        [&account_history_impl]( const broadcast::block_accepted& ba ) {
          account_history_impl.handle_block_accepted( ba );
        }
      );
    }

    // Mempool subscribes to accepted transactions, block events, and local expiry.
    boost::asio::steady_timer mempool_prune_timer( main_ioc );
    std::function< void( const boost::system::error_code& ) > mempool_prune_tick;
    if( cfg.is_enabled( "mempool" ) )
    {
      event_bus.on_transaction_accepted.connect(
        [&mempool_adapter]( const broadcast::transaction_accepted& ta ) {
          try
          {
            auto rc_used = mempool_adapter.add_transaction_accepted( ta );
            LOG( debug ) << "[mempool] accepted transaction id="
                         << util::to_hex( ta.transaction().id() )
                         << " reserved_rc=" << rc_used;
          }
          catch( const std::exception& e )
          {
            LOG( warning ) << "[mempool] failed to add accepted transaction: " << e.what();
          }
        }
      );
      event_bus.on_block_accepted.connect(
        [&mempool_impl]( const broadcast::block_accepted& ba ) {
          mempool_impl.handle_block( ba );
        }
      );
      event_bus.on_block_irreversible.connect(
        [&mempool_impl]( const broadcast::block_irreversible& bi ) {
          mempool_impl.handle_irreversibility( bi );
        }
      );

      mempool_prune_tick = [&]( const boost::system::error_code& ec ) {
        if( ec )
          return;

        try
        {
          auto pruned = mempool_adapter.prune( std::chrono::seconds( cfg.mempool_transaction_expiration ) );
          if( pruned )
            LOG( debug ) << "[mempool] pruned expired pending transactions count=" << pruned;
        }
        catch( const std::exception& e )
        {
          LOG( warning ) << "[mempool] prune failed: " << e.what();
        }

        mempool_prune_timer.expires_after( std::chrono::seconds( 1 ) );
        mempool_prune_timer.async_wait( mempool_prune_tick );
      };
      mempool_prune_timer.expires_after( std::chrono::seconds( 1 ) );
      mempool_prune_timer.async_wait( mempool_prune_tick );
    }

    event_bus.on_block_accepted.connect(
      [&]( const broadcast::block_accepted& ba ) {
        LOG( debug ) << "[event_bus] block_accepted height="
                     << ba.block().header().height();
      }
    );

    event_bus.on_block_irreversible.connect(
      [&]( const broadcast::block_irreversible& bi ) {
        LOG( debug ) << "[event_bus] block_irreversible height="
                     << bi.topology().height();
      }
    );

    event_bus.on_gossip_status.connect(
      [&]( bool enabled ) {
        producer_gossip_enabled = enabled;
        LOG( info ) << "[block_producer] Gossip production gate "
                    << ( enabled ? "opened" : "closed" );
      }
    );

    // ── Phase 5: P2P ──
    std::unique_ptr< node::p2p::P2PNode > p2p_node;
    if( cfg.is_enabled( "p2p" ) )
    {
#ifdef KOINOS_HAS_LIBP2P
      // cpp-libp2p transport available — create real P2P node
	      node::p2p::Libp2pTransport::Config transport_cfg;
	      transport_cfg.listen_address = cfg.p2p_listen;
	      transport_cfg.discovery_peers = cfg.p2p_seeds;
	      transport_cfg.enable_dht = cfg.p2p_peer_discovery_enabled;
	      transport_cfg.requested_io_threads = static_cast< unsigned int >( std::max< uint64_t >( 1, cfg.p2p_jobs ) );

      auto transport = std::make_unique< node::p2p::Libp2pTransport >( transport_cfg );

	      node::p2p::P2POptions p2p_opts;
	      p2p_opts.seed_reconnect_interval = std::chrono::seconds( cfg.p2p_seed_reconnect_interval_seconds );
	      p2p_opts.peer_discovery_enabled   = cfg.p2p_peer_discovery_enabled;
	      p2p_opts.target_peer_count        = static_cast< uint32_t >( cfg.p2p_target_peer_count );
	      p2p_opts.max_peer_candidates      = static_cast< uint32_t >( cfg.p2p_max_peer_candidates );
	      p2p_opts.max_candidate_dials_per_cycle =
	        static_cast< uint32_t >( cfg.p2p_max_candidate_dials_per_cycle );
	      p2p_opts.peer_acquisition_interval = std::chrono::seconds( cfg.p2p_peer_acquisition_interval_seconds );
	      p2p_opts.candidate_redial_interval = std::chrono::seconds( cfg.p2p_candidate_redial_interval_seconds );
	      p2p_opts.peer_log_interval        = std::chrono::seconds( cfg.p2p_peer_log_interval_seconds );
	      p2p_opts.always_enable_gossip    = cfg.p2p_force_gossip;
	      p2p_opts.always_disable_gossip   = cfg.p2p_disable_gossip;
      for( const auto& checkpoint: cfg.p2p_checkpoints )
        p2p_opts.checkpoints.push_back( { checkpoint.block_height, checkpoint.block_id } );
      for( const auto& seed: cfg.p2p_seeds )
      {
        auto marker = seed.rfind( "/p2p/" );
        if( marker != std::string::npos )
          p2p_opts.seed_peers.push_back( { seed.substr( marker + 5 ), seed } );
      }
      p2p_node = std::make_unique< node::p2p::P2PNode >(
        p2p_opts, &chain_adapter, &block_store_impl, &event_bus, std::move( transport ) );

      registry.add(
        "p2p",
        [&]() { p2p_node->start(); },
        [&]() { p2p_node->stop(); }
      );
#else
      // Fallback: use Go P2P binary as sidecar process
      node::p2p::GoBridgeTransport::Config go_cfg;
      // Look for Go P2P binary in standard locations
      auto go_p2p_path = basedir / ".." / "koinos-p2p" / "build" / "bin" / "koinos-p2p";
      if( !std::filesystem::exists( go_p2p_path ) )
        go_p2p_path = std::filesystem::path( "/usr/local/bin/koinos-p2p" );

      go_cfg.go_p2p_binary  = go_p2p_path.string();
      go_cfg.basedir        = basedir.string();
      go_cfg.listen_address = cfg.p2p_listen;
      go_cfg.seed_peers     = cfg.p2p_seeds;

      auto transport = std::make_unique< node::p2p::GoBridgeTransport >( go_cfg );

	      node::p2p::P2POptions p2p_opts;
	      p2p_opts.seed_reconnect_interval = std::chrono::seconds( cfg.p2p_seed_reconnect_interval_seconds );
	      p2p_opts.peer_discovery_enabled   = cfg.p2p_peer_discovery_enabled;
	      p2p_opts.target_peer_count        = static_cast< uint32_t >( cfg.p2p_target_peer_count );
	      p2p_opts.max_peer_candidates      = static_cast< uint32_t >( cfg.p2p_max_peer_candidates );
	      p2p_opts.max_candidate_dials_per_cycle =
	        static_cast< uint32_t >( cfg.p2p_max_candidate_dials_per_cycle );
	      p2p_opts.peer_acquisition_interval = std::chrono::seconds( cfg.p2p_peer_acquisition_interval_seconds );
	      p2p_opts.candidate_redial_interval = std::chrono::seconds( cfg.p2p_candidate_redial_interval_seconds );
	      p2p_opts.peer_log_interval        = std::chrono::seconds( cfg.p2p_peer_log_interval_seconds );
	      p2p_opts.always_enable_gossip    = cfg.p2p_force_gossip;
	      p2p_opts.always_disable_gossip   = cfg.p2p_disable_gossip;
      for( const auto& checkpoint: cfg.p2p_checkpoints )
        p2p_opts.checkpoints.push_back( { checkpoint.block_height, checkpoint.block_id } );
      for( const auto& seed: cfg.p2p_seeds )
      {
        auto marker = seed.rfind( "/p2p/" );
        if( marker != std::string::npos )
          p2p_opts.seed_peers.push_back( { seed.substr( marker + 5 ), seed } );
      }
      p2p_node = std::make_unique< node::p2p::P2PNode >(
        p2p_opts, &chain_adapter, &block_store_impl, &event_bus, std::move( transport ) );

      registry.add(
        "p2p",
        [&]() { p2p_node->start(); },
        [&]() { p2p_node->stop(); }
      );
#endif
    }

    // ── Phase 6: gRPC server ──
    std::unique_ptr< node::grpc_server::GRPCServer > grpc_srv;
    if( cfg.is_enabled( "grpc" ) )
    {
      grpc_srv = std::make_unique< node::grpc_server::GRPCServer >(
        &chain_adapter,
        &mempool_adapter,
        &block_store_impl,
        cfg.is_enabled( "contract_meta_store" ) ? &contract_meta_impl : nullptr,
        cfg.is_enabled( "transaction_store" ) ? &transaction_store_impl : nullptr,
        cfg.is_enabled( "account_history" ) ? &account_history_impl : nullptr,
        cfg.grpc_listen,
        static_cast< unsigned int >( cfg.grpc_jobs ),
        &producer_gossip_enabled );
      registry.add(
        "grpc",
        [&]() { grpc_srv->start(); },
        [&]() { grpc_srv->stop(); }
      );
    }

    // ── Phase 3: JSON-RPC server ──
    // Parse listen address: "host:port", "port", or legacy multiaddr
    // values such as "/tcp/8080" and "/ip4/0.0.0.0/tcp/8080".
    auto [jsonrpc_host, jsonrpc_port] = parse_jsonrpc_listen( cfg.jsonrpc_listen );

    // Wire MonolithClient so the chain controller routes RPC/broadcast
    // through IBlockStore + EventBus instead of AMQP
    auto monolith_client = std::make_shared< node::MonolithRpcClient >(
      &block_store_impl, &mempool_adapter, &event_bus );
    controller.set_client( monolith_client );

    // Indexer will run after registry.start_all() — see below

    std::unique_ptr< node::jsonrpc::JSONRPCServer > jsonrpc_server;
    if( cfg.is_enabled( "jsonrpc" ) )
    {
      jsonrpc_server = std::make_unique< node::jsonrpc::JSONRPCServer >(
        &chain_adapter,
        &mempool_adapter,
        &block_store_impl,
        cfg.is_enabled( "contract_meta_store" ) ? &contract_meta_impl : nullptr,
        cfg.is_enabled( "transaction_store" ) ? &transaction_store_impl : nullptr,
        cfg.is_enabled( "account_history" ) ? &account_history_impl : nullptr,
        jsonrpc_host,
        jsonrpc_port,
        static_cast< unsigned int >( cfg.jsonrpc_jobs ),
        node::RpcAccessPolicy{ cfg.rpc_blacklist, cfg.rpc_whitelist }
      );

      registry.add(
        "jsonrpc",
        [&]() { jsonrpc_server->start(); },
        [&]() { jsonrpc_server->stop(); }
      );
    }

    std::unique_ptr< node::backup::BackupService > backup_service_runtime;
    std::unique_ptr< node::backup::BackupAdminServer > backup_admin_server;
    std::unique_ptr< node::backup::BackupScheduler > backup_scheduler;
    std::atomic< bool > restore_activation_shutdown_requested{ false };
    if( cfg.backup.admin.enabled || cfg.backup.schedule.enabled )
    {
      backup_service_runtime = std::make_unique< node::backup::BackupService >(
        cfg,
        basedir,
        config_path,
        storage_db );
    }

    if( cfg.backup.admin.enabled )
    {
      auto [admin_host, admin_port] = parse_jsonrpc_listen( cfg.backup.admin.listen );
      if( cfg.backup.admin.listen.find( ':' ) == std::string::npos
          && cfg.backup.admin.listen.find( "/tcp/" ) == std::string::npos )
        admin_host = "127.0.0.1";

      const auto admin_token = read_backup_admin_token_file( cfg.backup.admin.token_file );

      node::backup::PeerSnapshotProvider peer_snapshot_provider = [&]() {
        node::backup::AdminPeerSnapshot snapshot;
        snapshot.p2p_running = static_cast< bool >( p2p_node );
        if( !p2p_node )
          return snapshot;

        for( const auto& peer: p2p_node->connected_peers() )
          snapshot.connected.push_back( { peer.id, peer.address } );
        for( const auto& peer: p2p_node->known_peers() )
          snapshot.known.push_back( { peer.id, peer.address } );
        return snapshot;
      };

      backup_admin_server = std::make_unique< node::backup::BackupAdminServer >(
        backup_service_runtime.get(),
        admin_host,
        admin_port,
        static_cast< unsigned int >( std::max< uint64_t >( 1, cfg.backup.admin.jobs ) ),
        admin_token,
        std::move( peer_snapshot_provider ) );

      registry.add(
        "backup_admin",
        [&]() { backup_admin_server->start(); },
        [&]() { backup_admin_server->stop(); }
      );
    }

    if( cfg.backup.schedule.enabled )
    {
      backup_scheduler = std::make_unique< node::backup::BackupScheduler >(
        backup_service_runtime.get(),
        cfg,
        [&controller]() {
          return controller.get_head_info().head_topology().height();
        } );
    }

    // ── Signal handling ──
    auto request_runtime_stop = [&]( const std::string& reason ) {
      if( restore_activation_shutdown_requested.exchange( true ) )
        return;

      LOG( info ) << "[backup_restore] Runtime restore activation requested: " << reason;
      if( backup_scheduler )
        backup_scheduler->stop();
      registry.stop_all();
      main_ioc.stop();
    };

    boost::asio::signal_set signals( main_ioc, SIGINT, SIGTERM );
    signals.async_wait( [&]( const boost::system::error_code& ec, int sig ) {
      if( !ec )
      {
        LOG( info ) << "[node] Received signal " << sig << ", shutting down...";
        if( backup_scheduler )
          backup_scheduler->stop();
        registry.stop_all();
        main_ioc.stop();
      }
    } );

    // ── Start all registered components ──
    registry.start_all();

    // Run chain indexer AFTER all components are started (chain + block_store must be open)
    if( cfg.is_enabled( "chain" ) && cfg.is_enabled( "block_store" ) )
    {
      try
      {
        chain::indexer idx( chain_ioc, controller, monolith_client, effective_verify );
        auto future = idx.index();
        if( future.get() )
          LOG( info ) << "[chain] Indexing complete";
        else
          LOG( warning ) << "[chain] Indexing returned false";
      }
      catch( const std::exception& e )
      {
        LOG( warning ) << "[chain] Indexer: " << e.what();
      }
    }

    LOG( info ) << "[node] " << node::node_name() << " ready";
    if( backup_scheduler )
      backup_scheduler->start();

    boost::asio::steady_timer restore_activation_timer( main_ioc );
    std::function< void( const boost::system::error_code& ) > restore_activation_tick;
    restore_activation_tick = [&]( const boost::system::error_code& ec ) {
      if( ec )
        return;

      try
      {
        if( node::backup::has_pending_restore_activation_request( basedir ) )
        {
          const auto intent = node::backup::read_pending_restore_activation_request( basedir );
          request_runtime_stop( intent->intent_path.string() );
          return;
        }
      }
      catch( const std::exception& e )
      {
        LOG( warning ) << "[backup_restore] Failed while checking restore activation intent: " << e.what();
        request_runtime_stop( "invalid restore activation intent" );
        return;
      }

      restore_activation_timer.expires_after( std::chrono::seconds( 2 ) );
      restore_activation_timer.async_wait( restore_activation_tick );
    };
    restore_activation_timer.expires_after( std::chrono::seconds( 2 ) );
    restore_activation_timer.async_wait( restore_activation_tick );

    // ── Periodic metrics (every 60s) ──
    boost::asio::steady_timer metrics_timer( main_ioc );
    std::function< void( const boost::system::error_code& ) > metrics_tick;
    bool have_metrics_baseline = false;
    uint64_t previous_metrics_height = 0;
    auto previous_metrics_time = std::chrono::steady_clock::now();
    metrics_tick = [&]( const boost::system::error_code& ec ) {
      if( ec )
        return;

      try
      {
        const auto now = std::chrono::steady_clock::now();
        auto head = controller.get_head_info();
        const auto height = head.head_topology().height();
        double blocks_per_sec = 0.0;
        if( have_metrics_baseline )
        {
          const auto elapsed = std::chrono::duration< double >( now - previous_metrics_time ).count();
          if( elapsed > 0.0 && height >= previous_metrics_height )
            blocks_per_sec = static_cast< double >( height - previous_metrics_height ) / elapsed;
        }

        previous_metrics_height = height;
        previous_metrics_time = now;
        have_metrics_baseline = true;

        const auto peer_count = p2p_node ? p2p_node->connected_peer_count() : 0;
        const auto rss_bytes = current_process_rss_bytes();
        const auto rss_mb = static_cast< double >( rss_bytes ) / ( 1024.0 * 1024.0 );

        LOG( info ) << "[metrics] head_height=" << head.head_topology().height()
                    << " lib=" << head.last_irreversible_block()
                    << " blocks_per_sec=" << std::fixed << std::setprecision( 3 ) << blocks_per_sec
                    << " pending_txs=" << mempool_impl.get_pending_transactions( 0 ).size()
                    << " peer_count=" << peer_count
                    << " rss_bytes=" << rss_bytes
                    << " rss_mb=" << std::fixed << std::setprecision( 3 ) << rss_mb
                    << " components=" << registry.components().size();
      }
      catch( const std::exception& e )
      {
        LOG( warning ) << "[metrics] failed to collect metrics: " << e.what();
      }
      catch( ... )
      {
        LOG( warning ) << "[metrics] failed to collect metrics: unknown exception";
      }

      metrics_timer.expires_after( std::chrono::seconds( 60 ) );
      metrics_timer.async_wait( metrics_tick );
    };
    metrics_tick( boost::system::error_code{} );

    // ── Run main event loop ──
    main_ioc.run();

    if( backup_scheduler )
      backup_scheduler->stop();

    if( restore_activation_shutdown_requested
        || node::backup::has_pending_restore_activation_request( basedir ) )
    {
      registry.stop_all();
      backup_admin_server.reset();
      backup_service_runtime.reset();
      storage_db.close();

      auto result = node::backup::activate_pending_restore_activation_request( basedir );
      LOG( info ) << "[backup_restore] Activated pending restore after graceful runtime stop"
                  << " backup_id=" << result.backup_id
                  << " pre_restore_dir=" << result.pre_restore_dir.string()
                  << " observer_first=" << ( result.start_as_observer_first ? "true" : "false" );
      LOG( info ) << "[backup_restore] Restore activation complete; restart the node to begin observer-first recovery";
    }

    LOG( info ) << "[node] " << node::node_name() << " shutdown complete";
    return EXIT_SUCCESS;
  }
  catch( const std::exception& e )
  {
    std::cerr << "Fatal: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  catch( ... )
  {
    std::cerr << "Fatal: unknown exception" << std::endl;
    return EXIT_FAILURE;
  }
}
