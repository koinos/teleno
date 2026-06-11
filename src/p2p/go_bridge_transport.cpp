#include "go_bridge_transport.hpp"

#include <csignal>
#include <cstdlib>
#include <filesystem>

#include <koinos/log.hpp>

#include <sys/wait.h>
#include <unistd.h>

namespace koinos::node::p2p {

GoBridgeTransport::GoBridgeTransport( const Config& config )
    : _config( config )
{
  _io = std::make_shared< boost::asio::io_context >();
}

GoBridgeTransport::~GoBridgeTransport()
{
  stop();
}

void GoBridgeTransport::start()
{
  if( _config.go_p2p_binary.empty() || !std::filesystem::exists( _config.go_p2p_binary ) )
  {
    LOG( warning ) << "[p2p/go-bridge] Go P2P binary not found: " << _config.go_p2p_binary;
    LOG( warning ) << "[p2p/go-bridge] P2P networking disabled. Set the path in config or build with cpp-libp2p.";
    return;
  }

  _running = true;
  spawn_go_process();

  _monitor_thread = std::thread( [this]() { monitor_loop(); } );

  LOG( info ) << "[p2p/go-bridge] Started Go P2P sidecar (pid " << _go_pid << ")";
}

void GoBridgeTransport::stop()
{
  if( !_running.exchange( false ) )
    return;

  if( _go_pid > 0 )
  {
    kill( _go_pid, SIGTERM );

    // Wait up to 5s for graceful shutdown
    for( int i = 0; i < 50; ++i )
    {
      int status;
      pid_t result = waitpid( _go_pid, &status, WNOHANG );
      if( result != 0 )
        break;
      usleep( 100000 ); // 100ms
    }

    // Force kill if still running
    int status;
    if( waitpid( _go_pid, &status, WNOHANG ) == 0 )
    {
      kill( _go_pid, SIGKILL );
      waitpid( _go_pid, &status, 0 );
    }

    LOG( info ) << "[p2p/go-bridge] Stopped Go P2P sidecar (pid " << _go_pid << ")";
    _go_pid = 0;
  }

  if( _monitor_thread.joinable() )
    _monitor_thread.join();
}

void GoBridgeTransport::spawn_go_process()
{
  pid_t pid = fork();
  if( pid == 0 )
  {
    // Child process — exec the Go P2P binary
    std::string basedir_arg = "--basedir=" + _config.basedir;
    std::string listen_arg  = "--listen=" + _config.listen_address;

    std::vector< const char* > argv;
    argv.push_back( _config.go_p2p_binary.c_str() );
    argv.push_back( basedir_arg.c_str() );
    argv.push_back( listen_arg.c_str() );

    for( const auto& seed: _config.seed_peers )
    {
      // Go P2P uses --peer flag
      static std::vector< std::string > peer_args;
      peer_args.push_back( "--peer=" + seed );
      argv.push_back( peer_args.back().c_str() );
    }

    argv.push_back( nullptr );

    execv( _config.go_p2p_binary.c_str(), const_cast< char* const* >( argv.data() ) );
    _exit( 1 ); // exec failed
  }
  else if( pid > 0 )
  {
    _go_pid = pid;
  }
  else
  {
    LOG( error ) << "[p2p/go-bridge] fork() failed: " << strerror( errno );
  }
}

void GoBridgeTransport::monitor_loop()
{
  while( _running )
  {
    if( _go_pid > 0 )
    {
      int status;
      pid_t result = waitpid( _go_pid, &status, WNOHANG );

      if( result > 0 )
      {
        // Process exited
        if( WIFEXITED( status ) )
          LOG( warning ) << "[p2p/go-bridge] Go P2P exited with code " << WEXITSTATUS( status );
        else if( WIFSIGNALED( status ) )
          LOG( warning ) << "[p2p/go-bridge] Go P2P killed by signal " << WTERMSIG( status );

        // Restart after 5s
        if( _running )
        {
          LOG( info ) << "[p2p/go-bridge] Restarting Go P2P in 5s...";
          for( int i = 0; i < 50 && _running; ++i )
            usleep( 100000 );

          if( _running )
          {
            spawn_go_process();
            LOG( info ) << "[p2p/go-bridge] Restarted Go P2P (pid " << _go_pid << ")";
          }
        }
      }
    }

    // Check every 1s
    for( int i = 0; i < 10 && _running; ++i )
      usleep( 100000 );
  }
}

// ---------------------------------------------------------------------------
// ITransport methods — delegate to Go process via bridge
// Currently these are stubs; full implementation would use a local
// JSON-RPC or protobuf IPC channel to the Go process.
// ---------------------------------------------------------------------------

void GoBridgeTransport::connect_peer( const PeerID& ) {}
void GoBridgeTransport::disconnect_peer( const PeerID& ) {}
uint32_t GoBridgeTransport::connected_peer_count() const { return _peer_count; }
std::vector< PeerID > GoBridgeTransport::connected_peers() const { return {}; }
std::vector< PeerID > GoBridgeTransport::known_peers() const { return {}; }

std::string GoBridgeTransport::peer_get_chain_id( const PeerID& ) { return ""; }
PeerHeadInfo GoBridgeTransport::peer_get_head_block( const PeerID& ) { return {}; }
std::string GoBridgeTransport::peer_get_ancestor_block_id( const PeerID&, const std::string&, uint64_t ) { return ""; }
std::vector< protocol::block > GoBridgeTransport::peer_get_blocks( const PeerID&, const std::string&, uint64_t, uint32_t ) { return {}; }

void GoBridgeTransport::publish_block( const protocol::block& ) {}
void GoBridgeTransport::publish_transaction( const protocol::transaction& ) {}

void GoBridgeTransport::on_peer_connected( PeerConnectedCallback cb ) { _on_connected = std::move( cb ); }
void GoBridgeTransport::on_peer_disconnected( PeerDisconnectedCallback cb ) { _on_disconnected = std::move( cb ); }
void GoBridgeTransport::on_block_received( BlockReceivedCallback cb ) { _on_block = std::move( cb ); }
void GoBridgeTransport::on_transaction_received( TxReceivedCallback cb ) { _on_tx = std::move( cb ); }
void GoBridgeTransport::on_peer_rpc_request( PeerRpcRequestCallback cb ) { _on_peer_rpc_request = std::move( cb ); }

std::string GoBridgeTransport::bridge_call( const std::string&, const std::string& ) { return ""; }

} // namespace koinos::node::p2p
