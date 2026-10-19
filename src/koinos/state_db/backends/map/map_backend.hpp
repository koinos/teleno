#pragma once

#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/backends/map/map_iterator.hpp>
#include <unordered_set>

namespace koinos::state_db::detail { class retained_memory_accumulator; }

namespace koinos::state_db::backends::map {

class map_backend final: public abstract_backend
{
public:
  using key_type   = abstract_backend::key_type;
  using value_type = abstract_backend::value_type;
  using size_type  = abstract_backend::size_type;

  map_backend();
  virtual ~map_backend() override;

  // Iterators
  // Iterator wrappers allocate; allocation failures must unwind, not terminate.
  virtual iterator begin() override;
  virtual iterator end() override;

  // Modifiers
  virtual void put( const key_type& k, const value_type& v ) override;
  void put_owned( const key_type& k, value_ptr v ) override;
  virtual value_ptr get( const key_type& ) const override;
  virtual void erase( const key_type& k ) override;
  virtual void clear() noexcept override;

  virtual size_type size() const noexcept override;

  // Lookup
  virtual iterator find( const key_type& k ) override;
  virtual iterator lower_bound( const key_type& k ) override;

  virtual void start_write_batch() override;
  virtual void end_write_batch( write_durability durability = write_durability::default_async ) override;

  virtual void store_metadata() override;

  virtual std::shared_ptr< abstract_backend > clone() const override;

  // Transfer already allocated writes, replacing owners without copying values.
  // Uses this backend's fixed std::allocator/std::less<string> map. Callers must
  // exclude readers/writers and validate the entire batch before any transfer.
  void merge_writes_from( map_backend& source, std::unordered_set< key_type >& removed ) noexcept;

  // Internal diagnostic scan; caller must exclude all map mutations.
  // Avoids payload copies and allocating virtual iterator wrappers.
  void inventory_retained_memory( ::koinos::state_db::detail::retained_memory_accumulator& inventory ) const;

private:
  // Clones share immutable payloads, not mutable maps. A write replaces the
  // value owner and cannot change a previously read or cloned value.
  map_iterator::map_impl _map;
  protocol::block_header _header;
};

} // namespace koinos::state_db::backends::map
