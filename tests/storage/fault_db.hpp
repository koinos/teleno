#pragma once
#include <rocksdb/db.h>
struct StorageFaults
{
  bool fail_read=false, fail_write=false, committed_error=false;
  rocksdb::ColumnFamilyHandle* fail_put=nullptr;
  int fail_flush=0, flushes=0;
};
rocksdb::DB* make_fault_database(rocksdb::DB*, StorageFaults&);
