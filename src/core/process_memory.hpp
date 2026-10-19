#pragma once

#include <cstdint>
#include <istream>
#include <limits>

namespace koinos::node {

inline uint64_t linux_statm_rss_bytes( std::istream& statm, long page_size )
{
  // statm reports virtual size first and resident pages second.
  uint64_t virtual_pages = 0, resident_pages = 0;
  if( page_size <= 0 || !( statm >> virtual_pages >> resident_pages ) )
    return 0;
  const auto bytes_per_page = static_cast< uint64_t >( page_size );
  if( resident_pages > std::numeric_limits< uint64_t >::max() / bytes_per_page )
    return 0;
  return resident_pages * bytes_per_page;
}

} // namespace koinos::node
