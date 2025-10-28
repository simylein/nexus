#include "./device.h"
#include "../lib/base16.h"
#include "../lib/logger.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

const char *device_table = "device";
const char *device_schema = "create table device ("
														"id blob primary key, "
														"tag blob not null unique"
														")";

uint16_t device_select(sqlite3 *database, device_query_t *query, response_t *response, uint8_t *device_len) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char *sql = "select "
										"device.id, device.tag "
										"from device "
										"order by "
										"case when ?1 = 'id' and ?2 = 'asc' then device.id end asc, "
										"case when ?1 = 'id' and ?2 = 'desc' then device.id end desc, "
										"case when ?1 = 'tag' and ?2 = 'asc' then device.tag end asc, "
										"case when ?1 = 'tag' and ?2 = 'desc' then device.tag end desc";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	sqlite3_bind_text(stmt, 1, query->order, query->order_len, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, query->sort, query->sort_len, SQLITE_STATIC);

	while (true) {
		int result = sqlite3_step(stmt);
		if (result == SQLITE_ROW) {
			const uint8_t *id = sqlite3_column_blob(stmt, 0);
			const size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
			if (id_len != sizeof(*((device_t *)0)->id)) {
				error("id length %zu does not match buffer length %zu\n", id_len, sizeof(*((device_t *)0)->id));
				status = 500;
				goto cleanup;
			}
			const uint8_t *tag = sqlite3_column_blob(stmt, 1);
			const size_t tag_len = (size_t)sqlite3_column_bytes(stmt, 1);
			if (tag_len != sizeof(*((device_t *)0)->tag)) {
				error("tag length %zu does not match buffer length %zu\n", tag_len, sizeof(*((device_t *)0)->tag));
				status = 500;
				goto cleanup;
			}
			body_write(response, id, id_len);
			body_write(response, tag, tag_len);
			*device_len += 1;
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

int device_parse(device_t *device, request_t *request) {
	request->body.pos = 0;

	if (request->body.len < request->body.pos + sizeof(*device->id)) {
		debug("missing id on device\n");
		return -1;
	}
	device->id = (uint8_t (*)[16])body_read(request, sizeof(*device->id));

	if (request->body.len < request->body.pos + sizeof(*device->tag)) {
		debug("missing tag on device\n");
		return -1;
	}
	device->tag = (uint8_t (*)[2])body_read(request, sizeof(*device->tag));

	return 0;
}

int device_validate(device_t *device) {
	if (memcmp(device->id, device->tag, sizeof(*device->tag)) != 0) {
		debug("invalid id %02x%02x for tag %02x%02x on device\n", (*device->id)[0], (*device->id)[1], (*device->tag)[0],
					(*device->tag)[1]);
		return -1;
	}

	return 0;
}

uint16_t device_insert(sqlite3 *database, device_t *device) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char *sql = "insert into device (id, tag) "
										"values (?, ?) returning id";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	sqlite3_bind_blob(stmt, 1, device->id, sizeof(*device->id), SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 2, device->tag, sizeof(*device->tag), SQLITE_STATIC);

	int result = sqlite3_step(stmt);
	if (result == SQLITE_ROW) {
		const uint8_t *id = sqlite3_column_blob(stmt, 0);
		const size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
		if (id_len != sizeof(*device->id)) {
			error("id length %zu does not match buffer length %zu\n", id_len, sizeof(*device->id));
			status = 500;
			goto cleanup;
		}
		memcpy(device->id, id, id_len);
		status = 0;
	} else if (result == SQLITE_CONSTRAINT) {
		warn("device tag %02x%02x already taken\n", (*device->tag)[0], (*device->tag)[1]);
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

uint16_t device_delete(sqlite3 *database, device_t *device) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char *sql = "delete from device "
										"where id = ?";

	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	sqlite3_bind_blob(stmt, 1, device->id, sizeof(*device->id), SQLITE_STATIC);

	int result = sqlite3_step(stmt);
	if (result != SQLITE_DONE) {
		error("failed to execute statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	if (sqlite3_changes(database) == 0) {
		warn("device %02x%02x not found\n", (*device->id)[0], (*device->id)[1]);
		status = 404;
		goto cleanup;
	}

	status = 0;

cleanup:
	sqlite3_finalize(stmt);
	return status;
}

void device_find(sqlite3 *database, request_t *request, response_t *response) {
	device_query_t query;
	if (strnfind(request->search.ptr, request->search.len, "order=", "&", (const char **)&query.order, (size_t *)&query.order_len,
							 16) == -1) {
		response->status = 400;
		return;
	}

	if (strnfind(request->search.ptr, request->search.len, "sort=", "", (const char **)&query.sort, (size_t *)&query.sort_len,
							 8) == -1) {
		response->status = 400;
		return;
	}

	uint8_t devices_len = 0;
	uint16_t status = device_select(database, &query, response, &devices_len);
	if (status != 0) {
		response->status = status;
		return;
	}

	header_write(response, "content-type:application/octet-stream\r\n");
	header_write(response, "content-length:%u\r\n", response->body.len);
	info("found %hhu devices\n", devices_len);
	response->status = 200;
}

void device_create(sqlite3 *database, request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	uint8_t id[16];
	device_t device = {.id = &id};
	if (request->body.len == 0 || device_parse(&device, request) == -1 || device_validate(&device) == -1) {
		response->status = 400;
		return;
	}

	uint16_t status = device_insert(database, &device);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("created device %02x%02x\n", (*device.id)[0], (*device.id)[1]);
	response->status = 201;
}

void device_remove(sqlite3 *database, request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	uint8_t uuid_len = 0;
	const char *uuid = param_find(request, 13, &uuid_len);
	if (uuid_len != sizeof(*((device_t *)0)->id) * 2) {
		warn("uuid length %hhu does not match %zu\n", uuid_len, sizeof(*((device_t *)0)->id) * 2);
		response->status = 400;
		return;
	}

	uint8_t id[16];
	if (base16_decode(id, sizeof(id), uuid, uuid_len) != 0) {
		warn("failed to decode uuid from base 16\n");
		response->status = 400;
		return;
	}

	device_t device = {.id = &id};
	uint16_t status = device_delete(database, &device);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("deleted device %02x%02x\n", (*device.id)[0], (*device.id)[1]);
	response->status = 200;
}
