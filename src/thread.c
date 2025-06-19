#include "thread.h"
#include "endian.h"
#include "error.h"
#include "http.h"
#include "logger.h"
#include "radio.h"
#include "sx1278.h"
#include <errno.h>
#include <stdbool.h>
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
		uint16_t airtime = 0;
		time_t received_at = time(NULL);

		request_t request = {.body_len = 0};
		response_t response = {.status = 0};

		char method[] = "POST";
		request.method = method;
		request.method_len = sizeof(method);

		char pathname[] = "/api/uplink";
		request.pathname = pathname;
		request.pathname_len = sizeof(pathname);

		char protocol[] = "HTTP/1.1";
		request.protocol = protocol;
		request.protocol_len = sizeof(protocol);

		append_body(&request, &kind, sizeof(kind));
		append_body(&request, &data_len, sizeof(data_len));
		append_body(&request, data, data_len);
		append_body(&request, &(uint16_t[]){hton16(airtime)}, sizeof(airtime));
		append_body(&request, &(uint32_t[]){hton32(arg->radio->frequency)}, sizeof(arg->radio->frequency));
		append_body(&request, &(uint32_t[]){hton32(arg->radio->bandwidth)}, sizeof(arg->radio->bandwidth));
		append_body(&request, &(uint16_t[]){hton16((uint16_t)rssi)}, sizeof(rssi));
		append_body(&request, (char *)&snr, sizeof(snr));
		append_body(&request, &arg->radio->spreading_factor, sizeof(arg->radio->spreading_factor));
		append_body(&request, &(uint64_t[]){hton64((uint64_t)received_at)}, sizeof(received_at));
		append_body(&request, device_id, sizeof(*device_id));

		// TODO use config values
		const char *warden_address = "0.0.0.0";
		const uint16_t warden_port = 2254;

		if (fetch(warden_address, warden_port, &request, &response) == -1) {
			error("failed to talk with host %s:%hu\n", warden_address, warden_port);
			continue;
		}

		if (response.status != 201) {
			error("host rejected uplink with status %hu\n", response.status);
			continue;
		}

		info("successfully created uplink\n");
	}
}
