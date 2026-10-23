#ifndef TELENO_M1_BASELINE
#include "core/owned_allocation.hpp"
#include "mempool/mempool_adapter.hpp"
#include "core/monolith_rpc_client.hpp"
#else
#include <cstdlib>
#include <cstddef>
namespace koinos::node::owned_allocation {
inline void* allocate( std::size_t n ) { return std::malloc( n ? n : 1 ); }
inline void* allocate( std::size_t n, std::size_t a ) { void* p = nullptr; if( posix_memalign( &p, a, n ? n : 1 ) ) return nullptr; return p; }
inline void deallocate( void* p ) { std::free( p ); }
}
#endif
#include "memory_fixture.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <new>
#include <thread>
#include <sys/resource.h>
#ifdef __linux__
#include <unistd.h>
#endif
#include <google/protobuf/unknown_field_set.h>
#ifdef __GLIBC__
#include <malloc.h>
#endif

static thread_local long fail_at = -1;
static thread_local bool fired = false, measure_new = false;
static thread_local bool fail_continuously = false;
static thread_local uint64_t allocations = 0, allocation_bytes = 0;
static long diagnostic_operation = -1, diagnostic_ordinal = -1;

void* operator new( size_t n )
{
  if( measure_new ) { ++allocations; allocation_bytes += n; }
  if( ( fail_continuously && fired ) || ( fail_at >= 0 && fail_at-- == 0 ) )
  { fail_at = -1; fired = true; throw std::bad_alloc(); }
  if( auto p = koinos::node::owned_allocation::allocate( n ? n : 1 ) ) return p;
  throw std::bad_alloc();
}
void* operator new[]( size_t n ) { return ::operator new( n ); }
void operator delete( void* p ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
void operator delete[]( void* p ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
void operator delete( void* p, size_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
void operator delete[]( void* p, size_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
void* operator new( size_t n, std::align_val_t alignment )
{
  if( measure_new ) { ++allocations; allocation_bytes += n; }
  if( ( fail_continuously && fired ) || ( fail_at >= 0 && fail_at-- == 0 ) )
  { fail_at = -1; fired = true; throw std::bad_alloc(); }
  return koinos::node::owned_allocation::allocate( n, size_t( alignment ) );
}
void operator delete( void* p, std::align_val_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }
void operator delete( void* p, size_t, std::align_val_t ) noexcept { koinos::node::owned_allocation::deallocate( p ); }

namespace {
using namespace mempool_memory_fixture;

auto pool( uint64_t bytes = 1ULL << 30 )
{
#ifdef TELENO_M1_BASELINE
  (void)bytes;
  return std::make_unique< mempool::mempool >( state_db::fork_resolution_algorithm::fifo,
                                              mempool::pending_record_limits{ 1024, 0 } );
#else
  return std::make_unique< mempool::mempool >( state_db::fork_resolution_algorithm::fifo,
                                              mempool::pending_record_limits{ 1024, 0 }, bytes );
#endif
}

#ifndef TELENO_M1_BASELINE
void retained_metadata_and_boundary()
{
  auto probe = pool(); initialize( *probe );
  const auto before = probe->get_aggregate_memory_usage().accounted_bytes;
  const auto tx = transaction( 1, 65536 );
  probe->add_pending_transaction( tx, at(), 1000 );
  const auto peak = probe->get_aggregate_memory_usage().peak_accounted_bytes;
  const auto retained = probe->get_retained_memory_usage().indexed_state;
  require( probe->get_aggregate_memory_usage().accounted_bytes > before + 65536 && retained.map_entries > 10,
           "aggregate record/index ownership absent" );
  auto exact = pool( peak ); initialize( *exact ); exact->add_pending_transaction( tx, at(), 1000 );
  require( exact->pending_transaction_count() == 1, "exact aggregate boundary rejected" );
  auto short_one = pool( peak ); initialize( *short_one );
  const auto old = snapshot( *short_one );
  const auto before_refusal = short_one->get_aggregate_memory_usage();
  auto occupied = short_one->memory_budget().reserve(
    before_refusal.limit_bytes - before_refusal.accounted_bytes );
  bool refused = false;
  try { short_one->add_pending_transaction( tx, at(), 1000 ); }
  catch( const node::LocalCapacityExceeded& e ) { refused = std::string( e.reason_code() ) == "mempool_memory_capacity"; }
  occupied.reset();
  require( refused && snapshot( *short_one ) == old && !short_one->get_pending_record_usage().owners,
           "aggregate refusal changed accepted state or classification" );
  short_one->add_pending_transaction( tx, at(), 1000 );
  require( short_one->pending_transaction_count() == 1,
           "aggregate admission failed after capacity was released" );

  const auto retained_before_expiry = exact->get_aggregate_memory_usage().accounted_bytes;
  exact->prune( std::chrono::seconds{ 1 }, at() + std::chrono::seconds{ 5 } );
  require( exact->get_pending_record_usage().owners == 1 && !exact->pending_transaction_count(),
           "expiry returned ancestor record credits early" );
  lib( *exact, 2 );
  const auto remaining = exact->get_retained_memory_usage().indexed_state;
  require( !exact->get_pending_record_usage().owners && remaining.tombstone_entries &&
           exact->get_aggregate_memory_usage().accounted_bytes > remaining.tombstone_key_capacity_bytes,
           "zero record count hid retained map/tombstone metadata" );
  require( exact->get_aggregate_memory_usage().accounted_bytes < retained_before_expiry,
           "LIB did not release retired owners" );
  exact->add_pending_transaction( transaction( 2, 1024 ), at(), 1000 );
  require( exact->pending_transaction_count() == 1, "admission did not recover after cleanup" );
}

void concurrency()
{
  auto p = pool( 512 * 1024 ); initialize( *p );
  std::atomic< unsigned > accepted_count = 0, refused_count = 0;
  std::atomic< bool > failed = false;
  std::mutex error_mutex;
  std::string first_error;
  const auto record_error = [&]( const char* message ) {
    std::lock_guard guard( error_mutex );
    if( first_error.empty() ) first_error = message;
    failed = true;
  };
  std::vector< std::thread > workers;
  for( unsigned n = 0; n < 8; ++n ) workers.emplace_back( [&,n] {
    for( unsigned i = 0; i < 12; ++i )
      try { p->add_pending_transaction( transaction( n * 12 + i + 1 ), at(), 1000 ); ++accepted_count; }
      catch( const node::LocalCapacityExceeded& e )
      { if( std::strcmp( e.reason_code(), "mempool_memory_capacity" ) != 0 ) record_error( e.reason_code() ); ++refused_count; }
      catch( const std::exception& e ) { record_error( e.what() ); }
      catch( ... ) { record_error( "non-standard exception" ); }
  } );
  for( auto& worker: workers ) worker.join();
  const auto usage = p->get_aggregate_memory_usage();
  const auto pending = p->pending_transaction_count();
  // Eight simultaneous staging reservations can temporarily fill the small
  // test quota before any thread publishes. All-local refusal is permitted;
  // after those reservations unwind, a later ordinary admission must work.
  const bool valid = !failed && accepted_count + refused_count == 96 && refused_count > 0 &&
                     pending == accepted_count && usage.accounted_bytes <= usage.limit_bytes;
  if( !valid )
    std::cerr << "M1_CONCURRENCY accepted=" << accepted_count << " refused=" << refused_count
              << " pending=" << pending << " accounted=" << usage.accounted_bytes
              << " limit=" << usage.limit_bytes << " first_error=" << first_error << '\n';
  require( valid,
           "concurrent aggregate admission lost work, exceeded limit or misclassified refusal" );
  require( p->get_pending_transactions( 2000, id( 3 ) ).size() == accepted_count,
           "concurrent admission only published some forks" );
  if( !accepted_count )
  {
    p->add_pending_transaction( transaction( 1000 ), at(), 1000 );
    require( p->pending_transaction_count() == 1 &&
             p->get_pending_transactions( 2000, id( 3 ) ).size() == 1,
             "transient aggregate overload did not permit a correct later retry" );
  }
}

void canonical_work_at_full_admission_capacity()
{
  auto p = pool( 512 * 1024 ); initialize( *p );
  p->add_pending_transaction( transaction( 1 ), at(), 1000 );
  p->add_pending_transaction( transaction( 2 ), at(), 1000 );
  const auto usage = p->get_aggregate_memory_usage();
  auto occupied = p->memory_budget().reserve( usage.limit_bytes - usage.accounted_bytes );
  require( p->handle_block( accepted( 4, 2, 3, transaction( 1 ).transaction().id() ) ),
           "full standalone quota rejected canonical block work" );
  lib( *p, 4 );
  require( p->pending_transaction_count() == 1 &&
           p->has_pending_transaction( transaction( 2 ).transaction().id() ) &&
           !p->has_pending_transaction( transaction( 1 ).transaction().id() ),
           "canonical cleanup at full quota lost an unincluded accepted transaction" );
  require( p->get_aggregate_memory_usage().peak_accounted_bytes > usage.limit_bytes &&
           p->get_aggregate_memory_usage().protected_allocations > 0,
           "protected canonical/recovery consumption was hidden" );
  occupied.reset();
  p->add_pending_transaction( transaction( 3 ), at(), 1000 );
  require( p->pending_transaction_count() == 2, "admission did not recover after protected work" );
}

void mixed_fork_cleanup_concurrency()
{
  auto p = pool( 64 * 1024 * 1024 ); initialize( *p );
  p->add_pending_transaction( transaction( 999 ), at() - std::chrono::hours{ 1 }, 1000 );
  std::atomic< bool > failed = false;
  std::vector< std::thread > workers;
  for( unsigned worker = 0; worker < 4; ++worker ) workers.emplace_back( [&,worker] {
    try
    {
      for( unsigned n = 1; n <= 24; ++n )
        p->add_pending_transaction( transaction( worker * 24 + n ), at(), 1000 );
    }
    catch( ... ) { failed = true; }
  } );
  workers.emplace_back( [&] {
    try
    {
      for( unsigned n = 4; n <= 20; ++n )
      {
        block( *p, n, n == 4 ? 2 : n - 1, n - 1 );
        if( n % 4 == 0 ) lib( *p, n == 4 ? 2 : n - 2 );
      }
    }
    catch( ... ) { failed = true; }
  } );
  workers.emplace_back( [&] {
    try
    {
      for( unsigned n = 0; n < 40; ++n )
      {
        p->prune( std::chrono::seconds{ 30 }, at() );
        p->remove_pending_transactions( { "absent-transaction" } );
        (void)p->get_retained_memory_usage();
      }
    }
    catch( ... ) { failed = true; }
  } );
  for( auto& worker: workers ) worker.join();
  p->prune( std::chrono::seconds{ 30 }, at() );
  require( !failed && p->pending_transaction_count() == 96, "mixed fork/LIB/cleanup concurrency lost accepted work" );
  for( unsigned n = 1; n <= 96; ++n )
  {
    const auto t = transaction( n );
    require( p->has_pending_transaction( t.transaction().id() ) &&
             p->get_reserved_account_rc( t.transaction().header().payer() ) == 100,
             "mixed concurrency changed accepted IDs or RC reservations" );
  }
  require( !p->has_pending_transaction( transaction( 999 ).transaction().id() ), "expiry did not remove the old record" );
}

void opposite_fork_enumeration_order()
{
  for( const bool reverse: { false, true } )
  {
    auto p = pool( 64 * 1024 * 1024 );
    block( *p, 1, 0, 1 );
    if( reverse ) { block( *p, 3, 1, 2 ); block( *p, 2, 1, 2 ); }
    else { block( *p, 2, 1, 2 ); block( *p, 3, 1, 2 ); }
    p->add_pending_transaction( transaction( 1 ), at(), 1000 );
    require( p->remove_pending_transactions( { "absent-transaction" } ) == 0 &&
             p->prune( std::chrono::hours{ 1 }, at() ) == 0 &&
             p->pending_transaction_count() == 1,
             "opposite fork insertion order changed admission, removal or expiry" );
  }
}

void native_message_shapes()
{
  for( unsigned shape = 0; shape < 4; ++shape )
  {
    auto p = pool( 64 * 1024 * 1024 ); initialize( *p );
    auto tx = transaction( 1, shape == 0 ? 0 : 65536 );
    if( shape == 1 ) tx.mutable_transaction()->mutable_header()->set_payer( std::string( 65536, 'p' ) );
    if( shape == 2 ) tx.mutable_transaction()->set_id( std::string( 65536, 'i' ) );
    if( shape == 3 )
    {
      auto* unknown = tx.GetReflection()->MutableUnknownFields( &tx );
      for( unsigned n = 0; n < 4096; ++n ) unknown->AddLengthDelimited( 300, "" );
    }
    const auto expected = tx.SerializeAsString();
    const auto tx_id = tx.transaction().id();
    p->add_pending_transaction( tx, at(), 1000 ); tx.Clear();
    const auto returned = p->get_pending_transactions( 1 );
    require( returned.size() == 1 && returned.front().SerializeAsString() == expected,
             "native memory reservation changed a large/repeated/unknown input" );
    const auto selected = p->get_pending_transactions(
      std::vector< mempool::transaction_id_type >{ tx_id } );
    require( selected.size() == 1 && selected.front().SerializeAsString() == expected,
             "selected native query changed a large/repeated/unknown input" );
    p->prune( std::chrono::seconds{ 1 }, at() + std::chrono::seconds{ 5 } ); lib( *p, 2 );
    require( !p->get_pending_record_usage().owners, "native input shape leaked a retained record" );
  }
}

void aggregate_only_and_external_record_owner()
{
  node::OwnedMemoryBudget memory( 1 << 20 );
  std::shared_ptr< const std::string > caller;
  {
    mempool::pending_record_budget factory( {}, memory );
    caller = factory.encode( 8192, [] { return std::string( 8192, 'r' ); } );
    require( !factory.snapshot().enabled && memory.snapshot().accounted_bytes >= 8192,
             "aggregate-only factory depends on the optional legacy record policy" );
  }
  require( memory.snapshot().accounted_bytes > 0 && *caller == std::string( 8192, 'r' ),
           "external record owner lost bytes or credits when its factory expired" );
  auto alias = caller; caller.reset();
  require( memory.snapshot().accounted_bytes > 0, "record alias returned aggregate credits early" );
  alias.reset();
  const auto bootstrap = memory.snapshot().bootstrap_accounted_bytes;
  require( memory.snapshot().accounted_bytes == bootstrap, "final external record owner leaked aggregate credit" );
  {
    mempool::pending_record_budget factory( { 1, 0 }, memory );
    require( memory.snapshot().accounted_bytes > bootstrap, "record-policy state/control block was not owned" );
    caller = factory.encode( 8192, [] { return std::string( 8192, 'r' ); } );
  }
  require( memory.snapshot().accounted_bytes > bootstrap, "record-policy owner released its state credit too early" );
  caller.reset();
  require( memory.snapshot().accounted_bytes == bootstrap, "record-policy last owner leaked its state credit" );

  mempool::mempool aggregate_only( state_db::fork_resolution_algorithm::fifo, {}, 8 * 1024 * 1024 );
  initialize( aggregate_only ); aggregate_only.add_pending_transaction( transaction( 1 ), at(), 1000 );
  require( !aggregate_only.get_pending_record_usage().enabled && aggregate_only.pending_transaction_count() == 1 &&
           aggregate_only.get_aggregate_memory_usage().accounted_bytes > 8192,
           "native aggregate policy requires independent record limits" );
  aggregate_only.prune( std::chrono::seconds{ 1 }, at() + std::chrono::seconds{ 5 } ); lib( aggregate_only, 2 );
  require( !aggregate_only.pending_transaction_count(), "aggregate-only cleanup failed" );
}

void decoded_query_owner_lifetime()
{
  auto p = pool( 64 * 1024 * 1024 ); initialize( *p );
  p->add_pending_transaction( transaction( 1 ), at(), 1000 );
  p->add_pending_transaction( transaction( 2 ), at(), 1000 );
  auto memory = p->memory_budget();
  const auto before = memory.snapshot().accounted_bytes;
  {
    auto values = p->get_pending_transactions( 2 );
    const auto first_owner = memory.snapshot().accounted_bytes;
    require( values.size() == 2 && first_owner > before + 16384,
             "native decoded result escaped its lifetime reservation" );
    auto copy = values;
    require( memory.snapshot().accounted_bytes > first_owner,
             "independent decoded vector copy did not reserve a new physical owner" );
    auto selected = p->get_pending_transactions( std::vector< mempool::transaction_id_type >{
      transaction( 2 ).transaction().id() } );
    require( selected.size() == 1 && selected.front().transaction().id() == transaction( 2 ).transaction().id(),
             "selected native result changed its record or ordering" );
    p->prune( std::chrono::seconds{ 1 }, at() + std::chrono::seconds{ 5 } ); lib( *p, 2 );
    p.reset();
    require( memory.snapshot().accounted_bytes > memory.snapshot().bootstrap_accounted_bytes &&
             copy.front().SerializeAsString() == values.front().SerializeAsString(),
             "decoded caller lost its memory domain or bytes when the mempool was destroyed" );
  }
  require( memory.snapshot().accounted_bytes == memory.snapshot().bootstrap_accounted_bytes,
           "last decoded result owner leaked aggregate credit" );

  auto limited = pool( 512 * 1024 ); initialize( *limited );
  limited->add_pending_transaction( transaction( 1 ), at(), 1000 );
  {
    auto values = limited->get_pending_transactions( 1 );
    const auto held = limited->get_aggregate_memory_usage();
    require( held.accounted_bytes > held.bootstrap_accounted_bytes, "held native result was not accounted" );
    auto occupied = limited->memory_budget().reserve( held.limit_bytes - held.accounted_bytes );
    bool refused = false;
    try { limited->add_pending_transaction( transaction( 2 ), at(), 1000 ); }
    catch( const node::LocalCapacityExceeded& e ) { refused = std::string( e.reason_code() ) == "mempool_memory_capacity"; }
    require( refused && limited->pending_transaction_count() == 1, "held decoded owner did not regulate standalone admission" );
    require( limited->handle_block( accepted( 4, 2, 3 ) ), "held decoded owner blocked canonical processing" );
    lib( *limited, 4 );
    require( values.front().transaction().id() == transaction( 1 ).transaction().id(),
             "canonical cleanup invalidated an independent decoded result" );
  }
  limited->add_pending_transaction( transaction( 2 ), at(), 1000 );
  require( limited->pending_transaction_count() == 2, "decoded last-owner release did not allow a correct retry" );
}

void decoded_query_allocation_failures()
{
  auto p = pool( 64 * 1024 * 1024 ); initialize( *p );
  p->add_pending_transaction( transaction( 1 ), at(), 1000 );
  p->add_pending_transaction( transaction( 2 ), at(), 1000 );
  const auto before = p->get_aggregate_memory_usage().accounted_bytes;
  for( const bool selected: { false, true } )
  {
    const auto query = [&]() {
      if( selected ) return p->get_pending_transactions(
        std::vector< mempool::transaction_id_type >{
          transaction( 1 ).transaction().id(), transaction( 2 ).transaction().id() } );
      return p->get_pending_transactions( 2 );
    };
    allocations = allocation_bytes = 0; measure_new = true;
    { auto measured = query(); }
    measure_new = false;
    require( allocations < 12000, "decoded query exceeds the finite fault-sweep allocation budget" );
    std::cout << "M1_QUERY_ALLOCATIONS selected=" << selected << " observed=" << allocations << '\n';
    unsigned failures = 0;
    for( long ordinal = 0; ordinal < 12000; ++ordinal )
    {
      fail_at = ordinal; fired = false;
      bool rejected = false;
      try { auto result = query(); require( result.size() == 2, "query lost accepted records" ); }
      catch( const std::exception& ) { rejected = true; }
      fail_at = -1;
      require( p->get_aggregate_memory_usage().accounted_bytes == before,
               "failed decoded query leaked or prematurely returned aggregate credits" );
      if( !fired )
      {
        require( !rejected && failures > 0, "decoded query fault coverage was not exercised" );
        std::cout << "M1_QUERY_FAULT_SWEEP selected=" << selected << " failures=" << failures
                  << " first_uninjected_ordinal=" << ordinal << '\n';
        break;
      }
      if( rejected )
      {
        ++failures;
        auto retry = query();
        require( retry.size() == 2 && p->pending_transaction_count() == 2,
                 "decoded query allocation failure damaged accepted transactions or retry" );
      }
      if( ordinal == 11999 )
        throw std::runtime_error( "decoded query fault sweep exhausted its finite ordinal limit" );
    }
  }
}

void read_workspace_and_refusal()
{
  auto p = pool( 64 * 1024 * 1024 ); initialize( *p );
  p->add_pending_transaction( transaction( 1 ), at(), 1000 );
  const auto account = std::string( 1024 * 1024, 'a' );
  const auto before = p->get_aggregate_memory_usage();
  require( p->get_pending_nonce( account ).empty() && !p->get_pending_transaction_count( account ) &&
           !p->get_reserved_account_rc( account ), "oversized native account query changed its result" );
  const auto after = p->get_aggregate_memory_usage();
  require( after.peak_allocator_requested_bytes > before.allocator_requested_bytes + account.size() &&
           after.accounted_bytes == before.accounted_bytes,
           "ordinary nonce/RC/count workspace was unaccounted or retained after return" );
  auto occupied = p->memory_budget().reserve( after.limit_bytes - after.accounted_bytes );
  bool refused = false;
  try { (void)p->get_pending_nonce( account ); }
  catch( const node::LocalCapacityExceeded& e )
  { refused = std::strcmp( e.reason_code(), "mempool_memory_capacity" ) == 0; }
  require( refused && p->pending_transaction_count() == 1,
           "read overload was misclassified or damaged accepted transactions" );
  occupied.reset();
  require( p->get_pending_nonce( account ).empty() && p->pending_transaction_count() == 1,
           "read refusal did not permit a correct later retry" );
}

void nonce_key_parity()
{
  auto p = pool( 64 * 1024 * 1024 ); initialize( *p );
  auto tx = transaction( 1, 128 );
  const auto payer = tx.transaction().header().payer();
  const auto nonce = tx.transaction().header().nonce();
  p->add_pending_transaction( tx, at(), 1000 );
  require( !p->check_account_nonce( payer, nonce ) && p->get_pending_nonce( payer ) == nonce &&
           p->get_pending_transaction_count( payer ) == 1,
           "payer nonce key changed during direct string construction" );

  auto paid = transaction( 2, 128 );
  const auto payee = std::string( 40, 'q' ) + std::string( 1, '\0' ) + std::string( 32, 'r' );
  paid.mutable_transaction()->mutable_header()->set_payee( payee );
  const auto paid_nonce = paid.transaction().header().nonce();
  const auto paid_payer = paid.transaction().header().payer();
  p->add_pending_transaction( paid, at(), 1000 );
  require( !p->check_account_nonce( payee, paid_nonce ) && p->get_pending_nonce( payee ) == paid_nonce &&
           p->get_pending_transaction_count( payee ) == 1 &&
           p->check_account_nonce( paid_payer, paid_nonce ) && p->get_pending_nonce( paid_payer ).empty(),
           "payee nonce key or binary account prefix changed during direct string construction" );
}

void shorter_predecessor_read()
{
  auto p = pool( 64 * 1024 * 1024 ); initialize( *p );
  const auto accepted = transaction( 1, 128 );
  p->add_pending_transaction( accepted, at(), 1000 );
  const auto payer = accepted.transaction().header().payer();
  const auto nonce_bytes = util::converter::as< std::string >( uint64_t( 1 ) );
  // A predecessor key is payer || nonce. The requested account extends it
  // with a NUL, which matches std::string's terminator, then more bytes.
  // Before the size check this drives both three-iterator mismatch calls
  // beyond the shorter predecessor under ASan.
  const auto longer_account = payer + nonce_bytes + std::string( 1, '\0' )
                              + std::string( 1024, 'z' );
  const auto before = snapshot( *p );
  const auto accounted = p->get_aggregate_memory_usage().accounted_bytes;
  require( p->get_pending_nonce( longer_account ).empty() &&
           p->get_pending_transaction_count( longer_account ) == 0,
           "shorter predecessor was accepted as a pending account nonce" );
  require( p->get_pending_transaction_count( payer ) == 1 &&
           !p->get_pending_nonce( payer ).empty() &&
           snapshot( *p ) == before &&
           p->get_aggregate_memory_usage().accounted_bytes == accounted,
           "shorter-predecessor query changed accepted work or leaked M1 credit" );
}

void rpc_and_serialized_owner_lifetime()
{
  auto p = pool( 16 * 1024 * 1024 ); initialize( *p );
  p->add_pending_transaction( transaction( 1 ), at(), 1000 );
  auto memory = p->memory_budget();
  const auto before = memory.snapshot().accounted_bytes;
  std::vector< rpc::mempool::get_pending_transactions_response > responses;
  {
    node::MempoolAdapter adapter( *p );
    rpc::mempool::get_pending_transactions_request req; req.set_limit( 1 );
    for( unsigned n = 0; n < 128; ++n ) responses.push_back( adapter.get_pending_transactions( req ) );
    require( memory.snapshot().accounted_bytes > before + 1024 * 1024,
             "moved RPC protobuf fields returned their credits with the native vector" );
    const auto held = memory.snapshot().accounted_bytes;
    auto moved = std::move( responses.back() ); responses.pop_back();
    require( memory.snapshot().accounted_bytes == held && moved.pending_transactions_size() == 1,
             "protobuf move copied or prematurely released physical ownership" );
    responses.push_back( std::move( moved ) );
  }
  responses.clear();
  require( memory.snapshot().accounted_bytes == before, "RPC last-owner release did not return its field credits" );
  std::shared_future< std::string > retained;
  {
    node::MempoolAdapter adapter( *p );
    node::MonolithRpcClient client( nullptr, &adapter, nullptr, {}, {}, memory );
    rpc::mempool::mempool_request req; req.mutable_get_pending_transactions()->set_limit( 1 );
    retained = client.rpc( util::service::mempool, req.SerializeAsString() );
    require( retained.get().size() > 8192 && memory.snapshot().accounted_bytes > before + 8192,
             "serialized native RPC future escaped its allocation owner" );
  }
  require( memory.snapshot().accounted_bytes > before + 8192,
           "RPC client destruction returned a retained future's credits" );
  retained = {};
  require( memory.snapshot().accounted_bytes == before, "serialized future's last owner leaked credits" );
}

template< typename Message >
void copied_message_allocation_failures( const Message& source )
{
  node::OwnedMemoryBudget memory( 64 * 1024 * 1024 );
  const auto before = memory.snapshot().accounted_bytes;
  const auto expected = source.SerializeAsString();
  unsigned failures = 0;
  for( long ordinal = 0; ordinal < 4096; ++ordinal )
  {
    bool refused = false; fired = false; fail_at = ordinal;
    {
      node::OwnedMemoryBudget::Scope scope( memory );
      try { Message copy( source ); fail_at = -1; require( copy.SerializeAsString() == expected, "message copy changed wire bytes" ); }
      catch( const std::bad_alloc& ) { refused = true; }
    }
    fail_at = -1;
    require( memory.snapshot().accounted_bytes == before,
             "throwing generated message copy leaked a physical allocation owner" );
    require( source.SerializeAsString() == expected, "failed copy changed its source message" );
    if( !fired ) { require( !refused && failures > 0, "message copy fault coverage was not exercised" ); return; }
    ++failures;
  }
  throw std::runtime_error( "message copy fault sweep exhausted its finite ordinal limit" );
}

void generated_oneof_failure_and_retry()
{
  node::OwnedMemoryBudget memory( 1 << 20 );
  const auto before = memory.snapshot().accounted_bytes;
  {
    node::OwnedMemoryBudget::Scope scope( memory );
    protocol::operation value;
    value.mutable_upload_contract()->set_bytecode( "existing payload" );
    const auto wire = value.SerializeAsString();
    const auto retained = memory.snapshot().accounted_bytes;
    fired = false; fail_at = 0;
    bool allocation_failure = false;
    try { (void)value.mutable_call_contract(); }
    catch( const std::bad_alloc& ) { allocation_failure = true; }
    fail_at = -1;
    const bool wire_same = value.SerializeAsString() == wire;
    const auto after_wire = memory.snapshot().accounted_bytes;
    require( fired && allocation_failure && value.has_upload_contract() && wire_same && after_wire == retained,
             "failed generated oneof allocation destroyed its existing case" );
    value.mutable_call_contract()->set_args( "retry payload" );
    require( value.has_call_contract() && value.call_contract().args() == "retry payload",
             "generated oneof allocation failure prevented a correct later retry" );
  }
  require( memory.snapshot().accounted_bytes == before, "generated oneof failure/retry leaked ownership" );
}

void generated_message_copy_failures()
{
  auto tx = transaction( 1 ).transaction();
  for( unsigned n = 0; n < 16; ++n )
  {
    tx.add_signatures( std::string( 128, 's' ) );
    tx.add_operations()->mutable_call_contract()->set_args( std::string( 256, 'a' ) );
  }
  tx.GetReflection()->MutableUnknownFields( &tx )->AddGroup( 300 )->AddLengthDelimited( 301, "future field" );
  copied_message_allocation_failures( tx );
  rpc::mempool::get_pending_transactions_response response;
  *response.add_pending_transactions()->mutable_transaction() = tx;
  *response.add_pending_transactions()->mutable_transaction() = tx;
  copied_message_allocation_failures( response );
  protocol::transaction_receipt receipt; receipt.set_id( tx.id() );
  for( unsigned n = 0; n < 16; ++n ) receipt.add_logs( std::string( 128, 'l' ) );
  copied_message_allocation_failures( receipt );
}

void persistent_cpp_allocation_failure()
{
  unsigned failures = 0;
  for( long ordinal = 0; ordinal < 1000; ++ordinal )
  {
    mempool::mempool p( state_db::fork_resolution_algorithm::fifo, { 1, 0 }, 64 * 1024 * 1024 );
    initialize( p ); p.add_pending_transaction( transaction( 1 ), at(), 1000 );
    const auto input = transaction( 2 );
    const auto before = snapshot( p );
    const auto accounted = p.get_aggregate_memory_usage().accounted_bytes;
    const auto prior_refusals = p.get_pending_record_usage().refusals;
    bool allocation_failure = false, local_refusal = false, unexpected = false;
    fired = false; fail_at = ordinal; fail_continuously = true;
    try { p.add_pending_transaction( input, at(), 1000 ); unexpected = true; }
    catch( const std::bad_alloc& ) { allocation_failure = true; }
    catch( const node::LocalCapacityExceeded& e )
    { local_refusal = std::strcmp( e.reason_code(), "pending_record_capacity" ) == 0; }
    catch( ... ) { unexpected = true; }
    fail_at = -1; fail_continuously = false;
    require( p.get_pending_record_usage().refusals == prior_refusals || local_refusal,
             "diagnostic allocation replaced a native pending-record capacity refusal" );
    require( !unexpected && ( allocation_failure || local_refusal ) && snapshot( p ) == before &&
             p.get_aggregate_memory_usage().accounted_bytes == accounted && p.get_pending_record_usage().owners == 1,
             "persistent C++ allocation failure changed accepted work or its local classification" );
    if( fired ) ++failures;
    // Restore capacity through the ordinary accepted-record lifecycle and
    // prove that this same pool can admit a correct later retry.
    p.prune( std::chrono::seconds{ 1 }, at() + std::chrono::seconds{ 5 } ); lib( p, 2 );
    p.add_pending_transaction( input, at(), 1000 );
    require( p.pending_transaction_count() == 1 && p.has_pending_transaction( input.transaction().id() ),
             "persistent allocation failure prevented later recovery/retry" );
    if( !fired )
    {
      require( failures > 0 && local_refusal, "persistent allocation-fault coverage was not exercised" );
      std::cout << "M1_PERSISTENT_CPP_FAILURES failures=" << failures << " first_uninjected_ordinal=" << ordinal << '\n';
      return;
    }
  }
  throw std::runtime_error( "persistent C++ allocation-fault sweep exhausted its finite ordinal limit" );
}
#endif

enum class Operation { admission, expiry, block, lib };

void failure_sweep( Operation operation )
{
  unsigned failures = 0;
  for( long ordinal = 0; ordinal < 5000; ++ordinal )
  {
    diagnostic_operation = long( operation ); diagnostic_ordinal = ordinal;
    auto p = pool(); initialize( *p );
    p->add_pending_transaction( transaction( 1 ), at(), 1000 );
    p->add_pending_transaction( transaction( 2 ), at(), 1000 );
    if( operation == Operation::lib ) p->prune( std::chrono::seconds{ 1 }, at() + std::chrono::seconds{ 5 } );
    const auto old = snapshot( *p );
    const auto record_owners = p->get_pending_record_usage().owners;
#ifndef TELENO_M1_BASELINE
    const auto accounted = p->get_aggregate_memory_usage().accounted_bytes;
#endif
    const auto tx = transaction( 3 );
    const auto block_input = accepted( 4, 2, 3, transaction( 1 ).transaction().id() );
    bool rejected = false;
    if( std::getenv( "TELENO_M1_FAULT_PROGRESS" ) )
      std::cerr << "M1_FAULT_ATTEMPT operation=" << unsigned( operation ) << " ordinal=" << ordinal << '\n';
    fail_at = ordinal; fired = false;
    try
    {
      switch( operation )
      {
        case Operation::admission: p->add_pending_transaction( tx, at(), 1000 ); break;
        case Operation::expiry: p->prune( std::chrono::seconds{ 1 }, at() + std::chrono::seconds{ 5 } ); break;
        case Operation::block: require( p->handle_block( block_input ), "candidate block not linked" ); break;
        case Operation::lib: lib( *p, 2 ); break;
      }
    }
    catch( const std::exception& ) { rejected = true; }
    fail_at = -1;
    if( rejected )
    {
      ++failures;
      require( fired, "operation failed without the selected allocation fault" );
      require( snapshot( *p ) == old && p->get_pending_record_usage().owners == record_owners,
               "allocation failure changed accepted records, nonce/RC or fork/head visibility" );
#ifndef TELENO_M1_BASELINE
      require( p->get_aggregate_memory_usage().accounted_bytes == accounted,
               "failed preparation leaked or prematurely returned aggregate credits" );
#endif
    }
    // Successful retries exercise the same affected pool, including failures
    // after finalization/root preparation. Never substitute an empty mempool.
    if( rejected )
      switch( operation )
      {
        case Operation::admission: p->add_pending_transaction( tx, at(), 1000 ); break;
        case Operation::expiry: p->prune( std::chrono::seconds{ 1 }, at() + std::chrono::seconds{ 5 } ); break;
        case Operation::block: require( p->handle_block( block_input ), "block retry did not link" ); break;
        case Operation::lib: lib( *p, 2 ); break;
      }
    if( !fired )
    {
      require( failures > 0, "allocation failures were not exercised" );
      std::cout << "M1_FAULT_SWEEP operation=" << unsigned( operation ) << " failures=" << failures
                << " first_uninjected_ordinal=" << ordinal << '\n';
      return;
    }
  }
  throw std::runtime_error( "allocation fault sweep exhausted its finite ordinal limit" );
}

void measure( bool query_owners = false )
{
  auto p = pool( 64 * 1024 * 1024 ); initialize( *p );
  std::vector< mempool::pending_transaction > input;
  for( unsigned n = 1; n <= 64; ++n ) input.push_back( transaction( n ) );
  rusage start_usage{}, end_usage{}; getrusage( RUSAGE_SELF, &start_usage );
  const auto start = std::chrono::steady_clock::now();
  std::vector< decltype( p->get_pending_transactions( 64 ) ) > callers;
  measure_new = true; allocations = allocation_bytes = 0;
  for( const auto& tx: input ) p->add_pending_transaction( tx, at(), 1000 );
  if( query_owners )
    for( unsigned n = 0; n < 2; ++n ) callers.push_back( p->get_pending_transactions( 64 ) );
  const auto admitted = p->get_retained_memory_usage().indexed_state;
  p->prune( std::chrono::seconds{ 1 }, at() + std::chrono::seconds{ 5 } );
  lib( *p, 2 );
  measure_new = false;
  const auto elapsed = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::steady_clock::now() - start ).count();
  getrusage( RUSAGE_SELF, &end_usage );
  const auto remaining = p->get_retained_memory_usage().indexed_state;
  uint64_t accounted = 0, allocator_requested = 0, peak_accounted = 0;
#ifndef TELENO_M1_BASELINE
  const auto usage = p->get_aggregate_memory_usage();
  accounted = usage.accounted_bytes; allocator_requested = usage.allocator_requested_bytes;
  peak_accounted = usage.peak_accounted_bytes;
#endif
  uint64_t heap = 0;
#ifdef __GLIBC__
  const auto info = mallinfo2(); heap = info.uordblks + info.hblkhd;
#endif
  const auto micros = []( timeval t ) { return uint64_t( t.tv_sec ) * 1000000 + t.tv_usec; };
  const uint64_t rss = end_usage.ru_maxrss *
#ifdef __APPLE__
    1;
#else
    1024;
#endif
  std::cout << "M1_MEASURE {\"transactions\":64,\"signature_bytes\":8192,\"elapsed_ns\":" << elapsed
    << ",\"held_query_copies\":" << callers.size()
    << ",\"new_calls\":" << allocations << ",\"new_requested_bytes\":" << allocation_bytes
    << ",\"peak_rss_bytes\":" << rss << ",\"glibc_active_heap_bytes\":" << heap
    << ",\"user_us\":" << micros( end_usage.ru_utime ) - micros( start_usage.ru_utime )
    << ",\"system_us\":" << micros( end_usage.ru_stime ) - micros( start_usage.ru_stime )
    << ",\"input_blocks\":" << end_usage.ru_inblock - start_usage.ru_inblock
    << ",\"output_blocks\":" << end_usage.ru_oublock - start_usage.ru_oublock
    << ",\"admitted_map_entries\":" << admitted.map_entries
    << ",\"admitted_unique_value_capacity_bytes\":" << admitted.unique_value_capacity_bytes
    << ",\"remaining_map_entries\":" << remaining.map_entries
    << ",\"remaining_tombstones\":" << remaining.tombstone_entries
    << ",\"remaining_record_owners\":" << p->get_pending_record_usage().owners
    << ",\"accounted_bytes\":" << accounted << ",\"allocator_requested_bytes\":" << allocator_requested
    << ",\"peak_accounted_bytes\":" << peak_accounted << "}\n";
}

#ifndef TELENO_M1_BASELINE
void measure_table()
{
  auto p = pool( 64 * 1024 * 1024 ); initialize( *p );
  auto memory = p->memory_budget();
  auto sample = [&]( const char* phase ) {
    const auto table = node::owned_allocation::table_snapshot();
    const auto usage = memory.snapshot();
    uint64_t rss = 0, heap = 0;
#ifdef __linux__
    if( auto* statm = std::fopen( "/proc/self/statm", "r" ) )
    {
      unsigned long resident = 0;
      if( std::fscanf( statm, "%*lu %lu", &resident ) == 1 )
        rss = resident * uint64_t( sysconf( _SC_PAGESIZE ) );
      std::fclose( statm );
    }
#endif
#ifdef __GLIBC__
    const auto info = mallinfo2(); heap = info.uordblks + info.hblkhd;
#endif
    std::cout << "M1_TABLE {\"phase\":\"" << phase << "\",\"live_entries\":" << table.live_entries
      << ",\"entry_requested_bytes\":" << table.live_entry_requested_bytes
      << ",\"buckets\":" << table.buckets << ",\"nonempty_buckets\":" << table.nonempty_buckets
      << ",\"max_bucket_depth\":" << table.max_bucket_depth
      << ",\"accounted_bytes\":" << usage.accounted_bytes
      << ",\"allocator_requested_bytes\":" << usage.allocator_requested_bytes
      << ",\"glibc_active_heap_bytes\":" << heap << ",\"current_rss_bytes\":" << rss << "}\n";
  };
  sample( "initialized" );
  for( unsigned n = 1; n <= 64; ++n ) p->add_pending_transaction( transaction( n ), at(), 1000 );
  sample( "admitted" );
  {
    auto caller = p->get_pending_transactions( 64 );
    sample( "caller_held" );
    p->prune( std::chrono::seconds{ 1 }, at() + std::chrono::seconds{ 5 } );
    lib( *p, 2 );
    sample( "expired_and_lib" );
    p.reset();
    sample( "facade_destroyed_with_caller" );
    require( caller.size() == 64, "escaped caller lost its decoded records" );
  }
  sample( "final_owner_released" );
}

void measure_table_after_untracked_warmup()
{
  // Identify one-time library/log metadata independently of a budgeted pool.
  // This is diagnostic only; it does not change the ordinary admission path.
  {
    auto warm = pool( 0 ); initialize( *warm );
    warm->add_pending_transaction( transaction( 1 ), at(), 1000 );
    (void)warm->get_pending_transactions( 1 );
    warm->prune( std::chrono::seconds{ 1 }, at() + std::chrono::seconds{ 5 } );
    lib( *warm, 2 );
  }
  measure_table();
}
#endif
} // namespace

int main( int argc, char** argv )
{
  try
  {
    if( argc == 2 && std::string( argv[ 1 ] ) == "--measure" ) { measure(); return 0; }
    if( argc == 2 && std::string( argv[ 1 ] ) == "--measure-query" ) { measure( true ); return 0; }
#ifndef TELENO_M1_BASELINE
    if( argc == 2 && std::string( argv[ 1 ] ) == "--measure-table" ) { measure_table(); return 0; }
    if( argc == 2 && std::string( argv[ 1 ] ) == "--measure-table-prewarmed" )
    { measure_table_after_untracked_warmup(); return 0; }
#endif
    if( argc == 2 )
    {
      const std::string mode = argv[ 1 ];
      if( mode == "--fault-admission" ) { failure_sweep( Operation::admission ); return 0; }
      if( mode == "--fault-expiry" ) { failure_sweep( Operation::expiry ); return 0; }
      if( mode == "--fault-block" ) { failure_sweep( Operation::block ); return 0; }
      if( mode == "--fault-lib" ) { failure_sweep( Operation::lib ); return 0; }
#ifndef TELENO_M1_BASELINE
      if( mode == "--concurrency" ) { concurrency(); mixed_fork_cleanup_concurrency(); return 0; }
      if( mode == "--short-predecessor" ) { shorter_predecessor_read(); return 0; }
#endif
    }
#ifndef TELENO_M1_BASELINE
    // Match ordinary startup: initialize shared protobuf/log infrastructure
    // without attributing process-global startup allocations to one test pool.
    { mempool::mempool startup; initialize( startup ); startup.add_pending_transaction( transaction( 1 ), at(), 1000 ); }
    retained_metadata_and_boundary(); concurrency(); canonical_work_at_full_admission_capacity();
    mixed_fork_cleanup_concurrency(); opposite_fork_enumeration_order(); native_message_shapes();
    aggregate_only_and_external_record_owner();
    decoded_query_owner_lifetime(); decoded_query_allocation_failures(); persistent_cpp_allocation_failure();
    read_workspace_and_refusal(); nonce_key_parity(); shorter_predecessor_read(); rpc_and_serialized_owner_lifetime();
    generated_message_copy_failures(); generated_oneof_failure_and_retry();
#endif
    for( const auto operation: { Operation::admission, Operation::expiry, Operation::block, Operation::lib } )
      failure_sweep( operation );
  }
  catch( const std::exception& e )
  { fail_at = -1; std::cerr << "operation=" << diagnostic_operation << " ordinal=" << diagnostic_ordinal << ": " << e.what() << '\n'; return 1; }
}
