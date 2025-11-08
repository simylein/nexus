#pragma once

#include "../api/host.h"
#include "../lib/strn.h"
#include <pthread.h>
#include <stdint.h>
#include <time.h>

typedef struct downlink_t {
	uint8_t kind;
	uint8_t data[256];
	uint8_t data_len;
	uint16_t airtime;
	uint32_t frequency;
	uint32_t bandwidth;
	uint8_t spreading_factor;
	uint8_t tx_power;
	time_t sent_at;
	uint8_t device_id[16];
} downlink_t;

typedef struct downlinks_t {
	downlink_t *ptr;
	uint8_t head;
	uint8_t tail;
	uint8_t size;
	pthread_mutex_t lock;
	pthread_cond_t filled;
	pthread_cond_t available;
} downlinks_t;

typedef struct downlink_arg_t {
	host_t *hosts;
	uint8_t hosts_len;
} downlink_arg_t;

extern struct downlinks_t downlinks;

int downlink_init(sqlite3 *database);

int downlink_spawn(pthread_t *thread, void *(*function)(void *), downlink_arg_t *arg);

void *downlink_thread(void *args);

int downlink_create(downlink_t *downlink, host_t *host, strn8_t *cookie);
