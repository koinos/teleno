#include "teleno_tsan_scope_mutex.hpp"
#include <iostream>
#include <memory>
#include <string>

template<class Mutex> struct Pair { Mutex a,b; };
template<class Mutex> void ordered(Pair<Mutex>& pair, bool reverse)
{
  if(reverse) { std::lock_guard<std::mutex> b(pair.b);std::lock_guard<std::mutex> a(pair.a); }
  else { std::lock_guard<std::mutex> a(pair.a);std::lock_guard<std::mutex> b(pair.b); }
}
template<class Mutex> void reused_storage()
{
  alignas(Pair<Mutex>) unsigned char storage[sizeof(Pair<Mutex>)];
  auto pair=new(storage)Pair<Mutex>;ordered(*pair,false);std::destroy_at(pair);
  pair=new(storage)Pair<Mutex>;ordered(*pair,true);std::destroy_at(pair);
}
int main(int argc,char** argv)
{
  const std::string mode=argc>1?argv[1]:"annotated-reuse";
  if(mode=="plain-reuse")reused_storage<std::mutex>();
  else if(mode=="annotated-reuse")reused_storage<teleno::diagnostics::ScopeMutex>();
  else if(mode=="live-cycle") { Pair<teleno::diagnostics::ScopeMutex> pair;ordered(pair,false);ordered(pair,true); }
  else if(mode=="destroy-held") { teleno::diagnostics::ScopeMutex mutex;mutex.lock(); }
  else return 1;
  std::cout<<"scope-lifetime-completed\n";
}
