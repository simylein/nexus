#pragma once

#include "host.h"
#include <stdint.h>
#include <time.h>

int auth(host_t *host, char (*cookie)[128], uint8_t *cookie_len, time_t *cookie_age);
