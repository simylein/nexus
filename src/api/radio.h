#pragma once

#include "../lib/request.h"
#include "../lib/response.h"
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct radio_t {
	uint8_t (*id)[16];
	char *device;
	uint8_t device_len;
	uint32_t frequency;
	uint32_t bandwidth;
	uint8_t spreading_factor;
	uint8_t coding_rate;
	uint8_t tx_power;
	uint8_t sync_word;
	bool checksum;
} radio_t;

typedef struct radio_query_t {
	char *order;
	uint8_t order_len;
	char *sort;
	uint8_t sort_len;
} radio_query_t;

extern const char *radio_table;
extern const char *radio_schema;

uint16_t radio_select(sqlite3 *database, radio_query_t *query, response_t *response, uint8_t *radios_len);
uint16_t radio_insert(sqlite3 *database, radio_t *radio);
uint16_t radio_delete(sqlite3 *database, radio_t *radio);

void radio_find(sqlite3 *database, request_t *request, response_t *response);
void radio_create(sqlite3 *database, request_t *request, response_t *response);
void radio_remove(sqlite3 *database, request_t *request, response_t *response);
