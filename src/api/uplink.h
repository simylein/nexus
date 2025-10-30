#pragma once

#include "../lib/strn.h"
#include <pthread.h>
#include <stdint.h>
#include <time.h>

typedef struct uplink_t {
	uint8_t kind;
	uint8_t data[256];
	uint8_t data_len;
	uint16_t airtime;
	uint32_t frequency;
	uint32_t bandwidth;
	int16_t rssi;
	int8_t snr;
	uint8_t spreading_factor;
	uint8_t tx_power;
	time_t received_at;
	uint8_t device_id[16];
} uplink_t;

typedef struct uplinks_t {
	uplink_t *ptr;
	uint8_t head;
	uint8_t tail;
	uint8_t size;
	pthread_mutex_t lock;
	pthread_cond_t filled;
	pthread_cond_t available;
} uplinks_t;

extern struct uplinks_t uplinks;

int uplink_spawn(pthread_t *thread, void *(*function)(void *));

void *uplink_thread(void *args);

int uplink_create(uplink_t *uplink, strn8_t *cookie);
