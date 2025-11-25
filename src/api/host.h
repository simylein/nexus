#pragma once

#include "../lib/request.h"
#include "../lib/response.h"
#include <sqlite3.h>
#include <stdint.h>

typedef struct host_t {
	uint8_t (*id)[16];
	char *address;
	uint8_t address_len;
	uint16_t port;
	char *username;
	uint8_t username_len;
	char *password;
	uint8_t password_len;
} host_t;

typedef struct host_query_t {
	const char *order;
	size_t order_len;
	const char *sort;
	size_t sort_len;
	uint8_t limit;
	uint32_t offset;
} host_query_t;

extern const char *host_table;
extern const char *host_schema;

uint16_t host_select(sqlite3 *database, host_query_t *query, response_t *response, uint8_t *hosts_len);
uint16_t host_insert(sqlite3 *database, host_t *host);
uint16_t host_delete(sqlite3 *database, host_t *host);

void host_find(sqlite3 *database, request_t *request, response_t *response);
void host_create(sqlite3 *database, request_t *request, response_t *response);
void host_remove(sqlite3 *database, request_t *request, response_t *response);
