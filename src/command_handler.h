#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <stdint.h>

// Function to handle incoming commands
void handle_command(const char* command, char* response, int response_size);

// Function to get top N DNS queries
void get_top_dns_queries(uint32_t n, char* response, int response_size);

// Function to get traffic for a specific IP
void get_ip_traffic(const char* ip, char* response, int response_size);

#endif // COMMAND_HANDLER_H