#include "koinos/chain/controller.hpp"
#include "koinos/chain/rectify.hpp"
#include "koinos/chain/state.hpp"
#include "koinos/state_db/backends/map/map_backend.hpp"

#include <koinos/crypto/merkle_tree.hpp>
#include <koinos/protocol/protocol.pb.h>
#include <koinos/util/base64.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>

#include <nlohmann/json.hpp>

#include <google/protobuf/util/json_util.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace koinos;

namespace {

const std::filesystem::path fixture_dir{ TELENO_FAST_REPLAY_FIXTURE_DIR };

std::string read_file( const std::filesystem::path& path )
{
  std::ifstream stream( path, std::ios::binary );
  assert( stream );
  return { std::istreambuf_iterator< char >( stream ), std::istreambuf_iterator< char >() };
}

std::string decode_fixture( const std::string& filename )
{
  auto encoded = read_file( fixture_dir / filename );
  encoded.erase( std::remove_if( encoded.begin(), encoded.end(), []( unsigned char ch ) {
                   return ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t';
                 } ),
                 encoded.end() );
  return util::from_base64< std::string >( encoded );
}

protocol::block_receipt load_receipt( const std::string& filename,
                                      std::size_t expected_size,
                                      const std::string& expected_sha256 )
{
  const auto bytes = decode_fixture( filename );
  assert( bytes.size() == expected_size );

  const auto digest = util::converter::as< std::string >(
    crypto::hash( crypto::multicodec::sha2_256, bytes ) );
  assert( util::to_hex( digest ) == "0x1220" + expected_sha256 );

  protocol::block_receipt receipt;
  assert( receipt.ParseFromString( bytes ) );
  assert( receipt.SerializeAsString() == bytes );
  return receipt;
}

protocol::block load_block( const std::string& filename,
                            std::size_t expected_size,
                            const std::string& expected_sha256 )
{
  const auto json = read_file( fixture_dir / filename );
  assert( json.size() == expected_size );
  assert( util::to_hex( crypto::hash( crypto::multicodec::sha2_256, json ) )
          == "0x1220" + expected_sha256 );

  protocol::block block;
  const auto status = google::protobuf::util::JsonStringToMessage( json, &block );
  assert( status.ok() );
  return block;
}

chain::genesis_data load_mainnet_genesis()
{
  const auto repository_root = fixture_dir.parent_path().parent_path().parent_path();
  const auto genesis_json    = read_file( repository_root / "config/example/mainnet/genesis_data.json" );
  chain::genesis_data genesis;
  const auto status = google::protobuf::util::JsonStringToMessage( genesis_json, &genesis );
  assert( status.ok() );
  return genesis;
}

chain::object_space chain_space( const protocol::object_space& space )
{
  chain::object_space result;
  result.set_system( space.system() );
  result.set_zone( space.zone() );
  result.set_id( space.id() );
  return result;
}

std::string delta_merkle_root( const protocol::block_receipt& receipt )
{
  std::vector< std::pair< std::string, std::string > > entries;
  entries.reserve( receipt.state_delta_entries_size() );
  for( const auto& entry: receipt.state_delta_entries() )
  {
    chain::database_key key;
    *key.mutable_space() = chain_space( entry.object_space() );
    key.set_key( entry.key() );
    entries.emplace_back( util::converter::as< std::string >( key ),
                          entry.has_value() ? entry.value() : std::string() );
  }

  std::sort( entries.begin(), entries.end(), []( const auto& lhs, const auto& rhs ) {
    return lhs.first < rhs.first;
  } );

  std::vector< crypto::multihash > leaves;
  leaves.reserve( entries.size() * 2 );
  for( const auto& [ key, value ]: entries )
  {
    leaves.emplace_back( crypto::hash( crypto::multicodec::sha2_256, key ) );
    leaves.emplace_back( crypto::hash( crypto::multicodec::sha2_256, value ) );
  }
  return util::converter::as< std::string >(
    crypto::merkle_tree( crypto::multicodec::sha2_256, leaves ).root()->hash() );
}

void verify_manifest()
{
  const auto manifest = nlohmann::json::parse( read_file( fixture_dir / "manifest.json" ) );
  assert( manifest.at( "format" ) == 1 );
  assert( manifest.at( "network" ) == "Koinos mainnet" );
  assert( manifest.at( "receipts" ).size() == 2 );
  assert( manifest.at( "blocks" ).size() == 4 );
  assert( manifest.at( "normal_neighbors" ).size() == 4 );
  assert( manifest.at( "receipts" ).at( 0 ).at( "height" ) == 30'504'202 );
  assert( manifest.at( "receipts" ).at( 1 ).at( "height" ) == 32'789'377 );
  assert( manifest.at( "blocks" ).at( 0 ).at( "height" ) == 30'504'202 );
  assert( manifest.at( "blocks" ).at( 1 ).at( "height" ) == 30'504'203 );
  assert( manifest.at( "blocks" ).at( 2 ).at( "height" ) == 32'789'377 );
  assert( manifest.at( "blocks" ).at( 3 ).at( "height" ) == 32'789'378 );
}

void verify_incomplete_receipt_fixture()
{
  const auto block_id = util::from_hex< std::string >(
    "0x1220f0ca713b49490ff60f5636e2848f48a7b31c95f583074a30ce7e3cb35d154524" );
  const auto expected_root = util::from_hex< std::string >(
    "0x12209d2d9592ddf831e892a5d4d38e93324f6834e255f84509b5e1c907ccfaa685e6" );

  const auto block = load_block(
    "block-30504202.json",
    768,
    "b8fa5b5fbb356d97cdce683e2908ef362565d673b4d4dc283ab02101d507468c" );
  assert( block.header().height() == 30'504'202 );
  assert( block.id() == block_id );
  assert( block.header().previous()
          == util::from_hex< std::string >(
            "0x12207b71f59f308d5ea2431e4a91a398baf078677ec04815063b82b8422b81a54b36" ) );
  assert( block.header().previous_state_merkle_root()
          == util::from_hex< std::string >(
            "0x1220d70b80a2cb05c287977b5c13f620f8c7227309514386bad9c1e9e58ba3b12e9b" ) );
  assert( block.id()
          == util::converter::as< std::string >(
            crypto::hash( crypto::multicodec::sha2_256, block.header() ) ) );

  const auto successor = load_block(
    "block-30504203.json",
    768,
    "8c4268893525e09ae0d39e3fa8e1b8522169f4eec7eae3612dac0456d8d727fe" );
  assert( successor.header().height() == 30'504'203 );
  assert( successor.id()
          == util::from_hex< std::string >(
            "0x12208f0ab558a8d90c0abfd1075bc602b96658df29d088f075a396e7fcf21735cc5a" ) );
  assert( successor.header().previous() == block_id );
  assert( successor.header().previous_state_merkle_root() == expected_root );
  assert( successor.id()
          == util::converter::as< std::string >(
            crypto::hash( crypto::multicodec::sha2_256, successor.header() ) ) );

  const auto receipt = load_receipt(
    "block-30504202-incomplete-receipt.pb.base64",
    1009,
    "c7013c114379dac717add9e5c350283715aa78ae389d8f113e8d44e0ab77d69c" );
  assert( receipt.state_delta_entries_size() == 8 );
  assert( std::all_of( receipt.state_delta_entries().begin(),
                       receipt.state_delta_entries().end(),
                       []( const auto& entry ) { return entry.has_value(); } ) );

  const auto incomplete_root = delta_merkle_root( receipt );
  assert( util::to_hex( incomplete_root )
          == "0x12207fb526273e706238cef899350facfd1ddcfa5e19ae352284be53e68d5c516f45" );
  assert( incomplete_root != expected_root );
  assert( !chain::acceptable_rectified_previous_root( block_id, incomplete_root, expected_root ) );
}

void verify_consensus_scar_fixture()
{
  const auto block_id = util::from_hex< std::string >(
    "0x1220a97d7b0567ad55e3b04446a2bef447335cfd676668b069544b04a4719146d586" );
  const auto honest_root = util::from_hex< std::string >(
    "0x12203a22d59290a838dd49c87f57fe80319636950948f6b9aaf02287c03bb36e5f68" );
  const auto signed_root = util::from_hex< std::string >(
    "0x12209948b54dee01acd8528cf15dec02366b76e7739aedaf4487859bf6d0d182d690" );

  const auto block = load_block(
    "block-32789377.json",
    683,
    "7ee7f986a89103fe9a753b9ae6fbe233b4d628883766b1e7e943b403c09999fb" );
  assert( block.header().height() == 32'789'377 );
  assert( block.id() == block_id );
  assert( block.header().previous()
          == util::from_hex< std::string >(
            "0x122044622d4cade292dfbf1cc24ba069cd385107997e03fbc7475c5d89069a03a661" ) );
  assert( block.header().previous_state_merkle_root()
          == util::from_hex< std::string >(
            "0x1220e4a6e321d41acff12f3d26fee110c53d92a9efb331547d1fd0d300c6c5d02dac" ) );
  assert( block.id()
          == util::converter::as< std::string >(
            crypto::hash( crypto::multicodec::sha2_256, block.header() ) ) );

  const auto successor = load_block(
    "block-32789378.json",
    683,
    "8e4086a9d37cadad6964a96d0c1653225d99de54b6d72e3b600c7807f5df189a" );
  assert( successor.header().height() == 32'789'378 );
  assert( successor.id()
          == util::from_hex< std::string >(
            "0x122086d9090d82fceb9900293bd3f870c4d2ac769682a85f997fd847f4f716a96344" ) );
  assert( successor.header().previous() == block_id );
  assert( successor.header().previous_state_merkle_root() == signed_root );
  assert( successor.id()
          == util::converter::as< std::string >(
            crypto::hash( crypto::multicodec::sha2_256, successor.header() ) ) );

  auto receipt = load_receipt(
    "block-32789377-honest-receipt.pb.base64",
    2242,
    "2a4ad905226bcc119926c53e6521fed671cd10b1c54550ad90ee4c6e487a348a" );
  assert( receipt.state_delta_entries_size() == 12 );
  assert( std::count_if( receipt.state_delta_entries().begin(),
                         receipt.state_delta_entries().end(),
                         []( const auto& entry ) { return entry.has_value(); } ) == 10 );
  assert( delta_merkle_root( receipt ) == honest_root );
  assert( chain::acceptable_rectified_previous_root( block_id, honest_root, signed_root ) );

  const std::string phantom_key = "02076430234253060999996";
  bool removed = false;
  for( int i = 0; i < receipt.state_delta_entries_size(); ++i )
  {
    const auto& entry = receipt.state_delta_entries( i );
    if( !entry.has_value() && entry.key() == phantom_key )
    {
      receipt.mutable_state_delta_entries()->DeleteSubrange( i, 1 );
      removed = true;
      break;
    }
  }
  assert( removed );
  assert( receipt.state_delta_entries_size() == 11 );
  assert( delta_merkle_root( receipt ) == signed_root );
}

void verify_controller_accepts_the_exact_consensus_scar_without_fallback()
{
  const auto block_id = util::from_hex< std::string >(
    "0x1220a97d7b0567ad55e3b04446a2bef447335cfd676668b069544b04a4719146d586" );
  const auto honest_root = util::from_hex< std::string >(
    "0x12203a22d59290a838dd49c87f57fe80319636950948f6b9aaf02287c03bb36e5f68" );
  const auto signed_root = util::from_hex< std::string >(
    "0x12209948b54dee01acd8528cf15dec02366b76e7739aedaf4487859bf6d0d182d690" );
  const auto receipt = load_receipt(
    "block-32789377-honest-receipt.pb.base64",
    2242,
    "2a4ad905226bcc119926c53e6521fed671cd10b1c54550ad90ee4c6e487a348a" );

  chain::controller controller;
  controller.open(
    std::make_shared< state_db::backends::map::map_backend >(),
    load_mainnet_genesis(),
    chain::fork_resolution_algorithm::pob,
    false );

  protocol::block block;
  block.set_id( block_id );
  block.mutable_header()->set_height( 1 );
  block.mutable_header()->set_previous(
    util::converter::as< std::string >(
      crypto::multihash::zero( crypto::multicodec::sha2_256 ) ) );
  block.mutable_header()->set_previous_state_merkle_root(
    controller.get_head_info().head_state_merkle_root() );

  assert( !controller.apply_block_delta_checked( block, receipt, signed_root, 1 ) );
  assert( controller.get_head_info().head_topology().id() == block_id );
  assert( controller.get_head_info().head_state_merkle_root() == honest_root );
}

void verify_normal_neighbors_do_not_expand_exception()
{
  const std::vector< std::pair< std::string, std::string > > neighbors = {
    { "0x12207b71f59f308d5ea2431e4a91a398baf078677ec04815063b82b8422b81a54b36",
      "0x1220d70b80a2cb05c287977b5c13f620f8c7227309514386bad9c1e9e58ba3b12e9b" },
    { "0x12208f0ab558a8d90c0abfd1075bc602b96658df29d088f075a396e7fcf21735cc5a",
      "0x1220f73169eb40a9977cefbf0e64e95a07f12a1a4050bed0e765ed84e63d9606f088" },
    { "0x122044622d4cade292dfbf1cc24ba069cd385107997e03fbc7475c5d89069a03a661",
      "0x1220e4a6e321d41acff12f3d26fee110c53d92a9efb331547d1fd0d300c6c5d02dac" },
    { "0x122086d9090d82fceb9900293bd3f870c4d2ac769682a85f997fd847f4f716a96344",
      "0x1220fcbb38c16543c8c9b3ceb3a06b5ddf944d8574262df11f0e6dd8c9a53c68bdb2" }
  };

  for( const auto& [ id, root ]: neighbors )
  {
    const auto id_bytes = util::from_hex< std::string >( id );
    const auto root_bytes = util::from_hex< std::string >( root );
    assert( !chain::acceptable_rectified_previous_root( id_bytes, root_bytes, root_bytes ) );

    auto changed = root_bytes;
    changed.back() ^= 1;
    assert( !chain::acceptable_rectified_previous_root( id_bytes, root_bytes, changed ) );
  }
}

} // namespace

int main()
{
  verify_manifest();
  verify_incomplete_receipt_fixture();
  verify_consensus_scar_fixture();
  verify_controller_accepts_the_exact_consensus_scar_without_fallback();
  verify_normal_neighbors_do_not_expand_exception();
  return 0;
}
