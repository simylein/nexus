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
													"port integer not null, "
													"username text not null, "
													"password text not null"
													")";

uint16_t host_select(sqlite3 *database, host_t (*hosts)[16], uint8_t *hosts_len) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char sql[] = "select id, address, port, username, password from host";
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
			const uint8_t *username = sqlite3_column_text(stmt, 3);
			const size_t username_len = (size_t)sqlite3_column_bytes(stmt, 3);
			if (username_len > sizeof(((host_t *)0)->username)) {
				error("username length %zu exceeds buffer length %zu\n", username_len, sizeof(((host_t *)0)->username));
				status = 500;
				goto cleanup;
			}
			const uint8_t *password = sqlite3_column_text(stmt, 4);
			const size_t password_len = (size_t)sqlite3_column_bytes(stmt, 4);
			if (password_len > sizeof(((host_t *)0)->password)) {
				error("password length %zu exceeds buffer length %zu\n", password_len, sizeof(((host_t *)0)->password));
				status = 500;
				goto cleanup;
			}
			memcpy((*hosts)[*hosts_len].id, id, id_len);
			memcpy((*hosts)[*hosts_len].address, address, address_len);
			(*hosts)[*hosts_len].address_len = (uint8_t)address_len;
			(*hosts)[*hosts_len].port = port;
			memcpy((*hosts)[*hosts_len].username, username, username_len);
			(*hosts)[*hosts_len].username_len = (uint8_t)username_len;
			memcpy((*hosts)[*hosts_len].password, password, password_len);
			(*hosts)[*hosts_len].password_len = (uint8_t)password_len;
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
