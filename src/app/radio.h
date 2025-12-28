#pragma once

#include "../api/device.h"
#include "../api/radio.h"
#include "../lib/response.h"
#include <pthread.h>
#include <sqlite3.h>
#include <stdint.h>

typedef struct radio_arg_t {
	int fd;
	radio_t *radio;
	device_t *devices;
	uint8_t devices_len;
} radio_arg_t;

typedef struct radio_worker_t {
	pthread_t thread;
	radio_arg_t arg;
} radio_worker_t;

typedef struct comms_t {
	radio_worker_t *workers;
	radio_t *radios;
	uint8_t radios_len;
	device_t *devices;
	uint8_t devices_len;
} comms_t;

extern struct comms_t comms;

int radio_init(sqlite3 *database);
int radio_spawn(pthread_t *thread, void *(*function)(void *), radio_arg_t *arg);
void *radio_thread(void *args);
void radio_reload(sqlite3 *database, response_t *response);
