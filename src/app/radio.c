#include "../api/radio.h"
#include "../lib/error.h"
#include "../lib/logger.h"
#include "radio.h"
#include "sx1278.h"
#include <errno.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int radio_init(sqlite3 *database) {
	int status;
	sqlite3_stmt *stmt;

	const char *sql = "select "
										"radio.id, radio.device, radio.frequency, radio.bandwidth, "
										"radio.spreading_factor, radio.coding_rate, radio.tx_power, radio.sync_word, radio.checksum "
										"from radio "
										"order by device asc";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = -1;
		goto cleanup;
	}

	radio_t *radios = NULL;
	uint8_t radios_len = 0;
	while (true) {
		int result = sqlite3_step(stmt);
		if (result == SQLITE_ROW) {
			radios = realloc(radios, sizeof(radio_t) * (radios_len + 1));
			if (radios == NULL) {
				error("failed to allocate %zu bytes for radios because %s\n", sizeof(radio_t) * radios_len, errno_str());
				status = -1;
				goto cleanup;
			}
			const uint8_t *id = sqlite3_column_blob(stmt, 0);
			const size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
			if (id_len != sizeof(*((radio_t *)0)->id)) {
				error("id length %zu does not match buffer length %zu\n", id_len, sizeof(*((radio_t *)0)->id));
				status = 500;
				goto cleanup;
			}
			radios[radios_len].id = malloc(sizeof(*((radio_t *)0)->id));
			if (radios[radios_len].id == NULL) {
				error("failed to allocate %zu bytes for id because %s\n", sizeof(*((radio_t *)0)->id), errno_str());
				status = -1;
				goto cleanup;
			}
			memcpy(radios[radios_len].id, id, id_len);
			const uint8_t *device = sqlite3_column_text(stmt, 1);
			const size_t device_len = (uint8_t)sqlite3_column_bytes(stmt, 1);
			radios[radios_len].device = malloc(radios[radios_len].device_len);
			if (radios[radios_len].device == NULL) {
				error("failed to allocate %hhu bytes for device because %s\n", radios[radios_len].device_len, errno_str());
				status = -1;
				goto cleanup;
			}
			memcpy(radios[radios_len].device, device, radios[radios_len].device_len);
			radios[radios_len].device_len = (uint8_t)device_len;
			radios[radios_len].frequency = (uint32_t)sqlite3_column_int(stmt, 2);
			radios[radios_len].bandwidth = (uint32_t)sqlite3_column_int(stmt, 3);
			radios[radios_len].spreading_factor = (uint8_t)sqlite3_column_int(stmt, 4);
			radios[radios_len].coding_rate = (uint8_t)sqlite3_column_int(stmt, 5);
			radios[radios_len].tx_power = (uint8_t)sqlite3_column_int(stmt, 6);
			radios[radios_len].sync_word = (uint8_t)sqlite3_column_int(stmt, 7);
			radios[radios_len].checksum = (bool)sqlite3_column_int(stmt, 8);
			radios_len += 1;
		} else if (result == SQLITE_DONE) {
			status = 0;
			break;
		} else {
			error("failed to execute statement because %s\n", sqlite3_errmsg(database));
			status = 500;
			goto cleanup;
		}
	}

	pthread_t *threads = malloc(sizeof(pthread_t) * radios_len);
	if (threads == NULL) {
		error("failed to allocate %zu bytes for threads because %s\n", sizeof(pthread_t) * radios_len, errno_str());
		status = -1;
		goto cleanup;
	}

	for (uint8_t index = 0; index < radios_len; index++) {
		radio_spawn(&threads[index], radio_thread, &radios[index]);
	}

cleanup:
	sqlite3_finalize(stmt);
	return status;
}

int radio_spawn(pthread_t *thread, void *(*function)(void *), radio_t *radio) {
	trace("spawning radio thread %02x%02x\n", (*radio->id)[0], (*radio->id)[1]);

	radio_arg_t arg = {.radio = radio};

	int spawn_error = pthread_create(thread, NULL, function, (void *)&arg);
	if (spawn_error != 0) {
		errno = spawn_error;
		fatal("failed to spawn uplink thread because %s\n", errno_str());
		return -1;
	}

	int detach_error = pthread_detach(*thread);
	if (detach_error != 0) {
		errno = detach_error;
		fatal("failed to detach uplink thread because %s\n", errno_str());
		return -1;
	}

	return 0;
}

void *radio_thread(void *args) {
	radio_arg_t *arg = (radio_arg_t *)args;

	srand((unsigned int)time(NULL));

	if (sx1278_sleep(arg->fd) == -1) {
		error("failed to enable sleep mode\n");
	}

	if (sx1278_standby(arg->fd) == -1) {
		error("failed to enable standby mode\n");
	}

	if (sx1278_frequency(arg->fd, arg->radio->frequency) == -1) {
		error("failed to set radio frequency\n");
	}

	if (sx1278_tx_power(arg->fd, arg->radio->tx_power) == -1) {
		error("failed to set radio tx power\n");
	}

	if (sx1278_coding_rate(arg->fd, arg->radio->coding_rate) == -1) {
		error("failed to set radio coding rate\n");
	}

	if (sx1278_bandwidth(arg->fd, arg->radio->bandwidth) == -1) {
		error("failed to set radio bandwidth\n");
	}

	if (sx1278_spreading_factor(arg->fd, arg->radio->spreading_factor) == -1) {
		error("failed to set radio spreading factor\n");
	}

	if (sx1278_checksum(arg->fd, arg->radio->checksum) == -1) {
		error("failed to set radio checksum\n");
	}

	if (sx1278_sync_word(arg->fd, arg->radio->sync_word) == -1) {
		error("failed to set sync word\n");
	}

	while (true) {
		uint8_t rx_data[256];
		uint8_t rx_data_len = 0;
		if (sx1278_receive(arg->fd, &rx_data, &rx_data_len) == -1) {
			error("failed to receive packet\n");
			continue;
		}

		if (rx_data_len < 3) {
			debug("received packet without headers\n");
			continue;
		}

		int16_t rssi;
		if (sx1278_rssi(arg->fd, &rssi) == -1) {
			error("failed to read packet rssi\n");
			continue;
		}

		int8_t snr;
		if (sx1278_snr(arg->fd, &snr) == -1) {
			error("failed to read packet snr\n");
			continue;
		}

		rx("id %02x%02x kind %02x bytes %hhu rssi %hd snr %.2f sf %hhu\n", rx_data[0], rx_data[1], rx_data[2], rx_data_len, rssi,
			 snr / 4.0f, arg->radio->spreading_factor);

		uint8_t tx_data[256];
		uint8_t tx_data_len = 0;

		tx_data[tx_data_len] = rx_data[0];
		tx_data_len += 1;
		tx_data[tx_data_len] = rx_data[1];
		tx_data_len += 1;
		tx_data[tx_data_len] = 0x00;
		tx_data_len += 1;

		if (sx1278_transmit(arg->fd, &tx_data, tx_data_len) == -1) {
			error("failed to transmit packet\n");
			continue;
		}

		tx("id %02x%02x kind %02x bytes %hhu power %hhu sf %hhu\n", tx_data[0], tx_data[1], tx_data[2], tx_data_len,
			 arg->radio->tx_power, arg->radio->spreading_factor);
	}
}
