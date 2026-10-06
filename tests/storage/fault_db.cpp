// Compiled without RTTI to match Hunter RocksDB. Keep Koinos/Boost.Log
// headers in the caller, where their RTTI contract is unchanged.
#include "fault_db.hpp"
#include <rocksdb/utilities/stackable_db.h>

namespace {
class FaultDB : public rocksdb::StackableDB
{
  StorageFaults& _faults;
public:
  FaultDB( rocksdb::DB* db, StorageFaults& faults ):
    StackableDB( std::shared_ptr< rocksdb::DB >( db, []( auto* ) {} ) ),
    _faults( faults ) {}

  rocksdb::Status Put( const rocksdb::WriteOptions& options,
                      rocksdb::ColumnFamilyHandle* cf,
                      const rocksdb::Slice& key,
                      const rocksdb::Slice& value ) override
  {
    return cf == _faults.fail_put ? rocksdb::Status::IOError( "synthetic put" )
                                 : db_->Put( options, cf, key, value );
  }
};
} // namespace

rocksdb::DB* make_fault_database( rocksdb::DB* db, StorageFaults& faults )
{
  return new FaultDB( db, faults );
}
