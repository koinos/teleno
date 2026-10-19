#pragma once

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

// Explicit test-only storage selection; never changes a runtime basedir or
// restore policy. Fixtures still create a fresh child and retain source data.
inline std::filesystem::path test_artifact_root( const std::filesystem::path& repository )
{
  namespace fs = std::filesystem;
  const char* selected = std::getenv( "TELENO_TEST_ARTIFACT_ROOT" );
  const auto root = selected ? fs::path( selected ) : fs::canonical( repository ) / "build-single-copy-evidence";
  for( const auto& part: root )
    if( part == ".." ) throw std::runtime_error( "test artifact root cannot contain parent traversal" );
  if( !root.is_absolute() || fs::weakly_canonical( root ) == root.root_path() || fs::is_symlink( root ) )
    throw std::runtime_error( "test artifact root must be an absolute non-root directory, not a symlink" );
  if( selected && !fs::is_directory( root ) )
    throw std::runtime_error( "explicit test artifact root must already exist" );
  fs::create_directories( root );
  return fs::canonical( root );
}

inline bool test_artifact_child( const std::filesystem::path& path, const std::filesystem::path& root )
{
  const auto relative = std::filesystem::weakly_canonical( path ).lexically_relative( root );
  return !relative.empty() && relative != "." && *relative.begin() != "..";
}
