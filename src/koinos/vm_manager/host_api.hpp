#pragma once

#include <cstdint>
#include <cstring>

namespace koinos::vm_manager {

// The VM resolves bytes_written as four in-bounds linear-memory bytes, not
// necessarily an aligned native uint32_t object. Keep the legacy API signature
// and native byte representation without a typed store. Write at the original
// call site: this address can alias argument or result bytes, so deferring the
// write through a temporary would change observable ordering.
inline void store_bytes_written( uint32_t* destination, uint32_t value ) noexcept
{
  std::memcpy( static_cast< void* >( destination ), &value, sizeof( value ) );
}

/**
 * An abstract class representing an implementation of an API.
 *
 * The user of the vm_manager library is responsible for creating an application-specific subclass.
 * bytes_written can be unaligned; implementations must use store_bytes_written
 * (or an equivalent byte copy), not dereference it as a native uint32_t object.
 */
class abstract_host_api
{
   public:
      abstract_host_api();
      virtual ~abstract_host_api();

      virtual int32_t invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written  ) = 0;
      virtual int32_t invoke_system_call( uint32_t xid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written  ) = 0;
      virtual int64_t get_meter_ticks()const = 0;
      virtual void use_meter_ticks( uint64_t meter_ticks ) = 0;
};

} // koinos::vm_manager
