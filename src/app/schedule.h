#pragma once

#include <pthread.h>
#include <stdint.h>

typedef struct schedule_t {
	uint8_t kind;
	uint8_t data[256];
	uint8_t data_len;
	uint8_t device_id[16];
	uint8_t device_tag[2];
} schedule_t;

typedef struct schedules_t {
	schedule_t *ptr;
	uint8_t len;
	uint8_t cap;
	pthread_mutex_t lock;
} schedules_t;

extern struct schedules_t schedules;

int schedule_init(void);

int schedule_push(schedule_t *schedule);

int schedule_find(schedule_t *schedule, uint8_t (*device_tag)[2]);
