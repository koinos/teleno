#include <koinos/vm_manager/fizzy/module_memory.hpp>
// This private layout is tied to Teleno's hash-pinned fork, not a system library.
#include <fizzy/module.hpp>

namespace koinos::vm_manager::fizzy {
namespace {
template< typename T >
void vector_capacity( capacity_sum& sum, const std::vector< T >& values ) noexcept
{
  sum.add( values.capacity(), sizeof( T ) );
}
void function_capacity( capacity_sum& sum, const ::fizzy::FuncType& type ) noexcept
{
  vector_capacity( sum, type.inputs );
  vector_capacity( sum, type.outputs );
}
void string_capacity( capacity_sum& sum, const std::string& value ) noexcept
{
  sum.add( value.capacity() );
  sum.add( 1 );
}
} // namespace
std::optional< uint64_t > parsed_module_capacity_bytes( const FizzyModule* opaque ) noexcept
{
  if( !opaque ) return 0;
  // The pinned C API uses this same unwrap; instances borrow this immutable object.
  const auto& module = *reinterpret_cast< const ::fizzy::Module* >( opaque );
  capacity_sum sum;
  sum.add( sizeof( module ) );
  vector_capacity( sum, module.typesec );
  vector_capacity( sum, module.importsec );
  vector_capacity( sum, module.funcsec );
  vector_capacity( sum, module.tablesec );
  vector_capacity( sum, module.memorysec );
  vector_capacity( sum, module.globalsec );
  vector_capacity( sum, module.exportsec );
  vector_capacity( sum, module.elementsec );
  vector_capacity( sum, module.codesec );
  vector_capacity( sum, module.datasec );
  vector_capacity( sum, module.imported_function_types );
  vector_capacity( sum, module.imported_table_types );
  vector_capacity( sum, module.imported_memory_types );
  vector_capacity( sum, module.imported_global_types );
  for( const auto& type: module.typesec ) function_capacity( sum, type );
  for( const auto& type: module.imported_function_types ) function_capacity( sum, type );
  for( const auto& import: module.importsec )
  {
    string_capacity( sum, import.module );
    string_capacity( sum, import.name );
  }
  for( const auto& entry: module.exportsec ) string_capacity( sum, entry.name );
  for( const auto& element: module.elementsec ) vector_capacity( sum, element.init );
  for( const auto& code: module.codesec ) vector_capacity( sum, code.instructions );
  for( const auto& data: module.datasec )
  {
    // Pinned fizzy::bytes is basic_string<uint8_t>, including a terminator.
    sum.add( data.init.capacity() );
    sum.add( 1 );
  }
  return sum.value;
}
} // namespace koinos::vm_manager::fizzy
