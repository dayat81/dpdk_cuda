#ifndef ROCKSDB_HANDLER_H
#define ROCKSDB_HANDLER_H

#include <rocksdb/c.h>

// Initialize RocksDB
int init_rocksdb(const char *db_path);

// Close RocksDB
void close_rocksdb(void);

// Update IP traffic in RocksDB
void update_ip_traffic(const char *ip_addr, uint32_t bytes);

#endif // ROCKSDB_HANDLER_H