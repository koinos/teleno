#include "core/restore_startup.hpp"

#include <fstream>
#include <stdexcept>

#if !defined( _WIN32 )
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace koinos::node {
namespace {

void sync_path( const std::filesystem::path& path )
{
#if !defined( _WIN32 )
  const int fd = ::open( path.c_str(), O_RDONLY );
  if( fd < 0 )
    throw std::runtime_error( "failed to open restore recovery path for fsync: "
                              + path.string() + ": " + std::strerror( errno ) );
  if( ::fsync( fd ) != 0 )
  {
    const auto error = std::string( std::strerror( errno ) );
    ::close( fd );
    throw std::runtime_error( "failed to fsync restore recovery path: "
                              + path.string() + ": " + error );
  }
  if( ::close( fd ) != 0 )
    throw std::runtime_error( "failed to close restore recovery path: "
                              + path.string() + ": " + std::strerror( errno ) );
#else
  (void)path;
#endif
}

} // namespace

RestoreStartupPolicyResult apply_restore_startup_policy(
  NodeConfig& config,
  const std::filesystem::path& basedir,
  bool explicit_producer_activation )
{
  RestoreStartupPolicyResult result;
  result.verify_blocks = config.verify_blocks;

  const auto restore_marker = basedir / ".backup-just-restored";
  const auto recovery_hold  = basedir / ".backup-observer-recovery";
  result.restore_marker_found          = std::filesystem::exists( restore_marker );
  result.producer_recovery_hold_active = std::filesystem::exists( recovery_hold );

  if( result.restore_marker_found )
  {
    // Establish the durable hold before consuming the one-shot restore marker.
    // An explicit enable on this same startup cannot bypass the required
    // observer validation run.
    std::ofstream hold_stream( recovery_hold, std::ios::out | std::ios::trunc );
    if( !hold_stream )
      throw std::runtime_error( "failed to create restore producer recovery hold: "
                                + recovery_hold.string() );
    hold_stream << "observer validation required before block production\n";
    hold_stream.close();
    if( !hold_stream )
      throw std::runtime_error( "failed to persist restore producer recovery hold: "
                                + recovery_hold.string() );

    sync_path( recovery_hold );
    sync_path( basedir );
    std::filesystem::remove( restore_marker );
    sync_path( basedir );
    result.producer_recovery_hold_active = true;
  }
  else if( result.producer_recovery_hold_active && explicit_producer_activation )
  {
    std::filesystem::remove( recovery_hold );
    sync_path( basedir );
    result.producer_recovery_hold_active   = false;
    result.producer_recovery_hold_released = true;
  }

  if( !result.producer_recovery_hold_active )
    return result;

  result.block_producer_was_enabled = config.is_enabled( "block_producer" );
  config.features[ "block_producer" ] = false;
  return result;
}

} // namespace koinos::node
