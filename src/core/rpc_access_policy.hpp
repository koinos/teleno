#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace koinos::node {

struct RpcAccessPolicy
{
  std::vector< std::string > blacklist;
  std::vector< std::string > whitelist;

  bool allows( const std::string& service, const std::string& method ) const
  {
    if( !whitelist.empty() )
      return matches( whitelist, service, method );
    return !matches( blacklist, service, method );
  }

private:
  static std::string trim( std::string value )
  {
    auto is_space = []( unsigned char c ) { return std::isspace( c ) != 0; };
    value.erase( value.begin(), std::find_if( value.begin(), value.end(), [&]( unsigned char c ) { return !is_space( c ); } ) );
    value.erase( std::find_if( value.rbegin(), value.rend(), [&]( unsigned char c ) { return !is_space( c ); } ).base(), value.end() );
    return value;
  }

  static std::string canonicalize( std::string target )
  {
    target = trim( std::move( target ) );
    static const std::string prefix = "koinos.rpc.";
    if( target.rfind( prefix, 0 ) == 0 )
      target.erase( 0, prefix.size() );
    return target;
  }

  static bool matches( const std::vector< std::string >& entries,
                       const std::string& service,
                       const std::string& method )
  {
    const auto full_target = service + "." + method;
    for( const auto& raw_entry: entries )
    {
      const auto entry = canonicalize( raw_entry );
      if( entry.empty() )
        continue;
      if( entry == service || entry == full_target )
        return true;
    }
    return false;
  }
};

} // namespace koinos::node
