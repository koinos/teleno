#include "durable_publication.hpp"
#include <stdexcept>
#include <vector>
#include <cerrno>
#include <cstdlib>
#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif
namespace koinos::node::backup {
#if !defined(_WIN32)
namespace {
void check(bool ok) { if(!ok) throw std::runtime_error("durable snapshot publication I/O failed; preserve repository"); }
struct File {
  int fd;
  explicit File(int fd):fd(fd) { check(fd>=0); }
  ~File() { if(fd>=0) ::close(fd); }
  void close() { int value=fd; fd=-1; check(::close(value)==0); }
};
}
void sync_publication_directory(const std::filesystem::path& p)
{ File f(::open(p.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC)); check(::fsync(f.fd)==0); f.close(); }
void sync_publication_file(const std::filesystem::path& p)
{ File f(::open(p.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW)); check(::fsync(f.fd)==0); f.close(); }
void create_publication_directories(const std::filesystem::path& p)
{
  if(p.empty()||std::filesystem::is_directory(p)) return;
  create_publication_directories(p.parent_path());
  if(std::filesystem::create_directory(p)) sync_publication_directory(p.parent_path().empty()?".":p.parent_path());
}
void durable_atomic_write(const std::filesystem::path& p,const std::string& content,const PublicationCheckpoint& checkpoint)
{
  const auto parent=p.parent_path().empty()?std::filesystem::path("."):p.parent_path();
  create_publication_directories(parent);
  auto pattern=(parent/(p.filename().string()+".tmp.XXXXXX")).string();
  std::vector<char> name(pattern.begin(),pattern.end()); name.push_back('\0');
  File f(::mkstemp(name.data())); const std::filesystem::path temporary(name.data());
  auto phase=[&](const char* s) { if(checkpoint) checkpoint(s,p); };
  try {
    phase("before_write"); size_t written=0;
    while(written<content.size()) {
      auto n=::write(f.fd,content.data()+written,content.size()-written);
      if(n<0&&errno==EINTR) continue;
      check(n>0); written+=size_t(n);
    }
    phase("before_file_sync"); check(::fsync(f.fd)==0); f.close();
    phase("before_rename"); std::filesystem::rename(temporary,p);
    phase("after_rename"); sync_publication_directory(parent); phase("after_directory_sync");
  } catch(...) { std::error_code ec; std::filesystem::remove(temporary,ec); throw; }
}
#else
void sync_publication_directory(const std::filesystem::path&) { throw std::runtime_error("durable snapshot publication is not supported on this platform"); }
void sync_publication_file(const std::filesystem::path&) { throw std::runtime_error("durable snapshot publication is not supported on this platform"); }
void create_publication_directories(const std::filesystem::path&) { throw std::runtime_error("durable snapshot publication is not supported on this platform"); }
void durable_atomic_write(const std::filesystem::path&,const std::string&,const PublicationCheckpoint&) { throw std::runtime_error("durable snapshot publication is not supported on this platform"); }
#endif
}
