#include "block_production/block_producer.hpp"

#include <koinos/bigint.hpp>
#include <koinos/chain/system_call_ids.pb.h>
#include <koinos/contracts/name_service/name_service.pb.h>
#include <koinos/contracts/pob/pob.pb.h>
#include <koinos/contracts/token/token.pb.h>
#include <koinos/contracts/vhp/vhp.pb.h>
#include <koinos/crypto/merkle_tree.hpp>
#include <koinos/log.hpp>
#include <koinos/rpc/chain/chain_rpc.pb.h>
#include <koinos/rpc/mempool/mempool_rpc.pb.h>
#include <koinos/util/base58.hpp>
#include <koinos/util/base64.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>
#include <koinos/util/random.hpp>

#include <boost/multiprecision/cpp_int.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace koinos::node::block_production {

namespace {

using boost::multiprecision::uint128_t;
using boost::multiprecision::uint256_t;

constexpr uint32_t get_metadata_entry_point             = 0xfcf7a68f;
constexpr uint32_t get_consensus_parameters_entry_point = 0x5fd7ac0f;
constexpr uint32_t effective_balance_of_entry_point     = 0x629f31e6;
constexpr uint32_t decimals_entry_point                 = 0xee80fd2f;
constexpr uint32_t symbol_entry_point                   = 0xb76a7ca1;

uint64_t to_ms( std::chrono::system_clock::time_point time )
{
  return uint64_t( std::chrono::duration_cast< std::chrono::milliseconds >( time.time_since_epoch() ).count() );
}

std::chrono::system_clock::time_point from_ms( uint64_t milliseconds )
{
  return std::chrono::system_clock::time_point{ std::chrono::milliseconds{ milliseconds } };
}

template< typename Integer >
Integer decode_big_endian_unsigned( const std::string& bytes )
{
  Integer value = 0;
  for( auto byte: bytes )
  {
    value <<= 8;
    value |= static_cast< unsigned char >( byte );
  }

  return value;
}

template< typename Integer >
Integer decode_big_endian_unsigned( const crypto::digest_type& bytes )
{
  Integer value = 0;
  for( auto byte: bytes )
  {
    value <<= 8;
    value |= std::to_integer< unsigned int >( byte );
  }

  return value;
}

} // anonymous namespace

crypto::private_key load_private_key_file( const std::filesystem::path& path )
{
  std::ifstream input( path );
  if( !input.is_open() )
    throw std::runtime_error( "unable to open private key file: " + path.string() );

  std::string private_key_wif;
  std::getline( input, private_key_wif );
  if( private_key_wif.empty() )
    throw std::runtime_error( "private key file is empty: " + path.string() );

  return crypto::private_key::from_wif( private_key_wif );
}

namespace {

void restrict_private_key_permissions( const std::filesystem::path& path )
{
  std::error_code ec;
  std::filesystem::permissions(
    path,
    std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
    std::filesystem::perm_options::replace,
    ec );
  if( ec )
    LOG( warning ) << "[block_producer] Unable to restrict private key permissions at "
                   << path << ": " << ec.message();
}

} // anonymous namespace

crypto::private_key load_or_create_private_key_file( const std::filesystem::path& path )
{
  if( std::filesystem::exists( path ) )
    return load_private_key_file( path );

  LOG( info ) << "[block_producer] Private key not found at " << path
              << ", generating a new producer hot key";

  const auto parent_path = path.parent_path();
  if( !parent_path.empty() )
    std::filesystem::create_directories( parent_path );

  auto seed        = koinos::util::random_alphanumeric( 64 );
  auto secret      = koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, seed );
  auto private_key = koinos::crypto::private_key::regenerate( secret );

  std::ofstream output( path, std::ios::out | std::ios::trunc );
  if( !output.is_open() )
    throw std::runtime_error( "unable to write private key file: " + path.string() );

  restrict_private_key_permissions( path );
  output << private_key.to_wif() << '\n';
  output.close();
  if( !output )
    throw std::runtime_error( "unable to finish writing private key file: " + path.string() );

  restrict_private_key_permissions( path );
  return private_key;
}

BlockProducer::BlockProducer( IChain& chain,
                              IMempool& mempool,
                              crypto::private_key signing_key,
                              ProducerConfig config ):
    _chain( chain ),
    _mempool( mempool ),
    _signing_key( std::move( signing_key ) ),
    _config( std::move( config ) )
{
  if( _config.resources_lower_bound > 100 )
    throw std::invalid_argument( "block_producer.resources-lower-bound must be in [0..100]" );
  if( _config.resources_upper_bound > 100 )
    throw std::invalid_argument( "block_producer.resources-upper-bound must be in [0..100]" );
  if( _config.resources_lower_bound > _config.resources_upper_bound )
    throw std::invalid_argument( "block_producer.resources-lower-bound must not exceed resources-upper-bound" );
  if( _config.max_inclusion_attempts == 0 )
    _config.max_inclusion_attempts = 2'000;
}

std::string BlockProducer::public_address() const
{
  return util::to_base58( _signing_key.get_public_key().to_address_bytes() );
}

std::string BlockProducer::public_key_base64() const
{
  return util::to_base64( _signing_key.get_public_key().serialize() );
}

void BlockProducer::write_public_key_file( const std::filesystem::path& path ) const
{
  std::filesystem::create_directories( path.parent_path() );
  std::ofstream output( path );
  if( !output.is_open() )
    throw std::runtime_error( "unable to write public key file: " + path.string() );
  output << public_key_base64() << '\n';
}

ProductionResult BlockProducer::produce_once()
{
  if( _config.algorithm == "pob" )
    return produce_pob_once();
  if( _config.algorithm == "federated" )
    return produce_federated_once();

  throw std::invalid_argument( "unsupported block_producer.algorithm: " + _config.algorithm );
}

ProductionResult BlockProducer::produce_federated_once()
{
  ProductionResult result;
  result.retry_after = std::chrono::seconds( 5 );

  auto block = next_block( _signing_key.get_public_key().to_address_bytes() );
  block.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, block.header() ) ) );
  block.set_signature( util::converter::as< std::string >(
    _signing_key.sign_compact( util::converter::to< crypto::multihash >( block.id() ) ) ) );

  while( submit_block( block, result ) )
  {
    block.set_id( util::converter::as< std::string >( crypto::hash( crypto::multicodec::sha2_256, block.header() ) ) );
    block.set_signature( util::converter::as< std::string >(
      _signing_key.sign_compact( util::converter::to< crypto::multihash >( block.id() ) ) ) );
  }

  if( result.status == ProductionResult::Status::produced )
    result.retry_after = std::chrono::seconds( 5 );

  return result;
}

ProductionResult BlockProducer::produce_pob_once()
{
  if( _config.producer_address.empty() )
    throw std::invalid_argument( "block_producer.producer is required when algorithm is pob" );

  if( !_pob_auxiliary_data )
    refresh_pob_auxiliary_data();

  if( !_pob_bundle )
    _pob_bundle = next_pob_bundle();
  else
  {
    const auto head = _chain.get_head_info();
    if( _pob_bundle->block.header().previous() != head.head_topology().id() )
      _pob_bundle = next_pob_bundle();
  }

  ProductionResult result;
  const auto quantum_length = std::chrono::milliseconds{ _pob_auxiliary_data->quantum_length };
  result.retry_after        = quantum_length.count() > 0 ? quantum_length : std::chrono::milliseconds( 10 );

  if( _pob_bundle->time_quantum > std::chrono::system_clock::now() + std::chrono::seconds( 5 ) )
  {
    result.status = ProductionResult::Status::not_our_turn;
    return result;
  }

  const auto timestamp = to_ms( _pob_bundle->time_quantum );
  _pob_bundle->block.mutable_header()->set_timestamp( timestamp );

  contracts::pob::vrf_payload payload;
  payload.set_seed( _pob_bundle->seed );
  payload.set_block_time( timestamp );

  auto [ proof, proof_hash ] = _signing_key.generate_random_proof( util::converter::as< std::string >( payload ) );

  _pob_bundle->block.set_id( util::converter::as< std::string >(
    crypto::hash( crypto::multicodec::sha2_256, _pob_bundle->block.header() ) ) );

  contracts::pob::signature_data signature_data;
  signature_data.set_vrf_hash( util::converter::as< std::string >( proof_hash ) );
  signature_data.set_vrf_proof( proof );
  signature_data.set_signature( util::converter::as< std::string >(
    _signing_key.sign_compact( util::converter::to< crypto::multihash >( _pob_bundle->block.id() ) ) ) );

  _pob_bundle->block.set_signature( util::converter::as< std::string >( signature_data ) );

  if( !difficulty_met( proof_hash, _pob_bundle->vhp_balance, _pob_bundle->difficulty ) )
  {
    _pob_bundle->time_quantum = next_time_quantum( _pob_bundle->time_quantum );
    result.status            = ProductionResult::Status::not_our_turn;
    return result;
  }

  while( submit_block( _pob_bundle->block, result ) )
  {
    _pob_bundle->block.set_id( util::converter::as< std::string >(
      crypto::hash( crypto::multicodec::sha2_256, _pob_bundle->block.header() ) ) );
    signature_data.set_signature( util::converter::as< std::string >(
      _signing_key.sign_compact( util::converter::to< crypto::multihash >( _pob_bundle->block.id() ) ) ) );
    _pob_bundle->block.set_signature( util::converter::as< std::string >( signature_data ) );
  }

  _pob_bundle.reset();
  return result;
}

protocol::block BlockProducer::next_block( const std::string& signer )
{
  auto head_info = _chain.get_head_info();

  protocol::block block;
  block.mutable_header()->set_previous( head_info.head_topology().id() );
  block.mutable_header()->set_height( head_info.head_topology().height() + 1 );
  block.mutable_header()->set_timestamp( std::max( now_ms(), head_info.head_block_time() ) );
  block.mutable_header()->set_previous_state_merkle_root( head_info.head_state_merkle_root() );
  block.mutable_header()->set_signer( signer );
  block.mutable_header()->mutable_approved_proposals()->Add(
    _config.approved_proposals.begin(), _config.approved_proposals.end() );

  fill_block( block );
  set_merkle_roots( block );
  return block;
}

void BlockProducer::fill_block( protocol::block& block )
{
  rpc::mempool::get_pending_transactions_request pending_req;
  pending_req.set_limit( _config.max_inclusion_attempts );
  pending_req.set_block_id( block.header().previous() );
  auto pending_resp = _mempool.get_pending_transactions( pending_req );

  auto resource_resp = _chain.get_resource_limits( rpc::chain::get_resource_limits_request() );
  const auto& limits = resource_resp.resource_limit_data();

  uint64_t disk_storage_count      = 0;
  uint64_t network_bandwidth_count = 0;
  uint64_t compute_bandwidth_count = 0;

  for( int i = 0; i < pending_resp.pending_transactions_size(); ++i )
  {
    if( static_cast< uint64_t >( i ) >= _config.max_inclusion_attempts )
      break;

    if( disk_storage_count >= limits.disk_storage_limit() * _config.resources_lower_bound / 100 )
      break;
    if( network_bandwidth_count >= limits.network_bandwidth_limit() * _config.resources_lower_bound / 100 )
      break;
    if( compute_bandwidth_count >= limits.compute_bandwidth_limit() * _config.resources_lower_bound / 100 )
      break;

    const auto& pending_transaction = pending_resp.pending_transactions( i );
    const auto& transaction         = pending_transaction.transaction();
    if( transaction.header().rc_limit() == 0 )
      continue;

    auto new_disk_storage_count =
      pending_transaction.disk_storage_used() + pending_transaction.system_disk_storage_used() + disk_storage_count;
    auto new_network_bandwidth_count = pending_transaction.network_bandwidth_used()
                                       + pending_transaction.system_network_bandwidth_used()
                                       + network_bandwidth_count;
    auto new_compute_bandwidth_count = pending_transaction.compute_bandwidth_used()
                                       + pending_transaction.system_compute_bandwidth_used()
                                       + compute_bandwidth_count;

    bool disk_storage_within_bounds =
      new_disk_storage_count <= limits.disk_storage_limit() * _config.resources_upper_bound / 100;
    bool network_bandwidth_within_bounds =
      new_network_bandwidth_count <= limits.network_bandwidth_limit() * _config.resources_upper_bound / 100;
    bool compute_bandwidth_within_bounds =
      new_compute_bandwidth_count <= limits.compute_bandwidth_limit() * _config.resources_upper_bound / 100;

    if( disk_storage_within_bounds && network_bandwidth_within_bounds && compute_bandwidth_within_bounds )
    {
      *block.add_transactions()   = transaction;
      disk_storage_count          = new_disk_storage_count;
      network_bandwidth_count     = new_network_bandwidth_count;
      compute_bandwidth_count     = new_compute_bandwidth_count;
    }
  }

  LOG( debug ) << "[block_producer] Created block containing " << block.transactions_size()
               << ( block.transactions_size() == 1 ? " transaction" : " transactions" );
}

void BlockProducer::set_merkle_roots( protocol::block& block ) const
{
  std::vector< crypto::multihash > hashes;
  hashes.reserve( block.transactions().size() * 2 );

  for( const auto& transaction: block.transactions() )
  {
    hashes.emplace_back( crypto::hash( crypto::multicodec::sha2_256, transaction.header() ) );
    hashes.emplace_back( crypto::hash( crypto::multicodec::sha2_256, transaction.signatures() ) );
  }

  auto transaction_merkle_tree = crypto::merkle_tree( crypto::multicodec::sha2_256, hashes );
  block.mutable_header()->set_transaction_merkle_root(
    util::converter::as< std::string >( transaction_merkle_tree.root()->hash() ) );
}

bool BlockProducer::submit_block( protocol::block& block, ProductionResult& result )
{
  rpc::chain::propose_block_request req;
  req.mutable_block()->CopyFrom( block );

  auto resp = _chain.propose_block( req );
  if( !resp.has_receipt() )
  {
    const auto& failed_indices = resp.failed_transaction_indices();
    if( failed_indices.empty() )
    {
      result.status      = ProductionResult::Status::not_ready;
      result.retry_after = std::chrono::seconds( 5 );
      return false;
    }

    auto* transactions = block.mutable_transactions();
    for( auto it = failed_indices.rbegin(); it != failed_indices.rend(); ++it )
      transactions->DeleteSubrange( *it, 1 );

    result.removed_failed_transactions += failed_indices.size();
    set_merkle_roots( block );
    return true;
  }

  const auto& receipt      = resp.receipt();
  result.status            = ProductionResult::Status::produced;
  result.height            = receipt.height();
  result.id                = receipt.id();
  result.transaction_count = receipt.transaction_receipts_size();
  result.retry_after       = std::chrono::milliseconds( 0 );

  LOG( info ) << "[block_producer] Produced block - Height: " << result.height
              << ", ID: " << util::to_hex( result.id ) << " (" << result.transaction_count
              << ( result.transaction_count == 1 ? " transaction)" : " transactions)" );

  return false;
}

std::chrono::system_clock::time_point
BlockProducer::next_time_quantum( std::chrono::system_clock::time_point time ) const
{
  auto time_ms = std::max( to_ms( time ), now_ms() );
  auto step    = uint64_t( 10 );
  if( _pob_auxiliary_data && _pob_auxiliary_data->quantum_length > 0 )
    step = _pob_auxiliary_data->quantum_length;

  auto remainder = time_ms % step;
  if( remainder )
    time_ms += step - remainder;
  else
    time_ms += step;

  return from_ms( time_ms );
}

uint64_t BlockProducer::now_ms() const
{
  return to_ms( std::chrono::system_clock::now() );
}

void BlockProducer::refresh_pob_auxiliary_data()
{
  PobAuxiliaryData data;
  data.pob_contract_id = get_contract_address( "pob" );
  data.vhp_contract_id = get_contract_address( "vhp" );

  _pob_auxiliary_data = data;

  contracts::pob::get_consensus_parameters_result params_result;
  rpc::chain::read_contract_request params_req;
  params_req.set_contract_id( _pob_auxiliary_data->pob_contract_id );
  params_req.set_entry_point( get_consensus_parameters_entry_point );

  auto params_resp = _chain.read_contract( params_req );
  if( !params_result.ParseFromString( params_resp.result() ) )
    throw std::runtime_error( "unable to parse PoB consensus parameters" );

  auto vhp_decimals = get_vhp_decimals();
  if( vhp_decimals == 0 || vhp_decimals >= 10 )
    throw std::runtime_error( "invalid VHP decimals from chain" );

  static uint32_t pow10[] =
    { 1, 10, 100, 1'000, 10'000, 100'000, 1'000'000, 10'000'000, 100'000'000, 1'000'000'000 };

  auto consensus_params = params_result.value();
  if( consensus_params.target_block_interval() == 0 || consensus_params.quantum_length() == 0 )
    throw std::runtime_error( "invalid PoB consensus timing from chain" );

  _pob_auxiliary_data->vhp_symbol            = get_vhp_symbol();
  _pob_auxiliary_data->vhp_precision         = pow10[ vhp_decimals ];
  _pob_auxiliary_data->target_block_interval = consensus_params.target_block_interval();
  _pob_auxiliary_data->quantum_length        = consensus_params.quantum_length();
  _pob_auxiliary_data->quanta_per_block_interval =
    consensus_params.target_block_interval() / consensus_params.quantum_length();

  if( _pob_auxiliary_data->quanta_per_block_interval == 0 )
    throw std::runtime_error( "invalid PoB quanta per block interval from chain" );

  LOG( info ) << "[block_producer] PoB contract address: " << util::to_base58( _pob_auxiliary_data->pob_contract_id );
  LOG( info ) << "[block_producer] VHP contract address: " << util::to_base58( _pob_auxiliary_data->vhp_contract_id );
  LOG( info ) << "[block_producer] Target block interval: " << _pob_auxiliary_data->target_block_interval << "ms";
  LOG( info ) << "[block_producer] Quantum length: " << _pob_auxiliary_data->quantum_length << "ms";
}

BlockProducer::PobBundle BlockProducer::next_pob_bundle()
{
  refresh_pob_auxiliary_data();

  PobBundle bundle;
  auto metadata       = get_pob_metadata();
  bundle.block        = next_block( util::from_base58< std::string >( _config.producer_address ) );
  bundle.seed         = metadata.seed();
  bundle.difficulty   = metadata.difficulty();
  bundle.vhp_balance  = get_vhp_balance();
  bundle.time_quantum = next_time_quantum( from_ms( bundle.block.header().timestamp() ) );

  if( bundle.vhp_balance == 0 )
    throw std::runtime_error( "configured producer has no effective VHP balance" );

  auto difficulty = decode_big_endian_unsigned< uint128_t >( bundle.difficulty );
  if( difficulty == 0 )
    throw std::runtime_error( "PoB difficulty is zero" );

  return bundle;
}

bool BlockProducer::difficulty_met( const crypto::multihash& proof_hash,
                                    uint64_t vhp_balance,
                                    const std::string& difficulty ) const
{
  if( vhp_balance == 0 )
    return false;

  auto difficulty_value = decode_big_endian_unsigned< uint128_t >( difficulty );
  if( difficulty_value == 0 )
    return false;

  auto target      = std::numeric_limits< uint128_t >::max() / difficulty_value;
  auto proof_value = decode_big_endian_unsigned< uint256_t >( proof_hash.digest() );
  return ( proof_value >> 128 ) / vhp_balance < target;
}

std::string BlockProducer::get_contract_address( const std::string& name )
{
  contracts::name_service::get_address_arguments args;
  args.set_name( name );

  rpc::chain::invoke_system_call_request req;
  req.set_id( koinos::chain::get_contract_address );
  req.set_args( args.SerializeAsString() );

  auto resp = _chain.invoke_system_call( req );

  contracts::name_service::get_address_result result;
  if( !result.ParseFromString( resp.value() ) )
    throw std::runtime_error( "unable to parse contract address for " + name );

  return result.value().address();
}

std::string BlockProducer::get_vhp_symbol()
{
  rpc::chain::read_contract_request req;
  req.set_contract_id( _pob_auxiliary_data->vhp_contract_id );
  req.set_entry_point( symbol_entry_point );

  auto resp = _chain.read_contract( req );

  contracts::token::symbol_result result;
  if( !result.ParseFromString( resp.result() ) )
    throw std::runtime_error( "unable to parse VHP symbol" );

  return result.value();
}

uint32_t BlockProducer::get_vhp_decimals()
{
  rpc::chain::read_contract_request req;
  req.set_contract_id( _pob_auxiliary_data->vhp_contract_id );
  req.set_entry_point( decimals_entry_point );

  auto resp = _chain.read_contract( req );

  contracts::token::decimals_result result;
  if( !result.ParseFromString( resp.result() ) )
    throw std::runtime_error( "unable to parse VHP decimals" );

  return result.value();
}

uint64_t BlockProducer::get_vhp_balance()
{
  contracts::vhp::effective_balance_of_arguments args;
  args.set_owner( util::from_base58< std::string >( _config.producer_address ) );

  rpc::chain::read_contract_request req;
  req.set_contract_id( _pob_auxiliary_data->vhp_contract_id );
  req.set_entry_point( effective_balance_of_entry_point );
  req.set_args( args.SerializeAsString() );

  auto resp = _chain.read_contract( req );

  contracts::vhp::effective_balance_of_result result;
  if( !result.ParseFromString( resp.result() ) )
    throw std::runtime_error( "unable to parse VHP balance" );

  return result.value();
}

contracts::pob::metadata BlockProducer::get_pob_metadata()
{
  rpc::chain::read_contract_request req;
  req.set_contract_id( _pob_auxiliary_data->pob_contract_id );
  req.set_entry_point( get_metadata_entry_point );

  auto resp = _chain.read_contract( req );

  contracts::pob::get_metadata_result result;
  if( !result.ParseFromString( resp.result() ) )
    throw std::runtime_error( "unable to parse PoB metadata" );

  return result.value();
}

} // namespace koinos::node::block_production
