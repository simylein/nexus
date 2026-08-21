#include "uplink.h"
#include "../api/uplink.h"
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

uplinks_t uplinks = {
		.head = 0,
		.tail = 0,
		.size = 0,
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.filled = PTHREAD_COND_INITIALIZER,
		.available = PTHREAD_COND_INITIALIZER,
};

int uplink_init(octet_t *db) {
	int status;
	octet_stmt_t stmt_host = {.fd = -1};
	octet_stmt_t stmt_uplink = {.fd = -1};

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, host_file) == -1) {
		error("failed to sprintf to file\n");
		status = -1;
		goto cleanup;
	}

	if (octet_open(&stmt_host, file, O_RDONLY, F_RDLCK) == -1) {
		status = -1;
		goto cleanup;
	}

	if (stmt_host.stat.st_size > db->table_len) {
		error("file length %zu exceeds buffer length %u\n", (size_t)stmt_host.stat.st_size, db->table_len);
		status = -1;
		goto cleanup;
	}

	debug("select hosts\n");

	host_t *hosts = NULL;
	uint8_t hosts_len = 0;
	off_t offset = 0;
	while (true) {
		if (offset >= stmt_host.stat.st_size) {
			status = 0;
			break;
		}
		if (octet_row_read(&stmt_host, file, offset, db->row, host_row.size) == -1) {
			status = -1;
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

	if (sprintf(file, "%s/%s.data", db->directory, uplink_file) == -1) {
		error("failed to sprintf to file\n");
		status = -1;
		goto cleanup;
	}

	if (octet_open(&stmt_uplink, file, O_RDONLY, F_RDLCK) == -1) {
		status = -1;
		goto cleanup;
	}

	debug("select uplinks\n");

	time_t received_at_max = 0;
	time_t received_at_min = 0;
	uint8_t null[512];
	memset(null, 0x00, uplink_row.size);
	offset = 0;
	while (true) {
		if (offset >= stmt_uplink.stat.st_size) {
			status = 0;
			break;
		}
		if (octet_row_read(&stmt_uplink, file, offset, db->row, uplink_row.size) == -1) {
			status = -1;
			goto cleanup;
		}
		time_t received_at = (time_t)octet_uint64_read(db->row, uplink_row.received_at);
		if (memcmp(db->row, null, uplink_row.size) != 0) {
			if (received_at < received_at_min || received_at_min == 0) {
				uplinks.head = (uint16_t)(offset / uplink_row.size);
				received_at_min = received_at;
			}
			if (received_at >= received_at_max) {
				uplinks.tail = (uint16_t)(offset / uplink_row.size);
				received_at_max = received_at;
			}
			uplinks.size++;
		}
		offset += uplink_row.size;
	}
	if (uplinks.size != 0) {
		uplinks.tail = (uint16_t)(uplinks.tail + 1) % uplinks_size;
	}

	uplinks.worker.arg.db.directory = database_directory;
	uplinks.worker.arg.db.row = malloc(512);
	if (uplinks.worker.arg.db.row == NULL) {
		fatal("failed to allocate %hu bytes for uplinks because %s\n", 512, errno_str());
		status = -1;
		goto cleanup;
	}
	uplinks.worker.arg.db.row_len = 512;

	uplinks.worker.arg.hosts = hosts;
	uplinks.worker.arg.hosts_len = hosts_len;
	if (uplink_spawn(&uplinks.worker.thread, uplink_thread, &uplinks.worker.arg) == -1) {
		status = -1;
		goto cleanup;
	}

cleanup:
	octet_close(&stmt_host, file);
	octet_close(&stmt_uplink, file);
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

		uplink_t uplink;
		if (uplink_select_one(&arg->db, &uplink, &uplinks.head) != 0) {
			goto unlock;
		}
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
		if (uplink_delete(&arg->db, &uplink, &uplinks.head) != 0) {
			goto unlock;
		}
		uplinks.head = (uint16_t)((uplinks.head + 1) % uplinks_size);
		uplinks.size--;
		trace("uplink thread decreased queue size to %hu\n", uplinks.size);
		pthread_cond_signal(&uplinks.available);

	unlock:
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

	memcpy(&request.body.ptr[request.body.len], (uint16_t[]){hton16(uplink->frame)}, sizeof(uplink->frame));
	request.body.len += sizeof(uplink->frame);
	memcpy(&request.body.ptr[request.body.len], &uplink->kind, sizeof(uplink->kind));
	request.body.len += sizeof(uplink->kind);
	memcpy(&request.body.ptr[request.body.len], &uplink->data_len, sizeof(uplink->data_len));
	request.body.len += sizeof(uplink->data_len);
	memcpy(&request.body.ptr[request.body.len], uplink->data, uplink->data_len);
	request.body.len += uplink->data_len;
	memcpy(&request.body.ptr[request.body.len], (uint16_t[]){hton16(uplink->airtime)}, sizeof(uplink->airtime));
	request.body.len += sizeof(uplink->airtime);
	memcpy(&request.body.ptr[request.body.len], (uint32_t[]){hton32(uplink->frequency)}, sizeof(uplink->frequency));
	request.body.len += sizeof(uplink->frequency);
	memcpy(&request.body.ptr[request.body.len], (uint32_t[]){hton32(uplink->bandwidth)}, sizeof(uplink->bandwidth));
	request.body.len += sizeof(uplink->bandwidth);
	memcpy(&request.body.ptr[request.body.len], (uint16_t[]){hton16((uint16_t)uplink->rssi)}, sizeof(uplink->rssi));
	request.body.len += sizeof(uplink->rssi);
	memcpy(&request.body.ptr[request.body.len], &uplink->snr, sizeof(uplink->snr));
	request.body.len += sizeof(uplink->snr);
	memcpy(&request.body.ptr[request.body.len], &uplink->spreading_factor, sizeof(uplink->spreading_factor));
	request.body.len += sizeof(uplink->spreading_factor);
	memcpy(&request.body.ptr[request.body.len], &uplink->coding_rate, sizeof(uplink->coding_rate));
	request.body.len += sizeof(uplink->coding_rate);
	memcpy(&request.body.ptr[request.body.len], &uplink->checksum, sizeof(uplink->checksum));
	request.body.len += sizeof(uplink->checksum);
	memcpy(&request.body.ptr[request.body.len], &uplink->tx_power, sizeof(uplink->tx_power));
	request.body.len += sizeof(uplink->tx_power);
	memcpy(&request.body.ptr[request.body.len], &uplink->preamble_len, sizeof(uplink->preamble_len));
	request.body.len += sizeof(uplink->preamble_len);
	memcpy(&request.body.ptr[request.body.len], (time_t[]){(time_t)hton64((uint64_t)uplink->received_at)},
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
