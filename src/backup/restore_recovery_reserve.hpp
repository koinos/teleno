#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace koinos::node::backup {

// Application headroom policy, independent of consensus. The smaller profile
// is an explicit experiment; neither value is a bound on compaction workspace.
enum class RestoreReserveProfile { standard, experimental_8gib };

inline const char* restore_reserve_profile_name( RestoreReserveProfile profile )
{
  switch( profile )
  {
    case RestoreReserveProfile::standard: return "standard";
    case RestoreReserveProfile::experimental_8gib: return "experimental-8gib";
  }
  throw std::invalid_argument( "unsupported restore reserve profile" );
}

inline RestoreReserveProfile parse_restore_reserve_profile( const std::string& name )
{
  if( name == "standard" ) return RestoreReserveProfile::standard;
  if( name == "experimental-8gib" ) return RestoreReserveProfile::experimental_8gib;
  throw std::invalid_argument( "unsupported restore reserve profile" );
}

inline uint64_t restore_recovery_reserve_bytes( uint64_t database_bytes,
  RestoreReserveProfile profile = RestoreReserveProfile::standard )
{
  // Validate the enum even for callers outside the command-line parser.
  (void)restore_reserve_profile_name( profile );
  const uint64_t minimum = ( profile == RestoreReserveProfile::standard ? 10ULL : 8ULL ) * 1024 * 1024 * 1024;
  return std::max( minimum, database_bytes / 5 );
}

} // namespace koinos::node::backup
