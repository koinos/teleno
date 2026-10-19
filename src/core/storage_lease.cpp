#include "core/storage_lease.hpp"

#include <stdexcept>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace koinos::node {
StorageLease::StorageLease( const std::filesystem::path& basedir )
{
#if defined(__unix__) || defined(__APPLE__)
  const auto target = std::filesystem::weakly_canonical( std::filesystem::absolute( basedir ) );
  if( target == target.root_path() || target.filename().empty() )
    throw std::runtime_error( "storage gate requires a non-root basedir" );
  const auto path = target.parent_path() / ( "." + target.filename().string() + ".teleno-storage.lock" );
  _intent = path.string() + ".activation.json";
  _fd = ::open( path.c_str(), O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600 );
  struct stat st{};
  if( _fd < 0 || ::fstat( _fd, &st ) != 0 || !S_ISREG( st.st_mode ) || st.st_nlink != 1
      || st.st_uid != ::geteuid() || ( st.st_mode & 077 ) != 0
      || ::flock( _fd, LOCK_EX | LOCK_NB ) != 0 )
  {
    if( _fd >= 0 ) ::close( _fd );
    _fd = -1;
    throw std::runtime_error( "storage gate busy or unsafe; stop the process using this basedir before restore/startup" );
  }
#else
  (void)basedir;
  throw std::runtime_error( "direct restore storage gate requires POSIX" );
#endif
}
StorageLease::~StorageLease()
{
#if defined(__unix__) || defined(__APPLE__)
  if( _fd >= 0 ) ::close( _fd );
#endif
}
} // namespace koinos::node
