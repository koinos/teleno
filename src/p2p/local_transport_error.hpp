#pragma once
#include <exception>

namespace koinos::node::p2p {

// A local lifecycle/refusal condition, never remote misconduct or consensus invalidity.
class LocalTransportUnavailable final : public std::exception
{
public:
  enum class Reason { lifecycle, response_capacity, dial_capacity, peer_snapshot_capacity };
  explicit LocalTransportUnavailable( Reason value = Reason::lifecycle ) noexcept : reason( value ) {}
  const char* what() const noexcept override
  {
    if( reason == Reason::peer_snapshot_capacity ) return "Local peer snapshot admission capacity exhausted";
    if( reason == Reason::dial_capacity ) return "Local peer dial admission capacity exhausted";
    return reason == Reason::response_capacity
      ? "Local Peer RPC response capacity exhausted"
      : "Local Peer RPC transport stopped or unavailable";
  }
  const Reason reason;
};

} // namespace koinos::node::p2p
