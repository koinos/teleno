#include "core/owned_allocation.hpp"
// Isolated TCP/Noise/Yamux peers on loopback; no seeds, DHT or public network.
#include "p2p/libp2p_transport.hpp"
#include "p2p/gorpc_codec.hpp"
#include "p2p/peer_rpc_read.hpp"
#include "p2p/peer_rpc_write.hpp"

#include <boost/di.hpp>
#include <boost/di/extension/scopes/shared.hpp>
#include <libp2p/basic/write_return_size.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <libp2p/muxer/yamux/yamux.hpp>
#include <libp2p/network/connection_manager.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <limits>
namespace dial_test_alloc {
struct alignas(std::max_align_t) Header { size_t size; bool counted; };
thread_local long fail_after = -1;
thread_local bool tracking = false;
std::atomic<unsigned long long> live{0}, peak{0}, total{0}, attempts{0};
}
void* operator new( std::size_t size )
{
  using namespace dial_test_alloc;
  if( fail_after == 0 ) { fail_after = -1; throw std::bad_alloc(); }
  if( fail_after > 0 ) --fail_after;
  if( size > std::numeric_limits<size_t>::max() - sizeof(Header) ) throw std::bad_alloc();
  auto p = static_cast<Header*>( koinos::node::owned_allocation::allocate( sizeof(Header) + (size ? size : 1) ) );
  if( !p ) throw std::bad_alloc();p->size = size;p->counted = tracking;
  if( tracking )
  {
    auto now = live.fetch_add(size) + size;auto old = peak.load();
    while( old < now && !peak.compare_exchange_weak(old, now) ) {}
    total.fetch_add(size);attempts.fetch_add(1);
  }
  return p + 1;
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept
{
  if( !p ) return;auto h = static_cast<dial_test_alloc::Header*>(p) - 1;
  if( h->counted ) dial_test_alloc::live.fetch_sub(h->size);koinos::node::owned_allocation::deallocate(h);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }

#include <condition_variable>
#include <future>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {
using namespace std::chrono_literals;
namespace p2p = koinos::node::p2p;
namespace gorpc = p2p::gorpc;
using Stream = std::shared_ptr< libp2p::connection::Stream >;

void require( bool ok, const char* message )
{ if( !ok ) throw std::runtime_error( message ); }

template< typename T > T awaited( std::future< T >& future )
{
  require( future.wait_for( 6s ) == std::future_status::ready, "loopback fixture operation timed out" );
  return future.get();
}

auto injector_for( const std::shared_ptr< boost::asio::io_context >& io )
{
  return libp2p::injector::makeHostInjector< boost::di::extension::shared_config >(
    boost::di::bind< boost::asio::io_context >.to( io )[ boost::di::override ],
    libp2p::injector::useLibp2pClientVersion( libp2p::Libp2pClientVersion{ "koinos/p2p/1.0.0" } ),
    libp2p::injector::useLayerAdaptors<>(),
    libp2p::injector::useMuxerAdaptors< libp2p::muxer::Yamux >(),
    libp2p::injector::useTransportAdaptors< libp2p::transport::TcpTransport >() );
}

struct ExactRead : std::enable_shared_from_this< ExactRead >
{
  Stream stream;
  libp2p::Bytes bytes;
  size_t used = 0;
  std::promise< outcome::result< libp2p::Bytes > > done;
  ExactRead( Stream input, size_t count ) : stream( std::move( input ) ), bytes( count ) {}
  void next()
  {
    stream->readSome( libp2p::BytesOut( bytes ).subspan( used ), bytes.size() - used,
      [self = shared_from_this()]( outcome::result< size_t > result ) {
        if( !result.has_value() ) { self->done.set_value( result.error() );return; }
        const auto count = result.value();
        if( count == 0 || count > self->bytes.size() - self->used )
        { self->done.set_value( std::make_error_code( std::errc::io_error ) );return; }
        self->used += count;
        if( self->used == self->bytes.size() ) self->done.set_value( std::move( self->bytes ) );
        else self->next();
      } );
  }
};

struct FixturePeer
{
  std::shared_ptr< boost::asio::io_context > io = std::make_shared< boost::asio::io_context >();
  using Injector = decltype(injector_for(io));
  // Heap ownership gives reused dependency mutexes explicit allocator lifetimes,
  // matching the native runtime and avoiding stale TSan stack-mutex identities.
  std::unique_ptr<Injector> injector = std::make_unique<Injector>(injector_for(io));
  std::shared_ptr< libp2p::Host > host = injector->create< std::shared_ptr< libp2p::Host > >();
  boost::asio::executor_work_guard< boost::asio::io_context::executor_type > guard = boost::asio::make_work_guard( *io );
  std::thread runner;
  libp2p::event::Handle connected;
  libp2p::event::Handle disconnected;
  std::mutex mutex;
  std::condition_variable changed;
  std::optional< libp2p::peer::PeerId > remote;
  size_t connections = 0;
  size_t disconnections = 0;
  p2p::PeerID endpoint;

  FixturePeer()
  {
    connected = host->setOnNewConnectionHandler( [this]( libp2p::peer::PeerInfo&& peer ) {
      std::lock_guard lock( mutex );remote = peer.id;++connections;changed.notify_all();
    } );
    disconnected = host->getBus()
      .getChannel< libp2p::event::network::OnPeerDisconnectedChannel >()
      .subscribe( [this]( const libp2p::peer::PeerId& ) {
        std::lock_guard lock( mutex );++disconnections;changed.notify_all();
      } );
    auto address = libp2p::multi::Multiaddress::create( "/ip4/127.0.0.1/tcp/0" );
    require( address.has_value() && host->listen( address.value() ).has_value(), "listen on loopback only" );
    host->start();
    const auto addresses = host->getAddressesInterfaces();
    require( !addresses.empty(), "fixture host must expose its actual listening address" );
    endpoint.id = host->getId().toBase58();
    endpoint.address = std::string( addresses.front().getStringAddress() ) + "/p2p/" + endpoint.id;
    runner = std::thread( [this] { io->run(); } );
  }

  ~FixturePeer()
  {
    auto done = std::make_shared< std::promise< void > >();auto future = done->get_future();
    boost::asio::post( *io, [this, done] {
      connected.unsubscribe();disconnected.unsubscribe();host->stop();
      auto& manager = host->getNetwork().getConnectionManager();
      for( const auto& connection: manager.getConnections() )
        if( connection && !connection->isClosed() )
        {
          auto peer = connection->remotePeer();
          if( peer.has_value() ) manager.closeConnectionsToPeer( peer.value() );
          else (void)connection->close();
        }
      boost::asio::post( *io, [done] { done->set_value(); } );
    } );
    future.wait();guard.reset();io->stop();runner.join();
  }

  libp2p::peer::PeerId peer_after( size_t previous )
  {
    std::unique_lock lock( mutex );
    require( changed.wait_for( lock, 6s, [&] { return connections > previous; } ), "transport did not connect/reconnect" );
    return *remote;
  }

  void require_disconnected_after( size_t previous )
  {
    std::unique_lock lock( mutex );
    require( changed.wait_for( lock, 6s, [&] { return disconnections > previous; } ),
             "transport stop must close its established connection" );
  }

  Stream open( const libp2p::peer::PeerId& peer )
  {
    auto done = std::make_shared< std::promise< libp2p::StreamAndProtocolOrError > >();auto future = done->get_future();
    boost::asio::post( *io, [this, peer, done] {
      host->newStream( peer, { "/koinos/peerrpc/1.0.0" }, [done]( auto result ) { done->set_value( std::move( result ) ); } );
    } );
    auto result = awaited( future );require( result.has_value(), "open real incoming Peer RPC stream" );
    return std::move( result.value().stream );
  }

  void serve( std::function< void( Stream ) > handler )
  {
    auto done = std::make_shared< std::promise< void > >();auto future = done->get_future();
    boost::asio::post( *io, [this, handler = std::move( handler ), done] {
      host->setProtocolHandler( { "/koinos/peerrpc/1.0.0" },
        [handler]( libp2p::StreamAndProtocol incoming ) { handler( std::move( incoming.stream ) ); } );
      done->set_value();
    } );
    awaited( future );
  }

  void write( Stream stream, const std::string& raw )
  {
    auto bytes = std::make_shared< libp2p::Bytes >( raw.begin(), raw.end() );
    auto done = std::make_shared< std::promise< outcome::result< size_t > > >();auto future = done->get_future();
    boost::asio::post( *io, [stream, bytes, done] {
      libp2p::writeReturnSize( stream, *bytes, [bytes, done]( auto result ) { done->set_value( std::move( result ) ); } );
    } );
    auto result = awaited( future );require( result.has_value() && result.value() == raw.size(), "write fixture request" );
  }

  std::future< outcome::result< libp2p::Bytes > > read( Stream stream, size_t count )
  {
    auto read = std::make_shared< ExactRead >( std::move( stream ), count );auto future = read->done.get_future();
    boost::asio::post( *io, [read] { read->next(); } );return future;
  }
};

void dial_failure_recovery()
{
  p2p::Libp2pTransport::Config config;config.listen_address.clear();config.enable_dht=false;
  p2p::Libp2pTransport transport(config);transport.start();
  struct Stop { p2p::Libp2pTransport& t;~Stop(){ t.stop(); } } stop{transport};
  for( long site=0;site<32;++site )
  {
    p2p::PeerID invalid{std::string(80,'d')+std::to_string(site),"/ip4/not-an-address/tcp/0"};
    bool first=false,second=false;
    dial_test_alloc::fail_after=site;
    try { transport.connect_peer(invalid); } catch(const std::exception&) { first=true; }
    dial_test_alloc::fail_after=-1;
    try { transport.connect_peer(invalid); } catch(const std::exception&) { second=true; }
    if( !first || !second )
      throw std::runtime_error("dial allocation site "+std::to_string(site)+" left a stale connecting marker; independent retry was suppressed");
  }
}

void dial_queue_measurement()
{
  std::vector<p2p::PeerID> inputs;inputs.reserve(512);
  for(unsigned i=1;i<=512;++i)
  {
    std::array<uint8_t,34> wire{};wire[0]=0x12;wire[1]=0x20;wire[32]=i>>8;wire[33]=i&255;
    auto parsed=libp2p::peer::PeerId::fromBytes(wire);require(parsed.has_value(),"fixture multihash");
    auto id=parsed.value().toBase58();inputs.push_back({id,"/ip4/127.0.0.1/tcp/9/p2p/"+id});
  }
  unsigned long long retained=0,peak=0,total=0,attempts=0;size_t accepted=0,refused=0;long long elapsed=0;
  {
    p2p::Libp2pTransport::Config config;config.listen_address.clear();config.enable_dht=false;
    p2p::Libp2pTransport transport(config);transport.start();FixturePeer peer;
    auto entered=std::make_shared<std::promise<void>>();auto ready=entered->get_future();
    std::mutex mutex;std::condition_variable changed;bool released=false;
    struct Release
    {
      p2p::Libp2pTransport& transport;std::mutex& mutex;std::condition_variable& changed;bool& released;
      ~Release(){ {std::lock_guard lock(mutex);released=true;}changed.notify_all();transport.stop(); }
    } release{transport,mutex,changed,released};
    transport.on_peer_rpc_request([&](const auto&,const auto&,const auto&){
      entered->set_value();std::unique_lock lock(mutex);changed.wait(lock,[&]{return released;});return gorpc::encode_id_response("fixture");
    });
    peer.serve([&](Stream stream){
      p2p::detail::read_rpc_request(*peer.io,stream,[&peer,stream](auto error,auto){
        if(error)return;
        auto raw=gorpc::encode_request("PeerRPCService","GetChainID",gorpc::encode_empty_request());
        auto bytes=std::make_shared<libp2p::Bytes>(raw.begin(),raw.end());
        p2p::detail::write_rpc_frame(*peer.io,stream,bytes,[stream](auto){});
      });
    });
    // Establish the control connection before sending an incoming request that
    // holds only the production transport's serialized runner.
    transport.connect_peer(peer.endpoint);auto remote=peer.peer_after(0);auto control=peer.open(remote);
    auto raw=gorpc::encode_request("PeerRPCService","GetChainID",gorpc::encode_empty_request());
    peer.write(control,raw);awaited(ready);
    dial_test_alloc::live=0;dial_test_alloc::peak=0;dial_test_alloc::total=0;dial_test_alloc::attempts=0;
    const auto start=std::chrono::steady_clock::now();dial_test_alloc::tracking=true;
    for(const auto& input:inputs)
    {
      try{transport.connect_peer(input);++accepted;}
      catch(const p2p::LocalTransportUnavailable&){++refused;}
    }
    dial_test_alloc::tracking=false;
    elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    retained=dial_test_alloc::live.load();peak=dial_test_alloc::peak.load();total=dial_test_alloc::total.load();attempts=dial_test_alloc::attempts.load();
    {std::lock_guard lock(mutex);released=true;}changed.notify_all();
    control.reset();transport.stop();
  }
  std::cout<<"{\"calls\":512,\"accepted\":"<<accepted<<",\"local_refusals\":"<<refused
    <<",\"retained_cpp_new_bytes\":"<<retained<<",\"peak_cpp_new_bytes\":"<<peak
    <<",\"total_cpp_new_bytes\":"<<total<<",\"allocation_attempts\":"<<attempts
    <<",\"elapsed_ns\":"<<elapsed<<",\"post_destructor_cpp_bytes\":"<<dial_test_alloc::live.load()<<"}\n";
}

void dial_publication_failure_recovery()
{
  std::array<uint8_t,34> wire{};wire[0]=0x12;wire[1]=0x20;wire[33]=17;
  auto id=libp2p::peer::PeerId::fromBytes(wire).value().toBase58();p2p::PeerID first{id,"/ip4/127.0.0.1/tcp/9/p2p/"+id};
  wire[33]=18;id=libp2p::peer::PeerId::fromBytes(wire).value().toBase58();p2p::PeerID second{id,"/ip4/127.0.0.1/tcp/9/p2p/"+id};
  p2p::Libp2pTransport::Config config;config.listen_address.clear();config.enable_dht=false;config.max_pending_dials=4;
  p2p::Libp2pTransport transport(config);transport.start();FixturePeer peer;
  std::promise<void> entered;auto ready=entered.get_future();std::mutex mutex;std::condition_variable changed;bool released=false;
  struct Release
  {
    p2p::Libp2pTransport& transport;std::mutex& mutex;std::condition_variable& changed;bool& released;
    ~Release(){dial_test_alloc::fail_after=-1;dial_test_alloc::tracking=false;{std::lock_guard lock(mutex);released=true;}changed.notify_all();transport.stop();}
  } release{transport,mutex,changed,released};
  transport.on_peer_rpc_request([&](const auto&,const auto&,const auto&){
    entered.set_value();std::unique_lock lock(mutex);changed.wait(lock,[&]{return released;});return gorpc::encode_id_response("post-fault-fixture");
  });
  transport.connect_peer(peer.endpoint);auto remote=peer.peer_after(0);auto control=peer.open(remote);
  peer.write(control,gorpc::encode_request("PeerRPCService","GetChainID",gorpc::encode_empty_request()));awaited(ready);
  dial_test_alloc::attempts=0;dial_test_alloc::tracking=true;transport.connect_peer(first);dial_test_alloc::tracking=false;
  const auto count=dial_test_alloc::attempts.load();require(count>4&&count<256,"measure actual caller-side dial allocation sites");
  const auto retained=transport.dial_stats().peer_capacity_bytes;
  for(size_t site=0;site<count;++site)
  {
    bool failed=false;dial_test_alloc::fail_after=site;
    try{transport.connect_peer(second);}catch(const std::exception&){failed=true;}
    dial_test_alloc::fail_after=-1;
    require(failed,"each measured valid-dial caller allocation must fail before publication");
    require(transport.dial_stats().jobs==1&&transport.dial_stats().peer_capacity_bytes==retained,
            "parse, callback/queue publication failure must restore all native dial credits");
  }
  transport.connect_peer(second);require(transport.dial_stats().jobs==2,"independent retry must publish after every local allocation failure");
  control.reset();
}

void seed_dial_admission(bool legacy_alias=false)
{
  // Initialize the pinned SDK logger before constructing its independent peer.
  p2p::Libp2pTransport::Config empty;empty.listen_address.clear();empty.enable_dht=false;
  {p2p::Libp2pTransport logging(empty);logging.start();logging.stop();}
  FixturePeer peer;
  peer.serve([&](Stream stream){
    p2p::detail::read_rpc_request(*peer.io,stream,[&peer,stream](auto error,auto){
      if(error)return;
      auto raw=gorpc::encode_success_response({"PeerRPCService","GetChainID"},gorpc::encode_id_response("seed-fixture"));
      auto bytes=std::make_shared<libp2p::Bytes>(raw.begin(),raw.end());
      p2p::detail::write_rpc_frame(*peer.io,stream,bytes,[stream](auto){});
    });
  });
  auto address=peer.endpoint.address;
  if(legacy_alias)
  {
    auto marker=address.find("/p2p/");require(marker!=std::string::npos,"fixture peer component");
    while(marker!=std::string::npos){address.replace(marker,5,"/ipfs/");marker=address.find("/p2p/",marker+6);}
    auto parsed=libp2p::multi::Multiaddress::create(address);
    require(parsed.has_value()&&parsed.value().getPeerId()&&*parsed.value().getPeerId()==peer.endpoint.id,
            "the pinned SDK accepts this legacy peer spelling");
  }
  auto config=empty;config.seed_peers={address,address};config.max_pending_dials=1;
  std::atomic<size_t> connections{0};p2p::Libp2pTransport transport(config);
  transport.on_peer_connected([&](const auto&){++connections;});transport.start();(void)peer.peer_after(0);
  require(transport.peer_get_chain_id(peer.endpoint)=="seed-fixture","seed uses the same protocol and remains usable");
  require(connections==1,"duplicate seed advice must notify one actual connection once");
  const auto seed_jobs=transport.dial_stats().peak_jobs;
  if(seed_jobs!=1)std::cerr<<"native seed admission peak="<<seed_jobs<<" alias="<<legacy_alias<<"\n";
  require(seed_jobs==1,"explicit startup seed must pass through native admission before implicit bootstrap can hide it");
  transport.stop();require(transport.dial_stats().jobs==0,"stop releases the final native seed owner");bool unavailable=false;
  try{transport.connect_peer(peer.endpoint);}catch(const p2p::LocalTransportUnavailable&){unavailable=true;}
  require(unavailable,"stopped dial must refuse locally before queue publication");
  for(auto invalid:std::vector<std::pair<size_t,size_t>>{{0,1024},{1,0}})
  {
    auto bad=empty;bad.max_pending_dials=invalid.first;bad.pending_dial_peer_capacity_bytes=invalid.second;
    bool rejected=false;try{p2p::Libp2pTransport invalid_transport(bad);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"native configured dial limits must be finite and positive");
  }
}

void peer_metadata_lifecycle()
{
  p2p::Libp2pTransport::Config empty;empty.listen_address.clear();empty.enable_dht=false;
  {p2p::Libp2pTransport warm(empty);warm.start();warm.stop();}
  FixturePeer peer;
  peer.serve([&](Stream stream){p2p::detail::read_rpc_request(*peer.io,stream,[&peer,stream](auto error,auto){
    if(error)return;auto raw=gorpc::encode_success_response({"PeerRPCService","GetChainID"},gorpc::encode_id_response("peer-metadata-fixture"));
    auto bytes=std::make_shared<libp2p::Bytes>(raw.begin(),raw.end());p2p::detail::write_rpc_frame(*peer.io,stream,bytes,[stream](auto){});
  });});
  std::array<uint8_t,34> bootstrap_wire{};bootstrap_wire[0]=0x12;bootstrap_wire[1]=0x20;bootstrap_wire[33]=77;
  const auto bootstrap_id=libp2p::peer::PeerId::fromBytes(bootstrap_wire).value().toBase58();
  auto config=empty;config.enable_dht=true;config.enable_dht_local=true;config.discovery_peers={"/ip4/127.0.0.1/tcp/9/p2p/"+bootstrap_id};
  config.seed_peers={peer.endpoint.address};config.max_known_peer_advice=4;config.known_peer_advice_capacity_bytes=4096;config.max_peer_snapshots=1;
  config.sdk_address_limits={4,8,8,4096,2};config.sdk_metadata_collection_interval_ms=10;
  p2p::Libp2pTransport transport(config);transport.start();(void)peer.peer_after(0);
  require(transport.peer_advice_stats().sdk_observations>0,"SDK bootstrap address signals must populate native advice");
  const auto collecting=std::chrono::steady_clock::now()+2s;
  while(!transport.sdk_address_stats().maintenance_runs)
  {require(std::chrono::steady_clock::now()<collecting,"native SDK collector must run on its owned timer");std::this_thread::sleep_for(1ms);}
  auto sdk=transport.sdk_address_stats();
  require(sdk.repositories>0&&sdk.peers>0&&sdk.addresses>0,"native host must use the accounted SDK address repository");
  require(!sdk.maintenance_failures&&!sdk.maintenance_schedule_failures,"native SDK maintenance must complete successfully");
  for(unsigned i=1;i<=24;++i)
  {
    std::array<uint8_t,34> wire{};wire[0]=0x12;wire[1]=0x20;wire[33]=i;const auto id=libp2p::peer::PeerId::fromBytes(wire).value().toBase58();
    transport.connect_peer({id,"/ip4/127.0.0.1/tcp/9/p2p/"+id});
    const auto until=std::chrono::steady_clock::now()+2s;
    while(transport.dial_stats().jobs){require(std::chrono::steady_clock::now()<until,"bounded native dial cleanup");std::this_thread::sleep_for(1ms);}
  }
  require(transport.peer_advice_stats().entries==4&&transport.peer_advice_stats().peer_capacity_bytes<=4096,"failed optional advice remains bounded");
  sdk=transport.sdk_address_stats();
  require(sdk.peak_peers<=4&&sdk.peak_addresses<=8&&sdk.peak_slots<=8&&sdk.peak_encoded_bytes<=4096,
          "actual SDK bootstrap retention must obey shared limits");
  auto held=transport.known_peers();bool found=false,bootstrap_found=false;for(const auto& row:held){found|=row.id==peer.endpoint.id;bootstrap_found|=row.id==bootstrap_id;}
  require(found&&bootstrap_found&&held.size()==4,"configured seed, DHT bootstrap and active connection survive optional churn");
  bool refused=false;try{(void)transport.connected_peers();}catch(const p2p::LocalTransportUnavailable& e){refused=e.reason==p2p::LocalTransportUnavailable::Reason::peer_snapshot_capacity;}
  require(refused&&transport.peer_snapshot_stats().jobs==1,"shared snapshot reservations follow live results");
  dial_test_alloc::fail_after=0;const auto connected=transport.connected_peer_count();const auto untouched=dial_test_alloc::fail_after;dial_test_alloc::fail_after=-1;
  require(connected==1&&untouched==0,"native connected count must allocate no projection while budget is busy");
  transport.stop();require(transport.peer_snapshot_stats().jobs==1&&held.size()==4,"stop must not return credits held by callers");
  const auto stopped_runs=transport.sdk_address_stats().maintenance_runs;
  std::this_thread::sleep_for(35ms);
  require(transport.sdk_address_stats().maintenance_runs==stopped_runs,"retired generation must not rearm SDK collection");
  transport.start();(void)peer.peer_after(1);refused=false;
  const auto restarted=std::chrono::steady_clock::now()+2s;
  while(transport.sdk_address_stats().maintenance_runs==stopped_runs)
  {require(std::chrono::steady_clock::now()<restarted,"restart must own a fresh SDK collection timer");std::this_thread::sleep_for(1ms);}
  try{(void)transport.known_peers();}catch(const p2p::LocalTransportUnavailable&){refused=true;}
  require(refused,"restart must not evade retained snapshot credits");
  require(transport.peer_get_chain_id(peer.endpoint)=="peer-metadata-fixture","retired snapshot saturation leaves new-generation protocol RPC usable");held=p2p::PeerSnapshot{};
  {auto current=transport.known_peers();require(!current.empty(),"fresh projection after old owner release");}
  transport.stop();
  for(unsigned field=0;field<5;++field)
  {
    auto invalid=empty;if(field==0)invalid.max_known_peer_advice=0;if(field==1)invalid.known_peer_advice_capacity_bytes=0;if(field==2)invalid.max_peer_snapshots=0;if(field==3)invalid.peer_snapshot_entries=0;if(field==4)invalid.peer_snapshot_capacity_bytes=0;
    bool bad=false;try{p2p::Libp2pTransport rejected(invalid);}catch(const std::invalid_argument&){bad=true;}require(bad,"native peer metadata limits must be positive");
  }
  for(unsigned field=0;field<7;++field)
  {
    auto invalid=empty;auto& limits=invalid.sdk_address_limits;
    if(field==0)limits.peers=0;if(field==1)limits.addresses=0;if(field==2)limits.slots=0;
    if(field==3)limits.encoded_bytes=0;if(field==4)limits.addresses_per_peer=0;
    if(field==5)limits.addresses_per_peer=65;if(field==6)invalid.sdk_metadata_collection_interval_ms=0;
    bool bad=false;try{p2p::Libp2pTransport rejected(invalid);}catch(const std::invalid_argument&){bad=true;}
    require(bad,"native SDK address limits and collection interval must be valid before startup");
  }
}

void sdk_bootstrap_capacity()
{
  FixturePeer peer;
  peer.serve([&](Stream stream){p2p::detail::read_rpc_request(*peer.io,stream,[&peer,stream](auto error,auto){
    if(error)return;auto raw=gorpc::encode_success_response({"PeerRPCService","GetChainID"},gorpc::encode_id_response("sdk-capacity-fixture"));
    auto bytes=std::make_shared<libp2p::Bytes>(raw.begin(),raw.end());p2p::detail::write_rpc_frame(*peer.io,stream,bytes,[stream](auto){});
  });});
  p2p::Libp2pTransport::Config config;config.listen_address.clear();config.enable_dht_local=true;
  config.seed_peers={peer.endpoint.address};config.sdk_address_limits={2,2,2,4096,1};
  config.sdk_metadata_collection_interval_ms=10;
  for(unsigned n=1;n<=8;++n)
  {
    std::array<uint8_t,34> wire{};wire[0]=0x12;wire[1]=0x20;wire[33]=100+n;
    config.discovery_peers.push_back("/ip4/127.0.0.1/tcp/9/p2p/"+libp2p::peer::PeerId::fromBytes(wire).value().toBase58());
  }
  p2p::Libp2pTransport transport(config);transport.start();(void)peer.peer_after(0);
  auto stats=transport.sdk_address_stats();
  require(stats.repositories>0&&stats.capacity_bypasses>0,"actual DHT bootstrap must exercise the injected address capacity policy");
  require(stats.peak_peers<=2&&stats.peak_addresses<=2&&stats.peak_slots<=2&&stats.peak_encoded_bytes<=4096,
          "configured bootstrap saturation must preserve every SDK address limit");
  require(transport.peer_get_chain_id(peer.endpoint)=="sdk-capacity-fixture","SDK optional-address saturation must preserve real protocol RPC");
  auto known=transport.known_peers();
  for(const auto& address:config.discovery_peers)
  {
    auto id=*libp2p::multi::Multiaddress::create(address).value().getPeerId();bool found=false;
    for(const auto& row:known)found|=row.id==id;
    require(found,"SDK retention refusal must preserve separately owned configured discovery routes");
  }
  transport.stop();
}

void peer_snapshot_stop_concurrency()
{
  p2p::Libp2pTransport::Config config;config.listen_address.clear();config.enable_dht=false;p2p::Libp2pTransport transport(config);transport.start();
  std::atomic<bool> reading{true};std::atomic<unsigned> completed{0};std::thread reader([&]{while(reading){try{auto known=transport.known_peers();auto connected=transport.connected_peers();(void)transport.connected_peer_count();(void)transport.sdk_address_stats();++completed;}catch(const p2p::LocalTransportUnavailable&) {}}});
  struct Join{std::atomic<bool>& reading;std::thread& reader;~Join(){reading=false;if(reader.joinable())reader.join();}} join{reading,reader};
  const auto deadline=std::chrono::steady_clock::now()+2s;while(completed<50){require(std::chrono::steady_clock::now()<deadline,"snapshot reader entered");std::this_thread::yield();}
  for(unsigned i=0;i<8;++i){transport.stop();transport.start();}reading=false;reader.join();transport.stop();require(completed>=50,"snapshot queries survive serialized lifecycle changes");
}

void loopback_ingress()
{
  p2p::Libp2pTransport::Config config;
  config.listen_address.clear();config.enable_dht = false;
  p2p::Libp2pTransport transport( config );
  std::atomic< size_t > calls{ 0 }, connections{0};
  transport.on_peer_connected([&](const auto&){ ++connections; });
  const auto args = gorpc::encode_empty_request();
  const auto payload = gorpc::encode_blocks_response( { std::string( 256 * 1024, 'b' ) } );
  transport.on_peer_rpc_request( [&]( const auto& service, const auto& method, const auto& input ) {
    require( service == "PeerRPCService" && method == "GetBlocks" && input == args, "handler receives exact request" );
    ++calls;return payload;
  } );
  struct StopBeforeCapturedStateDies
  {
    p2p::Libp2pTransport& transport;
    ~StopBeforeCapturedStateDies() { transport.stop(); }
  } stop_before_captured_state_dies{ transport };
  transport.start();
  FixturePeer peer;
  transport.connect_peer( peer.endpoint );auto remote = peer.peer_after( 0 );
  auto stream = peer.open( remote );
  const auto request = gorpc::encode_request( "PeerRPCService", "GetBlocks", args );
  const auto expected = gorpc::encode_success_response( { "PeerRPCService", "GetBlocks" }, payload );
  auto response = peer.read( stream, expected.size() );
  peer.write( stream, request.substr( 0, 1 ) );
  require( response.wait_for( 100ms ) != std::future_status::ready && calls == 0,
           "partial real request must not error or dispatch before completion" );
  peer.write( stream, request.substr( 1 ) );
  auto result = awaited( response );
  require( result.has_value() && std::string( result.value().begin(), result.value().end() ) == expected && calls == 1,
           "real transport must send the complete unchanged large response once" );
  require(connections==1,"one actual connection must notify once across host event and dial completion");
  stream.reset();

  auto partial = peer.open( remote );auto closed = peer.read( partial, 1 );
  peer.write( partial, request.substr( 0, 1 ) );
  require( closed.wait_for( 50ms ) != std::future_status::ready, "partial request must remain pending" );
  transport.stop();
  peer.require_disconnected_after( 0 );
  auto cancelled = awaited( closed );
  require( !cancelled.has_value() && calls == 1, "stop must reset a pending read without dispatching work" );
  partial.reset();

  transport.start();transport.connect_peer( peer.endpoint );remote = peer.peer_after( 1 );
  auto fresh = peer.open( remote );auto fresh_response = peer.read( fresh, expected.size() );
  peer.write( fresh, request );auto after = awaited( fresh_response );
  require( after.has_value() && after.value().size() == expected.size() && calls == 2,
           "fresh generation must work without replaying an old request" );
  require(connections==2,"fresh connection must notify once without retired-generation duplicates");
  fresh.reset();transport.stop();peer.require_disconnected_after( 1 );transport.stop();
}

void outbound_response_deadline()
{
  // Production start initializes the pinned SDK's logging before the second injector.
  p2p::Libp2pTransport::Config config;config.listen_address.clear();config.enable_dht = false;
  p2p::Libp2pTransport transport( config );transport.start();
  FixturePeer peer;
  auto received = std::make_shared< std::promise< Stream > >();auto request = received->get_future();
  peer.serve( [&, received]( Stream stream ) {
    p2p::detail::read_rpc_request( *peer.io, stream, [stream, received]( auto error, auto decoded ) {
      if( error ) { received->set_exception( error );return; }
      require( decoded.service.method == "GetChainID", "fixture receives the expected request" );
      received->set_value( stream );
    } );
  } );
  auto client = std::async( std::launch::async, [&] {
    try { return std::pair( true, transport.peer_get_chain_id( peer.endpoint ) ); }
    catch( const std::exception& e ) { return std::pair( false, std::string( e.what() ) ); }
  } );
  auto stream = awaited( request );
  const auto raw = gorpc::encode_success_response(
    { "PeerRPCService", "GetChainID" }, gorpc::encode_id_response( "controlled-chain" ) );
  peer.write( stream, raw.substr( 0, 1 ) );
  std::mutex mutex;std::condition_variable changed;bool stopping = false;
  auto feeder = std::async( std::launch::async, [&] {
    for( size_t fragment = 1;fragment <= 2;++fragment )
    {
      std::unique_lock lock( mutex );
      if( changed.wait_for( lock, 4s, [&] { return stopping; } ) ) return;
      lock.unlock();
      try { peer.write( stream, fragment == 1 ? raw.substr( 1, 1 ) : raw.substr( 2 ) ); }
      catch( ... ) { return; } // Expected if the client's complete-body deadline closes the stream.
    }
  } );
  const bool ready = client.wait_for( 10s ) == std::future_status::ready;
  {
    std::lock_guard lock( mutex );stopping = true;
  }
  changed.notify_all();feeder.get();transport.stop();
  require( ready, "complete Peer RPC response must have a finite deadline" );
  const auto result = client.get();
  require( !result.first && result.second.find( "Timed out" ) != std::string::npos,
           "partial response fragments must not renew the complete-body deadline" );
}

void outbound_cancel_and_restart()
{
  p2p::Libp2pTransport::Config config;config.listen_address.clear();config.enable_dht = false;
  p2p::Libp2pTransport transport( config );
  bool unavailable = false;
  try { (void)transport.peer_get_chain_id( {} ); }
  catch( const p2p::LocalTransportUnavailable& ) { unavailable = true; }
  require( unavailable, "outbound call before start must fail locally without posting stale work" );
  std::atomic< bool > pause{ false };
  std::atomic< size_t > calls{ 0 };
  auto pending = std::make_shared< std::promise< Stream > >();auto waiting = pending->get_future();
  std::string value( 256 * 1024, 'r' );value[1234] = '\0';
  const auto response = gorpc::encode_success_response(
    { "PeerRPCService", "GetChainID" }, gorpc::encode_id_response( value ) );
  transport.start();FixturePeer peer;
  struct Stop { p2p::Libp2pTransport& transport;~Stop() { transport.stop(); } } guard{ transport };
  peer.serve( [&]( Stream stream ) {
    p2p::detail::read_rpc_request( *peer.io, stream,
      [&, stream]( auto error, auto decoded ) {
        if( error ) return;
        require( decoded.service.name == "PeerRPCService" && decoded.service.method == "GetChainID",
                 "outbound request preserves its service/method" );
        ++calls;
        if( pause.load() ) { pending->set_value( stream );return; }
        auto bytes = std::make_shared< libp2p::Bytes >( response.begin(), response.end() );
        p2p::detail::write_rpc_frame( *peer.io, stream, bytes, [stream]( auto result ) {
          if( result.has_value() ) stream->close( []( auto ) {} );
          else stream->reset();
        } );
      } );
  } );
  require( transport.peer_get_chain_id( peer.endpoint ) == value, "real outgoing response retains complete binary payload" );
  pause = true;
  auto client = std::async( std::launch::async, [&] { return transport.peer_get_chain_id( peer.endpoint ); } );
  auto stream = awaited( waiting );auto reset = peer.read( stream, 1 );
  transport.stop();
  require( client.wait_for( 500ms ) == std::future_status::ready, "stop must wake an outgoing caller immediately" );
  unavailable = false;
  try { (void)client.get(); } catch( const p2p::LocalTransportUnavailable& ) { unavailable = true; }
  require( unavailable, "transport stop must remain a local cancellation" );
  require( !awaited( reset ).has_value(), "stop must reset the pending outgoing stream" );
  peer.require_disconnected_after( 0 );stream.reset();
  unavailable = false;
  try { (void)transport.peer_get_chain_id( peer.endpoint ); }
  catch( const p2p::LocalTransportUnavailable& ) { unavailable = true; }
  require( unavailable, "stopped transport refuses calls without waiting for a future restart" );
  pause = false;transport.start();
  require( transport.peer_get_chain_id( peer.endpoint ) == value && calls == 3,
           "fresh outgoing generation succeeds without replaying the cancelled request" );
  transport.stop();peer.require_disconnected_after( 1 );
}
} // namespace

int main( int argc, char** argv )
{
  try
  {
    if( argc > 1 && std::string(argv[1]) == "dial-measure" ) { dial_queue_measurement();return 0; }
    if( argc > 1 && std::string(argv[1]) == "peer-metadata" ) { peer_metadata_lifecycle();sdk_bootstrap_capacity();peer_snapshot_stop_concurrency();return 0; }
    if( argc > 1 && std::string(argv[1]) == "seed-alias" ) { seed_dial_admission(true);return 0; }
    if( argc > 1 && std::string(argv[1]) == "dial-post-faults" ) { dial_publication_failure_recovery();return 0; }
    if( argc > 1 && std::string(argv[1]) == "dial-faults" ) { dial_failure_recovery();return 0; }
    peer_metadata_lifecycle();
    sdk_bootstrap_capacity();
    peer_snapshot_stop_concurrency();
    dial_failure_recovery();
    dial_publication_failure_recovery();
    seed_dial_admission();
    seed_dial_admission(true);
    if( argc == 1 ) loopback_ingress();
    outbound_response_deadline();
    outbound_cancel_and_restart();
    std::cout << "Real Peer RPC fragmentation, complete response, cancellation, restart and response deadline passed\n";
    return 0;
  }
  catch( const std::exception& error ) { std::cerr << error.what() << '\n';return 1; }
}
