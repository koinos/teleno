#include <koinos/mempool/mempool.hpp>
#include <koinos/mempool/state.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <limits>
#include <shared_mutex>
#include <tuple>

#include <boost/multiprecision/cpp_int.hpp>
#include <google/protobuf/arena.h>

#include <koinos/chain/value.pb.h>
#include <koinos/mempool/mempool.pb.h>
#include <koinos/protocol/protocol.pb.h>

#include <koinos/util/base58.hpp>
#include <koinos/util/base64.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>

namespace koinos::mempool {

namespace detail {

using int128_t = boost::multiprecision::int128_t;

google::protobuf::ArenaOptions owned_arena_options()
{
  google::protobuf::ArenaOptions options;
  options.block_alloc = []( size_t bytes ) { return ::operator new( bytes ); };
  options.block_dealloc = []( void* pointer, size_t ) { ::operator delete( pointer ); };
  return options;
}

// Wire bytes are not heap bytes. This supplementary parser envelope includes
// small repeated/unknown protobuf fields and arena block growth; actual native
// allocator coverage is qualified separately. The lease precedes the arena and
// outlives its decoded record, including partial-parse unwinding.
node::OwnedMemoryBudget::Lease cleanup_workspace( size_t wire_size )
{
  const auto memory = node::OwnedMemoryBudget::current();
  if( !memory.enabled() ) return {};
  return memory.reserve( node::OwnedMemoryBudget::checked_add(
    node::OwnedMemoryBudget::checked_multiply( wire_size, 128 ), 65536 ) );
}

template< typename Record >
Record& parse_cleanup_record( const std::string& bytes, google::protobuf::Arena& storage )
{
  // Own partially decoded repeated fields if the pinned protobuf parser throws.
  // Return an arena-backed reference without another heap-message copy.
  auto& record = *google::protobuf::Arena::CreateMessage< Record >( &storage );
  // Stream conversion can absorb an allocation exception into stream state;
  // it also ignores ParseFromIstream's result. Never clean up from a partial
  // or default record after such a failure. Direct parsing propagates allocation
  // failure, and an ordinary decoding failure explicitly rejects preparation.
  KOINOS_ASSERT( bytes.size() <= static_cast< size_t >( std::numeric_limits< int >::max() ) &&
                 record.ParseFromArray( bytes.data(), static_cast< int >( bytes.size() ) ),
                 state_db::internal_error, "mempool cleanup record cannot be decoded" );
  return record;
}

template< typename Record >
Record parse_record( const std::string& bytes )
{
  Record record;
  // The generic stream converter can absorb bad_alloc and return a partial
  // message. Direct parsing preserves allocation failure and rejects damaged
  // stored records before any reader or writer can use their default fields.
  KOINOS_ASSERT( bytes.size() <= static_cast< size_t >( std::numeric_limits< int >::max() ) &&
                 record.ParseFromArray( bytes.data(), static_cast< int >( bytes.size() ) ),
                 state_db::internal_error, "mempool record cannot be decoded" );
  return record;
}

class mempool_impl final
{
private:
  node::OwnedMemoryBudget _memory;
  node::OwnedMemoryBudget::Lease _implementation_memory;
  state_db::database _db;
  pending_record_budget _record_budget;
  using mutex_allocator = node::OwnedMemoryBudget::Allocator<
    std::pair< const crypto::multihash, std::shared_ptr< std::shared_mutex > > >;
  using relevant_mutex_map = std::map< crypto::multihash, std::shared_ptr< std::shared_mutex >,
    std::less< crypto::multihash >, mutex_allocator >;
  relevant_mutex_map _relevant_node_mutexes{
      std::less< crypto::multihash >{}, mutex_allocator( node::OwnedMemoryBudget::current(), 128 ) };

  std::vector< std::unique_lock< std::shared_mutex > > lock_relevant_mutexes(
    std::vector< relevant_mutex_map::const_iterator > targets ) const;

  crypto::multihash tmp_id( crypto::multihash id ) const;
  std::pair< state_db::state_node_ptr, std::shared_ptr< std::shared_mutex > >
  relevant_node( std::optional< crypto::multihash > id, state_db::shared_lock_ptr lock ) const;
  std::pair< state_db::state_node_ptr, std::shared_ptr< std::shared_mutex > >
  relevant_node( std::optional< crypto::multihash > id, state_db::unique_lock_ptr lock ) const;

  void cleanup_account_resources_on_node( state_db::abstract_state_node_ptr node, const pending_transaction_record& record );

  uint64_t remove_pending_transactions_on_node( state_db::abstract_state_node_ptr node,
                                                const std::vector< transaction_id_type >& ids );

  bool check_pending_account_resources_on_node( state_db::abstract_state_node_ptr node,
                                                const account_type& payer,
                                                uint64_t max_payer_resources,
                                                uint64_t trx_resource_limit ) const;

  bool check_account_nonce_on_node( state_db::abstract_state_node_ptr node,
                                    const std::string& account_nonce_key ) const;

  std::string get_pending_nonce_on_node( state_db::abstract_state_node_ptr node, const std::string& account ) const;

  uint64_t get_pending_transaction_count_on_node( state_db::abstract_state_node_ptr node,
                                                  const std::string& account ) const;

  uint64_t add_pending_transaction_to_node( state_db::anonymous_state_node_ptr node,
                                            const pending_transaction_record& record,
                                            state_db::object_value_ptr encoded_record,
                                            uint64_t max_payer_rc );

  uint64_t get_reserved_account_rc_from_node( std::shared_ptr< koinos::state_db::state_node > node,
                                              const account_type& account ) const;

public:
  mempool_impl( state_db::fork_resolution_algorithm algo, pending_record_limits record_limits,
                node::OwnedMemoryBudget memory, node::OwnedMemoryBudget::Lease implementation_memory );
  virtual ~mempool_impl();

  bool has_pending_transaction( const transaction_id_type& id, std::optional< crypto::multihash > block_id ) const;

  pending_transaction_vector get_pending_transactions( uint64_t limit,
                                                               std::optional< crypto::multihash > block_id );

  pending_transaction_vector get_pending_transactions( const std::vector< transaction_id_type >& ids,
                                                               std::optional< crypto::multihash > block_id );

  uint64_t get_reserved_account_rc( const account_type& account ) const;

  bool check_pending_account_resources( const account_type& payer,
                                        uint64_t max_payer_resources,
                                        uint64_t trx_resource_limit,
                                        std::optional< crypto::multihash > block_id = {} ) const;

  bool check_account_nonce( const account_type& payer,
                            const std::string& nonce,
                            std::optional< crypto::multihash > block_id = {} ) const;

  std::string get_pending_nonce( const std::string& account, std::optional< crypto::multihash > block_id ) const;

  uint64_t get_pending_transaction_count( const std::string& account,
                                          std::optional< crypto::multihash > block_id ) const;

  uint64_t pending_transaction_count() const;
  retained_memory_usage get_retained_memory_usage() const;
  pending_record_snapshot get_pending_record_usage() const { return _record_budget.snapshot(); }
  const node::OwnedMemoryBudget& memory_budget() const noexcept { return _memory; }

  uint64_t add_pending_transaction( const pending_transaction& pending_trx,
                                    std::chrono::system_clock::time_point time,
                                    uint64_t max_payer_rc );

  uint64_t remove_pending_transactions( const std::vector< transaction_id_type >& ids );

  uint64_t prune( std::chrono::seconds expiration, std::chrono::system_clock::time_point now );

  bool handle_block( const koinos::broadcast::block_accepted& bam );
  void handle_irreversibility( const koinos::broadcast::block_irreversible& bi );
};

std::string join_account_nonce_key( const std::string& account, const std::string& nonce_bytes )
{
  std::string key;
  key.reserve( node::OwnedMemoryBudget::checked_add( account.size(), nonce_bytes.size() ) );
  key.append( account );
  key.append( nonce_bytes );
  return key;
}

std::string create_account_nonce_key( const protocol::transaction& transaction )
{
  auto nonce_bytes = util::converter::as< std::string >(
    util::converter::to< chain::value_type >( transaction.header().nonce() ).uint64_value() );
  const auto& account = transaction.header().payee().empty() ? transaction.header().payer()
                                                             : transaction.header().payee();
  return join_account_nonce_key( account, nonce_bytes );
}

mempool_impl::mempool_impl( state_db::fork_resolution_algorithm algo, pending_record_limits record_limits,
                          node::OwnedMemoryBudget memory, node::OwnedMemoryBudget::Lease implementation_memory )
  : _memory( std::move( memory ) ), _implementation_memory( std::move( implementation_memory ) ),
    _record_budget( record_limits, _memory )
{
  auto lock = _db.get_unique_lock();
  _db.open( {}, []( state_db::state_node_ptr ) {}, algo, lock );
  auto node_id     = _db.get_head( lock )->id();
  auto relevant_id = tmp_id( node_id );
  [[maybe_unused]]
  auto node = _db.create_writable_node( node_id, relevant_id, protocol::block_header(), lock );
  assert( node );
  _relevant_node_mutexes[ relevant_id ] = node::make_owned_shared< std::shared_mutex >();
}

mempool_impl::~mempool_impl() = default;

std::vector< std::unique_lock< std::shared_mutex > > mempool_impl::lock_relevant_mutexes(
  std::vector< relevant_mutex_map::const_iterator > targets ) const
{
  // Admission and cleanup enumerate retained forks differently. Lock every
  // selected fork in map-key order without changing overlay/publication order.
  std::sort( targets.begin(), targets.end(), []( auto lhs, auto rhs ) {
    return std::less< crypto::multihash >{}( lhs->first, rhs->first );
  } );
  KOINOS_ASSERT( std::adjacent_find( targets.begin(), targets.end() ) == targets.end(),
                 state_db::internal_error, "duplicate relevant node mutex" );

  std::vector< std::unique_lock< std::shared_mutex > > locks;
  locks.reserve( targets.size() );
  for( auto target: targets ) locks.emplace_back( *target->second );
  return locks;
}

crypto::multihash mempool_impl::tmp_id( crypto::multihash id ) const
{
  return crypto::hash( crypto::multicodec::sha2_256, id );
}

std::pair< state_db::state_node_ptr, std::shared_ptr< std::shared_mutex > >
mempool_impl::relevant_node( std::optional< crypto::multihash > id, state_db::shared_lock_ptr lock ) const
{
  state_db::state_node_ptr node;
  std::shared_ptr< std::shared_mutex > relevant_mutex;

  if( id )
    node = _db.get_node( *id, lock );
  else
    node = _db.get_head( lock );

  if( node )
  {
    auto relevant_id = tmp_id( node->id() );
    node             = _db.get_node( relevant_id, lock );
    relevant_mutex   = _relevant_node_mutexes.at( relevant_id );
  }

  return std::make_pair( node, relevant_mutex );
}

std::pair< state_db::state_node_ptr, std::shared_ptr< std::shared_mutex > >
mempool_impl::relevant_node( std::optional< crypto::multihash > id, state_db::unique_lock_ptr lock ) const
{
  state_db::state_node_ptr node;
  std::shared_ptr< std::shared_mutex > relevant_mutex;

  if( id )
    node = _db.get_node( *id, lock );
  else
    node = _db.get_head( lock );

  if( node )
  {
    auto relevant_id = tmp_id( node->id() );
    node             = _db.get_node( relevant_id, lock );
    relevant_mutex   = _relevant_node_mutexes.at( relevant_id );
  }

  return std::make_pair( node, relevant_mutex );
}

bool mempool_impl::handle_block( const koinos::broadcast::block_accepted& bam )
{
  auto block_workspace = _memory.enabled() ? _memory.reserve( node::OwnedMemoryBudget::native_message_workspace(
    node::OwnedMemoryBudget::protobuf_space_used( bam ) ) )
    : node::OwnedMemoryBudget::Lease{};
  auto block_id = util::converter::to< crypto::multihash >( bam.block().id() );
  auto previous_id = util::converter::to< crypto::multihash >( bam.block().header().previous() );
  auto lock = _db.get_unique_lock();
  auto mutexes = _relevant_node_mutexes;
  bool linked = false;
  uint64_t removed_count = 0;
  try
  {
    _db.with_reversible_fork_update( lock, [&] {
      const auto root_id = _db.get_root( lock )->id();
      const auto parent_id = root_id != _db.get_head( lock )->id() ? previous_id : root_id;
      auto node = _db.get_node( tmp_id( parent_id ), lock );
      if( !node ) return;
      node = _db.clone_node( node->id(), block_id, bam.block().header(), lock );
      KOINOS_ASSERT( node, state_db::internal_error, "accepted block already has a pending state" );
      std::vector< transaction_id_type > ids;
      ids.reserve( bam.block().transactions_size() );
      for( const auto& transaction: bam.block().transactions() ) ids.push_back( transaction.id() );
      removed_count = remove_pending_transactions_on_node( node, ids );
      _db.finalize_node( block_id, lock );
      const auto relevant_id = tmp_id( block_id );
      auto pending = _db.create_writable_node( block_id, relevant_id, protocol::block_header(), lock );
      KOINOS_ASSERT( pending, state_db::internal_error, "cannot prepare pending state for accepted block" );
      _relevant_node_mutexes.emplace( relevant_id, ::koinos::node::make_owned_shared< std::shared_mutex >() );
      linked = true;
    } );
  }
  catch( ... ) { _relevant_node_mutexes.swap( mutexes ); throw; }
  // Publication has finished. An observer/logging allocation must not turn a
  // successfully linked block into a reported failure or trigger a second start.
  if( linked && bam.live() && removed_count )
    try { LOG( info ) << "Removed " << removed_count << " included transaction(s) via block - Height: "
                     << bam.block().header().height() << ", ID: " << util::to_hex( bam.block().id() ); }
    catch( ... ) {}
  return linked;
}

void mempool_impl::handle_irreversibility( const koinos::broadcast::block_irreversible& bi )
{
  auto block_id = util::converter::to< crypto::multihash >( bi.topology().id() );
  auto lock     = _db.get_unique_lock();
  if( auto lib = _db.get_node( block_id, lock ); lib )
  {
    _db.commit_node( block_id, lock );

    // For all node mutexes
    for( auto itr = _relevant_node_mutexes.begin(); itr != _relevant_node_mutexes.end(); )
    {
      // If the node was removed during commit, remove the mutex
      if( !_db.contains_node( itr->first, lock ) )
        itr = _relevant_node_mutexes.erase( itr );
      else
        ++itr;
    }
  }
}

bool mempool_impl::has_pending_transaction( const transaction_id_type& id,
                                            std::optional< crypto::multihash > block_id ) const
{
  auto lock = _db.get_shared_lock();

  if( auto [ node, mutex ] = relevant_node( block_id, lock ); node )
  {
    std::shared_lock node_lock( *mutex );
    return node->get_object( space::transaction_index(), id ) != nullptr;
  }

  return false;
}

pending_transaction_vector mempool_impl::get_pending_transactions( uint64_t limit,
                                                                           std::optional< crypto::multihash > block_id )
{
  KOINOS_ASSERT( limit <= constants::max_request_limit,
                 pending_transaction_request_overflow,
                 "requested too many pending transactions. max: ${max}",
                 ( "max", constants::max_request_limit ) );

  auto lock = _db.get_shared_lock();

  // If the root is the head, we have not heard of blocks yet.
  // The passed block ID may be legit, but in the past. Return the transactions we know about.
  if( _db.get_head( lock )->id() == _db.get_root( lock )->id() )
    block_id.reset();

  auto [ node, mutex ] = relevant_node( block_id, lock );

  KOINOS_ASSERT( node,
                 pending_transaction_unknown_block,
                 "cannot retrieve pending transactions from an unknown block" );

  std::shared_lock node_lock( *mutex );
  pending_transaction_vector pending_transactions(
    node::OwnedMemoryBudget::Allocator< pending_transaction >{ _memory } );
  // Grow with actual results. Every vector/field allocation reserves before
  // allocation; no second scan or wire-size envelope owns escaped fields.
  state_db::object_key next = state_db::object_key();

  while( pending_transactions.size() < limit )
  {
    auto [ value, key ] = node->get_next_object( space::pending_transaction(), next );

    if( !value )
      break;

    next = key;

    auto record = parse_record< pending_transaction_record >( *value );
    // The decoded record is temporary. Move its transaction into the caller's
    // owned vector instead of allocating and copying every nested field again.
    pending_transactions.push_back( std::move( *record.mutable_pending_transaction() ) );
  }

  return pending_transactions;
}

pending_transaction_vector
mempool_impl::get_pending_transactions( const std::vector< transaction_id_type >& ids,
                                        std::optional< crypto::multihash > block_id )
{
  KOINOS_ASSERT( ids.size() <= constants::max_request_limit,
                 pending_transaction_request_overflow,
                 "requested too many pending transactions. max: ${max}",
                 ( "max", constants::max_request_limit ) );

  auto lock = _db.get_shared_lock();

  // If the root is the head, we have not heard of blocks yet.
  // The passed block ID may be legit, but in the past. Return the transactions we know about.
  if( _db.get_head( lock )->id() == _db.get_root( lock )->id() )
    block_id.reset();

  auto [ node, mutex ] = relevant_node( block_id, lock );

  KOINOS_ASSERT( node,
                 pending_transaction_unknown_block,
                 "cannot retrieve pending transactions from an unknown block" );

  std::shared_lock node_lock( *mutex );
  pending_transaction_vector pending_transactions(
    node::OwnedMemoryBudget::Allocator< pending_transaction >{ _memory } );

  for( auto itr = ids.begin(); itr != ids.end(); ++itr )
  {
    auto seq_obj = node->get_object( space::transaction_index(), *itr );
    if( !seq_obj )
      continue;

    auto ptx_obj = node->get_object( space::pending_transaction(), *seq_obj );
    if( !ptx_obj )
      continue;

    auto record = parse_record< pending_transaction_record >( *ptx_obj );
    pending_transactions.push_back( std::move( *record.mutable_pending_transaction() ) );
  }

  return pending_transactions;
}

uint64_t mempool_impl::get_reserved_account_rc_from_node( std::shared_ptr< koinos::state_db::state_node > node,
                                                          const account_type& account ) const
{
  uint64_t max_rc     = 0;
  uint64_t current_rc = 0;

  if( auto obj = node->get_object( space::address_resources(), account ); obj )
  {
    auto arr   = parse_record< address_resource_record >( *obj );
    max_rc     = arr.max_rc();
    current_rc = arr.current_rc();
  }

  return max_rc - current_rc;
}

uint64_t mempool_impl::get_reserved_account_rc( const account_type& account ) const
{
  auto lock  = _db.get_shared_lock();
  auto nodes = _db.get_all_nodes( lock );

  uint64_t pending_rc = 0;
  for( auto node: nodes )
  {
    auto rc    = get_reserved_account_rc_from_node( node, account );
    pending_rc = std::max( rc, pending_rc );
  }

  return pending_rc;
}

bool mempool_impl::check_pending_account_resources( const account_type& payer,
                                                    uint64_t max_payer_resources,
                                                    uint64_t trx_resource_limit,
                                                    std::optional< crypto::multihash > block_id ) const
{
  auto lock = _db.get_shared_lock();

  if( block_id.has_value() )
  {
    auto [ node, mutex ] = relevant_node( block_id, lock );

    KOINOS_ASSERT( node,
                   pending_transaction_unknown_block,
                   "cannot check pending account resources from an unknown block" );

    std::shared_lock< std::shared_mutex > node_mutex( *mutex );
    return check_pending_account_resources_on_node( node, payer, max_payer_resources, trx_resource_limit );
  }

  auto nodes = _db.get_all_nodes( lock );

  for( auto node: nodes )
  {
    if( node->is_finalized() )
      continue;

    std::shared_lock< std::shared_mutex > node_lock( *_relevant_node_mutexes.at( node->id() ) );

    if( !check_pending_account_resources_on_node( node, payer, max_payer_resources, trx_resource_limit ) )
      return false;
  }

  return true;
}

bool mempool_impl::check_pending_account_resources_on_node( state_db::abstract_state_node_ptr node,
                                                            const account_type& payer,
                                                            uint64_t max_payer_resources,
                                                            uint64_t trx_resource_limit ) const
{
  if( auto obj = node->get_object( space::address_resources(), payer ); obj )
  {
    auto arr = parse_record< address_resource_record >( *obj );

    int128_t max_resource_delta = int128_t( max_payer_resources ) - int128_t( arr.max_rc() );
    int128_t new_resources      = int128_t( arr.current_rc() ) + max_resource_delta - int128_t( trx_resource_limit );
    return new_resources >= 0;
  }

  return trx_resource_limit <= max_payer_resources;
}

bool mempool_impl::check_account_nonce( const account_type& payee,
                                        const std::string& nonce,
                                        std::optional< crypto::multihash > block_id ) const
{
  auto lock = _db.get_shared_lock();

  auto nonce_bytes =
    util::converter::as< std::string >( util::converter::to< chain::value_type >( nonce ).uint64_value() );

  auto account_nonce_key = join_account_nonce_key( payee, nonce_bytes );

  if( block_id.has_value() )
  {
    auto [ node, mutex ] = relevant_node( block_id, lock );

    KOINOS_ASSERT( node,
                   pending_transaction_unknown_block,
                   "cannot check pending account resources from an unknown block" );

    std::shared_lock< std::shared_mutex > node_lock( *mutex );
    return check_account_nonce_on_node( node, account_nonce_key );
  }
  else
  {
    auto nodes = _db.get_all_nodes( lock );

    for( auto node: nodes )
    {
      if( node->is_finalized() )
        continue;

      std::shared_lock< std::shared_mutex > node_lock( *_relevant_node_mutexes.at( node->id() ) );

      if( check_account_nonce_on_node( node, account_nonce_key ) )
        return true;
    }

    return false;
  }
}

bool mempool_impl::check_account_nonce_on_node( state_db::abstract_state_node_ptr node,
                                                const std::string& account_nonce_key ) const
{
  return node->get_object( space::account_nonce(), account_nonce_key ) == nullptr;
}

std::string mempool_impl::get_pending_nonce( const std::string& account,
                                             std::optional< crypto::multihash > block_id ) const
{
  auto lock = _db.get_shared_lock();
  state_db::state_node_ptr node;
  std::shared_ptr< std::shared_mutex > mutex;

  if( block_id.has_value() )
    std::tie( node, mutex ) = relevant_node( block_id, lock );
  else
    std::tie( node, mutex ) = relevant_node( _db.get_head( lock )->id(), lock );

  KOINOS_ASSERT( node, pending_transaction_unknown_block, "cannot check pending nonce from an unknown block" );

  std::shared_lock< std::shared_mutex > node_lock( *mutex );
  return get_pending_nonce_on_node( node, account );
}

std::string mempool_impl::get_pending_nonce_on_node( state_db::abstract_state_node_ptr node,
                                                     const std::string& account ) const
{
  auto nonce_bytes = util::converter::as< std::string >( std::numeric_limits< uint64_t >::max() );

  auto account_nonce_key = join_account_nonce_key( account, nonce_bytes );

  const auto [ value, key ] = node->get_prev_object( space::account_nonce(), account_nonce_key );

  if( !value )
    return "";

  // The predecessor can belong to a shorter account. The three-iterator
  // mismatch overload would read beyond that key when its terminator also
  // matches the requested account's next byte.
  if( key.size() < account.size() || !std::equal( account.begin(), account.end(), key.begin() ) )
    return "";

  // Extract the nonce value from the returned key
  chain::value_type nonce;
  nonce.set_uint64_value( util::converter::to< uint64_t >( key.substr( account.size() ) ) );

  return util::converter::as< std::string >( nonce );
}

uint64_t mempool_impl::get_pending_transaction_count( const std::string& account,
                                                      std::optional< crypto::multihash > block_id ) const
{
  auto lock = _db.get_shared_lock();
  state_db::state_node_ptr node;
  std::shared_ptr< std::shared_mutex > mutex;

  if( block_id.has_value() )
    std::tie( node, mutex ) = relevant_node( block_id, lock );
  else
    std::tie( node, mutex ) = relevant_node( _db.get_head( lock )->id(), lock );

  KOINOS_ASSERT( node, pending_transaction_unknown_block, "cannot check pending nonce from an unknown block" );

  std::shared_lock< std::shared_mutex > node_lock( *mutex );
  return get_pending_transaction_count_on_node( node, account );
}

uint64_t mempool_impl::pending_transaction_count() const
{
  auto lock = _db.get_shared_lock();
  auto [ node, mutex ] = relevant_node( _db.get_head( lock )->id(), lock );
  KOINOS_ASSERT( node, pending_transaction_unknown_block, "cannot count pending transactions from an unknown head" );
  std::shared_lock node_lock( *mutex );
  uint64_t count = 0;
  state_db::object_key key;
  for( ;; )
  {
    auto [ value, next ] = node->get_next_object( space::pending_transaction(), key );
    if( !value ) break;
    ++count;
    key = std::move( next );
  }
  return count;
}

retained_memory_usage mempool_impl::get_retained_memory_usage() const
{
  // Every private mempool writer already holds this exclusive database lock.
  // Holding it also prevents relevant-mutex/index changes during the scan.
  auto lock = _db.get_unique_lock();
  return { _db.get_retained_memory_usage( lock ), _relevant_node_mutexes.size() };
}

uint64_t mempool_impl::get_pending_transaction_count_on_node( state_db::abstract_state_node_ptr node,
                                                              const std::string& account ) const
{
  auto nonce_bytes = util::converter::as< std::string >( std::numeric_limits< uint64_t >::max() );

  auto account_nonce_key = join_account_nonce_key( account, nonce_bytes );

  uint64_t count = 0;

  // Perform a linear walk to count pending transactions for an account.
  // This is an O(n) algorithm for the number of transactions per account.
  // We are going to start with a small limit, so this value will be double digits.
  // As an optimization, we should add another table to track the count separately.
  // But that will be done later due to the added complexity and potential
  // introduction of errors.
  while( true )
  {
    const auto [ value, key ] = node->get_prev_object( space::account_nonce(), account_nonce_key );

    if( !value )
      break;

    if( key.size() < account.size() || !std::equal( account.begin(), account.end(), key.begin() ) )
      break;

    account_nonce_key = key;
    count++;
  }

  return count;
}

uint64_t mempool_impl::add_pending_transaction_to_node( state_db::anonymous_state_node_ptr node,
                                                        const pending_transaction_record& record,
                                                        state_db::object_value_ptr encoded_record,
                                                        uint64_t max_payer_rc )
{
  uint64_t rc_used        = 0;
  const auto& transaction = record.pending_transaction().transaction();

  KOINOS_ASSERT(
    check_pending_account_resources_on_node( node,
                                             transaction.header().payer(),
                                             max_payer_rc,
                                             transaction.header().rc_limit() ),
    pending_transaction_exceeds_resources,
    "transaction would exceed maximum resources for account: ${a}",
    ( "a", util::encode_base58( util::converter::as< std::vector< std::byte > >( transaction.header().payer() ) ) ) );

  KOINOS_ASSERT( node->get_object( space::transaction_index(), transaction.id() ) == nullptr,
                 pending_transaction_insertion_failure,
                 "cannot insert duplicate transaction" );

  auto account_nonce_key = create_account_nonce_key( transaction );

  KOINOS_ASSERT(
    check_account_nonce_on_node( node, account_nonce_key ),
    pending_transaction_nonce_conflict,
    "cannot insert transaction for account with duplicate nonce - account: ${a}, nonce: ${n}",
    ( "a", util::to_base58( transaction.header().payer() ) )( "n", util::to_base64( transaction.header().nonce() ) ) );

  // Grab the latest metadata object if it exists
  mempool_metadata metadata;
  if( auto obj = node->get_object( space::mempool_metadata(), std::string{} ); obj )
    metadata = parse_record< mempool_metadata >( *obj );
  else
    metadata.set_seq_num( 1 );

  uint64_t tx_id = metadata.seq_num();

  metadata.set_seq_num( metadata.seq_num() + 1 );

  // Update metadata
  auto obj = util::converter::as< std::string >( metadata );
  assert( !obj.empty() );
  node->put_object( space::mempool_metadata(), std::string{}, &obj );

  std::string transaction_index_bytes = util::converter::as< std::string >( tx_id );
  node->put_object_owned( space::pending_transaction(), transaction_index_bytes, std::move( encoded_record ) );
  node->put_object( space::transaction_index(), transaction.id(), &transaction_index_bytes );

  address_resource_record arr;
  if( auto obj = node->get_object( space::address_resources(), transaction.header().payer() ); obj )
  {
    arr = parse_record< address_resource_record >( *obj );

    int128_t max_rc_delta = int128_t( max_payer_rc ) - int128_t( arr.max_rc() );
    int128_t new_rc       = int128_t( arr.current_rc() ) + max_rc_delta - int128_t( transaction.header().rc_limit() );

    arr.set_max_rc( max_payer_rc );
    arr.set_current_rc( new_rc.convert_to< uint64_t >() );

    rc_used = max_payer_rc - arr.current_rc();
  }
  else
  {
    arr.set_max_rc( max_payer_rc );
    arr.set_current_rc( max_payer_rc - transaction.header().rc_limit() );

    rc_used = transaction.header().rc_limit();
  }

  auto address_resource_bytes = util::converter::as< std::string >( arr );
  node->put_object( space::address_resources(), transaction.header().payer(), &address_resource_bytes );

  std::string account_nonce_value;
  node->put_object( space::account_nonce(), account_nonce_key, &account_nonce_value );

  return rc_used;
}

uint64_t mempool_impl::add_pending_transaction( const pending_transaction& pending_trx,
                                                std::chrono::system_clock::time_point time,
                                                uint64_t max_payer_rc )
{
  uint64_t rc_used = 0;

  // Reserve a conservative native admission envelope before protobuf copying,
  // encoding, temporary vectors and per-fork staging. Retained map/record
  // allocations have their own lifetime-owned credits and survive this lease.
  auto input_workspace = _memory.enabled() ? _memory.reserve( node::OwnedMemoryBudget::native_message_workspace(
    node::OwnedMemoryBudget::protobuf_space_used( pending_trx ) ) )
    : node::OwnedMemoryBudget::Lease{};
  auto lock  = _db.get_unique_lock();
  auto fork_workspace = _memory.enabled() ? _memory.reserve( node::OwnedMemoryBudget::checked_multiply(
    node::OwnedMemoryBudget::checked_add( _relevant_node_mutexes.size(), 1 ), 4096 ) )
    : node::OwnedMemoryBudget::Lease{};
  auto nodes = _db.get_all_nodes( lock );

  try
  {
    std::vector< state_db::anonymous_state_node_ptr > anonymous_state_nodes;
    auto [ tmp_head, head_mutex ] = relevant_node( _db.get_head( lock )->id(), lock );

    {
      std::shared_lock< std::shared_mutex > head_lock( *head_mutex );

      // The pinned protobuf copy path can throw before recording ownership of
      // newly allocated repeated elements. The arena owns those elements even
      // when copying the incoming transaction fails partway through.
      google::protobuf::Arena record_storage( owned_arena_options() );
      auto& record = *google::protobuf::Arena::CreateMessage< pending_transaction_record >( &record_storage );
      *record.mutable_pending_transaction() = pending_trx;
      record.set_timestamp(
        std::chrono::duration_cast< std::chrono::milliseconds >( time.time_since_epoch() ).count() );
      const auto encoded_record = _record_budget.encode( record.ByteSizeLong(), [&record] {
        std::string bytes;
        KOINOS_ASSERT( record.SerializeToString( &bytes ), state_db::internal_error,
                       "pending record cannot be encoded" );
        return bytes;
      } );

      bool trx_added = false;

      for( auto state_node: nodes )
      {
        if( state_node->is_finalized() )
          continue;

        auto node = state_node->create_anonymous_node();
        anonymous_state_nodes.push_back( node );

        try
        {
          auto rc = add_pending_transaction_to_node( node, record, encoded_record, max_payer_rc );

          // We're only returning the RC used as it pertains to head
          if( state_node->id() == tmp_head->id() )
            rc_used = rc;

          trx_added = true;
        }
        catch( const pending_transaction_nonce_conflict& )
        {
          continue;
        }
      }

      KOINOS_ASSERT( trx_added, pending_transaction_nonce_conflict, "transaction nonce conflict" );
    }

    std::vector< relevant_mutex_map::const_iterator > targets;
    targets.reserve( anonymous_state_nodes.size() );
    for( const auto& anonymous_state_node: anonymous_state_nodes )
    {
      const auto target = _relevant_node_mutexes.find( anonymous_state_node->id() );
      KOINOS_ASSERT( target != _relevant_node_mutexes.end(), state_db::internal_error,
                     "missing relevant node mutex during admission" );
      targets.push_back( target );
    }
    auto node_locks = lock_relevant_mutexes( std::move( targets ) );
    // The DB unique lock and all node locks exclude readers across publication.
    // Prepare every allocation/reset before transferring any fork's writes.
    state_db::anonymous_state_node::commit_all_writes( anonymous_state_nodes );
  }
  catch( const node::LocalCapacityExceeded& ) { throw; }
  catch( const std::bad_alloc& ) { throw; }
  catch( const std::exception& e )
  {
    // Diagnostic formatting cannot replace the original admission failure.
    // In particular, never allocate optional diagnostics during allocation or
    // local-capacity failure unwinding.
    try { LOG( debug ) << "Failed to apply pending transaction " << util::to_hex( pending_trx.transaction().id() )
                       << " with: " << e.what(); }
    catch( ... ) {}
    throw;
  }
  catch( ... )
  {
    try { LOG( debug ) << "Failed to apply pending transaction " << util::to_hex( pending_trx.transaction().id() ); }
    catch( ... ) {}
    throw;
  }

  // Logging is an observer, not a second admission result. An allocation failure
  // here must not report rejection for already-published work on every fork.
  try { LOG( debug ) << "Transaction added to mempool: " << util::to_hex( pending_trx.transaction().id() ); }
  catch( ... ) {}

  return rc_used;
}

uint64_t mempool_impl::remove_pending_transactions( const std::vector< transaction_id_type >& ids )
{
  auto lock = _db.get_unique_lock();
  auto fork_workspace = _memory.enabled() ? _memory.reserve( node::OwnedMemoryBudget::checked_add(
    node::OwnedMemoryBudget::checked_multiply(
      node::OwnedMemoryBudget::checked_add( _relevant_node_mutexes.size(), 1 ), 4096 ), 65536 ) )
    : node::OwnedMemoryBudget::Lease{};
  uint64_t count = 0;

  auto nodes = _db.get_fork_heads( lock );
  auto head  = _db.get_head( lock );

  std::vector< state_db::state_node_ptr > relevant_nodes;
  std::vector< relevant_mutex_map::const_iterator > targets;
  std::vector< state_db::anonymous_state_node_ptr > overlays;
  relevant_nodes.reserve( nodes.size() );
  targets.reserve( nodes.size() );
  overlays.reserve( nodes.size() );

  for( const auto& block_node: nodes )
  {
    const auto relevant_id = tmp_id( block_node->id() );
    auto node = _db.get_node( relevant_id, lock );
    const auto target = _relevant_node_mutexes.find( relevant_id );
    KOINOS_ASSERT( node && target != _relevant_node_mutexes.end(), state_db::internal_error,
                   "missing relevant node during removal" );
    relevant_nodes.push_back( std::move( node ) );
    targets.push_back( target );
  }
  auto node_locks = lock_relevant_mutexes( std::move( targets ) );

  for( size_t i = 0; i < nodes.size(); ++i )
  {
    const auto& block_node = nodes[ i ];
    const auto& node = relevant_nodes[ i ];
    auto overlay = node->create_anonymous_node();
    auto num_removed = remove_pending_transactions_on_node( overlay, ids );
    if( num_removed )
      overlays.push_back( std::move( overlay ) );

    // We're only returning the number of transactions removed as it pertains to head
    if( block_node->id() == head->id() )
      count = num_removed;
  }

  state_db::anonymous_state_node::commit_all_changes( overlays );

  return count;
}

uint64_t mempool_impl::remove_pending_transactions_on_node( state_db::abstract_state_node_ptr node,
                                                            const std::vector< transaction_id_type >& ids )
{
  uint64_t count = 0;

  for( const auto& id: ids )
  {
    auto seq_obj = node->get_object( space::transaction_index(), id );
    if( !seq_obj )
      continue;

    auto ptx_obj = node->get_object( space::pending_transaction(), *seq_obj );
    KOINOS_ASSERT( ptx_obj, state_db::internal_error, "pending transaction record is missing during cleanup" );

    auto decode_workspace = cleanup_workspace( ptx_obj->size() );
    google::protobuf::Arena record_storage( owned_arena_options() );
    const auto& record = parse_cleanup_record< pending_transaction_record >( *ptx_obj, record_storage );

    cleanup_account_resources_on_node( node, record );
    node->remove_object( space::pending_transaction(), *seq_obj );
    node->remove_object( space::transaction_index(), id );
    node->remove_object( space::account_nonce(),
                         create_account_nonce_key( record.pending_transaction().transaction() ) );

    count++;
  }

  return count;
}

uint64_t mempool_impl::prune( std::chrono::seconds expiration, std::chrono::system_clock::time_point now )
{
  auto lock = _db.get_unique_lock();
  auto fork_workspace = _memory.enabled() ? _memory.reserve( node::OwnedMemoryBudget::checked_add(
    node::OwnedMemoryBudget::checked_multiply(
      node::OwnedMemoryBudget::checked_add( _relevant_node_mutexes.size(), 1 ), 4096 ), 65536 ) )
    : node::OwnedMemoryBudget::Lease{};
  uint64_t count = 0;

  auto nodes = _db.get_fork_heads( lock );
  auto head  = _db.get_head( lock );

  std::vector< state_db::state_node_ptr > relevant_nodes;
  std::vector< relevant_mutex_map::const_iterator > targets;
  std::vector< state_db::anonymous_state_node_ptr > overlays;
  relevant_nodes.reserve( nodes.size() );
  targets.reserve( nodes.size() );
  overlays.reserve( nodes.size() );

  for( const auto& block_node: nodes )
  {
    const auto relevant_id = tmp_id( block_node->id() );
    auto node = _db.get_node( relevant_id, lock );
    const auto target = _relevant_node_mutexes.find( relevant_id );
    KOINOS_ASSERT( node && target != _relevant_node_mutexes.end(), state_db::internal_error,
                   "missing relevant node during expiry" );
    relevant_nodes.push_back( std::move( node ) );
    targets.push_back( target );
  }
  auto node_locks = lock_relevant_mutexes( std::move( targets ) );

  for( size_t i = 0; i < nodes.size(); ++i )
  {
    const auto& block_node = nodes[ i ];
    const auto& node = relevant_nodes[ i ];
    auto overlay = node->create_anonymous_node();
    bool changed = false;

    for( ;; )
    {
      auto [ ptx_obj, key ] = overlay->get_next_object( space::pending_transaction(), std::string{} );
      if( !ptx_obj )
        break;

      auto decode_workspace = cleanup_workspace( ptx_obj->size() );
      google::protobuf::Arena record_storage( owned_arena_options() );
      const auto& record = parse_cleanup_record< pending_transaction_record >( *ptx_obj, record_storage );

      std::chrono::system_clock::time_point time{ std::chrono::milliseconds{ record.timestamp() } };
      if( time + expiration > now )
        break;

      cleanup_account_resources_on_node( overlay, record );
      overlay->remove_object( space::pending_transaction(), key );
      overlay->remove_object( space::transaction_index(), record.pending_transaction().transaction().id() );
      overlay->remove_object( space::account_nonce(),
                           create_account_nonce_key( record.pending_transaction().transaction() ) );
      changed = true;

      // Only consider pruned transactions on the fork considered head
      if( block_node->id() == head->id() )
        count++;
    }
    if( changed )
      overlays.push_back( std::move( overlay ) );
  }

  state_db::anonymous_state_node::commit_all_changes( overlays );

  return count;
}

void mempool_impl::cleanup_account_resources_on_node( state_db::abstract_state_node_ptr node,
                                                      const pending_transaction_record& record )
{
  const auto& transaction = record.pending_transaction().transaction();
  auto obj                = node->get_object( space::address_resources(), transaction.header().payer() );
  KOINOS_ASSERT( obj, state_db::internal_error, "pending account resource record is missing during cleanup" );

  auto decode_workspace = cleanup_workspace( obj->size() );
  google::protobuf::Arena resource_storage( owned_arena_options() );
  auto& arr = parse_cleanup_record< address_resource_record >( *obj, resource_storage );
  if( arr.current_rc() + transaction.header().rc_limit() >= arr.max_rc() )
  {
    node->remove_object( space::address_resources(), transaction.header().payer() );
  }
  else
  {
    arr.set_current_rc( arr.current_rc() + transaction.header().rc_limit() );
    auto arr_obj = util::converter::as< std::string >( arr );
    node->put_object( space::address_resources(), transaction.header().payer(), &arr_obj );
  }
}

} // namespace detail

mempool::mempool( state_db::fork_resolution_algorithm algo, pending_record_limits record_limits,
                  uint64_t aggregate_max_accounted_bytes ):
    _my( [&] {
      node::OwnedMemoryBudget memory( aggregate_max_accounted_bytes );
      node::OwnedMemoryBudget::Scope scope( memory, false );
      auto implementation_memory = memory.reserve( sizeof( detail::mempool_impl ) + 4096 );
      return std::make_unique< detail::mempool_impl >( algo, record_limits, std::move( memory ),
                                                     std::move( implementation_memory ) );
    }() )
{}

mempool::~mempool() = default;

uint64_t mempool::pending_transaction_count() const
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget(), true );
  return _my->pending_transaction_count();
}

retained_memory_usage mempool::get_retained_memory_usage() const
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget(), true );
  return _my->get_retained_memory_usage();
}

pending_record_snapshot mempool::get_pending_record_usage() const
{
  return _my->get_pending_record_usage();
}

node::OwnedMemorySnapshot mempool::get_aggregate_memory_usage() const
{
  return _my->memory_budget().snapshot();
}

node::OwnedMemoryBudget mempool::memory_budget() const noexcept
{
  return _my->memory_budget();
}

bool mempool::has_pending_transaction( const transaction_id_type& id,
                                       std::optional< crypto::multihash > block_id ) const
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget() );
  return _my->has_pending_transaction( id, block_id );
}

pending_transaction_vector mempool::get_pending_transactions( uint64_t limit,
                                                                      std::optional< crypto::multihash > block_id )
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget() );
  return _my->get_pending_transactions( limit, block_id );
}

pending_transaction_vector mempool::get_pending_transactions( const std::vector< transaction_id_type >& ids,
                                                                      std::optional< crypto::multihash > block_id )
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget() );
  return _my->get_pending_transactions( ids, block_id );
}

uint64_t mempool::get_reserved_account_rc( const account_type& account ) const
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget() );
  return _my->get_reserved_account_rc( account );
}

bool mempool::check_pending_account_resources( const account_type& payer,
                                               uint64_t max_payer_resources,
                                               uint64_t trx_resource_limit,
                                               std::optional< crypto::multihash > block_id ) const
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget() );
  return _my->check_pending_account_resources( payer, max_payer_resources, trx_resource_limit, block_id );
}

bool mempool::check_account_nonce( const account_type& payee,
                                   const std::string& nonce,
                                   std::optional< crypto::multihash > block_id ) const
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget() );
  return _my->check_account_nonce( payee, nonce, block_id );
}

std::string mempool::get_pending_nonce( const std::string& account, std::optional< crypto::multihash > block_id ) const
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget() );
  return _my->get_pending_nonce( account, block_id );
}

uint64_t mempool::get_pending_transaction_count( const std::string& account,
                                                 std::optional< crypto::multihash > block_id ) const
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget() );
  return _my->get_pending_transaction_count( account, block_id );
}

uint64_t mempool::add_pending_transaction( const pending_transaction& pending_trx,
                                           std::chrono::system_clock::time_point time,
                                           uint64_t max_payer_rc )
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget(), false );
  return _my->add_pending_transaction( pending_trx, time, max_payer_rc );
}

uint64_t mempool::remove_pending_transactions( const std::vector< transaction_id_type >& ids )
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget(), true );
  return _my->remove_pending_transactions( ids );
}

uint64_t mempool::prune( std::chrono::seconds expiration, std::chrono::system_clock::time_point now )
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget(), true );
  return _my->prune( expiration, now );
}

bool mempool::handle_block( const koinos::broadcast::block_accepted& bam )
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget(), true );
  return _my->handle_block( bam );
}

void mempool::handle_irreversibility( const koinos::broadcast::block_irreversible& bi )
{
  node::OwnedMemoryBudget::Scope memory_scope( _my->memory_budget(), true );
  _my->handle_irreversibility( bi );
}

} // namespace koinos::mempool
