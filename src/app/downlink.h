#pragma once

#include "../api/downlink.h"
#include "../api/host.h"
#include "../lib/octet.h"
#include "../lib/strn.h"
#include "auth.h"
#include <pthread.h>
#include <stdint.h>

typedef struct downlink_arg_t {
	octet_t db;
	host_t *hosts;
	uint8_t hosts_len;
} downlink_arg_t;

typedef struct downlink_worker_t {
	pthread_t thread;
	downlink_arg_t arg;
} downlink_worker_t;

typedef struct downlinks_t {
	downlink_worker_t worker;
	uint8_t head;
	uint8_t tail;
	uint8_t size;
	pthread_mutex_t lock;
	pthread_cond_t filled;
	pthread_cond_t available;
} downlinks_t;

extern struct downlinks_t downlinks;

int downlink_init(octet_t *db);

int downlink_spawn(pthread_t *thread, void *(*function)(void *), downlink_arg_t *arg);

void *downlink_thread(void *args);

int downlink_create(downlink_t *downlink, host_t *host, cookie_t *cookie);
