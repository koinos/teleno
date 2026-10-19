#include "backup/direct_restore.hpp"
#include "../artifact_root.hpp"
#include "backup/offline_restore_validation.hpp"
#include "backup/checkpoint_identity.hpp"
#include "backup/snapshot_repository.hpp"
#include "core/restore_startup.hpp"
#include "core/storage_lease.hpp"
#include "storage/rocksdb_manager.hpp"

#include <fstream>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <thread>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <google/protobuf/util/json_util.h>
#include <koinos/block_store/block_store.pb.h>
#include <koinos/chain/object_spaces.pb.h>
#include <koinos/crypto/multihash.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <rocksdb/utilities/checkpoint.h>
#include <rocksdb/write_batch.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace koinos;
using namespace koinos::node;
using namespace koinos::node::backup;
namespace {
unsigned checks = 0;
void check(bool b, const std::string& message = "check")
{ ++checks; if(!b) throw std::runtime_error("SC3: " + message); }
void status(const rocksdb::Status& s) { check(s.ok(), s.ToString()); }
template<class F> void rejects(F f)
{ bool threw = false; try { f(); } catch(const std::exception&) { threw = true; } check(threw,"expected rejection"); }
std::string read(const fs::path& p)
{ std::ifstream in(p, std::ios::binary); check(bool(in),p.string()); return {std::istreambuf_iterator<char>(in),{}}; }
json record(const fs::path& p) { return json::parse(read(p)); }
void write(const fs::path& p, const std::string& bytes)
{ fs::create_directories(p.parent_path()); std::ofstream out(p,std::ios::binary); out << bytes; out.close(); check(bool(out)); }
std::string hash(const std::string& bytes)
{
  unsigned char digest[EVP_MAX_MD_SIZE]; unsigned n = 0;
  check(EVP_Digest(bytes.data(),bytes.size(),digest,&n,EVP_sha256(),nullptr) == 1);
  std::ostringstream out; out << std::hex << std::setfill('0');
  for(unsigned i=0;i<n;++i) out << std::setw(2) << unsigned(digest[i]);
  return out.str();
}
uint64_t inode(const fs::path& p)
{ struct stat st{}; check(::lstat(p.c_str(),&st) == 0); return st.st_ino; }
std::string state_key(const std::string& name)
{
  chain::database_key key; key.mutable_space()->set_system(true); key.mutable_space()->set_id(chain::system_space_id::metadata);
  key.set_key(util::converter::as<std::string>(crypto::hash(crypto::multicodec::sha2_256,"object_key::" + name)));
  return key.SerializeAsString();
}
struct Fixture
{
  RestoreMetadataInput input;
  BackupPublicRestoreConfig source;
  RestoreSpecification spec;
  Fixture(const fs::path& root, bool wal = false, bool nonzero = false, bool mismatch = false, bool padding = false):
    spec(make(root,wal,nonzero,mismatch,padding)) {}
  RestoreSpecification make(const fs::path& root, bool wal, bool nonzero, bool mismatch, bool padding)
  {
    const auto node = root / "source-node", checkpoint_path = root / "checkpoint", public_path = root / "public";
    fs::create_directories(checkpoint_path); fs::create_directories(node);
    const auto genesis_bytes = read(fs::path(TELENO_SC3_REPOSITORY_ROOT) / "config/example/harbinger/genesis_data.json");
    chain::genesis_data genesis; check(google::protobuf::util::JsonStringToMessage(genesis_bytes,&genesis).ok());
    const auto chain_id = util::converter::as<std::string>(crypto::hash(crypto::multicodec::sha2_256,genesis));
    const auto zero = util::converter::as<std::string>(crypto::multihash::zero(crypto::multicodec::sha2_256));
    NodeConfig config; config.rocksdb_compression = "none"; config.rocksdb_blocks_compression = "none";
    config.rocksdb_block_cache_mb = 4;
    storage::RocksDBManager db; db.open(node,config);
    rocksdb::WriteBatch batch;
    auto put = [&](storage::ColumnFamily cf,const std::string& key,const std::string& value) { status(batch.Put(db.handle(cf),key,value)); };
    for(const auto& entry: genesis.entries())
    { chain::database_key key; *key.mutable_space() = entry.space(); key.set_key(entry.key()); put(storage::ColumnFamily::chain_state,key.SerializeAsString(),entry.value()); }
    put(storage::ColumnFamily::chain_state,state_key("chain_id"),chain_id);
    protocol::block_header header; std::string id = zero, merkle = zero;
    if(nonzero)
    {
      header.set_height(1); header.set_previous(zero); header.set_timestamp(1);
      id = util::converter::as<std::string>(crypto::hash(crypto::multicodec::sha2_256,header));
      merkle = util::converter::as<std::string>(crypto::hash(crypto::multicodec::sha2_256,std::string("synthetic-checkpoint-root")));
      block_store::block_record block;
      block.set_block_id(id); block.set_block_height(1); block.add_previous_block_ids(zero);
      block.mutable_block()->set_id(id); *block.mutable_block()->mutable_header() = header;
      block.mutable_receipt()->set_id(id); block.mutable_receipt()->set_state_merkle_root(mismatch ? zero : merkle);
      put(storage::ColumnFamily::blocks,id,block.SerializeAsString());
      put(storage::ColumnFamily::chain_state,state_key("head_block"),block.block().SerializeAsString());
    }
    put(storage::ColumnFamily::chain_metadata,"size",util::converter::as<std::string>(uint64_t(genesis.entries_size()+1+(nonzero ? 1 : 0))));
    put(storage::ColumnFamily::chain_metadata,"revision",util::converter::as<std::string>(uint64_t(nonzero ? 1 : 0)));
    put(storage::ColumnFamily::chain_metadata,"id",id); put(storage::ColumnFamily::chain_metadata,"merkle_root",merkle);
    put(storage::ColumnFamily::chain_metadata,"block_header",header.SerializeAsString());
    block_topology highest; highest.set_height(nonzero ? 1 : 0); highest.set_id(id);
    put(storage::ColumnFamily::block_meta,std::string(1,'\x01'),highest.SerializeAsString());
    // Exercise several CFs and WAL replay without assuming the runtime's
    // unchecked backend flush is proof of checkpoint consistency.
    for(auto cf: {storage::ColumnFamily::default_state,storage::ColumnFamily::contract_meta,
      storage::ColumnFamily::transaction_index,storage::ColumnFamily::account_history}) put(cf,"sc3-fixture","synthetic");
    // SC5 only: 32 MiB of deterministic optional-CF bytes, not a chain workload.
    if(padding)
    {
      uint32_t seed = 0x5c0005;
      for(unsigned row = 0; row < 8192; ++row)
      {
        std::string value(4096,'\0');
        for(auto& byte: value) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; byte = char(seed & 255); }
        put(storage::ColumnFamily::contract_meta,"sc5-padding-" + std::to_string(row),value);
      }
    }
    rocksdb::WriteOptions writes; writes.sync = true; status(db.db()->Write(writes,&batch));
    status(db.db()->FlushWAL(true));
    rocksdb::Checkpoint* raw = nullptr; status(rocksdb::Checkpoint::Create(db.db(),&raw));
    std::unique_ptr<rocksdb::Checkpoint> checkpoint(raw);
    status(checkpoint->CreateCheckpoint((checkpoint_path / "db").string(),wal ? UINT64_MAX : 0));
    checkpoint.reset(); db.close();
    write(checkpoint_path / "chain/genesis_data.json",genesis_bytes);
    write(checkpoint_path / "config.yml","features:\n  block_producer: true\n");
    json files = json::array(); uint64_t db_bytes = 0,runtime_bytes = 0; std::set<std::string> objects;
    for(const auto& entry: fs::recursive_directory_iterator(checkpoint_path)) if(entry.is_regular_file())
    {
      const auto relative = entry.path().lexically_relative(checkpoint_path).generic_string();
      const auto bytes = read(entry.path()), digest = hash(bytes);
      const bool runtime = relative.rfind("db/",0) != 0;
      (runtime ? runtime_bytes : db_bytes) += bytes.size(); objects.insert(digest);
      files.push_back({{"path",relative},{"sha256",digest},{"size_bytes",bytes.size()},{"runtime_file",runtime}});
      const auto object = public_path / "objects/sha256" / digest.substr(0,2) / digest.substr(2,2) / digest;
      if(!fs::exists(object)) write(object,bytes);
    }
    input.backup_id = "sc3-native-fixture"; input.source_identity = "file://" + public_path.string();
    input.expected_chain_id = util::to_hex(chain_id); input.expected_network = "testnet";
    input.manifest = json{{"format","teleno-native-rocksdb-snapshot"},{"version",1},{"backup_id",input.backup_id},
      {"source",{{"storage_layout","unified"}}},{"public_bootstrap",{{"network","testnet"},{"chain_id",input.expected_chain_id}}},
      {"snapshot",{{"file_count",files.size()},{"object_count",objects.size()},{"total_bytes",db_bytes+runtime_bytes}}},
      {"sizes",{{"restored_database_bytes",db_bytes},{"runtime_files_bytes",runtime_bytes},{"object_download_bytes",db_bytes+runtime_bytes},{"archive_bytes",0}}}}.dump();
    input.inventory = json{{"format","teleno-native-snapshot-files"},{"version",1},{"backup_id",input.backup_id},{"files",files}}.dump();
    source.base_url = input.source_identity; source.network = "testnet"; source.require_https = false; source.retries = 1;
    write(root / "spec-input.json",json{{"backup_id",input.backup_id},{"source_identity",input.source_identity},
      {"expected_network",input.expected_network},{"expected_chain_id",input.expected_chain_id},
      {"manifest",input.manifest},{"inventory",input.inventory}}.dump());
    return validate_restore_metadata(input);
  }
};
void staged(const Fixture& f,const fs::path& target,const fs::path& attempt)
{ check(stage_direct_restore_snapshot(f.spec,f.source,target,attempt).download_complete,"stage"); }
void verified(const Fixture& f,const fs::path& target,const fs::path& attempt)
{ staged(f,target,attempt); check(validate_direct_restore_attempt(f.spec,target,attempt).state == "verified"); }
void publish_cli_fixture(const Fixture& f,const fs::path& root)
{
  const auto pub = root / "public", snapshot = pub / "snapshots" / f.input.backup_id;
  write(snapshot / "manifest.json",f.input.manifest);
  write(snapshot / "files.json",f.input.inventory);
  write(snapshot / "COMPLETE","complete\n");
  const auto metadata = json{{"format","teleno-public-bootstrap-snapshot"},{"version",1},
    {"backup_id",f.input.backup_id},{"network","testnet"},{"chain_id",f.input.expected_chain_id}}.dump();
  write(snapshot / "public-bootstrap.json",metadata);
  const auto prefix = "snapshots/" + f.input.backup_id + "/";
  const auto latest = json{{"format","teleno-native-latest-snapshot"},{"version",1},{"backup_id",f.input.backup_id},
    {"snapshot_dir",f.input.backup_id},{"manifest",prefix + "manifest.json"},{"files",prefix + "files.json"},
    {"public_metadata",prefix + "public-bootstrap.json"},{"signature",prefix + "public-bootstrap-signature.json"}}.dump();
  write(pub / "latest.json",latest);
  // Publicly known deterministic fixture seed, never an operator/producer key.
  std::array<unsigned char,32> seed{}; for(size_t i=0;i<seed.size();++i) seed[i] = unsigned(i+1);
  std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)> key(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519,nullptr,seed.data(),seed.size()),EVP_PKEY_free);
  check(bool(key)); std::unique_ptr<BIO,decltype(&BIO_free)> bio(BIO_new(BIO_s_mem()),BIO_free);
  check(bio && PEM_write_bio_PUBKEY(bio.get(),key.get()) == 1); char* pem = nullptr;
  const auto length = BIO_get_mem_data(bio.get(),&pem); check(length > 0);
  write(root / "fixture-signing.pub",std::string(pem,length));
  const auto manifest = json::parse(f.input.manifest);
  const json payload{{"backup_id",f.input.backup_id},{"network","testnet"},{"chain_id",f.input.expected_chain_id},
    {"manifest_sha256",hash(f.input.manifest)},{"files_sha256",hash(f.input.inventory)},
    {"latest_sha256",hash(latest)},{"public_metadata_sha256",hash(metadata)},
    {"object_count",manifest.at("snapshot").at("object_count")},{"total_bytes",manifest.at("snapshot").at("total_bytes")}};
  const auto message = payload.dump();
  std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(),EVP_MD_CTX_free);
  check(context && EVP_DigestSignInit(context.get(),nullptr,nullptr,nullptr,key.get()) == 1);
  std::array<unsigned char,64> signature{}; size_t size = signature.size();
  check(EVP_DigestSign(context.get(),signature.data(),&size,reinterpret_cast<const unsigned char*>(message.data()),message.size()) == 1);
  std::ostringstream hex; hex << std::hex << std::setfill('0');
  for(size_t i=0;i<size;++i) hex << std::setw(2) << unsigned(signature[i]);
  write(snapshot / "public-bootstrap-signature.json",json{{"format","teleno-public-bootstrap-signature"},{"version",1},
    {"algorithm","ed25519"},{"key_id","publicly-known-sc4-fixture-key"},{"payload",payload},{"signature_hex",hex.str()}}.dump());
  write(root / "fixture.json",json{{"backup_id",f.input.backup_id},{"chain_id",f.input.expected_chain_id},
    {"manifest_sha256",hash(f.input.manifest)},{"inventory_sha256",hash(f.input.inventory)}}.dump());
}
void held(const fs::path& target)
{
  const auto generation = read(target / ".backup-observer-recovery");
  NodeConfig cfg; cfg.features["block_producer"] = true; cfg.verify_blocks = false;
  auto first = apply_restore_startup_policy(cfg,target,true);
  check(first.producer_recovery_hold_active && !cfg.is_enabled("block_producer") && !cfg.verify_blocks,"first observer even with explicit activation");
  cfg.features["block_producer"] = true;
  auto second = apply_restore_startup_policy(cfg,target,false);
  check(second.producer_recovery_hold_active && !cfg.is_enabled("block_producer"),"persistent observer hold");
  check(read(target / ".backup-observer-recovery") == generation,"generation preserved across startup");
}
void startup_refuses(const std::string& node, const fs::path& target, const fs::path& log, const std::string& reason)
{
  const auto pid = ::fork(); check(pid >= 0);
  if(pid == 0)
  {
    const int fd = ::open(log.c_str(),O_WRONLY | O_CREAT | O_EXCL,0600);
    if(fd < 0 || ::dup2(fd,1) < 0 || ::dup2(fd,2) < 0) ::_exit(3);
    ::close(fd);
    ::execl(node.c_str(),node.c_str(),"--basedir",target.c_str(),nullptr); ::_exit(4);
  }
  int result = 0;
  check(::waitpid(pid,&result,0) == pid && WIFEXITED(result) && WEXITSTATUS(result) == 1,"startup refused");
  check(read(log).find(reason) != std::string::npos,"startup refusal reason");
}
void observer_smoke(const std::string& node, const fs::path& target, const fs::path& log)
{
  const auto pid = ::fork(); check(pid >= 0);
  if(pid == 0)
  {
    const int fd = ::open(log.c_str(),O_WRONLY | O_CREAT | O_EXCL,0600);
    if(fd < 0 || ::dup2(fd,1) < 0 || ::dup2(fd,2) < 0) ::_exit(3);
    ::close(fd);
    // No sockets, network peers, optional writers, producer activation/keys.
    // Config deliberately enables production; the persisted hold must win.
    ::execl(node.c_str(),node.c_str(),"--basedir",target.c_str(),"--disable",
      "p2p","jsonrpc","grpc","mempool","account_history","contract_meta_store","transaction_store",nullptr);
    ::_exit(4);
  }
  bool ready = false, exited = false; int result = 0;
  for(unsigned n=0;n<500;++n)
  {
    if(::waitpid(pid,&result,WNOHANG) == pid) { exited = true; break; }
    if(fs::exists(log))
    {
      std::ifstream in(log); const std::string bytes{std::istreambuf_iterator<char>(in),{}};
      if(bytes.find("[node] teleno_node ready") != std::string::npos) { ready = true; break; }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  if(!exited)
  {
    ::kill(pid,SIGINT);
    for(unsigned n=0;n<500;++n)
    { if(::waitpid(pid,&result,WNOHANG) == pid) { exited = true; break; } std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
    if(!exited) { ::kill(pid,SIGKILL); ::waitpid(pid,&result,0); }
  }
  check(ready && exited && WIFEXITED(result) && WEXITSTATUS(result) == 0,"isolated restored observer ready and clean shutdown: " + log.string());
  check(read(log).find("Restore recovery hold active") != std::string::npos,"native producer hold applied");
  check(fs::exists(target / ".backup-observer-recovery"),"native hold retained");
}
} // namespace

int main(int argc, char** argv)
{
  try
  {
    if(argc == 5 && std::string(argv[1]) == "--sc5-inspect")
    {
      const auto repo = test_artifact_root(TELENO_SC3_REPOSITORY_ROOT);
      for(int i=2;i<argc;++i)
      {
        check(test_artifact_child(fs::absolute(argv[i]),repo),"SC5 inspect confined to scratch");
      }
      const fs::path source_path = fs::absolute(argv[2]);
      const auto selected = record(source_path / "fixture.json");
      BackupPublicRestoreConfig source; source.enabled = true; source.network = "testnet";
      source.base_url = "file://" + (source_path / "public").string(); source.require_https = false;
      source.signature_required = true; source.signature_public_key_file = (source_path / "fixture-signing.pub").string();
      auto spec = acquire_public_restore_specification(argv[4],source,selected.at("backup_id"),selected.at("chain_id"));
      StorageLease lease(argv[3]);
      std::cout << validate_offline_restore_database(spec,argv[3]) << std::endl; return 0;
    }
    if((argc == 3 && (std::string(argv[1]) == "--sc4-prepare" || std::string(argv[1]) == "--sc4-prepare-inconsistent" || std::string(argv[1]) == "--sc5-prepare"))
      || (argc == 5 && std::string(argv[1]) == "--sc4-interrupt-activation"))
    {
      const auto repo = test_artifact_root(TELENO_SC3_REPOSITORY_ROOT);
      for(int i=2;i<argc;++i)
      {
        check(test_artifact_child(fs::absolute(argv[i]),repo),"SC4 helper confined to ignored fixtures");
      }
      const fs::path root = fs::absolute(argv[2]);
      if(argc == 3)
      {
        check(fs::create_directory(root),"fresh SC4 fixture root");
        const bool inconsistent = std::string(argv[1]) == "--sc4-prepare-inconsistent";
        Fixture fixture(root,false,inconsistent,inconsistent,std::string(argv[1]) == "--sc5-prepare");
        publish_cli_fixture(fixture,root); return 0;
      }
      const auto selected = record(root / "fixture.json");
      BackupPublicRestoreConfig source; source.enabled = true; source.base_url = "file://" + (root / "public").string();
      source.network = "testnet"; source.require_https = false; source.signature_required = true;
      source.signature_public_key_file = (root / "fixture-signing.pub").string();
      auto spec = acquire_public_restore_specification(root.parent_path() / "helper-metadata",source,
        selected.at("backup_id"),selected.at("chain_id"));
      validate_direct_restore_attempt(spec,argv[3],argv[4]);
      DirectStageOptions fault; fault.checkpoint = [](const std::string& boundary,uint64_t){if(boundary == "after_database_rename") ::_exit(99);};
      activate_direct_restore_attempt(spec,argv[3],argv[4],fault); return 2;
    }
    if(argc == 6 && (std::string(argv[1]) == "--sc3-validation-fault" || std::string(argv[1]) == "--sc3-activation-fault"))
    {
      // Exec a fresh process: do not inherit RocksDB Env worker-pool mutexes
      // from a parent that has previously created fixture databases.
      const auto repo = test_artifact_root(TELENO_SC3_REPOSITORY_ROOT);
      for(int i: {2,3,4})
      {
        check(test_artifact_child(fs::absolute(argv[i]),repo),"worker confined to ignored fixtures");
      }
      const auto input_json = record(argv[2]); RestoreMetadataInput input;
      input.backup_id = input_json.at("backup_id"); input.source_identity = input_json.at("source_identity");
      input.expected_network = input_json.at("expected_network"); input.expected_chain_id = input_json.at("expected_chain_id");
      input.manifest = input_json.at("manifest"); input.inventory = input_json.at("inventory");
      const auto spec = validate_restore_metadata(input); const auto nth = std::stoull(argv[5]); size_t event = 0;
      DirectStageOptions options; options.checkpoint = [&](const std::string&,uint64_t){if(event++ == nth) ::_exit(99);};
      if(std::string(argv[1]) == "--sc3-validation-fault") validate_direct_restore_attempt(spec,argv[3],argv[4],options);
      else activate_direct_restore_attempt(spec,argv[3],argv[4],options);
      return 1; // The requested fault must have been reached.
    }
    check(argc == 2,"test requires the native node executable"); const std::string native_node = argv[1];
    const auto evidence = test_artifact_root(TELENO_SC3_REPOSITORY_ROOT);
    fs::create_directories(evidence);
    const auto root = evidence / ("sc3-unit-" + std::to_string(std::random_device{}())); check(fs::create_directory(root));
    Fixture fixture(root / "fixture");
    const auto checkpoint_dir=root / "fixture/checkpoint";
    const auto backup_genesis=read(checkpoint_dir / "chain/genesis_data.json");
    const auto identity=read_checkpoint_identity(checkpoint_dir / "db",checkpoint_dir / "chain/genesis_data.json");
    check(identity.at("chain_id")==fixture.spec.data().chain_id,"backup identity measured from checkpoint");
    check(identity.at("head_height_kind")=="durable_state_revision" && !identity.at("lib_height_known").get<bool>());
    CheckpointResult checkpoint; checkpoint.checkpoint_dir=checkpoint_dir; checkpoint.db_dir=checkpoint_dir / "db";
    NodeConfig backup_config; backup_config.backup.local.retention_count=3;
    LocalSnapshotRepository backup_repository(root / "native-backup");
    auto native_snapshot=backup_repository.store_checkpoint_snapshot(checkpoint,backup_config,checkpoint_dir,checkpoint_dir / "config.yml");
    const auto native_manifest=record(native_snapshot.manifest_path);
    check(native_manifest.at("source").at("chain_id")==fixture.spec.data().chain_id);
    check(native_manifest.at("source").at("identity_status")=="checkpoint_verified");
    validate_public_checkpoint_identity(native_manifest.at("source"),"testnet");
    auto inconsistent_identity=native_manifest.at("source"); inconsistent_identity["head_height"]=uint64_t(1);
    rejects([&]{validate_public_checkpoint_identity(inconsistent_identity,"testnet");});
    const auto previous_latest=read(native_snapshot.latest_path);
    write(checkpoint_dir / "chain/genesis_data.json","{}");
    rejects([&]{read_checkpoint_identity(checkpoint_dir / "db",checkpoint_dir / "chain/genesis_data.json");});
    rejects([&]{backup_repository.store_checkpoint_snapshot(checkpoint,backup_config,checkpoint_dir,checkpoint_dir / "config.yml");});
    check(read(native_snapshot.latest_path)==previous_latest,"failed backup preserves latest");
    write(checkpoint_dir / "chain/genesis_data.json",backup_genesis);
    const auto backed_up_files=record(native_snapshot.files_path);
    const auto digest=backed_up_files.at("files").at(0).at("sha256").get<std::string>();
    const auto object=root / "native-backup/objects/sha256" / digest.substr(0,2) / digest.substr(2,2) / digest;
    const auto original_object=read(object); write(object,"corrupt-owned-fixture-object");
    rejects([&]{backup_repository.store_checkpoint_snapshot(checkpoint,backup_config,checkpoint_dir,checkpoint_dir / "config.yml");});
    check(read(native_snapshot.latest_path)==previous_latest,"corrupt reusable object cannot publish latest");
    write(object,original_object);
    const auto target = root / "success-target", attempt = root / "success-attempt";
    write(target / "config.yml","features:\n  block_producer: true\n# preserve user config\n"); const auto config = read(target / "config.yml");
    staged(fixture,target,attempt);
    std::vector<std::string> validation_boundaries;
    DirectStageOptions validation_tracing;
    validation_tracing.checkpoint = [&](const std::string& name,uint64_t){validation_boundaries.push_back(name);};
    check(validate_direct_restore_attempt(fixture.spec,target,attempt,validation_tracing).state == "verified");
    const auto db_inode = inode(attempt / "payload/db");
    std::vector<std::string> boundaries;
    DirectStageOptions tracing; tracing.checkpoint = [&](const std::string& name,uint64_t){boundaries.push_back(name);};
    check(activate_direct_restore_attempt(fixture.spec,target,attempt,tracing).state == "activated");
    check(std::count(validation_boundaries.begin(),validation_boundaries.end(),"before_offline_database_validation") == 1
      && std::count(boundaries.begin(),boundaries.end(),"before_offline_database_validation") == 2,
      "standalone validation and activation retain three complete database scans");
    check(inode(target / "db") == db_inode && !fs::exists(attempt / "payload/db"),"one renamed database");
    check(read(target / "config.yml") == config && !fs::exists(target / ".pre-restore"),"no config overwrite/old DB copy");
    held(target);
    check(activate_direct_restore_attempt(fixture.spec,target,attempt).state == "activated","idempotent after observer startup");
    const auto saved_hold = read(target / ".backup-observer-recovery");
    auto wrong_hold = json::parse(saved_hold); wrong_hold["attempt_id"] = std::string(32,'0');
    write(target / ".backup-observer-recovery",wrong_hold.dump());
    startup_refuses(native_node,target,root / "startup-foreign-hold.log","direct restore observer hold generation mismatch");
    write(target / ".backup-observer-recovery",saved_hold);
    observer_smoke(native_node,target,root / "observer-smoke.log");
    {
      const auto t = root / "combined-target", a = root / "combined-attempt";
      staged(fixture,t,a); const auto original_inode = inode(a / "payload/db");
      unsigned scans = 0; bool handoff_locked = false;
      DirectStageOptions combined;
      combined.checkpoint = [&](const std::string& name,uint64_t) {
        if(name == "before_offline_database_validation") ++scans;
        if(name == "before_activation")
        {
          rejects([&]{StorageLease competing(t);});
          const auto competing = ::open((a / "lock").c_str(),O_RDWR | O_NOFOLLOW | O_CLOEXEC);
          check(competing >= 0);
          const auto result = ::flock(competing,LOCK_EX | LOCK_NB), error = errno;
          ::close(competing);
          check(result != 0 && (error == EWOULDBLOCK || error == EAGAIN),"attempt lock retained across validation handoff");
          handoff_locked = true;
        }
      };
      const auto result = validate_and_activate_direct_restore_attempt(fixture.spec,t,a,combined);
      check(result.state == "activated" && result.observer_hold && scans == 2 && handoff_locked,
        "combined completion validates fresh and reopens after publication");
      check(inode(t / "db") == original_inode && !fs::exists(a / "payload/db")); held(t);
    }
    {
      const auto t = root / "combined-resume-target", a = root / "combined-resume-attempt";
      staged(fixture,t,a); unsigned scans = 0;
      DirectStageOptions interrupted;
      interrupted.checkpoint = [&](const std::string& name,uint64_t) {
        if(name == "before_offline_database_validation") ++scans;
        if(name == "before_activation") throw std::runtime_error("interrupt after fresh validation");
      };
      rejects([&]{validate_and_activate_direct_restore_attempt(fixture.spec,t,a,interrupted);});
      check(scans == 1 && read_direct_restore_attempt_state(a) == "verified" && !fs::exists(t / "db"));
      auto forged = record(a / "validation.json"); forged["validation"]["historical_replay_validated"] = true;
      write(a / "validation.json",forged.dump()); scans = 0;
      DirectStageOptions retry;
      retry.checkpoint = [&](const std::string& name,uint64_t) {if(name == "before_offline_database_validation") ++scans;};
      const auto result = validate_and_activate_direct_restore_attempt(fixture.spec,t,a,retry);
      check(result.state == "activated" && scans == 2,"resumed completion never reuses a persisted proof");
      check(!json::parse(result.validation_json).at("historical_replay_validated").get<bool>()); held(t);
    }
    for(const auto& tamper: {"payload","validation-record"})
    {
      const auto t = root / (std::string("combined-tamper-") + tamper + "-target");
      const auto a = root / (std::string("combined-tamper-") + tamper + "-attempt");
      staged(fixture,t,a); unsigned scans = 0; bool changed = false;
      DirectStageOptions fault;
      fault.checkpoint = [&](const std::string& name,uint64_t) {
        if(name == "before_offline_database_validation") ++scans;
        if(name != "before_activation") return;
        if(std::string(tamper) == "payload")
        {
          for(const auto& file: fs::directory_iterator(a / "payload/db")) if(file.path().extension() == ".sst")
          { auto bytes = read(file.path()); bytes.at(bytes.size()/2) ^= 0xff; write(file.path(),bytes); changed = true; break; }
        }
        else
        {
          auto forged = record(a / "validation.json"); forged["validation"]["historical_replay_validated"] = true;
          write(a / "validation.json",forged.dump()); changed = true;
        }
      };
      rejects([&]{validate_and_activate_direct_restore_attempt(fixture.spec,t,a,fault);});
      check(changed && scans == 1 && fs::exists(a / "payload/db") && !fs::exists(t / "db"),
        "fresh proof cannot bypass changed payload hashes or a changed verification record");
      check(!fs::exists(t.parent_path() / ("." + t.filename().string() + ".teleno-storage.lock.activation.json")));
    }
    {
      const auto t = root / "combined-rename-target", a = root / "combined-rename-attempt";
      staged(fixture,t,a); const auto original_inode = inode(a / "payload/db");
      DirectStageOptions fault;
      fault.checkpoint = [](const std::string& name,uint64_t) {
        if(name == "after_database_rename") throw std::runtime_error("interrupt after publication");
      };
      rejects([&]{validate_and_activate_direct_restore_attempt(fixture.spec,t,a,fault);});
      check(read_direct_restore_attempt_state(a) == "activation_pending" && inode(t / "db") == original_inode);
      unsigned scans = 0; DirectStageOptions recover;
      recover.checkpoint = [&](const std::string& name,uint64_t) {if(name == "before_offline_database_validation") ++scans;};
      check(validate_and_activate_direct_restore_attempt(fixture.spec,t,a,recover).state == "activated"
        && scans == 1 && inode(t / "db") == original_inode,"interrupted publication requires a fresh validated reopen"); held(t);
    }
    // A subsequent legacy activation keeps its established .pre-restore flow,
    // but must archive the previous direct receipt with the old database.
    const auto legacy_stage = root / "legacy-after-direct-stage";
    fs::copy(root / "fixture/checkpoint",legacy_stage,fs::copy_options::recursive);
    write(legacy_stage / "RESTORE_STAGE_COMPLETE","complete\n");
    write(legacy_stage / ".teleno-restore-stage.json",json{{"backup_id","sc3-legacy-after-direct"}}.dump());
    const auto old_receipt = read(target / ".teleno-direct-restore.json");
    {
      StorageLease lease(target);
      const auto legacy = activate_staged_restore_snapshot(legacy_stage,target);
      check(read(legacy.pre_restore_dir / ".teleno-direct-restore.json") == old_receipt,"legacy preserves retired direct receipt");
      check(inode(legacy.pre_restore_dir / "db") == db_inode,"legacy preserves old direct DB");
      check(!fs::exists(target / ".teleno-direct-restore.json"),"legacy does not adopt stale direct receipt");
      check(read(target / "config.yml") == config,"legacy preserves active config");
      NodeConfig legacy_config; legacy_config.features["block_producer"] = true;
      check(apply_restore_startup_policy(legacy_config,target,true).producer_recovery_hold_active
        && !legacy_config.is_enabled("block_producer"),"legacy remains observer-first after direct restore");
    }
    // Validation interruptions resume offline, with no writable DB open or
    // re-downloading of source bytes after the validating transition.
    for(bool kill: {false,true}) for(size_t n=0;n<validation_boundaries.size();++n)
    {
      const auto name = std::string(kill ? "validate-kill-" : "validate-throw-") + std::to_string(n);
      const auto t = root / (name + "-target"), a = root / (name + "-attempt");
      staged(fixture,t,a); size_t event = 0;
      DirectStageOptions fault; fault.checkpoint = [&](const std::string&,uint64_t) {
        if(event++ == n) { if(kill) ::_exit(99); throw std::runtime_error("injected validation boundary"); }
      };
      if(kill)
      {
        const auto pid = ::fork(); check(pid >= 0);
        if(pid == 0) { const auto nth = std::to_string(n); ::execl(argv[0],argv[0],"--sc3-validation-fault",
          (root / "fixture/spec-input.json").c_str(),t.c_str(),a.c_str(),nth.c_str(),nullptr); ::_exit(2); }
        int result = 0; check(::waitpid(pid,&result,0) == pid && WIFEXITED(result) && WEXITSTATUS(result) == 99);
      }
      else rejects([&]{validate_direct_restore_attempt(fixture.spec,t,a,fault);});
      check(!fs::exists(t) && fs::exists(a / "payload/db"));
      const auto state = record(a / "state.json").at("state");
      if(state != "download_complete") rejects([&]{stage_direct_restore_snapshot(fixture.spec,fixture.source,t,a,true);});
      check(validate_direct_restore_attempt(fixture.spec,t,a).state == "verified");
    }
    // Exclusion is cross-process, shared with node startup, stage and validation.
    const auto locked_target = root / "locked-target";
    {
      StorageLease lease(locked_target);
      const auto pid = ::fork(); check(pid >= 0);
      if(pid == 0) { try { StorageLease other(locked_target); ::_exit(1); } catch(...) { ::_exit(0); } }
      int result = 0; check(::waitpid(pid,&result,0) == pid && WIFEXITED(result) && WEXITSTATUS(result) == 0);
      rejects([&]{staged(fixture,locked_target,root / "locked-attempt");});
      startup_refuses(native_node,locked_target,root / "startup-locked.log","storage gate busy");
      check(!fs::exists(locked_target),"blocked startup did not create target");
    }
    // Kill and exception injection at EVERY observed activation write/sync/
    // journal/marker/runtime/db rename boundary, using a new owned attempt.
    for(bool kill: {false,true}) for(size_t n=0;n<boundaries.size();++n)
    {
      const auto name = std::string(kill ? "kill-" : "throw-") + std::to_string(n);
      const auto t = root / (name + "-target"), a = root / (name + "-attempt");
      write(t / "config.yml",config);
      verified(fixture,t,a); const auto before = inode(a / "payload/db"); size_t event = 0;
      DirectStageOptions fault; fault.checkpoint = [&](const std::string&,uint64_t) {
        if(event++ == n) { if(kill) ::_exit(99); throw std::runtime_error("injected SC3 boundary"); }
      };
      if(kill)
      {
        const auto pid = ::fork(); check(pid >= 0);
        if(pid == 0) { const auto nth = std::to_string(n); ::execl(argv[0],argv[0],"--sc3-activation-fault",
          (root / "fixture/spec-input.json").c_str(),t.c_str(),a.c_str(),nth.c_str(),nullptr); ::_exit(2); }
        int result = 0; check(::waitpid(pid,&result,0) == pid && WIFEXITED(result) && WEXITSTATUS(result) == 99,"fault boundary reached");
      }
      else rejects([&]{activate_direct_restore_attempt(fixture.spec,t,a,fault);});
      check(fs::exists(a / "payload/db") != fs::exists(t / "db"),"exactly one database after interruption");
      { StorageLease lease(t);
        if(fs::exists(lease.intent_path()))
        {
          // Release the gate before testing the durable-intent refusal below.
          check(fs::is_regular_file(lease.intent_path()));
        }
      }
      const auto intent_path = t.parent_path() / ("." + t.filename().string() + ".teleno-storage.lock.activation.json");
      if(fs::exists(intent_path)) startup_refuses(native_node,t,root / (name + "-startup.log"),"unfinished direct restore activation");
      check(activate_direct_restore_attempt(fixture.spec,t,a).state == "activated","recovery");
      check(inode(t / "db") == before,"recovery preserved inode"); held(t);
    }
    const auto bad_target = root / "changed-target", bad_attempt = root / "changed-attempt";
    verified(fixture,bad_target,bad_attempt);
    auto current = read(bad_attempt / "payload/db/CURRENT"); write(bad_attempt / "payload/db/CURRENT",current + "bad");
    rejects([&]{activate_direct_restore_attempt(fixture.spec,bad_target,bad_attempt);}); check(!fs::exists(bad_target / "db"));
    rejects([&]{stage_direct_restore_snapshot(fixture.spec,fixture.source,bad_target,bad_attempt,true);});
    const auto race_target = root / "race-target", race_attempt = root / "race-attempt";
    verified(fixture,race_target,race_attempt);
    DirectStageOptions race; race.checkpoint = [&](const std::string& name,uint64_t) {
      if(name == "before_database_rename") write(race_target / "unrelated-user-file","preserve");
    };
    rejects([&]{activate_direct_restore_attempt(fixture.spec,race_target,race_attempt,race);});
    check(read(race_target / "unrelated-user-file") == "preserve" && fs::exists(race_attempt / "payload/db"));
    const auto populated = root / "populated-target", populated_attempt = root / "populated-attempt";
    verified(fixture,populated,populated_attempt); write(populated / "db/user-file","do not overwrite");
    rejects([&]{activate_direct_restore_attempt(fixture.spec,populated,populated_attempt);});
    check(read(populated / "db/user-file") == "do not overwrite");
    const auto invalid = root / "invalid-target", invalid_attempt = root / "invalid-attempt";
    staged(fixture,invalid,invalid_attempt); write(invalid_attempt / "payload/db/CURRENT","MANIFEST-absent\n");
    rejects([&]{validate_direct_restore_attempt(fixture.spec,invalid,invalid_attempt);}); check(!fs::exists(invalid));
    const auto corrupt = root / "corrupt-target", corrupt_attempt = root / "corrupt-attempt";
    staged(fixture,corrupt,corrupt_attempt);
    bool corrupted = false;
    for(const auto& entry: fs::directory_iterator(corrupt_attempt / "payload/db")) if(entry.path().extension() == ".sst")
    { auto data = read(entry.path()); data.at(data.size()/2) ^= 0xff; write(entry.path(),data); corrupted = true; break; }
    check(corrupted); rejects([&]{validate_offline_restore_database(fixture.spec,corrupt_attempt / "payload");});
    // An absent SST is not treated as a missing application object.
    const auto incomplete = root / "incomplete-target", incomplete_attempt = root / "incomplete-attempt";
    staged(fixture,incomplete,incomplete_attempt);
    bool removed = false;
    for(const auto& entry: fs::directory_iterator(incomplete_attempt / "payload/db")) if(entry.path().extension() == ".sst")
    { check(fs::remove(entry.path())); removed = true; break; } // Only this new owned test fixture.
    check(removed); rejects([&]{validate_offline_restore_database(fixture.spec,incomplete_attempt / "payload");});
    const auto foreign = root / "foreign-target", foreign_attempt = root / "foreign-attempt";
    verified(fixture,foreign,foreign_attempt); write(foreign / ".backup-observer-recovery","existing user hold");
    rejects([&]{activate_direct_restore_attempt(fixture.spec,foreign,foreign_attempt);});
    check(read(foreign / ".backup-observer-recovery") == "existing user hold");
    const auto space_target = root / "space-target", space_attempt = root / "space-attempt";
    staged(fixture,space_target,space_attempt); DirectStageOptions no_space;
    no_space.available_bytes = [](const auto&){return 0;};
    rejects([&]{validate_direct_restore_attempt(fixture.spec,space_target,space_attempt,no_space);});
    check(record(space_attempt / "state.json").at("state") == "download_complete");
    bool cancelled = false; DirectStageOptions cancel;
    cancel.transfer.cancel_requested = [&]{return cancelled;};
    cancel.checkpoint = [&](const std::string& phase,uint64_t){if(phase == "before_database_validation") cancelled = true;};
    rejects([&]{validate_direct_restore_attempt(fixture.spec,space_target,space_attempt,cancel);});
    check(record(space_attempt / "state.json").at("state") == "validating");
    check(validate_direct_restore_attempt(fixture.spec,space_target,space_attempt).state == "verified");
    rejects([&]{activate_direct_restore_attempt(fixture.spec,space_target,space_attempt,no_space);});
    check(!fs::exists(space_target));
    const auto experimental_target = root / "experimental-target", experimental_attempt = root / "experimental-attempt";
    DirectStageOptions experimental; experimental.reserve_profile = RestoreReserveProfile::experimental_8gib;
    RestoreBudgetPolicy experimental_policy; experimental_policy.reserve_profile = experimental.reserve_profile;
    const auto experimental_budget = estimate_mode_restore_space(fixture.spec,RestoreMode::direct,experimental_policy);
    experimental.available_bytes = [&](const auto&){return experimental_budget.recommended_free_bytes;};
    check(stage_direct_restore_snapshot(fixture.spec,fixture.source,experimental_target,experimental_attempt,false,experimental).download_complete);
    rejects([&]{validate_direct_restore_attempt(fixture.spec,experimental_target,experimental_attempt);});
    experimental.available_bytes = [&](const auto&){return experimental_budget.reserve_bytes
      + experimental_budget.validation_allowance_bytes + experimental_budget.metadata_journal_bytes - 1;};
    rejects([&]{validate_direct_restore_attempt(fixture.spec,experimental_target,experimental_attempt,experimental);});
    check(record(experimental_attempt / "state.json").at("state") == "download_complete");
    experimental.available_bytes = [&](const auto&){return experimental_budget.reserve_bytes
      + experimental_budget.validation_allowance_bytes + experimental_budget.metadata_journal_bytes;};
    check(validate_direct_restore_attempt(fixture.spec,experimental_target,experimental_attempt,experimental).state == "verified");
    rejects([&]{activate_direct_restore_attempt(fixture.spec,experimental_target,experimental_attempt);});
    check(activate_direct_restore_attempt(fixture.spec,experimental_target,experimental_attempt,experimental).observer_hold);
    const auto duplicate_target = root / "duplicate-target", a1 = root / "duplicate-attempt1", a2 = root / "duplicate-attempt2";
    verified(fixture,duplicate_target,a1); verified(fixture,duplicate_target,a2);
    { StorageLease lease(duplicate_target);
      rejects([&]{validate_direct_restore_attempt(fixture.spec,duplicate_target,a1);});
      rejects([&]{activate_direct_restore_attempt(fixture.spec,duplicate_target,a1);});
    }
    check(activate_direct_restore_attempt(fixture.spec,duplicate_target,a1).state == "activated");
    rejects([&]{activate_direct_restore_attempt(fixture.spec,duplicate_target,a2);});
    Fixture mismatch(root / "mismatch-fixture",false,true,true);
    rejects([&]{read_checkpoint_identity(root / "mismatch-fixture/checkpoint/db",root / "mismatch-fixture/checkpoint/chain/genesis_data.json");});
    staged(mismatch,root / "mismatch-target",root / "mismatch-attempt");
    rejects([&]{validate_direct_restore_attempt(mismatch.spec,root / "mismatch-target",root / "mismatch-attempt");});
    check(record(root / "mismatch-attempt/state.json").at("state") == "validating");
    rejects([&]{stage_direct_restore_snapshot(mismatch.spec,mismatch.source,root / "mismatch-target",root / "mismatch-attempt",true);});
    Fixture wal(root / "wal-fixture",true,true);
    const auto wal_identity=read_checkpoint_identity(root / "wal-fixture/checkpoint/db",root / "wal-fixture/checkpoint/chain/genesis_data.json");
    check(wal_identity.at("state_revision")==1 && wal_identity.at("highest_stored_height")==1);
    verified(wal,root / "wal-target",root / "wal-attempt");
    check(activate_direct_restore_attempt(wal.spec,root / "wal-target",root / "wal-attempt").state == "activated");
    check(record(root / "wal-attempt/validation.json").at("validation").at("wal_input_bytes").get<uint64_t>() > 0,"real WAL recovery exercised");
    auto no_chain = fixture.input; no_chain.expected_chain_id.clear(); const auto no_chain_spec = validate_restore_metadata(no_chain);
    // The validator independently refuses an unreviewed chain identity.
    rejects([&]{validate_offline_restore_database(no_chain_spec,root / "success-target");});
    auto wrong_chain = fixture.input; auto wrong_manifest = json::parse(wrong_chain.manifest);
    wrong_chain.expected_chain_id = "0x1220" + std::string(64,'0');
    wrong_manifest["public_bootstrap"]["chain_id"] = wrong_chain.expected_chain_id; wrong_chain.manifest = wrong_manifest.dump();
    rejects([&]{validate_offline_restore_database(validate_restore_metadata(wrong_chain),root / "success-target");});
    const auto missing_cf = root / "missing-cf"; fs::create_directories(missing_cf);
    write(missing_cf / "chain/genesis_data.json",read(root / "fixture/checkpoint/chain/genesis_data.json"));
    rocksdb::Options empty_options; empty_options.create_if_missing = true; rocksdb::DB* empty_db = nullptr;
    status(rocksdb::DB::Open(empty_options,(missing_cf / "db").string(),&empty_db));
    status(empty_db->Close()); delete empty_db;
    rejects([&]{validate_offline_restore_database(fixture.spec,missing_cf);});
    json summary{{"checks",checks},{"activation_boundaries",boundaries},{"validation_boundaries",validation_boundaries},
      {"validation_crash_cases",validation_boundaries.size()},{"validation_exception_cases",validation_boundaries.size()},
      {"crash_cases",boundaries.size()},
      {"exception_cases",boundaries.size()},{"fixture_chain_id",fixture.input.expected_chain_id},
      {"manifest_sha256",fixture.spec.data().manifest_sha256},{"inventory_sha256",fixture.spec.data().inventory_sha256},
      {"mainnet_replay",false},{"capacity_40gb_qualified",false}};
    write(root / "summary.json",summary.dump(2));
    std::cout << "SC3 passed " << checks << " checks; " << boundaries.size() << " boundaries in each fault mode; artifacts " << root << '\n';
    return 0;
  }
  catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
