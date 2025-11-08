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
#include <stdlib.h>
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

int downlink_init(sqlite3 *database) {
	int status;
	sqlite3_stmt *stmt;

	const char *sql = "select "
										"host.id, host.address, host.port, host.username, host.password "
										"from host "
										"order by port asc";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = -1;
		goto cleanup;
	}

	host_t *hosts = NULL;
	uint8_t hosts_len = 0;
	while (true) {
		int result = sqlite3_step(stmt);
		if (result == SQLITE_ROW) {
			hosts = realloc(hosts, sizeof(host_t) * (hosts_len + 1));
			if (hosts == NULL) {
				error("failed to allocate %zu bytes for hosts because %s\n", sizeof(host_t) * (hosts_len + 1), errno_str());
				status = -1;
				goto cleanup;
			}
			const uint8_t *id = sqlite3_column_blob(stmt, 0);
			const size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
			if (id_len != sizeof(*((host_t *)0)->id)) {
				error("id length %zu does not match buffer length %zu\n", id_len, sizeof(*((host_t *)0)->id));
				status = 500;
				goto cleanup;
			}
			hosts[hosts_len].id = malloc(sizeof(*((host_t *)0)->id));
			if (hosts[hosts_len].id == NULL) {
				error("failed to allocate %zu bytes for id because %s\n", sizeof(*((host_t *)0)->id), errno_str());
				status = -1;
				goto cleanup;
			}
			memcpy(hosts[hosts_len].id, id, id_len);
			const uint8_t *addr = sqlite3_column_text(stmt, 1);
			const size_t address_len = (uint8_t)sqlite3_column_bytes(stmt, 1);
			hosts[hosts_len].address = malloc(address_len);
			if (hosts[hosts_len].address == NULL) {
				error("failed to allocate %zu bytes for address because %s\n", address_len, errno_str());
				status = -1;
				goto cleanup;
			}
			memcpy(hosts[hosts_len].address, addr, address_len);
			hosts[hosts_len].address_len = (uint8_t)address_len;
			hosts[hosts_len].port = (uint16_t)sqlite3_column_int(stmt, 2);
			const uint8_t *username = sqlite3_column_text(stmt, 3);
			const size_t username_len = (uint8_t)sqlite3_column_bytes(stmt, 3);
			hosts[hosts_len].username = malloc(username_len);
			if (hosts[hosts_len].username == NULL) {
				error("failed to allocate %zu bytes for username because %s\n", username_len, errno_str());
				status = -1;
				goto cleanup;
			}
			memcpy(hosts[hosts_len].username, username, username_len);
			hosts[hosts_len].username_len = (uint8_t)username_len;
			const uint8_t *password = sqlite3_column_text(stmt, 4);
			const size_t password_len = (uint8_t)sqlite3_column_bytes(stmt, 4);
			hosts[hosts_len].password = malloc(password_len);
			if (hosts[hosts_len].password == NULL) {
				error("failed to allocate %zu bytes for password because %s\n", password_len, errno_str());
				status = -1;
				goto cleanup;
			}
			memcpy(hosts[hosts_len].password, password, password_len);
			hosts[hosts_len].password_len = (uint8_t)password_len;
			hosts_len += 1;
		} else if (result == SQLITE_DONE) {
			status = 0;
			break;
		} else {
			error("failed to execute statement because %s\n", sqlite3_errmsg(database));
			status = 500;
			goto cleanup;
		}
	}

	downlinks.ptr = malloc(downlinks_size * sizeof(*downlinks.ptr));
	if (downlinks.ptr == NULL) {
		fatal("failed to allocate %zu bytes for downlinks because %s\n", downlinks_size * sizeof(*downlinks.ptr), errno_str());
		exit(1);
	}

	pthread_t downlink;
	downlink_arg_t *arg = malloc(sizeof(downlink_arg_t));
	if (arg == NULL) {
		error("failed to allocate %zu bytes for downlink arg because %s\n", sizeof(downlink_arg_t), errno_str());
		status = -1;
		goto cleanup;
	}
	arg->hosts = hosts;
	arg->hosts_len = hosts_len;
	if (downlink_spawn(&downlink, downlink_thread, arg) == -1) {
		exit(1);
	}

cleanup:
	sqlite3_finalize(stmt);
	return status;
}

int downlink_spawn(pthread_t *thread, void *(*function)(void *), downlink_arg_t *arg) {
	trace("spawning downlink thread\n");

	int spawn_error = pthread_create(thread, NULL, function, (void *)arg);
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
	downlink_arg_t *arg = (downlink_arg_t *)args;

	char buffer[128];
	strn8_t cookie = {.ptr = (char *)&buffer, .len = 0, .cap = sizeof(buffer)};

	while (true) {
		pthread_mutex_lock(&downlinks.lock);

		while (downlinks.size == 0) {
			pthread_cond_wait(&downlinks.filled, &downlinks.lock);
		}

		downlink_t downlink = downlinks.ptr[downlinks.head];
		pthread_mutex_unlock(&downlinks.lock);

		while (true) {
			host_t *host = NULL;
			if (arg->hosts_len == 0) {
				warn("%hhu host connections to forward to\n", arg->hosts_len);
				sleep(8);
				continue;
			}
			host = &arg->hosts[rand() % arg->hosts_len];

			if (downlink_create(&downlink, host, &cookie) != -1) {
				break;
			}

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

int downlink_create(downlink_t *downlink, host_t *host, strn8_t *cookie) {
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

	char buffer[64];
	sprintf(buffer, "%.*s", host->address_len, host->address);
	if (fetch(buffer, host->port, &request, &response) == -1) {
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
