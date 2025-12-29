#pragma once

#include "../lib/request.h"
#include "../lib/response.h"
#include <pthread.h>
#include <stdint.h>
#include <time.h>

typedef struct streams_t {
	int *ptr;
	uint8_t size;
	pthread_mutex_t lock;
} streams_t;

typedef struct transmission_t {
	time_t timestamp;
	uint8_t radio_id[16];
	char type[2];
	uint8_t device_id[16];
	uint8_t kind;
	uint8_t data[256];
	uint8_t data_len;
	int16_t rssi;
	int8_t snr;
	uint8_t sf;
	uint8_t cr;
	uint8_t tx_power;
	uint8_t preamble_len;
} transmission_t;

typedef struct transmission_worker_t {
	pthread_t thread;
} transmission_worker_t;

typedef struct transmissions_t {
	transmission_worker_t worker;
	transmission_t *ptr;
	uint8_t head;
	uint8_t tail;
	uint8_t size;
	pthread_mutex_t lock;
	pthread_cond_t filled;
	pthread_cond_t available;
} transmissions_t;

extern struct streams_t streams;
extern struct transmissions_t transmissions;

int transmission_init(void);

int transmission_spawn(pthread_t *thread, void *(*function)(void *));

void *transmission_thread(void *args);

void transmission_stream(request_t *request, response_t *response);
