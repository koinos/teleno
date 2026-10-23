#pragma once

#include <koinos/state_db/backends/iterator.hpp>
#include "core/owned_memory_budget.hpp"

#include <map>
#include <unordered_set>

namespace koinos::state_db::backends::map {

class map_backend;

using removed_key_set = std::unordered_set< detail::key_type, std::hash< detail::key_type >,
  std::equal_to< detail::key_type >, node::OwnedMemoryBudget::Allocator< detail::key_type > >;

class map_iterator final: public abstract_iterator
{
public:
  using value_type    = abstract_iterator::value_type;
  using map_impl      = std::map< detail::key_type, detail::value_ptr,
    std::less< detail::key_type >,
    node::OwnedMemoryBudget::Allocator< std::pair< const detail::key_type, detail::value_ptr > > >;
  using iterator_impl = map_impl::iterator;

  map_iterator( std::unique_ptr< iterator_impl > itr, const map_impl& map );
  ~map_iterator();

  virtual const value_type& operator*() const override;
  virtual value_ptr share_value() const override;

  virtual const key_type& key() const override;

  virtual abstract_iterator& operator++() override;
  virtual abstract_iterator& operator--() override;

private:
  virtual bool valid() const override;
  virtual std::unique_ptr< abstract_iterator > copy() const override;

  std::unique_ptr< iterator_impl > _itr;
  const map_impl& _map;
};

} // namespace koinos::state_db::backends::map
