#include "uplink.h"
#include "../lib/config.h"
#include "../lib/endian.h"
#include "../lib/error.h"
#include "../lib/logger.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include "../lib/strn.h"
#include "http.h"
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

uplinks_t uplinks = {
		.head = 0,
		.tail = 0,
		.size = 0,
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.filled = PTHREAD_COND_INITIALIZER,
		.available = PTHREAD_COND_INITIALIZER,
};

int uplink_spawn(pthread_t *thread, void *(*function)(void *), uplink_arg_t *arg) {
	trace("spawning uplink thread\n");

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

void *uplink_thread(void *args) {
	uplink_arg_t *arg = (uplink_arg_t *)args;

	char buffer[128];
	strn8_t cookie = {.ptr = (char *)&buffer, .len = 0, .cap = sizeof(buffer)};

	while (true) {
		pthread_mutex_lock(&uplinks.lock);

		while (uplinks.size == 0) {
			pthread_cond_wait(&uplinks.filled, &uplinks.lock);
		}

		uplink_t uplink = uplinks.ptr[uplinks.head];
		pthread_mutex_unlock(&uplinks.lock);

		while (true) {
			host_t *host = NULL;
			if (arg->hosts_len == 0) {
				warn("%hhu host connections to forward to\n", arg->hosts_len);
				sleep(8);
				continue;
			}
			host = &arg->hosts[rand() % arg->hosts_len];

			if (uplink_create(&uplink, host, &cookie) != -1) {
				break;
			}

			sleep(8);
		}

		pthread_mutex_lock(&uplinks.lock);
		uplinks.head = (uint8_t)((uplinks.head + 1) % uplinks_size);
		uplinks.size--;
		trace("uplink thread decreased queue size to %hhu\n", uplinks.size);
		pthread_cond_signal(&uplinks.available);
		pthread_mutex_unlock(&uplinks.lock);
	}
}

int uplink_create(uplink_t *uplink, host_t *host, strn8_t *cookie) {
	request_t request;
	response_t response;

	char method[] = "POST";
	request.method.ptr = method;
	request.method.len = sizeof(method) - 1;

	char pathname[] = "/api/uplink";
	request.pathname.ptr = pathname;
	request.pathname.len = sizeof(pathname) - 1;

	char protocol[] = "HTTP/1.1";
	request.protocol.ptr = protocol;
	request.protocol.len = sizeof(protocol) - 1;

	char header[128];
	request.header.ptr = header;
	request.header.len = (uint16_t)sprintf(request.header.ptr, "cookie:auth=%.*s\r\n", cookie->len, cookie->ptr);

	char body[512];
	request.body.ptr = body;
	request.body.len = 0;

	memcpy(&request.body.ptr[request.body.len], &uplink->kind, sizeof(uplink->kind));
	request.body.len += sizeof(uplink->kind);
	memcpy(&request.body.ptr[request.body.len], &uplink->data_len, sizeof(uplink->data_len));
	request.body.len += sizeof(uplink->data_len);
	memcpy(&request.body.ptr[request.body.len], uplink->data, uplink->data_len);
	request.body.len += uplink->data_len;
	memcpy(&request.body.ptr[request.body.len], &(uint16_t[]){hton16(uplink->airtime)}, sizeof(uplink->airtime));
	request.body.len += sizeof(uplink->airtime);
	memcpy(&request.body.ptr[request.body.len], &(uint32_t[]){hton32(uplink->frequency)}, sizeof(uplink->frequency));
	request.body.len += sizeof(uplink->frequency);
	memcpy(&request.body.ptr[request.body.len], &(uint32_t[]){hton32(uplink->bandwidth)}, sizeof(uplink->bandwidth));
	request.body.len += sizeof(uplink->bandwidth);
	memcpy(&request.body.ptr[request.body.len], &(uint16_t[]){hton16((uint16_t)uplink->rssi)}, sizeof(uplink->rssi));
	request.body.len += sizeof(uplink->rssi);
	memcpy(&request.body.ptr[request.body.len], &uplink->snr, sizeof(uplink->snr));
	request.body.len += sizeof(uplink->snr);
	memcpy(&request.body.ptr[request.body.len], &uplink->spreading_factor, sizeof(uplink->spreading_factor));
	request.body.len += sizeof(uplink->spreading_factor);
	memcpy(&request.body.ptr[request.body.len], &uplink->tx_power, sizeof(uplink->tx_power));
	request.body.len += sizeof(uplink->tx_power);
	memcpy(&request.body.ptr[request.body.len], &(time_t[]){(time_t)hton64((uint64_t)uplink->received_at)},
				 sizeof(uplink->received_at));
	request.body.len += sizeof(uplink->received_at);
	memcpy(&request.body.ptr[request.body.len], uplink->device_id, sizeof(uplink->device_id));
	request.body.len += sizeof(uplink->device_id);

	char buffer[64];
	sprintf(buffer, "%s", host->address);
	if (fetch(buffer, host->port, &request, &response) == -1) {
		return -1;
	}

	if (response.status == 400) {
		warn("host rejected uplink with status %hu\n", response.status);
		return 0;
	}

	if (response.status != 201) {
		error("host rejected uplink with status %hu\n", response.status);
		return -1;
	}

	info("successfully created uplink\n");
	return 0;
}
