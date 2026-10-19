#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace koinos::state_db::backends::detail {

using key_type   = std::string;
using value_type = std::string;
using value_ptr  = std::shared_ptr<const value_type>;
using size_type  = uint64_t;

} // namespace koinos::state_db::backends::detail
