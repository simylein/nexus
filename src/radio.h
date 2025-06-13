#pragma once

#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct radio_t {
	uint8_t (*id)[16];
	char *device;
	uint8_t device_len;
	uint32_t frequency;
	uint8_t tx_power;
	uint8_t coding_rate;
	uint32_t bandwidth;
	uint8_t spreading_factor;
	bool checksum;
	uint8_t sync_word;
} radio_t;

extern const char *radio_table;
extern const char *radio_schema;

uint16_t radio_insert(sqlite3 *database, radio_t *radio);
