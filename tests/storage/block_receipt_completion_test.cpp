#include "block_store/block_store.hpp"
#include "fault_db.hpp"


#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>

namespace fs = std::filesystem;
using koinos::node::block_store::BlockStore;

void check( bool value, const char* message )
{
  if( !value ) throw std::runtime_error( message );
}

struct Fixture
{
  fs::path path;
  std::unique_ptr< rocksdb::DB > db;
  std::vector< rocksdb::ColumnFamilyHandle* > handles;
  StorageFaults faults;
  std::unique_ptr< rocksdb::DB > fault_db;

  Fixture()
  {
    const char* selected = std::getenv( "TELENO_TEST_ARTIFACT_ROOT" );
    check( selected != nullptr, "TELENO_TEST_ARTIFACT_ROOT must select owned test storage" );
    const fs::path root( selected );
    check( root.is_absolute() && fs::is_directory( root ) && !fs::is_symlink( root )
           && fs::weakly_canonical( root ) != root.root_path(), "invalid test artifact root" );
    for( const auto& part: root ) check( part != "..", "test root parent traversal" );
    path = root / ( "receipt-completion-" + std::to_string( std::random_device{}() ) );
    check( fs::create_directory( path ), "fresh fixture required" );
    std::cout << "retained receipt fixture: " << path << '\n';
    open();
  }

  void open()
  {
    rocksdb::DBOptions options;
    options.create_if_missing = true;
    options.create_missing_column_families = true;
    std::vector< rocksdb::ColumnFamilyDescriptor > columns;
    for( const auto& name: { rocksdb::kDefaultColumnFamilyName, std::string( "blocks" ),
                           std::string( "block_meta" ) } )
      columns.emplace_back( name, rocksdb::ColumnFamilyOptions{} );
    rocksdb::DB* value = nullptr;
    check( rocksdb::DB::Open( options, path.string(), columns, &handles, &value ).ok(), "open fixture" );
    db.reset( value );
    fault_db.reset( make_fault_database( db.get(), faults ) );
  }

  void close()
  {
    fault_db.reset();
    for( auto* handle: handles ) check( db->DestroyColumnFamilyHandle( handle ).ok(), "close handle" );
    handles.clear();
    db.reset();
  }

  ~Fixture()
  {
    if( db ) { fault_db.reset(); for( auto* handle: handles ) db->DestroyColumnFamilyHandle( handle ); }
  }

  BlockStore store() { return BlockStore( fault_db.get(), handles[ 1 ], handles[ 2 ] ); }
  std::string raw( const std::string& id )
  {
    std::string value;
    check( db->Get( {}, handles[ 1 ], id, &value ).ok(), "read persisted record" );
    return value;
  }
};

std::string hash( char byte ) { return std::string( "\x12\x20", 2 ) + std::string( 32, byte ); }

koinos::rpc::block_store::add_block_request early_receipt()
{
  koinos::rpc::block_store::add_block_request req;
  auto* block = req.mutable_block_to_add();
  block->set_id( hash( 'b' ) );
  block->mutable_header()->set_height( 1 );
  block->mutable_header()->set_previous( std::string( 34, '\0' ) );
  auto* receipt = req.mutable_receipt_to_add();
  receipt->set_id( block->id() );
  receipt->set_height( 1 );
  receipt->set_disk_storage_used( 47 );
  receipt->add_logs( "synthetic executed block" );
  receipt->add_transaction_receipts()->set_id( hash( 't' ) );
  return req;
}

auto complete( const koinos::rpc::block_store::add_block_request& early )
{
  auto req = early;
  req.mutable_receipt_to_add()->set_state_merkle_root( hash( 's' ) );
  return req;
}

void assert_receipt( BlockStore& store, const koinos::rpc::block_store::add_block_request& req )
{
  koinos::rpc::block_store::get_blocks_by_id_request read;
  read.add_block_ids( req.block_to_add().id() );
  read.set_return_block( true );
  read.set_return_receipt( true );
  const auto result = store.get_blocks_by_id( read );
  check( result.block_items_size() == 1, "block RPC item retained" );
  check( result.block_items( 0 ).block().SerializeAsString() == req.block_to_add().SerializeAsString(),
         "complete block preserved" );
  check( result.block_items( 0 ).receipt().SerializeAsString() == req.receipt_to_add().SerializeAsString(),
         "exact execution receipt/root preserved" );
}

void accepted_path_and_reopen()
{
  Fixture fixture;
  auto early = early_receipt();
  auto final = complete( early );
  {
    auto store = fixture.store();
    store.initialize();
    store.add_block( early );
    assert_receipt( store, early );
    auto next = early;
    next.mutable_block_to_add()->set_id( hash( 'c' ) );
    next.mutable_block_to_add()->mutable_header()->set_height( 2 );
    next.mutable_block_to_add()->mutable_header()->set_previous( early.block_to_add().id() );
    next.mutable_receipt_to_add()->set_id( hash( 'c' ) );
    next.mutable_receipt_to_add()->set_height( 2 );
    store.add_block( next ); // Complete an older receipt after topology advances.
    const auto topology = store.get_highest_block( {} ).SerializeAsString();
    koinos::broadcast::block_accepted accepted;
    *accepted.mutable_block() = final.block_to_add();
    *accepted.mutable_receipt() = final.receipt_to_add();
    store.handle_block_accepted( accepted );
    assert_receipt( store, final );
    const auto persisted = fixture.raw( early.block_to_add().id() );
    store.add_block( early ); // A late incomplete repeat must not erase the root.
    store.handle_block_accepted( accepted );
    check( fixture.raw( early.block_to_add().id() ) == persisted, "duplicate changed complete record" );
    check( store.get_highest_block( {} ).SerializeAsString() == topology, "receipt completion moved topology" );
  }
  fixture.close();
  fixture.open();
  auto reopened = fixture.store();
  reopened.initialize();
  assert_receipt( reopened, final );
}

void conflicts_and_failed_write()
{
  Fixture fixture;
  auto store = fixture.store();
  store.initialize();
  auto early = early_receipt();
  auto final = complete( early );
  store.add_block( early );
  fixture.faults.fail_put = fixture.handles[ 1 ];
  bool failed = false;
  try { store.add_block( final ); } catch( const std::exception& ) { failed = true; }
  check( failed, "failed completion write reported success" );
  assert_receipt( store, early );
  fixture.faults.fail_put = nullptr;
  for( int phase = 0; phase < 2; ++phase )
  {
  if( phase == 1 ) store.add_block( final );
  const auto retained = fixture.raw( early.block_to_add().id() );
  for( int n = 0; n < 7; ++n )
  {
    auto conflicting = final;
    auto* receipt = conflicting.mutable_receipt_to_add();
    switch( n )
    {
      case 0: receipt->set_id( hash( 'x' ) ); break;
      case 1: receipt->set_height( 2 ); break;
      case 2: receipt->set_disk_storage_used( 48 ); break;
      case 3:
        // An incomplete receipt has no expected digest to compare with. Check
        // its encoding; after completion, refuse a different valid digest.
        receipt->set_state_merkle_root( phase == 0 ? std::string( "\x13\x20", 2 ) + std::string( 32, 'x' )
                                                  : hash( 'x' ) );
        break;
      case 4: receipt->set_state_merkle_root( "invalid" ); break;
      case 5: receipt->mutable_transaction_receipts( 0 )->set_id( hash( 'x' ) ); break;
      case 6: conflicting.mutable_block_to_add()->mutable_header()->set_previous( hash( 'x' ) ); break;
    }
    bool rejected = false;
    try { store.add_block( conflicting ); } catch( const std::exception& ) { rejected = true; }
    check( rejected, "conflicting completion accepted" );
    check( fixture.raw( early.block_to_add().id() ) == retained, "refusal damaged archived record" );
  }
  }
  assert_receipt( store, final );
}

void absent_receipt_is_preserved()
{
  Fixture fixture;
  auto store = fixture.store();
  store.initialize();
  auto early = early_receipt();
  auto no_receipt = early;
  no_receipt.clear_receipt_to_add();
  store.add_block( no_receipt );
  const auto before = fixture.raw( early.block_to_add().id() );
  store.add_block( complete( early ) );
  check( fixture.raw( early.block_to_add().id() ) == before, "unexpected historical receipt replacement" );
}

int main()
{
  try
  {
    accepted_path_and_reopen();
    conflicts_and_failed_write();
    absent_receipt_is_preserved();
    std::cout << "receipt completion: accepted notification, cold reopen, duplicate preservation, "
                 "write failure/retry, fourteen conflict refusals and absent-receipt preservation passed\n";
    return 0;
  }
  catch( const std::exception& error ) { std::cerr << error.what() << '\n'; return 1; }
}
