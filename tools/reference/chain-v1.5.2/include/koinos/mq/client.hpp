#pragma once

#include <chrono>
#include <future>
#include <string>

namespace koinos::mq {

enum class retry_policy
{
  none
};

// The exact v1.5.2 controller stores this type only for block-store and
// broadcast side effects. The isolated reference runner deliberately never
// installs a client, so an interface-only definition is sufficient.
class client
{
public:
  virtual ~client() = default;

  virtual std::shared_future< std::string >
  rpc( const std::string&,
       const std::string&,
       std::chrono::milliseconds = std::chrono::milliseconds( 5000 ),
       retry_policy = retry_policy::none ) = 0;

  virtual void broadcast( const std::string&, const std::string& ) = 0;
};

} // namespace koinos::mq
