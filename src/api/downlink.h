#pragma once

#include "../lib/octet.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct downlink_t {
	uint16_t frame;
	uint8_t kind;
	uint8_t data[256];
	uint8_t data_len;
	uint16_t airtime;
	uint32_t frequency;
	uint32_t bandwidth;
	uint8_t spreading_factor;
	uint8_t coding_rate;
	bool checksum;
	uint8_t tx_power;
	uint8_t preamble_len;
	time_t sent_at;
	uint8_t device_id[8];
} downlink_t;

typedef struct downlink_row_t {
	uint16_t frame;
	uint16_t kind;
	uint16_t data_len;
	uint16_t data;
	uint16_t airtime;
	uint16_t frequency;
	uint16_t bandwidth;
	uint16_t spreading_factor;
	uint16_t coding_rate;
	uint16_t checksum;
	uint16_t tx_power;
	uint16_t preamble_len;
	uint16_t sent_at;
	uint16_t device_id;
	uint16_t size;
} downlink_row_t;

extern const char *downlink_file;

extern const downlink_row_t downlink_row;

uint16_t downlink_select_one(octet_t *db, downlink_t *downlink, uint8_t *head);
uint16_t downlink_insert(octet_t *db, downlink_t *downlink, uint8_t *tail);
uint16_t downlink_delete(octet_t *db, downlink_t *downlink, uint8_t *head);
