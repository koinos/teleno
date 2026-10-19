#pragma once
#include <exception>

namespace koinos::node {

enum class LocalCapacityReason { transaction_work, pending_record };

// Local and retryable, never a consensus validity or execution exception.
// Messages/reason codes are static: refusal needs no C++ message allocation.
class LocalCapacityExceeded : public std::exception
{
public:
  explicit LocalCapacityExceeded( LocalCapacityReason reason = LocalCapacityReason::transaction_work ) noexcept
    : _reason( reason ) {}
  const char* what() const noexcept override
  {
    return _reason == LocalCapacityReason::pending_record
      ? "Local pending record capacity exhausted; retry later"
      : "Local transaction work capacity exhausted; retry later";
  }
  const char* reason_code() const noexcept
  {
    return _reason == LocalCapacityReason::pending_record ? "pending_record_capacity" : "transaction_work_capacity";
  }
private:
  LocalCapacityReason _reason;
};

} // namespace koinos::node
