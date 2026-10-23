#pragma once

#include "local_capacity.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace koinos::node {

struct OwnedMemorySnapshot
{
  bool enabled = false;
  uint64_t limit_bytes = 0;
  uint64_t accounted_bytes = 0, allocator_requested_bytes = 0;
  uint64_t bootstrap_accounted_bytes = 0;
  uint64_t peak_accounted_bytes = 0, peak_allocator_requested_bytes = 0;
  uint64_t refusals = 0, protected_allocations = 0;
  uint64_t refusal_limit_bytes = 0, refusal_accounted_bytes = 0;
  uint64_t peak_refusal_accounted_bytes = 0, refusal_capacity_refusals = 0;
  uint64_t refusal_transport_cancellations = 0;
};

// An allocation/reservation policy, not an allocator-heap or RSS measurement.
// Allocator requests include our ownership header; supplementary reservations
// cover declared string/metadata workspace. The ordinary zero setting is
// untracked. Canonical/recovery work is counted but is never rejected merely
// because the optional standalone-admission policy is full.
class OwnedMemoryBudget
{
public:
  enum class WorkClass { ordinary, canonical, refusal };

private:
  struct State
  {
    State( uint64_t limit, uint64_t bootstrap, uint64_t refusal_limit )
    {
      usage.enabled = true; usage.limit_bytes = limit;
      usage.refusal_limit_bytes = refusal_limit;
      usage.bootstrap_accounted_bytes = bootstrap;
      usage.accounted_bytes = usage.peak_accounted_bytes = bootstrap;
    }
    std::mutex mutex;
    OwnedMemorySnapshot usage;
  };
  // A trivially initialized TLS pointer avoids accessing a destroyed TLS
  // shared_ptr from process-wide delete during thread/process teardown.
  inline static thread_local const std::shared_ptr< State >* current_state = nullptr;
  inline static thread_local WorkClass current_work = WorkClass::ordinary;
  inline static thread_local uint64_t container_key_extra = 0;
  inline static thread_local unsigned allocation_bypass = 0;

public:
  class Lease
  {
    friend class OwnedMemoryBudget;
    std::shared_ptr< State > owner;
    uint64_t bytes = 0, requested = 0;
    // Physical requests cannot reach PTRDIFF_MAX on the supported 64-bit
    // hosts. Tag this otherwise unused bit without enlarging every tracked
    // allocation's lease/side-table node for the exceptional control path.
    static constexpr uint64_t refusal_tag = uint64_t( 1 ) << 63;
    Lease( std::shared_ptr< State > state, uint64_t bytes, uint64_t requested, bool refusal ) noexcept
      : owner( std::move( state ) ), bytes( bytes ), requested( requested | ( refusal ? refusal_tag : 0 ) ) {}
  public:
    Lease() = default;
    Lease( const Lease& ) = delete;
    Lease& operator=( const Lease& ) = delete;
    Lease( Lease&& other ) noexcept
      : owner( std::move( other.owner ) ), bytes( other.bytes ), requested( other.requested ) {}
    Lease& operator=( Lease&& other ) noexcept
    {
      if( this != &other )
      { reset(); owner = std::move( other.owner ); bytes = other.bytes; requested = other.requested; }
      return *this;
    }
    ~Lease() { reset(); }
    void reset() noexcept
    {
      if( !owner ) return;
      // Keep State alive until its mutex has been released.
      auto state = std::move( owner );
      std::lock_guard lock( state->mutex );
      state->usage.accounted_bytes -= bytes;
      state->usage.allocator_requested_bytes -= requested & ~refusal_tag;
      if( requested & refusal_tag ) state->usage.refusal_accounted_bytes -= bytes;
    }
  };

  class Scope
  {
    const std::shared_ptr< State >* previous;
    std::shared_ptr< State > active;
    WorkClass previous_work;
  public:
    explicit Scope( const OwnedMemoryBudget& budget ) noexcept
      : Scope( budget, current_state && *current_state == budget.state ? current_work : WorkClass::ordinary ) {}
    Scope( const OwnedMemoryBudget& budget, bool recovery ) noexcept
      : Scope( budget, recovery ? WorkClass::canonical : WorkClass::ordinary ) {}
    Scope( const OwnedMemoryBudget& budget, WorkClass work ) noexcept
      : previous( current_state ), active( budget.state ), previous_work( current_work )
    { current_state = &active; current_work = work; }
    Scope( const Scope& ) = delete;
    Scope& operator=( const Scope& ) = delete;
    ~Scope() { current_state = previous; current_work = previous_work; }
  };

  // Owned allocators already reserve their physical request. Bypass automatic
  // capture only for that raw allocation, never for constructors/field growth.
  class AllocationBypass
  {
  public:
    AllocationBypass() noexcept { ++allocation_bypass; }
    ~AllocationBypass() { --allocation_bypass; }
    AllocationBypass( const AllocationBypass& ) = delete;
  };

  // The VM's C API owns a separate V1 allocation domain and cannot propagate
  // quota exceptions. Its native callbacks re-enter M1 capture explicitly.
  class AllocationCapture
  {
    unsigned previous;
  public:
    AllocationCapture() noexcept : previous( allocation_bypass ) { allocation_bypass = 0; }
    ~AllocationCapture() { allocation_bypass = previous; }
    AllocationCapture( const AllocationCapture& ) = delete;
  };

  // Generated Protobuf reflection tables are immutable process-global state.
  // GetReflection alone does not initialize lazy field types used by sizing.
  // Warm the reachable compiled descriptors/default prototypes once outside
  // request admission, including the temporary traversal vector. Both are
  // excluded process metadata/workspace and remain whole-node RSS consumers.
  // Keep sizing the actual message in its normal capture scope: map/extension
  // implementations may materialize message-specific data during inspection.
  // This helper is for generated messages with a fixed static descriptor.
  template< typename Message > static uint64_t protobuf_space_used( const Message& message )
  {
    {
      AllocationBypass process_metadata;
      (void)message.GetReflection();
      static const bool prepared = [] {
        const auto* root = Message::descriptor();
        std::vector< decltype( root ) > descriptors{ root };
        auto* factory = Message::default_instance().GetReflection()->GetMessageFactory();
        for( size_t i = 0; i < descriptors.size(); ++i )
        {
          const auto* descriptor = descriptors[ i ];
          for( int f = 0; f < descriptor->field_count(); ++f )
          {
            const auto* field = descriptor->field( f );
            if( field->cpp_type() == field->CPPTYPE_MESSAGE )
            {
              const auto* nested = field->message_type();
              if( std::find( descriptors.begin(), descriptors.end(), nested ) == descriptors.end() )
                descriptors.push_back( nested );
            }
          }
          const auto* prototype = factory->GetPrototype( descriptor );
          if( !prototype ) throw std::logic_error( "Generated Protobuf prototype is unavailable" );
          (void)prototype->SpaceUsedLong();
        }
        return true;
      }();
      (void)prepared;
    }
    return message.SpaceUsedLong();
  }

  class KeyScope
  {
    uint64_t previous;
  public:
    explicit KeyScope( size_t length ) : previous( container_key_extra )
    { container_key_extra = string_reservation( length ); }
    KeyScope( const KeyScope& ) = delete;
    ~KeyScope() { container_key_extra = previous; }
  };

  OwnedMemoryBudget() = default;
  // Reserve the counter/mutex/control block before constructing its first
  // allocation. This fixed allowance includes control-block and allocator
  // headroom; it is supplementary policy, not an observed allocator request.
  // It remains charged for the entire shared State lifetime, including when
  // the original facade is gone and only allocation headers retain it.
  static constexpr uint64_t bootstrap_reservation() noexcept { return sizeof( State ) + 128; }
  // A separate finite envelope keeps overload responses usable at a full
  // ordinary quota. Unlike canonical/recovery work, refusal allocations may
  // exhaust this envelope; their credits follow the same final owners.
  static constexpr uint64_t default_refusal_limit_bytes = 1024 * 1024;
  explicit OwnedMemoryBudget( uint64_t limit_bytes, bool capture_parent = false,
                              uint64_t refusal_limit_bytes = default_refusal_limit_bytes )
  {
    if( !limit_bytes ) return;
    const auto bootstrap = bootstrap_reservation();
    if( limit_bytes < bootstrap ) throw LocalCapacityExceeded( LocalCapacityReason::mempool_memory );
    // A subordinate admission gate can own its control block in the active
    // parent domain. Its policy credits are not additional physical bytes.
    if( capture_parent ) state = std::make_shared< State >( limit_bytes, bootstrap, refusal_limit_bytes );
    else
    {
      AllocationBypass bootstrap_owner;
      state = std::make_shared< State >( limit_bytes, bootstrap, refusal_limit_bytes );
    }
  }
  bool enabled() const noexcept { return bool( state ); }
  static OwnedMemoryBudget current() noexcept
  { return current_state ? OwnedMemoryBudget( *current_state ) : OwnedMemoryBudget{}; }
  static bool capture_allocation() noexcept
  { return !allocation_bypass && current_state && bool( *current_state ); }
  bool operator==( const OwnedMemoryBudget& other ) const noexcept { return state == other.state; }
  bool operator!=( const OwnedMemoryBudget& other ) const noexcept { return !( *this == other ); }

  static uint64_t checked_add( uint64_t a, uint64_t b )
  {
    if( b > std::numeric_limits< uint64_t >::max() - a ) throw std::bad_alloc();
    return a + b;
  }
  static uint64_t checked_multiply( uint64_t a, uint64_t b )
  {
    if( a && b > std::numeric_limits< uint64_t >::max() / a ) throw std::bad_alloc();
    return a * b;
  }
  // Conservative supplementary reservation for the selected native standard
  // libraries. It is deliberately separate from allocator_requested_bytes;
  // actual string capacity/allocator overhead still needs platform evidence.
  static uint64_t string_reservation( uint64_t length )
  { return checked_add( checked_multiply( length, 2 ), 64 ); }

  // Native protobuf copies and pinned stream-encoding temporaries can overlap
  // several buffers. This conservative policy envelope excludes VM and escaped
  // RPC/receipt ownership; it is not serialized-size-to-heap equality.
  static uint64_t native_message_workspace( uint64_t space_used )
  { return checked_add( checked_multiply( space_used, 16 ), 65536 ); }

  // Protobuf's JSON writer is not exception-safe when a C++ allocation is
  // refused partway through an encoded field. Admit a conservative envelope
  // before entering it, then run only that third-party conversion with quota
  // callbacks bypassed. The 32x factor covers worst-case JSON escaping,
  // geometric string growth, the parsed rewrite tree and the rewritten copy;
  // the fixed allowance covers writer and reflection scratch. This remains a
  // policy reservation rather than a claim about allocator heap or RSS.
  static uint64_t json_message_workspace( uint64_t space_used )
  { return checked_add( checked_multiply( space_used, 32 ), 1024 * 1024 ); }

  Lease reserve( uint64_t bytes, uint64_t requested = 0 ) const
  {
    if( !state ) return {};
    if( requested > bytes ) throw std::invalid_argument( "allocator request exceeds aggregate reservation" );
    if( requested & Lease::refusal_tag ) throw std::bad_alloc();
    std::lock_guard lock( state->mutex );
    auto& usage = state->usage;
    const auto max = std::numeric_limits< uint64_t >::max();
    const bool same_domain = current_state && *current_state == state;
    const bool protected_work = same_domain && current_work != WorkClass::ordinary;
    const bool refusal_work = same_domain && current_work == WorkClass::refusal;
    const bool overflow = bytes > max - usage.accounted_bytes || requested > max - usage.allocator_requested_bytes;
    const bool refusal_full = refusal_work &&
      ( usage.refusal_accounted_bytes > usage.refusal_limit_bytes ||
        bytes > usage.refusal_limit_bytes - usage.refusal_accounted_bytes );
    if( overflow || ( !protected_work &&
        ( usage.accounted_bytes > usage.limit_bytes || bytes > usage.limit_bytes - usage.accounted_bytes ) ) || refusal_full )
    {
      if( usage.refusals != max ) ++usage.refusals;
      if( refusal_full && usage.refusal_capacity_refusals != max ) ++usage.refusal_capacity_refusals;
      if( overflow ) throw std::bad_alloc();
      throw LocalCapacityExceeded( LocalCapacityReason::mempool_memory );
    }
    usage.accounted_bytes += bytes;
    usage.allocator_requested_bytes += requested;
    usage.peak_accounted_bytes = std::max( usage.peak_accounted_bytes, usage.accounted_bytes );
    usage.peak_allocator_requested_bytes = std::max( usage.peak_allocator_requested_bytes, usage.allocator_requested_bytes );
    if( protected_work && usage.protected_allocations != max ) ++usage.protected_allocations;
    if( refusal_work )
    {
      usage.refusal_accounted_bytes += bytes;
      usage.peak_refusal_accounted_bytes = std::max( usage.peak_refusal_accounted_bytes, usage.refusal_accounted_bytes );
    }
    return Lease( state, bytes, requested, refusal_work );
  }
  OwnedMemorySnapshot snapshot() const
  {
    if( !state ) return {};
    std::lock_guard lock( state->mutex ); return state->usage;
  }
  void record_refusal_transport_cancellation() const
  {
    if( !state ) return;
    std::lock_guard lock( state->mutex );
    auto& count = state->usage.refusal_transport_cancellations;
    if( count != std::numeric_limits< uint64_t >::max() ) ++count;
  }

private:
  explicit OwnedMemoryBudget( std::shared_ptr< State > state ) noexcept : state( std::move( state ) ) {}
  std::shared_ptr< State > state;

public:
  template< typename T > class Allocator;
};

template< typename T > class OwnedMemoryBudget::Allocator
{
    template< typename > friend class Allocator;
    struct Header
    {
      explicit Header( Lease lease ) noexcept : lease( std::move( lease ) ) {}
      Lease lease;
    };
    static constexpr size_t alignment = alignof( T ) > alignof( Header ) ? alignof( T ) : alignof( Header );
    static constexpr size_t offset = ( sizeof( Header ) + alignment - 1 ) / alignment * alignment;
    OwnedMemoryBudget budget;
    uint64_t extra = 0;
    bool charge_key = true;
  public:
    using value_type = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::false_type;
    Allocator() noexcept : budget( OwnedMemoryBudget::current() ) {}
    explicit Allocator( OwnedMemoryBudget budget, uint64_t extra = 0, bool charge_key = false ) noexcept
      : budget( std::move( budget ) ), extra( extra ), charge_key( charge_key ) {}
    template< typename U > Allocator( const Allocator< U >& other ) noexcept
      : budget( other.budget ), extra( other.extra ), charge_key( other.charge_key ) {}
    template< typename U, typename... Args >
    void construct( U* pointer, Args&&... args )
    {
      // Native vector/map copies can be requested outside their original
      // scope. Attribute their newly constructed fields to the allocator's
      // domain too; inherited canonical/recovery exemption remains intact.
      Scope scope( budget );
      ::new( static_cast< void* >( const_cast< std::remove_cv_t< U >* >( pointer ) ) )
        U( std::forward< Args >( args )... );
    }
    T* allocate( size_t n )
    {
      if( !budget.enabled() ) return std::allocator< T >{}.allocate( n );
      const auto object_bytes = checked_add( checked_multiply( n, sizeof( T ) ), offset );
      // Automatic capture is bypassed for the raw allocation itself. The
      // malloc-compatible primitive receives object_bytes, without a prefix.
      // Retain the previous conservative wrapper/alignment allowance as
      // supplementary policy so total admission boundaries remain unchanged;
      // it is not part of the actual operator-new request.
      const auto raw_alignment = std::max< size_t >( alignment, __STDCPP_DEFAULT_NEW_ALIGNMENT__ );
      const auto wrapper_allowance = checked_add( 2 * sizeof( void* ), raw_alignment - 1 );
      const auto previous_request_envelope = checked_add( object_bytes, wrapper_allowance );
      const auto requested = object_bytes;
      if( object_bytes > std::numeric_limits< size_t >::max() ) throw std::bad_alloc();
      // Supplementary allocator metadata/rounding headroom, distinct from the
      // actual operator-new request. Large mappings may round to OS pages.
      uint64_t supplementary = checked_add( wrapper_allowance,
        checked_add( extra, previous_request_envelope >= 65536 ? 16384 : 64 ) );
      if( charge_key && n == 1 && !std::is_pointer_v< T > ) supplementary = checked_add( supplementary, container_key_extra );
      auto lease = budget.reserve( checked_add( requested, supplementary ), requested );
      void* raw;
      {
        AllocationBypass bypass;
        if constexpr( alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__ )
          raw = ::operator new( object_bytes, std::align_val_t( alignment ) );
        else raw = ::operator new( object_bytes );
      }
      new( raw ) Header( std::move( lease ) );
      return reinterpret_cast< T* >( static_cast< unsigned char* >( raw ) + offset );
    }
    void deallocate( T* pointer, size_t n ) noexcept
    {
      if( !budget.enabled() ) { std::allocator< T >{}.deallocate( pointer, n ); return; }
      auto* raw = reinterpret_cast< unsigned char* >( pointer ) - offset;
      auto* header = reinterpret_cast< Header* >( raw );
      auto lease = std::move( header->lease );
      header->~Header();
      if constexpr( alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__ )
        ::operator delete( raw, std::align_val_t( alignment ) );
      else ::operator delete( raw );
      // lease returns its credit after the allocation has actually been freed.
    }
    template< typename U > bool operator==( const Allocator< U >& other ) const noexcept { return budget == other.budget; }
    template< typename U > bool operator!=( const Allocator< U >& other ) const noexcept { return !( *this == other ); }
    OwnedMemoryBudget memory_budget() const noexcept { return budget; }
};

template< typename T, typename... Args >
std::shared_ptr< T > make_owned_shared_extra( uint64_t extra, Args&&... args )
{
  const auto budget = OwnedMemoryBudget::current();
  if( !budget.enabled() ) return std::make_shared< T >( std::forward< Args >( args )... );
  return std::allocate_shared< T >( OwnedMemoryBudget::Allocator< T >( budget, extra ),
                                   std::forward< Args >( args )... );
}

template< typename T, typename... Args >
std::shared_ptr< T > make_owned_shared( Args&&... args )
{ return make_owned_shared_extra< T >( 0, std::forward< Args >( args )... ); }

} // namespace koinos::node
