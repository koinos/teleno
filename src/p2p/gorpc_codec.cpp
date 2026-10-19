#include "p2p/gorpc_codec.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace koinos::node::p2p::gorpc {

namespace {

constexpr uint8_t mp_nil     = 0xc0;
constexpr uint8_t mp_false   = 0xc2;
constexpr uint8_t mp_true    = 0xc3;
constexpr uint8_t mp_bin8    = 0xc4;
constexpr uint8_t mp_bin16   = 0xc5;
constexpr uint8_t mp_bin32   = 0xc6;
constexpr uint8_t mp_float32 = 0xca;
constexpr uint8_t mp_float64 = 0xcb;
constexpr uint8_t mp_uint8   = 0xcc;
constexpr uint8_t mp_uint16  = 0xcd;
constexpr uint8_t mp_uint32  = 0xce;
constexpr uint8_t mp_uint64  = 0xcf;
constexpr uint8_t mp_int8    = 0xd0;
constexpr uint8_t mp_int16   = 0xd1;
constexpr uint8_t mp_int32   = 0xd2;
constexpr uint8_t mp_int64   = 0xd3;
constexpr uint8_t mp_str8    = 0xd9;
constexpr uint8_t mp_str16   = 0xda;
constexpr uint8_t mp_str32   = 0xdb;
constexpr uint8_t mp_array16 = 0xdc;
constexpr uint8_t mp_array32 = 0xdd;
constexpr uint8_t mp_map16   = 0xde;
constexpr uint8_t mp_map32   = 0xdf;

void append_u16( std::string& out, uint16_t value )
{
  out.push_back( static_cast< char >( ( value >> 8 ) & 0xff ) );
  out.push_back( static_cast< char >( value & 0xff ) );
}

void append_u32( std::string& out, uint32_t value )
{
  out.push_back( static_cast< char >( ( value >> 24 ) & 0xff ) );
  out.push_back( static_cast< char >( ( value >> 16 ) & 0xff ) );
  out.push_back( static_cast< char >( ( value >> 8 ) & 0xff ) );
  out.push_back( static_cast< char >( value & 0xff ) );
}

void append_u64( std::string& out, uint64_t value )
{
  append_u32( out, static_cast< uint32_t >( value >> 32 ) );
  append_u32( out, static_cast< uint32_t >( value & 0xffffffff ) );
}

void append_map_header( std::string& out, size_t size )
{
  if( size < 16 )
  {
    out.push_back( static_cast< char >( 0x80 | size ) );
  }
  else if( size <= std::numeric_limits< uint16_t >::max() )
  {
    out.push_back( static_cast< char >( mp_map16 ) );
    append_u16( out, static_cast< uint16_t >( size ) );
  }
  else
  {
    out.push_back( static_cast< char >( mp_map32 ) );
    append_u32( out, static_cast< uint32_t >( size ) );
  }
}

void append_array_header( std::string& out, size_t size )
{
  if( size < 16 )
  {
    out.push_back( static_cast< char >( 0x90 | size ) );
  }
  else if( size <= std::numeric_limits< uint16_t >::max() )
  {
    out.push_back( static_cast< char >( mp_array16 ) );
    append_u16( out, static_cast< uint16_t >( size ) );
  }
  else
  {
    out.push_back( static_cast< char >( mp_array32 ) );
    append_u32( out, static_cast< uint32_t >( size ) );
  }
}

void append_raw( std::string& out, std::string_view value )
{
  if( value.size() < 32 )
  {
    out.push_back( static_cast< char >( 0xa0 | value.size() ) );
  }
  else if( value.size() <= std::numeric_limits< uint16_t >::max() )
  {
    out.push_back( static_cast< char >( mp_str16 ) );
    append_u16( out, static_cast< uint16_t >( value.size() ) );
  }
  else
  {
    out.push_back( static_cast< char >( mp_str32 ) );
    append_u32( out, static_cast< uint32_t >( value.size() ) );
  }

  out.append( value.data(), value.size() );
}

void append_uint( std::string& out, uint64_t value )
{
  if( value <= 0x7f )
  {
    out.push_back( static_cast< char >( value ) );
  }
  else if( value <= std::numeric_limits< uint8_t >::max() )
  {
    out.push_back( static_cast< char >( mp_uint8 ) );
    out.push_back( static_cast< char >( value ) );
  }
  else if( value <= std::numeric_limits< uint16_t >::max() )
  {
    out.push_back( static_cast< char >( mp_uint16 ) );
    append_u16( out, static_cast< uint16_t >( value ) );
  }
  else if( value <= std::numeric_limits< uint32_t >::max() )
  {
    out.push_back( static_cast< char >( mp_uint32 ) );
    append_u32( out, static_cast< uint32_t >( value ) );
  }
  else
  {
    out.push_back( static_cast< char >( mp_uint64 ) );
    append_u64( out, value );
  }
}

void append_service_id( std::string& out, std::string_view service, std::string_view method )
{
  append_map_header( out, 2 );
  append_raw( out, "Name" );
  append_raw( out, service );
  append_raw( out, "Method" );
  append_raw( out, method );
}

class Reader
{
public:
  explicit Reader( std::string_view raw ) : _raw( raw ) {}

  [[nodiscard]] size_t position() const noexcept { return _pos; }

  [[nodiscard]] bool done() const noexcept { return _pos == _raw.size(); }

  std::string_view slice( size_t begin, size_t end ) const
  {
    return _raw.substr( begin, end - begin );
  }

  uint8_t read_byte()
  {
    if( _pos >= _raw.size() )
      throw DecodeError( "truncated msgpack object", true );
    return static_cast< uint8_t >( _raw[ _pos++ ] );
  }

  uint16_t read_u16()
  {
    ensure( 2 );
    uint16_t value = ( static_cast< uint16_t >( byte_at( _pos ) ) << 8 )
                     | static_cast< uint16_t >( byte_at( _pos + 1 ) );
    _pos += 2;
    return value;
  }

  uint32_t read_u32()
  {
    ensure( 4 );
    uint32_t value = ( static_cast< uint32_t >( byte_at( _pos ) ) << 24 )
                     | ( static_cast< uint32_t >( byte_at( _pos + 1 ) ) << 16 )
                     | ( static_cast< uint32_t >( byte_at( _pos + 2 ) ) << 8 )
                     | static_cast< uint32_t >( byte_at( _pos + 3 ) );
    _pos += 4;
    return value;
  }

  uint64_t read_u64()
  {
    uint64_t hi = read_u32();
    uint64_t lo = read_u32();
    return ( hi << 32 ) | lo;
  }

  uint64_t read_uint()
  {
    uint8_t b = read_byte();
    if( b <= 0x7f )
      return b;
    if( b >= 0xe0 )
      throw DecodeError( "negative msgpack integer where unsigned integer was expected", false );

    switch( b )
    {
      case mp_uint8:
        return read_byte();
      case mp_uint16:
        return read_u16();
      case mp_uint32:
        return read_u32();
      case mp_uint64:
        return read_u64();
      case mp_int8:
      {
        int8_t value = static_cast< int8_t >( read_byte() );
        if( value < 0 )
          throw DecodeError( "negative msgpack integer where unsigned integer was expected", false );
        return static_cast< uint64_t >( value );
      }
      case mp_int16:
      {
        int16_t value = static_cast< int16_t >( read_u16() );
        if( value < 0 )
          throw DecodeError( "negative msgpack integer where unsigned integer was expected", false );
        return static_cast< uint64_t >( value );
      }
      case mp_int32:
      {
        int32_t value = static_cast< int32_t >( read_u32() );
        if( value < 0 )
          throw DecodeError( "negative msgpack integer where unsigned integer was expected", false );
        return static_cast< uint64_t >( value );
      }
      case mp_int64:
      {
        uint64_t raw = read_u64();
        if( raw & ( uint64_t{ 1 } << 63 ) )
          throw DecodeError( "negative msgpack integer where unsigned integer was expected", false );
        return raw;
      }
      default:
        throw DecodeError( "msgpack value is not an unsigned integer", false );
    }
  }

  size_t read_raw_size( uint8_t b )
  {
    if( ( b & 0xe0 ) == 0xa0 )
      return b & 0x1f;

    switch( b )
    {
      case mp_str8:
      case mp_bin8:
        return read_byte();
      case mp_str16:
      case mp_bin16:
        return read_u16();
      case mp_str32:
      case mp_bin32:
        return read_u32();
      default:
        throw DecodeError( "msgpack value is not raw/string data", false );
    }
  }

  std::string read_raw()
  {
    uint8_t b = read_byte();
    size_t size = read_raw_size( b );
    ensure( size );
    auto value = std::string( _raw.substr( _pos, size ) );
    _pos += size;
    return value;
  }

  size_t read_map_size()
  {
    uint8_t b = read_byte();
    if( ( b & 0xf0 ) == 0x80 )
      return b & 0x0f;

    switch( b )
    {
      case mp_map16:
        return read_u16();
      case mp_map32:
        return read_u32();
      default:
        throw DecodeError( "msgpack value is not a map", false );
    }
  }

  size_t read_array_size()
  {
    uint8_t b = read_byte();
    if( ( b & 0xf0 ) == 0x90 )
      return b & 0x0f;

    switch( b )
    {
      case mp_array16:
        return read_u16();
      case mp_array32:
        return read_u32();
      default:
        throw DecodeError( "msgpack value is not an array", false );
    }
  }

  void read_nil()
  {
    uint8_t b = read_byte();
    if( b != mp_nil )
      throw DecodeError( "msgpack value is not nil", false );
  }

  void skip()
  {
    ObjectSequenceScanner scanner;
    scanner.consume( _raw.substr( _pos ) );
    if( !scanner.complete() )
      throw DecodeError( "truncated msgpack object", true );
    _pos += scanner.consumed();
  }

private:
  uint8_t byte_at( size_t index ) const
  {
    return static_cast< uint8_t >( _raw[ index ] );
  }

  void ensure( size_t count )
  {
    if( count > _raw.size() - _pos )
      throw DecodeError( "truncated msgpack object", true );
  }

  std::string_view _raw;
  size_t _pos = 0;
};

ServiceID read_service_id( Reader& reader )
{
  ServiceID service;
  size_t map_size = reader.read_map_size();

  for( size_t i = 0; i < map_size; ++i )
  {
    auto key = reader.read_raw();
    if( key == "Name" )
      service.name = reader.read_raw();
    else if( key == "Method" )
      service.method = reader.read_raw();
    else
      reader.skip();
  }

  if( service.name.empty() || service.method.empty() )
    throw DecodeError( "gorpc ServiceID is missing Name or Method", false );

  return service;
}

ResponseHeader read_response_header( Reader& reader )
{
  ResponseHeader header;
  size_t map_size = reader.read_map_size();

  for( size_t i = 0; i < map_size; ++i )
  {
    auto key = reader.read_raw();
    if( key == "Service" )
    {
      header.service = read_service_id( reader );
    }
    else if( key == "Error" )
    {
      header.error = reader.read_raw();
    }
    else if( key == "ErrType" )
    {
      header.err_type = static_cast< ErrorType >( reader.read_uint() );
    }
    else
    {
      reader.skip();
    }
  }

  return header;
}

std::string read_id_field_response( Reader& reader )
{
  std::string id;
  size_t map_size = reader.read_map_size();

  for( size_t i = 0; i < map_size; ++i )
  {
    auto key = reader.read_raw();
    if( key == "ID" )
      id = reader.read_raw();
    else
      reader.skip();
  }

  return id;
}

} // anonymous namespace

DecodeError::DecodeError( std::string message, bool truncated )
  : std::runtime_error( std::move( message ) ), _truncated( truncated )
{}

std::string encode_empty_request()
{
  std::string out;
  append_map_header( out, 0 );
  return out;
}

std::string encode_get_ancestor_block_id_request( std::string_view parent_id, uint64_t child_height )
{
  std::string out;
  append_map_header( out, 2 );
  append_raw( out, "ParentID" );
  append_raw( out, parent_id );
  append_raw( out, "ChildHeight" );
  append_uint( out, child_height );
  return out;
}

std::string encode_get_blocks_request( std::string_view head_block_id,
                                        uint64_t start_block_height,
                                        uint32_t num_blocks )
{
  std::string out;
  append_map_header( out, 3 );
  append_raw( out, "HeadBlockID" );
  append_raw( out, head_block_id );
  append_raw( out, "StartBlockHeight" );
  append_uint( out, start_block_height );
  append_raw( out, "NumBlocks" );
  append_uint( out, num_blocks );
  return out;
}

std::string encode_id_response( std::string_view id )
{
  std::string out;
  append_map_header( out, 1 );
  append_raw( out, "ID" );
  append_raw( out, id );
  return out;
}

std::string encode_head_block_response( std::string_view id, uint64_t height )
{
  std::string out;
  append_map_header( out, 2 );
  append_raw( out, "ID" );
  append_raw( out, id );
  append_raw( out, "Height" );
  append_uint( out, height );
  return out;
}

std::string encode_blocks_response( const std::vector< std::string >& block_payloads )
{
  std::string out;
  append_map_header( out, 1 );
  append_raw( out, "Blocks" );
  append_array_header( out, block_payloads.size() );
  for( const auto& block_payload: block_payloads )
    append_raw( out, block_payload );
  return out;
}

std::string encode_request( std::string_view service,
                             std::string_view method,
                             std::string_view msgpack_args )
{
  std::string out;
  append_service_id( out, service, method );
  out.append( msgpack_args.data(), msgpack_args.size() );
  return out;
}

std::string encode_success_response( const ServiceID& service, std::string_view msgpack_payload )
{
  std::string out;
  append_map_header( out, 1 );
  append_raw( out, "Service" );
  append_service_id( out, service.name, service.method );
  out.append( msgpack_payload.data(), msgpack_payload.size() );
  return out;
}

std::string encode_error_response( const ServiceID& service,
                                   std::string_view error,
                                   ErrorType err_type )
{
  std::string out;
  append_map_header( out, 3 );
  append_raw( out, "Service" );
  append_service_id( out, service.name, service.method );
  append_raw( out, "Error" );
  append_raw( out, error );
  append_raw( out, "ErrType" );
  append_uint( out, static_cast< uint64_t >( err_type ) );
  out.push_back( static_cast< char >( mp_nil ) );
  return out;
}

void ObjectSequenceScanner::add_children( uint64_t count )
{
  if( count > std::numeric_limits< uint64_t >::max() - _pending )
    throw DecodeError( "msgpack container count overflow", false );
  _pending += count;
}

void ObjectSequenceScanner::consume( std::string_view bytes )
{
  size_t position = 0;
  while( position < bytes.size() && !complete() )
  {
    if( _payload )
    {
      const auto count = static_cast< size_t >( std::min< uint64_t >( _payload, bytes.size() - position ) );
      _payload -= count;
      position += count;
      _consumed += count;
      continue;
    }
    const auto value = static_cast< uint8_t >( bytes[ position++ ] );
    ++_consumed;
    if( _length_left )
    {
      _length = ( _length << 8 ) | value;
      if( --_length_left == 0 )
      {
        if( _length_kind == LengthKind::raw ) _payload = _length;
        else add_children( uint64_t( _length ) * ( _length_kind == LengthKind::map ? 2 : 1 ) );
        _length = 0;
      }
      continue;
    }
    --_pending;
    if( value <= 0x7f || value >= 0xe0 || value == mp_nil || value == mp_false || value == mp_true )
      continue;
    if( ( value & 0xe0 ) == 0xa0 ) { _payload = value & 0x1f;continue; }
    if( ( value & 0xf0 ) == 0x90 ) { add_children( value & 0x0f );continue; }
    if( ( value & 0xf0 ) == 0x80 ) { add_children( 2 * ( value & 0x0f ) );continue; }
    switch( value )
    {
      case mp_uint8: case mp_int8: _payload = 1;break;
      case mp_uint16: case mp_int16: _payload = 2;break;
      case mp_uint32: case mp_int32: case mp_float32: _payload = 4;break;
      case mp_uint64: case mp_int64: case mp_float64: _payload = 8;break;
      case mp_str8: case mp_bin8:
        _length_kind = LengthKind::raw;_length_left = 1;break;
      case mp_str16: case mp_bin16:
        _length_kind = LengthKind::raw;_length_left = 2;break;
      case mp_str32: case mp_bin32:
        _length_kind = LengthKind::raw;_length_left = 4;break;
      case mp_array16: _length_kind = LengthKind::array;_length_left = 2;break;
      case mp_array32: _length_kind = LengthKind::array;_length_left = 4;break;
      case mp_map16: _length_kind = LengthKind::map;_length_left = 2;break;
      case mp_map32: _length_kind = LengthKind::map;_length_left = 4;break;
      default: throw DecodeError( "unsupported msgpack descriptor", false );
    }
  }
}

DecodedRequest decode_request( std::string_view raw )
{
  Reader reader( raw );
  DecodedRequest request;
  request.service = read_service_id( reader );

  size_t args_begin = reader.position();
  reader.skip();
  request.args = std::string( reader.slice( args_begin, reader.position() ) );

  return request;
}

Response decode_response( std::string_view raw )
{
  Reader reader( raw );
  Response response;
  response.header = read_response_header( reader );

  size_t payload_begin = reader.position();
  reader.skip();
  response.payload = std::string( reader.slice( payload_begin, reader.position() ) );

  return response;
}

Response decode_response_owned( std::string raw )
{
  Reader reader( raw );
  Response response;
  response.header = read_response_header( reader );
  const auto begin = reader.position();
  reader.skip();
  const auto size = reader.position() - begin;
  // A large ignored header/suffix must not pin its allocation in a tiny
  // returned payload. Keep reuse for payload-dominated frames; otherwise
  // preserve only the actual payload, as the original decoder did.
  if( size < raw.capacity() - size )
  {
    response.payload.assign( raw.data() + begin, size );
    return response;
  }
  raw.erase( 0, begin );
  raw.resize( size );
  response.payload = std::move( raw );
  return response;
}

AncestorBlockIDRequest decode_get_ancestor_block_id_request( std::string_view payload )
{
  Reader reader( payload );
  AncestorBlockIDRequest request;
  size_t map_size = reader.read_map_size();

  for( size_t i = 0; i < map_size; ++i )
  {
    auto key = reader.read_raw();
    if( key == "ParentID" )
      request.parent_id = reader.read_raw();
    else if( key == "ChildHeight" )
      request.child_height = reader.read_uint();
    else
      reader.skip();
  }

  if( !reader.done() )
    throw DecodeError( "trailing bytes after GetAncestorBlockID request", false );

  return request;
}

BlocksRequest decode_get_blocks_request( std::string_view payload )
{
  Reader reader( payload );
  BlocksRequest request;
  size_t map_size = reader.read_map_size();

  for( size_t i = 0; i < map_size; ++i )
  {
    auto key = reader.read_raw();
    if( key == "HeadBlockID" )
    {
      request.head_block_id = reader.read_raw();
    }
    else if( key == "StartBlockHeight" )
    {
      request.start_block_height = reader.read_uint();
    }
    else if( key == "NumBlocks" )
    {
      auto num_blocks = reader.read_uint();
      if( num_blocks > std::numeric_limits< uint32_t >::max() )
        throw DecodeError( "GetBlocks NumBlocks exceeds uint32", false );
      request.num_blocks = static_cast< uint32_t >( num_blocks );
    }
    else
    {
      reader.skip();
    }
  }

  if( !reader.done() )
    throw DecodeError( "trailing bytes after GetBlocks request", false );

  return request;
}

std::string decode_id_response( std::string_view payload )
{
  Reader reader( payload );
  auto id = read_id_field_response( reader );
  if( !reader.done() )
    throw DecodeError( "trailing bytes after ID response", false );
  return id;
}

HeadBlockResponse decode_head_block_response( std::string_view payload )
{
  Reader reader( payload );
  HeadBlockResponse response;
  size_t map_size = reader.read_map_size();

  for( size_t i = 0; i < map_size; ++i )
  {
    auto key = reader.read_raw();
    if( key == "ID" )
      response.id = reader.read_raw();
    else if( key == "Height" )
      response.height = reader.read_uint();
    else
      reader.skip();
  }

  if( !reader.done() )
    throw DecodeError( "trailing bytes after head block response", false );

  return response;
}

std::vector< std::string > decode_blocks_response( std::string_view payload )
{
  Reader reader( payload );
  std::vector< std::string > blocks;
  size_t map_size = reader.read_map_size();

  for( size_t i = 0; i < map_size; ++i )
  {
    auto key = reader.read_raw();
    if( key == "Blocks" )
    {
      size_t block_count = reader.read_array_size();
      blocks.reserve( block_count );
      for( size_t j = 0; j < block_count; ++j )
        blocks.push_back( reader.read_raw() );
    }
    else
    {
      reader.skip();
    }
  }

  if( !reader.done() )
    throw DecodeError( "trailing bytes after blocks response", false );

  return blocks;
}

} // namespace koinos::node::p2p::gorpc
