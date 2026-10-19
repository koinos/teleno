#pragma once

#include "peer_snapshot.hpp"
#include "local_transport_error.hpp"
#include <algorithm>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace koinos::node::p2p::detail {
class PeerSnapshotBudget
{
  struct State
  {
    std::mutex mutex;
    size_t jobs=0, entries=0, bytes=0, peak_jobs=0, peak_entries=0, peak_bytes=0;
    const size_t max_jobs, max_entries, max_bytes;
    uint64_t refusals=0;
    State(size_t j,size_t e,size_t b):max_jobs(j),max_entries(e),max_bytes(b){}
  };
  struct Credit
  {
    std::shared_ptr<State> state;
    size_t entries,bytes;
    Credit(std::shared_ptr<State> s,size_t e,size_t b):state(std::move(s)),entries(e),bytes(b){}
    ~Credit(){if(state){std::lock_guard lock(state->mutex);--state->jobs;state->entries-=entries;state->bytes-=bytes;}}
    Credit(const Credit&)=delete;
    void resize(size_t e,size_t b)
    {
      std::lock_guard lock(state->mutex);
      if((e>entries&&e-entries>state->max_entries-state->entries)
         ||(b>bytes&&b-bytes>state->max_bytes-state->bytes))
      {++state->refusals;throw LocalTransportUnavailable(LocalTransportUnavailable::Reason::peer_snapshot_capacity);}
      state->entries=state->entries-entries+e;state->bytes=state->bytes-bytes+b;entries=e;bytes=b;
      state->peak_entries=std::max(state->peak_entries,state->entries);state->peak_bytes=std::max(state->peak_bytes,state->bytes);
    }
  };
  std::shared_ptr<State> _state;
public:
  struct Stats
  {
    size_t jobs,entries,capacity_bytes,peak_jobs,peak_entries,peak_capacity_bytes,max_jobs,max_entries,max_capacity_bytes;
    uint64_t capacity_refusals;
  };
  PeerSnapshotBudget(size_t jobs=4,size_t entries=4096,size_t bytes=4*1024*1024)
    :_state(std::make_shared<State>(jobs,entries,bytes))
  {if(!jobs||!entries||!bytes)throw std::invalid_argument("Native peer snapshot limits must be positive");}
  static size_t add(size_t a,size_t b)
  {
    if(b>std::numeric_limits<size_t>::max()-a)
      throw LocalTransportUnavailable(LocalTransportUnavailable::Reason::peer_snapshot_capacity);
    return a+b;
  }
  static size_t strings(const PeerID& p,bool retained=false)
  {
    const auto minimum=std::string().capacity();
    return add(retained?p.id.capacity():std::max(p.id.size(),minimum),
               retained?p.address.capacity():std::max(p.address.size(),minimum));
  }
  template<typename Builder> PeerSnapshot make(size_t entries,size_t string_bytes,Builder&& builder)
  {
    if(!entries)return {};
    const bool vector_overflow=entries>std::numeric_limits<size_t>::max()/sizeof(PeerID);
    const size_t vector_bytes=vector_overflow?0:entries*sizeof(PeerID);
    const bool overflow=vector_overflow||string_bytes>std::numeric_limits<size_t>::max()-vector_bytes;
    const size_t bytes=overflow?0:vector_bytes+string_bytes;
    {
      std::lock_guard lock(_state->mutex);
      if(overflow||_state->jobs>=_state->max_jobs||entries>_state->max_entries-_state->entries||bytes>_state->max_bytes-_state->bytes)
      {++_state->refusals;throw LocalTransportUnavailable(LocalTransportUnavailable::Reason::peer_snapshot_capacity);}
      ++_state->jobs;_state->entries+=entries;_state->bytes+=bytes;
      _state->peak_jobs=std::max(_state->peak_jobs,_state->jobs);_state->peak_entries=std::max(_state->peak_entries,_state->entries);_state->peak_bytes=std::max(_state->peak_bytes,_state->bytes);
    }
    // Return the provisional reservation even if allocating its shared owner fails.
    struct Rollback
    {
      std::shared_ptr<State> state;size_t entries,bytes;
      ~Rollback(){if(state){std::lock_guard lock(state->mutex);--state->jobs;state->entries-=entries;state->bytes-=bytes;}}
    } rollback{_state,entries,bytes};
    auto credit=std::make_shared<Credit>(_state,entries,bytes);rollback.state.reset();
    std::vector<PeerID> peers;peers.reserve(entries);std::forward<Builder>(builder)(peers);
    if(peers.size()!=entries)throw std::logic_error("Native peer snapshot projection changed during construction");
    size_t actual=0;for(const auto& peer:peers)actual=add(actual,strings(peer,true));
    if(peers.capacity()>std::numeric_limits<size_t>::max()/sizeof(PeerID))throw LocalTransportUnavailable(LocalTransportUnavailable::Reason::peer_snapshot_capacity);
    actual=add(actual,peers.capacity()*sizeof(PeerID));credit->resize(peers.capacity(),actual);
    return PeerSnapshot(std::move(credit),std::move(peers));
  }
  Stats stats()const
  {
    std::lock_guard lock(_state->mutex);return {_state->jobs,_state->entries,_state->bytes,_state->peak_jobs,_state->peak_entries,_state->peak_bytes,_state->max_jobs,_state->max_entries,_state->max_bytes,_state->refusals};
  }
};
} // namespace koinos::node::p2p::detail
