#include "user.h"
#include "../lib/bwt.h"
#include "../lib/config.h"
#include "../lib/format.h"
#include "../lib/logger.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include "../lib/sha256.h"
#include <sqlite3.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char *user_table = "user";
const char *user_schema = "create table user ("
													"id blob primary key, "
													"username text not null unique, "
													"password blob not null, "
													"signup_at datetime not null, "
													"signin_at datetime not null, "
													"permissions blob not null"
													")";

int user_parse(user_t *user, request_t *request) {
	request->body.pos = 0;

	uint8_t stage = 0;

	user->username_len = 0;
	const uint8_t username_index = (uint8_t)request->body.pos;
	while (stage == 0 && user->username_len < 16 && request->body.pos < request->body.len) {
		const char *byte = body_read(request, sizeof(char));
		if (*byte == '\0') {
			stage = 1;
		} else {
			user->username_len++;
		}
	}
	user->username = &request->body.ptr[username_index];
	if (stage != 1) {
		debug("found username with %hhu bytes\n", user->username_len);
		return -1;
	}

	user->password_len = 0;
	const uint8_t password_index = (uint8_t)request->body.pos;
	while (stage == 1 && user->password_len < 64 && request->body.pos < request->body.len) {
		const char *byte = body_read(request, sizeof(char));
		if (*byte == '\0') {
			stage = 2;
		} else {
			user->password_len++;
		}
	}
	user->password = &request->body.ptr[password_index];
	if (stage != 2) {
		debug("found password with %hhu bytes\n", user->password_len);
		return -1;
	}

	trace("username %hhu bytes and password %hhu bytes\n", user->username_len, user->password_len);
	return 0;
}

int user_validate(user_t *user) {
	if (user->username_len < 4) {
		return -1;
	}

	uint8_t username_index = 0;
	while (username_index < user->username_len) {
		char *byte = &user->username[username_index];
		if (*byte < 'a' || *byte > 'z') {
			debug("username contains invalid character %02x\n", *byte);
			return -1;
		}
		username_index++;
	}

	if (user->password_len < 4) {
		return -1;
	}

	bool lower = false;
	bool upper = false;
	bool digit = false;

	uint8_t password_index = 0;
	while (password_index < user->password_len) {
		char *byte = &user->password[password_index];
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

uint16_t user_insert(sqlite3 *database, user_t *user) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char *sql = "insert into user (id, username, password, signup_at, signin_at, permissions) "
										"values (randomblob(16), ?, ?, ?, ?, ?) returning id, permissions";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	uint8_t hash[32];
	sha256(user->password, user->password_len, &hash);
	time_t now = time(NULL);
	memset(user->permissions, 0x00, sizeof(*user->permissions));

	sqlite3_bind_text(stmt, 1, user->username, user->username_len, SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 2, hash, sizeof(hash), SQLITE_STATIC);
	sqlite3_bind_int64(stmt, 3, now);
	sqlite3_bind_int64(stmt, 4, now);
	sqlite3_bind_blob(stmt, 5, user->permissions, sizeof(*user->permissions), SQLITE_STATIC);

	int result = sqlite3_step(stmt);
	if (result == SQLITE_ROW) {
		const uint8_t *id = sqlite3_column_blob(stmt, 0);
		const size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
		if (id_len != sizeof(*user->id)) {
			error("id length %zu does not match buffer length %zu\n", id_len, sizeof(*user->id));
			status = 500;
			goto cleanup;
		}
		const uint8_t *permissions = sqlite3_column_blob(stmt, 1);
		const size_t permissions_len = (size_t)sqlite3_column_bytes(stmt, 1);
		if (permissions_len != sizeof(*user->permissions)) {
			error("permissions length %zu does not match buffer length %zu\n", permissions_len, sizeof(*user->permissions));
			status = 500;
			goto cleanup;
		}
		memcpy(user->id, id, id_len);
		memcpy(user->permissions, permissions, permissions_len);
		status = 0;
	} else if (result == SQLITE_CONSTRAINT) {
		warn("username %.*s already taken\n", (int)user->username_len, user->username);
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

uint16_t user_update(sqlite3 *database, user_t *user) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char *sql = "update user set signin_at = ? "
										"where username = ? and password = ? returning id, permissions";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	uint8_t hash[32];
	sha256(user->password, user->password_len, &hash);
	time_t now = time(NULL);

	sqlite3_bind_int64(stmt, 1, now);
	sqlite3_bind_text(stmt, 2, user->username, user->username_len, SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 3, hash, sizeof(hash), SQLITE_STATIC);

	int result = sqlite3_step(stmt);
	if (result == SQLITE_ROW) {
		const uint8_t *id = sqlite3_column_blob(stmt, 0);
		const size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
		if (id_len != sizeof(*user->id)) {
			error("id length %zu does not match buffer length %zu\n", id_len, sizeof(*user->id));
			status = 500;
			goto cleanup;
		}
		const uint8_t *permissions = sqlite3_column_blob(stmt, 1);
		const size_t permissions_len = (size_t)sqlite3_column_bytes(stmt, 1);
		if (permissions_len != sizeof(*user->permissions)) {
			error("permissions length %zu does not match buffer length %zu\n", permissions_len, sizeof(*user->permissions));
			status = 500;
			goto cleanup;
		}
		memcpy(user->id, id, id_len);
		memcpy(user->permissions, permissions, permissions_len);
		status = 0;
	} else if (result == SQLITE_DONE) {
		warn("invalid password for %.*s\n", (int)user->username_len, user->username);
		status = 401;
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

void user_signin(sqlite3 *database, request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	uint8_t id[16];
	uint8_t permissions[4];
	user_t user = {.id = &id, .permissions = &permissions};
	if (request->body.len == 0 || user_parse(&user, request) == -1 || user_validate(&user) == -1) {
		response->status = 400;
		return;
	}

	uint16_t status = user_update(database, &user);
	if (status != 0) {
		response->status = status;
		return;
	}

	char bwt[109];
	if (bwt_sign(&bwt, user.id, user.permissions) == -1) {
		response->status = 500;
		return;
	}

	header_write(response, "set-cookie:auth=%.*s;Path=/;Max-Age=%d;SameSite=Strict;HttpOnly;\r\n", (int)sizeof(bwt), bwt,
							 bwt_ttl);
	info("user %.*s signed in\n", (int)user.username_len, user.username);
	response->status = 201;
}
