#include "core/owned_allocation.hpp"
#include <koinos/mempool/mempool.hpp>
#include <koinos/state_db/retained_memory.hpp>
#include <koinos/state_db/state_delta.hpp>
#include <koinos/chain/value.pb.h>
#include <koinos/util/conversion.hpp>

#include "../artifact_root.hpp"

#include <chrono>
#include <atomic>
#include <cstdlib>
#include <future>
#include <iostream>
#include <new>
#include <stdexcept>
#include <tuple>
#include <thread>
#include <vector>
#include <sys/resource.h>
#include <unistd.h>

// Measures/fails ordinary calling-thread C++ new/new[], not dependency malloc,
// aligned allocation, retained heap, races or machine OOM.
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
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

namespace {
using namespace koinos;
using Usage = state_db::retained_memory_usage;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
auto fields(const Usage& u) {
  return std::tie(u.state_nodes, u.finalized_state_nodes, u.writable_state_nodes,
    u.map_backends, u.unmeasured_backends, u.map_entries, u.map_key_size_bytes,
    u.map_key_capacity_bytes, u.value_references, u.unique_values,
    u.unique_value_size_bytes, u.unique_value_capacity_bytes, u.tombstone_entries,
    u.tombstone_key_size_bytes, u.tombstone_key_capacity_bytes, u.tombstone_bucket_count);
}
crypto::multihash id(uint64_t n) { return crypto::hash(crypto::multicodec::sha2_256, n); }

void test_map_clones_and_excluded_owners() {
  state_db::database db;
  auto lock = db.get_unique_lock();
  db.open({}, [](auto) {}, state_db::fork_resolution_algorithm::fifo, lock);
  const state_db::object_space space;
  auto a = db.create_writable_node(db.get_root(lock)->id(), id(1), {}, lock);
  const std::string payload(4096, 'x'), replacement(8192, 'y');
  a->put_object(space, "a", &payload); a->put_object(space, "b", &payload);
  const auto before = db.get_retained_memory_usage(lock);
  require(before.state_nodes == 2 && before.map_backends == 2 && !before.unmeasured_backends,
    "empty root/map node inventory incorrect");
  require(before.unique_values == 2 && before.unique_value_size_bytes == 8192,
    "equal bytes in distinct owners must not be deduplicated");
  auto b = db.clone_node(a->id(), id(2), {}, lock);
  const auto cloned = db.get_retained_memory_usage(lock);
  require(cloned.state_nodes == 3 && cloned.map_entries == 4 && cloned.value_references == 4,
    "cloned independent map entries not inventoried");
  require(cloned.unique_values == 2 && cloned.unique_value_size_bytes == before.unique_value_size_bytes
    && cloned.unique_value_capacity_bytes == before.unique_value_capacity_bytes,
    "cloned immutable owners counted more than once");
  require(cloned.map_key_size_bytes == 2 * before.map_key_size_bytes,
    "physical key copies in map clone not inventoried");
  b->put_object(space, "a", &replacement);
  a->remove_object(space, "b");
  a->remove_object_preserve_tombstone(space, "never-present");
  const auto replaced = db.get_retained_memory_usage(lock);
  require(replaced.map_entries == 3 && replaced.unique_values == 3
    && replaced.unique_value_size_bytes == 16384 && replaced.tombstone_entries == 2,
    "replacement/erase owner or tombstone inventory incorrect");
  auto c = db.clone_node(a->id(), id(3), {}, lock);
  const auto copied_tombstones = db.get_retained_memory_usage(lock);
  require(copied_tombstones.map_entries == 4 && copied_tombstones.unique_values == 3
    && copied_tombstones.tombstone_entries == 4,
    "cloned tombstones or shared remaining value incorrect");
  require(copied_tombstones.tombstone_key_size_bytes == 2 * replaced.tombstone_key_size_bytes,
    "cloned tombstone keys not inventoried");
  const auto a_root = a->pending_merkle_root(), b_root = b->pending_merkle_root();
  auto overlay = a->create_anonymous_node(); overlay->put_object(space, "temporary", &replacement);
  require(fields(db.get_retained_memory_usage(lock)) == fields(copied_tombstones),
    "unindexed overlay erroneously reported as indexed retention");
  require(a->pending_merkle_root() == a_root && b->pending_merkle_root() == b_root,
    "inventory changed pending roots");
  overlay.reset();
  db.discard_node(c->id(), lock); c.reset();
  db.finalize_node(a->id(), lock);
  const auto finalized = db.get_retained_memory_usage(lock);
  require(finalized.finalized_state_nodes == 2 && finalized.writable_state_nodes == 1,
    "finalization not reflected");
  auto held = b->get_object(space, "a");
  b->put_object(space, "a", &payload);
  require(*held == replacement && db.get_retained_memory_usage(lock).unique_value_size_bytes == 12288,
    "caller-only replaced owner must remain valid but excluded");
  db.close(lock);
  require(*held == replacement, "inventory disturbed owner lifetime across close");
  bool closed = false;
  try { db.get_retained_memory_usage(lock); } catch (const state_db::database_not_open&) { closed = true; }
  require(closed, "closed database reported zero instead of unavailable");
}

void test_backend_dedup_and_lock_contract() {
  using namespace state_db;
  auto map = std::make_shared<backends::map::map_backend>(); map->put("key", "value");
  state_db::detail::state_delta a(map), b(map);
  state_db::detail::retained_memory_accumulator inventory;
  a.inventory_retained_memory(inventory); b.inventory_retained_memory(inventory);
  require(inventory.usage.state_nodes == 2 && inventory.usage.map_backends == 1
    && inventory.usage.map_entries == 1 && inventory.usage.unique_values == 1,
    "shared backend itself was counted more than once");
  // No persistent DB is opened or read. Non-map payload remains unavailable.
  state_db::detail::state_delta persistent(std::make_shared<backends::rocksdb::rocksdb_backend>());
  persistent.inventory_retained_memory(inventory);
  require(inventory.usage.unmeasured_backends == 1 && inventory.usage.map_entries == 1,
    "non-map backend reported as measured");
  database db, other;
  auto foreign = other.get_unique_lock();
  std::shared_mutex mutex;
  auto unlocked = std::make_shared<const std::unique_lock<std::shared_mutex>>(mutex, std::defer_lock);
  for (const auto& invalid : {unique_lock_ptr{}, foreign, unlocked}) {
    bool rejected = false;
    try { db.get_retained_memory_usage(invalid); } catch (const illegal_argument&) { rejected = true; }
    require(rejected, "invalid/foreign/unowned lock accepted");
  }
  bool overflow = false;
  uint64_t maximum = std::numeric_limits<uint64_t>::max();
  try { state_db::detail::retained_memory_accumulator::add(maximum, 1); }
  catch (const internal_error&) { overflow = true; }
  require(overflow && maximum == std::numeric_limits<uint64_t>::max(), "inventory arithmetic wrapped");
}

void test_persistent_backend_is_unmeasured() {
  const auto root = test_artifact_root(TELENO_LR2_REPOSITORY_ROOT);
  auto pattern = (root / "m1d-retention-db-XXXXXX").string();
  std::vector<char> name(pattern.begin(), pattern.end()); name.push_back('\0');
  const auto* created = ::mkdtemp(name.data()); require(created, "scratch database creation failed");
  const std::filesystem::path path(created);
  require(test_artifact_child(path, root), "scratch database outside selected artifact root");
  const std::string payload(4096, 'p');
  for (int pass = 0; pass < 2; ++pass) {
    state_db::database db; auto lock = db.get_unique_lock();
    db.open(path, [&](auto node) { node->put_object({}, "stored", &payload); },
      state_db::fork_resolution_algorithm::fifo, lock);
    const auto usage = db.get_retained_memory_usage(lock);
    require(usage.state_nodes == 1 && usage.unmeasured_backends == 1 && !usage.map_backends
      && !usage.unique_value_size_bytes, "persistent cache bytes invented by map inventory");
    require(*db.get_root(lock)->get_object({}, "stored") == payload, "diagnostic changed stored bytes/reopen");
    db.close(lock);
  }
  // Retain this fresh fixture for reproducibility; no existing database touched.
  std::cout << "persistent_fixture=" << path.string() << '\n';
}

void block(mempool::mempool& pool, uint64_t n, uint64_t parent, uint64_t height) {
  broadcast::block_accepted accepted;
  accepted.mutable_block()->set_id(util::converter::as<std::string>(id(n)));
  accepted.mutable_block()->mutable_header()->set_previous(util::converter::as<std::string>(id(parent)));
  accepted.mutable_block()->mutable_header()->set_height(height);
  require(pool.handle_block(accepted), "synthetic block not linked");
}
auto at() { return std::chrono::system_clock::time_point{std::chrono::seconds{1000}}; }
mempool::pending_transaction transaction(size_t n) {
  mempool::pending_transaction pending;
  auto* tx = pending.mutable_transaction();
  tx->set_id(std::string(40, 'i') + std::to_string(n));
  tx->mutable_header()->set_payer(std::string(40, 'p') + std::to_string(n));
  tx->mutable_header()->set_rc_limit(100);
  chain::value_type nonce; nonce.set_uint64_value(1);
  tx->mutable_header()->set_nonce(util::converter::as<std::string>(nonce));
  tx->add_signatures(std::string(64*1024, 's'));
  return pending;
}
void fill(mempool::mempool& pool) {
  block(pool, 1, 0, 1); block(pool, 2, 1, 2); block(pool, 3, 1, 2);
  for (size_t i=0; i<64; ++i) pool.add_pending_transaction(transaction(i), at(), 1000);
}

void print_usage(const mempool::retained_memory_usage& measured) {
  const auto& u = measured.indexed_state;
  std::cout << "{\"node_mutexes\":" << measured.node_mutexes
    << ",\"state_nodes\":" << u.state_nodes << ",\"finalized_state_nodes\":" << u.finalized_state_nodes
    << ",\"writable_state_nodes\":" << u.writable_state_nodes << ",\"map_backends\":" << u.map_backends
    << ",\"unmeasured_backends\":" << u.unmeasured_backends << ",\"map_entries\":" << u.map_entries
    << ",\"map_key_size_bytes\":" << u.map_key_size_bytes << ",\"map_key_capacity_bytes\":" << u.map_key_capacity_bytes
    << ",\"value_references\":" << u.value_references << ",\"unique_values\":" << u.unique_values
    << ",\"unique_value_size_bytes\":" << u.unique_value_size_bytes
    << ",\"unique_value_capacity_bytes\":" << u.unique_value_capacity_bytes
    << ",\"tombstone_entries\":" << u.tombstone_entries << ",\"tombstone_key_size_bytes\":" << u.tombstone_key_size_bytes
    << ",\"tombstone_key_capacity_bytes\":" << u.tombstone_key_capacity_bytes
    << ",\"tombstone_bucket_count\":" << u.tombstone_bucket_count << "}";
}

void test_mempool_retention(bool measure) {
  mempool::mempool pool; fill(pool);
  const auto before = pool.get_retained_memory_usage();
  require(pool.pending_transaction_count() == 64 && before.node_mutexes == 4
    && before.indexed_state.state_nodes == 8 && before.indexed_state.writable_state_nodes == 4,
    "four-fork fixture changed");
  require(before.indexed_state.unique_value_size_bytes >= 64*65536
    && before.indexed_state.unique_value_size_bytes < 2*64*65536
    && before.indexed_state.unique_values == 836,
    "pending records were copied across forks or auxiliary owners omitted");
  allocations = requested_bytes = 0; fired = false; fail_at = 1'000'000;
  const auto start = std::chrono::steady_clock::now();
  const auto scanned = pool.get_retained_memory_usage();
  const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
  fail_at = -1;
  const auto count = allocations, bytes = requested_bytes;
  require(fields(before.indexed_state) == fields(scanned.indexed_state), "inventory not repeatable");
  require(pool.prune(std::chrono::seconds{1}, at()+std::chrono::seconds{5}) == 64
    && !pool.pending_transaction_count(), "expiry did not empty visible head");
  const auto expired = pool.get_retained_memory_usage();
  require(expired.indexed_state.unique_value_size_bytes >= 64*65536
    && expired.indexed_state.tombstone_entries > 0,
    "empty head incorrectly interpreted as no retained ancestor payload/tombstones");
  require(pool.has_pending_transaction(transaction(0).transaction().id(), id(1)),
    "diagnostic/expiry changed retained ancestor semantics");
  broadcast::block_irreversible lib;
  lib.mutable_topology()->set_id(util::converter::as<std::string>(id(2)));
  pool.handle_irreversibility(lib);
  const auto committed = pool.get_retained_memory_usage();
  require(committed.node_mutexes == 1 && committed.indexed_state.state_nodes == 2
    && committed.indexed_state.unique_value_size_bytes < 65536,
    "LIB did not release indexed ancestor payload/fork bookkeeping");
  require(!pool.pending_transaction_count(), "inventory/LIB changed head-visible pending set");
  if (measure) {
    rusage usage{}; require(getrusage(RUSAGE_SELF, &usage) == 0, "RSS query failed");
#if defined(__APPLE__)
    const uint64_t peak = usage.ru_maxrss;
#else
    const uint64_t peak = static_cast<uint64_t>(usage.ru_maxrss)*1024;
#endif
    std::cout << "{\"format\":\"lr2-m1d-retention\",\"records\":64,\"signature_bytes\":65536,\"head_before\":64,\"head_after_expiry\":0,\"before\":";
    print_usage(before); std::cout << ",\"after_expiry\":"; print_usage(expired);
    std::cout << ",\"after_lib\":"; print_usage(committed);
    std::cout << ",\"scan_cpp_allocation_attempts\":" << count << ",\"scan_requested_cpp_allocation_bytes\":" << bytes
      << ",\"scan_elapsed_microseconds\":" << us << ",\"process_peak_rss_bytes\":" << peak << "}\n";
  }
}

void test_scan_allocation_failure_is_read_only() {
  mempool::mempool pool; fill(pool);
  const auto before = pool.get_retained_memory_usage();
  for (size_t i=0; i<2000; ++i) {
    fired = false; fail_at = static_cast<long>(i);
    bool failed = false;
    try { pool.get_retained_memory_usage(); } catch (const std::bad_alloc&) { failed = true; }
    fail_at = -1;
    const bool injected = fired;
    const auto after = pool.get_retained_memory_usage();
    require(fields(before.indexed_state) == fields(after.indexed_state) && before.node_mutexes == after.node_mutexes
      && pool.pending_transaction_count() == 64, "failed diagnostic mutated retained work");
    require(failed == injected, "injected scan allocation failure was swallowed");
    if (!injected) { std::cout << "read_only_scan_fault_cases=" << i << '\n'; return; }
  }
  throw std::runtime_error("scan fault sweep did not reach success");
}

void test_mempool_scan_during_admission_and_expiry() {
  mempool::mempool pool;
  block(pool, 1, 0, 1); block(pool, 2, 1, 2); block(pool, 3, 1, 2);
  std::atomic<size_t> observations{0};
  std::atomic<bool> stop{false};
  std::promise<void> started;
  auto ready = started.get_future();
  std::exception_ptr reader_error;
  std::thread reader([&] {
    bool announced = false;
    try {
      while (!stop) {
        const auto usage = pool.get_retained_memory_usage();
        const auto& state = usage.indexed_state;
        require(state.state_nodes == 8 && state.finalized_state_nodes == 4
          && state.writable_state_nodes == 4 && state.map_backends == 8
          && state.map_entries == state.value_references
          && state.unique_values <= state.value_references,
          "concurrent scan observed inconsistent publication/index state");
        ++observations;
        if (!announced) { announced = true; started.set_value(); }
        std::this_thread::yield();
      }
    } catch (...) {
      reader_error = std::current_exception();
      if (!announced) started.set_value();
    }
  });
  struct Join {
    std::atomic<bool>& stop;
    std::thread& reader;
    ~Join() { stop = true; if (reader.joinable()) reader.join(); }
  } join{stop, reader};
  ready.wait(); // The reader must actually sample before the writer can finish.
  // Reader/writer both use the same database lock; this is functional overlap,
  // not a replacement for functioning integrated ThreadSanitizer qualification.
  for (size_t i=0; i<32; ++i) {
    pool.add_pending_transaction(transaction(i), at(), 1000);
    require(pool.prune(std::chrono::seconds{1}, at()+std::chrono::seconds{5}) == 1,
      "concurrent expiry changed visible-head removal result");
  }
  stop = true; reader.join();
  if (reader_error) std::rethrow_exception(reader_error);
  require(observations > 0 && !pool.pending_transaction_count(), "concurrent diagnostic not exercised");
}
} // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--measure-retention") { test_mempool_retention(true); return 0; }
    test_map_clones_and_excluded_owners(); test_backend_dedup_and_lock_contract();
    test_persistent_backend_is_unmeasured(); test_mempool_retention(false);
    test_scan_allocation_failure_is_read_only();
    test_mempool_scan_during_admission_and_expiry();
  } catch (const std::exception& e) { fail_at = -1; std::cerr << e.what() << '\n'; return 1; }
}
