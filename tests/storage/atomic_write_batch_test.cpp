#include <koinos/state_db/state_db.hpp>
#include <koinos/state_db/state_delta.hpp>

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

static thread_local bool count_allocations = false;
static thread_local size_t allocations = 0;
static thread_local long fail_at = -1;
static thread_local bool fired = false;
void* operator new(std::size_t n) {
  if (count_allocations) ++allocations;
  if (fail_at >= 0 && fail_at-- == 0) { fail_at = -1; fired = true; throw std::bad_alloc(); }
  if (auto p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

void test_no_allocation_transfer() {
  using Map = koinos::state_db::backends::map::map_backend;
  Map source, target;
  target.put("replaced", "old");
  source.put("replaced", std::string(4096, 'r')); source.put("inserted", std::string(4096, 'i'));
  auto owner = source.get("inserted"), replacement = source.get("replaced");
  std::unordered_set<std::string> removed{"inserted", "unchanged-tombstone"};
  allocations = 0; count_allocations = true;
  target.merge_writes_from(source, removed);
  count_allocations = false;
  require(allocations == 0, "map publication allocated");
  require(source.empty() && target.get("inserted") == owner && target.get("replaced") == replacement, "transfer copied/lost owners");
  require(!removed.count("inserted") && removed.count("unchanged-tombstone"), "write transfer changed unrelated tombstones");
}

void test_batch_validation_and_reset() {
  using namespace koinos;
  state_db::database db;
  auto lock = db.get_unique_lock();
  db.open({}, [](auto) {}, state_db::fork_resolution_algorithm::fifo, lock);
  const auto id = [](uint64_t n) { return crypto::hash(crypto::multicodec::sha2_256, n); };
  const state_db::object_space space;
  auto a = db.create_writable_node(db.get_head(lock)->id(), id(1), {}, lock);
  auto b = db.create_writable_node(db.get_head(lock)->id(), id(2), {}, lock);
  const std::string before = "before", after = std::string(1024, 'a');
  for (const auto& node : {a, b}) {
    node->put_object(space, "replaced", &before);
    node->remove_object_preserve_tombstone(space, "inserted");
  }
  const auto a_before = a->pending_merkle_root(), b_before = b->pending_merkle_root();
  auto first = a->create_anonymous_node(), second = b->create_anonymous_node();
  first->put_object(space, "inserted", &after);
  second->remove_object(space, "replaced");
  auto rejected = [&](const auto& batch) {
    bool failed = false;
    try { state_db::anonymous_state_node::commit_all_writes(batch); } catch (const std::exception&) { failed = true; }
    require(failed, "unsupported batch not rejected");
    require(a->pending_merkle_root() == a_before && b->pending_merkle_root() == b_before, "batch validation modified parents");
    require(*first->get_object(space, "inserted") == after, "failed preparation consumed first overlay");
  };
  rejected(std::vector<state_db::anonymous_state_node_ptr>{first, second}); // later removal
  rejected(std::vector<state_db::anonymous_state_node_ptr>{first, a->create_anonymous_node()}); // same parent
  rejected(std::vector<state_db::anonymous_state_node_ptr>{first, first->create_anonymous_node()}); // consumed anonymous parent
  rejected(std::vector<state_db::anonymous_state_node_ptr>{first, nullptr});
  db.finalize_node(b->id(), lock);
  rejected(std::vector<state_db::anonymous_state_node_ptr>{first, b->create_anonymous_node()}); // later finalized parent

  auto expected = db.clone_node(a->id(), id(3), {}, lock);
  // Compare with the original anonymous squash path, not direct parent writes:
  // that path removes a parent tombstone when the overlay writes the same key.
  auto ordinary = expected->create_anonymous_node();
  ordinary->put_object(space, "inserted", &after); ordinary->put_object(space, "replaced", &after);
  ordinary->commit();
  first->put_object(space, "replaced", &after);
  auto held = first->get_object(space, "inserted");
  state_db::anonymous_state_node::commit_all_writes({first});
  require(a->pending_merkle_root() == expected->pending_merkle_root(), "roots/tombstones differ from ordinary anonymous commit");
  require(a->get_object(space, "inserted") == held, "publication made another value copy");
  require(first->get_delta_entries().empty() && first->parent()->id() == a->id(), "overlay not reset to original parent");
  const std::string again = "again";
  first->put_object(space, "replaced", &again);
  state_db::anonymous_state_node::commit_all_writes({first});
  require(*a->get_object(space, "replaced") == again && *held == after, "reused overlay changed owned values");
  state_db::anonymous_state_node::commit_all_writes({});
  db.close(lock);
}

void test_nonmap_parent_rejected_before_writes() {
  using namespace koinos::state_db;
  auto map = std::make_shared<backends::map::map_backend>();
  auto parent = std::make_shared<detail::state_delta>(map);
  auto first = parent->make_child(); first->put("key", "value");
  // Unopened persistent backend: no filesystem or durable write is performed.
  auto persistent = std::make_shared<detail::state_delta>(std::make_shared<backends::rocksdb::rocksdb_backend>());
  auto later = persistent->make_child(); later->put("key", "value");
  bool rejected = false;
  try { detail::state_delta::squash_all_writes({first, later}); }
  catch (const std::exception&) { rejected = true; }
  require(rejected && map->empty() && first->find("key"), "nonmap batch changed an earlier parent");
}

using Delta = koinos::state_db::detail::state_delta;
std::shared_ptr<Delta> change_fixture() {
  auto root = std::make_shared<Delta>(std::make_shared<koinos::state_db::backends::map::map_backend>());
  root->put("inherited", "ancestor");
  auto parent = root->make_child();
  parent->put("local-remove", "local"); parent->put("overlap", "old");
  parent->erase("old-marker", true); parent->erase("resurrect", true);
  auto source = parent->make_child();
  source->erase("inherited"); source->erase("local-remove"); source->erase("absent");
  source->erase("preserved-absent", true); source->erase("overlap");
  source->put("overlap", "new"); source->put("resurrect", "alive"); source->put("inserted", "value");
  return source;
}
auto entries(const std::shared_ptr<Delta>& delta) {
  std::vector<std::string> result;
  for (const auto& entry : delta->get_delta_entries()) result.push_back(entry.SerializeAsString());
  return result;
}
void test_prepared_removal_publication() {
  auto first = change_fixture(), second = change_fixture();
  auto expected = change_fixture(); expected->squash();
  auto held = first->find("inserted"), previous = first->parent()->find("overlap");
  auto prepared = Delta::prepare_all_changes({first, second});
  allocations = 0; count_allocations = true; fail_at = 0;
  prepared.apply(); prepared.apply(); // Single-use publication is harmless on repeat.
  fail_at = -1; count_allocations = false;
  require(allocations == 0, "prepared removal publication allocated");
  for (const auto& source : {first, second}) {
    require(source->parent()->calculate_merkle_root() == expected->parent()->calculate_merkle_root(), "prepared removals changed ordinary squash root");
    require(entries(source->parent()) == entries(expected->parent()), "prepared removals changed tombstones/entries");
    require(entries(source).empty(), "prepared change source not consumed");
    require(!source->parent()->find("inherited") && !source->parent()->find("local-remove"), "removed inherited/local values visible");
    require(!source->parent()->is_removed("absent") && source->parent()->is_removed("preserved-absent"), "normal/preserved missing removal changed");
    require(source->parent()->is_removed("old-marker") && !source->parent()->is_removed("overlap") && !source->parent()->is_removed("resurrect"), "write precedence/unrelated markers changed");
  }
  require(first->parent()->find("inserted") == held && *previous == "old", "publication copied owner or invalidated held value");
  // The prepared batch keeps all source/parent/backend lifetimes through apply.
  auto owned = change_fixture(); std::weak_ptr<Delta> parent = owned->parent();
  auto lifetime = Delta::prepare_all_changes({owned}); owned.reset(); lifetime.apply();
  require(!parent.expired() && *parent.lock()->find("inserted") == "value", "prepared input ownership lost");
}
void test_change_preparation_failures() {
  size_t rejected = 0;
  for (size_t ordinal=0; ordinal<1000; ++ordinal) {
    auto first = change_fixture(), second = change_fixture();
    const auto a = entries(first->parent()), b = entries(second->parent());
    const auto sa = entries(first), sb = entries(second);
    const std::vector<std::shared_ptr<Delta>> inputs{first, second};
    fail_at = static_cast<long>(ordinal); fired = false;
    bool failed = false;
    try { auto batch = Delta::prepare_all_changes(inputs); batch.apply(); }
    catch (const std::bad_alloc&) { failed = true; }
    fail_at = -1;
    if (!failed) { require(!fired, "unexpected injected successful preparation"); break; }
    ++rejected;
    require(entries(first->parent()) == a && entries(second->parent()) == b && entries(first) == sa && entries(second) == sb, "rejected preparation mutated inputs");
    auto retry = Delta::prepare_all_changes(inputs); retry.apply();
    require(*first->parent()->find("inserted") == "value", "prepared removal cannot retry");
    require(ordinal!=999, "preparation fault sweep incomplete");
  }
  require(rejected>0, "preparation faults not exercised");
  auto first=change_fixture(), other=change_fixture();
  const auto before=entries(first->parent()), source=entries(first);
  auto rejects=[&](const std::vector<std::shared_ptr<Delta>>& batch) {
    bool failed=false;
    try { auto prepared=Delta::prepare_all_changes(batch); prepared.apply(); } catch(const std::exception&) { failed=true; }
    require(failed && entries(first->parent())==before && entries(first)==source, "invalid later change modified earlier parent");
  };
  rejects({first,nullptr}); rejects({first,first->parent()->make_child()});
  rejects({first,first->make_child()}); // Parent is another source in the same batch.
  rejects({first,other->parent()}); // Root parent is unsupported.
  other->parent()->finalize(); rejects({first,other});
  std::cout << "change preparation failures=" << rejected << std::endl;
}
void test_change_wrapper_reset() {
  using namespace koinos;
  state_db::database db; auto lock=db.get_unique_lock();
  db.open({},[](auto){},state_db::fork_resolution_algorithm::fifo,lock);
  auto parent=db.create_writable_node(db.get_head(lock)->id(),crypto::hash(crypto::multicodec::sha2_256,uint64_t{10}),{},lock);
  const state_db::object_space space;
  const std::string old="old", value="new";
  parent->put_object(space,"removed",&old);
  auto overlay=parent->create_anonymous_node(); overlay->remove_object(space,"removed"); overlay->put_object(space,"inserted",&value);
  const auto before=parent->pending_merkle_root();
  for (const auto& later : {state_db::anonymous_state_node_ptr{},overlay->create_anonymous_node(),db.get_head(lock)->create_anonymous_node()}) {
    bool failed=false;
    try { state_db::anonymous_state_node::commit_all_changes({overlay,later}); } catch(const std::exception&) { failed=true; }
    require(failed && parent->pending_merkle_root()==before, "invalid wrapper changed parent");
  }
  state_db::anonymous_state_node::commit_all_changes({overlay});
  require(overlay->get_delta_entries().empty() && overlay->parent()->id()==parent->id(), "change wrapper reset not fresh original-parent child");
  require(!parent->get_object(space,"removed") && *parent->get_object(space,"inserted")==value, "change wrapper lost removal/write");
  overlay->remove_object(space,"inserted"); state_db::anonymous_state_node::commit_all_changes({overlay});
  require(!parent->get_object(space,"inserted"), "change wrapper cannot reuse overlay");
  state_db::anonymous_state_node::commit_all_changes({}); db.close(lock);
}

int main() {
  try { test_no_allocation_transfer(); test_batch_validation_and_reset(); test_nonmap_parent_rejected_before_writes();
    test_prepared_removal_publication(); test_change_preparation_failures(); test_change_wrapper_reset(); }
  catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
