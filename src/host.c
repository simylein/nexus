#include "host.h"
#include "logger.h"
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char *host_table = "host";
const char *host_schema = "create table host ("
													"id blob primary key, "
													"address text not null, "
													"port integer not null"
													")";

uint16_t host_select(sqlite3 *database, host_t (*hosts)[16], uint8_t *hosts_len) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char sql[] = "select id, address, port from host";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, sizeof(sql), &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	while (true) {
		int result = sqlite3_step(stmt);
		if (result == SQLITE_ROW) {
			if (*hosts_len >= sizeof(*hosts) / sizeof(host_t)) {
				error("radio length %hu exceeds buffer length %zu\n", *hosts_len, sizeof(*hosts) / sizeof(host_t));
				status = 500;
				goto cleanup;
			}
			const uint8_t *id = sqlite3_column_blob(stmt, 0);
			const size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
			if (id_len != sizeof(((host_t *)0)->id)) {
				error("id length %zu does not match buffer length %zu\n", id_len, sizeof(((host_t *)0)->id));
				status = 500;
				goto cleanup;
			}
			const uint8_t *address = sqlite3_column_text(stmt, 1);
			const size_t address_len = (size_t)sqlite3_column_bytes(stmt, 1);
			if (address_len > sizeof(((host_t *)0)->address)) {
				error("address length %zu exceeds buffer length %zu\n", address_len, sizeof(((host_t *)0)->address));
				status = 500;
				goto cleanup;
			}
			const uint16_t port = (uint16_t)sqlite3_column_int(stmt, 2);
			memcpy((*hosts)[*hosts_len].id, id, id_len);
			memcpy((*hosts)[*hosts_len].address, address, address_len);
			(*hosts)[*hosts_len].address_len = (uint8_t)address_len;
			(*hosts)[*hosts_len].port = port;
			*hosts_len += 1;
		} else if (result == SQLITE_DONE) {
			status = 0;
			break;
		} else {
			error("failed to execute statement because %s\n", sqlite3_errmsg(database));
			status = 500;
			goto cleanup;
		}
	}

cleanup:
	sqlite3_finalize(stmt);
	return status;
}
