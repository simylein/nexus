#include "statistic.h"
#include "../lib/endian.h"
#include "../lib/logger.h"
#include "../lib/octet.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <fcntl.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

struct statistics_t statistics = {.bucket = 0};
const uint8_t statistics_size = sizeof(statistics.data) / sizeof(statistics.data[0]);

const char *statistic_file = "statistic";

const statistic_row_t statistic_row = {
		.packet_rx = 0,
		.packet_tx = 50,
		.packet_crc = 100,
		.packet_loss = 150,
		.packet_foreign = 200,
		.airtime_rx = 250,
		.airtime_tx = 350,
		.bucket = 450,
		.size = 458,
};

int statistic_init(octet_t *db) {
	int status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, statistic_file) == -1) {
		error("failed to sprintf to file\n");
		return -1;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDONLY, F_RDLCK) == -1) {
		status = -1;
		goto cleanup;
	}

	if (stmt.stat.st_size > db->table_len) {
		error("file length %zu exceeds buffer length %u\n", (size_t)stmt.stat.st_size, db->table_len);
		status = -1;
		goto cleanup;
	}

	if (stmt.stat.st_size == 0) {
		debug("file %s is empty\n", statistic_file);
		status = 0;
		goto cleanup;
	}

	if (stmt.stat.st_size != statistic_row.size) {
		error("file length %zu does not match %u\n", (size_t)stmt.stat.st_size, statistic_row.size);
		status = -1;
		goto cleanup;
	}

	debug("select statistics\n");

	if (octet_row_read(&stmt, file, 0, db->table, statistic_row.size) == -1) {
		status = -1;
		goto cleanup;
	}

	uint8_t index = 0;
	while (true) {
		if (index >= statistics_size) {
			status = 0;
			break;
		}
		uint16_t packet_rx = octet_uint16_read(db->table, (uint16_t)(statistic_row.packet_rx + index * sizeof(uint16_t)));
		uint16_t packet_tx = octet_uint16_read(db->table, (uint16_t)(statistic_row.packet_tx + index * sizeof(uint16_t)));
		uint16_t packet_crc = octet_uint16_read(db->table, (uint16_t)(statistic_row.packet_crc + index * sizeof(uint16_t)));
		uint16_t packet_loss = octet_uint16_read(db->table, (uint16_t)(statistic_row.packet_loss + index * sizeof(uint16_t)));
		uint16_t packet_foreign = octet_uint16_read(db->table, (uint16_t)(statistic_row.packet_foreign + index * sizeof(uint16_t)));
		uint32_t airtime_rx = octet_uint32_read(db->table, (uint16_t)(statistic_row.airtime_rx + index * sizeof(uint32_t)));
		uint32_t airtime_tx = octet_uint32_read(db->table, (uint16_t)(statistic_row.airtime_tx + index * sizeof(uint32_t)));
		atomic_store_explicit(&statistics.data[index].packet_rx, packet_rx, memory_order_relaxed);
		atomic_store_explicit(&statistics.data[index].packet_tx, packet_tx, memory_order_relaxed);
		atomic_store_explicit(&statistics.data[index].packet_crc, packet_crc, memory_order_relaxed);
		atomic_store_explicit(&statistics.data[index].packet_loss, packet_loss, memory_order_relaxed);
		atomic_store_explicit(&statistics.data[index].packet_foreign, packet_foreign, memory_order_relaxed);
		atomic_store_explicit(&statistics.data[index].airtime_rx, airtime_rx, memory_order_relaxed);
		atomic_store_explicit(&statistics.data[index].airtime_tx, airtime_tx, memory_order_relaxed);
		index += 1;
	}
	time_t bucket = (time_t)octet_uint64_read(db->table, statistic_row.bucket);
	atomic_store_explicit(&statistics.bucket, bucket, memory_order_relaxed);

cleanup:
	octet_close(&stmt, file);
	return status;
}

void statistic_account(time_t now, bool rx, uint16_t rx_time, bool tx, uint16_t tx_time, bool crc, uint16_t loss,
											 bool foreign) {
	int64_t bucket = (now / 3600) * 3600;
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

int statistic_save(octet_t *db) {
	int status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, statistic_file) == -1) {
		error("failed to sprintf to file\n");
		return -1;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = -1;
		goto cleanup;
	}

	if (statistic_row.size > db->table_len) {
		error("file length %hu exceeds buffer length %u\n", statistic_row.size, db->table_len);
		status = -1;
		goto cleanup;
	}

	debug("insert statistics\n");

	uint8_t index = 0;
	while (true) {
		if (index >= statistics_size) {
			status = 0;
			break;
		}
		uint16_t packet_rx = (uint16_t)atomic_load_explicit(&statistics.data[index].packet_rx, memory_order_relaxed);
		uint16_t packet_tx = (uint16_t)atomic_load_explicit(&statistics.data[index].packet_tx, memory_order_relaxed);
		uint16_t packet_crc = (uint16_t)atomic_load_explicit(&statistics.data[index].packet_crc, memory_order_relaxed);
		uint16_t packet_loss = (uint16_t)atomic_load_explicit(&statistics.data[index].packet_loss, memory_order_relaxed);
		uint16_t packet_foreign = (uint16_t)atomic_load_explicit(&statistics.data[index].packet_foreign, memory_order_relaxed);
		uint32_t airtime_rx = (uint32_t)atomic_load_explicit(&statistics.data[index].airtime_rx, memory_order_relaxed);
		uint32_t airtime_tx = (uint32_t)atomic_load_explicit(&statistics.data[index].airtime_tx, memory_order_relaxed);
		octet_uint16_write(db->table, (uint16_t)(statistic_row.packet_rx + index * sizeof(uint16_t)), packet_rx);
		octet_uint16_write(db->table, (uint16_t)(statistic_row.packet_tx + index * sizeof(uint16_t)), packet_tx);
		octet_uint16_write(db->table, (uint16_t)(statistic_row.packet_crc + index * sizeof(uint16_t)), packet_crc);
		octet_uint16_write(db->table, (uint16_t)(statistic_row.packet_loss + index * sizeof(uint16_t)), packet_loss);
		octet_uint16_write(db->table, (uint16_t)(statistic_row.packet_foreign + index * sizeof(uint16_t)), packet_foreign);
		octet_uint32_write(db->table, (uint16_t)(statistic_row.airtime_rx + index * sizeof(uint32_t)), airtime_rx);
		octet_uint32_write(db->table, (uint16_t)(statistic_row.airtime_tx + index * sizeof(uint32_t)), airtime_tx);
		index += 1;
	}
	time_t bucket = atomic_load_explicit(&statistics.bucket, memory_order_relaxed);
	octet_uint64_write(db->table, statistic_row.bucket, (uint64_t)bucket);

	if (octet_row_write(&stmt, file, 0, db->table, statistic_row.size) == -1) {
		status = -1;
		goto cleanup;
	}

	if (octet_trunc(&stmt, file, statistic_row.size) == -1) {
		status = -1;
		goto cleanup;
	}

cleanup:
	octet_close(&stmt, file);
	return status;
}
