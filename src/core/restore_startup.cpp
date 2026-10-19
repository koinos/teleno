#include "core/restore_startup.hpp"

#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

#if !defined( _WIN32 )
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
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

void validate_direct_startup_markers(const std::filesystem::path& basedir)
{
  const auto receipt = basedir / ".teleno-direct-restore.json";
  if(std::filesystem::symlink_status(receipt).type() == std::filesystem::file_type::not_found) return;
  auto read = [](const std::filesystem::path& path) -> nlohmann::json {
#if !defined(_WIN32)
    struct stat st{};
    if(::lstat(path.c_str(),&st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1 || st.st_size > 65536)
      throw std::runtime_error("unsafe direct restore generation marker");
    const int fd = ::open(path.c_str(),O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if(fd < 0) throw std::runtime_error("cannot open direct restore generation marker");
    std::string bytes; char buffer[4096]; bool failed = false;
    for(;;)
    {
      const auto n = ::read(fd,buffer,sizeof(buffer));
      if(n < 0 && errno == EINTR) continue;
      if(n < 0 || (n > 0 && bytes.size() + size_t(n) > 65536)) { failed = true; break; }
      if(!n) break; bytes.append(buffer,size_t(n));
    }
    if(::close(fd) != 0 || failed) throw std::runtime_error("cannot read direct restore generation marker");
    return nlohmann::json::parse(bytes,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&) {
      if(depth > 8) throw std::runtime_error("nested direct restore generation marker"); return true;
    });
#else
    (void)path; throw std::runtime_error("direct restore markers require POSIX");
#endif
  };
  const auto identity = read(receipt);
  const auto generation = identity.at("attempt_id").get<std::string>();
  if(identity.at("format") != "teleno-direct-restore-observer-hold" || identity.at("version") != 1
    || identity.at("target") != std::filesystem::absolute(basedir).lexically_normal().string()
    || generation.size() != 32 || generation.find_first_not_of("0123456789abcdef") != std::string::npos)
    throw std::runtime_error("direct restore generation marker mismatch");
  for(const auto* field: {"manifest_sha256","inventory_sha256"})
  {
    const auto hash = identity.at(field).get<std::string>();
    if(hash.size() != 64 || hash.find_first_not_of("0123456789abcdef") != std::string::npos)
      throw std::runtime_error("invalid direct restore generation digest");
  }
  const auto marker = basedir / ".backup-just-restored", hold = basedir / ".backup-observer-recovery";
  const bool first = std::filesystem::symlink_status(marker).type() != std::filesystem::file_type::not_found;
  const bool held = std::filesystem::symlink_status(hold).type() != std::filesystem::file_type::not_found;
  if((first && (!held || read(marker) != identity)) || (held && read(hold) != identity))
    throw std::runtime_error("direct restore observer hold generation mismatch");
}

RestoreStartupPolicyResult apply_restore_startup_policy(
  NodeConfig& config,
  const std::filesystem::path& basedir,
  bool explicit_producer_activation )
{
  validate_direct_startup_markers(basedir);
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
    if(!std::filesystem::exists(basedir / ".teleno-direct-restore.json"))
    {
      std::ofstream hold_stream( recovery_hold, std::ios::out | std::ios::trunc );
      if( !hold_stream )
        throw std::runtime_error( "failed to create restore producer recovery hold: "
                                  + recovery_hold.string() );
      hold_stream << "observer validation required before block production\n";
      hold_stream.close();
      if( !hold_stream )
        throw std::runtime_error( "failed to persist restore producer recovery hold: "
                                  + recovery_hold.string() );
    }

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
