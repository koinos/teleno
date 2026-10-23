#include "core/owned_allocation.hpp"
#include <koinos/mempool/mempool.hpp>
#include <koinos/chain/value.pb.h>
#include <koinos/util/conversion.hpp>
#include <google/protobuf/unknown_field_set.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
#include <sys/resource.h>

// Ordinary calling-thread C++ new/new[], not dependency malloc/aligned new,
// real machine OOM, simultaneous failures or integrated race instrumentation.
static thread_local long fail_at = -1;
static thread_local size_t allocations = 0, requested_bytes = 0;
static thread_local bool fired = false;
void* operator new(std::size_t n) {
  if (fail_at >= 0) {
    ++allocations; requested_bytes += n;
    if (fail_at-- == 0) { fail_at = -1; fired = true; throw std::bad_alloc(); }
  }
  if (auto p = koinos::node::owned_allocation::allocate(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { koinos::node::owned_allocation::deallocate(p); }
void operator delete[](void* p) noexcept { koinos::node::owned_allocation::deallocate(p); }
void operator delete(void* p, std::size_t) noexcept { koinos::node::owned_allocation::deallocate(p); }
void operator delete[](void* p, std::size_t) noexcept { koinos::node::owned_allocation::deallocate(p); }

namespace {
using namespace koinos;
void require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
auto id(uint64_t n) { return crypto::hash(crypto::multicodec::sha2_256, n); }
auto at(uint64_t seconds) { return std::chrono::system_clock::time_point{std::chrono::seconds{seconds}}; }
mempool::pending_transaction transaction(uint64_t nonce) {
  mempool::pending_transaction pending;
  auto* tx = pending.mutable_transaction();
  tx->set_id(std::string(40, 'i') + std::to_string(nonce));
  tx->mutable_header()->set_payer(std::string(40, 'p'));
  tx->mutable_header()->set_rc_limit(100);
  chain::value_type value; value.set_uint64_value(nonce);
  tx->mutable_header()->set_nonce(util::converter::as<std::string>(value));
  tx->add_signatures(std::string(1024, 's'));
  pending.GetReflection()->MutableUnknownFields(&pending)->AddLengthDelimited(222, "cleanup pending field");
  tx->GetReflection()->MutableUnknownFields(tx)->AddVarint(223, 98765);
  auto* header = tx->mutable_header();
  header->GetReflection()->MutableUnknownFields(header)->AddLengthDelimited(224, std::string(32, '\0'));
  return pending;
}
void block(mempool::mempool& pool, uint64_t n, uint64_t parent, uint64_t height) {
  broadcast::block_accepted accepted;
  accepted.mutable_block()->set_id(util::converter::as<std::string>(id(n)));
  accepted.mutable_block()->mutable_header()->set_previous(util::converter::as<std::string>(id(parent)));
  accepted.mutable_block()->mutable_header()->set_height(height);
  require(pool.handle_block(accepted), "fixture block not linked");
}
void initialize(mempool::mempool& pool) {
  block(pool, 1, 0, 1); block(pool, 2, 1, 2); block(pool, 3, 1, 2);
}
std::vector<std::string> snapshot(mempool::mempool& pool) {
  std::vector<std::string> result;
  const std::string payer(40, 'p');
  for (const auto& checkpoint : {crypto::multihash::zero(crypto::multicodec::sha2_256), id(1), id(2), id(3)}) {
    for (const auto& pending : pool.get_pending_transactions(2000, checkpoint)) result.push_back(pending.SerializeAsString());
    result.push_back(pool.get_pending_nonce(payer, checkpoint));
    result.push_back(std::to_string(pool.get_pending_transaction_count(payer, checkpoint)));
    for (uint64_t rc : {701, 801, 901})
      result.push_back(pool.check_pending_account_resources(payer, 1000, rc, checkpoint) ? "available" : "reserved");
    for (uint64_t n : {1, 2, 3}) result.push_back(pool.has_pending_transaction(transaction(n).transaction().id(), checkpoint) ? "present" : "absent");
  }
  result.push_back(std::to_string(pool.get_reserved_account_rc(payer)));
  return result;
}
auto fields(const mempool::retained_memory_usage& m) {
  const auto& u = m.indexed_state;
  return std::tuple(u.state_nodes, u.finalized_state_nodes, u.writable_state_nodes,
    u.map_backends, u.unmeasured_backends, u.map_entries, u.map_key_size_bytes,
    u.map_key_capacity_bytes, u.value_references, u.unique_values,
    u.unique_value_size_bytes, u.unique_value_capacity_bytes, u.tombstone_entries,
    u.tombstone_key_size_bytes, u.tombstone_key_capacity_bytes, u.tombstone_bucket_count, m.node_mutexes);
}
uint64_t cleanup(mempool::mempool& pool, bool prune, const std::vector<std::string>& ids) {
  if (prune) return pool.prune(std::chrono::seconds{10}, at(1015));
  return pool.remove_pending_transactions(ids);
}
struct Case { bool injected, valid, rejected; size_t attempts; };
Case fault_case(bool prune, size_t ordinal) {
  mempool::mempool pool; initialize(pool);
  pool.add_pending_transaction(transaction(1), at(1000), 1000);
  pool.add_pending_transaction(transaction(2), at(1000), 1000);
  pool.add_pending_transaction(transaction(3), at(1020), 1000);
  // Prepare caller inputs before enabling allocation failure. Constructing a
  // protobuf transaction here inside the fault window can fail in the fixture
  // before the cleanup operation is entered, invalidating the intended test.
  const std::vector<std::string> ids{transaction(1).transaction().id(), transaction(2).transaction().id(),
    transaction(1).transaction().id(), "not-present"};
  const auto before = snapshot(pool);
  const auto inventory = fields(pool.get_retained_memory_usage());
  allocations = 0; fired = false; fail_at = static_cast<long>(ordinal);
  bool rejected = false; uint64_t removed = 0;
  try { removed = cleanup(pool, prune, ids); } catch (...) { rejected = true; }
  fail_at = -1;
  const auto attempts = allocations;
  const bool injected = fired;
  const bool unchanged = snapshot(pool) == before && fields(pool.get_retained_memory_usage()) == inventory;
  bool success = removed == 2;
  for (const auto& checkpoint : {id(2), id(3)}) {
    success &= !pool.has_pending_transaction(transaction(1).transaction().id(), checkpoint);
    success &= !pool.has_pending_transaction(transaction(2).transaction().id(), checkpoint);
    success &= pool.has_pending_transaction(transaction(3).transaction().id(), checkpoint);
    success &= pool.get_pending_transaction_count(std::string(40, 'p'), checkpoint) == 1;
    success &= pool.check_pending_account_resources(std::string(40, 'p'), 1000, 900, checkpoint);
    success &= !pool.check_pending_account_resources(std::string(40, 'p'), 1000, 901, checkpoint);
  }
  for (const auto& checkpoint : {crypto::multihash::zero(crypto::multicodec::sha2_256), id(1)})
    success &= pool.get_pending_transaction_count(std::string(40, 'p'), checkpoint) == 3;
  const bool valid = rejected ? unchanged : success;
  if (valid && rejected) require(cleanup(pool, prune, ids) == 2, "cleanup cannot retry after rejected preparation");
  if (valid) {
    // Current forks free removed nonces/RC, but ancestors still retain history.
    auto replacement = transaction(1); replacement.mutable_transaction()->set_id(std::string(40, 'r'));
    pool.add_pending_transaction(replacement, at(1030), 1000);
    require(pool.has_pending_transaction(replacement.transaction().id(), id(2)) && pool.has_pending_transaction(replacement.transaction().id(), id(3)), "freed nonce cannot be readmitted");
    require(!pool.has_pending_transaction(replacement.transaction().id(), id(1)), "conflicting ancestor accepted replacement");
  }
  if (!valid) std::cout << "invalid mode=" << (prune ? "prune" : "remove") << " ordinal=" << ordinal << " rejected=" << rejected << " unchanged=" << unchanged << std::endl;
  return {injected, valid, rejected, attempts};
}
void measure(bool prune) {
  mempool::mempool pool; initialize(pool);
  auto add = [&](unsigned first, unsigned count, uint64_t time, size_t signature_bytes) {
    for (unsigned n=first; n<first+count; ++n) {
      auto tx=transaction(n+1); tx.mutable_transaction()->mutable_header()->set_payer(std::string(40, 'p')+std::to_string(n));
      tx.mutable_transaction()->set_signatures(0, std::string(signature_bytes, 's'));
      pool.add_pending_transaction(tx, at(time), 1000);
    }
  };
  add(1000,32,900,1024); pool.prune(std::chrono::seconds{10},at(920)); // pre-existing markers
  add(0,64,1000,65536);
  std::vector<std::string> ids;
  for (unsigned n=0;n<64;++n) ids.push_back(transaction(n+1).transaction().id());
  allocations=requested_bytes=0; fail_at=1'000'000; fired=false;
  const auto begin=std::chrono::steady_clock::now();
  const auto removed=prune ? pool.prune(std::chrono::seconds{10},at(1015)) : pool.remove_pending_transactions(ids);
  const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count();
  fail_at=-1; const auto calls=allocations, bytes=requested_bytes;
  rusage usage{}; require(getrusage(RUSAGE_SELF,&usage)==0,"RSS query failed");
#if defined(__APPLE__)
  const uint64_t rss=usage.ru_maxrss;
#else
  const uint64_t rss=static_cast<uint64_t>(usage.ru_maxrss)*1024;
#endif
  require(removed==64 && pool.pending_transaction_count()==0 && !fired,"measurement input/results changed");
  std::cout << "{\"format\":\"lr2-m1f-cleanup\",\"mode\":\"" << (prune?"prune":"remove")
    << "\",\"records\":64,\"signature_bytes\":65536,\"prior_expired_records\":32,\"allocation_attempts\":" << calls
    << ",\"requested_cpp_allocation_bytes\":" << bytes << ",\"elapsed_microseconds\":" << us
    << ",\"process_peak_rss_bytes\":" << rss << "}" << std::endl;
}
} // namespace
int main(int argc, char** argv) {
  try {
    if (argc==3 && std::string(argv[1])=="--measure") { measure(std::string(argv[2])=="prune"); return 0; }
    if (argc==4 && std::string(argv[1])=="--case") {
      require(std::string(argv[2])=="remove" || std::string(argv[2])=="prune", "unknown cleanup mode");
      const auto c=fault_case(std::string(argv[2])=="prune", std::stoul(argv[3]));
      std::cout << "case injected=" << c.injected << " valid=" << c.valid << " rejected=" << c.rejected << " attempts=" << c.attempts << std::endl;
      return !c.valid ? 91 : !c.injected ? 92 : 0;
    }
    const bool audit=argc==2 && std::string(argv[1])=="--audit";
    size_t invalid=0;
    for (bool prune : {false,true}) {
      size_t cases=0, rejections=0, bad=0;
      // Cover the deeper anonymous read/iterator path until an unfailed call;
      // this is a bounded sweep, not a shortened sampling of allocation sites.
      constexpr size_t sweep_limit=12000;
      for (size_t i=0;i<sweep_limit;++i) {
        const auto c=fault_case(prune,i); ++cases; rejections+=c.rejected; bad+=!c.valid;
        if (!audit) require(c.valid,"allocation failure left partially cleaned forks");
        if (!c.injected) break;
        require(i!=sweep_limit-1,"fault sweep did not reach noninjected case");
      }
      invalid+=bad;
      std::cout << "mode=" << (prune?"prune":"remove") << " cases=" << cases << " rejected=" << rejections << " invalid=" << bad << std::endl;
    }
    return invalid ? 91 : 0;
  } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
