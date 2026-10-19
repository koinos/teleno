#include "preflight.hpp"

#include <boost/asio.hpp>
#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <thread>
#include <sys/resource.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace koinos::node {
namespace fs = std::filesystem;

void PreflightReport::add( const std::string& id, const std::string& status,
                           const std::string& observed, const std::string& action )
{
  checks.push_back( { { "id", id }, { "status", status }, { "observed", observed }, { "action", action } } );
}

int PreflightReport::exit_code() const
{
  for( const auto& check: checks )
    if( check[ "status" ] == "fail" ) return 1;
  for( const auto& check: checks )
    if( check[ "status" ] == "error" ) return 2;
  return 0;
}

std::string PreflightReport::json() const
{
  return nlohmann::json( { { "schema_version", 1 }, { "scope", "local-startup-prerequisites" },
    { "no_known_blockers", exit_code() == 0 }, { "runtime_readiness_verified", false },
    { "exit_code", exit_code() }, { "checks", checks } } ).dump( 2 ) + "\n";
}

std::string PreflightReport::text() const
{
  std::ostringstream out;
  out << "Teleno local preflight\n";
  for( const auto& check: checks )
  {
    out << '[' << check[ "status" ].get< std::string >() << "] "
        << check[ "id" ].get< std::string >() << ": " << check[ "observed" ].get< std::string >() << '\n';
    if( !check[ "action" ].get< std::string >().empty() )
      out << "  Action: " << check[ "action" ].get< std::string >() << '\n';
  }
  out << "Result: " << ( exit_code() == 0 ? "no known blockers in completed checks" : "checks failed or incomplete" )
      << "; runtime readiness is NOT verified.\n";
  return out.str();
}

PreflightReport run_preflight( const NodeConfig& cfg )
{
  PreflightReport report;
  report.add( "config", "pass", "Configuration loaded with strict scalar and feature checks; CLI overrides applied." );
  report.add( "config.schema", "not_checked", "Unknown YAML keys and complete semantic validation are not covered.",
              "Compare configuration with the documented schema." );
  report.add( "role", cfg.is_enabled( "block_producer" ) ? "warning" : "pass",
              cfg.is_enabled( "block_producer" ) ? "Production requested by effective configuration." : "Observer configuration.",
              cfg.is_enabled( "block_producer" ) ? "Validate the observer and producer activation gate before starting." : "" );
  if( cfg.rocksdb_require_compression || cfg.rocksdb_blocks_zstd_level
      || cfg.rocksdb_blocks_zstd_dictionary_bytes )
    report.add( "storage.compression", "not_checked", "Exact linked-library compression support is not probed.", "Startup enforces compression requirements, including explicit block Zstd tuning." );
  report.add( "storage.wal", "not_checked",
              cfg.rocksdb_max_total_wal_size
                ? std::to_string( cfg.rocksdb_max_total_wal_size ) + " configured WAL flush-trigger bytes; this is a soft threshold, not a disk ceiling."
                : "Automatic RocksDB WAL flush trigger configured; retained WAL usage is not measured.",
              "Verify saved runtime options and budget WAL retention, flushing, compaction and recovery headroom." );
  if( cfg.is_enabled( "block_producer" ) &&
      ( cfg.block_producer_resources_lower_bound > cfg.block_producer_resources_upper_bound ||
        cfg.block_producer_resources_upper_bound > 100 ) )
    report.add( "producer.resources", "fail", "Producer resource bounds must satisfy lower <= upper <= 100." );
  if( cfg.reset )
    report.add( "config.reset", "fail", "global.reset is enabled.", "Review and disable reset before a normal startup." );
  if( !cfg.verify_blocks )
    report.add( "chain.verification", "warning", "Block verification is disabled.", "Review the selected verification profile." );
  if( cfg.fork_algorithm != "pob" && cfg.fork_algorithm != "fifo" && cfg.fork_algorithm != "block-time" )
    report.add( "chain.fork_algorithm", "fail", "Unrecognized fork algorithm.", "Select a supported algorithm for the intended network." );

  try
  {
    const auto base = fs::absolute( cfg.basedir );
    auto existing = base;
    while( !fs::exists( existing ) && !fs::is_symlink( existing ) && existing != existing.root_path() )
      existing = existing.parent_path();
    if( !fs::is_directory( existing ) )
      report.add( "storage.directory", "fail", "Data path or nearest existing ancestor is not a directory.", "Choose a valid data directory." );
    else
    {
      const bool accessible = ::access( existing.c_str(), R_OK | W_OK | X_OK ) == 0;
      report.add( "storage.permissions", accessible ? "pass" : "fail",
                  accessible ? "OS permission check allows directory access; no write attempted." : "Directory access denied.",
                  accessible ? "" : "Check directory ownership, ACLs and mount permissions." );
      const auto space = fs::space( existing );
      report.add( "storage.available", space.available == 0 ? "fail" : "warning",
                  std::to_string( space.available ) + " bytes available on the data directory volume.",
                  "Budget chain growth, compaction, backups and restore separately; no universal capacity minimum is asserted." );
      for( const auto* child: { "db", "chain", "chain/blockchain" } )
      {
        const auto path = base / child;
        if( fs::exists( path ) && ( !fs::is_directory( path ) || ::access( path.c_str(), R_OK | W_OK | X_OK ) != 0 ) )
          report.add( std::string( "storage.access." ) + child, "fail", "Existing storage directory is inaccessible or not a directory.", "Check the existing storage path." );
      }
    }
  }
  catch( const fs::filesystem_error& )
  {
    report.add( "storage", "error", "Cannot inspect filesystem metadata.", "Check paths and permissions." );
  }

  const auto cpus = std::thread::hardware_concurrency();
  report.add( "resources.cpu", cpus ? "pass" : "not_checked", std::to_string( cpus ) + " logical CPUs reported; throughput is not measured." );
  uint64_t memory = 0;
#ifdef __APPLE__
  size_t size = sizeof( memory );
  if( sysctlbyname( "hw.memsize", &memory, &size, nullptr, 0 ) != 0 ) memory = 0;
#elif defined( __linux__ )
  const long pages = sysconf( _SC_PHYS_PAGES );
  const long page_size = sysconf( _SC_PAGESIZE );
  if( pages > 0 && page_size > 0 ) memory = uint64_t( pages ) * uint64_t( page_size );
#endif
  report.add( "resources.memory", memory ? "pass" : "not_checked",
              std::to_string( memory ) + " host physical bytes; configured RocksDB block cache: " +
              std::to_string( cfg.rocksdb_block_cache_mb ) + " MiB. Total runtime memory is not estimated." );
  if( memory && cfg.rocksdb_block_cache_mb >= memory / ( 1024 * 1024 ) )
    report.add( "resources.cache", "warning", "Configured block cache meets or exceeds host RAM.", "Reduce cache allocation and reserve memory for the remaining runtime." );
  report.add( "resources.effective_limits", "not_checked", "Available RAM, container/cgroup limits and CPU quotas are not measured.", "Check effective service/container limits before deployment." );
  struct rlimit limit {};
  if( getrlimit( RLIMIT_NOFILE, &limit ) == 0 )
    report.add( "resources.files", limit.rlim_cur == 0 ? "fail" : "pass",
                limit.rlim_cur == RLIM_INFINITY ? "Open-file limit: unlimited." : "Open-file soft limit: " + std::to_string( limit.rlim_cur ) + "; workload sufficiency is not verified." );
  else
    report.add( "resources.files", "error", "Cannot read open-file limit." );

  // Bind without listen, DNS, connects or SO_REUSEADDR. Hold all sockets until
  // the end so configured listeners are also checked against one another.
  boost::asio::io_context io;
  std::vector< std::unique_ptr< boost::asio::ip::tcp::socket > > sockets;
  auto port_check = [&]( const std::string& id, const std::string& input, bool p2p, bool admin ) {
    std::string host;
    std::string port_text;
    if( p2p && ( input.rfind( "/ip4/", 0 ) == 0 || input.rfind( "/ip6/", 0 ) == 0 ) )
    {
      const auto separator = input.find( "/tcp/", 5 );
      if( separator != std::string::npos )
      {
        host = input.substr( 5, separator - 5 );
        port_text = input.substr( separator + 5 );
      }
    }
    else if( !p2p )
    {
      const auto separator = input.rfind( ':' );
      if( separator != std::string::npos )
      {
        host = input.substr( 0, separator );
        port_text = input.substr( separator + 1 );
      }
    }
    if( host.empty() || port_text.empty() || port_text.find_first_not_of( "0123456789" ) != std::string::npos )
    {
      report.add( id, "not_checked", "Unsupported listener syntax for the local probe.", "Use a numeric IP TCP endpoint or verify the listener manually." );
      return;
    }
    if( host.front() == '[' && host.back() == ']' ) host = host.substr( 1, host.size() - 2 );
    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address( host, ec );
    if( ec )
    {
      report.add( id, "not_checked", "Listener is not a numeric IP; DNS is not queried.", "Verify address resolution separately." );
      return;
    }
    if( p2p && ( ( input.rfind( "/ip4/", 0 ) == 0 ) != address.is_v4() ) )
    {
      report.add( id, "fail", "P2P IP address does not match its multiaddress family." );
      return;
    }
    uint64_t port = 0;
    try { port = std::stoull( port_text ); }
    catch( ... ) { port = 65536; }
    if( port > 65535 )
    {
      report.add( id, "fail", "TCP port is outside 0..65535.", "Correct the configured port." );
      return;
    }
    if( !p2p && !address.is_loopback() )
      report.add( id + ".exposure", admin ? "fail" : "warning", "Listener is not loopback-bound.",
                  admin ? "Keep backup administration loopback-bound." : "Confirm intended RPC exposure and access controls." );
    auto socket = std::make_unique< boost::asio::ip::tcp::socket >( io );
    boost::asio::ip::tcp::endpoint endpoint( address, static_cast< uint16_t >( port ) );
    socket->open( endpoint.protocol(), ec );
    if( !ec ) socket->bind( endpoint, ec );
    report.add( id, ec ? "fail" : "pass", ec ? "Local TCP bind failed (busy, unavailable address or permission denied)." : "Local TCP bind succeeded; socket never listens and is released before exit.",
                ec ? "Check listener ownership and address permissions; do not stop another process automatically." : "" );
    if( port == 0 ) report.add( id + ".dynamic", "warning", "Port zero selects a dynamic port; the future startup port may differ." );
    sockets.push_back( std::move( socket ) );
  };
  if( cfg.is_enabled( "p2p" ) ) port_check( "ports.p2p", cfg.p2p_listen, true, false );
  if( cfg.is_enabled( "jsonrpc" ) ) port_check( "ports.jsonrpc", cfg.jsonrpc_listen, false, false );
  if( cfg.is_enabled( "grpc" ) ) port_check( "ports.grpc", cfg.grpc_listen, false, false );
  if( cfg.backup.enabled && cfg.backup.admin.enabled ) port_check( "ports.admin", cfg.backup.admin.listen, false, true );

  if( cfg.is_enabled( "block_producer" ) )
  {
    try
    {
      fs::path key = cfg.block_producer_private_key_file;
      if( key.empty() ) key = cfg.basedir / "block_producer/private.key";
      else if( key.is_relative() )
      {
        if( key.has_parent_path() ) key = cfg.basedir / key;
        else
        {
          auto primary = cfg.basedir / "block_producer" / key;
          key = fs::exists( primary ) ? primary : fs::exists( cfg.basedir / key ) ? cfg.basedir / key : primary;
        }
      }
      const bool readable = fs::is_regular_file( key ) && ::access( key.c_str(), R_OK ) == 0;
      report.add( "producer.key", readable ? "pass" : "fail", readable ? "Key file exists and is readable; contents were not read." : "Key file is missing or unreadable.",
                  readable ? "" : "Review the intended key before startup; normal producer startup may generate a missing key." );
      if( readable && ( fs::status( key ).permissions() & ( fs::perms::group_all | fs::perms::others_all ) ) != fs::perms::none )
        report.add( "producer.key_permissions", "warning", "Key file has group or other permission bits.", "Restrict key access to the intended service account." );
    }
    catch( const fs::filesystem_error& ) { report.add( "producer.key", "error", "Cannot inspect key file metadata." ); }
    report.add( "producer.activation", "not_checked", "Key correspondence, registration, VHP and observer health are unverified.", "Complete the producer activation gate before starting." );
  }
  report.add( "storage.instance", "not_checked", "Database locks and ownership are not tested; LOCK file existence is not proof of a running instance.", "Ensure no other process owns this basedir before startup." );
  report.add( "network.identity", "not_checked", "Genesis, database network identity, peers and connectivity are unverified.", "Validate the selected network and observer head progress." );
  report.add( "storage.integrity", "not_checked", "RocksDB was not opened; database health, recovery markers and restore capacity are unverified.", "Use the dedicated restore preflight for restore sizing; validate recovery as an observer." );
  if( cfg.backup.enabled ) report.add( "backup", "not_checked", "Backup configuration and destinations were not validated.", "Run --backup-dry-run separately." );
  return report;
}

} // namespace koinos::node
