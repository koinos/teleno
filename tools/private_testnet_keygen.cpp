#include <koinos/crypto/elliptic.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/base64.hpp>
#include <koinos/util/conversion.hpp>

#include <iostream>
#include <string>

int main( int argc, char** argv )
{
  const std::string seed = argc > 1 ? argv[ 1 ] : "teleno-private-testnet-producer";
  auto key = koinos::crypto::private_key::regenerate(
    koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, seed ) );
  auto address = key.get_public_key().to_address_bytes();
  auto public_key = key.get_public_key().serialize();
  auto genesis_key = koinos::util::converter::as< std::string >(
    koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, std::string( "object_key::genesis_key" ) ) );

  std::cout << "seed=" << seed << '\n';
  std::cout << "wif=" << key.to_wif() << '\n';
  std::cout << "address=" << koinos::util::to_base58( address ) << '\n';
  std::cout << "address_base64=" << koinos::util::to_base64( address, false ) << '\n';
  std::cout << "public_key_base64=" << koinos::util::to_base64( public_key ) << '\n';
  std::cout << "genesis_key_base64=" << koinos::util::to_base64( genesis_key, false ) << '\n';
  return 0;
}
