#include "block_store.hpp"
#include "skip_list.hpp"

#include <koinos/exception.hpp>
#include <koinos/log.hpp>

#include <stdexcept>

namespace koinos::node::block_store {

// Metadata key: single byte 0x01 (same as Go implementation)
const std::string BlockStore::META_KEY = std::string( 1, '\x01' );

KOINOS_DECLARE_EXCEPTION( block_store_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( block_not_present, block_store_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( unexpected_height, block_store_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( db_error, block_store_exception );

BlockStore::BlockStore( rocksdb::DB* db,
                        rocksdb::ColumnFamilyHandle* cf_handle,
                        rocksdb::ColumnFamilyHandle* cf_meta )
    : _db( db ), _cf_blocks( cf_handle ), _cf_meta( cf_meta )
{
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::string BlockStore::get_record_bytes( const std::string& block_id ) const
{
  std::string value;
  auto s = _db->Get( rocksdb::ReadOptions(), _cf_blocks, block_id, &value );
  if( s.IsNotFound() )
    return {};
  if( !s.ok() )
    KOINOS_THROW( db_error, "RocksDB get failed: ${msg}", ( "msg", s.ToString() ) );
  return value;
}

void BlockStore::put_record_bytes( const std::string& block_id, const std::string& value )
{
  auto s = _db->Put( rocksdb::WriteOptions(), _cf_blocks, block_id, value );
  if( !s.ok() )
    KOINOS_THROW( db_error, "RocksDB put failed: ${msg}", ( "msg", s.ToString() ) );
}

bool BlockStore::get_highest_block_topology( koinos::block_topology& topo ) const
{
  std::string value;
  auto s = _db->Get( rocksdb::ReadOptions(), _cf_meta, META_KEY, &value );
  if( s.IsNotFound() )
    return false;
  if( !s.ok() )
    KOINOS_THROW( db_error, "RocksDB meta get failed: ${msg}", ( "msg", s.ToString() ) );
  return topo.ParseFromString( value );
}

void BlockStore::update_highest_block( const koinos::block_topology& topo )
{
  std::string value;
  if( !topo.SerializeToString( &value ) )
    KOINOS_THROW( db_error, "Failed to serialize block topology" );
  auto s = _db->Put( rocksdb::WriteOptions(), _cf_meta, META_KEY, value );
  if( !s.ok() )
    KOINOS_THROW( db_error, "RocksDB meta put failed: ${msg}", ( "msg", s.ToString() ) );
}

std::string BlockStore::get_ancestor_id_at_height( const std::string& block_id,
                                                    uint64_t target_height ) const
{
  std::string current_id = block_id;
  bool has_expected_height = false;
  uint64_t expected_height = 0;

  for( ;; )
  {
    auto record_bytes = get_record_bytes( current_id );
    if( record_bytes.empty() )
      KOINOS_THROW( block_not_present, "block not found during ancestor traversal" );

    koinos::block_store::block_record record;
    if( !record.ParseFromString( record_bytes ) )
      KOINOS_THROW( db_error, "failed to parse block record" );

    if( has_expected_height && record.block_height() != expected_height )
      KOINOS_THROW( unexpected_height, "height mismatch during ancestor traversal" );

    if( record.block_height() == target_height )
      return record.block_id();

    auto [idx, h] = get_previous_height_index( target_height, record.block_height() );

    if( idx < 0 || idx >= record.previous_block_ids_size() )
      KOINOS_THROW( unexpected_height, "skip-list index out of range" );

    current_id = record.previous_block_ids( idx );

    if( h == target_height )
      return current_id;

    expected_height     = h;
    has_expected_height = true;
  }
}

std::vector< koinos::block_store::block_item >
BlockStore::fill_blocks( const std::string& start_id,
                         uint32_t count,
                         bool return_block,
                         bool return_receipt ) const
{
  std::vector< koinos::block_store::block_item > items;
  items.reserve( count );

  std::string current_id = start_id;
  uint64_t expected_height = 0;
  bool first = true;

  for( uint32_t i = 0; i < count && !current_id.empty(); ++i )
  {
    auto record_bytes = get_record_bytes( current_id );
    if( record_bytes.empty() )
      break;

    koinos::block_store::block_record record;
    if( !record.ParseFromString( record_bytes ) )
      break;

    if( !first && record.block_height() != expected_height )
      break;

    koinos::block_store::block_item item;
    item.set_block_id( record.block_id() );
    item.set_block_height( record.block_height() );

    if( return_block && record.has_block() )
      *item.mutable_block() = record.block();
    if( return_receipt && record.has_receipt() )
      *item.mutable_receipt() = record.receipt();

    items.push_back( std::move( item ) );

    // Walk forward: the first skip-list entry (index 0) is the direct child
    // Actually, we need to walk from the block that has this block as previous[0].
    // The Go implementation walks backward, not forward.
    // For get_blocks_by_height, we collect blocks by walking the skip-list
    // from the head down to the start height, then return them in ascending order.
    //
    // Correction: fill_blocks walks backward through previous_block_ids[0].
    // The caller reverses the result for ascending order.
    if( record.previous_block_ids_size() > 0 )
      current_id = record.previous_block_ids( 0 );
    else
      current_id.clear();

    expected_height = record.block_height() - 1;
    first = false;
  }

  return items;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void BlockStore::initialize()
{
  std::unique_lock lock( _mutex );

  koinos::block_topology topo;
  if( !get_highest_block_topology( topo ) )
  {
    // Initialize with genesis (height 0, empty ID)
    topo.set_height( 0 );
    topo.set_id( std::string( 34, '\0' ) ); // 34-byte zero multihash
    update_highest_block( topo );
    LOG( info ) << "[block_store] Initialized metadata with genesis";
  }
}

// ---------------------------------------------------------------------------
// IBlockStore — RPC methods
// ---------------------------------------------------------------------------

rpc::block_store::get_highest_block_response
BlockStore::get_highest_block( const rpc::block_store::get_highest_block_request& )
{
  std::shared_lock lock( _mutex );

  rpc::block_store::get_highest_block_response resp;
  koinos::block_topology topo;
  if( get_highest_block_topology( topo ) )
    *resp.mutable_topology() = topo;

  return resp;
}

rpc::block_store::get_blocks_by_id_response
BlockStore::get_blocks_by_id( const rpc::block_store::get_blocks_by_id_request& req )
{
  std::shared_lock lock( _mutex );

  rpc::block_store::get_blocks_by_id_response resp;

  int count = std::min( req.block_ids_size(), static_cast< int >( max_block_request ) );
  for( int i = 0; i < count; ++i )
  {
    const auto& block_id = req.block_ids( i );
    auto record_bytes    = get_record_bytes( block_id );
    if( record_bytes.empty() )
      continue;

    koinos::block_store::block_record record;
    if( !record.ParseFromString( record_bytes ) )
      continue;

    auto* item = resp.add_block_items();
    item->set_block_id( record.block_id() );
    item->set_block_height( record.block_height() );

    if( req.return_block() && record.has_block() )
      *item->mutable_block() = record.block();
    if( req.return_receipt() && record.has_receipt() )
      *item->mutable_receipt() = record.receipt();
  }

  return resp;
}

rpc::block_store::get_blocks_by_height_response
BlockStore::get_blocks_by_height( const rpc::block_store::get_blocks_by_height_request& req )
{
  std::shared_lock lock( _mutex );

  rpc::block_store::get_blocks_by_height_response resp;

  if( req.ancestor_start_height() == 0 )
    return resp; // Height 0 rejected (same as Go)

  uint32_t num_blocks = std::min( req.num_blocks(), max_block_request );
  if( num_blocks == 0 )
    return resp;

  try
  {
    // Navigate to the ancestor at start height via skip-list
    auto start_id = get_ancestor_id_at_height(
      req.head_block_id(), req.ancestor_start_height() );

    // Collect blocks walking backward from start_id
    // We need num_blocks starting at ancestor_start_height going UP, but the
    // skip-list walks DOWN. Strategy: walk from head_block_id down to
    // start_height, then collect num_blocks walking down from start_height+num_blocks-1.
    //
    // Simpler approach matching Go: collect backwards and reverse.
    // But the Go code actually walks forward by following the chain from start.
    // Since we can't walk forward efficiently, we walk backward from a higher
    // point and reverse.

    // For simplicity matching Go behavior: just return the start block
    // and walk backward through previous[0], then reverse for ascending order.
    auto items = fill_blocks( start_id, 1, req.return_block(), req.return_receipt() );

    // If we need more blocks after the start, we'd need to find blocks at
    // higher heights. The Go implementation uses a separate "fillBlocks" that
    // walks from the head backward. For now, return at least the start block.
    // Full multi-block range requires the head→start walk to be reimplemented.

    // Walk backward from head to get blocks in the requested range
    if( num_blocks > 1 )
    {
      uint64_t end_height = req.ancestor_start_height() + num_blocks - 1;

      // Find the block at end_height
      auto end_id = get_ancestor_id_at_height( req.head_block_id(), end_height );

      // Walk backward from end to start, collecting blocks
      items = fill_blocks( end_id, num_blocks, req.return_block(), req.return_receipt() );

      // Reverse to get ascending order
      std::reverse( items.begin(), items.end() );
    }

    for( auto& item: items )
      *resp.add_block_items() = std::move( item );
  }
  catch( const block_not_present& )
  {
    // Block not found — return empty
  }
  catch( const unexpected_height& )
  {
    // Height mismatch — return empty
  }

  return resp;
}

rpc::block_store::add_block_response
BlockStore::add_block( const rpc::block_store::add_block_request& req )
{
  std::unique_lock lock( _mutex );

  rpc::block_store::add_block_response resp;

  if( !req.has_block_to_add() || !req.block_to_add().has_header() )
    throw std::runtime_error( "expected field block_to_add was nil" );

  const auto& block  = req.block_to_add();
  const auto& header = block.header();
  uint64_t height    = header.height();

  // Check if already stored
  auto existing = get_record_bytes( block.id() );
  if( !existing.empty() )
    return resp; // Idempotent

  // Build the BlockRecord with skip-list pointers
  koinos::block_store::block_record record;
  record.set_block_id( block.id() );
  record.set_block_height( height );
  *record.mutable_block() = block;

  if( req.has_receipt_to_add() )
    *record.mutable_receipt() = req.receipt_to_add();

  // Build skip-list previous_block_ids
  if( height > 1 )
  {
    auto prev_heights = get_previous_heights( height );

    for( size_t i = 0; i < prev_heights.size(); ++i )
    {
      uint64_t h = prev_heights[ i ];

      if( h == height - 1 )
      {
        // Direct parent: use header.previous
        record.add_previous_block_ids( header.previous() );
      }
      else
      {
        // Skip-list pointer: look up ancestor from the parent
        try
        {
          auto ancestor_id = get_ancestor_id_at_height( header.previous(), h );
          record.add_previous_block_ids( ancestor_id );
        }
        catch( const std::exception& e )
        {
          LOG( warning ) << "[block_store] Cannot build skip-list for height "
                         << height << " ancestor " << h << ": " << e.what();
          return resp; // Reject block if ancestors missing
        }
      }
    }
  }
  else if( height == 1 )
  {
    // Genesis block: single pointer to previous
    record.add_previous_block_ids( header.previous() );
  }

  // Serialize and store
  std::string record_bytes;
  if( !record.SerializeToString( &record_bytes ) )
    KOINOS_THROW( db_error, "failed to serialize block record" );

  put_record_bytes( block.id(), record_bytes );

  // Update highest block if this is the new tip
  koinos::block_topology current_highest;
  if( get_highest_block_topology( current_highest ) )
  {
    if( height > current_highest.height() )
    {
      koinos::block_topology new_highest;
      new_highest.set_id( block.id() );
      new_highest.set_height( height );
      new_highest.set_previous( header.previous() );
      update_highest_block( new_highest );
    }
  }

  return resp;
}

// ---------------------------------------------------------------------------
// Broadcast handler
// ---------------------------------------------------------------------------

void BlockStore::handle_block_accepted( const broadcast::block_accepted& ba )
{
  if( !ba.has_block() )
    return;

  rpc::block_store::add_block_request req;
  *req.mutable_block_to_add() = ba.block();
  if( ba.has_receipt() )
    *req.mutable_receipt_to_add() = ba.receipt();

  try
  {
    add_block( req );

    if( ba.live() )
    {
      LOG( debug ) << "[block_store] Stored live block height="
                   << ba.block().header().height();
    }
    else if( ba.block().header().height() % 1000 == 0 )
    {
      LOG( info ) << "[block_store] Sync progress height="
                  << ba.block().header().height();
    }
  }
  catch( const std::exception& e )
  {
    LOG( warning ) << "[block_store] Failed to store block: " << e.what();
  }
}

} // namespace koinos::node::block_store
