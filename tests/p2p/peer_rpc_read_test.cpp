#include "p2p/peer_rpc_read.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#ifndef _WIN32
#include <pthread.h>
#endif

namespace {

void require( bool ok, const char* message )
{
  if( !ok ) throw std::runtime_error( message );
}

struct FragmentReader : libp2p::basic::Reader
{
  explicit FragmentReader( boost::asio::io_context& context ) : io( context ) {}
  enum class Fault { none, zero, overcount, disconnected, throwing, allocation };
  boost::asio::io_context& io;
  std::string frame;
  size_t position = 0;
  size_t chunk = 1;
  size_t calls = 0;
  size_t depth = 0, maximum_depth = 0;
  bool deferred = false, hold = false;
  Fault fault = Fault::none;
  libp2p::BytesOut borrowed;
  ReadCallbackFunc pending;
  void read( libp2p::BytesOut out, size_t size, ReadCallbackFunc callback ) override
  { readSome( out, size, std::move( callback ) ); }
  void readSome( libp2p::BytesOut out, size_t size, ReadCallbackFunc callback ) override
  {
    require( !pending, "only one SDK read may be outstanding" );
    require( out.size() == size && size > 0, "read must use a nonempty remaining span" );
    ++calls;
    if( fault == Fault::throwing ) throw std::runtime_error( "read initiation failed" );
    if( fault == Fault::allocation ) throw std::bad_alloc();
    borrowed = out;pending = std::move( callback );
    if( hold ) return;
    if( deferred ) boost::asio::post( io, [this] { deliver(); } );
    else deliver();
  }
  void deliver()
  {
    require( bool( pending ), "fixture needs a pending read" );
    auto callback = std::move( pending );
    pending = {};
    ++depth;maximum_depth = std::max( maximum_depth, depth );
    if( fault == Fault::zero ) callback( size_t( 0 ) );
    else if( fault == Fault::overcount ) callback( borrowed.size() + 1 );
    else if( fault == Fault::disconnected ) callback( std::make_error_code( std::errc::connection_reset ) );
    else
    {
      const auto count = std::min< size_t >( { borrowed.size(), frame.size() - position, chunk } );
      // Borrowed storage is written only at callback completion, including
      // deliberately late completion after logical cancellation.
      std::copy_n( frame.begin() + position, count, borrowed.begin() );
      position += count;callback( count );
    }
    --depth;
  }
  void deferReadCallback( outcome::result< size_t > result, ReadCallbackFunc callback ) override
  { callback( std::move( result ) ); }
};

void fragmented_request()
{
  boost::asio::io_context io;
  auto reader = std::make_shared< FragmentReader >( io );
  reader->frame = koinos::node::p2p::gorpc::encode_request(
    "PeerRPCService", "GetChainID", koinos::node::p2p::gorpc::encode_empty_request() );
  std::exception_ptr error;
  size_t completions = 0;
  koinos::node::p2p::gorpc::DecodedRequest received;
  koinos::node::p2p::detail::read_rpc_request( io, reader,
    [&]( std::exception_ptr failure, auto request ) {
      ++completions;error = failure;received = std::move( request );
    } );
  io.run();
  if( error )
    try { std::rethrow_exception( error ); }
    catch( const std::exception& e ) { std::cerr << "fragment failure: " << e.what() << '\n'; }
  require( completions == 1 && !error, "fragmented valid request was rejected before handler dispatch" );
  require( received.service.method == "GetChainID", "wrong decoded method" );
  require( reader->maximum_depth == 1, "short inline reads must not recurse" );
}

namespace gorpc = koinos::node::p2p::gorpc;
namespace detail = koinos::node::p2p::detail;

std::string request_with_size( size_t size )
{
  const auto header = gorpc::encode_request( "PeerRPCService", "GetChainID", {} );
  require( size >= header.size() + 5, "invalid boundary fixture size" );
  const auto length = size - header.size() - 5;
  std::string args( 1, char( 0xc6 ) );
  for( int shift: { 24, 16, 8, 0 } ) args.push_back( char( ( length >> shift ) & 255 ) );
  args.append( length, 'x' );return header + args;
}

void successful_reads()
{
  for( bool deferred: { false, true } )
  {
    for( size_t size: { size_t( 120 ), detail::RpcRequestRead::max_frame_bytes } )
    {
      boost::asio::io_context io;
      auto reader = std::make_shared< FragmentReader >( io );
      reader->frame = request_with_size( size );reader->chunk = 7;reader->deferred = deferred;
      const auto expected = gorpc::decode_request( reader->frame );
      std::exception_ptr error;gorpc::DecodedRequest decoded;size_t completions = 0;
      auto operation = detail::read_rpc_request( io, reader, [&]( auto failure, auto request ) {
        error = failure;decoded = std::move( request );++completions;
      } );
      require( operation->buffer_capacity() == 65536, "one bounded input buffer must own all fragments" );
      io.run();
      require( !error && completions == 1, "valid bounded request must complete once" );
      require( decoded.args == expected.args && decoded.service.method == expected.service.method,
               "fragmentation must preserve exact decoded arguments and service" );
      require( reader->maximum_depth == 1, "deferred/inline read callbacks must be sequential" );
    }
  }
  // Existing decode_request ignores data after its first two objects.
  boost::asio::io_context io;auto reader = std::make_shared< FragmentReader >( io );
  reader->frame = request_with_size( 120 );const auto expected = gorpc::decode_request( reader->frame );
  reader->frame += "trailing bytes";reader->chunk = 65536;
  size_t completions = 0;bool passed = false;
  detail::read_rpc_request( io, reader, [&]( auto error, auto request ) {
    ++completions;passed = !error && request.args == expected.args;
  } );
  io.run();require( completions == 1 && passed, "preserve existing trailing-data behavior" );
}

void failed_reads()
{
  for( auto fault: { FragmentReader::Fault::zero, FragmentReader::Fault::overcount,
                    FragmentReader::Fault::disconnected, FragmentReader::Fault::throwing,
                    FragmentReader::Fault::allocation } )
  {
    boost::asio::io_context io;auto reader = std::make_shared< FragmentReader >( io );
    reader->frame = request_with_size( 120 );reader->fault = fault;
    size_t completions = 0;std::exception_ptr failure;
    detail::read_rpc_request( io, reader, [&]( auto error, auto ) { ++completions;failure = error; } );
    io.run();require( completions == 1 && failure && reader->calls == 1, "failed read cannot retry or dispatch" );
    if( fault == FragmentReader::Fault::allocation )
    {
      bool local = false;try { std::rethrow_exception( failure ); } catch( const std::bad_alloc& ) { local = true; }
      require( local, "local allocation failure must retain its type" );
    }
  }
  for( auto frame: { request_with_size( 65537 ), std::string( 1, char( 0xc1 ) ),
                    request_with_size( 120 ).substr( 0, 80 ) } )
  {
    boost::asio::io_context io;auto reader = std::make_shared< FragmentReader >( io );
    reader->frame = frame;reader->chunk = 3000;
    size_t completions = 0;std::exception_ptr failure;
    detail::read_rpc_request( io, reader, [&]( auto error, auto ) { ++completions;failure = error; } );
    io.run();require( completions == 1 && failure, "oversize/malformed/truncated requests cannot dispatch" );
    if( frame.size() == 65537 )
    {
      bool capacity = false;
      try { std::rethrow_exception( failure ); }
      catch( const detail::RpcRequestReadError& error ) { capacity = error.reason == detail::RpcRequestReadError::Reason::capacity; }
      require( capacity && reader->position == 65536, "request capacity must be enforced before reading past the window" );
    }
  }
  boost::asio::io_context io;size_t completions = 0;bool failed = false;
  detail::read_rpc_request( io, {}, [&]( auto error, auto ) { ++completions;failed = bool( error ); } );
  io.run();require( completions == 1 && failed, "missing reader must fail safely" );
}

void cancelled_and_late_reads()
{
  using Reason = detail::RpcRequestReadError::Reason;
  for( bool timeout: { false, true } )
  {
    boost::asio::io_context io;auto reader = std::make_shared< FragmentReader >( io );
    reader->frame = request_with_size( 120 );reader->chunk = 120;reader->hold = true;
    size_t completions = 0, aborts = 0;std::exception_ptr failure;
    auto operation = detail::read_rpc_request( io, reader,
      [&]( auto error, auto ) { ++completions;failure = error; }, [&] { ++aborts; },
      timeout ? std::chrono::milliseconds( 2 ) : std::chrono::hours( 1 ) );
    std::weak_ptr< detail::RpcRequestRead > weak = operation;
    if( !timeout ) operation->cancel();
    operation.reset();io.run();
    require( completions == 1 && failure && aborts == 1, "cancel/timeout must terminate once and abort SDK I/O" );
    bool reason_matches = false;
    try { std::rethrow_exception( failure ); }
    catch( const detail::RpcRequestReadError& error )
    { reason_matches = error.reason == ( timeout ? Reason::timeout : Reason::cancelled ); }
    require( reason_matches && !weak.expired(), "pending SDK callback must retain cancelled buffer ownership" );
    reader->deliver();
    require( weak.expired() && completions == 1, "late SDK completion cannot dispatch twice or retain finished ownership" );
  }
  boost::asio::io_context io;auto reader = std::make_shared< FragmentReader >( io );
  reader->frame = request_with_size( 120 );reader->hold = true;reader->chunk = 120;
  auto active = std::make_shared< std::atomic< bool > >( true );
  size_t callbacks = 0, dispatches = 0;
  auto operation = detail::read_rpc_request( io, reader, [&]( auto failure, auto ) {
    ++callbacks;if( !failure && *active ) ++dispatches;
  }, {}, std::chrono::hours( 1 ), active );
  active->store( false );reader->deliver();operation.reset();io.run();
  require( callbacks == 1 && dispatches == 0, "stopped generation must not dispatch an old request" );
}

void throwing_completion()
{
  boost::asio::io_context io;auto reader = std::make_shared< FragmentReader >( io );
  reader->frame = request_with_size( 120 );reader->chunk = 65536;size_t completions = 0;
  std::weak_ptr< FragmentReader > weak = reader;
  detail::read_rpc_request( io, reader, [&]( auto, auto ) { ++completions;throw std::runtime_error( "callback failed" ); } );
  reader.reset();io.run();require( completions == 1 && weak.expired(), "completion exception must not leak or retry" );
}

void deeply_fragmented_request()
{
  boost::asio::io_context io;auto reader = std::make_shared< FragmentReader >( io );
  const std::string args = std::string( 16384, char( 0x92 ) ) + std::string( 16385, char( 0xc0 ) );
  reader->frame = gorpc::encode_request( "PeerRPCService", "GetChainID", args );
  reader->deferred = true;reader->chunk = 1;
  size_t completions = 0;bool passed = false;
  detail::read_rpc_request( io, reader, [&]( auto error, auto request ) {
    ++completions;passed = !error && request.args == args;
  }, {}, std::chrono::steady_clock::duration::zero() );
  io.run();require( completions == 1 && passed && reader->maximum_depth == 1,
                    "deep one-byte fragments must preserve bytes without recursive growth" );
}

void scanner_cases()
{
  for( const std::string hex: { "c0", "c2", "c3", "01", "ff", "a0", "a3616263", "d90161", "da000161",
    "db0000000161", "c40161", "c5000161", "c60000000161", "ccff", "cd00ff", "ce000000ff",
    "cf00000000000000ff", "d0ff", "d1ffff", "d2ffffffff", "d3ffffffffffffffff", "ca00000000",
    "cb0000000000000000", "90", "92c090", "dc0001c0", "dd00000001c0", "80", "81c0c0",
    "de0001c0c0", "df00000001c0c0" } )
  {
    std::string raw;for( size_t i = 0; i < hex.size(); i += 2 ) raw.push_back( char( std::stoul( hex.substr( i, 2 ), nullptr, 16 ) ) );
    for( size_t split = 0; split <= raw.size(); ++split )
    {
      gorpc::ObjectSequenceScanner scanner;
      scanner.consume( std::string_view( raw ).substr( 0, split ) );
      require( scanner.complete() == ( split == raw.size() ), "scanner completed an incomplete object" );
      scanner.consume( std::string_view( raw ).substr( split ) );scanner.consume( "ignored" );
      require( scanner.complete() && scanner.consumed() == raw.size(), "scanner boundary/length bytes changed" );
    }
    gorpc::ObjectSequenceScanner pair( 2 );pair.consume( raw );require( !pair.complete(), "RPC needs two objects" );
    pair.consume( std::string( 1, char( 0xc0 ) ) );require( pair.complete(), "RPC sequence did not complete" );
  }
  gorpc::ObjectSequenceScanner huge;huge.consume( std::string( "\xc6\xff\xff\xff\xff", 5 ) );
  require( !huge.complete() && huge.consumed() == 5, "declared huge payload must not allocate or appear complete" );
  bool invalid = false;try { gorpc::ObjectSequenceScanner bad;bad.consume( std::string( 1, char( 0xc1 ) ) ); }
  catch( const gorpc::DecodeError& error ) { invalid = !error.truncated(); }
  require( invalid, "unsupported descriptor must remain an invalid decoding error" );
}

void deep_skip()
{
  struct Probe { std::string frame; bool passed = false; } probe;
  std::string args( 16384, static_cast< char >( 0x92 ) );
  args.append( 16385, static_cast< char >( 0xc0 ) );
  probe.frame = koinos::node::p2p::gorpc::encode_request( "PeerRPCService", "GetChainID", args );
  auto work = []( void* pointer ) -> void* {
    auto& value = *static_cast< Probe* >( pointer );
    try
    {
      const auto request = koinos::node::p2p::gorpc::decode_request( value.frame );
      value.passed = request.service.method == "GetChainID" && request.args.size() == 32769;
    }
    catch( ... ) {}
    return nullptr;
  };
#ifndef _WIN32
  pthread_attr_t attr;
  require( pthread_attr_init( &attr ) == 0, "initialize parser thread attributes" );
  require( pthread_attr_setstacksize( &attr, 128 * 1024 ) == 0, "select bounded parser stack" );
  pthread_t thread;
  const auto created = pthread_create( &thread, &attr, work, &probe );
  pthread_attr_destroy( &attr );
  require( created == 0, "create parser thread" );
  require( pthread_join( thread, nullptr ) == 0, "join parser thread" );
#else
  std::thread thread( [&] { work( &probe ); } );thread.join();
#endif
  require( probe.passed, "deep supported payload must decode without recursive stack growth" );
}

} // namespace

int main( int argc, char** argv )
{
  try
  {
    if( argc == 2 && std::string( argv[ 1 ] ) == "deep" ) deep_skip();
    else
    {
      fragmented_request();successful_reads();failed_reads();cancelled_and_late_reads();
      throwing_completion();scanner_cases();deeply_fragmented_request();deep_skip();
    }
    std::cout << "Peer RPC read regression passed\n";return 0;
  }
  catch( const std::exception& e ) { std::cerr << e.what() << '\n';return 1; }
}
