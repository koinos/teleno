#pragma once

#include "sdk_address_repository.hpp"
#include <boost/asio/steady_timer.hpp>
#include <libp2p/host/host.hpp>
#include <libp2p/network/network.hpp>
#include <libp2p/network/connection_manager.hpp>
#include <libp2p/peer/peer_repository.hpp>
#include <libp2p/peer/protocol_repository.hpp>
#include <atomic>

namespace koinos::node::p2p::detail {
inline void arm_sdk_metadata_maintenance(
  const std::shared_ptr<boost::asio::steady_timer>& timer,
  std::weak_ptr<libp2p::Host> host,
  const std::shared_ptr<std::atomic<bool>>& active,
  const std::shared_ptr<SdkAddressBudget>& budget,
  std::chrono::milliseconds interval) noexcept
{
  if(!active->load(std::memory_order_acquire))return;
  try
  {
    timer->expires_after(interval);
    timer->async_wait([weak=std::weak_ptr(timer),host,active,budget,interval](boost::system::error_code error){
      if(error||!active->load(std::memory_order_acquire))return;
      auto timer=weak.lock();auto owner=host.lock();if(!timer||!owner)return;
      bool success=true;
      // Each collector is optional and isolated. One local failure must not
      // prevent the other stores from reclaiming already-disposable metadata.
      try{owner->getPeerRepository().getAddressRepository().collectGarbage();}catch(...){success=false;}
      try{owner->getPeerRepository().getProtocolRepository().collectGarbage();}catch(...){success=false;}
      try{owner->getNetwork().getConnectionManager().collectGarbage();}catch(...){success=false;}
      budget->maintenance_finished(success);
      arm_sdk_metadata_maintenance(timer,host,active,budget,interval);
    });
  }
  catch(...)
  {
    // Failed timer admission is observable, with no spin, retrying RPC, peer
    // penalty or new work after retirement. The address retention cap remains.
    try{budget->maintenance_schedule_failed();}catch(...){}
  }
}
} // namespace koinos::node::p2p::detail
