#include "transmission.h"
#include "../lib/config.h"
#include "../lib/error.h"
#include "../lib/logger.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

streams_t streams = {
		.size = 0,
		.lock = PTHREAD_MUTEX_INITIALIZER,
};

transmissions_t transmissions = {
		.head = 0,
		.tail = 0,
		.size = 0,
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.filled = PTHREAD_COND_INITIALIZER,
		.available = PTHREAD_COND_INITIALIZER,
};

int transmission_init(void) {
	streams.ptr = malloc(streams_size * sizeof(*streams.ptr));
	if (streams.ptr == NULL) {
		fatal("failed to allocate %zu bytes for streams because %s\n", streams_size * sizeof(*streams.ptr), errno_str());
		return -1;
	}
	for (uint8_t index = 0; index < streams_size; index++) {
		streams.ptr[index] = -1;
	}

	transmissions.ptr = malloc(transmissions_size * sizeof(*transmissions.ptr));
	if (transmissions.ptr == NULL) {
		fatal("failed to allocate %zu bytes for transmissions because %s\n", transmissions_size * sizeof(*transmissions.ptr),
					errno_str());
		return -1;
	}

	if (transmission_spawn(&transmissions.worker.thread, transmission_thread) == -1) {
		return -1;
	}

	return 0;
}

int transmission_spawn(pthread_t *thread, void *(*function)(void *)) {
	trace("spawning transmission thread\n");

	int spawn_error = pthread_create(thread, NULL, function, NULL);
	if (spawn_error != 0) {
		errno = spawn_error;
		fatal("failed to spawn transmission thread because %s\n", errno_str());
		return -1;
	}

	return 0;
}

void *transmission_thread(void *args) {
	(void)args;

	while (true) {
		pthread_mutex_lock(&transmissions.lock);

		while (transmissions.size == 0) {
			pthread_cond_wait(&transmissions.filled, &transmissions.lock);
		}

		transmission_t transmission = transmissions.ptr[transmissions.head];
		pthread_mutex_unlock(&transmissions.lock);

		char buffer[512];
		uint16_t buffer_len = 0;

		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "data:");
		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%lu", transmission.timestamp);
		buffer[buffer_len] = ' ';
		buffer_len += sizeof(char);
		for (uint8_t index = 0; index < 2; index++) {
			buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%02x", transmission.radio_id[index]);
		}
		buffer[buffer_len] = ' ';
		buffer_len += sizeof(char);
		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%.*s", (int)sizeof(transmission.type), transmission.type);
		buffer[buffer_len] = ' ';
		buffer_len += sizeof(char);
		for (uint8_t index = 0; index < 2; index++) {
			buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%02x", transmission.device_id[index]);
		}
		buffer[buffer_len] = ' ';
		buffer_len += sizeof(char);
		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%02x", transmission.kind);
		buffer[buffer_len] = ' ';
		buffer_len += sizeof(char);
		for (uint8_t index = 0; index < transmission.data_len; index++) {
			buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%02x", transmission.data[index]);
		}
		buffer[buffer_len] = ' ';
		buffer_len += sizeof(char);
		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%hd", transmission.rssi);
		buffer[buffer_len] = ' ';
		buffer_len += sizeof(char);
		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%hhd", transmission.snr);
		buffer[buffer_len] = ' ';
		buffer_len += sizeof(char);
		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%hu", transmission.sf);
		buffer[buffer_len] = ' ';
		buffer_len += sizeof(char);
		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%hu", transmission.cr);
		buffer[buffer_len] = ' ';
		buffer_len += sizeof(char);
		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%hu", transmission.tx_power);
		buffer[buffer_len] = ' ';
		buffer_len += sizeof(char);
		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "%hu", transmission.preamble_len);
		buffer_len += (uint16_t)sprintf(&buffer[buffer_len], "\n\n");

		for (uint8_t index = 0; index < streams_size; index++) {
			if (streams.ptr[index] != -1) {
				debug("streaming transmission to socket %d\n", streams.ptr[index]);
				ssize_t sent = send(streams.ptr[index], buffer, buffer_len, MSG_NOSIGNAL);
				if (sent == -1) {
					error("failed to send data to client because %s\n", errno_str());
					close(streams.ptr[index]);
					streams.ptr[index] = -1;
				}
				if (sent == 0) {
					warn("server did not send any data\n");
					close(streams.ptr[index]);
					streams.ptr[index] = -1;
				}
			}
		}

		pthread_mutex_lock(&transmissions.lock);
		transmissions.head = (uint8_t)((transmissions.head + 1) % transmissions_size);
		transmissions.size--;
		trace("transmission thread decreased queue size to %hhu\n", transmissions.size);
		pthread_cond_signal(&transmissions.available);
		pthread_mutex_unlock(&transmissions.lock);
	}
}

void transmission_stream(request_t *request, response_t *response) {
	header_write(response, "content-type:text/event-stream\r\n");

	bool space = false;
	pthread_mutex_lock(&streams.lock);
	for (uint8_t index = 0; index < streams_size; index++) {
		if (streams.ptr[index] == -1) {
			streams.ptr[index] = request->socket;
			space = true;
			break;
		}
	}
	pthread_mutex_unlock(&streams.lock);

	if (space == false) {
		warn("no more streams available\n");
		response->status = 503;
		return;
	}

	info("streaming transmissions\n");
	response->status = 200;
	response->stream = true;
}
