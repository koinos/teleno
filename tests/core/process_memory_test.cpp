#include "core/process_memory.hpp"

#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#if defined( __linux__ )
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
void check( bool value, const char* message )
{
  if( !value ) throw std::runtime_error( message );
}

uint64_t parse( const std::string& text, long page_size )
{
  std::istringstream input( text );
  return koinos::node::linux_statm_rss_bytes( input, page_size );
}

#if defined( __linux__ )
uint64_t resident_bytes()
{
  std::ifstream statm( "/proc/self/statm" );
  return koinos::node::linux_statm_rss_bytes( statm, sysconf( _SC_PAGESIZE ) );
}

uint64_t status_resident_bytes()
{
  std::ifstream status( "/proc/self/status" );
  std::string line;
  while( std::getline( status, line ) )
    if( line.rfind( "VmRSS:", 0 ) == 0 )
    {
      std::istringstream field( line.substr( 6 ) );
      uint64_t kib = 0;
      std::string unit;
      check( bool( field >> kib >> unit ) && unit == "kB", "invalid kernel VmRSS" );
      return kib * 1024;
    }
  throw std::runtime_error( "kernel VmRSS unavailable" );
}

void check_untouched_virtual_mapping()
{
  constexpr size_t bytes = size_t( 1 ) << 30;
  const auto before = resident_bytes();
  struct Mapping
  {
    void* address = ::mmap( nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0 );
    ~Mapping() { if( address != MAP_FAILED ) ::munmap( address, bytes ); }
  } mapping;
  check( mapping.address != MAP_FAILED, "cannot reserve untouched virtual mapping" );
  const auto after = resident_bytes(), status = status_resident_bytes();
  check( before > 0 && after > 0, "resident measurement unavailable" );
  // Leave room for instrumentation shadow memory, while excluding the 1-GiB
  // untouched virtual range from the resident measurement.
  check( after <= before + ( uint64_t( 256 ) << 20 ), "virtual reservation counted as resident memory" );
  const auto difference = after > status ? after - status : status - after;
  check( difference <= ( uint64_t( 8 ) << 20 ), "resident measurement differs from kernel VmRSS" );
}
#endif
} // namespace

int main()
{
  try
  {
    check( parse( "1048576 256 12 3 0 4 0\n", 4096 ) == 1024 * 1024, "resident field/4-KiB pages" );
    check( parse( "1048576 256 12 3 0 4 0\n", 16384 ) == 4 * 1024 * 1024, "resident field/16-KiB pages" );
    check( parse( "", 4096 ) == 0 && parse( "1024", 4096 ) == 0, "incomplete statm accepted" );
    check( parse( "invalid 12", 4096 ) == 0 && parse( "1024 invalid", 4096 ) == 0, "invalid statm accepted" );
    check( parse( "1024 12", 0 ) == 0 && parse( "1024 12", -1 ) == 0, "invalid page size accepted" );
    const auto maximum = std::numeric_limits< uint64_t >::max();
    const auto prefix = std::to_string( maximum ) + " ";
    check( parse( prefix + std::to_string( maximum / 4096 ), 4096 ) == maximum / 4096 * 4096, "valid boundary refused" );
    check( parse( prefix + std::to_string( maximum / 4096 + 1 ), 4096 ) == 0, "RSS multiplication overflow" );
    check( parse( "18446744073709551616 12", 4096 ) == 0, "page-count overflow accepted" );
#if defined( __linux__ )
    check_untouched_virtual_mapping();
#endif
    std::cout << "Process resident memory checks passed\n";
    return 0;
  }
  catch( const std::exception& error )
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
