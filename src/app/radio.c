#include "../api/radio.h"
#include "../api/database.h"
#include "../app/downlink.h"
#include "../app/uplink.h"
#include "../lib/config.h"
#include "../lib/error.h"
#include "../lib/logger.h"
#include "airtime.h"
#include "radio.h"
#include "schedule.h"
#include "spi.h"
#include "sx1278.h"
#include <errno.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

comms_t comms = {
		.radios = NULL,
		.radios_len = 0,
		.devices = NULL,
		.devices_len = 0,
};

int radio_init(sqlite3 *database) {
	int status;
	sqlite3_stmt *stmt_radio = NULL;
	sqlite3_stmt *stmt_device = NULL;

	const char *sql_radio = "select "
													"radio.id, radio.device, radio.frequency, radio.bandwidth, "
													"radio.spreading_factor, radio.coding_rate, radio.tx_power, "
													"radio.preamble_len, radio.sync_word, radio.checksum "
													"from radio "
													"order by device asc";
	debug("%s\n", sql_radio);

	if (sqlite3_prepare_v2(database, sql_radio, -1, &stmt_radio, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = -1;
		goto cleanup;
	}

	while (true) {
		int result = sqlite3_step(stmt_radio);
		if (result == SQLITE_ROW) {
			comms.radios = realloc(comms.radios, sizeof(radio_t) * (comms.radios_len + 1));
			if (comms.radios == NULL) {
				error("failed to allocate %zu bytes for radios because %s\n", sizeof(radio_t) * (comms.radios_len + 1), errno_str());
				status = -1;
				goto cleanup;
			}
			const uint8_t *id = sqlite3_column_blob(stmt_radio, 0);
			const size_t id_len = (size_t)sqlite3_column_bytes(stmt_radio, 0);
			if (id_len != sizeof(*((radio_t *)0)->id)) {
				error("id length %zu does not match buffer length %zu\n", id_len, sizeof(*((radio_t *)0)->id));
				status = 500;
				goto cleanup;
			}
			comms.radios[comms.radios_len].id = malloc(sizeof(*((radio_t *)0)->id));
			if (comms.radios[comms.radios_len].id == NULL) {
				error("failed to allocate %zu bytes for id because %s\n", sizeof(*((radio_t *)0)->id), errno_str());
				status = -1;
				goto cleanup;
			}
			memcpy(comms.radios[comms.radios_len].id, id, id_len);
			const uint8_t *device = sqlite3_column_text(stmt_radio, 1);
			const size_t device_len = (uint8_t)sqlite3_column_bytes(stmt_radio, 1);
			comms.radios[comms.radios_len].device = malloc(device_len);
			if (comms.radios[comms.radios_len].device == NULL) {
				error("failed to allocate %zu bytes for device because %s\n", device_len, errno_str());
				status = -1;
				goto cleanup;
			}
			memcpy(comms.radios[comms.radios_len].device, device, device_len);
			comms.radios[comms.radios_len].device_len = (uint8_t)device_len;
			comms.radios[comms.radios_len].frequency = (uint32_t)sqlite3_column_int(stmt_radio, 2);
			comms.radios[comms.radios_len].bandwidth = (uint32_t)sqlite3_column_int(stmt_radio, 3);
			comms.radios[comms.radios_len].spreading_factor = (uint8_t)sqlite3_column_int(stmt_radio, 4);
			comms.radios[comms.radios_len].coding_rate = (uint8_t)sqlite3_column_int(stmt_radio, 5);
			comms.radios[comms.radios_len].tx_power = (uint8_t)sqlite3_column_int(stmt_radio, 6);
			comms.radios[comms.radios_len].preamble_len = (uint8_t)sqlite3_column_int(stmt_radio, 7);
			comms.radios[comms.radios_len].sync_word = (uint8_t)sqlite3_column_int(stmt_radio, 8);
			comms.radios[comms.radios_len].checksum = (bool)sqlite3_column_int(stmt_radio, 9);
			comms.radios_len += 1;
		} else if (result == SQLITE_DONE) {
			status = 0;
			break;
		} else {
			status = database_error(database, result);
			goto cleanup;
		}
	}

	const char *sql_device = "select "
													 "device.id, device.tag "
													 "from device "
													 "order by tag asc";
	debug("%s\n", sql_device);

	if (sqlite3_prepare_v2(database, sql_device, -1, &stmt_device, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = -1;
		goto cleanup;
	}

	while (true) {
		int result = sqlite3_step(stmt_device);
		if (result == SQLITE_ROW) {
			comms.devices = realloc(comms.devices, sizeof(device_t) * (comms.devices_len + 1));
			if (comms.devices == NULL) {
				error("failed to allocate %zu bytes for devices because %s\n", sizeof(device_t) * (comms.devices_len + 1), errno_str());
				status = -1;
				goto cleanup;
			}
			comms.devices[comms.devices_len].id = malloc(sizeof(*((device_t *)0)->id));
			if (comms.devices[comms.devices_len].id == NULL) {
				error("failed to allocate %zu bytes for id because %s\n", sizeof(*((device_t *)0)->id), errno_str());
				status = -1;
				goto cleanup;
			}
			comms.devices[comms.devices_len].tag = malloc(sizeof(*((device_t *)0)->tag));
			if (comms.devices[comms.devices_len].tag == NULL) {
				error("failed to allocate %zu bytes for tag because %s\n", sizeof(*((device_t *)0)->tag), errno_str());
				status = -1;
				goto cleanup;
			}
			const uint8_t *id = sqlite3_column_blob(stmt_device, 0);
			const size_t id_len = (size_t)sqlite3_column_bytes(stmt_device, 0);
			if (id_len != sizeof(*((device_t *)0)->id)) {
				error("id length %zu does not match buffer length %zu\n", id_len, sizeof(*((device_t *)0)->id));
				status = 500;
				goto cleanup;
			}
			const uint8_t *tag = sqlite3_column_blob(stmt_device, 1);
			const size_t tag_len = (size_t)sqlite3_column_bytes(stmt_device, 1);
			if (tag_len != sizeof(*((device_t *)0)->tag)) {
				error("id length %zu does not match buffer length %zu\n", tag_len, sizeof(*((device_t *)0)->tag));
				status = 500;
				goto cleanup;
			}
			memcpy(comms.devices[comms.devices_len].id, id, id_len);
			memcpy(comms.devices[comms.devices_len].tag, tag, tag_len);
			comms.devices_len += 1;
		} else if (result == SQLITE_DONE) {
			status = 0;
			break;
		} else {
			status = database_error(database, result);
			goto cleanup;
		}
	}

	comms.workers = malloc(sizeof(radio_worker_t) * comms.radios_len);
	if (comms.workers == NULL) {
		error("failed to allocate %zu bytes for workers because %s\n", sizeof(radio_worker_t) * comms.radios_len, errno_str());
		status = -1;
		goto cleanup;
	}

	for (uint8_t index = 0; index < comms.radios_len; index++) {
		char device[64];
		sprintf(device, "%.*s", (int)comms.radios[index].device_len, comms.radios[index].device);
		if ((comms.workers[index].arg.fd = spi_init(device, 0, 8 * 1000 * 1000, 8)) == -1) {
			return -1;
		}

		comms.workers[index].arg.radio = &comms.radios[index];
		comms.workers[index].arg.devices = comms.devices;
		comms.workers[index].arg.devices_len = comms.devices_len;
		if (radio_spawn(&comms.workers[index].thread, radio_thread, &comms.workers[index].arg) == -1) {
			return -1;
		}
	}

	info("spawned %hhu radio threads\n", comms.radios_len);

cleanup:
	sqlite3_finalize(stmt_radio);
	sqlite3_finalize(stmt_device);
	return status;
}

int radio_spawn(pthread_t *thread, void *(*function)(void *), radio_arg_t *arg) {
	trace("spawning radio thread %02x%02x\n", (*arg->radio->id)[0], (*arg->radio->id)[1]);

	int spawn_error = pthread_create(thread, NULL, function, (void *)arg);
	if (spawn_error != 0) {
		errno = spawn_error;
		fatal("failed to spawn uplink thread because %s\n", errno_str());
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

	if (sx1278_preamble_length(arg->fd, arg->radio->preamble_len) == -1) {
		error("failed to set radio preamble length\n");
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

		if (rx_data_len < 4) {
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

		rx("id %02x%02x kind %02x bytes %hhu rssi %hd snr %.2f sf %hhu power %hhu\n", rx_data[0], rx_data[1], rx_data[3],
			 rx_data_len, rssi, snr / 4.0f, arg->radio->spreading_factor, ((rx_data[2] >> 4) & 0x0f) + 2);

		device_t *device = NULL;
		for (uint8_t ind = 0; ind < arg->devices_len; ind++) {
			if (memcmp(&rx_data[0], arg->devices[ind].tag, sizeof(*arg->devices[ind].tag)) == 0) {
				device = &arg->devices[ind];
				break;
			}
		}

		if (device == NULL) {
			debug("no registration for device %02x%02x\n", rx_data[0], rx_data[1]);
			continue;
		}

		uplink_t uplink;
		uplink.kind = rx_data[3];
		memcpy(uplink.data, &rx_data[4], rx_data_len - 4);
		uplink.data_len = rx_data_len - 4;
		uplink.airtime = airtime_calculate(arg->radio, rx_data_len);
		uplink.frequency = arg->radio->frequency;
		uplink.bandwidth = arg->radio->bandwidth;
		uplink.rssi = rssi;
		uplink.snr = snr;
		uplink.spreading_factor = arg->radio->spreading_factor;
		uplink.tx_power = ((rx_data[2] >> 4) & 0x0f) + 2;
		uplink.preamble_len = (rx_data[2] & 0x0f) + 6;
		uplink.received_at = time(NULL);
		memcpy(uplink.device_id, device->id, sizeof(*device->id));

		pthread_mutex_lock(&uplinks.lock);

		while (uplinks.size >= uplinks_size) {
			warn("waiting for uplinks size %hhu to decrease\n", uplinks.size);
			pthread_cond_wait(&uplinks.available, &uplinks.lock);
		}

		memcpy(&uplinks.ptr[uplinks.tail], &uplink, sizeof(uplink));
		uplinks.tail = (uint8_t)((uplinks.tail + 1) % uplinks_size);
		uplinks.size++;
		trace("radio thread increased uplinks size to %hhu\n", uplinks.size);

		pthread_cond_signal(&uplinks.filled);
		pthread_mutex_unlock(&uplinks.lock);

		if (sx1278_standby(arg->fd) == -1) {
			error("failed to enable standby mode\n");
		}

		if (arg->radio->tx_power != ((rx_data[2] >> 4) & 0x0f) + 2) {
			arg->radio->tx_power = ((rx_data[2] >> 4) & 0x0f) + 2;
			if (sx1278_tx_power(arg->fd, arg->radio->tx_power) == -1) {
				error("failed to set radio tx power\n");
			}
		}

		if (arg->radio->preamble_len != (rx_data[2] & 0x0f) + 6) {
			arg->radio->preamble_len = (rx_data[2] & 0x0f) + 6;
			if (sx1278_preamble_length(arg->fd, arg->radio->preamble_len) == -1) {
				error("failed to set radio preamble length\n");
			}
		}

		uint8_t tx_data[256];
		uint8_t tx_data_len = 0;

		tx_data[tx_data_len] = rx_data[0];
		tx_data_len += sizeof(rx_data[0]);
		tx_data[tx_data_len] = rx_data[1];
		tx_data_len += sizeof(rx_data[1]);
		tx_data[tx_data_len] = (uint8_t)((((arg->radio->tx_power - 2) << 4) & 0xf0) | ((arg->radio->preamble_len - 6) & 0x0f));
		tx_data_len += sizeof(tx_data[tx_data_len]);
		schedule_t schedule;
		if (schedule_find(&schedule, (uint8_t (*)[2])(&rx_data[0])) == 0) {
			tx_data[tx_data_len] = schedule.kind;
			tx_data_len += sizeof(schedule.kind);
			memcpy(&tx_data[tx_data_len], schedule.data, schedule.data_len);
			tx_data_len += schedule.data_len;
		} else {
			tx_data[tx_data_len] = 0x00;
			tx_data_len += sizeof(uint8_t);
		}

		if (sx1278_transmit(arg->fd, &tx_data, tx_data_len) == -1) {
			error("failed to transmit packet\n");
			continue;
		}

		tx("id %02x%02x kind %02x bytes %hhu sf %hhu power %hhu\n", tx_data[0], tx_data[1], tx_data[3], tx_data_len,
			 arg->radio->spreading_factor, ((tx_data[2] >> 4) & 0x0f) + 2);

		downlink_t downlink;
		downlink.kind = tx_data[3];
		memcpy(downlink.data, &tx_data[4], tx_data_len - 4);
		downlink.data_len = tx_data_len - 4;
		downlink.airtime = airtime_calculate(arg->radio, tx_data_len);
		downlink.frequency = arg->radio->frequency;
		downlink.bandwidth = arg->radio->bandwidth;
		downlink.spreading_factor = arg->radio->spreading_factor;
		downlink.tx_power = ((tx_data[2] >> 4) & 0x0f) + 2;
		downlink.preamble_len = (tx_data[2] & 0x0f) + 6;
		downlink.sent_at = time(NULL);
		memcpy(downlink.device_id, device->id, sizeof(*device->id));

		pthread_mutex_lock(&downlinks.lock);

		while (downlinks.size >= downlinks_size) {
			warn("waiting for downlinks size %hhu to decrease\n", downlinks.size);
			pthread_cond_wait(&downlinks.available, &downlinks.lock);
		}

		memcpy(&downlinks.ptr[downlinks.tail], &downlink, sizeof(downlink));
		downlinks.tail = (uint8_t)((downlinks.tail + 1) % downlinks_size);
		downlinks.size++;
		trace("radio thread increased downlinks size to %hhu\n", downlinks.size);

		pthread_cond_signal(&downlinks.filled);
		pthread_mutex_unlock(&downlinks.lock);
	}
}
