#pragma once

#include "../api/host.h"
#include <stdint.h>
#include <time.h>

typedef struct cookie_t {
	char *ptr;
	uint8_t len;
	uint8_t cap;
	time_t age;
} cookie_t;

int auth(host_t *host, cookie_t *cookie);
