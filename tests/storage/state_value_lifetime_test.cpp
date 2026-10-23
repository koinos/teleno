#include "core/owned_allocation.hpp"
#include "../artifact_root.hpp"
#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>
#include <koinos/state_db/backends/map/map_backend.hpp>
#include <koinos/state_db/state_db.hpp>
#include <koinos/util/conversion.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>
#include <sys/resource.h>

// Observe deletion of a previously live string buffer without dereferencing
// released storage. This is a deterministic ownership probe, not a sanitizer.
static std::atomic<const void*> watched_buffer{nullptr};
static std::atomic<bool> buffer_deleted{false};
void* operator new(std::size_t n) {
  if (auto p = koinos::node::owned_allocation::allocate(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
  if (p && p == watched_buffer.load()) buffer_deleted = true;
  koinos::node::owned_allocation::deallocate(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

void measure_map_clones(bool small_values) {
  using Map = koinos::state_db::backends::map::map_backend;
  Map source;
  const size_t records = small_values ? 4000 : 128;
  const size_t value_bytes = small_values ? 16 : 64 * 1024;
  const size_t clone_count = 16;
  for (size_t i=0; i<records; ++i) source.put(std::to_string(i), std::string(value_bytes, 'v'));
  { auto warmup = source.clone(); require(warmup->size() == records, "clone warm-up failed"); }
  std::vector<std::shared_ptr<koinos::state_db::backends::abstract_backend>> clones;
  const auto start = std::chrono::steady_clock::now();
  for (size_t i=0; i<clone_count; ++i) clones.push_back(source.clone());
  const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
  rusage usage{}; require(getrusage(RUSAGE_SELF, &usage) == 0, "RSS query failed");
#if defined(__APPLE__)
  const uint64_t peak = usage.ru_maxrss;
#else
  const uint64_t peak = static_cast<uint64_t>(usage.ru_maxrss) * 1024;
#endif
  std::cout << "{\"format\":\"lr2-map-clones\",\"records\":" << records
    << ",\"value_bytes\":" << value_bytes << ",\"clones\":" << clone_count
    << ",\"elapsed_microseconds\":" << us << ",\"process_peak_rss_bytes\":" << peak << "}\n";
}

void test_map_owned_values_and_clone_isolation() {
  using Map = koinos::state_db::backends::map::map_backend;
  Map source;
  source.put("key", std::string(1024, 'a'));
  auto held = source.get("key");
  auto clone = source.clone();
  require(clone->get("key") == held, "clones must share immutable payloads");
  source.put("key", "changed-original");
  require(*held == std::string(1024, 'a') && clone->get("key") == held, "original write changed clone/read bytes");
  clone->put("key", "changed-clone");
  require(*source.get("key") == "changed-original", "clone write changed original map");
  auto from_iterator = clone->begin().share_value();
  clone->erase("key");
  clone.reset(); source.clear();
  require(*from_iterator == "changed-clone" && *held == std::string(1024, 'a'), "read owner did not survive erase/clear/destruction");
  std::weak_ptr<const std::string> weak = held;
  held.reset(); require(weak.expired(), "released clone/read payload retained unexpectedly");
}

void test_state_results_survive_replacement_and_close() {
  using namespace koinos;
  state_db::database db;
  auto lock = db.get_unique_lock();
  db.open(std::make_shared<state_db::backends::map::map_backend>(), [](const auto&) {}, state_db::pob_comparator, lock);
  const state_db::object_space space;
  const auto id = crypto::hash(crypto::multicodec::sha2_256, uint64_t(1));
  auto child = db.create_writable_node(db.get_head(lock)->id(), id, {}, lock);
  const std::string value(1024, 'v'), replacement(1024, 'w');
  for (const auto& key : {"aa", "bb", "cc"}) child->put_object(space, key, &value);
  const auto before = child->pending_merkle_root();
  auto direct = child->get_object(space, "bb");
  auto [next, next_key] = child->get_next_object(space, "aa");
  auto [prev, prev_key] = child->get_prev_object(space, "cc");
  require(direct && next == direct && prev == direct && next_key == "bb" && prev_key == "bb", "state iteration must preserve the selected owner/key");
  child->remove_object(space, "bb");
  require(!child->get_object(space, "bb") && *direct == value && *next == value && *prev == value, "removed state result lost ownership");
  child->put_object(space, "bb", &replacement);
  require(*child->get_object(space, "bb") == replacement && *direct == value, "replacement changed held state bytes");
  require(child->pending_merkle_root() != before, "fresh root did not reflect changed bytes");
  const auto normal = child->pending_merkle_root();
  child->remove_object(space, "absent");
  require(child->pending_merkle_root() == normal, "normal absent removal changed semantics");
  child->remove_object_preserve_tombstone(space, "absent");
  require(child->pending_merkle_root() != normal, "replay tombstone not retained");
  db.finalize_node(id, lock); child.reset(); db.commit_node(id, lock);
  auto final_value = db.get_root(lock)->get_object(space, "bb");
  db.close(lock);
  require(*final_value == replacement && *direct == value && *prev == value, "state result did not survive close");
}

int main(int argc, char** argv) {
  if (argc == 2 && (std::string(argv[1]) == "--measure-map-clones" || std::string(argv[1]) == "--measure-map-clones-small")) {
    measure_map_clones(std::string(argv[1]) == "--measure-map-clones-small"); return 0;
  }
  test_map_owned_values_and_clone_isolation();
  test_state_results_survive_replacement_and_close();
  const auto repository = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
  const auto root = test_artifact_root(repository);
  const auto path = root / ("state-value-lifetime-" + std::to_string(
    std::chrono::steady_clock::now().time_since_epoch().count()));
  require(test_artifact_child(path, root) && std::filesystem::create_directory(path), "fresh owned fixture required");
  koinos::state_db::backends::rocksdb::rocksdb_backend backend(2048);
  backend.open(path);
  const std::string small(1024, 's');
  const std::string large(1024, 'l'); // two entries exceed this test-only cache capacity
  backend.put("small", small);
  backend.put("large", large);
  auto held = backend.get("small");
  require(held && *held == small, "small state read failed");
  watched_buffer = held->data();
  buffer_deleted = false;
  // Only reads run concurrently: no unsupported concurrent DB close/write.
  std::thread evictor([&] { auto other = backend.get("large"); require(other && *other == large, "large state read failed"); });
  evictor.join();
  std::cout << "held_buffer_deleted_after_read_eviction=" << buffer_deleted.load() << std::endl;
  require(!buffer_deleted, "state read buffer was freed by another read before consumption");
  require(*held == small, "held state value changed");
  std::weak_ptr<const std::string> weak = held;
  held.reset();
  require(buffer_deleted && weak.expired(), "evicted value not reclaimed after its last reader");
  watched_buffer = nullptr;
  // Exercise the public state API, including a value returned from a destroyed
  // temporary RocksDB iterator, not only the backend's direct get().
  const koinos::state_db::object_space space;
  auto encoded = [&](const std::string& key) {
    koinos::chain::database_key dk; *dk.mutable_space() = space; dk.set_key(key);
    return koinos::util::converter::as<std::string>(dk);
  };
  backend.put(encoded("aa"), small);
  backend.put(encoded("bb"), small);
  backend.put(encoded("cc"), small);
  koinos::state_db::database db;
  auto lock = db.get_unique_lock();
  db.open(std::shared_ptr<koinos::state_db::backends::abstract_backend>(&backend, [](auto*) {}),
          [](const auto&) {}, koinos::state_db::pob_comparator, lock);
  auto node = db.get_root(lock);
  auto result = node->get_object(space, "bb");
  auto [next, next_key] = node->get_next_object(space, "aa");
  auto [prev, prev_key] = node->get_prev_object(space, "cc");
  require(result && next && prev && next_key == "bb" && prev_key == "bb", "persistent state results missing");
  watched_buffer = result->data(); buffer_deleted = false;
  std::thread root_evictor([&] {
    auto other = backend.get("large"); require(other && *other == large, "root eviction read failed");
    auto second = backend.get("small"); require(second && *second == small, "root second eviction read failed");
  });
  root_evictor.join();
  require(!buffer_deleted && *result == small && *next == small && *prev == small, "public persistent state results did not survive read eviction");
  node.reset(); db.close(lock);
  backend.close();
  require(!buffer_deleted && *result == small && *next == small && *prev == small, "persistent state result lost ownership at close");
  watched_buffer = nullptr;
  // Uncached reads still return ownership: do not rely on an oversized cache
  // exception or a thread-local borrowed value for lifetime safety.
  backend.open(path);
  const std::string oversized(4096, 'u');
  backend.put("oversized", oversized);
  auto uncached = backend.get("oversized"), second_uncached = backend.get("oversized");
  require(uncached && second_uncached && uncached != second_uncached && *uncached == oversized,
          "oversized reads must bypass retention with independent owners");
  backend.close();
  require(*uncached == oversized, "uncached oversized result did not survive close");
  koinos::state_db::backends::rocksdb::rocksdb_backend zero(0);
  zero.open(path);
  auto zero_first = zero.get("small"), zero_second = zero.get("small");
  require(zero_first && zero_second && zero_first != zero_second && *zero_first == small,
          "zero-capacity reads must return owners without retention");
  require(!zero.get("absent") && !zero.get("absent"), "uncached missing reads changed");
  zero.close();
  require(*zero_first == small, "zero-capacity result did not survive reopen/close");
  std::filesystem::remove_all(path); // only the validated fresh fixture
}
