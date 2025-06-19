#pragma once

#include <sqlite3.h>
#include <stdint.h>

typedef struct device_t {
	uint8_t id[16];
	uint8_t tag[2];
} device_t;

extern const char *device_table;
extern const char *device_schema;

uint16_t device_select(sqlite3 *database, device_t (*devices)[16], uint8_t *device_len);
uint16_t device_insert(sqlite3 *database, device_t *device);
