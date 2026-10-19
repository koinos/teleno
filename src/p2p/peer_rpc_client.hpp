#pragma once

#include "p2p/gorpc_codec.hpp"
#include "p2p/local_transport_error.hpp"
#include "p2p/peer_rpc_write.hpp"

#include <algorithm>
#include <atomic>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <exception>
#include <functional>
#include <libp2p/basic/reader.hpp>
#include <memory>
#include <mutex>
#include <vector>

namespace koinos::node::p2p::detail {

class RpcClientError final : public std::exception
{
public:
  enum class Reason { open_timeout, write_timeout, response_timeout, unavailable,
                      no_progress, invalid_count };
  explicit RpcClientError( Reason input ) noexcept : reason( input ) {}
  const char* what() const noexcept override
  {
    switch( reason )
    {
      case Reason::open_timeout: return "Timed out opening Peer RPC stream";
      case Reason::write_timeout: return "Timed out writing Peer RPC request";
      case Reason::response_timeout: return "Timed out reading complete Peer RPC response";
      case Reason::unavailable: return "Peer RPC stream unavailable";
      case Reason::no_progress: return "Peer RPC stream closed before complete response";
      case Reason::invalid_count: return "Invalid Peer RPC response read count";
    }
    return "Peer RPC client failed";
  }
  const Reason reason;
};

struct RpcClientChannel
{
  std::shared_ptr< libp2p::basic::Reader > reader;
  std::shared_ptr< libp2p::basic::Writer > writer;
  std::function< void() > close;
  std::function< void() > abort;
};
using RpcClientOpenResult = std::function< void( std::exception_ptr, RpcClientChannel ) >;
using RpcClientOpener = std::function< void( RpcClientOpenResult ) >;
using RpcClientCompletion = std::function< void( std::exception_ptr, gorpc::Response ) >;

// State and SDK callbacks are serialized on the transport's I/O runner. That
// runner outlives all callbacks, including late callbacks holding borrowed
// storage. Only request_cancel() is called by another thread.
class RpcClientCall : public std::enable_shared_from_this< RpcClientCall >
{
public:
  static constexpr size_t maximum_response_bytes = 64 * 1024 * 1024;
  static constexpr size_t read_chunk_bytes = 64 * 1024;
  using Reason = RpcClientError::Reason;

  RpcClientCall( boost::asio::io_context& io, RpcClientOpener opener,
                 std::shared_ptr< const libp2p::Bytes > request,
                 RpcClientCompletion completion,
                 std::shared_ptr< std::atomic< bool > > active,
                 std::chrono::steady_clock::duration timeout = std::chrono::seconds( 6 ),
                 size_t maximum_frame = maximum_response_bytes )
    : _io( io ), _opener( std::move( opener ) ), _request( std::move( request ) ),
      _completion( std::move( completion ) ), _active( std::move( active ) ),
      _deadline( io ), _timeout( timeout ), _maximum_frame( maximum_frame ) {}

  void begin() noexcept
  {
    try
    {
      if( !active() ) return cancel();
      if( !_opener || !_request ) return fail( Reason::unavailable );
      if( _maximum_frame == 0 || _maximum_frame > maximum_response_bytes )
        return capacity();
      arm_deadline( Reason::open_timeout );
      auto opener = std::move( _opener );_opener = {};
      opener( [weak = weak_from_this()]( std::exception_ptr error, RpcClientChannel channel ) {
        if( auto self = weak.lock() ) self->opened( error, std::move( channel ) );
        else dispose( channel, true );
      } );
    }
    catch( ... ) { finish( std::current_exception() ); }
  }

  void request_cancel() noexcept { _cancel_requested.store( true, std::memory_order_release ); }
  void cancel() noexcept
  { finish( std::make_exception_ptr( LocalTransportUnavailable() ) ); }

private:
  bool active() const noexcept
  {
    return !_cancel_requested.load( std::memory_order_acquire )
      && ( !_active || _active->load( std::memory_order_acquire ) );
  }
  static void dispose( RpcClientChannel& channel, bool abort ) noexcept
  {
    try
    {
      auto& action = abort ? channel.abort : channel.close;
      if( action ) action();
    }
    catch( ... )
    {
      if( !abort && channel.abort )
        try { channel.abort(); } catch( ... ) {}
    }
    channel = {};
  }
  void arm_deadline( Reason reason )
  {
    const auto generation = ++_timer_generation;
    _deadline.expires_after( _timeout );
    _deadline.async_wait( [weak = weak_from_this(), generation, reason]( const boost::system::error_code& error ) {
      if( error ) return;
      if( auto self = weak.lock(); self && generation == self->_timer_generation )
      {
        if( !self->active() ) self->cancel();
        else self->fail( reason );
      }
    } );
  }
  void opened( std::exception_ptr error, RpcClientChannel channel ) noexcept
  {
    if( _completed ) { dispose( channel, true );return; }
    _channel = std::move( channel );
    if( !active() ) return cancel();
    if( error ) return finish( error );
    try
    {
      if( !_channel.reader || !_channel.writer ) return fail( Reason::unavailable );
      arm_deadline( Reason::write_timeout );
      _write = write_rpc_frame( _io, _channel.writer, _request,
        [self = shared_from_this()]( outcome::result< size_t > result ) {
          self->written( std::move( result ) );
        } );
    }
    catch( ... ) { finish( std::current_exception() ); }
  }
  void written( outcome::result< size_t > result ) noexcept
  {
    if( _completed ) return;
    if( !active() ) return cancel();
    try
    {
      if( !result.has_value() )
      {
        if( result.error() == std::errc::not_enough_memory ) throw std::bad_alloc();
        throw std::runtime_error( "Failed to write RPC request: " + result.error().message() );
      }
      if( result.value() != _request->size() ) return fail( Reason::invalid_count );
      _request.reset();
      _buffer.resize( std::min( read_chunk_bytes, _maximum_frame ) );
      // This deadline spans the entire response. Fragments never re-arm it.
      arm_deadline( Reason::response_timeout );
      read_next();
    }
    catch( ... ) { finish( std::current_exception() ); }
  }
  void read_next() noexcept
  {
    if( _completed ) return;
    if( !active() ) return cancel();
    try
    {
      _read_window = std::min( _buffer.size(), _maximum_frame - _raw.size() );
      if( _read_window == 0 ) return capacity();
      _channel.reader->readSome( libp2p::BytesOut( _buffer ).first( _read_window ), _read_window,
        [self = shared_from_this()]( outcome::result< size_t > result ) {
          self->read( std::move( result ) );
        } );
    }
    catch( ... ) { finish( std::current_exception() ); }
  }
  void read( outcome::result< size_t > result ) noexcept
  {
    if( _completed ) return;
    if( !active() ) return cancel();
    try
    {
      if( !result.has_value() )
      {
        if( result.error() == std::errc::not_enough_memory ) throw std::bad_alloc();
        throw std::runtime_error( "Failed to read RPC response: " + result.error().message() );
      }
      const auto count = result.value();
      if( count == 0 ) return fail( Reason::no_progress );
      if( count > _read_window ) return fail( Reason::invalid_count );
      const std::string_view added( reinterpret_cast< const char* >( _buffer.data() ), count );
      _scanner.consume( added );
      const auto needed = _raw.size() + count;
      if( needed > _raw.capacity() )
        _raw.reserve( std::min( _maximum_frame,
          std::max( needed, _raw.capacity() > _maximum_frame / 2 ? _maximum_frame : _raw.capacity() * 2 ) ) );
      _raw.append( added );
      if( _scanner.complete() )
        return finish( {}, gorpc::decode_response_owned( std::move( _raw ) ) );
      if( _raw.size() == _maximum_frame ) return capacity();
      boost::asio::post( _io, [self = shared_from_this()] { self->read_next(); } );
    }
    catch( ... ) { finish( std::current_exception() ); }
  }
  void fail( Reason reason ) noexcept
  { finish( std::make_exception_ptr( RpcClientError( reason ) ) ); }
  void capacity() noexcept
  { finish( std::make_exception_ptr( LocalTransportUnavailable( LocalTransportUnavailable::Reason::response_capacity ) ) ); }
  void finish( std::exception_ptr error, gorpc::Response response = {} ) noexcept
  {
    if( _completed ) return;
    _completed = true;
    boost::system::error_code ignored;_deadline.cancel( ignored );
    if( auto write = _write.lock() ) write->cancel();
    dispose( _channel, bool( error ) );
    _opener = {};_request.reset();
    std::string().swap( _raw );
    auto completion = std::move( _completion );_completion = {};
    try { if( completion ) completion( error, std::move( response ) ); }
    catch( ... ) {} // Never retry the request or escape the SDK runner.
  }

  boost::asio::io_context& _io;
  RpcClientOpener _opener;
  std::shared_ptr< const libp2p::Bytes > _request;
  RpcClientCompletion _completion;
  std::shared_ptr< std::atomic< bool > > _active;
  std::atomic< bool > _cancel_requested{ false };
  RpcClientChannel _channel;
  std::weak_ptr< RpcFrameWrite > _write;
  boost::asio::steady_timer _deadline;
  std::chrono::steady_clock::duration _timeout;
  uint64_t _timer_generation = 0;
  const size_t _maximum_frame;
  libp2p::Bytes _buffer;
  size_t _read_window = 0;
  std::string _raw;
  gorpc::ObjectSequenceScanner _scanner{ 2 };
  bool _completed = false;
};

// Admission and shutdown share a mutex so no accepted initiation can be posted
// behind shutdown's drain. The weak inventory is accessed only by the I/O runner.
class RpcClientRegistry : public std::enable_shared_from_this< RpcClientRegistry >
{
public:
  RpcClientRegistry( boost::asio::io_context& io, std::shared_ptr< std::atomic< bool > > active )
    : _io( io ), _active( std::move( active ) ) {}
  void submit( std::shared_ptr< RpcClientCall > call )
  {
    std::lock_guard lock( _admission );
    if( !_accepting || !_active->load( std::memory_order_acquire ) ) throw LocalTransportUnavailable();
    boost::asio::post( _io, [self = shared_from_this(), call] {
      try
      {
        self->_calls.erase( std::remove_if( self->_calls.begin(), self->_calls.end(),
          []( const auto& weak ) { return weak.expired(); } ), self->_calls.end() );
        self->_calls.emplace_back( call ); // Track before SDK initiation.
        call->begin();
      }
      catch( ... ) { call->request_cancel();call->cancel(); }
    } );
  }
  void stop_accepting()
  {
    std::lock_guard lock( _admission );
    _accepting = false;
  }
  void request_cancel( const std::shared_ptr< RpcClientCall >& call )
  {
    call->request_cancel();
    std::lock_guard lock( _admission );
    if( _accepting ) boost::asio::post( _io, [call] { call->cancel(); } );
  }
  void cancel_all() noexcept
  {
    for( const auto& weak: _calls ) if( auto call = weak.lock() ) call->cancel();
    std::vector< std::weak_ptr< RpcClientCall > >().swap( _calls );
  }
private:
  boost::asio::io_context& _io;
  std::shared_ptr< std::atomic< bool > > _active;
  std::mutex _admission;
  bool _accepting = true;
  std::vector< std::weak_ptr< RpcClientCall > > _calls;
};

} // namespace koinos::node::p2p::detail
