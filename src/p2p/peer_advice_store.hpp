#pragma once

#include "types.hpp"
#include <algorithm>
#include <limits>
#include <list>
#include <map>
#include <stdexcept>
#include <string_view>

namespace koinos::node::p2p::detail {
// Caller serializes all access. Entries own each ID/address once; index views
// borrow stable list nodes. Retained capacity excludes one staged publication,
// allocator/node overhead, SDK storage and caller-owned copies.
class PeerAdviceStore
{
  struct Entry{PeerID peer;bool seed=false,connected=false;};
  using Order=std::list<Entry>;
  Order _order;
  std::map<std::string_view,Order::iterator,std::less<>> _index;
  const size_t _max_entries,_max_bytes;
  size_t _bytes=0,_peak_bytes=0;
  uint64_t _evictions=0,_capacity_refusals=0,_allocation_refusals=0,_sdk_observations=0;
  static size_t bytes(const PeerID& p)
  {if(p.id.capacity()>std::numeric_limits<size_t>::max()-p.address.capacity())throw std::length_error("Peer advice capacity overflow");return p.id.capacity()+p.address.capacity();}
  bool can_fit(size_t needed,size_t old_bytes,Order::const_iterator excluded,bool adding)const noexcept
  {
    if(needed>_max_bytes)return false;
    size_t available=_max_bytes-(_bytes-old_bytes);size_t count=_index.size()+(adding?1:0);
    for(auto it=_order.begin();(available<needed||count>_max_entries)&&it!=_order.end();++it)
      if(it!=excluded&&!it->seed&&!it->connected){available+=it->peer.id.capacity()+it->peer.address.capacity();--count;}
    return available>=needed&&count<=_max_entries;
  }
  void erase(Order::iterator it)noexcept
  {_bytes-=it->peer.id.capacity()+it->peer.address.capacity();_index.erase(std::string_view(it->peer.id));_order.erase(it);++_evictions;}
  void make_room(size_t needed,size_t old_bytes,Order::iterator excluded)noexcept
  {
    for(auto it=_order.begin();(_bytes-old_bytes>_max_bytes-needed||_index.size()>_max_entries)&&it!=_order.end();)
      if(it!=excluded&&!it->seed&&!it->connected){auto victim=it++;erase(victim);}else ++it;
  }
public:
  struct Stats{size_t entries,peer_capacity_bytes,peak_peer_capacity_bytes,max_entries,max_peer_capacity_bytes;uint64_t evictions,capacity_refusals,allocation_refusals,sdk_observations;};
  explicit PeerAdviceStore(size_t entries=1024,size_t capacity=1024*1024):_max_entries(entries),_max_bytes(capacity)
  {if(!entries||!capacity)throw std::invalid_argument("Native peer advice limits must be positive");}
  PeerAdviceStore(const PeerAdviceStore&)=delete;
  PeerAdviceStore&operator=(const PeerAdviceStore&)=delete;
  bool remember(const PeerID& peer,bool seed=false,bool connected=false)noexcept
  {
    if(peer.id.empty()||peer.address.empty())return false;
    try
    {
      const auto minimum=std::string().capacity();
      if(peer.id.size()>_max_bytes||peer.address.size()>_max_bytes-peer.id.size()
         ||std::max(peer.id.size(),minimum)>_max_bytes
         ||std::max(peer.address.size(),minimum)>_max_bytes-std::max(peer.id.size(),minimum))
      {++_capacity_refusals;return false;}
      auto found=_index.find(std::string_view(peer.id));
      if(found!=_index.end())
      {
        auto it=found->second;
        if(it->peer.address==peer.address)
        {it->seed|=seed;it->connected|=connected;_order.splice(_order.end(),_order,it);return true;}
        const auto minimum_address=std::max(peer.address.size(),minimum);
        if(minimum_address>_max_bytes-it->peer.id.capacity()
           ||!can_fit(it->peer.id.capacity()+minimum_address,bytes(it->peer),it,false))
        {++_capacity_refusals;return false;}
        std::string ready(peer.address);
        if(ready.capacity()>std::numeric_limits<size_t>::max()-it->peer.id.capacity())throw std::length_error("Peer advice capacity overflow");
        const auto needed=it->peer.id.capacity()+ready.capacity(),old=bytes(it->peer);
        if(!can_fit(needed,old,it,false)){++_capacity_refusals;return false;}
        make_room(needed,old,it);_bytes=_bytes-old+needed;it->peer.address.swap(ready);it->seed|=seed;it->connected|=connected;
        _order.splice(_order.end(),_order,it);_peak_bytes=std::max(_peak_bytes,_bytes);return true;
      }
      if(!can_fit(std::max(peer.id.size(),minimum)+std::max(peer.address.size(),minimum),0,_order.end(),true))
      {++_capacity_refusals;return false;}
      PeerID ready(peer);const auto provisional=bytes(ready);
      if(!can_fit(provisional,0,_order.end(),true)){++_capacity_refusals;return false;}
      _order.push_back(Entry{std::move(ready),seed,connected});auto staged=std::prev(_order.end());
      struct Rollback{Order& order;Order::iterator it;bool armed=true;~Rollback(){if(armed)order.erase(it);}} rollback{_order,staged};
      const auto actual=bytes(staged->peer);
      if(!can_fit(actual,0,staged,true)){++_capacity_refusals;return false;}
      const auto inserted=_index.emplace(std::string_view(staged->peer.id),staged).second;
      if(!inserted){++_capacity_refusals;return false;}
      rollback.armed=false;
      make_room(actual,0,staged);_bytes+=actual;_peak_bytes=std::max(_peak_bytes,_bytes);return true;
    }
    catch(const std::bad_alloc&){++_allocation_refusals;return false;}
    catch(const std::length_error&){++_capacity_refusals;return false;}
  }
  void disconnected(std::string_view id)noexcept{auto it=_index.find(id);if(it!=_index.end())it->second->connected=false;}
  void protect(std::string_view id,bool seed,bool connected)noexcept
  {auto it=_index.find(id);if(it!=_index.end()){it->second->seed|=seed;it->second->connected|=connected;}}
  void disconnected_all()noexcept{for(auto& entry:_order)entry.connected=false;}
  void note_capacity_refusal()noexcept{++_capacity_refusals;}
  void note_allocation_refusal()noexcept{++_allocation_refusals;}
  void note_sdk_observation()noexcept{++_sdk_observations;}
  template<typename Visitor>void visit(Visitor&& visitor)const{for(const auto& [id,it]:_index)visitor(it->peer);}
  Stats stats()const noexcept{return {_index.size(),_bytes,_peak_bytes,_max_entries,_max_bytes,_evictions,_capacity_refusals,_allocation_refusals,_sdk_observations};}
};
} // namespace koinos::node::p2p::detail
