#pragma once
#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/backends/map/map_backend.hpp>
#include <koinos/state_db/backends/rocksdb/rocksdb_backend.hpp>
#include <koinos/state_db/state_db_types.hpp>

#include <koinos/crypto/multihash.hpp>
#include <koinos/protocol/protocol.pb.h>

#include <any>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace koinos::state_db::detail {

class retained_memory_accumulator;

class state_delta: public std::enable_shared_from_this< state_delta >
{
public:
  using backend_type = backends::abstract_backend;
  using key_type     = backend_type::key_type;
  using value_type   = backend_type::value_type;
  using value_ptr    = backend_type::value_ptr;

private:
  std::shared_ptr< state_delta > _parent;

  std::shared_ptr< backend_type > _backend;
  backends::map::removed_key_set _removed_objects;

  state_node_id _id;
  uint64_t _revision = 0;
  mutable std::optional< crypto::multihash > _merkle_root;

  bool _finalized = false;

  std::timed_mutex _cv_mutex;
  std::condition_variable_any _cv;

public:
  state_delta() = default;
  state_delta( const std::optional< std::filesystem::path >& p );
  explicit state_delta( std::shared_ptr< backend_type > backend );
  ~state_delta() = default;

  void put( const key_type& k, const value_type& v );
  void put_owned( const key_type& k, value_ptr v );
  void erase( const key_type& k, bool preserve_tombstone = false );
  value_ptr find( const key_type& key ) const;

  void squash();
  // Internal helper: caller must provide named database parents disjoint from
  // every unindexed source overlay (commit_all_writes validates that contract).
  // Preparation/validation may throw; application does not allocate.
  // This is not a RocksDB/durable commit and does not replace general squash().
  static void squash_all_writes( const std::vector< std::shared_ptr< state_delta > >& children );
  // Internal map-only, non-root parent batch. Exclusive access is required
  // through preparation and apply; prepared ownership keeps inputs alive.
  class prepared_change_batch
  {
    struct transfer
    {
      std::shared_ptr< state_delta > child, parent;
      backends::map::map_backend* source;
      backends::map::map_backend* target;
      backends::map::removed_key_set removed;
    };
    std::vector< transfer, node::OwnedMemoryBudget::Allocator< transfer > > _transfers;
    bool _applied = false;
    prepared_change_batch() = default;
    friend class state_delta;

  public:
    prepared_change_batch( const prepared_change_batch& ) = delete;
    prepared_change_batch& operator=( const prepared_change_batch& ) = delete;
    prepared_change_batch( prepared_change_batch&& ) = default;
    prepared_change_batch& operator=( prepared_change_batch&& ) = default;
    // Verified matching allocators and fixed hash/comparison. Does not allocate,
    // perform persistence, or invalidate any general cached root semantics.
    void apply() noexcept;
  };
  static prepared_change_batch prepare_all_changes( const std::vector< std::shared_ptr< state_delta > >& children );
  void commit();
  // Internal database publication: caller has verified the replacement has
  // this node's current parent ID and excludes all readers and writers.
  void replace_parent_after_commit( const std::shared_ptr< state_delta >& parent ) noexcept { _parent = parent; }

  void clear();

  bool is_modified( const key_type& k ) const;
  bool is_removed( const key_type& k ) const;
  bool is_root() const;
  bool is_empty() const;

  uint64_t revision() const;
  void set_revision( uint64_t revision );

  bool is_finalized() const;
  void finalize();

  std::condition_variable_any& cv();
  std::timed_mutex& cv_mutex();

  crypto::multihash merkle_root() const;
  crypto::multihash calculate_merkle_root() const;
  std::vector< protocol::state_delta_entry > get_delta_entries() const;

  const state_node_id& id() const;
  const state_node_id& parent_id() const;
  std::shared_ptr< state_delta > parent() const;
  const protocol::block_header& block_header() const;

  std::shared_ptr< state_delta > make_child( const state_node_id& id              = state_node_id(),
                                             const protocol::block_header& header = protocol::block_header() );
  std::shared_ptr< state_delta > clone( const state_node_id& id, const protocol::block_header& header );

  const std::shared_ptr< backend_type > backend() const;

  // Internal diagnostic scan; caller must exclude backend/delta mutations.
  void inventory_retained_memory( retained_memory_accumulator& inventory ) const;

private:
  void commit_helper();

  std::shared_ptr< state_delta > get_root();
};

} // namespace koinos::state_db::detail
