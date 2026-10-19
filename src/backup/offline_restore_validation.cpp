#include "backup/offline_restore_validation.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>
#include <google/protobuf/util/json_util.h>
#include <koinos/block_store/block_store.pb.h>
#include <koinos/chain/chain.pb.h>
#include <koinos/chain/object_spaces.pb.h>
#include <koinos/crypto/multihash.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>
#include <nlohmann/json.hpp>
#include <rocksdb/db.h>

namespace koinos::node::backup {
namespace {
void require( bool b, const std::string& message )
{ if( !b ) throw std::runtime_error( "offline restore validation: " + message ); }
void checked( const rocksdb::Status& s ) { require( s.ok(), s.ToString() ); }
struct Database
{
  rocksdb::DB* db = nullptr;
  std::vector<rocksdb::ColumnFamilyHandle*> handles;
  ~Database() { for( auto* h: handles ) delete h; delete db; }
  void close_checked()
  {
    for(auto*& h: handles) { const auto s = db->DestroyColumnFamilyHandle(h); h = nullptr; checked(s); }
    handles.clear(); checked(db->Close()); delete db; db = nullptr;
  }
};
std::string metadata_key( const std::string& name )
{
  chain::database_key key;
  key.mutable_space()->set_system( true );
  key.mutable_space()->set_id( chain::system_space_id::metadata );
  key.set_key( util::converter::as<std::string>( crypto::hash( crypto::multicodec::sha2_256, "object_key::" + name ) ) );
  return util::converter::as<std::string>( key );
}
}

std::string validate_offline_restore_database( const RestoreSpecification& spec,
  const std::filesystem::path& payload, const std::function<void()>& cancellation_check )
{
  require( spec.data().expected_chain_checked, "an independently selected expected chain ID is required" );
  const auto genesis_path = payload / "chain/genesis_data.json";
  require( std::filesystem::file_size( genesis_path ) <= 32ULL * 1024 * 1024, "genesis input exceeds 32 MiB validation limit" );
  std::ifstream genesis_stream( genesis_path, std::ios::binary );
  require( bool( genesis_stream ), "cannot read genesis" );
  std::string bytes{ std::istreambuf_iterator<char>( genesis_stream ), {} };
  require( !genesis_stream.bad(), "genesis read failed" );
  chain::genesis_data genesis;
  require( google::protobuf::util::JsonStringToMessage( bytes, &genesis ).ok()
    && genesis.entries_size() > 0, "invalid/empty genesis protobuf" );
  const auto chain_id = util::converter::as<std::string>( crypto::hash( crypto::multicodec::sha2_256, genesis ) );
  require( util::to_hex( chain_id ) == spec.data().chain_id, "genesis does not match the pinned expected chain ID (hex)" );

  uint64_t wal_bytes = 0;
  for(const auto& file: spec.data().files) if(file.path.rfind("db/",0) == 0 && std::filesystem::path(file.path).extension() == ".log")
  {
    require(file.size_bytes <= 128ULL * 1024 * 1024 - wal_bytes,"read-only WAL recovery exceeds the 128 MiB input guard");
    wal_bytes += file.size_bytes;
  }

  std::vector<std::string> names;
  rocksdb::Options options;
  options.create_if_missing = false; options.create_missing_column_families = false;
  options.paranoid_checks = true; options.max_open_files = 64;
  checked( rocksdb::DB::ListColumnFamilies( options, ( payload / "db" ).string(), &names ) );
  const std::vector<std::string> expected = { "default", "blocks", "block_meta", "contract_meta",
    "transaction_index", "account_history", "chain_state", "chain_metadata", "storage_metadata" };
  auto sorted = names, expected_sorted = expected;
  std::sort( sorted.begin(), sorted.end() ); std::sort( expected_sorted.begin(), expected_sorted.end() );
  require( sorted == expected_sorted, "unsupported or incomplete unified column-family layout" );
  std::vector<rocksdb::ColumnFamilyDescriptor> descriptors;
  for( const auto& name: names ) descriptors.emplace_back( name, rocksdb::ColumnFamilyOptions{} );
  Database database;
  checked( rocksdb::DB::OpenForReadOnly( options, ( payload / "db" ).string(), descriptors,
    &database.handles, &database.db, false ) ); // WAL replay in memory; never writable recovery.
  auto handle = [&]( const std::string& name ) {
    return database.handles.at( std::distance( names.begin(), std::find( names.begin(), names.end(), name ) ) );
  };
  rocksdb::ReadOptions reads; reads.verify_checksums = true; reads.fill_cache = false;
  auto get = [&]( const std::string& cf, const std::string& key ) {
    std::string value; checked( database.db->Get( reads, handle( cf ), key, &value ) ); return value;
  };
  require( get( "storage_metadata", "layout.version" ) == "1"
    && get( "storage_metadata", "layout.chain_storage" ) == "unified", "unified storage metadata mismatch" );
  require( get( "chain_state", metadata_key( "chain_id" ) ) == chain_id, "state chain ID differs from genesis" );
  require( !get( "chain_state", metadata_key( "genesis_key" ) ).empty(), "missing genesis public key" );
  const auto revision_bytes = get( "chain_metadata", "revision" );
  const auto size_bytes = get( "chain_metadata", "size" );
  require( revision_bytes.size() == sizeof(uint64_t) && size_bytes.size() == sizeof(uint64_t), "invalid numeric state metadata" );
  const auto revision = util::converter::to<uint64_t>( revision_bytes );
  const auto state_size = util::converter::to<uint64_t>( size_bytes );
  const auto id = get( "chain_metadata", "id" ), root = get( "chain_metadata", "merkle_root" );
  require( id.size() == 34 && root.size() == 34 && root[0] == '\x12' && root[1] == '\x20', "invalid persisted state identity/root" );
  protocol::block_header state_header;
  require( state_header.ParseFromString( get( "chain_metadata", "block_header" ) ) && state_header.height() == revision,
    "state revision/header mismatch" );
  block_topology highest;
  require( highest.ParseFromString( get( "block_meta", std::string(1, '\x01') ) )
    && highest.id().size() == 34 && highest.height() >= revision, "block-store topology/state mismatch" );

  // Validate every readable key/value and iterator status across all CFs. This
  // bypasses the runtime backend's error-to-missing/unchecked flush paths (WP1).
  uint64_t records = 0, state_records = 0;
  for( const auto& name: names )
  {
    std::unique_ptr<rocksdb::Iterator> it( database.db->NewIterator( reads, handle(name) ) );
    for( it->SeekToFirst(); it->Valid(); it->Next() )
    {
      if( cancellation_check ) cancellation_check();
      ++records;
      if( name == "chain_state" )
      {
        require(it->key().size() <= 1024 * 1024, "state key exceeds offline parsing limit");
        ++state_records; chain::database_key key;
        require( key.ParseFromString( it->key().ToString() ) && key.has_space(), "invalid state object key" );
      }
      if( name == "blocks" )
      {
        require(it->value().size() <= 64ULL * 1024 * 1024, "block record exceeds offline parsing limit");
        block_store::block_record record;
        require( record.ParseFromString( it->value().ToString() ) && record.has_block() && record.has_receipt()
          && record.block_id() == it->key().ToString() && record.block().id() == record.block_id()
          && record.block_height() == record.block().header().height()
          && record.receipt().id() == record.block_id(), "incomplete/inconsistent block record" );
      }
    }
    checked( it->status() );
  }
  checked(database.db->VerifyChecksum(reads));
  require( state_records == state_size, "persisted state size differs from actual objects" );
  auto current = highest.id(); auto height = highest.height();
  while( height > 0 )
  {
    if( cancellation_check ) cancellation_check();
    block_store::block_record record;
    require( record.ParseFromString( get("blocks", current) ) && record.block_height() == height,
      "missing canonical ancestry record" );
    if( height == revision )
    {
      require( current == id && record.receipt().state_merkle_root() == root
        && record.block().header().SerializeAsString() == state_header.SerializeAsString(),
        "persisted state does not match its block/receipt checkpoint" );
      protocol::block state_block;
      require(state_block.ParseFromString(get("chain_state",metadata_key("head_block")))
        && state_block.SerializeAsString() == record.block().SerializeAsString(),"state head object differs from checkpoint block");
      break;
    }
    require( record.previous_block_ids_size() > 0
      && record.previous_block_ids(0) == record.block().header().previous(), "broken block ancestry" );
    current = record.previous_block_ids(0); --height;
    if( height == 0 ) break;
  }
  if( revision == 0 )
  {
    const auto zero = util::converter::as<std::string>( crypto::multihash::zero(crypto::multicodec::sha2_256) );
    require( id == zero && root == zero
      && (highest.height() == 0 ? highest.id() == zero : current == zero)
      && state_records == uint64_t(genesis.entries_size()) + 1, "invalid genesis state checkpoint" );
    for( const auto& entry: genesis.entries() )
    {
      chain::database_key key; *key.mutable_space() = entry.space(); key.set_key(entry.key());
      require( get("chain_state", key.SerializeAsString()) == entry.value(), "genesis state object mismatch" );
    }
  }
  const auto result = nlohmann::json{ { "format", "teleno-offline-restore-validation" }, { "version", 1 },
    { "network", spec.data().network }, { "chain_id", spec.data().chain_id }, { "column_families", names.size() },
    { "records_checked", records }, { "state_objects", state_records }, { "state_revision", revision },
    { "state_id", util::to_hex(id) }, { "state_root", util::to_hex(root) }, { "highest_stored_height", highest.height() },
    { "read_only", true }, { "wal_input_bytes", wal_bytes }, { "historical_replay_validated", false } }.dump();
  database.close_checked();
  return result;
}
}
