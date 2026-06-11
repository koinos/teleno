#include "account_history.hpp"

#include <algorithm>
#include <set>

#include <koinos/log.hpp>

namespace koinos::node::account_history {

AccountHistory::AccountHistory( rocksdb::DB* db, rocksdb::ColumnFamilyHandle* cf )
    : _db( db ), _cf( cf )
{
}

// ---------------------------------------------------------------------------
// RPC
// ---------------------------------------------------------------------------

rpc::account_history::get_account_history_response
AccountHistory::get_account_history( const rpc::account_history::get_account_history_request& req )
{
  std::shared_lock lock( _mutex );

  rpc::account_history::get_account_history_response resp;

  if( req.address().empty() )
    return resp;

  uint32_t limit = req.limit() > 0
    ? std::min( static_cast< uint32_t >( req.limit() ), max_records_per_query )
    : max_records_per_query;
  uint64_t seq_start = req.seq_num(); // 0 means from the beginning

  // Scan keys: address + seq (big-endian) for lexicographic order
  std::string prefix = req.address();
  std::string start_key = prefix + encode_seq( seq_start );

  rocksdb::ReadOptions read_opts;
  std::unique_ptr< rocksdb::Iterator > it( _db->NewIterator( read_opts, _cf ) );

  if( req.ascending() || seq_start == 0 )
  {
    // Forward scan
    it->Seek( start_key );
    uint32_t count = 0;
    while( it->Valid() && count < limit )
    {
      auto key = it->key().ToString();
      if( key.size() <= prefix.size() || key.substr( 0, prefix.size() ) != prefix )
        break;

      rpc::account_history::account_history_entry record;
      if( record.ParseFromString( it->value().ToString() ) )
        *resp.add_values() = record;

      ++count;
      it->Next();
    }
  }
  else
  {
    // Reverse scan (descending from seq_start)
    it->SeekForPrev( start_key );
    uint32_t count = 0;
    while( it->Valid() && count < limit )
    {
      auto key = it->key().ToString();
      if( key.size() <= prefix.size() || key.substr( 0, prefix.size() ) != prefix )
        break;

      rpc::account_history::account_history_entry record;
      if( record.ParseFromString( it->value().ToString() ) )
        *resp.add_values() = record;

      ++count;
      it->Prev();
    }
  }

  return resp;
}

// ---------------------------------------------------------------------------
// EventBus handler
// ---------------------------------------------------------------------------

void AccountHistory::handle_block_accepted( const broadcast::block_accepted& ba )
{
  if( !ba.has_block() )
    return;

  const auto& block = ba.block();
  uint64_t height   = block.header().height();

  // Collect impacted addresses from this block
  std::set< std::string > addresses;

  // Block signer
  if( !block.header().signer().empty() )
    addresses.insert( block.header().signer() );

  // Transactions: payer, operations, events
  for( const auto& tx: block.transactions() )
  {
    if( tx.has_header() )
    {
      if( !tx.header().payer().empty() )
        addresses.insert( tx.header().payer() );
      if( !tx.header().payee().empty() )
        addresses.insert( tx.header().payee() );
    }

    // Check operations for contract calls
    for( const auto& op: tx.operations() )
    {
      if( op.has_call_contract() && !op.call_contract().contract_id().empty() )
        addresses.insert( op.call_contract().contract_id() );
      if( op.has_upload_contract() && !op.upload_contract().contract_id().empty() )
        addresses.insert( op.upload_contract().contract_id() );
    }
  }

  // Store records for each impacted address
  if( addresses.empty() )
    return;

  std::unique_lock lock( _mutex );

  for( const auto& addr: addresses )
  {
    uint64_t seq = next_sequence( addr );

    // Find the transaction ID that relates to this address
    // For simplicity, store the block ID. A more detailed impl would
    // create per-transaction records.
    store_record( addr, seq, "", block.id(), height );
  }

  if( ba.live() )
  {
    LOG( debug ) << "[account_history] Indexed " << addresses.size()
                 << " addresses from block height=" << height;
  }
  else if( height % 1000 == 0 )
  {
    LOG( info ) << "[account_history] Sync progress height=" << height
                << " (" << addresses.size() << " addresses)";
  }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

uint64_t AccountHistory::next_sequence( const std::string& address )
{
  std::string meta_key = "meta:" + address;
  std::string value;
  auto s = _db->Get( rocksdb::ReadOptions(), _cf, meta_key, &value );

  uint64_t seq = 0;
  if( s.ok() && value.size() == 8 )
    seq = decode_seq( value );

  uint64_t next = seq + 1;
  _db->Put( rocksdb::WriteOptions(), _cf, meta_key, encode_seq( next ) );

  return seq;
}

void AccountHistory::store_record( const std::string& address,
                                    uint64_t seq,
                                    const std::string& trx_id,
                                    const std::string& block_id,
                                    uint64_t block_height )
{
  rpc::account_history::account_history_entry record;
  record.set_seq_num( seq );

  // Store as block record with header containing height
  auto* block_rec = record.mutable_block();
  auto* header = block_rec->mutable_header();
  header->set_height( block_height );
  header->set_previous( block_id ); // Store block ID in previous field for reference

  std::string record_bytes;
  if( !record.SerializeToString( &record_bytes ) )
    return;

  std::string key = address + encode_seq( seq );
  _db->Put( rocksdb::WriteOptions(), _cf, key, record_bytes );
}

std::string AccountHistory::encode_seq( uint64_t seq )
{
  std::string s( 8, '\0' );
  for( int i = 7; i >= 0; --i )
  {
    s[ i ] = static_cast< char >( seq & 0xFF );
    seq >>= 8;
  }
  return s;
}

uint64_t AccountHistory::decode_seq( const std::string& s )
{
  if( s.size() < 8 )
    return 0;
  uint64_t v = 0;
  for( int i = 0; i < 8; ++i )
    v = ( v << 8 ) | static_cast< uint8_t >( s[ i ] );
  return v;
}

} // namespace koinos::node::account_history
