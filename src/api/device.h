#pragma once

#include "../lib/octet.h"
#include "../lib/request.h"
#include "../lib/response.h"
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

typedef struct device_row_t {
	uint8_t id;
	uint8_t tag;
	uint8_t key;
	uint8_t size;
} device_row_t;

extern const char *device_file;

extern const device_row_t device_row;

uint16_t device_select(octet_t *db, device_query_t *query, response_t *response, uint8_t *devices_len);
uint16_t device_insert(octet_t *db, device_t *device);
uint16_t device_update(octet_t *db, uint8_t (*id)[16], device_t *device);
uint16_t device_delete(octet_t *db, device_t *device);

void device_find(octet_t *db, request_t *request, response_t *response);
void device_create(octet_t *db, request_t *request, response_t *response);
void device_modify(octet_t *db, request_t *request, response_t *response);
void device_remove(octet_t *db, request_t *request, response_t *response);
