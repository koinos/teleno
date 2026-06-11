#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace koinos::node::p2p::gorpc {

class DecodeError final : public std::runtime_error
{
public:
  DecodeError( std::string message, bool truncated );

  [[nodiscard]] bool truncated() const noexcept { return _truncated; }

private:
  bool _truncated;
};

struct ServiceID
{
  std::string name;
  std::string method;
};

enum class ErrorType : uint64_t
{
  non_rpc       = 0,
  server        = 1,
  client        = 2,
  authorization = 3
};

struct ResponseHeader
{
  ServiceID service;
  std::string error;
  ErrorType err_type = ErrorType::non_rpc;
};

struct Response
{
  ResponseHeader header;
  std::string payload;
};

struct DecodedRequest
{
  ServiceID service;
  std::string args;
};

struct HeadBlockResponse
{
  std::string id;
  uint64_t height = 0;
};

struct AncestorBlockIDRequest
{
  std::string parent_id;
  uint64_t child_height = 0;
};

struct BlocksRequest
{
  std::string head_block_id;
  uint64_t start_block_height = 0;
  uint32_t num_blocks = 0;
};

std::string encode_empty_request();
std::string encode_get_ancestor_block_id_request( std::string_view parent_id, uint64_t child_height );
std::string encode_get_blocks_request( std::string_view head_block_id,
                                        uint64_t start_block_height,
                                        uint32_t num_blocks );
std::string encode_id_response( std::string_view id );
std::string encode_head_block_response( std::string_view id, uint64_t height );
std::string encode_blocks_response( const std::vector< std::string >& block_payloads );

std::string encode_request( std::string_view service,
                             std::string_view method,
                             std::string_view msgpack_args );

std::string encode_success_response( const ServiceID& service, std::string_view msgpack_payload );
std::string encode_error_response( const ServiceID& service,
                                   std::string_view error,
                                   ErrorType err_type );

DecodedRequest decode_request( std::string_view raw );
Response decode_response( std::string_view raw );

AncestorBlockIDRequest decode_get_ancestor_block_id_request( std::string_view payload );
BlocksRequest decode_get_blocks_request( std::string_view payload );
std::string decode_id_response( std::string_view payload );
HeadBlockResponse decode_head_block_response( std::string_view payload );
std::vector< std::string > decode_blocks_response( std::string_view payload );

} // namespace koinos::node::p2p::gorpc
