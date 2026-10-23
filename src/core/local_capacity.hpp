#pragma once
#include <exception>

namespace koinos::node {

enum class LocalCapacityReason { transaction_work, pending_record, mempool_memory };

// Local and retryable, never a consensus validity or execution exception.
// Messages/reason codes are static: refusal needs no C++ message allocation.
class LocalCapacityExceeded : public std::exception
{
public:
  explicit LocalCapacityExceeded( LocalCapacityReason reason = LocalCapacityReason::transaction_work ) noexcept
    : _reason( reason ) {}
  const char* what() const noexcept override
  {
    if( _reason == LocalCapacityReason::mempool_memory )
      return "Local mempool memory admission capacity exhausted; retry later";
    return _reason == LocalCapacityReason::pending_record
      ? "Local pending record capacity exhausted; retry later"
      : "Local transaction work capacity exhausted; retry later";
  }
  const char* reason_code() const noexcept
  {
    if( _reason == LocalCapacityReason::mempool_memory ) return "mempool_memory_capacity";
    return _reason == LocalCapacityReason::pending_record ? "pending_record_capacity" : "transaction_work_capacity";
  }
private:
  LocalCapacityReason _reason;
};

} // namespace koinos::node
