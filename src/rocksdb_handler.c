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

    // Reset all data in the database
    rocksdb_iterator_t* it = rocksdb_create_iterator(db, readoptions);
    rocksdb_iter_seek_to_first(it);

    while (rocksdb_iter_valid(it)) {
        size_t key_len;
        const char* key = rocksdb_iter_key(it, &key_len);
        rocksdb_delete(db, writeoptions, key, key_len, &err);
        if (err != NULL) {
            fprintf(stderr, "Error deleting key: %s\n", err);
            free(err);
            err = NULL;
        }
        rocksdb_iter_next(it);
    }

    rocksdb_iter_destroy(it);

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

TrafficData* read_all_traffic_data(int* count) {
    rocksdb_iterator_t* iter = rocksdb_create_iterator(db, readoptions);
    rocksdb_iter_seek_to_first(iter);

    TrafficData* data = NULL;
    int capacity = 10;
    *count = 0;

    data = malloc(capacity * sizeof(TrafficData));
    if (data == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        rocksdb_iter_destroy(iter);
        return NULL;
    }

    while (rocksdb_iter_valid(iter)) {
        size_t key_len, value_len;
        const char* key = rocksdb_iter_key(iter, &key_len);
        const char* value = rocksdb_iter_value(iter, &value_len);

        if (*count >= capacity) {
            capacity *= 2;
            TrafficData* temp = realloc(data, capacity * sizeof(TrafficData));
            if (temp == NULL) {
                fprintf(stderr, "Memory reallocation failed\n");
                free(data);
                rocksdb_iter_destroy(iter);
                return NULL;
            }
            data = temp;
        }

        strncpy(data[*count].ip_addr, key, sizeof(data[*count].ip_addr) - 1);
        data[*count].ip_addr[sizeof(data[*count].ip_addr) - 1] = '\0';
        data[*count].bytes = strtoull(value, NULL, 10);

        (*count)++;
        rocksdb_iter_next(iter);
    }

    rocksdb_iter_destroy(iter);
    return data;
}