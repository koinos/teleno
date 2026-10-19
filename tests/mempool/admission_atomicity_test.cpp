#include <koinos/mempool/mempool.hpp>
#include <koinos/chain/value.pb.h>
#include <koinos/util/conversion.hpp>

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/resource.h>
#include <unistd.h>

// One-shot C++ allocation failures on the calling thread only. This does not
// instrument dependency malloc(), aligned new(), races, or real machine OOM.
static thread_local long fail_at = -1;
static thread_local size_t allocations = 0;
static thread_local size_t requested_bytes = 0;
static thread_local bool fired = false;
void* operator new(std::size_t n) {
  if (fail_at >= 0) {
    ++allocations;
    requested_bytes += n;
    if (fail_at-- == 0) { fail_at = -1; fired = true; throw std::bad_alloc(); }
  }
  if (auto p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

koinos::mempool::pending_transaction transaction(uint64_t nonce) {
  koinos::mempool::pending_transaction result;
  auto* tx = result.mutable_transaction();
  tx->set_id(std::string(40, 'i') + std::to_string(nonce));
  tx->mutable_header()->set_payer(std::string(40, 'p'));
  tx->mutable_header()->set_rc_limit(100);
  koinos::chain::value_type value; value.set_uint64_value(nonce);
  tx->mutable_header()->set_nonce(koinos::util::converter::as<std::string>(value));
  tx->add_signatures(std::string(1024, 's'));
  return result;
}

koinos::crypto::multihash block_id(uint64_t n) {
  return koinos::crypto::hash(koinos::crypto::multicodec::sha2_256, n);
}
void block(koinos::mempool::mempool& pool, uint64_t n, uint64_t parent, uint64_t height) {
  koinos::broadcast::block_accepted accepted;
  accepted.mutable_block()->set_id(koinos::util::converter::as<std::string>(block_id(n)));
  accepted.mutable_block()->mutable_header()->set_previous(koinos::util::converter::as<std::string>(block_id(parent)));
  accepted.mutable_block()->mutable_header()->set_height(height);
  require(pool.handle_block(accepted), "fixture block not linked");
}

std::vector<std::string> snapshot(koinos::mempool::mempool& pool,
                                const std::vector<koinos::crypto::multihash>& nodes,
                                const std::string& payer) {
  std::vector<std::string> result;
  for (const auto& node : nodes) {
    for (const auto& tx : pool.get_pending_transactions(2000, node)) result.push_back(tx.SerializeAsString());
    result.push_back(pool.get_pending_nonce(payer, node));
    result.push_back(std::to_string(pool.get_pending_transaction_count(payer, node)));
    result.push_back(pool.check_pending_account_resources(payer, 1000, 801, node) ? "rc-available" : "rc-reserved");
  }
  result.push_back(std::to_string(pool.get_reserved_account_rc(payer)));
  return result;
}

// Exit 91 specifically identifies a rejected call with changed visible state.
// Exit 86 identifies terminate(), e.g. an allocating noexcept iterator.
bool fault_case(size_t ordinal) {
  using namespace koinos;
  mempool::mempool pool;
  block(pool, 1, 0, 1); block(pool, 2, 1, 2); block(pool, 3, 1, 2);
  const std::vector<crypto::multihash> nodes{
    crypto::multihash::zero(crypto::multicodec::sha2_256), block_id(1), block_id(2), block_id(3)};
  const auto initial = transaction(1), candidate = transaction(2);
  const auto at = std::chrono::system_clock::time_point{std::chrono::seconds{1000}};
  pool.add_pending_transaction(initial, at, 1000);
  const auto before = snapshot(pool, nodes, initial.transaction().header().payer());
  allocations = 0; fired = false; fail_at = static_cast<long>(ordinal);
  bool rejected = false;
  try { pool.add_pending_transaction(candidate, at, 1000); }
  catch (...) { rejected = true; }
  fail_at = -1;
  const auto count = allocations;
  const bool injected = fired;
  const bool unchanged = snapshot(pool, nodes, initial.transaction().header().payer()) == before;
  bool everywhere = true, nowhere = true;
  for (const auto& node : nodes) {
    const bool found = pool.has_pending_transaction(candidate.transaction().id(), node);
    everywhere &= found; nowhere &= !found;
  }
  const bool valid = rejected ? unchanged && nowhere : everywhere;
  std::cout << "{\"ordinal\":" << ordinal << ",\"allocation_attempts\":" << count
            << ",\"injected\":" << (injected ? "true" : "false")
            << ",\"rejected\":" << (rejected ? "true" : "false")
            << ",\"unchanged\":" << (unchanged ? "true" : "false")
            << ",\"everywhere\":" << (everywhere ? "true" : "false")
            << ",\"nowhere\":" << (nowhere ? "true" : "false") << "}" << std::endl;
  if (!valid) std::exit(91);
  if (rejected) {
    // Same ID/nonce must be readmissible after failed preparation, on all forks.
    pool.add_pending_transaction(candidate, at, 1000);
    for (const auto& node : nodes) require(pool.has_pending_transaction(candidate.transaction().id(), node), "retry not admitted");
    require(pool.get_reserved_account_rc(initial.transaction().header().payer()) == 200, "retry RC changed");
  }
  return injected;
}

void measure_admission() {
  using namespace koinos;
  mempool::mempool pool;
  block(pool, 1, 0, 1); block(pool, 2, 1, 2); block(pool, 3, 1, 2);
  const auto at = std::chrono::system_clock::time_point{std::chrono::seconds{1000}};
  for (size_t i=0; i<64; ++i) {
    auto pending = transaction(i+10);
    pending.mutable_transaction()->mutable_header()->set_payer(std::string(40, 'p') + std::to_string(i));
    pending.mutable_transaction()->set_signatures(0, std::string(64*1024, 's'));
    pool.add_pending_transaction(pending, at, 1000);
  }
  auto warmup = transaction(1); pool.add_pending_transaction(warmup, at, 1000);
  auto candidate = transaction(2); candidate.mutable_transaction()->set_signatures(0, std::string(128*1024, 's'));
  allocations = requested_bytes = 0; fired = false; fail_at = 1'000'000;
  const auto start = std::chrono::steady_clock::now();
  pool.add_pending_transaction(candidate, at, 1000);
  const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
  fail_at = -1;
  const auto count = allocations, bytes = requested_bytes;
  rusage usage{}; require(getrusage(RUSAGE_SELF, &usage) == 0, "RSS query failed");
#if defined(__APPLE__)
  const uint64_t peak = usage.ru_maxrss;
#else
  const uint64_t peak = static_cast<uint64_t>(usage.ru_maxrss) * 1024;
#endif
  require(pool.pending_transaction_count() == 66 && !fired, "measurement input changed");
  std::cout << "{\"format\":\"lr2-m1c-admission\",\"mutable_forks\":4,\"prefilled_records\":64,\"prefilled_signature_bytes\":65536,\"candidate_signature_bytes\":131072,\"allocation_attempts\":"
            << count << ",\"requested_cpp_allocation_bytes\":" << bytes
            << ",\"elapsed_microseconds\":" << us << ",\"process_peak_rss_bytes\":" << peak << "}" << std::endl;
}

void test_nonce_conflict_is_fork_specific() {
  using namespace koinos;
  mempool::mempool pool;
  block(pool, 1, 0, 1);
  const auto initial = transaction(1);
  const auto at = std::chrono::system_clock::time_point{std::chrono::seconds{1000}};
  pool.add_pending_transaction(initial, at, 1000);
  broadcast::block_accepted included;
  included.mutable_block()->set_id(util::converter::as<std::string>(block_id(2)));
  included.mutable_block()->mutable_header()->set_previous(util::converter::as<std::string>(block_id(1)));
  included.mutable_block()->mutable_header()->set_height(2);
  *included.mutable_block()->add_transactions() = initial.transaction();
  require(pool.handle_block(included), "included fixture block not linked");
  block(pool, 3, 1, 2);
  auto replacement = initial; replacement.mutable_transaction()->set_id(std::string(40, 'j'));
  pool.add_pending_transaction(replacement, at, 1000);
  require(pool.has_pending_transaction(replacement.transaction().id(), block_id(2)), "nonce-free fork lost insertion");
  require(!pool.has_pending_transaction(initial.transaction().id(), block_id(2)), "included ID returned");
  for (const auto& id : {block_id(1), block_id(3)}) {
    require(!pool.has_pending_transaction(replacement.transaction().id(), id), "nonce-conflicting fork gained insertion");
    require(pool.has_pending_transaction(initial.transaction().id(), id), "nonce-conflicting fork lost original");
  }
}

int main(int argc, char** argv) {
  std::set_terminate([] {
    constexpr char text[] = "allocation fault reached terminate()\n";
    ::write(STDERR_FILENO, text, sizeof(text)-1); std::_Exit(86);
  });
  if (argc == 3 && std::string(argv[1]) == "--fault") { fault_case(std::stoull(argv[2])); return 0; }
  if (argc == 2 && std::string(argv[1]) == "--measure-admission") { measure_admission(); return 0; }
  test_nonce_conflict_is_fork_specific();
  for (size_t i=0; i<2000; ++i) if (!fault_case(i)) return 0;
  throw std::runtime_error("fault sweep did not reach the end of admission");
}
