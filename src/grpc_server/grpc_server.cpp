#include "grpc_server.hpp"
#include "mempool_output_state.hpp"
#include "core/transaction_work_budget.hpp"

#include <koinos/log.hpp>

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/support/server_interceptor.h>

#include <koinos/rpc/p2p/p2p_rpc.pb.h>
#include <koinos/rpc/services.grpc.pb.h>

#include <algorithm>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace koinos::node::grpc_server {

namespace {

using detail::MempoolOutputState;

class MempoolOutputRegistry
{
public:
  void add( void* context, const std::shared_ptr< MempoolOutputState >& state )
  {
    std::lock_guard lock( _mutex );
    _states[ context ] = state;
  }

  void remove( void* context, const MempoolOutputState* state ) noexcept
  {
    std::lock_guard lock( _mutex );
    const auto it = _states.find( context );
    if( it != _states.end() && it->second.lock().get() == state ) _states.erase( it );
  }

  std::shared_ptr< MempoolOutputState > find( void* context )
  {
    std::lock_guard lock( _mutex );
    const auto it = _states.find( context );
    return it == _states.end() ? nullptr : it->second.lock();
  }

private:
  std::mutex _mutex;
  std::unordered_map< void*, std::weak_ptr< MempoolOutputState > > _states;
};

class MempoolOutputInterceptor final : public grpc::experimental::Interceptor
{
public:
  MempoolOutputInterceptor( std::shared_ptr< MempoolOutputRegistry > registry,
                            void* context,
                            std::shared_ptr< MempoolOutputState > state )
    : _registry( std::move( registry ) ), _context( context ), _state( std::move( state ) )
  {}

  ~MempoolOutputInterceptor() override
  {
    _registry->remove( _context, _state.get() );
  }

  void Intercept( grpc::experimental::InterceptorBatchMethods* methods ) override
  {
    if( methods->QueryInterceptionHookPoint(
          grpc::experimental::InterceptionHookPoints::PRE_SEND_MESSAGE ) )
      _state->serialize_message( methods );
    if( methods->QueryInterceptionHookPoint(
          grpc::experimental::InterceptionHookPoints::POST_SEND_MESSAGE ) )
      _state->release_message();
    methods->Proceed();
  }

private:
  std::shared_ptr< MempoolOutputRegistry > _registry;
  void* _context;
  std::shared_ptr< MempoolOutputState > _state;
};

class MempoolOutputInterceptorFactory final
  : public grpc::experimental::ServerInterceptorFactoryInterface
{
public:
  MempoolOutputInterceptorFactory( IMempool* mempool,
                                   std::shared_ptr< MempoolOutputRegistry > registry )
    : _mempool( mempool ), _registry( std::move( registry ) )
  {}

  grpc::experimental::Interceptor* CreateServerInterceptor(
    grpc::experimental::ServerRpcInfo* info ) override
  {
    if( !_mempool ) return nullptr;
    const auto budget = _mempool->memory_budget();
    if( !budget.enabled() ) return nullptr;
    const std::string_view method = info->method() ? info->method() : "";
    if( method != "/koinos.services.koinos/submit_transaction" &&
        method != "/koinos.services.koinos/get_pending_transactions" &&
        method != "/koinos.services.koinos/check_pending_account_resources" ) return nullptr;
    auto* context = info->server_context();
    std::shared_ptr< MempoolOutputState > state;
    const char* refusal = nullptr;
    try
    {
      OwnedMemoryBudget::Scope admission( budget, false );
      state = std::make_shared< MempoolOutputState >( budget );
      _registry->add( context, state );
      return new MempoolOutputInterceptor( _registry, context, std::move( state ) );
    }
    catch( const LocalCapacityExceeded& )
    {
      refusal = "Local M1 RPC control memory limit reached";
    }
    catch( const std::bad_alloc& )
    {
      refusal = "Local M1 RPC control allocation failed";
    }
    // Failure at any setup allocation must release its state/registry owner
    // before publishing the bounded refusal. Never execute native work merely
    // because the SDK cannot propagate a quota exception from this callback.
    _registry->remove( context, state.get() );
    state.reset();
    try
    {
      OwnedMemoryBudget::Scope refusal_control( budget, OwnedMemoryBudget::WorkClass::refusal );
      state = std::make_shared< MempoolOutputState >( budget, refusal );
      _registry->add( context, state );
      return new MempoolOutputInterceptor( _registry, context, std::move( state ) );
    }
    catch( const LocalCapacityExceeded& ) {}
    catch( const std::bad_alloc& ) {}
    // The bounded refusal allowance can also be full. Cancel this call with
    // no published native state; its handler must return before native work.
    // The explicit counter exposes this second resource boundary. SDK call
    // cancellation/queue storage remains part of the separate R1 consumer.
    _registry->remove( context, state.get() );
    state.reset();
    budget.record_refusal_transport_cancellation();
    context->TryCancel();
    return nullptr;
  }

private:
  IMempool* _mempool;
  std::shared_ptr< MempoolOutputRegistry > _registry;
};

grpc::Status service_unavailable( const std::string& service )
{
  return grpc::Status( grpc::StatusCode::UNAVAILABLE, service + " service is not enabled" );
}

grpc::Status unimplemented( const std::string& method )
{
  return grpc::Status( grpc::StatusCode::UNIMPLEMENTED, method + " is not implemented by the monolith gRPC server" );
}

grpc::StatusCode status_code_for_exception( const std::exception& e )
{
  const std::string_view message = e.what();
  if( message.rfind( "expected field ", 0 ) == 0 )
    return grpc::StatusCode::INVALID_ARGUMENT;
  return grpc::StatusCode::INTERNAL;
}

template< typename Handler >
grpc::Status invoke_service( const std::string& method, Handler&& handler,
                             MempoolOutputState* output = nullptr )
{
  try
  {
    handler();
    return grpc::Status::OK;
  }
  catch( const LocalCapacityExceeded& e )
  {
    return output ? output->prepare_status( grpc::StatusCode::RESOURCE_EXHAUSTED, e.what() )
                  : grpc::Status( grpc::StatusCode::RESOURCE_EXHAUSTED, e.what() );
  }
  catch( const std::exception& e )
  {
    LOG( warning ) << "[grpc] Error handling " << method << ": " << e.what();
    return output ? output->prepare_status( status_code_for_exception( e ), e.what() )
                  : grpc::Status( status_code_for_exception( e ), e.what() );
  }
  catch( ... )
  {
    LOG( warning ) << "[grpc] Unknown error handling " << method;
    return grpc::Status( grpc::StatusCode::INTERNAL, "unknown gRPC service error" );
  }
}

class KoinosService final : public services::koinos::Service
{
public:
  KoinosService( IChain* chain,
                 IMempool* mempool,
                 IBlockStore* block_store,
                 contract_meta_store::ContractMetaStore* contract_meta,
                 transaction_store::TransactionStore* tx_store,
                 account_history::AccountHistory* acct_history,
                 const std::atomic< bool >* gossip_status,
                 std::shared_ptr< MempoolOutputRegistry > output_registry )
      : _chain( chain ),
        _mempool( mempool ),
        _block_store( block_store ),
        _contract_meta( contract_meta ),
        _tx_store( tx_store ),
        _acct_history( acct_history ),
        _gossip_status( gossip_status ),
        _output_registry( std::move( output_registry ) )
  {
  }

  grpc::Status get_account_history( grpc::ServerContext*,
                                    const rpc::account_history::get_account_history_request* request,
                                    rpc::account_history::get_account_history_response* response ) override
  {
    if( !_acct_history )
      return service_unavailable( "account_history" );
    return invoke_service( "get_account_history", [&]() { *response = _acct_history->get_account_history( *request ); } );
  }

  grpc::Status get_blocks_by_id( grpc::ServerContext*,
                                 const rpc::block_store::get_blocks_by_id_request* request,
                                 rpc::block_store::get_blocks_by_id_response* response ) override
  {
    if( !_block_store )
      return service_unavailable( "block_store" );
    return invoke_service( "get_blocks_by_id", [&]() { *response = _block_store->get_blocks_by_id( *request ); } );
  }

  grpc::Status get_blocks_by_height( grpc::ServerContext*,
                                     const rpc::block_store::get_blocks_by_height_request* request,
                                     rpc::block_store::get_blocks_by_height_response* response ) override
  {
    if( !_block_store )
      return service_unavailable( "block_store" );
    return invoke_service( "get_blocks_by_height", [&]() { *response = _block_store->get_blocks_by_height( *request ); } );
  }

  grpc::Status get_highest_block( grpc::ServerContext*,
                                  const rpc::block_store::get_highest_block_request* request,
                                  rpc::block_store::get_highest_block_response* response ) override
  {
    if( !_block_store )
      return service_unavailable( "block_store" );
    return invoke_service( "get_highest_block", [&]() { *response = _block_store->get_highest_block( *request ); } );
  }

  grpc::Status submit_block( grpc::ServerContext*,
                             const rpc::chain::submit_block_request* request,
                             rpc::chain::submit_block_response* response ) override
  {
    if( !_chain )
      return service_unavailable( "chain" );
    return invoke_service( "submit_block", [&]() { *response = _chain->submit_block( *request ); } );
  }

  grpc::Status submit_transaction( grpc::ServerContext* context,
                                   const rpc::chain::submit_transaction_request* request,
                                   rpc::chain::submit_transaction_response* response ) override
  {
    if( !_chain )
      return service_unavailable( "chain" );
    const auto budget = _mempool ? _mempool->memory_budget() : OwnedMemoryBudget{};
    if( !budget.enabled() )
      return invoke_service( "submit_transaction", [&]() { *response = _chain->submit_transaction( *request ); } );
    auto output = _output_registry->find( static_cast< grpc_impl::ServerContextBase* >( context ) );
    if( !output ) return context->IsCancelled() ? grpc::Status( grpc::StatusCode::CANCELLED, {} )
      : grpc::Status( grpc::StatusCode::INTERNAL, "missing gRPC output ownership state" );
    if( !output->admitted() ) return output->control_refusal_status();
    return invoke_service( "submit_transaction", [&]() {
      OwnedMemoryBudget::Scope scope( budget, false );
      *response = _chain->submit_transaction_with_prepared_response( *request,
        { output.get(), []( void* state, const rpc::chain::submit_transaction_response& native ) {
            static_cast< MempoolOutputState* >( state )->prepare_message( native );
          } } );
    }, output.get() );
  }

  grpc::Status get_head_info( grpc::ServerContext*,
                              const rpc::chain::get_head_info_request* request,
                              rpc::chain::get_head_info_response* response ) override
  {
    if( !_chain )
      return service_unavailable( "chain" );
    return invoke_service( "get_head_info", [&]() { *response = _chain->get_head_info( *request ); } );
  }

  grpc::Status get_chain_id( grpc::ServerContext*,
                             const rpc::chain::get_chain_id_request* request,
                             rpc::chain::get_chain_id_response* response ) override
  {
    if( !_chain )
      return service_unavailable( "chain" );
    return invoke_service( "get_chain_id", [&]() { *response = _chain->get_chain_id( *request ); } );
  }

  grpc::Status get_fork_heads( grpc::ServerContext*,
                               const rpc::chain::get_fork_heads_request* request,
                               rpc::chain::get_fork_heads_response* response ) override
  {
    if( !_chain )
      return service_unavailable( "chain" );
    return invoke_service( "get_fork_heads", [&]() { *response = _chain->get_fork_heads( *request ); } );
  }

  grpc::Status read_contract( grpc::ServerContext*,
                              const rpc::chain::read_contract_request* request,
                              rpc::chain::read_contract_response* response ) override
  {
    if( !_chain )
      return service_unavailable( "chain" );
    return invoke_service( "read_contract", [&]() { *response = _chain->read_contract( *request ); } );
  }

  grpc::Status get_account_nonce( grpc::ServerContext*,
                                  const rpc::chain::get_account_nonce_request* request,
                                  rpc::chain::get_account_nonce_response* response ) override
  {
    if( !_chain )
      return service_unavailable( "chain" );
    return invoke_service( "get_account_nonce", [&]() { *response = _chain->get_account_nonce( *request ); } );
  }

  grpc::Status get_account_rc( grpc::ServerContext*,
                               const rpc::chain::get_account_rc_request* request,
                               rpc::chain::get_account_rc_response* response ) override
  {
    if( !_chain )
      return service_unavailable( "chain" );
    return invoke_service( "get_account_rc", [&]() { *response = _chain->get_account_rc( *request ); } );
  }

  grpc::Status get_resource_limits( grpc::ServerContext*,
                                    const rpc::chain::get_resource_limits_request* request,
                                    rpc::chain::get_resource_limits_response* response ) override
  {
    if( !_chain )
      return service_unavailable( "chain" );
    return invoke_service( "get_resource_limits", [&]() { *response = _chain->get_resource_limits( *request ); } );
  }

  grpc::Status invoke_system_call( grpc::ServerContext*,
                                   const rpc::chain::invoke_system_call_request* request,
                                   rpc::chain::invoke_system_call_response* response ) override
  {
    if( !_chain )
      return service_unavailable( "chain" );
    return invoke_service( "invoke_system_call", [&]() { *response = _chain->invoke_system_call( *request ); } );
  }

  grpc::Status get_contract_meta( grpc::ServerContext*,
                                  const rpc::contract_meta_store::get_contract_meta_request* request,
                                  rpc::contract_meta_store::get_contract_meta_response* response ) override
  {
    if( !_contract_meta )
      return service_unavailable( "contract_meta_store" );
    return invoke_service( "get_contract_meta", [&]() { *response = _contract_meta->get_contract_meta( *request ); } );
  }

  grpc::Status get_pending_transactions( grpc::ServerContext* context,
                                         const rpc::mempool::get_pending_transactions_request* request,
                                         rpc::mempool::get_pending_transactions_response* response ) override
  {
    if( !_mempool )
      return service_unavailable( "mempool" );
    const auto budget = _mempool->memory_budget();
    if( !budget.enabled() )
      return invoke_service( "get_pending_transactions", [&]() { *response = _mempool->get_pending_transactions( *request ); } );
    auto output = _output_registry->find( static_cast< grpc_impl::ServerContextBase* >( context ) );
    if( !output ) return context->IsCancelled() ? grpc::Status( grpc::StatusCode::CANCELLED, {} )
      : grpc::Status( grpc::StatusCode::INTERNAL, "missing gRPC output ownership state" );
    if( !output->admitted() ) return output->control_refusal_status();
    return invoke_service( "get_pending_transactions", [&]() {
      OwnedMemoryBudget::Scope scope( budget, false );
      *response = _mempool->get_pending_transactions( *request );
      output->prepare_message( *response );
    }, output.get() );
  }

  grpc::Status check_pending_account_resources(
    grpc::ServerContext* context,
    const rpc::mempool::check_pending_account_resources_request* request,
    rpc::mempool::check_pending_account_resources_response* response ) override
  {
    if( !_mempool )
      return service_unavailable( "mempool" );
    const auto budget = _mempool->memory_budget();
    if( !budget.enabled() )
      return invoke_service( "check_pending_account_resources",
                             [&]() { *response = _mempool->check_pending_account_resources( *request ); } );
    auto output = _output_registry->find( static_cast< grpc_impl::ServerContextBase* >( context ) );
    if( !output ) return context->IsCancelled() ? grpc::Status( grpc::StatusCode::CANCELLED, {} )
      : grpc::Status( grpc::StatusCode::INTERNAL, "missing gRPC output ownership state" );
    if( !output->admitted() ) return output->control_refusal_status();
    return invoke_service( "check_pending_account_resources", [&]() {
      OwnedMemoryBudget::Scope scope( budget, false );
      *response = _mempool->check_pending_account_resources( *request );
      output->prepare_message( *response );
    }, output.get() );
  }

  grpc::Status get_gossip_status( grpc::ServerContext*,
                                  const rpc::p2p::get_gossip_status_request*,
                                  rpc::p2p::get_gossip_status_response* response ) override
  {
    response->set_enabled( _gossip_status && _gossip_status->load() );
    return grpc::Status::OK;
  }

  grpc::Status get_transactions_by_id( grpc::ServerContext*,
                                       const rpc::transaction_store::get_transactions_by_id_request* request,
                                       rpc::transaction_store::get_transactions_by_id_response* response ) override
  {
    if( !_tx_store )
      return service_unavailable( "transaction_store" );
    return invoke_service( "get_transactions_by_id", [&]() { *response = _tx_store->get_transactions_by_id( *request ); } );
  }

private:
  IChain* _chain;
  IMempool* _mempool;
  IBlockStore* _block_store;
  contract_meta_store::ContractMetaStore* _contract_meta;
  transaction_store::TransactionStore* _tx_store;
  account_history::AccountHistory* _acct_history;
  const std::atomic< bool >* _gossip_status;
  std::shared_ptr< MempoolOutputRegistry > _output_registry;
};

} // anonymous namespace

struct GRPCServer::Impl
{
  std::unique_ptr< grpc::Server > server;
  std::unique_ptr< KoinosService > service;
  std::shared_ptr< MempoolOutputRegistry > output_registry;
  int bound_port = 0;
};

GRPCServer::GRPCServer( IChain* chain,
                        IMempool* mempool,
                        IBlockStore* block_store,
                        contract_meta_store::ContractMetaStore* contract_meta,
                        transaction_store::TransactionStore* tx_store,
                        account_history::AccountHistory* acct_history,
                        const std::string& listen_address,
                        unsigned int threads,
                        const std::atomic< bool >* gossip_status )
    : _chain( chain ),
      _mempool( mempool ),
      _block_store( block_store ),
      _contract_meta( contract_meta ),
      _tx_store( tx_store ),
      _acct_history( acct_history ),
      _gossip_status( gossip_status ),
      _listen_address( listen_address ),
      _thread_count( std::max( threads, 1u ) )
{
}

GRPCServer::~GRPCServer()
{
  stop();
}

void GRPCServer::start()
{
  if( _running.exchange( true ) )
    return;

  _impl          = std::make_unique< Impl >();
  const auto memory = _mempool ? _mempool->memory_budget() : OwnedMemoryBudget{};
  {
    // Fixed server control owners must survive a full mempool on restart.
    // Their physical allocations remain charged until the final owner dies.
    OwnedMemoryBudget::Scope startup_control( memory, true );
    _impl->output_registry = std::make_shared< MempoolOutputRegistry >();
  }
  _impl->service = std::make_unique< KoinosService >(
    _chain, _mempool, _block_store, _contract_meta, _tx_store, _acct_history, _gossip_status,
    _impl->output_registry );

  grpc::EnableDefaultHealthCheckService( true );

  grpc::ServerBuilder builder;
  std::vector< std::unique_ptr< grpc::experimental::ServerInterceptorFactoryInterface > > interceptors;
  {
    OwnedMemoryBudget::Scope startup_control( memory, true );
    interceptors.emplace_back(
      std::make_unique< MempoolOutputInterceptorFactory >( _mempool, _impl->output_registry ) );
  }
  builder.experimental().SetInterceptorCreators( std::move( interceptors ) );
  builder.SetSyncServerOption( grpc::ServerBuilder::SyncServerOption::NUM_CQS, static_cast< int >( _thread_count ) );
  builder.SetSyncServerOption( grpc::ServerBuilder::SyncServerOption::MIN_POLLERS, static_cast< int >( _thread_count ) );
  builder.SetSyncServerOption( grpc::ServerBuilder::SyncServerOption::MAX_POLLERS, static_cast< int >( _thread_count ) );
  builder.AddListeningPort(
    _listen_address, grpc::InsecureServerCredentials(), &_impl->bound_port );
  builder.RegisterService( _impl->service.get() );

  _impl->server = builder.BuildAndStart();

  if( _impl->server )
  {
    LOG( info ) << "[grpc] Listening on " << _listen_address << " (bound port " << _impl->bound_port
                << ") with koinos.services.koinos";
  }
  else
  {
    _running = false;
    LOG( error ) << "[grpc] Failed to start on " << _listen_address;
  }
}

void GRPCServer::stop()
{
  if( !_running.exchange( false ) )
    return;

  if( _impl && _impl->server )
    _impl->server->Shutdown();

  _impl.reset();
}

int GRPCServer::bound_port() const
{
  return _impl ? _impl->bound_port : 0;
}

} // namespace koinos::node::grpc_server
