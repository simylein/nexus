#include "../api/radio.h"
#include "../api/downlink.h"
#include "../api/transmission.h"
#include "../api/uplink.h"
#include "../lib/config.h"
#include "../lib/error.h"
#include "../lib/logger.h"
#include "../lib/octet.h"
#include "../lib/response.h"
#include "../lib/ssc128.h"
#include "airtime.h"
#include "downlink.h"
#include "gpio.h"
#include "radio.h"
#include "schedule.h"
#include "spi.h"
#include "sx1278.h"
#include "uplink.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

comms_t comms = {
		.radios = NULL,
		.radios_len = 0,
		.devices = NULL,
		.devices_len = 0,
};

int radio_init(octet_t *db) {
	int status;
	octet_stmt_t stmt_radio = {.fd = -1};
	octet_stmt_t stmt_device = {.fd = -1};

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, radio_file) == -1) {
		error("failed to sprintf to file\n");
		status = -1;
		goto cleanup;
	}

	if (octet_open(&stmt_radio, file, O_RDONLY, F_RDLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	if (stmt_radio.stat.st_size > db->table_len) {
		error("file length %zu exceeds buffer length %u\n", (size_t)stmt_radio.stat.st_size, db->table_len);
		status = -1;
		goto cleanup;
	}

	debug("select radios\n");

	off_t offset = 0;
	while (true) {
		if (offset >= stmt_radio.stat.st_size) {
			break;
		}
		if (octet_row_read(&stmt_radio, file, offset, db->row, radio_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t (*id)[8] = (uint8_t (*)[8])octet_blob_read(db->row, radio_row.id);
		uint8_t spi_device_len = octet_uint8_read(db->row, radio_row.spi_device_len);
		char *spi_device = octet_text_read(db->row, radio_row.spi_device);
		uint8_t gpio_device_len = octet_uint8_read(db->row, radio_row.gpio_device_len);
		char *gpio_device = octet_text_read(db->row, radio_row.gpio_device);
		uint8_t gpio_int_pin = octet_uint8_read(db->row, radio_row.gpio_int_pin);
		uint32_t frequency = octet_uint32_read(db->row, radio_row.frequency);
		uint32_t bandwidth = octet_uint32_read(db->row, radio_row.bandwidth);
		uint8_t spreading_factor = octet_uint8_read(db->row, radio_row.spreading_factor);
		uint8_t coding_rate = octet_uint8_read(db->row, radio_row.coding_rate);
		uint8_t tx_power = octet_uint8_read(db->row, radio_row.tx_power);
		uint8_t preamble_len = octet_uint8_read(db->row, radio_row.preamble_len);
		uint8_t sync_word = octet_uint8_read(db->row, radio_row.sync_word);
		uint8_t checksum = octet_uint8_read(db->row, radio_row.checksum);
		comms.radios = realloc(comms.radios, sizeof(radio_t) * (comms.radios_len + 1));
		if (comms.radios == NULL) {
			error("failed to allocate %zu bytes for radios because %s\n", sizeof(radio_t) * (comms.radios_len + 1), errno_str());
			status = -1;
			goto cleanup;
		}
		comms.radios[comms.radios_len].id = malloc(sizeof(*id));
		if (comms.radios[comms.radios_len].id == NULL) {
			error("failed to allocate %zu bytes for id because %s\n", sizeof(*id), errno_str());
			status = -1;
			goto cleanup;
		}
		comms.radios[comms.radios_len].spi_device = malloc(spi_device_len);
		if (comms.radios[comms.radios_len].spi_device == NULL) {
			error("failed to allocate %hhu bytes for spi device because %s\n", spi_device_len, errno_str());
			status = -1;
			goto cleanup;
		}
		comms.radios[comms.radios_len].gpio_device = malloc(gpio_device_len);
		if (comms.radios[comms.radios_len].gpio_device == NULL) {
			error("failed to allocate %hhu bytes for gpio device because %s\n", gpio_device_len, errno_str());
			status = -1;
			goto cleanup;
		}
		memcpy(comms.radios[comms.radios_len].id, id, sizeof(*id));
		memcpy(comms.radios[comms.radios_len].spi_device, spi_device, spi_device_len);
		comms.radios[comms.radios_len].spi_device_len = spi_device_len;
		memcpy(comms.radios[comms.radios_len].gpio_device, gpio_device, gpio_device_len);
		comms.radios[comms.radios_len].gpio_device_len = gpio_device_len;
		comms.radios[comms.radios_len].gpio_int_pin = gpio_int_pin;
		comms.radios[comms.radios_len].frequency = frequency;
		comms.radios[comms.radios_len].bandwidth = bandwidth;
		comms.radios[comms.radios_len].spreading_factor = spreading_factor;
		comms.radios[comms.radios_len].coding_rate = coding_rate;
		comms.radios[comms.radios_len].tx_power = tx_power;
		comms.radios[comms.radios_len].preamble_len = preamble_len;
		comms.radios[comms.radios_len].sync_word = sync_word;
		comms.radios[comms.radios_len].checksum = checksum;
		comms.radios_len += 1;
		offset += radio_row.size;
	}

	if (sprintf(file, "%s/%s.data", db->directory, device_file) == -1) {
		error("failed to sprintf to file\n");
		status = -1;
		goto cleanup;
	}

	if (octet_open(&stmt_device, file, O_RDONLY, F_RDLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	if (stmt_device.stat.st_size > db->table_len) {
		error("file length %zu exceeds buffer length %u\n", (size_t)stmt_device.stat.st_size, db->table_len);
		status = -1;
		goto cleanup;
	}

	debug("select devices\n");

	offset = 0;
	while (true) {
		if (offset >= stmt_device.stat.st_size) {
			break;
		}
		if (octet_row_read(&stmt_device, file, offset, db->row, device_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t (*id)[8] = (uint8_t (*)[8])octet_blob_read(db->row, device_row.id);
		uint8_t (*tag)[2] = (uint8_t (*)[2])octet_blob_read(db->row, device_row.tag);
		uint8_t (*key)[16] = (uint8_t (*)[16])octet_blob_read(db->row, device_row.key);
		comms.devices = realloc(comms.devices, sizeof(device_t) * (comms.devices_len + 1));
		if (comms.devices == NULL) {
			error("failed to allocate %zu bytes for devices because %s\n", sizeof(device_t) * (comms.devices_len + 1), errno_str());
			status = -1;
			goto cleanup;
		}
		comms.devices[comms.devices_len].id = malloc(sizeof(*id));
		if (comms.devices[comms.devices_len].id == NULL) {
			error("failed to allocate %zu bytes for id because %s\n", sizeof(*id), errno_str());
			status = -1;
			goto cleanup;
		}
		comms.devices[comms.devices_len].tag = malloc(sizeof(*tag));
		if (comms.devices[comms.devices_len].tag == NULL) {
			error("failed to allocate %zu bytes for tag because %s\n", sizeof(*tag), errno_str());
			status = -1;
			goto cleanup;
		}
		comms.devices[comms.devices_len].key = malloc(sizeof(*key));
		if (comms.devices[comms.devices_len].key == NULL) {
			error("failed to allocate %zu bytes for key because %s\n", sizeof(*key), errno_str());
			status = -1;
			goto cleanup;
		}
		memcpy(comms.devices[comms.devices_len].id, id, sizeof(*id));
		memcpy(comms.devices[comms.devices_len].tag, tag, sizeof(*tag));
		memcpy(comms.devices[comms.devices_len].key, key, sizeof(*key));
		comms.devices_len += 1;
		offset += device_row.size;
	}

	comms.workers = malloc(sizeof(radio_worker_t) * comms.radios_len);
	if (comms.workers == NULL) {
		error("failed to allocate %zu bytes for workers because %s\n", sizeof(radio_worker_t) * comms.radios_len, errno_str());
		status = -1;
		goto cleanup;
	}

	for (uint8_t index = 0; index < comms.radios_len; index++) {
		char spi_device[64];
		sprintf(spi_device, "%.*s", (int)comms.radios[index].spi_device_len, comms.radios[index].spi_device);
		if ((comms.workers[index].arg.spi_fd = spi_init(spi_device, 0, 8 * 1000 * 1000, 8)) == -1) {
			return -1;
		}
		char gpio_device[64];
		sprintf(gpio_device, "%.*s", (int)comms.radios[index].gpio_device_len, comms.radios[index].gpio_device);
		if ((comms.workers[index].arg.gpio_fd = gpio_init(gpio_device, comms.radios[index].gpio_int_pin)) == -1) {
			return -1;
		}

		comms.workers[index].arg.db.directory = database_directory;
		comms.workers[index].arg.db.row = malloc(512);
		if (comms.workers[index].arg.db.row == NULL) {
			error("failed to allocate %hu bytes for workers because %s\n", 512, errno_str());
			status = -1;
			goto cleanup;
		}

		comms.workers[index].arg.db.row_len = 512;
		comms.workers[index].arg.radio = &comms.radios[index];
		comms.workers[index].arg.devices = comms.devices;
		comms.workers[index].arg.devices_len = comms.devices_len;
		if (radio_spawn(&comms.workers[index].thread, radio_thread, &comms.workers[index].arg) == -1) {
			return -1;
		}
	}

	info("spawned %hhu radio threads\n", comms.radios_len);
	status = 0;

cleanup:
	octet_close(&stmt_radio, file);
	octet_close(&stmt_device, file);
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

	if (sx1278_sleep(arg->spi_fd) == -1) {
		error("failed to enable sleep mode\n");
	}

	if (sx1278_standby(arg->spi_fd) == -1) {
		error("failed to enable standby mode\n");
	}

	if (sx1278_frequency(arg->spi_fd, arg->radio->frequency) == -1) {
		error("failed to set radio frequency\n");
	}

	if (sx1278_tx_power(arg->spi_fd, arg->radio->tx_power) == -1) {
		error("failed to set radio tx power\n");
	}

	if (sx1278_preamble_length(arg->spi_fd, arg->radio->preamble_len) == -1) {
		error("failed to set radio preamble length\n");
	}

	if (sx1278_coding_rate(arg->spi_fd, arg->radio->coding_rate) == -1) {
		error("failed to set radio coding rate\n");
	}

	if (sx1278_bandwidth(arg->spi_fd, arg->radio->bandwidth) == -1) {
		error("failed to set radio bandwidth\n");
	}

	if (sx1278_spreading_factor(arg->spi_fd, arg->radio->spreading_factor) == -1) {
		error("failed to set radio spreading factor\n");
	}

	if (sx1278_checksum(arg->spi_fd, arg->radio->checksum) == -1) {
		error("failed to set radio checksum\n");
	}

	if (sx1278_sync_word(arg->spi_fd, arg->radio->sync_word) == -1) {
		error("failed to set sync word\n");
	}

	while (true) {
		uint8_t rx_data[256];
		uint8_t rx_data_len = 0;
		if (sx1278_receive(arg->spi_fd, arg->gpio_fd, &rx_data, &rx_data_len) == -1) {
			error("failed to receive packet\n");
			continue;
		}

		if (rx_data_len < 6) {
			debug("received packet without headers\n");
			continue;
		}

		int16_t rssi;
		if (sx1278_rssi(arg->spi_fd, &rssi) == -1) {
			error("failed to read packet rssi\n");
			continue;
		}

		int8_t snr;
		if (sx1278_snr(arg->spi_fd, &snr) == -1) {
			error("failed to read packet snr\n");
			continue;
		}

		rx("id %02x%02x frame %hu kind %02x bytes %hhu rssi %hd snr %.2f sf %hhu power %hhu\n", rx_data[0], rx_data[1],
			 (uint16_t)(rx_data[2] << 8) | (uint16_t)rx_data[3], rx_data[5], rx_data_len, rssi, snr / 4.0f,
			 arg->radio->spreading_factor, ((rx_data[4] >> 4) & 0x0f) + 2);

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

		ssc128_decrypt(&rx_data[6], rx_data_len - 6, (uint16_t)(rx_data[2] << 8) | (uint16_t)rx_data[3],
									 (const uint8_t (*)[16])device->key);

		uplink_t uplink;
		uplink.frame = (uint16_t)(rx_data[2] << 8) | (uint16_t)rx_data[3];
		uplink.kind = rx_data[5];
		memcpy(uplink.data, &rx_data[6], rx_data_len - 6);
		uplink.data_len = rx_data_len - 6;
		uplink.airtime = airtime_calculate(arg->radio, rx_data_len);
		uplink.frequency = arg->radio->frequency;
		uplink.bandwidth = arg->radio->bandwidth;
		uplink.rssi = rssi;
		uplink.snr = snr;
		uplink.spreading_factor = arg->radio->spreading_factor;
		uplink.coding_rate = arg->radio->coding_rate;
		uplink.checksum = arg->radio->checksum;
		uplink.tx_power = ((rx_data[4] >> 4) & 0x0f) + 2;
		uplink.preamble_len = (rx_data[4] & 0x0f) + 6;
		uplink.received_at = time(NULL);
		memcpy(uplink.device_id, device->id, sizeof(*device->id));

		pthread_mutex_lock(&uplinks.lock);

		while (uplinks.size >= uplinks_size) {
			warn("waiting for uplinks size %hu to decrease\n", uplinks.size);
			pthread_cond_wait(&uplinks.available, &uplinks.lock);
		}

		if (uplink_insert(&arg->db, &uplink, &uplinks.tail) != 0) {
			pthread_mutex_unlock(&uplinks.lock);
			continue;
		}
		uplinks.tail = (uint16_t)((uplinks.tail + 1) % uplinks_size);
		uplinks.size++;
		trace("radio thread increased uplinks size to %hu\n", uplinks.size);

		pthread_cond_signal(&uplinks.filled);
		pthread_mutex_unlock(&uplinks.lock);

		transmission_t transmission;
		transmission.timestamp = uplink.received_at;
		memcpy(transmission.radio_id, arg->radio->id, sizeof(*arg->radio->id));
		memcpy(transmission.type, "rx", sizeof(transmission.type));
		memcpy(transmission.device_id, uplink.device_id, sizeof(uplink.device_id));
		transmission.frame = uplink.frame;
		transmission.kind = uplink.kind;
		memcpy(transmission.data, uplink.data, uplink.data_len);
		transmission.data_len = uplink.data_len;
		transmission.rssi = uplink.rssi;
		transmission.snr = uplink.snr;
		transmission.sf = uplink.spreading_factor;
		transmission.cr = uplink.coding_rate;
		transmission.tx_power = uplink.tx_power;
		transmission.preamble_len = uplink.preamble_len;

		pthread_mutex_lock(&transmissions.lock);

		while (transmissions.size >= transmissions_size) {
			warn("waiting for transmissions size %hhu to decrease\n", transmissions.size);
			pthread_cond_wait(&transmissions.available, &transmissions.lock);
		}

		memcpy(&transmissions.ptr[transmissions.tail], &transmission, sizeof(transmission));
		transmissions.tail = (uint8_t)((transmissions.tail + 1) % transmissions_size);
		transmissions.size++;
		trace("radio thread increased transmissions size to %hhu\n", transmissions.size);

		pthread_cond_signal(&transmissions.filled);
		pthread_mutex_unlock(&transmissions.lock);

		if (sx1278_standby(arg->spi_fd) == -1) {
			error("failed to enable standby mode\n");
		}

		if (arg->radio->tx_power != ((rx_data[4] >> 4) & 0x0f) + 2) {
			arg->radio->tx_power = ((rx_data[4] >> 4) & 0x0f) + 2;
			if (sx1278_tx_power(arg->spi_fd, arg->radio->tx_power) == -1) {
				error("failed to set radio tx power\n");
			}
		}

		if (arg->radio->preamble_len != (rx_data[4] & 0x0f) + 6) {
			arg->radio->preamble_len = (rx_data[4] & 0x0f) + 6;
			if (sx1278_preamble_length(arg->spi_fd, arg->radio->preamble_len) == -1) {
				error("failed to set radio preamble length\n");
			}
		}

		uint8_t tx_data[256];
		uint8_t tx_data_len = 0;

		tx_data[tx_data_len] = rx_data[0];
		tx_data_len += sizeof(rx_data[0]);
		tx_data[tx_data_len] = rx_data[1];
		tx_data_len += sizeof(rx_data[1]);
		tx_data[tx_data_len] = rx_data[2];
		tx_data_len += sizeof(rx_data[2]);
		tx_data[tx_data_len] = rx_data[3];
		tx_data_len += sizeof(rx_data[3]);
		tx_data[tx_data_len] = (uint8_t)((((arg->radio->tx_power - 2) << 4) & 0xf0) | ((arg->radio->preamble_len - 6) & 0x0f));
		tx_data_len += sizeof(tx_data[4]);
		schedule_t schedule;
		if (schedule_find(&schedule, (uint8_t (*)[2])(&rx_data[0])) == 0) {
			tx_data[tx_data_len] = schedule.kind;
			tx_data_len += sizeof(schedule.kind);
			memcpy(&tx_data[tx_data_len], schedule.data, schedule.data_len);
			tx_data_len += schedule.data_len;
			ssc128_encrypt(&tx_data[6], tx_data_len - 6, (uint16_t)(tx_data[2] << 8) | (uint16_t)tx_data[3],
										 (const uint8_t (*)[16])device->key);
		} else {
			tx_data[tx_data_len] = 0x00;
			tx_data_len += sizeof(uint8_t);
		}

		if (sx1278_transmit(arg->spi_fd, arg->gpio_fd, &tx_data, tx_data_len) == -1) {
			error("failed to transmit packet\n");
			continue;
		}

		tx("id %02x%02x frame %hu kind %02x bytes %hhu sf %hhu power %hhu\n", tx_data[0], tx_data[1],
			 (uint16_t)(tx_data[2] << 8) | (uint16_t)tx_data[3], tx_data[5], tx_data_len, arg->radio->spreading_factor,
			 ((tx_data[4] >> 4) & 0x0f) + 2);

		ssc128_decrypt(&tx_data[6], tx_data_len - 6, (uint16_t)(tx_data[2] << 8) | (uint16_t)tx_data[3],
									 (const uint8_t (*)[16])device->key);

		downlink_t downlink;
		downlink.frame = (uint16_t)(tx_data[2] << 8) | (uint16_t)tx_data[3];
		downlink.kind = tx_data[5];
		memcpy(downlink.data, &tx_data[6], tx_data_len - 6);
		downlink.data_len = tx_data_len - 6;
		downlink.airtime = airtime_calculate(arg->radio, tx_data_len);
		downlink.frequency = arg->radio->frequency;
		downlink.bandwidth = arg->radio->bandwidth;
		downlink.spreading_factor = arg->radio->spreading_factor;
		downlink.coding_rate = arg->radio->coding_rate;
		downlink.checksum = arg->radio->checksum;
		downlink.tx_power = ((tx_data[4] >> 4) & 0x0f) + 2;
		downlink.preamble_len = (tx_data[4] & 0x0f) + 6;
		downlink.sent_at = time(NULL);
		memcpy(downlink.device_id, device->id, sizeof(*device->id));

		pthread_mutex_lock(&downlinks.lock);

		while (downlinks.size >= downlinks_size) {
			warn("waiting for downlinks size %hu to decrease\n", downlinks.size);
			pthread_cond_wait(&downlinks.available, &downlinks.lock);
		}

		if (downlink_insert(&arg->db, &downlink, &downlinks.tail) != 0) {
			pthread_mutex_unlock(&downlinks.lock);
			continue;
		}
		downlinks.tail = (uint16_t)((downlinks.tail + 1) % downlinks_size);
		downlinks.size++;
		trace("radio thread increased downlinks size to %hu\n", downlinks.size);

		pthread_cond_signal(&downlinks.filled);
		pthread_mutex_unlock(&downlinks.lock);

		transmission.timestamp = downlink.sent_at;
		memcpy(transmission.radio_id, arg->radio->id, sizeof(*arg->radio->id));
		memcpy(transmission.type, "tx", sizeof(transmission.type));
		memcpy(transmission.device_id, downlink.device_id, sizeof(downlink.device_id));
		transmission.frame = downlink.frame;
		transmission.kind = downlink.kind;
		memcpy(transmission.data, downlink.data, downlink.data_len);
		transmission.data_len = downlink.data_len;
		transmission.rssi = -256;
		transmission.snr = -128;
		transmission.sf = downlink.spreading_factor;
		transmission.cr = downlink.coding_rate;
		transmission.tx_power = downlink.tx_power;
		transmission.preamble_len = downlink.preamble_len;

		pthread_mutex_lock(&transmissions.lock);

		while (transmissions.size >= transmissions_size) {
			warn("waiting for transmissions size %hhu to decrease\n", transmissions.size);
			pthread_cond_wait(&transmissions.available, &transmissions.lock);
		}

		memcpy(&transmissions.ptr[transmissions.tail], &transmission, sizeof(transmission));
		transmissions.tail = (uint8_t)((transmissions.tail + 1) % transmissions_size);
		transmissions.size++;
		trace("radio thread increased transmissions size to %hhu\n", transmissions.size);

		pthread_cond_signal(&transmissions.filled);
		pthread_mutex_unlock(&transmissions.lock);
	}
}

void radio_reload(octet_t *db, response_t *response) {
	for (uint8_t index = 0; index < comms.radios_len; index++) {
		if (pthread_cancel(comms.workers[index].thread) == -1) {
			error("failed to cancel radio thread %02x%02x\n", (*comms.radios[index].id)[0], (*comms.radios[index].id)[1]);
		};
		trace("waiting for radio %02x%02x to finish\n", (*comms.radios[index].id)[0], (*comms.radios[index].id)[1]);
		if (pthread_join(comms.workers[index].thread, NULL) == -1) {
			error("failed to join radio thread %02x%02x\n", (*comms.radios[index].id)[0], (*comms.radios[index].id)[1]);
		}
		if (close(comms.workers[index].arg.spi_fd) == -1) {
			error("failed to close ioctl because %s\n", errno_str());
		}
		if (close(comms.workers[index].arg.gpio_fd) == -1) {
			error("failed to close ioctl because %s\n", errno_str());
		}
		free(comms.radios[index].id);
		free(comms.radios[index].spi_device);
		free(comms.radios[index].gpio_device);
	}

	for (uint8_t index = 0; index < comms.devices_len; index++) {
		free(comms.devices[index].id);
		free(comms.devices[index].tag);
		free(comms.devices[index].key);
	}

	free(comms.workers);
	comms.workers = NULL;
	free(comms.radios);
	comms.radios = NULL;
	comms.radios_len = 0;
	free(comms.devices);
	comms.devices = NULL;
	comms.devices_len = 0;

	if (radio_init(db) == -1) {
		response->status = 500;
		return;
	}

	info("reloaded radios\n");
	response->status = 200;
}
