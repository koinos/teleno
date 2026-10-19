#pragma once
#include "backup/restore_plan.hpp"
#include <functional>

namespace koinos::node::backup {
// Read-only storage/identity consistency check, not historical replay parity.
// Opens every existing CF without creating missing DBs/CFs or starting services.
std::string validate_offline_restore_database(
  const RestoreSpecification&, const std::filesystem::path& payload,
  const std::function<void()>& cancellation_check = {} );
}
