#pragma once
#include <rocksdb/db.h>

struct StorageFaults
{
  rocksdb::ColumnFamilyHandle* fail_put = nullptr;
};

// The caller retains ownership of the wrapped database.
rocksdb::DB* make_fault_database( rocksdb::DB*, StorageFaults& );
