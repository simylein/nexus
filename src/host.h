#pragma once

#include <sqlite3.h>
#include <stdint.h>

typedef struct host_t {
	uint8_t id[16];
	char address[64];
	uint8_t address_len;
	uint16_t port;
} host_t;

extern const char *host_table;
extern const char *host_schema;

uint16_t host_select(sqlite3 *database, host_t (*hosts)[16], uint8_t *hosts_len);
