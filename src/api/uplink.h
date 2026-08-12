#pragma once

#include "../lib/octet.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct uplink_t {
	uint16_t frame;
	uint8_t kind;
	uint8_t data[256];
	uint8_t data_len;
	uint16_t airtime;
	uint32_t frequency;
	uint32_t bandwidth;
	int16_t rssi;
	int8_t snr;
	uint8_t spreading_factor;
	uint8_t coding_rate;
	bool checksum;
	uint8_t tx_power;
	uint8_t preamble_len;
	time_t received_at;
	uint8_t device_id[8];
} uplink_t;

typedef struct uplink_row_t {
	uint16_t frame;
	uint16_t kind;
	uint16_t data_len;
	uint16_t data;
	uint16_t airtime;
	uint16_t frequency;
	uint16_t bandwidth;
	uint16_t rssi;
	uint16_t snr;
	uint16_t spreading_factor;
	uint16_t coding_rate;
	uint16_t checksum;
	uint16_t tx_power;
	uint16_t preamble_len;
	uint16_t received_at;
	uint16_t device_id;
	uint16_t size;
} uplink_row_t;

extern const char *uplink_file;

extern const uplink_row_t uplink_row;

uint16_t uplink_select_one(octet_t *db, uplink_t *uplink, uint8_t *head);
uint16_t uplink_insert(octet_t *db, uplink_t *uplink, uint8_t *tail);
uint16_t uplink_delete(octet_t *db, uplink_t *uplink, uint8_t *head);
