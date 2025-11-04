#pragma once

#include "../api/device.h"
#include "../api/radio.h"
#include <pthread.h>
#include <sqlite3.h>

typedef struct radio_arg_t {
	int fd;
	radio_t *radio;
	device_t *devices;
	uint8_t devices_len;
} radio_arg_t;

int radio_init(sqlite3 *database);
int radio_spawn(pthread_t *thread, void *(*function)(void *), radio_arg_t *arg);
void *radio_thread(void *args);
