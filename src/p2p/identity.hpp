#pragma once

#ifdef KOINOS_HAS_LIBP2P

#include "core/config.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include <libp2p/crypto/key.hpp>

namespace koinos::node::p2p {

struct P2PIdentity
{
  std::string source;
  std::string peer_id;
  std::filesystem::path key_file;
  std::vector< std::string > advertised_multiaddrs;
  libp2p::crypto::KeyPair key_pair;
};

P2PIdentity resolve_p2p_identity( const NodeConfig& cfg );

} // namespace koinos::node::p2p

#endif // KOINOS_HAS_LIBP2P
