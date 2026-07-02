#include "identity.hpp"

#ifdef KOINOS_HAS_LIBP2P

#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <system_error>

#include <sys/stat.h>

#include <libp2p/crypto/crypto_provider/crypto_provider_impl.hpp>
#include <libp2p/crypto/ecdsa_provider/ecdsa_provider_impl.hpp>
#include <libp2p/crypto/ed25519_provider/ed25519_provider_impl.hpp>
#include <libp2p/crypto/hmac_provider/hmac_provider_impl.hpp>
#include <libp2p/crypto/key_marshaller/key_marshaller_impl.hpp>
#include <libp2p/crypto/key_validator/key_validator_impl.hpp>
#include <libp2p/crypto/random_generator/boost_generator.hpp>
#include <libp2p/crypto/rsa_provider/rsa_provider_impl.hpp>
#include <libp2p/crypto/secp256k1_provider/secp256k1_provider_impl.hpp>
#include <libp2p/peer/peer_id.hpp>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

namespace koinos::node::p2p {

namespace {

#include "go_math_rand_cooked.inc"

constexpr int k_go_rng_len = 607;
constexpr int k_go_rng_tap = 273;
constexpr uint64_t k_go_rng_mask = ( uint64_t{ 1 } << 63 ) - 1;
constexpr int32_t k_go_int32_max = std::numeric_limits< int32_t >::max();

std::shared_ptr< libp2p::crypto::CryptoProvider > make_crypto_provider()
{
  auto csprng = std::make_shared< libp2p::crypto::random::BoostRandomGenerator >();
  auto ed25519_provider = std::make_shared< libp2p::crypto::ed25519::Ed25519ProviderImpl >();
  auto rsa_provider = std::make_shared< libp2p::crypto::rsa::RsaProviderImpl >();
  auto ecdsa_provider = std::make_shared< libp2p::crypto::ecdsa::EcdsaProviderImpl >();
  auto secp256k1_provider = std::make_shared< libp2p::crypto::secp256k1::Secp256k1ProviderImpl >( csprng );
  auto hmac_provider = std::make_shared< libp2p::crypto::hmac::HmacProviderImpl >();
  return std::make_shared< libp2p::crypto::CryptoProviderImpl >(
    csprng, ed25519_provider, rsa_provider, ecdsa_provider, secp256k1_provider, hmac_provider );
}

std::shared_ptr< libp2p::crypto::marshaller::KeyMarshaller > make_key_marshaller(
  const std::shared_ptr< libp2p::crypto::CryptoProvider >& provider )
{
  auto validator = std::make_shared< libp2p::crypto::validator::KeyValidatorImpl >( provider );
  return std::make_shared< libp2p::crypto::marshaller::KeyMarshallerImpl >( validator );
}

std::vector< uint8_t > read_file_bytes( const std::filesystem::path& path )
{
  std::ifstream in( path, std::ios::binary );
  if( !in )
    throw std::runtime_error( "failed to open P2P identity key file: " + path.string() );

  return std::vector< uint8_t >(
    std::istreambuf_iterator< char >( in ),
    std::istreambuf_iterator< char >() );
}

void write_private_key_file( const std::filesystem::path& path, const std::vector< uint8_t >& bytes )
{
  std::filesystem::create_directories( path.parent_path() );
  const auto tmp = path.string() + ".tmp";

  {
    std::ofstream out( tmp, std::ios::binary | std::ios::trunc );
    if( !out )
      throw std::runtime_error( "failed to create P2P identity key file: " + path.string() );
    out.write( reinterpret_cast< const char* >( bytes.data() ), static_cast< std::streamsize >( bytes.size() ) );
    if( !out )
      throw std::runtime_error( "failed to write P2P identity key file: " + path.string() );
  }

  chmod( tmp.c_str(), S_IRUSR | S_IWUSR );
  std::filesystem::rename( tmp, path );
  chmod( path.c_str(), S_IRUSR | S_IWUSR );
}

std::vector< uint8_t > marshal_private_key( const libp2p::crypto::PrivateKey& key )
{
  auto provider = make_crypto_provider();
  auto marshaller = make_key_marshaller( provider );
  auto encoded = marshaller->marshal( key );
  if( !encoded )
    throw std::runtime_error( "failed to serialize P2P identity key: " + encoded.error().message() );
  return encoded.value().key;
}

libp2p::crypto::PrivateKey unmarshal_private_key( const std::vector< uint8_t >& encoded )
{
  auto provider = make_crypto_provider();
  auto marshaller = make_key_marshaller( provider );
  libp2p::crypto::ProtobufKey protobuf_key{ encoded };
  auto key = marshaller->unmarshalPrivateKey( protobuf_key );
  if( !key )
    throw std::runtime_error( "P2P identity key file is not a valid libp2p private key: " + key.error().message() );
  return key.value();
}

libp2p::crypto::ProtobufKey marshal_public_key_for_peer_id( const libp2p::crypto::PublicKey& key )
{
  auto provider = make_crypto_provider();
  auto marshaller = make_key_marshaller( provider );
  auto encoded = marshaller->marshal( key );
  if( !encoded )
    throw std::runtime_error( "failed to serialize P2P public key: " + encoded.error().message() );
  return encoded.value();
}

libp2p::crypto::KeyPair complete_key_pair( const libp2p::crypto::PrivateKey& private_key )
{
  auto provider = make_crypto_provider();
  auto public_key = provider->derivePublicKey( private_key );
  if( !public_key )
    throw std::runtime_error( "failed to derive P2P public key from private key: " + public_key.error().message() );
  return { public_key.value(), private_key };
}

std::string peer_id_from_key_pair( const libp2p::crypto::KeyPair& key_pair )
{
  auto protobuf_key = marshal_public_key_for_peer_id( key_pair.publicKey );
  auto peer_id = libp2p::peer::PeerId::fromPublicKey( protobuf_key );
  if( !peer_id )
    throw std::runtime_error( "failed to derive P2P peer ID: " + peer_id.error().message() );
  return peer_id.value().toBase58();
}

libp2p::crypto::KeyPair generate_ed25519_key_pair()
{
  auto provider = make_crypto_provider();
  auto key_pair = provider->generateKeys( libp2p::crypto::Key::Type::Ed25519 );
  if( !key_pair )
    throw std::runtime_error( "failed to generate P2P identity key: " + key_pair.error().message() );
  return key_pair.value();
}

uint64_t int64_to_bits( int64_t value )
{
  uint64_t bits = 0;
  std::memcpy( &bits, &value, sizeof( bits ) );
  return bits;
}

int64_t bits_to_int64( uint64_t value )
{
  int64_t bits = 0;
  std::memcpy( &bits, &value, sizeof( bits ) );
  return bits;
}

int32_t legacy_go_seedrand( int32_t x )
{
  constexpr int32_t a = 48271;
  constexpr int32_t q = 44488;
  constexpr int32_t r = 3399;

  const int32_t hi = x / q;
  const int32_t lo = x % q;
  x = a * lo - r * hi;
  if( x < 0 )
    x += k_go_int32_max;
  return x;
}

class LegacyGoRand
{
public:
  explicit LegacyGoRand( int64_t seed )
  {
    tap_ = 0;
    feed_ = k_go_rng_len - k_go_rng_tap;

    seed %= k_go_int32_max;
    if( seed < 0 )
      seed += k_go_int32_max;
    if( seed == 0 )
      seed = 89482311;

    auto x = static_cast< int32_t >( seed );
    for( int i = -20; i < k_go_rng_len; ++i )
    {
      x = legacy_go_seedrand( x );
      if( i >= 0 )
      {
        uint64_t u = uint64_t{ static_cast< uint32_t >( x ) } << 40;
        x = legacy_go_seedrand( x );
        u ^= uint64_t{ static_cast< uint32_t >( x ) } << 20;
        x = legacy_go_seedrand( x );
        u ^= uint64_t{ static_cast< uint32_t >( x ) };
        u ^= int64_to_bits( k_go_rng_cooked[ static_cast< std::size_t >( i ) ] );
        vec_[ static_cast< std::size_t >( i ) ] = u;
      }
    }
  }

  uint64_t uint64()
  {
    --tap_;
    if( tap_ < 0 )
      tap_ += k_go_rng_len;

    --feed_;
    if( feed_ < 0 )
      feed_ += k_go_rng_len;

    const auto feed_index = static_cast< std::size_t >( feed_ );
    const auto tap_index = static_cast< std::size_t >( tap_ );
    const uint64_t x = vec_[ feed_index ] + vec_[ tap_index ];
    vec_[ feed_index ] = x;
    return x;
  }

  int64_t int63()
  {
    return bits_to_int64( uint64() & k_go_rng_mask );
  }

  std::vector< uint8_t > read( std::size_t byte_count )
  {
    std::vector< uint8_t > out;
    out.reserve( byte_count );
    int8_t pos = 0;
    int64_t val = 0;
    for( std::size_t i = 0; i < byte_count; ++i )
    {
      if( pos == 0 )
      {
        val = int63();
        pos = 7;
      }
      out.push_back( static_cast< uint8_t >( val & 0xff ) );
      val >>= 8;
      --pos;
    }
    return out;
  }

private:
  int tap_ = 0;
  int feed_ = 0;
  std::array< uint64_t, k_go_rng_len > vec_{};
};

int64_t legacy_seed_to_int64( const std::string& seed )
{
  std::array< unsigned char, SHA256_DIGEST_LENGTH > digest{};
  SHA256( reinterpret_cast< const unsigned char* >( seed.data() ), seed.size(), digest.data() );

  uint64_t value = 0;
  for( std::size_t i = 0; i < 8; ++i )
    value = ( value << 8 ) | digest[ i ];
  return bits_to_int64( value );
}

std::vector< uint8_t > legacy_p256_private_key_der( const std::string& seed )
{
  LegacyGoRand rng( legacy_seed_to_int64( seed ) );
  auto random_bytes = rng.read( 40 );

  std::unique_ptr< EC_KEY, decltype( &EC_KEY_free ) > ec_key(
    EC_KEY_new_by_curve_name( NID_X9_62_prime256v1 ), EC_KEY_free );
  if( !ec_key )
    throw std::runtime_error( "failed to allocate legacy P2P ECDSA key" );
  EC_KEY_set_asn1_flag( ec_key.get(), OPENSSL_EC_NAMED_CURVE );

  const EC_GROUP* group = EC_KEY_get0_group( ec_key.get() );
  std::unique_ptr< BN_CTX, decltype( &BN_CTX_free ) > ctx( BN_CTX_new(), BN_CTX_free );
  std::unique_ptr< BIGNUM, decltype( &BN_free ) > n( BN_new(), BN_free );
  std::unique_ptr< BIGNUM, decltype( &BN_free ) > n_minus_one( BN_new(), BN_free );
  std::unique_ptr< BIGNUM, decltype( &BN_free ) > d( BN_bin2bn( random_bytes.data(), random_bytes.size(), nullptr ), BN_free );
  std::unique_ptr< EC_POINT, decltype( &EC_POINT_free ) > public_point( EC_POINT_new( group ), EC_POINT_free );
  if( !ctx || !n || !n_minus_one || !d || !public_point )
    throw std::runtime_error( "failed to allocate legacy P2P ECDSA key material" );

  if( EC_GROUP_get_order( group, n.get(), ctx.get() ) != 1
      || BN_copy( n_minus_one.get(), n.get() ) == nullptr
      || BN_sub_word( n_minus_one.get(), 1 ) != 1
      || BN_mod( d.get(), d.get(), n_minus_one.get(), ctx.get() ) != 1
      || BN_add_word( d.get(), 1 ) != 1
      || EC_POINT_mul( group, public_point.get(), d.get(), nullptr, nullptr, ctx.get() ) != 1
      || EC_KEY_set_private_key( ec_key.get(), d.get() ) != 1
      || EC_KEY_set_public_key( ec_key.get(), public_point.get() ) != 1 )
    throw std::runtime_error( "failed to derive legacy P2P ECDSA key material" );

  const int size = i2d_ECPrivateKey( ec_key.get(), nullptr );
  if( size <= 0 )
    throw std::runtime_error( "failed to encode legacy P2P ECDSA private key" );

  std::vector< uint8_t > der( static_cast< std::size_t >( size ) );
  auto* out = der.data();
  if( i2d_ECPrivateKey( ec_key.get(), &out ) != size )
    throw std::runtime_error( "failed to serialize legacy P2P ECDSA private key" );

  return der;
}

libp2p::crypto::KeyPair legacy_seed_key_pair( const std::string& seed )
{
  libp2p::crypto::PrivateKey private_key;
  private_key.type = libp2p::crypto::Key::Type::ECDSA;
  private_key.data = legacy_p256_private_key_der( seed );
  return complete_key_pair( private_key );
}

libp2p::crypto::KeyPair load_key_pair_file( const std::filesystem::path& key_file )
{
  auto bytes = read_file_bytes( key_file );
  auto private_key = unmarshal_private_key( bytes );
  if( private_key.type != libp2p::crypto::Key::Type::Ed25519
      && private_key.type != libp2p::crypto::Key::Type::ECDSA )
    throw std::runtime_error( "P2P identity key file must contain an Ed25519 or legacy ECDSA key" );
  return complete_key_pair( private_key );
}

libp2p::crypto::KeyPair load_or_create_key_pair_file( const std::filesystem::path& key_file )
{
  if( std::filesystem::exists( key_file ) )
    return load_key_pair_file( key_file );

  auto key_pair = generate_ed25519_key_pair();
  write_private_key_file( key_file, marshal_private_key( key_pair.privateKey ) );
  return key_pair;
}

std::vector< std::string > advertised_with_peer_id( const std::vector< std::string >& addresses,
                                                    const std::string& peer_id )
{
  std::vector< std::string > result;
  result.reserve( addresses.size() );
  for( const auto& address: addresses )
  {
    auto full = address;
    if( !full.empty() && full.back() != '/' )
      full += "/";
    full += "p2p/";
    full += peer_id;
    result.push_back( std::move( full ) );
  }
  return result;
}

} // namespace

P2PIdentity resolve_p2p_identity( const NodeConfig& cfg )
{
  P2PIdentity identity;

  if( cfg.p2p_identity_seed_configured )
  {
    identity.source = "seed-derived-legacy";
    identity.key_pair = legacy_seed_key_pair( cfg.p2p_identity_seed );
  }
  else
  {
    identity.key_file = cfg.p2p_identity_key_file;
    identity.source = cfg.p2p_identity_key_file_configured ? "configured-key-file" : "generated-key-file";
    identity.key_pair = cfg.p2p_identity_key_file_configured
      ? load_key_pair_file( identity.key_file )
      : load_or_create_key_pair_file( identity.key_file );
  }

  identity.peer_id = peer_id_from_key_pair( identity.key_pair );
  identity.advertised_multiaddrs = advertised_with_peer_id( cfg.p2p_advertised_addresses, identity.peer_id );
  return identity;
}

} // namespace koinos::node::p2p

#endif // KOINOS_HAS_LIBP2P
