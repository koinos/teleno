#include "backup/durable_publication.hpp"
#include "../artifact_root.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

using namespace koinos::node::backup;
namespace fs=std::filesystem;
void check(bool ok) { if(!ok) throw std::runtime_error("durable publication assertion failed"); }
std::string read(const fs::path& p) { std::ifstream f(p); check(bool(f)); return {std::istreambuf_iterator<char>(f),{}}; }
int main()
{
  try {
    const auto root=test_artifact_root(TELENO_LR1_REPOSITORY_ROOT)/
      ("lr1-publication-"+std::to_string(std::random_device{}()));
    fs::create_directories(root.parent_path()); check(fs::create_directory(root));
    std::cout << "retained publication fixture: " << root << std::endl;
    const auto path=root/"latest.json";
    const std::string old="{\"backup_id\":\"old-complete\"}\n", next="{\"backup_id\":\"new-complete\"}\n";
    for(const std::string phase:{"before_write","before_file_sync","before_rename","after_rename","after_directory_sync"}) {
      durable_atomic_write(path,old);
      const auto child=::fork(); check(child>=0);
      if(child==0) {
        try { durable_atomic_write(path,next,[&](const std::string& current,const fs::path&){if(current==phase) ::_exit(99);}); }
        catch(...) { ::_exit(4); }
        ::_exit(5);
      }
      int status=0; check(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==99);
      check(read(path)==(phase=="after_rename" || phase=="after_directory_sync" ? next:old));
    }
    durable_atomic_write(path,old);
    const auto child=::fork(); check(child>=0);
    if(child==0) {
      ::signal(SIGXFSZ,SIG_IGN); struct rlimit limit{8,8}; if(::setrlimit(RLIMIT_FSIZE,&limit)!=0) ::_exit(6);
      try { durable_atomic_write(path,next); } catch(const std::exception&) { ::_exit(99); }
      ::_exit(7);
    }
    int status=0; check(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==99);
    check(read(path)==old);
    for(const std::string phase:{"before_file_sync","before_rename","after_rename"}) {
      durable_atomic_write(path,old); bool failed=false;
      try { durable_atomic_write(path,next,[&](const std::string& current,const fs::path&){if(current==phase) throw std::runtime_error("injected failure");}); }
      catch(const std::exception&) { failed=true; }
      check(failed && read(path)==(phase=="after_rename" ? next:old));
    }
    std::cout << "publication: 5 process interruptions, 1 kernel write failure, 3 reported faults PASS\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
