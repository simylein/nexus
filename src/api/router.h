#pragma once

#include "../lib/octet.h"
#include "../lib/request.h"
#include "../lib/response.h"

void route(octet_t *db, request_t *request, response_t *response);
