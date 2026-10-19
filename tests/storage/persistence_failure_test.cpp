#include "storage/rocksdb_manager.hpp"
#include "block_store/block_store.hpp"
#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>
#include <koinos/state_db/state_delta.hpp>
#include "fault_db.hpp"
#include "../artifact_root.hpp"
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace koinos::node;
using Backend = koinos::state_db::backends::rocksdb::rocksdb_backend;
std::string executable;
void check(bool ok) { if(!ok) throw std::runtime_error("persistence assertion failed"); }
template<typename F> void rejects(F f) { bool threw=false; try { f(); } catch(const std::exception&) { threw=true; } check(threw); }
struct Fixture
{
  fs::path path;
  storage::RocksDBManager manager;
  StorageFaults faults;
  std::unique_ptr<rocksdb::DB> db;
  Fixture()
  {
    path=test_artifact_root(TELENO_LR1_REPOSITORY_ROOT)/
      ("lr1-storage-"+std::to_string(std::random_device{}()));
    fs::create_directories(path.parent_path()); check(fs::create_directory(path));
    NodeConfig cfg; cfg.rocksdb_block_cache_mb=16; manager.open(path,cfg);
    db.reset(make_fault_database(manager.db(),faults));
    std::cout << "retained fixture: " << path << '\n';
  }
  void open(Backend& b) { b.open(*db,*manager.handle(storage::ColumnFamily::default_state),
    *manager.handle(storage::ColumnFamily::chain_state),*manager.handle(storage::ColumnFamily::chain_metadata)); }
};
void state_read()
{
  Fixture f; Backend b; f.open(b);
  check(!b.get("absent")); f.faults.fail_read=true;
  rejects([&]{b.get("unread-key");}); f.faults.fail_read=false;
  check(!b.get("unread-key")); b.put("key","value"); check(*b.get("key")=="value"); b.close();
}
void state_flush()
{
  for(int n: {1,2}) { Fixture f; Backend b; f.open(b); b.put("a","b");
    f.faults.fail_flush=n; rejects([&]{b.flush();}); f.faults.fail_flush=0; b.close(); }
}
void failed_commit()
{
  Fixture f; auto b=std::make_shared<Backend>(); f.open(*b); b->put("original","value"); b->flush();
  auto root=std::make_shared<koinos::state_db::detail::state_delta>(b);
  auto child=root->make_child(); child->put("new","uncommitted");
  f.faults.fail_write=true; rejects([&]{child->commit();});
  check(root->backend()!=nullptr); check(!child->is_root());
  rejects([&]{root->find("new");}); rejects([&]{child->commit();});
  f.faults.fail_write=false; rejects([&]{b->close();});
  root.reset(); child.reset(); b.reset();
  Backend reopened; f.open(reopened); check(!reopened.get("new"));
  check(*reopened.get("original")=="value"); reopened.close();
}
koinos::rpc::block_store::add_block_request request(const std::string& id,uint64_t height=1,const std::string& previous="genesis")
{
  koinos::rpc::block_store::add_block_request r;
  r.mutable_block_to_add()->set_id(id); auto* h=r.mutable_block_to_add()->mutable_header();
  h->set_height(height); h->set_previous(previous);
  r.mutable_receipt_to_add()->set_id(id); return r;
}
bool stored(block_store::BlockStore& store,const std::string& id)
{
  koinos::rpc::block_store::get_blocks_by_id_request r; r.add_block_ids(id); r.set_return_receipt(true);
  auto result=store.get_blocks_by_id(r); if(result.block_items_size()) check(result.block_items(0).has_receipt());
  return result.block_items_size()==1;
}
void block_atomic()
{
  for(bool ambiguous: {false,true})
  {
    Fixture f; auto blocks=f.manager.handle(storage::ColumnFamily::blocks),meta=f.manager.handle(storage::ColumnFamily::block_meta);
    block_store::BlockStore store(f.db.get(),blocks,meta); store.initialize();
    f.faults.fail_put=meta; f.faults.fail_write=true; f.faults.committed_error=ambiguous;
    rejects([&]{store.add_block(request("one"));});
    check(stored(store,"one")==ambiguous); check(store.get_highest_block({}).topology().height()==uint64_t(ambiguous));
    f.faults.fail_put=nullptr; f.faults.fail_write=false; store.add_block(request("one"));
    check(stored(store,"one")); check(store.get_highest_block({}).topology().id()=="one");
    store.add_block(request("fork")); check(store.get_highest_block({}).topology().id()=="one");
    store.add_block(request("two",2,"one")); check(store.get_highest_block({}).topology().height()==2);
    f.db.reset(); f.manager.close(); NodeConfig cfg; f.manager.open(f.path,cfg);
    block_store::BlockStore reopen(f.manager.db(),f.manager.handle(storage::ColumnFamily::blocks),f.manager.handle(storage::ColumnFamily::block_meta));
    reopen.initialize(); check(stored(reopen,"one")&&stored(reopen,"fork")&&stored(reopen,"two"));
    check(reopen.get_highest_block({}).topology().height()==2);
  }
}
void corrupt_topology()
{
  Fixture f; auto meta=f.manager.handle(storage::ColumnFamily::block_meta);
  check(f.manager.db()->Put({},meta,std::string(1,'\1'),"invalid-protobuf").ok());
  block_store::BlockStore store(f.db.get(),f.manager.handle(storage::ColumnFamily::blocks),meta);
  rejects([&]{store.initialize();});
  std::string value; check(f.manager.db()->Get({},meta,std::string(1,'\1'),&value).ok()&&value=="invalid-protobuf");
}
void cache_clear()
{
  koinos::state_db::backends::rocksdb::object_cache cache(100);
  for(int i=0;i<3;++i) {
    cache.put("key",std::make_shared<const std::string>(60,'x'));
    check(cache.get("key").first); cache.clear(); check(!cache.get("key").first);
  }
}
void initialization_failure()
{
  Fixture f; Backend b; f.faults.fail_write=true; rejects([&]{f.open(b);});
  auto* meta=f.manager.handle(storage::ColumnFamily::chain_metadata);
  std::unique_ptr<rocksdb::Iterator> it(f.manager.db()->NewIterator({},meta));
  it->SeekToFirst(); check(it->status().ok() && !it->Valid()); it.reset();
  f.faults.fail_write=false; f.open(b); b.close();
  check(f.manager.db()->Delete({},meta,"size").ok());
  rejects([&]{f.open(b);});
  std::string value; check(f.manager.db()->Get({},meta,"size",&value).IsNotFound());
  check(f.manager.db()->Get({},meta,"id",&value).ok());
}
void interrupted_block()
{
#if !defined(_WIN32)
  for(bool complete:{false,true}) {
    Fixture f;
    block_store::BlockStore store(f.manager.db(),f.manager.handle(storage::ColumnFamily::blocks),f.manager.handle(storage::ColumnFamily::block_meta));
    store.initialize(); f.db.reset(); f.manager.close();
    const auto child=::fork(); check(child>=0);
    if(child==0) { ::execl(executable.c_str(),executable.c_str(),"--block-crash",f.path.c_str(),complete ? "complete":"before",nullptr); ::_exit(4); }
    int status=0; check(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==99);
    NodeConfig cfg; f.manager.open(f.path,cfg);
    block_store::BlockStore reopen(f.manager.db(),f.manager.handle(storage::ColumnFamily::blocks),f.manager.handle(storage::ColumnFamily::block_meta));
    reopen.initialize(); check(stored(reopen,"one")==complete);
    check(reopen.get_highest_block({}).topology().height()==uint64_t(complete));
  }
#endif
}
int main(int argc,char** argv)
{
  executable=fs::absolute(argv[0]).string();
#if !defined(_WIN32)
  if(argc==4 && std::string(argv[1])=="--block-crash") {
    const auto root=test_artifact_root(TELENO_LR1_REPOSITORY_ROOT);
    const auto path=fs::canonical(argv[2]); check(path.parent_path()==root && path.filename().string().rfind("lr1-storage-",0)==0);
    storage::RocksDBManager manager; NodeConfig cfg; manager.open(path,cfg);
    StorageFaults faults; std::unique_ptr<rocksdb::DB> db(make_fault_database(manager.db(),faults));
    block_store::BlockStore store(db.get(),manager.handle(storage::ColumnFamily::blocks),manager.handle(storage::ColumnFamily::block_meta));
    store.initialize(); faults.fail_write=true; faults.committed_error=std::string(argv[3])=="complete";
    rejects([&]{store.add_block(request("one"));}); ::_exit(99);
  }
#endif
  check(argc==1);
  int failures=0;
  for(auto test: {state_read,state_flush,failed_commit,block_atomic,corrupt_topology,cache_clear,initialization_failure,interrupted_block})
    try { test(); } catch(const std::exception& e) { ++failures; std::cerr << e.what() << '\n'; }
  std::cout << "LR1 persistence groups: " << (8-failures) << "/8\n";
  return failures ? 1 : 0;
}
