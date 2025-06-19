#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef struct request_t {
	char *method;
	uint8_t method_len;
	char *pathname;
	uint8_t pathname_len;
	char *search;
	uint8_t search_len;
	char *protocol;
	uint8_t protocol_len;
	char body[16384];
	size_t body_len;
} request_t;

typedef struct response_t {
	uint16_t status;
	char *status_text;
	uint8_t status_text_len;
	char *protocol;
	uint8_t protocol_len;
} response_t;

int fetch(const char *address, uint16_t port, request_t *request, response_t *response);

void append_body(request_t *request, const void *buffer, size_t buffer_len);
