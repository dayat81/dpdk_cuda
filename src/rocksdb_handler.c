#include "rocksdb_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static rocksdb_t *db;
static rocksdb_options_t *options;
static rocksdb_writeoptions_t *writeoptions;
static rocksdb_readoptions_t *readoptions;

int init_rocksdb(const char *db_path) {
    options = rocksdb_options_create();
    rocksdb_options_set_create_if_missing(options, 1);
    writeoptions = rocksdb_writeoptions_create();
    readoptions = rocksdb_readoptions_create();

    char *err = NULL;
    db = rocksdb_open(options, db_path, &err);
    if (err != NULL) {
        fprintf(stderr, "Error opening database: %s\n", err);
        free(err);
        return 1;
    }
    return 0;
}

void close_rocksdb(void) {
    rocksdb_close(db);
    rocksdb_options_destroy(options);
    rocksdb_writeoptions_destroy(writeoptions);
    rocksdb_readoptions_destroy(readoptions);
}

void update_ip_traffic(const char *ip_addr, uint32_t bytes) {
    char key[16];
    char value[32];
    char *err = NULL;
    size_t read_len;
    
    snprintf(key, sizeof(key), "%s", ip_addr);
    
    char *existing_value = rocksdb_get(db, readoptions, key, strlen(key), &read_len, &err);
    if (err != NULL) {
        fprintf(stderr, "Error reading from database: %s\n", err);
        free(err);
        return;
    }
    
    uint64_t total_bytes = bytes;
    if (existing_value != NULL) {
        total_bytes += strtoull(existing_value, NULL, 10);
        free(existing_value);
    }
    
    snprintf(value, sizeof(value), "%lu", total_bytes);
    
    rocksdb_put(db, writeoptions, key, strlen(key), value, strlen(value), &err);
    if (err != NULL) {
        fprintf(stderr, "Error writing to database: %s\n", err);
        free(err);
    }
}