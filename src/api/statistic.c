#include "statistic.h"
#include "../lib/endian.h"
#include "../lib/logger.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

struct statistics_t statistics = {.bucket = 0};
const uint8_t statistics_size = sizeof(statistics.data) / sizeof(statistics.data[0]);

void statistic_account(time_t time, bool rx, uint16_t rx_time, bool tx, uint16_t tx_time, bool crc, uint16_t loss,
											 bool foreign) {
	int64_t bucket = (time / 3600) * 3600;
	int64_t bucket_old = atomic_load_explicit(&statistics.bucket, memory_order_acquire);

	int32_t diff = (int32_t)((bucket - bucket_old) / 3600);
	if (diff < 0) {
		return;
	}

	if (bucket != bucket_old && atomic_compare_exchange_strong_explicit(&statistics.bucket, &bucket_old, bucket,
																																			memory_order_acq_rel, memory_order_acquire)) {
		if (diff > statistics_size) {
			diff = statistics_size;
		}

		for (uint8_t index = 0; index < statistics_size - diff; index++) {
			atomic_store_explicit(&statistics.data[index].packet_rx,
														atomic_load_explicit(&statistics.data[index + diff].packet_rx, memory_order_relaxed),
														memory_order_relaxed);
			atomic_store_explicit(&statistics.data[index].packet_tx,
														atomic_load_explicit(&statistics.data[index + diff].packet_tx, memory_order_relaxed),
														memory_order_relaxed);
			atomic_store_explicit(&statistics.data[index].packet_crc,
														atomic_load_explicit(&statistics.data[index + diff].packet_crc, memory_order_relaxed),
														memory_order_relaxed);
			atomic_store_explicit(&statistics.data[index].packet_loss,
														atomic_load_explicit(&statistics.data[index + diff].packet_loss, memory_order_relaxed),
														memory_order_relaxed);
			atomic_store_explicit(&statistics.data[index].packet_foreign,
														atomic_load_explicit(&statistics.data[index + diff].packet_foreign, memory_order_relaxed),
														memory_order_relaxed);
			atomic_store_explicit(&statistics.data[index].airtime_rx,
														atomic_load_explicit(&statistics.data[index + diff].airtime_rx, memory_order_relaxed),
														memory_order_relaxed);
			atomic_store_explicit(&statistics.data[index].airtime_tx,
														atomic_load_explicit(&statistics.data[index + diff].airtime_tx, memory_order_relaxed),
														memory_order_relaxed);
		}

		for (uint8_t index = statistics_size - (uint8_t)diff; index < statistics_size; index++) {
			atomic_store_explicit(&statistics.data[index].packet_rx, 0, memory_order_relaxed);
			atomic_store_explicit(&statistics.data[index].packet_tx, 0, memory_order_relaxed);
			atomic_store_explicit(&statistics.data[index].packet_crc, 0, memory_order_relaxed);
			atomic_store_explicit(&statistics.data[index].packet_loss, 0, memory_order_relaxed);
			atomic_store_explicit(&statistics.data[index].packet_foreign, 0, memory_order_relaxed);
			atomic_store_explicit(&statistics.data[index].airtime_rx, 0, memory_order_relaxed);
			atomic_store_explicit(&statistics.data[index].airtime_tx, 0, memory_order_relaxed);
		}
	}

	if (rx == true) {
		atomic_fetch_add_explicit(&statistics.data[statistics_size - 1].packet_rx, 1, memory_order_relaxed);
		atomic_fetch_add_explicit(&statistics.data[statistics_size - 1].airtime_rx, rx_time, memory_order_relaxed);
	}
	if (tx == true) {
		atomic_fetch_add_explicit(&statistics.data[statistics_size - 1].packet_tx, 1, memory_order_relaxed);
		atomic_fetch_add_explicit(&statistics.data[statistics_size - 1].airtime_tx, tx_time, memory_order_relaxed);
	}
	if (crc == true) {
		atomic_fetch_add_explicit(&statistics.data[statistics_size - 1].packet_crc, 1, memory_order_relaxed);
	}
	if (loss != 0) {
		atomic_fetch_add_explicit(&statistics.data[statistics_size - 1].packet_loss, loss, memory_order_relaxed);
	}
	if (foreign == true) {
		atomic_fetch_add_explicit(&statistics.data[statistics_size - 1].packet_foreign, 1, memory_order_relaxed);
	}
}

void statistic_find(request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	time_t age = time(NULL) - atomic_load_explicit(&statistics.bucket, memory_order_acquire);
	if (age < 0) {
		age = 0;
	}

	uint8_t statistics_len = 0;
	uint32_t start_index = (uint32_t)(age / 3600);
	for (uint8_t index = 0; index < statistics_size; index++) {
		uint32_t ind = index + start_index;
		if (ind < statistics_size) {
			uint16_t packet_rx = (uint16_t)atomic_load_explicit(&statistics.data[ind].packet_rx, memory_order_relaxed);
			uint16_t packet_tx = (uint16_t)atomic_load_explicit(&statistics.data[ind].packet_tx, memory_order_relaxed);
			uint16_t packet_crc = (uint16_t)atomic_load_explicit(&statistics.data[ind].packet_crc, memory_order_relaxed);
			uint16_t packet_loss = (uint16_t)atomic_load_explicit(&statistics.data[ind].packet_loss, memory_order_relaxed);
			uint16_t packet_foreign = (uint16_t)atomic_load_explicit(&statistics.data[ind].packet_foreign, memory_order_relaxed);
			uint32_t airtime_rx = (uint32_t)atomic_load_explicit(&statistics.data[ind].airtime_rx, memory_order_relaxed);
			uint32_t airtime_tx = (uint32_t)atomic_load_explicit(&statistics.data[ind].airtime_tx, memory_order_relaxed);
			body_write(response, (uint16_t[]){hton16(packet_rx)}, sizeof(packet_rx));
			body_write(response, (uint16_t[]){hton16(packet_tx)}, sizeof(packet_tx));
			body_write(response, (uint16_t[]){hton16(packet_crc)}, sizeof(packet_crc));
			body_write(response, (uint16_t[]){hton16(packet_loss)}, sizeof(packet_loss));
			body_write(response, (uint16_t[]){hton16(packet_foreign)}, sizeof(packet_foreign));
			body_write(response, (uint32_t[]){hton32(airtime_rx)}, sizeof(airtime_rx));
			body_write(response, (uint32_t[]){hton32(airtime_tx)}, sizeof(airtime_tx));
			statistics_len++;
		} else {
			uint8_t buffer[18] = {0};
			body_write(response, &buffer, sizeof(buffer));
		}
	}

	info("found %hhu statistics\n", statistics_len);
	response->status = 200;
}
