#include "downlink.h"
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
#include <string.h>
#include <time.h>
#include <unistd.h>

downlinks_t downlinks = {
		.head = 0,
		.tail = 0,
		.size = 0,
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.filled = PTHREAD_COND_INITIALIZER,
		.available = PTHREAD_COND_INITIALIZER,
};

int downlink_spawn(pthread_t *thread, void *(*function)(void *)) {
	trace("spawning downlink thread\n");

	int spawn_error = pthread_create(thread, NULL, function, NULL);
	if (spawn_error != 0) {
		errno = spawn_error;
		fatal("failed to spawn downlink thread because %s\n", errno_str());
		return -1;
	}

	int detach_error = pthread_detach(*thread);
	if (detach_error != 0) {
		errno = detach_error;
		fatal("failed to detach downlink thread because %s\n", errno_str());
		return -1;
	}

	return 0;
}

void *downlink_thread(void *args) {
	(void)args;

	char buffer[128];
	strn8_t cookie = {.ptr = (char *)&buffer, .len = 0, .cap = sizeof(buffer)};

	while (true) {
		pthread_mutex_lock(&downlinks.lock);

		while (downlinks.size == 0) {
			pthread_cond_wait(&downlinks.filled, &downlinks.lock);
		}

		downlink_t downlink = downlinks.ptr[downlinks.head];
		pthread_mutex_unlock(&downlinks.lock);

		while (downlink_create(&downlink, &cookie) == -1) {
			sleep(8);
		}

		pthread_mutex_lock(&downlinks.lock);
		downlinks.head = (uint8_t)((downlinks.head + 1) % downlinks_size);
		downlinks.size--;
		trace("downlink thread decreased queue size to %hhu\n", downlinks.size);
		pthread_cond_signal(&downlinks.available);
		pthread_mutex_unlock(&downlinks.lock);
	}
}

int downlink_create(downlink_t *downlink, strn8_t *cookie) {
	request_t request;
	response_t response;

	char method[] = "POST";
	request.method.ptr = method;
	request.method.len = sizeof(method) - 1;

	char pathname[] = "/api/downlink";
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

	memcpy(&request.body.ptr[request.body.len], &downlink->kind, sizeof(downlink->kind));
	request.body.len += sizeof(downlink->kind);
	memcpy(&request.body.ptr[request.body.len], &downlink->data_len, sizeof(downlink->data_len));
	request.body.len += sizeof(downlink->data_len);
	memcpy(&request.body.ptr[request.body.len], downlink->data, downlink->data_len);
	request.body.len += downlink->data_len;
	memcpy(&request.body.ptr[request.body.len], &(uint16_t[]){hton16(downlink->airtime)}, sizeof(downlink->airtime));
	request.body.len += sizeof(downlink->airtime);
	memcpy(&request.body.ptr[request.body.len], &(uint32_t[]){hton32(downlink->frequency)}, sizeof(downlink->frequency));
	request.body.len += sizeof(downlink->frequency);
	memcpy(&request.body.ptr[request.body.len], &(uint32_t[]){hton32(downlink->bandwidth)}, sizeof(downlink->bandwidth));
	request.body.len += sizeof(downlink->bandwidth);
	memcpy(&request.body.ptr[request.body.len], &downlink->spreading_factor, sizeof(downlink->spreading_factor));
	request.body.len += sizeof(downlink->spreading_factor);
	memcpy(&request.body.ptr[request.body.len], &downlink->tx_power, sizeof(downlink->tx_power));
	request.body.len += sizeof(downlink->tx_power);
	memcpy(&request.body.ptr[request.body.len], &(time_t[]){(time_t)hton64((uint64_t)downlink->sent_at)},
				 sizeof(downlink->sent_at));
	request.body.len += sizeof(downlink->sent_at);
	memcpy(&request.body.ptr[request.body.len], downlink->device_id, sizeof(downlink->device_id));
	request.body.len += sizeof(downlink->device_id);

	if (fetch("127.0.0.1", 2254, &request, &response) == -1) {
		return -1;
	}

	if (response.status == 400) {
		warn("host rejected uplink with status %hu\n", response.status);
		return 0;
	}

	if (response.status != 201) {
		error("host rejected downlink with status %hu\n", response.status);
		return -1;
	}

	info("successfully created downlink\n");
	return 0;
}
