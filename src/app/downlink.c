#include "downlink.h"
#include "../lib/config.h"
#include "../lib/endian.h"
#include "../lib/error.h"
#include "../lib/logger.h"
#include "../lib/octet.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include "auth.h"
#include "http.h"
#include <errno.h>
#include <fcntl.h>
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

int downlink_init(octet_t *db) {
	int status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, host_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDONLY, F_RDLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	if (stmt.stat.st_size > db->table_len) {
		error("file length %zu exceeds buffer length %u\n", (size_t)stmt.stat.st_size, db->table_len);
		status = 500;
		goto cleanup;
	}

	debug("select hosts\n");

	host_t *hosts = NULL;
	uint8_t hosts_len = 0;
	off_t offset = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			status = 0;
			break;
		}
		if (octet_row_read(&stmt, file, offset, db->row, host_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t (*id)[8] = (uint8_t (*)[8])octet_blob_read(db->row, host_row.id);
		uint8_t address_len = octet_uint8_read(db->row, host_row.address_len);
		char *host_address = octet_text_read(db->row, host_row.address);
		uint16_t host_port = octet_uint16_read(db->row, host_row.port);
		uint8_t username_len = octet_uint8_read(db->row, host_row.username_len);
		char *username = octet_text_read(db->row, host_row.username);
		uint8_t password_len = octet_uint8_read(db->row, host_row.password_len);
		char *password = octet_text_read(db->row, host_row.password);
		hosts = realloc(hosts, sizeof(host_t) * (hosts_len + 1));
		if (hosts == NULL) {
			error("failed to allocate %zu bytes for hosts because %s\n", sizeof(host_t) * (hosts_len + 1), errno_str());
			status = -1;
			goto cleanup;
		}
		hosts[hosts_len].id = malloc(sizeof(*id));
		if (hosts[hosts_len].id == NULL) {
			error("failed to allocate %zu bytes for id because %s\n", sizeof(*id), errno_str());
			status = -1;
			goto cleanup;
		}
		hosts[hosts_len].address = malloc(address_len);
		if (hosts[hosts_len].address == NULL) {
			error("failed to allocate %hhu bytes for address because %s\n", address_len, errno_str());
			status = -1;
			goto cleanup;
		}
		hosts[hosts_len].username = malloc(username_len);
		if (hosts[hosts_len].username == NULL) {
			error("failed to allocate %hhu bytes for username because %s\n", username_len, errno_str());
			status = -1;
			goto cleanup;
		}
		hosts[hosts_len].password = malloc(password_len);
		if (hosts[hosts_len].password == NULL) {
			error("failed to allocate %hhu bytes for password because %s\n", password_len, errno_str());
			status = -1;
			goto cleanup;
		}
		memcpy(hosts[hosts_len].id, id, sizeof(*id));
		memcpy(hosts[hosts_len].address, host_address, address_len);
		hosts[hosts_len].address_len = address_len;
		hosts[hosts_len].port = host_port;
		memcpy(hosts[hosts_len].username, username, username_len);
		hosts[hosts_len].username_len = username_len;
		memcpy(hosts[hosts_len].password, password, password_len);
		hosts[hosts_len].password_len = password_len;
		hosts_len += 1;
		offset += host_row.size;
	}

	downlinks.ptr = malloc(downlinks_size * sizeof(*downlinks.ptr));
	if (downlinks.ptr == NULL) {
		fatal("failed to allocate %zu bytes for downlinks because %s\n", downlinks_size * sizeof(*downlinks.ptr), errno_str());
		return -1;
	}

	downlinks.worker.arg.hosts = hosts;
	downlinks.worker.arg.hosts_len = hosts_len;
	if (downlink_spawn(&downlinks.worker.thread, downlink_thread, &downlinks.worker.arg) == -1) {
		return -1;
	}

cleanup:
	octet_close(&stmt, file);
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

	return 0;
}

void *downlink_thread(void *args) {
	downlink_arg_t *arg = (downlink_arg_t *)args;

	char buffer[128];
	cookie_t cookie = {.ptr = (char *)&buffer, .len = 0, .cap = sizeof(buffer), .age = 0};

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
				goto sleep;
			}
			host = &arg->hosts[rand() % arg->hosts_len];

			if (cookie.age + 3600 < time(NULL)) {
				debug("refreshing auth cookie with age %lu\n", cookie.age);
				if (auth(host, &cookie) == -1) {
					goto sleep;
				}
			}

			if (downlink_create(&downlink, host, &cookie) != -1) {
				break;
			}

		sleep:
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

int downlink_create(downlink_t *downlink, host_t *host, cookie_t *cookie) {
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

	memcpy(&request.body.ptr[request.body.len], (uint16_t[]){hton16(downlink->frame)}, sizeof(downlink->frame));
	request.body.len += sizeof(downlink->frame);
	memcpy(&request.body.ptr[request.body.len], &downlink->kind, sizeof(downlink->kind));
	request.body.len += sizeof(downlink->kind);
	memcpy(&request.body.ptr[request.body.len], &downlink->data_len, sizeof(downlink->data_len));
	request.body.len += sizeof(downlink->data_len);
	memcpy(&request.body.ptr[request.body.len], downlink->data, downlink->data_len);
	request.body.len += downlink->data_len;
	memcpy(&request.body.ptr[request.body.len], (uint16_t[]){hton16(downlink->airtime)}, sizeof(downlink->airtime));
	request.body.len += sizeof(downlink->airtime);
	memcpy(&request.body.ptr[request.body.len], (uint32_t[]){hton32(downlink->frequency)}, sizeof(downlink->frequency));
	request.body.len += sizeof(downlink->frequency);
	memcpy(&request.body.ptr[request.body.len], (uint32_t[]){hton32(downlink->bandwidth)}, sizeof(downlink->bandwidth));
	request.body.len += sizeof(downlink->bandwidth);
	memcpy(&request.body.ptr[request.body.len], &downlink->spreading_factor, sizeof(downlink->spreading_factor));
	request.body.len += sizeof(downlink->spreading_factor);
	memcpy(&request.body.ptr[request.body.len], &downlink->coding_rate, sizeof(downlink->coding_rate));
	request.body.len += sizeof(downlink->coding_rate);
	memcpy(&request.body.ptr[request.body.len], &downlink->tx_power, sizeof(downlink->tx_power));
	request.body.len += sizeof(downlink->tx_power);
	memcpy(&request.body.ptr[request.body.len], &downlink->preamble_len, sizeof(downlink->preamble_len));
	request.body.len += sizeof(downlink->preamble_len);
	memcpy(&request.body.ptr[request.body.len], (time_t[]){(time_t)hton64((uint64_t)downlink->sent_at)},
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
		warn("host rejected downlink with status %hu\n", response.status);
		return 0;
	}

	if (response.status != 201) {
		error("host rejected downlink with status %hu\n", response.status);
		return -1;
	}

	info("successfully created downlink\n");
	return 0;
}
