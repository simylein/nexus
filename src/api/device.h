#pragma once

#include "../lib/request.h"
#include "../lib/response.h"
#include <sqlite3.h>
#include <stdint.h>

typedef struct device_t {
	uint8_t (*id)[16];
	uint8_t (*tag)[2];
	uint8_t (*key)[16];
} device_t;

typedef struct device_query_t {
	const char *order;
	size_t order_len;
	const char *sort;
	size_t sort_len;
	uint8_t limit;
	uint32_t offset;
} device_query_t;

extern const char *device_table;
extern const char *device_schema;

uint16_t device_select(sqlite3 *database, device_query_t *query, response_t *response, uint8_t *device_len);
uint16_t device_insert(sqlite3 *database, device_t *device);
uint16_t device_update(sqlite3 *database, uint8_t (*id)[16], device_t *device);
uint16_t device_delete(sqlite3 *database, device_t *device);

void device_find(sqlite3 *database, request_t *request, response_t *response);
void device_create(sqlite3 *database, request_t *request, response_t *response);
void device_modify(sqlite3 *database, request_t *request, response_t *response);
void device_remove(sqlite3 *database, request_t *request, response_t *response);
