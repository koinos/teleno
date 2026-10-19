#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <libp2p/basic/writer.hpp>
#include <memory>
#include <new>
#include <system_error>
#include <utility>

namespace koinos::node::p2p::detail {

class RpcFrameWrite : public std::enable_shared_from_this< RpcFrameWrite >
{
public:
  RpcFrameWrite( boost::asio::io_context& io,
                 std::shared_ptr< libp2p::basic::Writer > writer,
                 std::shared_ptr< const libp2p::Bytes > bytes,
                 libp2p::basic::Writer::WriteCallbackFunc callback )
    : _io( io ), _writer( std::move( writer ) ), _bytes( std::move( bytes ) ),
      _callback( std::move( callback ) ) {}

  void start() noexcept
  {
    if( _completed )
      return;
    if( !_writer || !_bytes )
      return finish( std::make_error_code( std::errc::invalid_argument ) );
    if( _offset == _bytes->size() )
      return finish( _offset );

    try
    {
      // Hold the state through both synchronous and deferred SDK callbacks.
      const auto remaining = libp2p::BytesIn( *_bytes ).subspan( _offset );
      _writer->writeSome( remaining, remaining.size(),
        [self = shared_from_this()]( outcome::result< size_t > result ) {
          self->on_write( std::move( result ) );
        } );
    }
    catch( const std::bad_alloc& )
    {
      finish( std::make_error_code( std::errc::not_enough_memory ) );
    }
    catch( ... )
    {
      finish( std::make_error_code( std::errc::io_error ) );
    }
  }

  // Serialized with initiation/completions. A late SDK callback still owns the
  // frame storage it borrowed, but cannot continue writing after cancellation.
  void cancel() noexcept
  { finish( std::make_error_code( std::errc::operation_canceled ) ); }

private:
  void on_write( outcome::result< size_t > result ) noexcept
  {
    if( _completed )
      return;
    if( !result.has_value() )
      return finish( result.error() );
    const auto count = result.value();
    if( count == 0 || count > _bytes->size() - _offset )
      return finish( std::make_error_code( std::errc::io_error ) );
    _offset += count;
    if( _offset == _bytes->size() )
      return finish( _offset );

    try
    {
      // A writer may complete inline. Defer the next write to avoid an
      // unbounded recursive call stack and yield to the transport's I/O loop.
      boost::asio::post( _io, [self = shared_from_this()] { self->start(); } );
    }
    catch( const std::bad_alloc& )
    {
      finish( std::make_error_code( std::errc::not_enough_memory ) );
    }
    catch( ... )
    {
      finish( std::make_error_code( std::errc::io_error ) );
    }
  }

  void finish( outcome::result< size_t > result ) noexcept
  {
    if( _completed )
      return;
    _completed = true;
    auto callback = std::move( _callback );
    _callback = {};
    try
    {
      if( callback )
        callback( std::move( result ) );
    }
    catch( ... )
    {
      // A completion exception must not escape into the SDK's I/O runner or
      // trigger another write/completion after the frame has finished.
    }
  }

  boost::asio::io_context& _io;
  std::shared_ptr< libp2p::basic::Writer > _writer;
  std::shared_ptr< const libp2p::Bytes > _bytes;
  libp2p::basic::Writer::WriteCallbackFunc _callback;
  size_t _offset = 0;
  bool _completed = false;
};

// Initiation, SDK callbacks and continuations use the same serialized I/O
// context, which the caller keeps alive through completion. At most one write
// or continuation is outstanding for this frame. No new RPC is replayed.
inline std::shared_ptr< RpcFrameWrite > write_rpc_frame( boost::asio::io_context& io,
                             std::shared_ptr< libp2p::basic::Writer > writer,
                             std::shared_ptr< const libp2p::Bytes > bytes,
                             libp2p::basic::Writer::WriteCallbackFunc callback )
{
  auto write = std::make_shared< RpcFrameWrite >(
    io, std::move( writer ), std::move( bytes ), std::move( callback ) );
  write->start();
  return write;
}

} // namespace koinos::node::p2p::detail
