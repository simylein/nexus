#pragma once

#include "host.h"
#include <stdint.h>
#include <time.h>

typedef struct downlink_t {
	uint8_t kind;
	uint8_t *data;
	uint8_t data_len;
	uint16_t airtime;
	uint32_t frequency;
	uint32_t bandwidth;
	uint8_t tx_power;
	uint8_t sf;
	time_t sent_at;
	uint8_t (*device_id)[16];
} downlink_t;

int downlink_create(downlink_t *downlink, host_t *host, char (*cookie)[128], uint8_t cookie_len);
