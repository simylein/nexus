#include "./device.h"
#include "../lib/base16.h"
#include "../lib/logger.h"
#include "../lib/octet.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char *device_file = "device";

const device_row_t device_row = {
		.id = 0,
		.tag = 16,
		.key = 18,
		.size = 34,
};

int device_rowcmp(uint8_t *alpha, uint8_t *bravo, device_query_t *query) {
	if (query->order_len == 2 && memcmp(query->order, "id", query->order_len) == 0) {
		uint64_t id_alpha = octet_uint64_read(alpha, device_row.id);
		uint64_t id_bravo = octet_uint64_read(bravo, device_row.id);
		int result = (id_alpha > id_bravo) - (id_alpha < id_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 3 && memcmp(query->order, "tag", query->order_len) == 0) {
		uint16_t tag_alpha = octet_uint16_read(alpha, device_row.tag);
		uint16_t tag_bravo = octet_uint16_read(bravo, device_row.tag);
		int result = (tag_alpha > tag_bravo) - (tag_alpha < tag_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 3 && memcmp(query->order, "key", query->order_len) == 0) {
		uint64_t key_alpha = octet_uint64_read(alpha, device_row.key);
		uint64_t key_bravo = octet_uint64_read(bravo, device_row.key);
		int result = (key_alpha > key_bravo) - (key_alpha < key_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	return 0;
}

uint16_t device_select(octet_t *db, device_query_t *query, response_t *response, uint8_t *devices_len) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, device_file) == -1) {
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

	debug("select devices order by %.*s:%.*s\n", (int)query->order_len, query->order, (int)query->sort_len, query->sort);

	off_t offset = 0;
	uint32_t table_len = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			status = 0;
			break;
		}
		if (octet_row_read(&stmt, file, offset, &db->table[table_len], device_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		table_len += device_row.size;
		offset += device_row.size;
	}

	if (table_len >= device_row.size * 2) {
		for (uint8_t index = 0; index < table_len / device_row.size - 1; index++) {
			for (uint8_t ind = index + 1; ind < table_len / device_row.size; ind++) {
				if (device_rowcmp(&db->table[index * device_row.size], &db->table[ind * device_row.size], query) > 0) {
					memcpy(db->row, &db->table[index * device_row.size], device_row.size);
					memcpy(&db->table[index * device_row.size], &db->table[ind * device_row.size], device_row.size);
					memcpy(&db->table[ind * device_row.size], db->row, device_row.size);
				}
			}
		}
	}

	uint32_t index = device_row.size * query->offset;
	while (true) {
		if (index >= table_len || *devices_len >= query->limit) {
			status = 0;
			break;
		}
		uint8_t (*id)[16] = (uint8_t (*)[16])octet_blob_read(&db->table[index], device_row.id);
		uint8_t (*tag)[2] = (uint8_t (*)[2])octet_blob_read(&db->table[index], device_row.tag);
		uint8_t (*key)[16] = (uint8_t (*)[16])octet_blob_read(&db->table[index], device_row.key);
		body_write(response, id, sizeof(*id));
		body_write(response, tag, sizeof(*tag));
		body_write(response, key, sizeof(*key));
		*devices_len += 1;
		index += device_row.size;
	}

cleanup:
	octet_close(&stmt, file);
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

	if (request->body.len < request->body.pos + sizeof(*device->key)) {
		debug("missing key on device\n");
		return -1;
	}
	device->key = (uint8_t (*)[16])body_read(request, sizeof(*device->key));

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

uint16_t device_insert(octet_t *db, device_t *device) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, device_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("insert device %02x%02x\n", (*device->id)[0], (*device->id)[1]);

	off_t offset = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			break;
		}
		if (octet_row_read(&stmt, file, offset, db->row, device_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t (*id)[16] = (uint8_t (*)[16])octet_blob_read(db->row, device_row.id);
		if (memcmp(id, device->id, sizeof(*device->id)) == 0) {
			status = 409;
			warn("id %02x%02x already taken\n", (*device->id)[0], (*device->id)[1]);
			goto cleanup;
		}
		uint8_t (*tag)[2] = (uint8_t (*)[2])octet_blob_read(db->row, device_row.tag);
		if (memcmp(tag, device->tag, sizeof(*device->tag)) == 0) {
			status = 409;
			warn("tag %02x%02x already taken\n", (*device->tag)[0], (*device->tag)[1]);
			goto cleanup;
		}
		offset += device_row.size;
	}

	octet_blob_write(db->row, device_row.id, (uint8_t *)device->id, sizeof(*device->id));
	octet_blob_write(db->row, device_row.tag, (uint8_t *)device->tag, sizeof(*device->tag));
	octet_blob_write(db->row, device_row.key, (uint8_t *)device->key, sizeof(*device->key));

	offset = stmt.stat.st_size;
	if (octet_row_write(&stmt, file, offset, db->row, device_row.size) == -1) {
		status = octet_error();
		goto cleanup;
	}

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

uint16_t device_update(octet_t *db, uint8_t (*id)[16], device_t *device) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, device_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("update device %02x%02x\n", (*id)[0], (*id)[1]);

	off_t offset = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			warn("device %02x%02x not found\n", (*id)[0], (*id)[1]);
			status = 404;
			break;
		}
		if (octet_row_read(&stmt, file, offset, db->row, device_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t (*device_id)[16] = (uint8_t (*)[16])octet_blob_read(db->row, device_row.id);
		if (memcmp(device_id, id, sizeof(*id)) == 0) {
			octet_blob_write(db->row, device_row.id, (uint8_t *)device->id, sizeof(*device->id));
			octet_blob_write(db->row, device_row.tag, (uint8_t *)device->tag, sizeof(*device->tag));
			octet_blob_write(db->row, device_row.key, (uint8_t *)device->key, sizeof(*device->key));
			if (octet_row_write(&stmt, file, offset, db->row, device_row.size) == -1) {
				status = octet_error();
				goto cleanup;
			}
			status = 0;
			break;
		}
		offset += device_row.size;
	}

cleanup:
	octet_close(&stmt, file);
	return status;
}

uint16_t device_delete(octet_t *db, device_t *device) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, device_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("delete device %02x%02x\n", (*device->id)[0], (*device->id)[1]);

	off_t offset = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			warn("user %02x%02x not found\n", (*device->id)[0], (*device->id)[1]);
			status = 404;
			goto cleanup;
		}
		if (octet_row_read(&stmt, file, offset, db->row, device_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t (*id)[16] = (uint8_t (*)[16])octet_blob_read(db->row, device_row.id);
		if (memcmp(id, device->id, sizeof(*device->id)) == 0) {
			break;
		}
		offset += device_row.size;
	}

	off_t index = offset + device_row.size;
	while (index < stmt.stat.st_size) {
		if (octet_row_read(&stmt, file, index, db->row, device_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		if (octet_row_write(&stmt, file, index - device_row.size, db->row, device_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		index += device_row.size;
	}

	if (octet_trunc(&stmt, file, stmt.stat.st_size - device_row.size)) {
		status = 500;
		goto cleanup;
	}

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

void device_find(octet_t *db, request_t *request, response_t *response) {
	device_query_t query = {.limit = 16, .offset = 0};
	if (strnfind(request->search.ptr, request->search.len, "order=", "&", &query.order, &query.order_len, 16) == -1) {
		response->status = 400;
		return;
	}

	if (strnfind(request->search.ptr, request->search.len, "sort=", "", &query.sort, &query.sort_len, 8) == -1) {
		response->status = 400;
		return;
	}

	uint8_t devices_len = 0;
	uint16_t status = device_select(db, &query, response, &devices_len);
	if (status != 0) {
		response->status = status;
		return;
	}

	header_write(response, "content-type:application/octet-stream\r\n");
	header_write(response, "content-length:%u\r\n", response->body.len);
	info("found %hhu devices\n", devices_len);
	response->status = 200;
}

void device_create(octet_t *db, request_t *request, response_t *response) {
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

	uint16_t status = device_insert(db, &device);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("created device %02x%02x\n", (*device.id)[0], (*device.id)[1]);
	response->status = 201;
}

void device_modify(octet_t *db, request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	uint8_t uuid_len = 0;
	const char *uuid = param_find(request, 12, &uuid_len);
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

	device_t device;
	if (request->body.len == 0 || device_parse(&device, request) == -1) {
		response->status = 400;
		return;
	}

	uint16_t status = device_update(db, &id, &device);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("updated device %02x%02x\n", (*device.id)[0], (*device.id)[1]);
	response->status = 200;
}

void device_remove(octet_t *db, request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	uint8_t uuid_len = 0;
	const char *uuid = param_find(request, 12, &uuid_len);
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
	uint16_t status = device_delete(db, &device);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("deleted device %02x%02x\n", (*device.id)[0], (*device.id)[1]);
	response->status = 200;
}
