#include "../api/radio.h"
#include "../app/downlink.h"
#include "../app/uplink.h"
#include "../lib/config.h"
#include "../lib/error.h"
#include "../lib/logger.h"
#include "airtime.h"
#include "radio.h"
#include "spi.h"
#include "sx1278.h"
#include <errno.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int radio_init(sqlite3 *database) {
	int status;
	sqlite3_stmt *stmt_radio = NULL;
	sqlite3_stmt *stmt_device = NULL;

	const char *sql_radio = "select "
													"radio.id, radio.device, radio.frequency, radio.bandwidth, "
													"radio.spreading_factor, radio.coding_rate, radio.tx_power, radio.sync_word, radio.checksum "
													"from radio "
													"order by device asc";
	debug("%s\n", sql_radio);

	if (sqlite3_prepare_v2(database, sql_radio, -1, &stmt_radio, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = -1;
		goto cleanup;
	}

	radio_t *radios = NULL;
	uint8_t radios_len = 0;
	while (true) {
		int result = sqlite3_step(stmt_radio);
		if (result == SQLITE_ROW) {
			radios = realloc(radios, sizeof(radio_t) * (radios_len + 1));
			if (radios == NULL) {
				error("failed to allocate %zu bytes for radios because %s\n", sizeof(radio_t) * (radios_len + 1), errno_str());
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
			radios[radios_len].id = malloc(sizeof(*((radio_t *)0)->id));
			if (radios[radios_len].id == NULL) {
				error("failed to allocate %zu bytes for id because %s\n", sizeof(*((radio_t *)0)->id), errno_str());
				status = -1;
				goto cleanup;
			}
			memcpy(radios[radios_len].id, id, id_len);
			const uint8_t *device = sqlite3_column_text(stmt_radio, 1);
			const size_t device_len = (uint8_t)sqlite3_column_bytes(stmt_radio, 1);
			radios[radios_len].device = malloc(device_len);
			if (radios[radios_len].device == NULL) {
				error("failed to allocate %zu bytes for device because %s\n", device_len, errno_str());
				status = -1;
				goto cleanup;
			}
			memcpy(radios[radios_len].device, device, device_len);
			radios[radios_len].device_len = (uint8_t)device_len;
			radios[radios_len].frequency = (uint32_t)sqlite3_column_int(stmt_radio, 2);
			radios[radios_len].bandwidth = (uint32_t)sqlite3_column_int(stmt_radio, 3);
			radios[radios_len].spreading_factor = (uint8_t)sqlite3_column_int(stmt_radio, 4);
			radios[radios_len].coding_rate = (uint8_t)sqlite3_column_int(stmt_radio, 5);
			radios[radios_len].tx_power = (uint8_t)sqlite3_column_int(stmt_radio, 6);
			radios[radios_len].sync_word = (uint8_t)sqlite3_column_int(stmt_radio, 7);
			radios[radios_len].checksum = (bool)sqlite3_column_int(stmt_radio, 8);
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

	uint8_t *device_ids = NULL;
	uint8_t *device_tags = NULL;
	device_t *devices = NULL;
	uint8_t devices_len = 0;
	while (true) {
		int result = sqlite3_step(stmt_device);
		if (result == SQLITE_ROW) {
			devices = realloc(devices, sizeof(device_t) * (devices_len + 1));
			if (devices == NULL) {
				error("failed to allocate %zu bytes for devices because %s\n", sizeof(device_t) * (devices_len + 1), errno_str());
				status = -1;
				goto cleanup;
			}
			device_ids = realloc(device_ids, sizeof(*((device_t *)0)->id) * (devices_len + 1));
			if (device_ids == NULL) {
				error("failed to allocate %zu bytes for device ids because %s\n", sizeof(*((device_t *)0)->id) * (devices_len + 1),
							errno_str());
				status = -1;
				goto cleanup;
			}
			devices[devices_len].id = (uint8_t (*)[16])(&device_ids[devices_len * sizeof(*((device_t *)0)->id)]);
			device_tags = realloc(device_tags, sizeof(*((device_t *)0)->tag) * (devices_len + 1));
			if (device_tags == NULL) {
				error("failed to allocate %zu bytes for device ids because %s\n", sizeof(*((device_t *)0)->tag) * (devices_len + 1),
							errno_str());
				status = -1;
				goto cleanup;
			}
			devices[devices_len].tag = (uint8_t (*)[2])(&device_tags[devices_len * sizeof(*((device_t *)0)->tag)]);
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
			memcpy(devices[devices_len].id, id, id_len);
			memcpy(devices[devices_len].tag, tag, tag_len);
			devices_len += 1;
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

	radio_arg_t *args = malloc(sizeof(radio_arg_t) * radios_len);
	if (args == NULL) {
		error("failed to allocate %zu bytes for args because %s\n", sizeof(radio_arg_t) * radios_len, errno_str());
		status = -1;
		goto cleanup;
	}

	for (uint8_t index = 0; index < radios_len; index++) {
		char device[64];
		sprintf(device, "%.*s", (int)radios[index].device_len, radios[index].device);
		if ((args[index].fd = spi_init(device, 0, 8 * 1000 * 1000, 8)) == -1) {
			return -1;
		}

		args[index].radio = &radios[index];
		args[index].devices = devices;
		args[index].devices_len = devices_len;
		if (radio_spawn(&threads[index], radio_thread, &args[index]) == -1) {
			return -1;
		}
	}

	info("spawned %hhu radio threads\n", radios_len);

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

		device_t *device = NULL;
		for (uint8_t ind = 0; ind < arg->devices_len; ind++) {
			if (memcmp(&rx_data[0], arg->devices[ind].tag, sizeof(*arg->devices[ind].tag)) == 0) {
				device = &arg->devices[ind];
			}
		}

		if (device == NULL) {
			debug("no registration for device %02x%02x\n", rx_data[0], rx_data[1]);
			continue;
		}

		uplink_t uplink;
		uplink.kind = rx_data[2];
		memcpy(uplink.data, &rx_data[3], rx_data_len - 3);
		uplink.data_len = rx_data_len - 3;
		uplink.airtime = airtime_calculate(arg->radio, rx_data_len);
		uplink.frequency = arg->radio->frequency;
		uplink.bandwidth = arg->radio->bandwidth;
		uplink.rssi = rssi;
		uplink.snr = snr;
		uplink.spreading_factor = arg->radio->spreading_factor;
		uplink.tx_power = arg->radio->tx_power;
		uplink.received_at = time(NULL);
		memcpy(uplink.device_id, device->id, sizeof(*device->id));

		pthread_mutex_lock(&uplinks.lock);

		memcpy(&uplinks.ptr[uplinks.tail], &uplink, sizeof(uplink));
		uplinks.tail = (uint8_t)((uplinks.tail + 1) % uplinks_size);
		uplinks.size++;
		trace("radio thread increased uplinks size to %hhu\n", uplinks.size);

		pthread_cond_signal(&uplinks.filled);
		pthread_mutex_unlock(&uplinks.lock);

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

		downlink_t downlink;
		downlink.kind = tx_data[2];
		memcpy(downlink.data, &tx_data[3], tx_data_len - 3);
		downlink.data_len = tx_data_len - 3;
		downlink.airtime = airtime_calculate(arg->radio, tx_data_len);
		downlink.frequency = arg->radio->frequency;
		downlink.bandwidth = arg->radio->bandwidth;
		downlink.spreading_factor = arg->radio->spreading_factor;
		downlink.tx_power = arg->radio->tx_power;
		downlink.sent_at = time(NULL);
		memcpy(downlink.device_id, device->id, sizeof(*device->id));

		pthread_mutex_lock(&downlinks.lock);

		memcpy(&downlinks.ptr[downlinks.tail], &downlink, sizeof(downlink));
		downlinks.tail = (uint8_t)((downlinks.tail + 1) % downlinks_size);
		downlinks.size++;
		trace("radio thread increased downlinks size to %hhu\n", downlinks.size);

		pthread_cond_signal(&downlinks.filled);
		pthread_mutex_unlock(&downlinks.lock);
	}
}
