#pragma once

#include "../api/host.h"
#include "../api/uplink.h"
#include "../lib/octet.h"
#include "../lib/strn.h"
#include "auth.h"
#include <pthread.h>
#include <stdint.h>

typedef struct uplink_arg_t {
	octet_t db;
	host_t *hosts;
	uint8_t hosts_len;
} uplink_arg_t;

typedef struct uplink_worker_t {
	pthread_t thread;
	uplink_arg_t arg;
} uplink_worker_t;

typedef struct uplinks_t {
	uplink_worker_t worker;
	uint16_t head;
	uint16_t tail;
	uint16_t size;
	pthread_mutex_t lock;
	pthread_cond_t filled;
	pthread_cond_t available;
} uplinks_t;

extern struct uplinks_t uplinks;

int uplink_init(octet_t *db);

int uplink_spawn(pthread_t *thread, void *(*function)(void *), uplink_arg_t *arg);

void *uplink_thread(void *args);

int uplink_create(uplink_t *uplink, host_t *host, cookie_t *cookie);
