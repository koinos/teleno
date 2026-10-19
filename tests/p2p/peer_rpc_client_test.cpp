#include "p2p/peer_rpc_client.hpp"

#include <cstring>
#include <condition_variable>
#include <future>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {
using namespace std::chrono_literals;
namespace detail = koinos::node::p2p::detail;
namespace gorpc = koinos::node::p2p::gorpc;

void require( bool ok, const char* why )
{ if( !ok ) throw std::runtime_error( why ); }

struct Channel : libp2p::basic::Reader, libp2p::basic::Writer
{
  enum class Fault { none, zero, overcount, disconnected, throwing, allocation };
  explicit Channel( boost::asio::io_context& io ) : io( io ) {}
  boost::asio::io_context& io;
  std::string input = gorpc::encode_success_response(
    { "PeerRPCService", "GetChainID" }, gorpc::encode_id_response( "controlled-chain" ) );
  std::string written;
  size_t position = 0, read_calls = 0, write_calls = 0, closes = 0, aborts = 0;
  size_t read_chunk = 1, write_chunk = 3, depth = 0, max_depth = 0;
  bool hold_read = false, hold_write = false, deferred = false;
  Fault read_fault = Fault::none, write_fault = Fault::none;
  libp2p::BytesOut borrowed_read;
  libp2p::BytesIn borrowed_write;
  ReadCallbackFunc reading;
  WriteCallbackFunc writing;

  void read( libp2p::BytesOut bytes, size_t count, ReadCallbackFunc done ) override
  { readSome( bytes, count, std::move( done ) ); }
  void readSome( libp2p::BytesOut bytes, size_t count, ReadCallbackFunc done ) override
  {
    require( !reading && count > 0 && bytes.size() == count, "one valid read window at a time" );
    ++read_calls;
    if( read_fault == Fault::throwing ) throw std::runtime_error( "controlled read initiation fault" );
    if( read_fault == Fault::allocation ) throw std::bad_alloc();
    borrowed_read = bytes;reading = std::move( done );
    if( hold_read ) return;
    if( deferred ) boost::asio::post( io, [this] { deliver_read(); } );
    else deliver_read();
  }
  void deliver_read()
  {
    auto done = std::move( reading );reading = {};
    require( bool( done ), "read needs a pending callback" );
    ++depth;max_depth = std::max( max_depth, depth );
    if( read_fault == Fault::zero ) done( size_t( 0 ) );
    else if( read_fault == Fault::overcount ) done( borrowed_read.size() + 1 );
    else if( read_fault == Fault::disconnected ) done( std::make_error_code( std::errc::connection_reset ) );
    else
    {
      const auto count = std::min( { borrowed_read.size(), input.size() - position, read_chunk } );
      std::memcpy( borrowed_read.data(), input.data() + position, count );
      position += count;done( count );
    }
    --depth;
  }
  void writeSome( libp2p::BytesIn bytes, size_t count, WriteCallbackFunc done ) override
  {
    require( !writing && count > 0 && count == bytes.size(), "one valid request suffix at a time" );
    ++write_calls;
    if( write_fault == Fault::throwing ) throw std::runtime_error( "controlled write initiation fault" );
    if( write_fault == Fault::allocation ) throw std::bad_alloc();
    borrowed_write = bytes;writing = std::move( done );
    if( hold_write ) return;
    if( deferred ) boost::asio::post( io, [this] { deliver_write(); } );
    else deliver_write();
  }
  void deliver_write()
  {
    auto done = std::move( writing );writing = {};
    require( bool( done ), "write needs a pending callback" );
    ++depth;max_depth = std::max( max_depth, depth );
    if( write_fault == Fault::zero ) done( size_t( 0 ) );
    else if( write_fault == Fault::overcount ) done( borrowed_write.size() + 1 );
    else if( write_fault == Fault::disconnected ) done( std::make_error_code( std::errc::connection_reset ) );
    else
    {
      const auto count = std::min( borrowed_write.size(), write_chunk );
      written.append( reinterpret_cast< const char* >( borrowed_write.data() ), count );done( count );
    }
    --depth;
  }
  void deferReadCallback( outcome::result< size_t > result, ReadCallbackFunc done ) override
  { done( std::move( result ) ); }
  void deferWriteCallback( std::error_code error, WriteCallbackFunc done ) override
  { if( error ) done( error ); }
};

struct Harness
{
  boost::asio::io_context io;
  std::shared_ptr< Channel > channel = std::make_shared< Channel >( io );
  std::shared_ptr< std::atomic< bool > > active = std::make_shared< std::atomic< bool > >( true );
  std::string request = gorpc::encode_request( "PeerRPCService", "GetChainID", gorpc::encode_empty_request() );
  detail::RpcClientOpenResult opening;
  std::shared_ptr< detail::RpcClientCall > call;
  std::exception_ptr error;
  gorpc::Response response;
  size_t completions = 0, opens = 0;
  bool hold_open = false, throw_open = false;
  std::weak_ptr< const libp2p::Bytes > request_owner;

  detail::RpcClientChannel opened()
  { return { channel, channel, [c = channel] { ++c->closes; }, [c = channel] { ++c->aborts; } }; }
  void prepare( std::chrono::steady_clock::duration timeout = 1s,
                size_t cap = detail::RpcClientCall::maximum_response_bytes )
  {
    auto bytes = std::make_shared< const libp2p::Bytes >( request.begin(), request.end() );request_owner = bytes;
    call = std::make_shared< detail::RpcClientCall >( io,
      [this]( detail::RpcClientOpenResult done ) {
        ++opens;
        if( throw_open ) throw std::bad_alloc();
        if( hold_open ) opening = std::move( done );
        else done( {}, opened() );
      }, bytes,
      [this]( std::exception_ptr failure, gorpc::Response result ) {
        ++completions;error = failure;response = std::move( result );
      }, active, timeout, cap );
  }
  void start( std::chrono::steady_clock::duration timeout = 1s,
              size_t cap = detail::RpcClientCall::maximum_response_bytes )
  { prepare( timeout, cap );call->begin(); }
  ~Harness()
  {
    if( call ) call->cancel();
    opening = {};channel->reading = {};channel->writing = {};
    io.restart();io.poll();
  }
  void reason( detail::RpcClientError::Reason expected )
  {
    require( completions == 1 && bool( error ), "one failed completion" );
    try { std::rethrow_exception( error ); }
    catch( const detail::RpcClientError& e ) { require( e.reason == expected, "wrong client error" );return; }
    throw std::runtime_error( "wrong exception type" );
  }
  void local()
  {
    require( completions == 1 && bool( error ), "one local completion" );
    try { std::rethrow_exception( error ); }
    catch( const koinos::node::p2p::LocalTransportUnavailable& ) { return; }
    throw std::runtime_error( "local cancellation must remain local" );
  }
};

void frames_and_counts()
{
  for( bool deferred: { false, true } )
    for( size_t chunk: { size_t( 1 ), size_t( 17 ), size_t( 64 * 1024 ) } )
    {
      Harness h;h.channel->deferred = deferred;h.channel->read_chunk = chunk;
      h.start();h.io.run();
      require( h.completions == 1 && !h.error && h.channel->written == h.request, "complete unchanged request once" );
      require( h.channel->closes == 1 && h.channel->aborts == 0, "successful channel closes once" );
      require( gorpc::decode_id_response( h.response.payload ) == "controlled-chain", "exact decoded response" );
      require( h.channel->max_depth <= 2, "fragment continuations must not recurse" );
      require( h.request_owner.expired(), "completed request releases its owner" );
    }
  for( bool exact: { false, true } )
  {
    Harness h;h.channel->read_chunk = 65536;
    const auto size = h.channel->input.size();h.start( 1s, size - ( exact ? 0 : 1 ) );h.io.run();
    if( exact ) require( h.completions == 1 && !h.error, "exact frame capacity is accepted" );
    else
    {
      h.local();
      try { std::rethrow_exception( h.error ); }
      catch( const koinos::node::p2p::LocalTransportUnavailable& e )
      { require( e.reason == koinos::node::p2p::LocalTransportUnavailable::Reason::response_capacity,
                 "receive capacity must remain a distinct local reason" ); }
      require( h.channel->aborts == 1, "oversize aborts before overflow" );
    }
  }
  Harness trailing;trailing.channel->input += "ignored trailing bytes";trailing.channel->read_chunk = 65536;
  trailing.start();trailing.io.run();
  require( trailing.completions == 1 && !trailing.error
    && gorpc::decode_id_response( trailing.response.payload ) == "controlled-chain", "preserve trailing-byte compatibility" );
}

void failures()
{
  for( auto fault: { Channel::Fault::zero, Channel::Fault::overcount, Channel::Fault::disconnected,
                     Channel::Fault::throwing, Channel::Fault::allocation } )
    for( bool reading: { false, true } )
    {
      Harness h;
      if( reading ) h.channel->read_fault = fault;
      else h.channel->write_fault = fault;
      h.start();h.io.run();
      require( h.completions == 1 && bool( h.error ) && h.channel->aborts == 1 && h.channel->closes == 0,
               "I/O failure aborts and completes once" );
      if( fault == Channel::Fault::allocation )
      {
        bool allocation = false;
        try { std::rethrow_exception( h.error ); } catch( const std::bad_alloc& ) { allocation = true; }
        require( allocation, "local allocation failure must not become a remote protocol fault" );
      }
    }
  Harness h;h.throw_open = true;h.start();h.io.run();
  require( h.completions == 1 && bool( h.error ) && h.channel->read_calls == 0, "open initiation failure must finish" );
  Harness malformed;malformed.channel->input = std::string( "\x90\xc0", 2 );malformed.start();malformed.io.run();
  require( malformed.completions == 1 && bool( malformed.error ) && malformed.channel->aborts == 1, "typed decode failure aborts" );
}

void deadlines_and_late_callbacks()
{
  {
    Harness h;h.hold_open = true;h.start( 5ms );h.io.run();
    h.reason( detail::RpcClientError::Reason::open_timeout );h.call.reset();
    auto late = std::move( h.opening );late( {}, h.opened() );
    require( h.channel->aborts == 1 && h.channel->write_calls == 0 && h.completions == 1,
             "late successful open must reset without writing or completing again" );
  }
  {
    Harness h;h.channel->hold_write = true;h.start( 5ms );h.io.run();
    h.reason( detail::RpcClientError::Reason::write_timeout );
    require( !h.request_owner.expired(), "late SDK write still owns its borrowed request" );
    h.call.reset();h.channel->deliver_write();h.io.restart();h.io.run();
    require( h.request_owner.expired() && h.channel->write_calls == 1 && h.completions == 1,
             "late write completion releases bytes and cannot continue" );
  }
  {
    Harness h;h.channel->hold_read = true;h.channel->write_chunk = 65536;h.start( 5ms );
    // Keep the first physical borrow outstanding until timeout. The actual
    // slow-fragment transport fixture independently proves the total deadline.
    h.io.run();
    h.reason( detail::RpcClientError::Reason::response_timeout );
    std::weak_ptr< detail::RpcClientCall > retained = h.call;h.call.reset();
    require( !retained.expired(), "late SDK read must retain its borrowed buffer" );
    h.channel->deliver_read();h.io.restart();h.io.run();
    require( retained.expired() && h.completions == 1, "late read must release without parsing or re-completion" );
  }
}

void local_cancellation_and_registry()
{
  {
    Harness h;h.start(); // First short write completed inline; its continuation is queued.
    h.call->cancel();h.io.run();h.local();
    require( h.channel->write_calls == 1 && h.channel->read_calls == 0 && h.channel->aborts == 1,
             "cancelled queued write continuation cannot emit another request suffix" );
  }
  for( bool request_only: { false, true } )
  {
    Harness h;h.channel->hold_read = true;h.channel->write_chunk = 65536;h.start();
    if( request_only ) h.call->request_cancel();
    else { h.active->store( false );h.call->cancel(); }
    h.channel->deliver_read();h.io.run();h.local();
    require( h.channel->read_calls == 1 && h.channel->aborts == 1, "cancelled read cannot enqueue more I/O" );
  }
  Harness h;h.hold_open = true;h.prepare();
  auto registry = std::make_shared< detail::RpcClientRegistry >( h.io, h.active );
  registry->submit( h.call );h.io.poll();
  registry->stop_accepting();h.active->store( false );
  registry->cancel_all();h.local();
  bool refused = false;
  try { registry->submit( h.call ); }
  catch( const koinos::node::p2p::LocalTransportUnavailable& ) { refused = true; }
  require( refused, "closed registry refuses new posts without waiting for I/O restart" );
  auto late = std::move( h.opening );h.call.reset();late( {}, h.opened() );h.io.restart();h.io.run();
  require( h.channel->write_calls == 0 && h.completions == 1 && h.channel->aborts == 1,
           "old generation cannot start work on late open" );

  Harness queued;queued.prepare();
  auto paused = std::make_shared< detail::RpcClientRegistry >( queued.io, queued.active );
  paused->submit( queued.call );paused->stop_accepting();queued.active->store( false );
  boost::asio::post( queued.io, [paused] { paused->cancel_all(); } );queued.io.run();queued.local();
  require( queued.opens == 0 && queued.channel->write_calls == 0, "shutdown cancels admitted but unstarted work" );
}

void concurrent_registry_shutdown()
{
  boost::asio::io_context io;auto guard = boost::asio::make_work_guard( io );
  auto active = std::make_shared< std::atomic< bool > >( true );
  auto registry = std::make_shared< detail::RpcClientRegistry >( io, active );
  std::atomic< size_t > admitted{ 0 }, refused{ 0 }, completed{ 0 }, wrong_error{ 0 };
  auto bytes = std::make_shared< const libp2p::Bytes >( 1, 'x' );
  std::vector< std::shared_ptr< detail::RpcClientCall > > calls;
  for( size_t i = 0;i < 64;++i )
    calls.push_back( std::make_shared< detail::RpcClientCall >( io,
      []( detail::RpcClientOpenResult ) {}, bytes,
      [&]( std::exception_ptr error, auto ) {
        ++completed;
        try { if( error ) std::rethrow_exception( error );++wrong_error; }
        catch( const koinos::node::p2p::LocalTransportUnavailable& ) {}
        catch( ... ) { ++wrong_error; }
      }, active ) );
  std::thread runner( [&] { io.run(); } );
  std::mutex mutex;std::condition_variable changed;bool go = false;
  std::vector< std::thread > submitters;
  for( size_t thread = 0;thread < 4;++thread )
    submitters.emplace_back( [&, thread] {
      { std::unique_lock lock( mutex );changed.wait( lock, [&] { return go; } ); }
      for( size_t i = thread;i < calls.size();i += 4 )
      {
        try { registry->submit( calls[i] );++admitted; }
        catch( const koinos::node::p2p::LocalTransportUnavailable& ) { ++refused; }
        changed.notify_all();
      }
    } );
  { std::lock_guard lock( mutex );go = true; }
  changed.notify_all();
  { std::unique_lock lock( mutex );changed.wait_for( lock, 1s, [&] { return admitted >= 8; } ); }
  registry->stop_accepting();active->store( false );
  auto done = std::make_shared< std::promise< void > >();auto finished = done->get_future();
  boost::asio::post( io, [registry, done] { registry->cancel_all();done->set_value(); } );
  for( auto& thread: submitters ) thread.join();
  finished.get();guard.reset();runner.join();
  require( admitted >= 1 && admitted + refused == calls.size() && completed == admitted && wrong_error == 0,
           "concurrent shutdown must settle every accepted call once and reject later admission locally" );
}

void owned_decoder_compatibility()
{
  const gorpc::ServiceID service{ "PeerRPCService", "GetChainID" };
  const std::vector< std::string > frames{
    gorpc::encode_success_response( service, gorpc::encode_id_response( "test-chain" ) ),
    gorpc::encode_error_response( service, "controlled remote error", gorpc::ErrorType::server ),
    std::string( "\x90\xc0", 2 ), std::string( "\xc1\xc0", 2 )
  };
  for( const auto& frame: frames )
    for( size_t n = 0;n <= frame.size();++n )
    {
      auto decode = []( const std::string& raw, bool owned ) {
        try
        {
          const auto result = owned ? gorpc::decode_response_owned( raw ) : gorpc::decode_response( raw );
          return std::tuple( 0, result.header.service.name, result.header.service.method, result.header.error,
                             result.header.err_type, result.payload );
        }
        catch( const gorpc::DecodeError& e )
        { return std::tuple( e.truncated() ? 1 : 2, std::string(), std::string(), std::string(),
                             gorpc::ErrorType::non_rpc, std::string() ); }
      };
      const auto raw = frame.substr( 0, n );
      require( decode( raw, true ) == decode( raw, false ), "owned decoder must preserve prefix/error/byte behavior" );
    }
  auto raw = gorpc::encode_success_response( service, gorpc::encode_id_response( std::string( 256 * 1024, 'x' ) ) );
  const auto* allocation = raw.data();const auto expected = gorpc::decode_response( raw );
  const auto result = gorpc::decode_response_owned( std::move( raw ) );
  require( result.payload == expected.payload && result.payload.data() == allocation,
           "large owned payload reuses the frame allocation without a second complete payload" );

  for( bool header: { false, true } )
  {
    auto padded = gorpc::encode_success_response( service, gorpc::encode_id_response( "small" ) );
    if( header )
    {
      padded[0] = char( 0x82 ); // Add an ignored field before the existing Service field.
      padded.insert( 1, std::string( "\xa7Padding", 8 )
        + gorpc::encode_id_response( std::string( 512 * 1024, 'p' ) ) );
    }
    else padded += std::string( 512 * 1024, 't' ); // The existing decoder ignores trailing data.
    const auto expected = gorpc::decode_response( padded );
    const auto decoded = gorpc::decode_response_owned( std::move( padded ) );
    require( decoded.payload == expected.payload && decoded.payload.capacity() < 1024,
             "a small decoded payload must not retain a large ignored header or suffix" );
  }
}
} // namespace

int main()
{
  try
  {
    frames_and_counts();failures();deadlines_and_late_callbacks();local_cancellation_and_registry();
    concurrent_registry_shutdown();owned_decoder_compatibility();
    std::cout << "Peer RPC client fragmentation, deadlines, late ownership, cancellation and decoder compatibility passed\n";
    return 0;
  }
  catch( const std::exception& error ) { std::cerr << error.what() << '\n';return 1; }
}
