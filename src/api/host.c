#include "host.h"
#include "../lib/base16.h"
#include "../lib/endian.h"
#include "../lib/format.h"
#include "../lib/logger.h"
#include "../lib/octet.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char *host_file = "host";

const host_row_t host_row = {
		.id = 0,
		.address_len = 16,
		.address = 17,
		.port = 49,
		.username_len = 51,
		.username = 52,
		.password_len = 68,
		.password = 69,
		.size = 133,
};

int host_rowcmp(uint8_t *alpha, uint8_t *bravo, host_query_t *query) {
	if (query->order_len == 2 && memcmp(query->order, "id", query->order_len) == 0) {
		uint64_t id_alpha = octet_uint64_read(alpha, host_row.id);
		uint64_t id_bravo = octet_uint64_read(bravo, host_row.id);
		int result = (id_alpha > id_bravo) - (id_alpha < id_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 7 && memcmp(query->order, "address", query->order_len) == 0) {
		uint8_t address_len_alpha = octet_uint8_read(alpha, host_row.address_len);
		char *address_alpha = octet_text_read(alpha, host_row.address);
		uint8_t address_len_bravo = octet_uint8_read(bravo, host_row.address_len);
		char *address_bravo = octet_text_read(bravo, host_row.address);
		int result =
				memcmp(address_alpha, address_bravo, address_len_alpha < address_len_bravo ? address_len_alpha : address_len_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 4 && memcmp(query->order, "port", query->order_len) == 0) {
		uint16_t port_alpha = octet_uint16_read(alpha, host_row.port);
		uint16_t port_bravo = octet_uint16_read(bravo, host_row.port);
		int result = (port_alpha > port_bravo) - (port_alpha < port_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 8 && memcmp(query->order, "username", query->order_len) == 0) {
		uint8_t username_len_alpha = octet_uint8_read(alpha, host_row.username_len);
		char *username_alpha = octet_text_read(alpha, host_row.username);
		uint8_t username_len_bravo = octet_uint8_read(bravo, host_row.username_len);
		char *username_bravo = octet_text_read(bravo, host_row.username);
		int result = memcmp(username_alpha, username_bravo,
												username_len_alpha < username_len_bravo ? username_len_alpha : username_len_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 8 && memcmp(query->order, "password", query->order_len) == 0) {
		uint8_t password_len_alpha = octet_uint8_read(alpha, host_row.password_len);
		char *password_alpha = octet_text_read(alpha, host_row.password);
		uint8_t password_len_bravo = octet_uint8_read(bravo, host_row.password_len);
		char *password_bravo = octet_text_read(bravo, host_row.password);
		int result = memcmp(password_alpha, password_bravo,
												password_len_alpha < password_len_bravo ? password_len_alpha : password_len_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	return 0;
}

uint16_t host_select(octet_t *db, host_query_t *query, response_t *response, uint8_t *hosts_len) {
	uint16_t status;

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

	debug("select hosts order by %.*s:%.*s\n", (int)query->order_len, query->order, (int)query->sort_len, query->sort);

	off_t offset = 0;
	uint32_t table_len = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			status = 0;
			break;
		}
		if (octet_row_read(&stmt, file, offset, &db->table[table_len], host_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		table_len += host_row.size;
		offset += host_row.size;
	}

	if (table_len >= host_row.size * 2) {
		for (uint8_t index = 0; index < table_len / host_row.size - 1; index++) {
			for (uint8_t ind = index + 1; ind < table_len / host_row.size; ind++) {
				if (host_rowcmp(&db->table[index * host_row.size], &db->table[ind * host_row.size], query) > 0) {
					memcpy(db->row, &db->table[index * host_row.size], host_row.size);
					memcpy(&db->table[index * host_row.size], &db->table[ind * host_row.size], host_row.size);
					memcpy(&db->table[ind * host_row.size], db->row, host_row.size);
				}
			}
		}
	}

	uint32_t index = host_row.size * query->offset;
	while (true) {
		if (index >= table_len || *hosts_len >= query->limit) {
			status = 0;
			break;
		}
		uint8_t (*id)[16] = (uint8_t (*)[16])octet_blob_read(&db->table[index], host_row.id);
		uint8_t address_len = octet_uint8_read(&db->table[index], host_row.address_len);
		char *address = octet_text_read(&db->table[index], host_row.address);
		uint16_t port = octet_uint16_read(&db->table[index], host_row.port);
		uint8_t username_len = octet_uint8_read(&db->table[index], host_row.username_len);
		char *username = octet_text_read(&db->table[index], host_row.username);
		uint8_t password_len = octet_uint8_read(&db->table[index], host_row.password_len);
		char *password = octet_text_read(&db->table[index], host_row.password);
		body_write(response, id, sizeof(*id));
		body_write(response, address, address_len);
		body_write(response, (char[]){0x00}, sizeof(char));
		body_write(response, (uint16_t[]){hton16(port)}, sizeof(port));
		body_write(response, username, username_len);
		body_write(response, (char[]){0x00}, sizeof(char));
		body_write(response, password, password_len);
		body_write(response, (char[]){0x00}, sizeof(char));
		*hosts_len += 1;
		index += host_row.size;
	}

cleanup:
	octet_close(&stmt, file);
	return status;
}

int host_parse(host_t *host, request_t *request) {
	request->body.pos = 0;

	uint8_t stage = 0;

	host->address_len = 0;
	const uint8_t address_index = (uint8_t)request->body.pos;
	while (stage == 0 && host->address_len < 32 && request->body.pos < request->body.len) {
		const char *byte = body_read(request, sizeof(char));
		if (*byte == '\0') {
			stage = 1;
		} else {
			host->address_len++;
		}
	}
	host->address = &request->body.ptr[address_index];
	if (stage != 1) {
		debug("found address with %hhu bytes\n", host->address_len);
		return -1;
	}

	if (request->body.len < request->body.pos + sizeof(host->port)) {
		debug("missing port on host\n");
		return -1;
	}
	memcpy(&host->port, body_read(request, sizeof(host->port)), sizeof(host->port));
	host->port = ntoh16(host->port);

	host->username_len = 0;
	const uint8_t username_index = (uint8_t)request->body.pos;
	while (stage == 1 && host->username_len < 16 && request->body.pos < request->body.len) {
		const char *byte = body_read(request, sizeof(char));
		if (*byte == '\0') {
			stage = 2;
		} else {
			host->username_len++;
		}
	}
	host->username = &request->body.ptr[username_index];
	if (stage != 2) {
		debug("found username with %hhu bytes\n", host->username_len);
		return -1;
	}

	host->password_len = 0;
	const uint8_t password_index = (uint8_t)request->body.pos;
	while (stage == 2 && host->password_len < 64 && request->body.pos < request->body.len) {
		const char *byte = body_read(request, sizeof(char));
		if (*byte == '\0') {
			stage = 3;
		} else {
			host->password_len++;
		}
	}
	host->password = &request->body.ptr[password_index];
	if (stage != 3) {
		debug("found password with %hhu bytes\n", host->password_len);
		return -1;
	}

	return 0;
}

int host_validate(host_t *host) {
	if (host->address_len < 4) {
		return -1;
	}

	if (host->port < 1) {
		debug("invalid port %u on host\n", host->port);
		return -1;
	}

	if (host->username_len < 4) {
		return -1;
	}

	uint8_t username_index = 0;
	while (username_index < host->username_len) {
		char *byte = &host->username[username_index];
		if (*byte < 'a' || *byte > 'z') {
			debug("username contains invalid character %02x\n", *byte);
			return -1;
		}
		username_index++;
	}

	if (host->password_len < 4) {
		return -1;
	}

	bool lower = false;
	bool upper = false;
	bool digit = false;

	uint8_t password_index = 0;
	while (password_index < host->password_len) {
		char *byte = &host->password[password_index];
		if (*byte >= '0' && *byte <= '9') {
			digit = true;
		} else if (*byte >= 'a' && *byte <= 'z') {
			lower = true;
		} else if (*byte >= 'A' && *byte <= 'Z') {
			upper = true;
		}
		password_index++;
	}

	if (!lower || !upper || !digit) {
		debug("password contains lower %s upper %s digit %s \n", human_bool(lower), human_bool(upper), human_bool(digit));
		return -1;
	}

	return 0;
}

uint16_t host_insert(octet_t *db, host_t *host) {
	uint16_t status;

	for (uint8_t index = 0; index < sizeof(*host->id); index++) {
		(*host->id)[index] = (uint8_t)(rand() & 0xff);
	}

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, host_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("insert host %.*s port %hu\n", host->address_len, host->address, host->port);

	off_t offset = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			break;
		}
		if (octet_row_read(&stmt, file, offset, db->row, host_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t address_len = octet_uint8_read(db->row, host_row.address_len);
		char *address = octet_text_read(db->row, host_row.address);
		uint16_t port = octet_uint16_read(db->row, host_row.port);
		if (address_len == host->address_len && memcmp(address, host->address, host->address_len) == 0 && port == host->port) {
			status = 409;
			warn("address %.*s and port %hu already taken\n", host->address_len, host->address, host->port);
			goto cleanup;
		}
		offset += host_row.size;
	}

	octet_blob_write(db->row, host_row.id, (uint8_t *)host->id, sizeof(*host->id));
	octet_uint8_write(db->row, host_row.address_len, host->address_len);
	octet_text_write(db->row, host_row.address, host->address, host->address_len);
	octet_uint16_write(db->row, host_row.port, host->port);
	octet_uint8_write(db->row, host_row.username_len, host->username_len);
	octet_text_write(db->row, host_row.username, host->username, host->username_len);
	octet_uint8_write(db->row, host_row.password_len, host->password_len);
	octet_text_write(db->row, host_row.password, host->password, host->password_len);

	offset = stmt.stat.st_size;
	if (octet_row_write(&stmt, file, offset, db->row, host_row.size) == -1) {
		status = octet_error();
		goto cleanup;
	}

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

uint16_t host_update(octet_t *db, host_t *host) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, host_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("update host %02x%02x\n", (*host->id)[0], (*host->id)[1]);

	off_t offset = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			warn("host %02x%02x not found\n", (*host->id)[0], (*host->id)[1]);
			status = 404;
			break;
		}
		if (octet_row_read(&stmt, file, offset, db->row, host_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t (*id)[16] = (uint8_t (*)[16])octet_blob_read(db->row, host_row.id);
		if (memcmp(id, host->id, sizeof(*host->id)) == 0) {
			octet_uint8_write(db->row, host_row.address_len, host->address_len);
			octet_text_write(db->row, host_row.address, (char *)host->address, host->address_len);
			octet_uint16_write(db->row, host_row.port, host->port);
			octet_uint8_write(db->row, host_row.username_len, host->username_len);
			octet_text_write(db->row, host_row.username, (char *)host->username, host->username_len);
			octet_uint8_write(db->row, host_row.password_len, host->password_len);
			octet_text_write(db->row, host_row.password, (char *)host->password, host->password_len);
			if (octet_row_write(&stmt, file, offset, db->row, host_row.size) == -1) {
				status = octet_error();
				goto cleanup;
			}
			status = 0;
			break;
		}
		offset += host_row.size;
	}

cleanup:
	octet_close(&stmt, file);
	return status;
}

uint16_t host_delete(octet_t *db, host_t *host) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, host_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("delete host %02x%02x\n", (*host->id)[0], (*host->id)[1]);

	off_t offset = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			warn("user %02x%02x not found\n", (*host->id)[0], (*host->id)[1]);
			status = 404;
			goto cleanup;
		}
		if (octet_row_read(&stmt, file, offset, db->row, host_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t (*id)[16] = (uint8_t (*)[16])octet_blob_read(db->row, host_row.id);
		if (memcmp(id, host->id, sizeof(*host->id)) == 0) {
			break;
		}
		offset += host_row.size;
	}

	off_t index = offset + host_row.size;
	while (index < stmt.stat.st_size) {
		if (octet_row_read(&stmt, file, index, db->row, host_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		if (octet_row_write(&stmt, file, index - host_row.size, db->row, host_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		index += host_row.size;
	}

	if (octet_trunc(&stmt, file, stmt.stat.st_size - host_row.size)) {
		status = 500;
		goto cleanup;
	}

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

void host_find(octet_t *db, request_t *request, response_t *response) {
	host_query_t query = {.limit = 16, .offset = 0};
	if (strnfind(request->search.ptr, request->search.len, "order=", "&", &query.order, &query.order_len, 16) == -1) {
		response->status = 400;
		return;
	}

	if (strnfind(request->search.ptr, request->search.len, "sort=", "", &query.sort, &query.sort_len, 8) == -1) {
		response->status = 400;
		return;
	}

	uint8_t hosts_len = 0;
	uint16_t status = host_select(db, &query, response, &hosts_len);
	if (status != 0) {
		response->status = status;
		return;
	}

	header_write(response, "content-type:application/octet-stream\r\n");
	header_write(response, "content-length:%u\r\n", response->body.len);
	info("found %hhu hosts\n", hosts_len);
	response->status = 200;
}

void host_create(octet_t *db, request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	uint8_t id[16];
	host_t host = {.id = &id};
	if (request->body.len == 0 || host_parse(&host, request) == -1 || host_validate(&host) == -1) {
		response->status = 400;
		return;
	}

	uint16_t status = host_insert(db, &host);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("created host %02x%02x\n", (*host.id)[0], (*host.id)[1]);
	response->status = 201;
}

void host_modify(octet_t *db, request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	uint8_t uuid_len = 0;
	const char *uuid = param_find(request, 10, &uuid_len);
	if (uuid_len != sizeof(*((host_t *)0)->id) * 2) {
		warn("uuid length %hhu does not match %zu\n", uuid_len, sizeof(*((host_t *)0)->id) * 2);
		response->status = 400;
		return;
	}

	uint8_t id[16];
	if (base16_decode(id, sizeof(id), uuid, uuid_len) != 0) {
		warn("failed to decode uuid from base 16\n");
		response->status = 400;
		return;
	}

	host_t host = {.id = &id};
	if (request->body.len == 0 || host_parse(&host, request) == -1 || host_validate(&host) == -1) {
		response->status = 400;
		return;
	}

	uint16_t status = host_update(db, &host);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("updated host %02x%02x\n", (*host.id)[0], (*host.id)[1]);
	response->status = 200;
}

void host_remove(octet_t *db, request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	uint8_t uuid_len = 0;
	const char *uuid = param_find(request, 10, &uuid_len);
	if (uuid_len != sizeof(*((host_t *)0)->id) * 2) {
		warn("uuid length %hhu does not match %zu\n", uuid_len, sizeof(*((host_t *)0)->id) * 2);
		response->status = 400;
		return;
	}

	uint8_t id[16];
	if (base16_decode(id, sizeof(id), uuid, uuid_len) != 0) {
		warn("failed to decode uuid from base 16\n");
		response->status = 400;
		return;
	}

	host_t host = {.id = &id};
	uint16_t status = host_delete(db, &host);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("deleted host %02x%02x\n", (*host.id)[0], (*host.id)[1]);
	response->status = 200;
}
