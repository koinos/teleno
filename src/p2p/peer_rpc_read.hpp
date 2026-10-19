#pragma once

#include "p2p/gorpc_codec.hpp"

#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <exception>
#include <functional>
#include <libp2p/basic/reader.hpp>
#include <memory>
#include <system_error>

namespace koinos::node::p2p::detail {

using RpcRequestReadCallback = std::function< void( std::exception_ptr, gorpc::DecodedRequest ) >;

class RpcRequestReadError final : public std::exception
{
public:
  enum class Reason { unavailable, no_progress, invalid_count, capacity, cancelled, timeout };
  explicit RpcRequestReadError( Reason reason ) noexcept : reason( reason ) {}
  const char* what() const noexcept override
  {
    switch( reason )
    {
      case Reason::unavailable: return "Peer RPC reader unavailable";
      case Reason::no_progress: return "Peer RPC stream closed before complete request";
      case Reason::invalid_count: return "Invalid Peer RPC read count";
      case Reason::capacity: return "Peer RPC request exceeds incoming frame capacity";
      case Reason::cancelled: return "Peer RPC request read cancelled";
      case Reason::timeout: return "Timed out reading Peer RPC request";
    }
    return "Peer RPC request read failed";
  }
  const Reason reason;
};

// Initiation, cancellation and SDK callbacks use the serialized I/O context.
// The caller keeps it alive through all SDK callbacks. Inactive generations neither start
// I/O nor dispatch work. A pending SDK callback retains its borrowed buffer
// even after logical completion/cancellation.
class RpcRequestRead : public std::enable_shared_from_this< RpcRequestRead >
{
public:
  static constexpr size_t max_frame_bytes = 64 * 1024; // Existing incoming window.
  using Reason = RpcRequestReadError::Reason;

  RpcRequestRead( boost::asio::io_context& io,
                  std::shared_ptr< libp2p::basic::Reader > reader,
                  RpcRequestReadCallback callback,
                  std::function< void() > abort,
                  std::chrono::steady_clock::duration timeout,
                  std::shared_ptr< std::atomic< bool > > active )
    : _io( io ), _reader( std::move( reader ) ), _callback( std::move( callback ) ),
      _abort( std::move( abort ) ), _active( std::move( active ) ), _deadline( io ),
      _timeout( timeout ), _buffer( max_frame_bytes ) {}

  void begin() noexcept
  {
    try
    {
      if( !active() ) return cancel( Reason::cancelled );
      if( !_reader ) return fail( Reason::unavailable );
      if( _timeout > std::chrono::steady_clock::duration::zero() )
      {
        _deadline.expires_after( _timeout );
        _deadline.async_wait( [weak = weak_from_this()]( const boost::system::error_code& error ) {
          if( !error )
            if( auto self = weak.lock() ) self->cancel( Reason::timeout );
        } );
      }
      read_next();
    }
    catch( ... ) { finish( std::current_exception() ); }
  }

  void cancel( Reason reason = Reason::cancelled, bool force_abort = false ) noexcept
  { finish( std::make_exception_ptr( RpcRequestReadError( reason ) ), {}, force_abort || active() ); }

  size_t buffer_capacity() const noexcept { return _buffer.capacity(); }

private:
  bool active() const noexcept { return !_active || _active->load( std::memory_order_acquire ); }

  void read_next() noexcept
  {
    if( _completed ) return;
    if( !active() ) return cancel();
    try
    {
      const auto remaining = libp2p::BytesOut( _buffer ).subspan( _used );
      _reader->readSome( remaining, remaining.size(),
        [self = shared_from_this()]( outcome::result< size_t > result ) {
          self->on_read( std::move( result ) );
        } );
    }
    catch( ... ) { finish( std::current_exception(), {}, active() ); }
  }

  void on_read( outcome::result< size_t > result ) noexcept
  {
    if( _completed ) return;
    if( !active() ) return cancel();
    try
    {
      if( !result.has_value() ) throw std::system_error( result.error() );
      const auto count = result.value();
      if( count == 0 ) return fail( Reason::no_progress );
      if( count > _buffer.size() - _used ) return fail( Reason::invalid_count );
      const std::string_view added( reinterpret_cast< const char* >( _buffer.data() + _used ), count );
      _used += count;
      _scanner.consume( added );
      if( _scanner.complete() )
      {
        auto request = gorpc::decode_request(
          std::string_view( reinterpret_cast< const char* >( _buffer.data() ), _used ) );
        return finish( {}, std::move( request ) );
      }
      if( _used == _buffer.size() ) return fail( Reason::capacity );
      boost::asio::post( _io, [self = shared_from_this()] { self->read_next(); } );
    }
    catch( ... ) { finish( std::current_exception() ); }
  }

  void fail( Reason reason ) noexcept
  { finish( std::make_exception_ptr( RpcRequestReadError( reason ) ) ); }

  void finish( std::exception_ptr error, gorpc::DecodedRequest request = {}, bool abort = false ) noexcept
  {
    if( _completed ) return;
    _completed = true;
    boost::system::error_code ignored;
    _deadline.cancel( ignored );
    auto callback = std::move( _callback );
    auto cancel_io = std::move( _abort );
    _callback = {};
    _abort = {};
    if( abort && cancel_io )
      try { cancel_io(); } catch( ... ) {}
    try { if( callback ) callback( error, std::move( request ) ); }
    catch( ... ) {} // Never escape the SDK runner or retry application work.
  }

  boost::asio::io_context& _io;
  std::shared_ptr< libp2p::basic::Reader > _reader;
  RpcRequestReadCallback _callback;
  std::function< void() > _abort;
  std::shared_ptr< std::atomic< bool > > _active;
  boost::asio::steady_timer _deadline;
  std::chrono::steady_clock::duration _timeout;
  libp2p::Bytes _buffer;
  gorpc::ObjectSequenceScanner _scanner{ 2 };
  size_t _used = 0;
  bool _completed = false;
};

inline std::shared_ptr< RpcRequestRead > read_rpc_request(
  boost::asio::io_context& io,
  std::shared_ptr< libp2p::basic::Reader > reader,
  RpcRequestReadCallback callback,
  std::function< void() > abort = {},
  std::chrono::steady_clock::duration timeout = std::chrono::seconds( 6 ),
  std::shared_ptr< std::atomic< bool > > active = {} )
{
  auto read = std::make_shared< RpcRequestRead >(
    io, std::move( reader ), std::move( callback ), std::move( abort ), timeout, std::move( active ) );
  read->begin();
  return read;
}

} // namespace koinos::node::p2p::detail
