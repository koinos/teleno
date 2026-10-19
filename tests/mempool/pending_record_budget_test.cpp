#include <koinos/mempool/mempool.hpp>
#include <koinos/chain/value.pb.h>
#include <koinos/util/conversion.hpp>
#include <koinos/state_db/state_delta.hpp>
#include <google/protobuf/unknown_field_set.h>
#include "../artifact_root.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>
#include <unistd.h>

static thread_local long fail_at=-1;
static thread_local bool fired=false, count_new=false;
static thread_local size_t allocations=0;
void* operator new(std::size_t n) {
  if(count_new)++allocations;
  if(fail_at>=0 && fail_at--==0){fail_at=-1;fired=true;throw std::bad_alloc();}
  if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace koinos;
using Budget=mempool::pending_record_budget;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
auto value(Budget& b,size_t n=128){return b.encode(n,[=]{return std::string(n,'x');});}
template<class F>void refused(F&& f){bool rejected=false;try{f();}catch(const node::LocalCapacityExceeded& e){rejected=std::string(e.reason_code())=="pending_record_capacity";}require(rejected,"record refusal not classified locally");}
auto id(uint64_t n){return crypto::hash(crypto::multicodec::sha2_256,n);}
auto at(){return std::chrono::system_clock::time_point{std::chrono::seconds{1000}};}
void block(mempool::mempool& pool,uint64_t n,uint64_t parent,uint64_t height,const std::string& included={}){
  broadcast::block_accepted b;b.mutable_block()->set_id(util::converter::as<std::string>(id(n)));
  b.mutable_block()->mutable_header()->set_previous(util::converter::as<std::string>(parent ? id(parent) : crypto::multihash::zero(crypto::multicodec::sha2_256)));
  b.mutable_block()->mutable_header()->set_height(height);
  if(!included.empty())b.mutable_block()->add_transactions()->set_id(included);
  require(pool.handle_block(b),"fixture block not linked");
}
void lib(mempool::mempool& pool,uint64_t n){broadcast::block_irreversible b;b.mutable_topology()->set_id(util::converter::as<std::string>(id(n)));pool.handle_irreversibility(b);}
auto transaction(uint64_t n,bool same_payer=false){
  mempool::pending_transaction p;auto* tx=p.mutable_transaction();tx->set_id(std::string(40,'i')+std::to_string(n));
  tx->mutable_header()->set_payer(std::string(40,'p')+(same_payer?"":std::to_string(n)));
  tx->mutable_header()->set_rc_limit(100);chain::value_type nonce;nonce.set_uint64_value(1);
  tx->mutable_header()->set_nonce(util::converter::as<std::string>(nonce));tx->add_signatures(std::string(8192,'s'));return p;
}
void initialize(mempool::mempool& p){block(p,1,0,1);block(p,2,1,2);block(p,3,1,2);}
auto snapshot(mempool::mempool& p){std::vector<std::string> out;for(const auto& checkpoint:{crypto::multihash::zero(crypto::multicodec::sha2_256),id(1),id(2),id(3)})for(const auto& v:p.get_pending_transactions(2000,checkpoint))out.push_back(v.SerializeAsString());return out;}

void test_budget_axes_and_lifetime(){
  Budget disabled;count_new=true;allocations=0;require(!disabled.snapshot().enabled,"default budget enabled");count_new=false;require(!allocations,"disabled accounting allocated");
  auto untracked=value(disabled);require(!disabled.snapshot().owners,"disabled accounting claims tracked owners");
  Budget count({2,0});auto a=value(count), b=value(count);require(a!=b && count.snapshot().owners==2,"equal bytes merged or count incorrect");
  auto alias=a;refused([&]{value(count);});a.reset();require(count.snapshot().owners==2,"alias prematurely released credits");alias.reset();require(count.snapshot().owners==1,"last alias did not release credits");
  auto c=value(count);b.reset();c.reset();require(!count.snapshot().owners&&!count.snapshot().capacity_bytes,"credits leaked");
  const auto capacity=std::string(128,'x').capacity();Budget bytes({0,2*capacity});a=value(bytes);b=value(bytes);
  require(bytes.snapshot().capacity_bytes==2*capacity,"actual capacities not charged");refused([&]{value(bytes);});a.reset();c=value(bytes);b.reset();c.reset();
  Budget growth({0,128});refused([&]{growth.encode(128,[]{std::string s(128,'x');s.reserve(1024);return s;});});require(!growth.snapshot().owners&&!growth.snapshot().capacity_bytes,"failed capacity growth leaked credits");
  bool encoded=false;refused([&]{growth.encode(129,[&]{encoded=true;return std::string(129,'x');});});require(!encoded,"over-limit wire encoded before refusal");
  Budget overflow({1,0});bool mismatch=false;
  try{overflow.encode(std::numeric_limits<uint64_t>::max(),[&]{refused([&]{value(overflow);});return std::string{};});}catch(const std::logic_error&){mismatch=true;}
  require(mismatch&&!overflow.snapshot().owners&&!overflow.snapshot().capacity_bytes,"overflow/mismatch leaked credits");
  std::shared_ptr<const std::string> held;{Budget short_lived({1,0});held=value(short_lived);}
  require(*held==std::string(128,'x'),"record outliving budget became invalid");held.reset();
}
void test_factory_faults_and_concurrency(){
  size_t failures=0;
  for(size_t ordinal=0;ordinal<100;++ordinal){Budget b({1,4096});bool rejected=false;std::shared_ptr<const std::string> v;
    fail_at=ordinal;fired=false;try{v=value(b);}catch(const std::bad_alloc&){rejected=true;}fail_at=-1;
    require(rejected==fired,"factory swallowed allocation failure");
    if(!rejected){require(b.snapshot().owners==1,"successful factory not charged");v.reset();require(!b.snapshot().owners,"factory release failed");break;}
    ++failures;require(!b.snapshot().owners&&!b.snapshot().capacity_bytes,"factory allocation failure leaked credits");
    v=value(b);v.reset();require(!b.snapshot().owners,"factory retry leaked");require(ordinal!=99,"factory sweep incomplete");
  }
  require(failures>0,"factory failures not injected");
  Budget concurrent({4,1024});std::atomic<bool> failed=false;std::vector<std::thread> threads;
  try{for(int n=0;n<8;++n)threads.emplace_back([&]{for(int i=0;i<500;++i)try{auto v=value(concurrent);require(*v==std::string(128,'x'),"concurrent value changed");std::this_thread::yield();}catch(const node::LocalCapacityExceeded&){}catch(...){failed=true;}});}
  catch(...){for(auto& t:threads)t.join();throw;}
  for(auto& t:threads)t.join();const auto u=concurrent.snapshot();require(!failed&&!u.owners&&!u.capacity_bytes&&u.peak_owners<=4&&u.peak_capacity_bytes<=1024,"concurrent credits escaped limits");
  std::cout<<"factory_faults="<<failures<<" concurrent_peak_owners="<<u.peak_owners<<"\n";
}
void test_map_squash_lib_and_persistence(){
  Budget budget({2,0});state_db::database db;auto lock=db.get_unique_lock();db.open({},[](auto){},state_db::fork_resolution_algorithm::fifo,lock);
  auto a=db.create_writable_node(db.get_head(lock)->id(),id(500),{},lock), expected=db.create_writable_node(db.get_head(lock)->id(),id(501),{},lock);
  auto first=value(budget);const state_db::object_space space;
  require(a->put_object_owned(space,"key",first)==expected->put_object(space,"key",first.get()),"owned insertion changed byte accounting");
  require(a->pending_merkle_root()==expected->pending_merkle_root()&&a->get_object(space,"key")==first,"owned insertion changed root/owner");
  auto overlay=a->create_anonymous_node();auto second=value(budget,256);overlay->put_object_owned(space,"key",second);overlay->commit();
  require(a->get_object(space,"key")==second,"ordinary squash dropped charged owner");
  db.finalize_node(a->id(),lock);db.commit_node(a->id(),lock);
  require(db.get_root(lock)->get_object(space,"key")==second&&budget.snapshot().owners==2,"map LIB copied/lost charged owner");
  bool rejected=false;try{db.get_root(lock)->put_object_owned(space,"x",{});}catch(const std::exception&){rejected=true;}require(rejected,"invalid owned insertion accepted");
  db.close(lock);a.reset();expected.reset();overlay.reset();first.reset();require(budget.snapshot().owners==1&&*second==std::string(256,'x'),"caller-only owner not charged/valid after close");second.reset();require(!budget.snapshot().owners,"caller-only record not released");
  const auto root=test_artifact_root(TELENO_LR2_REPOSITORY_ROOT);auto pattern=(root/"m1g-owned-db-XXXXXX").string();std::vector<char> name(pattern.begin(),pattern.end());name.push_back('\0');auto created=::mkdtemp(name.data());require(created,"scratch DB creation failed");const std::filesystem::path dir(created);require(test_artifact_child(dir,root),"persistent fixture outside selected root");
  auto data=std::make_shared<const std::string>(4096,'d');crypto::multihash committed;
  for(int pass=0;pass<2;++pass){state_db::database persistent;auto l=persistent.get_unique_lock();persistent.open(dir,[](auto){},state_db::fork_resolution_algorithm::fifo,l);
    if(!pass){auto n=persistent.create_writable_node(persistent.get_head(l)->id(),id(600),{},l);n->put_object_owned(space,"persisted",data);committed=n->pending_merkle_root();persistent.finalize_node(n->id(),l);persistent.commit_node(n->id(),l);}
    require(*persistent.get_root(l)->get_object(space,"persisted")==*data&&persistent.get_root(l)->merkle_root()==committed,"owned persistent fallback changed bytes/root/reopen");persistent.close(l);}
  std::cout<<"persistent_fixture="<<dir.string()<<"\n";
}
void test_mempool_ancestors_and_capacity(){
  mempool::mempool p(state_db::fork_resolution_algorithm::fifo,{2,0});initialize(p);p.add_pending_transaction(transaction(1),at(),1000);p.add_pending_transaction(transaction(2),at(),1000);
  const auto before=snapshot(p);const auto usage=p.get_pending_record_usage();const auto retained=p.get_retained_memory_usage().indexed_state;
  require(usage.owners==2&&retained.unique_value_size_bytes>=2*8192&&retained.unique_value_size_bytes<3*8192,"record shared ownership absent across forks");
  refused([&]{p.add_pending_transaction(transaction(3),at(),1000);});require(snapshot(p)==before&&p.get_pending_record_usage().owners==2,"capacity refusal changed parents/credits");
  require(p.prune(std::chrono::seconds{1},at()+std::chrono::seconds{5})==2&&!p.pending_transaction_count(),"expiry did not empty heads");
  require(p.get_pending_record_usage().owners==2&&p.has_pending_transaction(transaction(1).transaction().id(),id(1)),"empty head freed retained ancestor credits");refused([&]{p.add_pending_transaction(transaction(3),at(),1000);});
  lib(p,2);require(!p.get_pending_record_usage().owners&&!p.get_pending_record_usage().capacity_bytes,"LIB did not release final retained records");p.add_pending_transaction(transaction(3),at(),1000);require(p.get_pending_record_usage().owners==1,"capacity did not recover after LIB");
  mempool::mempool survives(state_db::fork_resolution_algorithm::fifo,{1,0});initialize(survives);survives.add_pending_transaction(transaction(1),at(),1000);lib(survives,2);
  require(survives.get_pending_record_usage().owners==1&&survives.pending_transaction_count()==1,"LIB lost active record credit/data");survives.remove_pending_transactions({transaction(1).transaction().id()});require(!survives.get_pending_record_usage().owners,"last head removal leaked record credit");
  mempool::mempool fork(state_db::fork_resolution_algorithm::fifo,{3,0});auto original=transaction(1,true);fork.add_pending_transaction(original,at(),1000);block(fork,1,0,1,original.transaction().id());block(fork,2,0,1);auto replacement=transaction(2,true);fork.add_pending_transaction(replacement,at(),1000);
  require(fork.get_pending_record_usage().owners==2&&fork.has_pending_transaction(replacement.transaction().id(),id(1))&&!fork.has_pending_transaction(replacement.transaction().id(),id(2)),"fork-specific nonce/counter behavior changed");
  bool duplicate=false;try{fork.add_pending_transaction(replacement,at(),1000);}catch(const std::exception&){duplicate=true;}require(duplicate&&fork.get_pending_record_usage().owners==2,"duplicate leaked credits");
}
void test_enabled_admission_faults(){
  size_t rejected=0,successful_faults=0;
  for(size_t ordinal=0;ordinal<1200;++ordinal){mempool::mempool p(state_db::fork_resolution_algorithm::fifo,{2,1<<20});initialize(p);p.add_pending_transaction(transaction(1),at(),1000);const auto before=snapshot(p);const auto u=p.get_pending_record_usage();const auto candidate=transaction(2);bool failed=false;
    fail_at=ordinal;fired=false;try{p.add_pending_transaction(candidate,at(),1000);}catch(...){failed=true;}fail_at=-1;const bool injected=fired;
    if(failed){++rejected;require(snapshot(p)==before&&p.get_pending_record_usage().owners==u.owners&&p.get_pending_record_usage().capacity_bytes==u.capacity_bytes,"rejected admission changed data/live credits");p.add_pending_transaction(candidate,at(),1000);}
    else if(injected)++successful_faults;
    require(p.get_pending_record_usage().owners==2,"published candidate missing/live credits escaped");for(const auto& checkpoint:{crypto::multihash::zero(crypto::multicodec::sha2_256),id(1),id(2),id(3)})require(p.has_pending_transaction(candidate.transaction().id(),checkpoint),"candidate partially published");
    if(!injected){std::cout<<"admission_rejected="<<rejected<<" post_publish_faults="<<successful_faults<<"\n";return;}require(ordinal!=1199,"enabled admission sweep incomplete");
  }
}

void test_complete_pending_message_roundtrip(){
  auto candidate=transaction(700);
  candidate.GetReflection()->MutableUnknownFields(&candidate)->AddVarint(103,17);
  auto* tx=candidate.mutable_transaction();
  tx->add_signatures("");tx->add_signatures(std::string(16384,'t'));
  tx->GetReflection()->MutableUnknownFields(tx)->AddLengthDelimited(104,"unknown transaction bytes");
  auto* header=tx->mutable_header();
  header->GetReflection()->MutableUnknownFields(header)->AddFixed64(105,1234);
  auto* operation=tx->add_operations();
  operation->GetReflection()->MutableUnknownFields(operation)->AddVarint(106,29);
  const auto expected=candidate.SerializeAsString();
  mempool::mempool pool(state_db::fork_resolution_algorithm::fifo,{2,1<<20});initialize(pool);
  pool.add_pending_transaction(candidate,at(),1000);
  require(candidate.SerializeAsString()==expected,"admission changed the caller's message");
  for(const auto& checkpoint:{crypto::multihash::zero(crypto::multicodec::sha2_256),id(1),id(2),id(3)}){
    const auto records=pool.get_pending_transactions(10,checkpoint);
    require(records.size()==1&&records.front().SerializeAsString()==expected,"admission lost repeated or unknown protobuf fields");
  }
}

void test_mempool_byte_axis_and_concurrent_admission(){
  mempool::mempool probe(state_db::fork_resolution_algorithm::fifo,{1,0});
  probe.add_pending_transaction(transaction(1),at(),1000);
  const auto capacity=probe.get_pending_record_usage().capacity_bytes;
  mempool::mempool bytes(state_db::fork_resolution_algorithm::fifo,{0,capacity});initialize(bytes);
  bytes.add_pending_transaction(transaction(1),at(),1000);const auto before=snapshot(bytes);
  require(bytes.get_pending_record_usage().capacity_bytes==capacity,"single record capacity differs across forks");
  refused([&]{bytes.add_pending_transaction(transaction(2),at(),1000);});
  require(snapshot(bytes)==before&&bytes.get_pending_record_usage().capacity_bytes==capacity,"byte refusal changed data/credits");
  bytes.prune(std::chrono::seconds{1},at()+std::chrono::seconds{5});
  require(bytes.get_pending_record_usage().capacity_bytes==capacity,"cleanup at byte ceiling lost ancestor credits");
  lib(bytes,2);require(!bytes.get_pending_record_usage().capacity_bytes,"byte credits not returned after LIB");
  bytes.add_pending_transaction(transaction(2),at(),1000);

  mempool::mempool pool(state_db::fork_resolution_algorithm::fifo,{4,0});initialize(pool);
  std::atomic<size_t> accepted=0, refused_count=0;std::atomic<bool> failed=false;std::vector<std::thread> workers;
  try{for(uint64_t n=0;n<8;++n)workers.emplace_back([&,n]{for(uint64_t i=0;i<10;++i)try{
    pool.add_pending_transaction(transaction(100+n*10+i),at(),1000);++accepted;
  }catch(const node::LocalCapacityExceeded& e){if(std::string(e.reason_code())!="pending_record_capacity")failed=true;++refused_count;}
   catch(...){failed=true;}});}
  catch(...){for(auto& worker:workers)worker.join();throw;}
  for(auto& worker:workers)worker.join();const auto usage=pool.get_pending_record_usage();
  require(!failed&&accepted==4&&refused_count==76&&pool.pending_transaction_count()==4&&usage.owners==4&&usage.peak_owners==4&&usage.refusals==76,"concurrent native admission exceeded limit or misclassified refusal");
  for(const auto& checkpoint:{crypto::multihash::zero(crypto::multicodec::sha2_256),id(1),id(2),id(3)})
    require(pool.get_pending_transactions(2000,checkpoint).size()==4,"concurrent native admission partially published");
  std::cout<<"concurrent_native_accepted="<<accepted<<" concurrent_native_refused="<<refused_count<<" byte_capacity="<<capacity<<"\n";
}
}
int main(){try{test_budget_axes_and_lifetime();test_factory_faults_and_concurrency();test_map_squash_lib_and_persistence();test_mempool_ancestors_and_capacity();test_enabled_admission_faults();test_complete_pending_message_roundtrip();test_mempool_byte_axis_and_concurrent_admission();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
