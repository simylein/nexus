#pragma once

#include "device.h"
#include "radio.h"
#include <pthread.h>
#include <stdint.h>

typedef struct arg_t {
	uint8_t id;
	int fd;
	radio_t *radio;
	device_t (*devices)[16];
	uint8_t devices_len;
} arg_t;

typedef struct worker_t {
	arg_t arg;
	pthread_t thread;
} worker_t;

int spawn(worker_t *worker, void *(*function)(void *),
					void (*logger)(const char *message, ...) __attribute__((format(printf, 1, 2))));

void *thread(void *args);
