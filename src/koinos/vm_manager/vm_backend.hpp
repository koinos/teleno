#pragma once

#include <koinos/vm_manager/host_api.hpp>
#include <koinos/vm_manager/vm_memory.hpp>

#include <memory>
#include <string>
#include <vector>

namespace koinos::vm_manager {

/**
 * Abstract class for WebAssembly virtual machines.
 *
 * To add a new WebAssembly VM, you need to implement this class
 * and return it in get_vm_backends().
 */
class vm_backend
{
   public:
      vm_backend();
      virtual ~vm_backend();

      virtual std::string backend_name() = 0;

      /**
       * Initialize the backend.  Should only be called once.
       */
      virtual void initialize() = 0;

      /**
       * Run some bytecode.
       */
      virtual void run( abstract_host_api& hapi, const std::string& bytecode, const std::string& id = std::string() ) = 0;
      virtual module_cache_memory cache_memory() const { return {}; }
};

/**
 * Get a list of available VM backends.
 */
std::vector< std::shared_ptr< vm_backend > > get_vm_backends( uint64_t module_cache_max_capacity_bytes = 0 );

std::string get_default_vm_backend_name();

/**
 * Get a shared_ptr to the named VM backend.
 */
std::shared_ptr< vm_backend > get_vm_backend( const std::string& name = get_default_vm_backend_name(),
                                           uint64_t module_cache_max_capacity_bytes = 0 );

} // koinos::vm_manager
