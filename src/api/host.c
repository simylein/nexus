#include "host.h"
#include "../lib/endian.h"
#include "../lib/logger.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

const char *host_table = "host";
const char *host_schema = "create table host ( "
													"id blob primary key, "
													"address text not null, "
													"port integer not null, "
													"username text not null, "
													"password text not null"
													")";

uint16_t host_select(sqlite3 *database, host_query_t *query, response_t *response, uint8_t *hosts_len) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char *sql = "select "
										"host.id, host.address, host.port, host.username, host.password "
										"from host "
										"order by "
										"case when ?1 = 'id' and ?2 = 'asc' then host.id end asc, "
										"case when ?1 = 'id' and ?2 = 'desc' then host.id end desc, "
										"case when ?1 = 'address' and ?2 = 'asc' then host.address end asc, "
										"case when ?1 = 'address' and ?2 = 'desc' then host.address end desc, "
										"case when ?1 = 'port' and ?2 = 'asc' then host.port end asc, "
										"case when ?1 = 'port' and ?2 = 'desc' then host.port end desc, "
										"case when ?1 = 'username' and ?2 = 'asc' then host.username end asc, "
										"case when ?1 = 'username' and ?2 = 'desc' then host.username end desc, "
										"case when ?1 = 'password' and ?2 = 'asc' then host.password end asc, "
										"case when ?1 = 'password' and ?2 = 'desc' then host.password end desc";
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
			if (id_len != sizeof(*((host_t *)0)->id)) {
				error("id length %zu does not match buffer length %zu\n", id_len, sizeof(*((host_t *)0)->id));
				status = 500;
				goto cleanup;
			}
			const uint8_t *address = sqlite3_column_text(stmt, 1);
			const size_t address_len = (size_t)sqlite3_column_bytes(stmt, 1);
			const uint16_t port = (uint16_t)sqlite3_column_int(stmt, 2);
			const uint8_t *username = sqlite3_column_text(stmt, 3);
			const size_t username_len = (size_t)sqlite3_column_bytes(stmt, 3);
			const uint8_t *password = sqlite3_column_text(stmt, 4);
			const size_t password_len = (size_t)sqlite3_column_bytes(stmt, 4);
			body_write(response, id, id_len);
			body_write(response, address, address_len);
			body_write(response, (char[]){0x00}, sizeof(char));
			body_write(response, (uint16_t[]){hton16(port)}, sizeof(port));
			body_write(response, username, username_len);
			body_write(response, (char[]){0x00}, sizeof(char));
			body_write(response, password, password_len);
			body_write(response, (char[]){0x00}, sizeof(char));
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

uint16_t host_insert(sqlite3 *database, host_t *host) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char *sql = "insert into host (id, address, port, username, password) "
										"values (randomblob(16), ?, ?, ?, ?) returning id";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	sqlite3_bind_text(stmt, 1, host->address, host->address_len, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, host->port);
	sqlite3_bind_text(stmt, 3, host->username, host->username_len, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 4, host->password, host->password_len, SQLITE_STATIC);

	int result = sqlite3_step(stmt);
	if (result == SQLITE_ROW) {
		const uint8_t *id = sqlite3_column_blob(stmt, 0);
		const size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
		if (id_len != sizeof(*host->id)) {
			error("id length %zu does not match buffer length %zu\n", id_len, sizeof(*host->id));
			status = 500;
			goto cleanup;
		}
		memcpy(host->id, id, id_len);
		status = 0;
	} else {
		error("failed to execute statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

cleanup:
	sqlite3_finalize(stmt);
	return status;
}

void host_find(sqlite3 *database, request_t *request, response_t *response) {
	host_query_t query;
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

	uint8_t hosts_len = 0;
	uint16_t status = host_select(database, &query, response, &hosts_len);
	if (status != 0) {
		response->status = status;
		return;
	}

	header_write(response, "content-type:application/octet-stream\r\n");
	header_write(response, "content-length:%u\r\n", response->body.len);
	info("found %hhu hosts\n", hosts_len);
	response->status = 200;
}
