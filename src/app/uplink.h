#pragma once

#include "../api/host.h"
#include "../lib/strn.h"
#include "auth.h"
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
	uint8_t preamble_len;
	time_t received_at;
	uint8_t device_id[16];
} uplink_t;

typedef struct uplink_arg_t {
	host_t *hosts;
	uint8_t hosts_len;
} uplink_arg_t;

typedef struct uplink_worker_t {
	pthread_t thread;
	uplink_arg_t arg;
} uplink_worker_t;

typedef struct uplinks_t {
	uplink_worker_t worker;
	uplink_t *ptr;
	uint8_t head;
	uint8_t tail;
	uint8_t size;
	pthread_mutex_t lock;
	pthread_cond_t filled;
	pthread_cond_t available;
} uplinks_t;

extern struct uplinks_t uplinks;

int uplink_init(sqlite3 *database);

int uplink_spawn(pthread_t *thread, void *(*function)(void *), uplink_arg_t *arg);

void *uplink_thread(void *args);

int uplink_create(uplink_t *uplink, host_t *host, cookie_t *cookie);
