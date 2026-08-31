#pragma once

#include "../lib/octet.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct radio_t {
	uint8_t (*id)[8];
	char *spi_device;
	uint8_t spi_device_len;
	char *gpio_device;
	uint8_t gpio_device_len;
	uint8_t gpio_int_pin;
	uint32_t frequency;
	uint32_t bandwidth;
	uint8_t spreading_factor;
	uint8_t coding_rate;
	uint8_t tx_power;
	uint8_t preamble_len;
	uint8_t sync_word;
	bool checksum;
} radio_t;

typedef struct radio_query_t {
	const char *order;
	size_t order_len;
	const char *sort;
	size_t sort_len;
	uint8_t limit;
	uint32_t offset;
} radio_query_t;

typedef struct radio_row_t {
	uint8_t id;
	uint8_t spi_device_len;
	uint8_t spi_device;
	uint8_t gpio_device_len;
	uint8_t gpio_device;
	uint8_t gpio_int_pin;
	uint8_t frequency;
	uint8_t bandwidth;
	uint8_t spreading_factor;
	uint8_t coding_rate;
	uint8_t tx_power;
	uint8_t preamble_len;
	uint8_t sync_word;
	uint8_t checksum;
	uint8_t size;
} radio_row_t;

extern const char *radio_file;

extern const radio_row_t radio_row;

uint16_t radio_select(octet_t *db, radio_query_t *query, response_t *response, uint8_t *radios_len);
uint16_t radio_insert(octet_t *db, radio_t *radio);
uint16_t radio_update(octet_t *db, radio_t *radio);
uint16_t radio_delete(octet_t *db, radio_t *radio);

void radio_find(octet_t *db, request_t *request, response_t *response);
void radio_create(octet_t *db, request_t *request, response_t *response);
void radio_modify(octet_t *db, request_t *request, response_t *response);
void radio_remove(octet_t *db, request_t *request, response_t *response);
