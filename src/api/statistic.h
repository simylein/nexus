#pragma once

#include "../lib/request.h"
#include "../lib/response.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <time.h>

typedef struct statistic_t {
	atomic_uint_fast16_t packet_rx;
	atomic_uint_fast16_t packet_tx;
	atomic_uint_fast16_t packet_crc;
	atomic_uint_fast16_t packet_loss;
	atomic_uint_fast16_t packet_foreign;
	atomic_uint_fast32_t airtime_rx;
	atomic_uint_fast32_t airtime_tx;
} statistic_t;

typedef struct statistics_t {
	statistic_t data[25];
	atomic_int_fast64_t bucket;
} statistics_t;

extern struct statistics_t statistics;

void statistic_account(time_t time, bool rx, uint16_t rx_time, bool tx, uint16_t tx_time, bool crc, uint16_t loss,
											 bool foreign);
void statistic_find(request_t *request, response_t *response);
