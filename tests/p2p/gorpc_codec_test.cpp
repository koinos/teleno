#include "p2p/gorpc_codec.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string from_hex( const std::string& hex )
{
  assert( hex.size() % 2 == 0 );

  std::string out;
  out.reserve( hex.size() / 2 );

  for( size_t i = 0; i < hex.size(); i += 2 )
  {
    auto byte = static_cast< char >( std::stoul( hex.substr( i, 2 ), nullptr, 16 ) );
    out.push_back( byte );
  }

  return out;
}

std::string to_hex( const std::string& raw )
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;
  out.reserve( raw.size() * 2 );

  for( unsigned char byte: raw )
  {
    out.push_back( digits[ byte >> 4 ] );
    out.push_back( digits[ byte & 0x0f ] );
  }

  return out;
}

void expect_hex( const std::string& actual, const std::string& expected_hex )
{
  if( to_hex( actual ) != expected_hex )
  {
    std::cerr << "expected: " << expected_hex << "\nactual:   " << to_hex( actual ) << "\n";
    std::abort();
  }
}

} // anonymous namespace

int main()
{
  using namespace koinos::node::p2p::gorpc;

  const std::string id = from_hex( "122001020304" );

  expect_hex(
    encode_request( "PeerRPCService", "GetChainID", encode_empty_request() ),
    "82a44e616d65ae5065657252504353657276696365a64d6574686f64aa476574436861696e494480" );

  expect_hex(
    encode_get_ancestor_block_id_request( id, 42 ),
    "82a8506172656e744944a6122001020304ab4368696c644865696768742a" );

  expect_hex(
    encode_get_blocks_request( id, 7, 3 ),
    "83ab48656164426c6f636b4944a6122001020304b05374617274426c6f636b48656967687407a94e756d426c6f636b7303" );

  const ServiceID service{ "PeerRPCService", "GetChainID" };
  expect_hex(
    encode_success_response( service, from_hex( "81a24944a6122001020304" ) ),
    "81a75365727669636582a44e616d65ae5065657252504353657276696365a64d6574686f64aa476574436861696e494481a24944a6122001020304" );

  auto response = decode_response( from_hex(
    "81a75365727669636582a44e616d65ae5065657252504353657276696365a64d6574686f64aa476574436861696e4944"
    "81a24944a6122001020304" ) );
  assert( response.header.service.name == "PeerRPCService" );
  assert( response.header.service.method == "GetChainID" );
  assert( response.header.error.empty() );
  assert( decode_id_response( response.payload ) == id );

  auto error_response = decode_response( from_hex(
    "83a75365727669636582a44e616d65ae5065657252504353657276696365a64d6574686f64aa476574436861696e4944"
    "a54572726f72a4626f6f6da74572725479706501c0" ) );
  assert( error_response.header.error == "boom" );
  assert( error_response.header.err_type == ErrorType::server );

  auto head = decode_head_block_response( from_hex( "82a24944a6122001020304a648656967687463" ) );
  assert( head.id == id );
  assert( head.height == 99 );

  auto blocks = decode_blocks_response( from_hex( "81a6426c6f636b7392a2aabba1cc" ) );
  assert( blocks.size() == 2 );
  assert( blocks[ 0 ] == from_hex( "aabb" ) );
  assert( blocks[ 1 ] == from_hex( "cc" ) );

  bool saw_truncation = false;
  try
  {
    decode_response( from_hex( "81a7536572" ) );
  }
  catch( const DecodeError& e )
  {
    saw_truncation = e.truncated();
  }
  assert( saw_truncation );

  return 0;
}
