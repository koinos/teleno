#pragma once

#include <mutex>
#include <type_traits>
#include <sanitizer/tsan_interface.h>

#if !defined(__SANITIZE_THREAD__)
#if !defined(__has_feature)
#error "The scope lifetime overlay requires ThreadSanitizer"
#elif !__has_feature(thread_sanitizer)
#error "The scope lifetime overlay requires ThreadSanitizer"
#endif
#endif

namespace teleno::diagnostics {
// libstdc++'s trivial mutex destruction supplies no pthread lifetime event.
// Boost.DI creates temporary scope mutexes at reused stack addresses. Describe
// their real C++ lifetimes; ordinary std::mutex locks remain intercepted.
// No synthetic happens-before edges or reporting suppressions are introduced.
class ScopeMutex final : public std::mutex
{
public:
  ScopeMutex() noexcept
  {
    if constexpr( std::is_trivially_destructible_v<std::mutex> )
      __tsan_mutex_create(native_handle(), __tsan_mutex_not_static);
  }
  ~ScopeMutex() noexcept
  {
    if constexpr( std::is_trivially_destructible_v<std::mutex> )
      __tsan_mutex_destroy(native_handle(), __tsan_mutex_not_static);
  }
};
static_assert(sizeof(ScopeMutex) == sizeof(std::mutex));
static_assert(alignof(ScopeMutex) == alignof(std::mutex));
} // namespace teleno::diagnostics
