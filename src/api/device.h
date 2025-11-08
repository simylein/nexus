#pragma once

#include "../lib/request.h"
#include "../lib/response.h"
#include <sqlite3.h>
#include <stdint.h>

typedef struct device_t {
	uint8_t (*id)[16];
	uint8_t (*tag)[2];
} device_t;

typedef struct device_query_t {
	char *order;
	uint8_t order_len;
	char *sort;
	uint8_t sort_len;
} device_query_t;

extern const char *device_table;
extern const char *device_schema;

uint16_t device_select(sqlite3 *database, device_query_t *query, response_t *response, uint8_t *device_len);
uint16_t device_insert(sqlite3 *database, device_t *device);
uint16_t device_delete(sqlite3 *database, device_t *device);

void device_find(sqlite3 *database, request_t *request, response_t *response);
void device_create(sqlite3 *database, request_t *request, response_t *response);
void device_remove(sqlite3 *database, request_t *request, response_t *response);
