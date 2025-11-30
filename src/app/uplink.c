#include "uplink.h"
#include "../api/database.h"
#include "../lib/config.h"
#include "../lib/endian.h"
#include "../lib/error.h"
#include "../lib/logger.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include "auth.h"
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

int uplink_init(sqlite3 *database) {
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
			status = database_error(database, result);
			goto cleanup;
		}
	}

	uplinks.ptr = malloc(uplinks_size * sizeof(*uplinks.ptr));
	if (uplinks.ptr == NULL) {
		fatal("failed to allocate %zu bytes for uplinks because %s\n", uplinks_size * sizeof(*uplinks.ptr), errno_str());
		return -1;
	}

	uplinks.worker.arg.hosts = hosts;
	uplinks.worker.arg.hosts_len = hosts_len;
	if (uplink_spawn(&uplinks.worker.thread, uplink_thread, &uplinks.worker.arg) == -1) {
		return -1;
	}

cleanup:
	sqlite3_finalize(stmt);
	return status;
}

int uplink_spawn(pthread_t *thread, void *(*function)(void *), uplink_arg_t *arg) {
	trace("spawning uplink thread\n");

	int spawn_error = pthread_create(thread, NULL, function, (void *)arg);
	if (spawn_error != 0) {
		errno = spawn_error;
		fatal("failed to spawn uplink thread because %s\n", errno_str());
		return -1;
	}

	return 0;
}

void *uplink_thread(void *args) {
	uplink_arg_t *arg = (uplink_arg_t *)args;

	char buffer[128];
	cookie_t cookie = {.ptr = (char *)&buffer, .len = 0, .cap = sizeof(buffer), .age = 0};

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
				goto sleep;
			}
			host = &arg->hosts[rand() % arg->hosts_len];

			if (cookie.age + 3600 < time(NULL)) {
				debug("refreshing auth cookie with age %lu\n", cookie.age);
				if (auth(host, &cookie) == -1) {
					goto sleep;
				}
			}

			if (uplink_create(&uplink, host, &cookie) != -1) {
				break;
			}

		sleep:
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

int uplink_create(uplink_t *uplink, host_t *host, cookie_t *cookie) {
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

	char request_header[256];
	request.header.ptr = request_header;
	request.header.len = (uint16_t)sprintf(request.header.ptr, "cookie:auth=%.*s\r\n", cookie->len, cookie->ptr);
	request.header.cap = sizeof(request_header);

	char request_body[512];
	request.body.ptr = request_body;
	request.body.len = 0;
	request.body.cap = sizeof(request_body);

	char response_header[256];
	response.header.ptr = response_header;
	response.header.len = 0;
	response.header.cap = sizeof(response_header);

	char response_body[512];
	response.body.ptr = response_body;
	response.body.len = 0;
	response.body.cap = sizeof(response_body);

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
	memcpy(&request.body.ptr[request.body.len], &uplink->preamble_len, sizeof(uplink->preamble_len));
	request.body.len += sizeof(uplink->preamble_len);
	memcpy(&request.body.ptr[request.body.len], &(time_t[]){(time_t)hton64((uint64_t)uplink->received_at)},
				 sizeof(uplink->received_at));
	request.body.len += sizeof(uplink->received_at);
	memcpy(&request.body.ptr[request.body.len], uplink->device_id, sizeof(uplink->device_id));
	request.body.len += sizeof(uplink->device_id);

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
		error("host rejected uplink with status %hu\n", response.status);
		return -1;
	}

	info("successfully created uplink\n");
	return 0;
}
