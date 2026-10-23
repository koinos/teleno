#include <koinos/state_db/state_delta.hpp>
#include <koinos/state_db/retained_memory.hpp>

#include <koinos/crypto/merkle_tree.hpp>

namespace koinos::state_db::detail {

using backend_type = state_delta::backend_type;
using value_type   = state_delta::value_type;

void state_delta::inventory_retained_memory( retained_memory_accumulator& inventory ) const
{
  auto& usage = inventory.usage;
  inventory.add( usage.state_nodes, 1 );
  inventory.add( _finalized ? usage.finalized_state_nodes : usage.writable_state_nodes, 1 );
  inventory.add( usage.tombstone_bucket_count, _removed_objects.bucket_count() );
  for( const auto& key: _removed_objects )
    inventory.tombstone( key );
  if( inventory.first_backend( _backend.get() ) )
  {
    if( const auto* map = dynamic_cast< const backends::map::map_backend* >( _backend.get() ) )
    {
      inventory.add( usage.map_backends, 1 );
      map->inventory_retained_memory( inventory );
    }
    else
      inventory.add( usage.unmeasured_backends, 1 );
  }
}

state_delta::state_delta( const std::optional< std::filesystem::path >& p )
{
  if( p )
  {
    auto backend = ::koinos::node::make_owned_shared< backends::rocksdb::rocksdb_backend >();
    backend->open( *p );
    _backend = backend;
  }
  else
  {
    _backend = ::koinos::node::make_owned_shared_extra< backends::map::map_backend >( 256 );
  }

  _revision    = _backend->revision();
  _id          = _backend->id();
  _merkle_root = _backend->merkle_root();
}

state_delta::state_delta( std::shared_ptr< backend_type > backend ):
    _backend( std::move( backend ) )
{
  KOINOS_ASSERT( _backend, internal_error, "state delta backend is null" );
  _revision    = _backend->revision();
  _id          = _backend->id();
  _merkle_root = _backend->merkle_root();
}

void state_delta::put( const key_type& k, const value_type& v )
{
  _backend->put( k, v );
}

void state_delta::put_owned( const key_type& k, value_ptr v )
{
  _backend->put_owned( k, std::move( v ) );
}

void state_delta::erase( const key_type& k, bool preserve_tombstone )
{
  if( find( k ) || preserve_tombstone )
  {
    node::OwnedMemoryBudget::KeyScope key_reservation( k.size() );
    // Prepare the fallible marker before erasing a live map entry. In
    // particular, a local budget refusal cannot remove a value without its
    // tombstone. Overlay publication retains the stronger all-fork contract.
    const auto [ marker, inserted ] = _removed_objects.insert( k );
    try { _backend->erase( k ); }
    catch( ... ) { if( inserted ) _removed_objects.erase( marker ); throw; }
  }
}

state_delta::value_ptr state_delta::find( const key_type& key ) const
{
  if( auto val_ptr = _backend->get( key ); val_ptr )
    return val_ptr;

  if( is_removed( key ) )
    return nullptr;

  return is_root() ? nullptr : _parent->find( key );
}

void state_delta::squash()
{
  if( is_root() )
    return;

  // If an object is removed here and exists in the parent, it needs to only be removed in the parent
  // If an object is modified here, but removed in the parent, it needs to only be modified in the parent
  // These are O(m log n) operations. Because of this, squash should only be called from anonymouse state
  // nodes, whose modifications are much smaller
  for( const key_type& r_key: _removed_objects )
  {
    _parent->_backend->erase( r_key );

    if( !_parent->is_root() )
    {
      node::OwnedMemoryBudget::KeyScope key_reservation( r_key.size() );
      _parent->_removed_objects.insert( r_key );
    }
  }

  for( auto itr = _backend->begin(); itr != _backend->end(); ++itr )
  {
    _parent->_backend->put_owned( itr.key(), itr.share_value() );

    if( !_parent->is_root() )
    {
      _parent->_removed_objects.erase( itr.key() );
    }
  }
}

void state_delta::squash_all_writes( const std::vector< std::shared_ptr< state_delta > >& children )
{
  using map_backend = backends::map::map_backend;
  const auto* first_map = !children.empty() && children.front()
    ? dynamic_cast< const map_backend* >( children.front()->_backend.get() ) : nullptr;
  node::OwnedMemoryBudget::Scope memory_scope( first_map ? first_map->memory_budget()
    : node::OwnedMemoryBudget::current() );
  struct transfer
  {
    map_backend* source;
    map_backend* target;
    backends::map::removed_key_set* removed;
  };
  std::vector< transfer, node::OwnedMemoryBudget::Allocator< transfer > > prepared;
  prepared.reserve( children.size() );
  std::unordered_set< state_delta*, std::hash< state_delta* >, std::equal_to< state_delta* >,
    node::OwnedMemoryBudget::Allocator< state_delta* > > parents;
  parents.reserve( children.size() );
  for( const auto& child: children )
  {
    KOINOS_ASSERT( child && child->_parent, illegal_argument, "write batch requires a child state delta" );
    KOINOS_ASSERT( !child->_parent->is_finalized(), node_finalized, "cannot commit to a finalized node" );
    KOINOS_ASSERT( child->_removed_objects.empty(), illegal_argument, "atomic write batch does not accept removals" );
    KOINOS_ASSERT( parents.insert( child->_parent.get() ).second,
                   illegal_argument, "atomic write batch requires distinct parents" );
    auto* source = dynamic_cast< map_backend* >( child->_backend.get() );
    auto* target = dynamic_cast< map_backend* >( child->_parent->_backend.get() );
    KOINOS_ASSERT( source && target, illegal_argument, "atomic write batch requires map backends" );
    KOINOS_ASSERT( source->memory_budget() == target->memory_budget() &&
                   target->memory_budget() == child->_parent->_removed_objects.get_allocator().memory_budget(),
                   illegal_argument, "atomic write batch requires matching memory owners" );
    prepared.push_back( { source, target, &child->_parent->_removed_objects } );
  }
  // Everything that allocates or can reject the batch is above this boundary.
  // Verified matching map allocators allow node-handle transfer without allocation.
  for( const auto& item: prepared )
    item.target->merge_writes_from( *item.source, *item.removed );
}

state_delta::prepared_change_batch state_delta::prepare_all_changes(
  const std::vector< std::shared_ptr< state_delta > >& children )
{
  const auto* first_map = !children.empty() && children.front()
    ? dynamic_cast< const backends::map::map_backend* >( children.front()->_backend.get() ) : nullptr;
  node::OwnedMemoryBudget::Scope memory_scope( first_map ? first_map->memory_budget()
    : node::OwnedMemoryBudget::current() );
  prepared_change_batch batch;
  batch._transfers.reserve( children.size() );
  std::unordered_set< state_delta*, std::hash< state_delta* >, std::equal_to< state_delta* >,
    node::OwnedMemoryBudget::Allocator< state_delta* > > parents;
  std::unordered_set< backend_type*, std::hash< backend_type* >, std::equal_to< backend_type* >,
    node::OwnedMemoryBudget::Allocator< backend_type* > > targets, sources;
  parents.reserve( children.size() );
  targets.reserve( children.size() );
  sources.reserve( children.size() );
  for( const auto& child: children )
  {
    KOINOS_ASSERT( child && child->_parent, illegal_argument, "change batch requires child state deltas" );
    const auto& parent = child->_parent;
    KOINOS_ASSERT( !parent->is_root(), illegal_argument, "change batch does not support root parents" );
    KOINOS_ASSERT( !parent->is_finalized(), node_finalized, "cannot commit to a finalized node" );
    KOINOS_ASSERT( parents.insert( parent.get() ).second,
                   illegal_argument, "change batch requires distinct parents" );
    auto* source = dynamic_cast< backends::map::map_backend* >( child->_backend.get() );
    auto* target = dynamic_cast< backends::map::map_backend* >( parent->_backend.get() );
    KOINOS_ASSERT( source && target, illegal_argument, "change batch requires map backends" );
    KOINOS_ASSERT( source->memory_budget() == target->memory_budget() &&
                   target->memory_budget() == parent->_removed_objects.get_allocator().memory_budget() &&
                   target->memory_budget() == child->_removed_objects.get_allocator().memory_budget() &&
                   target->memory_budget() == node::OwnedMemoryBudget::current(),
                   illegal_argument, "change batch requires matching memory owners" );
    KOINOS_ASSERT( targets.insert( target ).second && sources.insert( source ).second,
                   illegal_argument, "change batch requires distinct backends" );
    batch._transfers.push_back( { child, parent, source, target, {} } );
    auto& prepared = batch._transfers.back().removed;
    const auto old_size = parent->_removed_objects.size();
    KOINOS_ASSERT( old_size <= prepared.max_size() &&
                   child->_removed_objects.size() <= prepared.max_size() - old_size,
                   illegal_argument, "change batch tombstone capacity overflow" );
    const auto upper = old_size + child->_removed_objects.size();
    // Reserve a separate bucket workspace, not the live parent's set: even a
    // rejected preparation must preserve its original buckets and all records.
    // Publication moves existing nodes rather than copying old keys/map values.
    if( upper )
      prepared.reserve( upper );
  }
  for( const auto& item: batch._transfers )
    KOINOS_ASSERT( !parents.count( item.child.get() ) && !targets.count( item.source ),
                   illegal_argument, "change batch parents and sources must be disjoint" );
  return batch;
}

void state_delta::prepared_change_batch::apply() noexcept
{
  if( _applied )
    return;
  for( auto& item: _transfers )
  {
    auto& old_removed = item.parent->_removed_objects;
    while( !old_removed.empty() )
      item.removed.insert( old_removed.extract( old_removed.begin() ) );
    auto& changes = item.child->_removed_objects;
    while( !changes.empty() )
    {
      auto marker = changes.extract( changes.begin() );
      item.target->erase( marker.value() );
      item.removed.insert( std::move( marker ) );
    }
    // Original squash processes removals first, then writes; a source value
    // wins even when that source also carries a tombstone for the same key.
    item.target->merge_writes_from( *item.source, item.removed );
    old_removed.swap( item.removed );
  }
  _applied = true;
}

void state_delta::commit()
{
  /**
   * commit works in two distinct phases. The first is head recursion until we are at the root
   * delta. At the root, we grab the backend and begin a write batch that will encompass all
   * state writes and the final write of the metadata.
   *
   * The second phase is popping off the stack, writing state to the backend. After all deltas
   * have been written to the backend, we write metadata to the backend and end the write batch.
   *
   * The result is this delta becomes the new root delta and state is written to the root backend
   * atomically.
   */
  KOINOS_ASSERT( !is_root(), internal_error, "cannot commit root" );

  std::vector< std::shared_ptr< state_delta >,
    node::OwnedMemoryBudget::Allocator< std::shared_ptr< state_delta > > > node_stack;
  auto current_node = shared_from_this();

  while( current_node )
  {
    node_stack.push_back( current_node );
    current_node = current_node->_parent;
  }

  // Because we already asserted we were not root, there will always exist a minimum of two nodes in the stack,
  // this and root.
  auto backend = node_stack.back()->_backend;
  auto previous_root = node_stack.back();
  node_stack.pop_back();

  // A map backend has no transactional write batch. Prepare a private map
  // instead: allocation failure must leave the old root and accepted records
  // intact. Immutable payloads remain shared; keys/nodes are separately owned.
  if( dynamic_cast< backends::map::map_backend* >( backend.get() ) )
    backend = backend->clone();

  try
  {
  // Start the write batch
  backend->start_write_batch();

  // While there are nodes on the stack, write them to the backend
  while( node_stack.size() )
  {
    auto& node = node_stack.back();

    for( const key_type& r_key: node->_removed_objects )
    {
      backend->erase( r_key );
    }

    for( auto itr = node->_backend->begin(); itr != node->_backend->end(); ++itr )
    {
      backend->put_owned( itr.key(), itr.share_value() );
    }

    node_stack.pop_back();
  }

  // Update metadata on the backend
  backend->set_block_header( block_header() );
  backend->set_revision( _revision );
  backend->set_id( _id );
  backend->set_merkle_root( merkle_root() );
  backend->store_metadata();

  // End the write batch making the entire merge atomic and durable.
  backend->end_write_batch( backends::write_durability::sync );
  }
  catch( ... ) { backend->abort_write_batch(); throw; }

  // Reset local variables to match new status as root delta
  // Caller-held old nodes retain valid payloads and their memory credits until
  // the final owner releases them. Never invalidate that ownership early.
  _removed_objects.clear();
  _backend = backend;
  _parent.reset();
}

void state_delta::clear()
{
  _backend->clear();
  _removed_objects.clear();

  _revision = 0;
  _id       = crypto::multihash::zero( crypto::multicodec::sha2_256 );
}

bool state_delta::is_modified( const key_type& k ) const
{
  return _backend->get( k ) || _removed_objects.find( k ) != _removed_objects.end();
}

bool state_delta::is_removed( const key_type& k ) const
{
  return _removed_objects.find( k ) != _removed_objects.end();
}

bool state_delta::is_root() const
{
  return !_parent;
}

uint64_t state_delta::revision() const
{
  return _revision;
}

void state_delta::set_revision( uint64_t revision )
{
  _revision = revision;
  if( is_root() )
  {
    _backend->set_revision( revision );
  }
}

bool state_delta::is_finalized() const
{
  return _finalized;
}

void state_delta::finalize()
{
  _finalized = true;
}

std::condition_variable_any& state_delta::cv()
{
  return _cv;
}

std::timed_mutex& state_delta::cv_mutex()
{
  return _cv_mutex;
}

crypto::multihash state_delta::merkle_root() const
{
  if( !_merkle_root )
  {
    _merkle_root = calculate_merkle_root();
  }

  return *_merkle_root;
}

crypto::multihash state_delta::calculate_merkle_root() const
{
  // Do not read or populate _merkle_root here. Writable-delta mutations do not
  // invalidate that finalized-root cache, so pending validation must always
  // calculate from the current backend and tombstone set.
  const auto memory = node::OwnedMemoryBudget::current();
  const auto keys = node::OwnedMemoryBudget::checked_add( _backend->size(), _removed_objects.size() );
  auto hash_workspace = memory.enabled() ? memory.reserve( node::OwnedMemoryBudget::checked_add(
    node::OwnedMemoryBudget::checked_multiply( keys, 1024 ), 65536 ) ) : node::OwnedMemoryBudget::Lease{};
  uint64_t strings = 0;
  if( memory.enabled() )
  {
    for( auto itr = _backend->begin(); itr != _backend->end(); ++itr )
      strings = node::OwnedMemoryBudget::checked_add( strings, node::OwnedMemoryBudget::string_reservation( itr.key().size() ) );
    for( const auto& key: _removed_objects )
      strings = node::OwnedMemoryBudget::checked_add( strings, node::OwnedMemoryBudget::string_reservation( key.size() ) );
  }
  auto string_workspace = memory.reserve( strings );
  std::vector< std::string > object_keys;
  object_keys.reserve( keys );
  for( auto itr = _backend->begin(); itr != _backend->end(); ++itr )
  {
    object_keys.push_back( itr.key() );
  }

  for( const auto& removed: _removed_objects )
  {
    object_keys.push_back( removed );
  }

  std::sort( object_keys.begin(), object_keys.end() );

  std::vector< crypto::multihash > merkle_leafs;
  merkle_leafs.reserve( object_keys.size() * 2 );

  for( const auto& key: object_keys )
  {
    merkle_leafs.emplace_back( crypto::hash( crypto::multicodec::sha2_256, key ) );
    auto val_ptr = _backend->get( key );
    merkle_leafs.emplace_back( crypto::hash( crypto::multicodec::sha2_256, val_ptr ? *val_ptr : std::string() ) );
  }

  return crypto::merkle_tree( crypto::multicodec::sha2_256, merkle_leafs ).root()->hash();
}

const protocol::block_header& state_delta::block_header() const
{
  return _backend->block_header();
}

std::shared_ptr< state_delta > state_delta::make_child( const state_node_id& id, const protocol::block_header& header )
{
  auto* source_map = dynamic_cast< backends::map::map_backend* >( _backend.get() );
  const auto inherited_memory = source_map ? source_map->memory_budget() : node::OwnedMemoryBudget{};
  node::OwnedMemoryBudget::Scope memory_scope( inherited_memory.enabled() ? inherited_memory : node::OwnedMemoryBudget::current() );
  auto child       = ::koinos::node::make_owned_shared_extra< state_delta >(
    node::OwnedMemoryBudget::current().enabled() ? node::OwnedMemoryBudget::checked_add( 256,
      node::OwnedMemoryBudget::checked_multiply( node::OwnedMemoryBudget::protobuf_space_used( header ), 2 ) ) : 0 );
  child->_parent   = shared_from_this();
  child->_id       = id;
  child->_revision = _revision + 1;
  child->_backend  = ::koinos::node::make_owned_shared_extra< backends::map::map_backend >( 256 );
  child->_backend->set_block_header( header );

  return child;
}

std::shared_ptr< state_delta > state_delta::clone( const state_node_id& id, const protocol::block_header& header )
{
  auto* source_map = dynamic_cast< backends::map::map_backend* >( _backend.get() );
  const auto inherited_memory = source_map ? source_map->memory_budget() : node::OwnedMemoryBudget{};
  node::OwnedMemoryBudget::Scope memory_scope( inherited_memory.enabled() ? inherited_memory : node::OwnedMemoryBudget::current() );
  auto new_node              = ::koinos::node::make_owned_shared_extra< state_delta >(
    node::OwnedMemoryBudget::current().enabled() ? node::OwnedMemoryBudget::checked_add( 256,
      node::OwnedMemoryBudget::checked_multiply( node::OwnedMemoryBudget::protobuf_space_used( header ), 2 ) ) : 0 );
  new_node->_parent          = _parent;
  new_node->_backend         = _backend->clone();
  if( !node::OwnedMemoryBudget::current().enabled() ) new_node->_removed_objects = _removed_objects;
  else
  {
    new_node->_removed_objects = backends::map::removed_key_set( node::OwnedMemoryBudget::Allocator< key_type >( node::OwnedMemoryBudget::current() ) );
    new_node->_removed_objects.reserve( _removed_objects.size() );
    for( const auto& key: _removed_objects )
    {
      node::OwnedMemoryBudget::KeyScope key_reservation( key.size() );
      new_node->_removed_objects.insert( key );
    }
  }

  new_node->_id          = id;
  new_node->_revision    = _revision;
  new_node->_merkle_root = _merkle_root;

  new_node->_finalized = _finalized;

  new_node->_backend->set_id( id );
  new_node->_backend->set_revision( _revision );
  new_node->_backend->set_block_header( header );

  if( _merkle_root )
  {
    new_node->_backend->set_merkle_root( *_merkle_root );
  }

  return new_node;
}

const std::shared_ptr< backend_type > state_delta::backend() const
{
  return _backend;
}

const state_node_id& state_delta::id() const
{
  return _id;
}

const state_node_id& state_delta::parent_id() const
{
  static const state_node_id null_id;
  return _parent ? _parent->_id : null_id;
}

std::shared_ptr< state_delta > state_delta::parent() const
{
  return _parent;
}

bool state_delta::is_empty() const
{
  if( _backend->size() )
    return false;
  else if( _parent )
    return _parent->is_empty();

  return true;
}

std::shared_ptr< state_delta > state_delta::get_root()
{
  if( !is_root() )
  {
    if( _parent->is_root() )
      return _parent;
    else
      return _parent->get_root();
  }

  return std::shared_ptr< state_delta >();
}

std::vector< protocol::state_delta_entry > state_delta::get_delta_entries() const
{
  std::vector< std::string > object_keys;
  object_keys.reserve( _backend->size() + _removed_objects.size() );
  for( auto itr = _backend->begin(); itr != _backend->end(); ++itr )
  {
    object_keys.push_back( itr.key() );
  }

  for( const auto& removed: _removed_objects )
  {
    object_keys.push_back( removed );
  }

  std::sort( object_keys.begin(), object_keys.end() );

  std::vector< protocol::state_delta_entry > deltas;
  deltas.reserve( object_keys.size() );

  for( const auto& key: object_keys )
  {
    protocol::state_delta_entry entry;

    // Deserialize the key into a database_key object
    koinos::chain::database_key db_key;
    if( db_key.ParseFromString( key ) )
    {
      entry.mutable_object_space()->set_system( db_key.space().system() );
      entry.mutable_object_space()->set_zone( db_key.space().zone() );
      entry.mutable_object_space()->set_id( db_key.space().id() );

      entry.set_key( db_key.key() );
      auto value = _backend->get( key );

      // Set the optional field if not null
      if( value != nullptr )
        entry.set_value( *value );

      deltas.push_back( entry );
    }
  }

  return deltas;
}

} // namespace koinos::state_db::detail
