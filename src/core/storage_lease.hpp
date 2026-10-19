#pragma once

#include <filesystem>

namespace koinos::node {

// A lifetime-held, cross-process gate outside the basedir. Never unlink it:
// replacing the inode would permit two simultaneous lock owners.
class StorageLease
{
public:
  explicit StorageLease( const std::filesystem::path& basedir );
  ~StorageLease();
  StorageLease( const StorageLease& ) = delete;
  StorageLease& operator=( const StorageLease& ) = delete;
  const std::filesystem::path& intent_path() const { return _intent; }
private:
  int _fd = -1;
  std::filesystem::path _intent;
};

} // namespace koinos::node
