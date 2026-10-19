// Compiled without RTTI to match the installed RocksDB library. Keep Koinos/
// Boost.Log headers in the caller, where their RTTI contract is unchanged.
#include "fault_db.hpp"
#include <rocksdb/utilities/stackable_db.h>
class FaultDB : public rocksdb::StackableDB
{
  StorageFaults& f;
public:
  FaultDB(rocksdb::DB* db,StorageFaults& f): StackableDB(std::shared_ptr<rocksdb::DB>(db,[](auto*){})),f(f) {}
  rocksdb::Status Get(const rocksdb::ReadOptions& o,rocksdb::ColumnFamilyHandle* cf,
    const rocksdb::Slice& k,rocksdb::PinnableSlice* v) override
  { return f.fail_read ? rocksdb::Status::IOError("synthetic read") : db_->Get(o,cf,k,v); }
  rocksdb::Status Put(const rocksdb::WriteOptions& o,rocksdb::ColumnFamilyHandle* cf,
    const rocksdb::Slice& k,const rocksdb::Slice& v) override
  { return cf==f.fail_put ? rocksdb::Status::IOError("synthetic put") : db_->Put(o,cf,k,v); }
  rocksdb::Status Write(const rocksdb::WriteOptions& o,rocksdb::WriteBatch* b) override
  {
    if(f.fail_write&&!f.committed_error) return rocksdb::Status::IOError("synthetic batch");
    auto s=db_->Write(o,b);
    return f.fail_write&&s.ok()?rocksdb::Status::IOError("synthetic ambiguous result"):s;
  }
  rocksdb::Status Flush(const rocksdb::FlushOptions& o,rocksdb::ColumnFamilyHandle* cf) override
  { return ++f.flushes==f.fail_flush?rocksdb::Status::IOError("synthetic flush"):db_->Flush(o,cf); }
};
rocksdb::DB* make_fault_database(rocksdb::DB* db,StorageFaults& f) { return new FaultDB(db,f); }
