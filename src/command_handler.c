#include "command_handler.h"
#include "rocksdb_handler.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void handle_command(const char* command, char* response, int response_size) {
    if (strncmp(command, "get_top_dns", 11) == 0) {
        uint32_t n = 10; // Default to top 10
        sscanf(command, "get_top_dns %u", &n);
        get_top_dns_queries(n, response, response_size);
    } else {
        snprintf(response, response_size, "Unknown command");
    }
}

void get_top_dns_queries(uint32_t n, char* response, int response_size) {
    // This function should interact with RocksDB to get the top N DNS queries
    // For now, we'll just return a placeholder message
    snprintf(response, response_size, "Top %u DNS queries (placeholder)", n);
}

