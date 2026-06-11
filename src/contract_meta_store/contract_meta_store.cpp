#include "contract_meta_store.hpp"

#include <koinos/log.hpp>
#include <koinos/protocol/protocol.pb.h>

#include <stdexcept>

namespace koinos::node::contract_meta_store {

ContractMetaStore::ContractMetaStore( rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cf )
    : _db( db ), _cf( cf )
{
}

rpc::contract_meta_store::get_contract_meta_response
ContractMetaStore::get_contract_meta( const rpc::contract_meta_store::get_contract_meta_request& req )
{
  std::shared_lock lock( _mutex );

  rpc::contract_meta_store::get_contract_meta_response resp;

  if( req.contract_id().empty() )
    throw std::runtime_error( "expected field contract_id was nil" );

  std::string value;
  auto s = _db->Get( rocksdb::ReadOptions(), _cf, req.contract_id(), &value );
  if( s.ok() && !value.empty() )
  {
    ::koinos::contract_meta_store::contract_meta_item item;
    if( item.ParseFromString( value ) )
      *resp.mutable_meta() = item;
  }

  return resp;
}

void ContractMetaStore::handle_block_accepted( const broadcast::block_accepted& ba )
{
  if( !ba.has_block() )
    return;

  const auto& block = ba.block();

  for( const auto& tx: block.transactions() )
  {
    for( const auto& op: tx.operations() )
    {
      if( op.has_upload_contract() )
      {
        const auto& upload = op.upload_contract();
        if( upload.contract_id().empty() )
          continue;

        ::koinos::contract_meta_store::contract_meta_item item;
        item.set_abi( upload.abi() );

        std::string value;
        if( !item.SerializeToString( &value ) )
          continue;

        std::unique_lock lock( _mutex );
        auto s = _db->Put( rocksdb::WriteOptions(), _cf, upload.contract_id(), value );
        if( !s.ok() )
        {
          LOG( warning ) << "[contract_meta_store] Put failed: " << s.ToString();
        }
      }
    }
  }
}

} // namespace koinos::node::contract_meta_store
