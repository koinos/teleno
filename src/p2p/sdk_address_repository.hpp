#pragma once

#include <libp2p/peer/address_repository/inmem_address_repository.hpp>
#include <libp2p/peer/errors.hpp>
#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace koinos::node::p2p::detail {

struct SdkAddressLimits
{
  size_t peers=1024, addresses=8192, slots=8192, encoded_bytes=4*1024*1024;
  size_t addresses_per_peer=64;
};

// Retained counts/slots and binary+text encoding lengths, not heap/RSS.
// One serialized staged peer, private string/allocator overhead, inherited DNS
// history, decoded inputs and SDK/caller copies are additional consumers.
class SdkAddressBudget
{
  friend class SdkAddressRepository;
public:
  struct Stats
  {
    size_t peers=0, addresses=0, slots=0, encoded_bytes=0, repositories=0;
    size_t peak_peers=0, peak_addresses=0, peak_slots=0, peak_encoded_bytes=0;
    uint64_t evicted_peers=0, capacity_bypasses=0, allocation_bypasses=0;
    uint64_t observer_failures=0, protection_failures=0, reentrant_refusals=0;
    uint64_t expired_addresses=0, collection_runs=0, retired_bypasses=0;
    uint64_t maintenance_runs=0, maintenance_failures=0, maintenance_schedule_failures=0;
    SdkAddressLimits limits;
  };
  explicit SdkAddressBudget(SdkAddressLimits limits={})
  {
    if(!limits.peers||!limits.addresses||!limits.slots||!limits.encoded_bytes
       ||!limits.addresses_per_peer||limits.addresses_per_peer>64)
      throw std::invalid_argument("SDK address limits must be positive; per-peer limit must not exceed 64");
    _stats.limits=limits;
  }
  Stats stats()const {std::lock_guard lock(_mutex);return _stats;}
  void maintenance_finished(bool success)
  {std::lock_guard lock(_mutex);++_stats.maintenance_runs;if(!success)++_stats.maintenance_failures;}
  void maintenance_schedule_failed()
  {std::lock_guard lock(_mutex);++_stats.maintenance_schedule_failures;}
private:
  // Signals may synchronously inspect the already-published repository. A
  // writer guard refuses recursive mutations, retaining one staged-peer bound.
  mutable std::recursive_mutex _mutex;
  bool _writing=false;
  Stats _stats;
  void peak()noexcept
  {
    _stats.peak_peers=std::max(_stats.peak_peers,_stats.peers);
    _stats.peak_addresses=std::max(_stats.peak_addresses,_stats.addresses);
    _stats.peak_slots=std::max(_stats.peak_slots,_stats.slots);
    _stats.peak_encoded_bytes=std::max(_stats.peak_encoded_bytes,_stats.encoded_bytes);
  }
};

// Base bootstrap resolution dispatches virtual addAddresses into this store.
// Its separate DNS-visited history is not covered by this address policy.
class SdkAddressRepository final : public libp2p::peer::InmemAddressRepository
{
  using Id=libp2p::peer::PeerId;
  using Address=libp2p::Multiaddress;
  using Clock=libp2p::peer::Clock;
  struct Slot {Address address;Clock::time_point expires;};
  struct Peer {Id id;std::vector<Slot> rows;size_t encoded_bytes;};
  using Order=std::list<Peer>;
  using Index=std::map<std::string_view,Order::iterator,std::less<>>;
  std::shared_ptr<SdkAddressBudget> _budget;
  std::function<bool(const Id&)> _protected;
  bool _retired=false;
  Order _order;
  Index _index; // Views borrow IDs in stable list nodes; destroy index first.
  struct Writer
  {
    SdkAddressBudget& budget;bool entered;
    explicit Writer(SdkAddressBudget& b):budget(b),entered(!b._writing)
    {if(entered)b._writing=true;else ++b._stats.reentrant_refusals;}
    ~Writer(){if(entered)budget._writing=false;}
  };
  static std::string_view key(const Id& id)noexcept
  {const auto& b=id.toVector();return {reinterpret_cast<const char*>(b.data()),b.size()};}
  static size_t encoded(const Address& a)
  {
    auto text=a.getStringAddress().size(),bytes=a.getBytesAddress().size();
    if(bytes>std::numeric_limits<size_t>::max()-text)throw std::length_error("SDK address encoding length overflow");
    return text+bytes;
  }
  static Clock::time_point expiry(Milliseconds ttl)noexcept
  {
    const auto now=Clock::now();using D=Clock::duration;
    // Avoid overflowing the implicit milliseconds -> clock-duration conversion,
    // including the SDK's milliseconds::max permanent sentinel.
    if(ttl>=std::chrono::duration_cast<Milliseconds>(D::max()))return Clock::time_point::max();
    if(ttl<=std::chrono::duration_cast<Milliseconds>(D::min()))return Clock::time_point::min();
    const auto delta=std::chrono::duration_cast<D>(ttl),current=now.time_since_epoch();
    if(delta>D::zero()&&current>D::max()-delta)return Clock::time_point::max();
    if(delta<D::zero()&&current<D::min()-delta)return Clock::time_point::min();
    return now+delta;
  }
  bool protected_peer(const Id& id)noexcept
  {
    try{return _protected&&_protected(id);}
    catch(...){++_budget->_stats.protection_failures;return true;}
  }
  void removed(const Id& id,const Address& address)noexcept
  {try{signal_removed_(id,address);}catch(...){++_budget->_stats.observer_failures;}}
  void added(const Id& id,const Address& address)noexcept
  {try{signal_added_(id,address);}catch(...){++_budget->_stats.observer_failures;}}
  void erase_peer(Order::iterator it,bool eviction)noexcept
  {
    auto& s=_budget->_stats;const auto count=it->rows.size(),slots=it->rows.capacity(),bytes=it->encoded_bytes;
    for(const auto& row:it->rows)removed(it->id,row.address);
    _index.erase(key(it->id));_order.erase(it); // Storage dies before credits.
    --s.peers;s.addresses-=count;s.slots-=slots;s.encoded_bytes-=bytes;
    if(eviction)++s.evicted_peers;
  }
  static auto find(std::vector<Slot>& rows,const Address& a)
  {return std::find_if(rows.begin(),rows.end(),[&](const Slot& row){return row.address==a;});}
  static auto find(const std::vector<Slot>& rows,const Address& a)
  {return std::find_if(rows.begin(),rows.end(),[&](const Slot& row){return row.address==a;});}
  outcome::result<bool> remember(const Id& id,std::span<const Address> input,Milliseconds ttl,bool upsert)
  {
    std::lock_guard lock(_budget->_mutex);Writer writer(*_budget);if(!writer.entered)return false;
    auto& s=_budget->_stats;const auto limits=s.limits;
    if(_retired){++s.retired_bypasses;return false;}
    auto found=_index.find(key(id));auto old=found==_index.end()?_order.end():found->second;
    try
    {
      const auto until=expiry(ttl);bool novel=false;
      for(const auto& address:input)if(old==_order.end()||find(old->rows,address)==old->rows.end()){novel=true;break;}
      if(old!=_order.end()&&!novel)
      {
        if(upsert)for(const auto& address:input){auto row=find(old->rows,address);if(row!=old->rows.end())row->expires=until;}
        if(!input.empty())_order.splice(_order.end(),_order,old);
        return false;
      }
      const size_t id_bytes=id.toVector().size();
      if(id_bytes>limits.encoded_bytes){++s.capacity_bypasses;return false;}
      // Determine the largest useful replacement using borrowed inputs. This
      // fixed-size plan owns no address copies and includes only unique rows
      // that could fit even if every disposable record were reclaimed.
      size_t wanted_count=old==_order.end()?0:old->rows.size();
      size_t wanted_bytes=old==_order.end()?id_bytes:old->encoded_bytes;
      std::array<size_t,64> wanted{};size_t wanted_size=0;
      for(size_t i=0;i<input.size();++i)
      {
        if(old!=_order.end()&&find(old->rows,input[i])!=old->rows.end())continue;
        bool duplicate=false;for(size_t n=0;n<wanted_size;++n)duplicate|=input[wanted[n]]==input[i];
        if(duplicate)continue;
        const auto bytes=encoded(input[i]);
        if(wanted_count>=limits.addresses_per_peer||wanted_count>=limits.addresses||wanted_count>=limits.slots
           ||bytes>limits.encoded_bytes-wanted_bytes)continue;
        wanted[wanted_size++]=i;++wanted_count;wanted_bytes+=bytes;
      }
      // Plan only as many disposable victims as this replacement can use.
      // Most insertions need no scan; saturated single-peer churn usually needs
      // one victim. Other generations remain charged and cannot be evicted.
      size_t peer_room=limits.peers-s.peers,addr_room=limits.addresses-s.addresses;
      size_t slot_room=limits.slots-s.slots,byte_room=limits.encoded_bytes-s.encoded_bytes;
      if(old!=_order.end())
      {++peer_room;addr_room+=old->rows.size();slot_room+=old->rows.capacity();byte_room+=old->encoded_bytes;}
      const auto enough=[&]{return peer_room&&addr_room>=wanted_count&&slot_room>=wanted_count&&byte_room>=wanted_bytes;};
      std::vector<Order::iterator> eligible;
      for(auto it=_order.begin();!enough()&&it!=_order.end();++it)
        if(it!=old&&!protected_peer(it->id))
        {
          ++peer_room;addr_room+=it->rows.size();slot_room+=it->rows.capacity();byte_room+=it->encoded_bytes;
          eligible.push_back(it);
        }
      if(!peer_room||id_bytes>byte_room){++s.capacity_bypasses;return false;}
      size_t count=old==_order.end()?0:old->rows.size();
      size_t bytes=old==_order.end()?id_bytes:old->encoded_bytes;
      std::array<size_t,64> selected{};size_t selected_count=0;bool bypassed=false;
      for(size_t i=0;i<input.size();++i)
      {
        if(old!=_order.end()&&find(old->rows,input[i])!=old->rows.end())continue;
        bool duplicate=false;for(size_t n=0;n<selected_count;++n)duplicate|=input[selected[n]]==input[i];
        if(duplicate)continue;
        const auto n=encoded(input[i]);
        if(count>=limits.addresses_per_peer||count>=addr_room||count>=slot_room||bytes>byte_room||n>byte_room-bytes)
        {bypassed=true;continue;}
        selected[selected_count++]=i;++count;bytes+=n;
      }
      if(bypassed)++s.capacity_bypasses;
      if(!selected_count)
      {
        if(old!=_order.end())
        {
          if(upsert)for(const auto& address:input){auto row=find(old->rows,address);if(row!=old->rows.end())row->expires=until;}
          return false;
        }
        if(!input.empty())return false;
      }
      std::vector<Slot> ready;ready.reserve(count);
      if(ready.capacity()>slot_room){++s.capacity_bypasses;return false;}
      if(old!=_order.end())for(const auto& row:old->rows)ready.push_back(row);
      for(size_t n=0;n<selected_count;++n)ready.push_back(Slot{input[selected[n]],until});
      if(upsert)for(const auto& address:input){auto row=find(ready,address);if(row!=ready.end())row->expires=until;}
      // Prepare both list and index nodes off to the side. No partially staged
      // peer is visible to synchronous eviction observers.
      Order stage;Index stage_index;typename Index::node_type node;
      if(old==_order.end())
      {
        stage.push_back(Peer{id,std::move(ready),bytes});auto it=stage.begin();
        stage_index.emplace(key(it->id),it);node=stage_index.extract(stage_index.begin());
      }
      const auto new_slots=old==_order.end()?stage.front().rows.capacity():ready.capacity();
      const size_t old_count=old==_order.end()?0:old->rows.size();
      const size_t old_slots=old==_order.end()?0:old->rows.capacity();
      const size_t old_bytes=old==_order.end()?0:old->encoded_bytes;
      size_t projected_peers=s.peers,projected_addresses=s.addresses;
      size_t projected_slots=s.slots,projected_bytes=s.encoded_bytes,victims=0;
      auto fits=[&]{return projected_peers-(old==_order.end()?0:1)<limits.peers
        &&projected_addresses-old_count<=limits.addresses-count
        &&projected_slots-old_slots<=limits.slots-new_slots
        &&projected_bytes-old_bytes<=limits.encoded_bytes-bytes;};
      // Freeze victim selection before any mutation or observer callback. A
      // changing/throwing protection predicate cannot strand partial eviction.
      while(!fits()&&victims<eligible.size())
      {
        const auto& victim=*eligible[victims++];--projected_peers;
        projected_addresses-=victim.rows.size();projected_slots-=victim.rows.capacity();projected_bytes-=victim.encoded_bytes;
      }
      if(!fits()){++s.capacity_bypasses;return false;}
      // All preparation is complete; destruction, splice, swap and compatible
      // map-node insertion do not allocate. Throwing observers stay optional.
      for(size_t i=0;i<victims;++i)erase_peer(eligible[i],true);
      if(old==_order.end())
      {
        auto it=stage.begin();_order.splice(_order.end(),stage,it);_index.insert(std::move(node));
        ++s.peers;s.addresses+=count;s.slots+=new_slots;s.encoded_bytes+=bytes;
      }
      else
      {
        old->rows.swap(ready);old->encoded_bytes=bytes;_order.splice(_order.end(),_order,old);
        {std::vector<Slot> retired;retired.swap(ready);} // Free before returning credits.
        s.addresses=s.addresses-old_count+count;s.slots=s.slots-old_slots+new_slots;s.encoded_bytes=s.encoded_bytes-old_bytes+bytes;
      }
      _budget->peak();for(size_t n=0;n<selected_count;++n)added(id,input[selected[n]]);
      return selected_count!=0;
    }
    catch(const std::bad_alloc&){++s.allocation_bypasses;return false;}
    catch(const std::length_error&){++s.capacity_bypasses;return false;}
  }
public:
  SdkAddressRepository(std::shared_ptr<libp2p::network::DnsaddrResolver> resolver,
                       std::shared_ptr<SdkAddressBudget> budget)
    :libp2p::peer::InmemAddressRepository(std::move(resolver)),_budget(std::move(budget))
  {
    if(!_budget)throw std::invalid_argument("SDK address budget is required");
    std::lock_guard lock(_budget->_mutex);++_budget->_stats.repositories;
  }
  ~SdkAddressRepository()override
  {
    std::lock_guard lock(_budget->_mutex);auto& s=_budget->_stats;
    size_t peers=_order.size(),addresses=0,slots=0,bytes=0;
    for(const auto& peer:_order){addresses+=peer.rows.size();slots+=peer.rows.capacity();bytes+=peer.encoded_bytes;}
    _index.clear();_order.clear();s.peers-=peers;s.addresses-=addresses;s.slots-=slots;s.encoded_bytes-=bytes;--s.repositories;
  }
  void protect(std::function<bool(const Id&)> predicate)
  {
    std::lock_guard lock(_budget->_mutex);
    if(_budget->_writing)throw std::logic_error("Recursive SDK address protection update");
    _protected=std::move(predicate);
  }
  void retire()
  {
    std::lock_guard lock(_budget->_mutex);Writer writer(*_budget);
    if(!writer.entered)throw std::logic_error("Recursive SDK address retirement");
    _retired=true; // Existing data remains charged until actual disposal.
  }
  using libp2p::peer::AddressRepository::bootstrap;
  void bootstrap(const Address& address,std::function<BootstrapCallback> cb)override
  {
    bool retired;
    {std::lock_guard lock(_budget->_mutex);retired=_retired;}
    if(retired){cb(std::make_error_code(std::errc::operation_canceled));return;}
    libp2p::peer::InmemAddressRepository::bootstrap(address,std::move(cb));
  }
  outcome::result<bool> addAddresses(const Id& id,std::span<const Address> input,Milliseconds ttl)override
  {return remember(id,input,ttl,false);}
  outcome::result<bool> upsertAddresses(const Id& id,std::span<const Address> input,Milliseconds ttl)override
  {return remember(id,input,ttl,true);}
  outcome::result<void> updateAddresses(const Id& id,Milliseconds ttl)override
  {
    std::lock_guard lock(_budget->_mutex);Writer writer(*_budget);
    if(!writer.entered)return std::make_error_code(std::errc::resource_unavailable_try_again);
    if(_retired)return std::make_error_code(std::errc::operation_canceled);
    auto found=_index.find(key(id));if(found==_index.end())return libp2p::peer::PeerError::NOT_FOUND;
    const auto until=expiry(ttl);for(auto& row:found->second->rows)row.expires=until;
    return outcome::success();
  }
  void dialFailed(const Id& id,const Address& address)override
  {
    static_assert(std::is_nothrow_move_constructible_v<Slot>&&std::is_nothrow_move_assignable_v<Slot>);
    std::lock_guard lock(_budget->_mutex);Writer writer(*_budget);
    if(!writer.entered)throw std::logic_error("Recursive SDK address dial ordering");
    auto p=_index.find(key(id));if(p==_index.end())return;auto& rows=p->second->rows;auto it=find(rows,address);
    if(it!=rows.end())std::rotate(it,std::next(it),rows.end());
  }
  outcome::result<std::vector<Address>> getAddresses(const Id& id)const override
  {
    std::lock_guard lock(_budget->_mutex);auto found=_index.find(key(id));
    if(found==_index.end())return libp2p::peer::PeerError::NOT_FOUND;
    std::vector<Address> out;out.reserve(found->second->rows.size());
    for(const auto& row:found->second->rows)out.push_back(row.address);
    return out;
  }
  std::unordered_set<Id> getPeers()const override
  {
    std::lock_guard lock(_budget->_mutex);std::unordered_set<Id> out;out.reserve(_order.size());
    for(const auto& peer:_order)out.insert(peer.id);return out;
  }
  void clear(const Id& id)override
  {
    std::lock_guard lock(_budget->_mutex);Writer writer(*_budget);
    if(!writer.entered)throw std::logic_error("Recursive SDK address clear");
    auto found=_index.find(key(id));if(found==_index.end())return;auto& peer=*found->second;auto& s=_budget->_stats;
    const auto count=peer.rows.size(),slots=peer.rows.capacity(),bytes=peer.encoded_bytes-id.toVector().size();
    for(const auto& row:peer.rows)removed(peer.id,row.address);
    {std::vector<Slot> gone;gone.swap(peer.rows);}peer.encoded_bytes-=bytes;
    s.addresses-=count;s.slots-=slots;s.encoded_bytes-=bytes;
  }
  void collectGarbage()override
  {
    std::lock_guard lock(_budget->_mutex);Writer writer(*_budget);
    if(!writer.entered)throw std::logic_error("Recursive SDK address collection");
    auto& s=_budget->_stats;++s.collection_runs;const auto now=Clock::now();
    for(auto peer=_order.begin();peer!=_order.end();)
    {
      auto& rows=peer->rows;
      for(auto row=rows.begin();row!=rows.end();)
        if(row->expires<=now)
        {
          const auto bytes=encoded(row->address);removed(peer->id,row->address);row=rows.erase(row);
          --s.addresses;s.encoded_bytes-=bytes;peer->encoded_bytes-=bytes;++s.expired_addresses;
        }else ++row;
      if(rows.empty()){auto gone=peer++;erase_peer(gone,false);}else ++peer;
    }
  }
};
} // namespace koinos::node::p2p::detail
