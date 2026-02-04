#pragma once

#include "../lib/octet.h"
#include "../lib/request.h"
#include "../lib/response.h"
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

typedef struct host_row_t {
	uint8_t id;
	uint8_t address_len;
	uint8_t address;
	uint8_t port;
	uint8_t username_len;
	uint8_t username;
	uint8_t password_len;
	uint8_t password;
	uint8_t size;
} host_row_t;

extern const char *host_file;

extern const host_row_t host_row;

uint16_t host_select(octet_t *db, host_query_t *query, response_t *response, uint8_t *hosts_len);
uint16_t host_insert(octet_t *db, host_t *host);
uint16_t host_update(octet_t *db, host_t *host);
uint16_t host_delete(octet_t *db, host_t *host);

void host_find(octet_t *db, request_t *request, response_t *response);
void host_create(octet_t *db, request_t *request, response_t *response);
void host_modify(octet_t *db, request_t *request, response_t *response);
void host_remove(octet_t *db, request_t *request, response_t *response);
