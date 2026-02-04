#pragma once

#include "octet.h"
#include <arpa/inet.h>

void handle(octet_t *db, char *request_buffer, char *response_buffer, int *client_sock, struct sockaddr_in *client_addr);
