#pragma once

#include "../lib/request.h"
#include "../lib/response.h"
#include <stdint.h>

int fetch(const char *address, uint16_t port, request_t *request, response_t *response);
