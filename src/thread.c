#include "thread.h"
#include "error.h"
#include "logger.h"
#include "radio.h"
#include "sx1278.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
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
	}
}
