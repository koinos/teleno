#include "p2p/gorpc_codec.hpp"
#include "p2p/peer_rpc_write.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

void require( bool condition, const char* message )
{
  if( !condition )
    throw std::runtime_error( message );
}

struct ShortWriter : libp2p::basic::Writer
{
  enum class Fault { none, zero, overcount, disconnected, throwing, allocation };

  boost::asio::io_context& io;
  explicit ShortWriter( boost::asio::io_context& context ) : io( context ) {}
  size_t chunk = 7;
  size_t calls = 0;
  size_t fail_on = 2;
  size_t depth = 0;
  size_t maximum_depth = 0;
  bool deferred = false;
  bool closed = false;
  bool outstanding = false;
  Fault fault = Fault::none;
  std::string written;

  void writeSome( libp2p::BytesIn input, size_t size, WriteCallbackFunc callback ) override
  {
    require( !closed && !outstanding, "writes must be sequential and precede close" );
    require( size == input.size() && size > 0, "writer must receive a nonempty suffix" );
    ++calls;
    const auto selected = calls == fail_on ? fault : Fault::none;
    if( selected == Fault::throwing )
      throw std::runtime_error( "write initiation failed" );
    if( selected == Fault::allocation )
      throw std::bad_alloc();

    outstanding = true;
    auto complete = [this, input, size, selected, callback = std::move( callback )]() mutable {
      ++depth;
      maximum_depth = std::max( maximum_depth, depth );
      outstanding = false;
      if( selected == Fault::zero )
        callback( size_t( 0 ) );
      else if( selected == Fault::overcount )
        callback( size + 1 );
      else if( selected == Fault::disconnected )
        callback( std::make_error_code( std::errc::connection_reset ) );
      else
      {
        const auto count = std::min( size, chunk );
        // Copy at completion, as the SDK borrows the input until then.
        written.append( reinterpret_cast< const char* >( input.data() ), count );
        callback( count );
      }
      --depth;
    };
    if( deferred )
      boost::asio::post( io, std::move( complete ) );
    else
      complete();
  }
  void deferWriteCallback( std::error_code error, WriteCallbackFunc callback ) override
  {
    if( error )
      boost::asio::post( io, [error, callback = std::move( callback )] { callback( error ); } );
  }
};

void full_frame_regression( const std::string& frame, bool deferred, size_t chunk )
{
  boost::asio::io_context io;
  auto writer = std::make_shared< ShortWriter >( io );
  writer->deferred = deferred;
  writer->chunk = chunk;
  auto bytes = std::make_shared< const libp2p::Bytes >( frame.begin(), frame.end() );
  std::weak_ptr< const libp2p::Bytes > weak_bytes = bytes;
  size_t completions = 0;
  bool retained_until_completion = false;
  std::optional< outcome::result< size_t > > completed;
  koinos::node::p2p::detail::write_rpc_frame( io, writer, bytes,
    [&]( outcome::result< size_t > result ) {
      ++completions;
      retained_until_completion = !weak_bytes.expired();
      completed.emplace( std::move( result ) );
      writer->closed = true; // Model the real response helper's close callback.
    } );
  bytes.reset();
  io.run();
  require( completions == 1, "frame must complete once" );
  require( completed && completed->has_value(), "valid short writes must succeed" );
  require( completed->value() == frame.size(), "successful completion truncated the gorpc frame" );
  require( writer->written == frame, "wire bytes must contain the complete unchanged gorpc frame" );
  require( retained_until_completion && weak_bytes.expired(), "input must outlive writes and then be released" );
  require( writer->maximum_depth == ( frame.empty() ? 0 : 1 ), "inline callbacks must not recurse" );
  require( writer->calls == ( frame.size() + chunk - 1 ) / chunk, "write count must match exact suffixes" );
}

void error_cases()
{
  for( bool deferred: { false, true } )
  {
    for( auto fault: { ShortWriter::Fault::zero, ShortWriter::Fault::overcount,
                      ShortWriter::Fault::disconnected, ShortWriter::Fault::throwing,
                      ShortWriter::Fault::allocation } )
    {
      boost::asio::io_context io;
      auto writer = std::make_shared< ShortWriter >( io );
      writer->deferred = deferred;
      writer->fault = fault;
      auto bytes = std::make_shared< const libp2p::Bytes >( 100, 'x' );
      size_t completions = 0;
      std::optional< outcome::result< size_t > > completed;
      koinos::node::p2p::detail::write_rpc_frame( io, writer, bytes,
        [&]( outcome::result< size_t > result ) {
          ++completions;
          completed.emplace( std::move( result ) );
          writer->closed = true;
        } );
      io.run();
      require( completions == 1 && completed && !completed->has_value(), "incomplete writes must fail exactly once" );
      require( writer->calls == 2 && writer->written == std::string( 7, 'x' ), "failed write must not replay the frame" );
      if( fault == ShortWriter::Fault::allocation )
        require( completed->error() == std::errc::not_enough_memory, "local allocation failure must remain distinguishable" );
      if( fault == ShortWriter::Fault::disconnected )
        require( completed->error() == std::errc::connection_reset, "transport errors must be preserved" );
    }
  }
}

void caller_release_and_completion_exception()
{
  boost::asio::io_context io;
  auto writer = std::make_shared< ShortWriter >( io );
  writer->deferred = true;
  auto bytes = std::make_shared< const libp2p::Bytes >( 31, 'y' );
  std::weak_ptr< ShortWriter > weak_writer = writer;
  std::weak_ptr< const libp2p::Bytes > weak_bytes = bytes;
  size_t completions = 0;
  bool success = false;
  bool owners_alive = false;
  koinos::node::p2p::detail::write_rpc_frame( io, writer, bytes,
    [&]( outcome::result< size_t > result ) {
      ++completions;
      owners_alive = !weak_writer.expired() && !weak_bytes.expired();
      success = result.has_value() && result.value() == 31;
      throw std::runtime_error( "completion exception must not escape the I/O runner" );
    } );
  writer.reset();
  bytes.reset();
  require( !weak_writer.expired() && !weak_bytes.expired(), "in-flight writes must retain their owners" );
  io.run();
  require( completions == 1 && success && owners_alive, "owned frame must complete exactly once" );
  require( weak_writer.expired() && weak_bytes.expired(), "completed operation must release owners even if completion throws" );
}

void unavailable_inputs()
{
  boost::asio::io_context io;
  auto writer = std::make_shared< ShortWriter >( io );
  auto bytes = std::make_shared< const libp2p::Bytes >( 1, 'z' );
  for( bool missing_writer: { false, true } )
  {
    std::optional< outcome::result< size_t > > completed;
    koinos::node::p2p::detail::write_rpc_frame( io, missing_writer ? nullptr : writer,
      missing_writer ? bytes : nullptr, [&]( outcome::result< size_t > result ) { completed.emplace( std::move( result ) ); } );
    require( completed && !completed->has_value() && completed->error() == std::errc::invalid_argument,
             "unavailable writer or input must fail without dereference" );
  }
  require( writer->calls == 0, "unavailable inputs must not initiate I/O" );
}

} // namespace

int main()
{
  try
  {
    namespace gorpc = koinos::node::p2p::gorpc;
    const auto request = gorpc::encode_request( "PeerRPCService", "GetChainID", gorpc::encode_empty_request() );
    const gorpc::ServiceID service{ "PeerRPCService", "GetBlocks" };
    const auto payload = gorpc::encode_blocks_response( { std::string( 262144, 'b' ) } );
    const auto response = gorpc::encode_success_response( service, payload );
    const auto error = gorpc::encode_error_response( service, "server failure", gorpc::ErrorType::server );
    full_frame_regression( request, false, 7 );
    full_frame_regression( request, true, 7 );
    full_frame_regression( response, false, 1 );
    full_frame_regression( response, true, 5003 );
    full_frame_regression( error, false, 1 );
    full_frame_regression( error, true, 7 );
    full_frame_regression( request, false, request.size() );
    full_frame_regression( {}, false, 1 );
    error_cases();
    caller_release_and_completion_exception();
    unavailable_inputs();
    std::cout << "Peer RPC writes: exact request/response/error frames, inline/deferred short writes, "
                 "failure paths and ownership passed\n";
    return 0;
  }
  catch( const std::exception& e )
  {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
