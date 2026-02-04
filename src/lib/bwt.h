#pragma once

#include <stdint.h>
#include <time.h>

typedef struct bwt_t {
	uint8_t id[16];
	time_t iat;
	time_t exp;
} bwt_t;

int bwt_sign(char (*buffer)[103], uint8_t (*id)[16]);
int bwt_verify(const char *cookie, const size_t cookie_len, bwt_t *bwt);
