#pragma once

#include "../lib/octet.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <stdint.h>
#include <time.h>

typedef struct user_t {
	uint8_t (*id)[16];
	char *username;
	uint8_t username_len;
	char *password;
	uint8_t password_len;
	time_t *signup_at;
	time_t *signin_at;
} user_t;

typedef struct user_row_t {
	uint8_t id;
	uint8_t username_len;
	uint8_t username;
	uint8_t password;
	uint8_t signup_at;
	uint8_t signin_at;
	uint8_t size;
} user_row_t;

extern const char *user_file;

extern const user_row_t user_row;

uint16_t user_insert(octet_t *db, user_t *user);
uint16_t user_update(octet_t *db, user_t *user);

void user_signin(octet_t *db, request_t *request, response_t *response);
