#include "checkpoint_identity.hpp"
#include <fstream>
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
#include <rocksdb/db.h>

namespace koinos::node::backup {
namespace {
void require(bool value) { if(!value) throw std::runtime_error("backup checkpoint identity is missing or inconsistent; preserve checkpoint"); }
void checked(const rocksdb::Status& s) { require(s.ok()); }
struct Database {
  rocksdb::DB* db=nullptr;
  std::vector<rocksdb::ColumnFamilyHandle*> handles;
  ~Database() { for(auto* h:handles) delete h; delete db; }
  void close_checked() {
    for(auto*& h:handles) { auto status=db->DestroyColumnFamilyHandle(h); h=nullptr; checked(status); }
    handles.clear(); checked(db->Close()); delete db; db=nullptr;
  }
};
}
void validate_public_checkpoint_identity(const nlohmann::json& source,const std::string& network)
{
  auto valid=[](const std::string& id) {
    return id.size()==70 && id.rfind("0x1220",0)==0
      && id.substr(2).find_first_not_of("0123456789abcdef")==std::string::npos;
  };
  if(source.value("identity_status",std::string{})!="checkpoint_verified"
     || source.value("identity_version",0)!=1 || !valid(source.value("chain_id",std::string{})))
    throw std::runtime_error("public bootstrap publish requires a new checkpoint-verified chain identity; recreate the backup");
  if(source.contains("network") && source.at("network")!=network)
    throw std::runtime_error("public bootstrap publish network contradicts checkpoint metadata");
  if(!source.contains("state_revision") || !source.at("state_revision").is_number_unsigned()
     || !source.contains("highest_stored_height") || !source.at("highest_stored_height").is_number_unsigned()
     || source.at("highest_stored_height").get<uint64_t>()<source.at("state_revision").get<uint64_t>()
     || source.value("head_height_kind",std::string{})!="durable_state_revision"
     || !source.contains("head_height") || source.at("head_height")!=source.at("state_revision")
     || !valid(source.value("state_id",std::string{})) || !valid(source.value("state_merkle_root",std::string{}))
     || !valid(source.value("highest_stored_id",std::string{})))
    throw std::runtime_error("public bootstrap publish requires consistent checkpoint-verified state metadata");
}
nlohmann::json read_checkpoint_identity(const std::filesystem::path& path,const std::filesystem::path& genesis_path)
{
  rocksdb::Options options; options.create_if_missing=false; options.paranoid_checks=true; options.max_open_files=64;
  std::vector<std::string> names;
  checked(rocksdb::DB::ListColumnFamilies(options,path.string(),&names));
  std::vector<rocksdb::ColumnFamilyDescriptor> descriptors;
  for(const auto& name:names) descriptors.emplace_back(name,rocksdb::ColumnFamilyOptions{});
  Database database;
  checked(rocksdb::DB::OpenForReadOnly(options,path.string(),descriptors,&database.handles,&database.db,false));
  auto handle=[&](const std::string& name) {
    for(size_t i=0;i<names.size();++i) if(names[i]==name) return database.handles[i];
    throw std::runtime_error("backup checkpoint column-family layout is incomplete");
  };
  rocksdb::ReadOptions reads; reads.verify_checksums=true; reads.fill_cache=false;
  auto get=[&](const std::string& cf,const std::string& key) {
    std::string value; checked(database.db->Get(reads,handle(cf),key,&value)); return value;
  };
  chain::database_key key; key.mutable_space()->set_system(true);
  key.mutable_space()->set_id(chain::system_space_id::metadata);
  key.set_key(util::converter::as<std::string>(crypto::hash(crypto::multicodec::sha2_256,std::string("object_key::chain_id"))));
  std::string chain_id;
  const auto status=database.db->Get(reads,handle("chain_state"),key.SerializeAsString(),&chain_id);
  if(status.IsNotFound()) {
    // Empty storage fixtures/diagnostic backups are not public bootstrap sources.
    for(const auto& cf:{"chain_state","chain_metadata","blocks"}) {
      std::unique_ptr<rocksdb::Iterator> it(database.db->NewIterator(reads,handle(cf)));
      it->SeekToFirst(); checked(it->status()); require(!it->Valid());
    }
    database.close_checked();
    return {{"identity_status","uninitialized"},{"identity_version",1}};
  }
  checked(status); require(chain_id.size()==34 && chain_id[0]=='\x12' && chain_id[1]=='\x20');
  require(std::filesystem::file_size(genesis_path)<=32ULL*1024*1024);
  std::ifstream input(genesis_path,std::ios::binary); require(bool(input));
  std::string bytes{std::istreambuf_iterator<char>(input),{}}; require(!input.bad());
  chain::genesis_data genesis;
  require(google::protobuf::util::JsonStringToMessage(bytes,&genesis).ok() && genesis.entries_size()>0);
  require(chain_id==util::converter::as<std::string>(crypto::hash(crypto::multicodec::sha2_256,genesis)));
  require(get("storage_metadata","layout.version")=="1" && get("storage_metadata","layout.chain_storage")=="unified");
  const auto revision_bytes=get("chain_metadata","revision"); require(revision_bytes.size()==sizeof(uint64_t));
  const auto revision=util::converter::to<uint64_t>(revision_bytes);
  const auto id=get("chain_metadata","id"), root=get("chain_metadata","merkle_root");
  require(id.size()==34 && id[0]=='\x12' && id[1]=='\x20' && root.size()==34 && root[0]=='\x12' && root[1]=='\x20');
  protocol::block_header header;
  require(header.ParseFromString(get("chain_metadata","block_header")) && header.height()==revision);
  block_topology highest;
  require(highest.ParseFromString(get("block_meta",std::string(1,'\1'))) && highest.height()>=revision && highest.id().size()==34
    && highest.id()[0]=='\x12' && highest.id()[1]=='\x20');
  if(revision>0) {
    block_store::block_record record;
    require(record.ParseFromString(get("blocks",id)) && record.has_block() && record.has_receipt()
      && record.block_id()==id && record.block().id()==id && record.block_height()==revision
      && record.receipt().id()==id && record.receipt().state_merkle_root()==root
      && record.block().header().SerializeAsString()==header.SerializeAsString());
  } else {
    const auto zero=util::converter::as<std::string>(crypto::multihash::zero(crypto::multicodec::sha2_256));
    require(id==zero && root==zero);
    if(highest.height()==0) require(highest.id()==zero);
  }
  database.close_checked();
  return {{"identity_status","checkpoint_verified"},{"identity_version",1},{"chain_id",util::to_hex(chain_id)},
    {"state_revision",revision},{"state_id",util::to_hex(id)},{"state_merkle_root",util::to_hex(root)},
    {"highest_stored_height",highest.height()},{"highest_stored_id",util::to_hex(highest.id())},
    {"head_height",revision},{"head_height_kind","durable_state_revision"},
    {"lib_height_known",false},{"historical_replay_validated",false}};
}
}
