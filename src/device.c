#include "device.h"
#include "logger.h"
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char *device_table = "device";
const char *device_schema = "create table device ("
														"id blob primary key, "
														"tag blob not null unique"
														")";

uint16_t device_select(sqlite3 *database, device_t (*devices)[16], uint8_t *devices_len) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char sql[] = "select id, tag from device";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, sizeof(sql), &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	while (true) {
		int result = sqlite3_step(stmt);
		if (result == SQLITE_ROW) {
			if (*devices_len >= sizeof(*devices) / sizeof(device_t)) {
				error("device length %hu exceeds buffer length %zu\n", *devices_len, sizeof(*devices) / sizeof(device_t));
				status = 500;
				goto cleanup;
			}
			const uint8_t *id = sqlite3_column_blob(stmt, 0);
			const size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
			if (id_len != sizeof(((device_t *)0)->id)) {
				error("id length %zu does not match buffer length %zu\n", id_len, sizeof(((device_t *)0)->id));
				status = 500;
				goto cleanup;
			}
			const uint8_t *tag = sqlite3_column_blob(stmt, 1);
			const size_t tag_len = (size_t)sqlite3_column_bytes(stmt, 1);
			if (tag_len != sizeof(((device_t *)0)->tag)) {
				error("tag length %zu does not match buffer length %zu\n", tag_len, sizeof(((device_t *)0)->tag));
				status = 500;
				goto cleanup;
			}
			memcpy((*devices)[*devices_len].id, id, id_len);
			memcpy((*devices)[*devices_len].tag, tag, tag_len);
			*devices_len += 1;
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

uint16_t device_insert(sqlite3 *database, device_t *device) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char sql[] = "insert into device (id, tag) "
										 "values (randomblob(16), ?) returning id";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, sizeof(sql), &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	sqlite3_bind_blob(stmt, 1, device->tag, sizeof(device->tag), SQLITE_STATIC);

	int result = sqlite3_step(stmt);
	if (result == SQLITE_ROW) {
		const uint8_t *id = sqlite3_column_blob(stmt, 0);
		const size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
		if (id_len != sizeof(device->id)) {
			error("id length %zu does not match buffer length %zu\n", id_len, sizeof(device->id));
			status = 500;
			goto cleanup;
		}
		memcpy(device->id, id, id_len);
		status = 0;
	} else if (result == SQLITE_CONSTRAINT) {
		warn("device tag %02x%02x already taken\n", device->tag[0], device->tag[1]);
		status = 409;
		goto cleanup;
	} else {
		error("failed to execute statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

cleanup:
	sqlite3_finalize(stmt);
	return status;
}
