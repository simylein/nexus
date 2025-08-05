#include "thread.h"
#include "airtime.h"
#include "auth.h"
#include "endian.h"
#include "error.h"
#include "http.h"
#include "logger.h"
#include "radio.h"
#include "sx1278.h"
#include "uplink.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int spawn(worker_t *worker, void *(*function)(void *),
					void (*logger)(const char *message, ...) __attribute__((format(printf, 1, 2)))) {
	trace("spawning worker thread %hhu\n", worker->arg.id);

	int spawn_error = pthread_create(&worker->thread, NULL, function, (void *)&worker->arg);
	if (spawn_error != 0) {
		errno = spawn_error;
		logger("failed to spawn worker thread %hhu because %s\n", worker->arg.id, errno_str());
		return -1;
	}

	int detach_error = pthread_detach(worker->thread);
	if (detach_error != 0) {
		errno = detach_error;
		logger("failed to detach worker thread %hhu because %s\n", worker->arg.id, errno_str());
		return -1;
	}

	return 0;
}

void *thread(void *args) {
	arg_t *arg = (arg_t *)args;

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

	if (sx1278_checksum(arg->fd, arg->radio->spreading_factor) == -1) {
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

		uint8_t(*device_id)[16] = NULL;
		for (uint8_t ind = 0; ind < arg->devices_len; ind++) {
			if (memcmp(&rx_data[0], (*arg->devices)[ind].tag, sizeof((*arg->devices)[ind].tag)) == 0) {
				device_id = &(*arg->devices)[ind].id;
			}
		}

		if (device_id == NULL) {
			debug("no registration for device %02x%02x\n", rx_data[0], rx_data[1]);
			continue;
		}

		uint8_t kind = rx_data[2];
		uint8_t *data = &rx_data[3];
		uint8_t data_len = rx_data_len - 3;
		uint16_t airtime = airtime_calculate(arg->radio, rx_data_len);
		time_t received_at = time(NULL);

		uplink_t uplink = {
				.kind = kind,
				.data = data,
				.data_len = data_len,
				.airtime = airtime,
				.frequency = arg->radio->frequency,
				.bandwidth = arg->radio->bandwidth,
				.rssi = rssi,
				.snr = snr,
				.sf = arg->radio->spreading_factor,
				.received_at = received_at,
				.device_id = device_id,
		};

		host_t *host = NULL;
		if (arg->hosts_len == 0) {
			warn("%hhu host connections to forward to\n", arg->hosts_len);
			continue;
		}
		host = &(*arg->hosts)[rand() % arg->hosts_len];

		if (arg->cookie_age + 3600 < received_at) {
			debug("refreshing auth cookie with age %lu\n", arg->cookie_age);
			if (auth(host, arg->cookie, &arg->cookie_len, &arg->cookie_age) == -1) {
				continue;
			}
		}

		if (uplink_create(&uplink, host, arg->cookie, arg->cookie_len) == -1) {
			continue;
		}

		uint8_t tx_data[256];
		uint8_t tx_data_len = 0;

		tx_data[tx_data_len] = rx_data[0];
		tx_data_len += 1;
		tx_data[tx_data_len] = rx_data[1];
		tx_data_len += 1;
		tx_data[tx_data_len] = 0x06;
		tx_data_len += 1;

		if (sx1278_transmit(arg->fd, &tx_data, tx_data_len) == -1) {
			error("failed to transmit packet\n");
			continue;
		}

		tx("id %02x%02x kind %02x bytes %hhu power %hhu sf %hhu\n", tx_data[0], tx_data[1], tx_data[2], tx_data_len,
			 arg->radio->tx_power, arg->radio->spreading_factor);
	}
}
