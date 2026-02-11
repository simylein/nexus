#include "user.h"
#include "../lib/bwt.h"
#include "../lib/config.h"
#include "../lib/format.h"
#include "../lib/logger.h"
#include "../lib/octet.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include "../lib/sha256.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char *user_file = "user";

const user_row_t user_row = {
		.id = 0,
		.username_len = 16,
		.username = 17,
		.password = 33,
		.signup_at = 65,
		.signin_at = 73,
		.size = 81,
};

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

uint16_t user_insert(octet_t *db, user_t *user) {
	uint16_t status;

	for (uint8_t index = 0; index < sizeof(*user->id); index++) {
		(*user->id)[index] = (uint8_t)(rand() & 0xff);
	}

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, user_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("insert user %.*s signup at %lu\n", user->username_len, user->username, *user->signup_at);

	off_t offset = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			break;
		}
		if (octet_row_read(&stmt, file, offset, db->row, user_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t username_len = octet_uint8_read(db->row, user_row.username_len);
		char *username = octet_text_read(db->row, user_row.username);
		if (username_len == user->username_len && memcmp(username, user->username, user->username_len) == 0) {
			status = 409;
			warn("username %.*s already taken\n", user->username_len, user->username);
			goto cleanup;
		}
		offset += user_row.size;
	}

	uint8_t hash[32];
	sha256(user->password, user->password_len, &hash);

	octet_blob_write(db->row, user_row.id, (uint8_t *)user->id, sizeof(*user->id));
	octet_uint8_write(db->row, user_row.username_len, user->username_len);
	octet_text_write(db->row, user_row.username, user->username, user->username_len);
	octet_blob_write(db->row, user_row.password, hash, sizeof(hash));
	octet_uint64_write(db->row, user_row.signup_at, (uint64_t)*user->signup_at);
	octet_uint64_write(db->row, user_row.signin_at, (uint64_t)*user->signin_at);

	offset = stmt.stat.st_size;
	if (octet_row_write(&stmt, file, offset, db->row, user_row.size) == -1) {
		status = octet_error();
		goto cleanup;
	}

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

uint16_t user_update(octet_t *db, user_t *user) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, user_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("update user %02x%02x signin at %lu\n", (*user->id)[0], (*user->id)[1], *user->signin_at);

	uint8_t hash[32];
	sha256(user->password, user->password_len, &hash);

	off_t offset = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			warn("invalid password for %.*s\n", user->username_len, user->username);
			status = 401;
			break;
		}
		if (octet_row_read(&stmt, file, offset, db->row, user_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t (*id)[8] = (uint8_t (*)[8])octet_blob_read(db->row, user_row.id);
		uint8_t username_len = octet_uint8_read(db->row, user_row.username_len);
		char *username = octet_text_read(db->row, user_row.username);
		uint8_t (*password)[32] = (uint8_t (*)[32])octet_blob_read(db->row, user_row.password);
		if (username_len == user->username_len && memcmp(username, user->username, user->username_len) == 0 &&
				memcmp(password, hash, sizeof(hash)) == 0) {
			octet_uint64_write(db->row, user_row.signin_at, (uint64_t)*user->signin_at);
			if (octet_row_write(&stmt, file, offset, db->row, user_row.size) == -1) {
				status = octet_error();
				goto cleanup;
			}
			memcpy(user->id, id, sizeof(*id));
			status = 0;
			break;
		}
		offset += user_row.size;
	}

cleanup:
	octet_close(&stmt, file);
	return status;
}

void user_signin(octet_t *db, request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	uint8_t id[8];
	user_t user = {.id = &id, .signin_at = (time_t[]){time(NULL)}};
	if (request->body.len == 0 || user_parse(&user, request) == -1 || user_validate(&user) == -1) {
		response->status = 400;
		return;
	}

	uint16_t status = user_update(db, &user);
	if (status != 0) {
		response->status = status;
		return;
	}

	char bwt[90];
	if (bwt_sign(&bwt, user.id) == -1) {
		response->status = 500;
		return;
	}

	header_write(response, "set-cookie:auth=%.*s;Path=/;Max-Age=%d;SameSite=Strict;HttpOnly;\r\n", (int)sizeof(bwt), bwt,
							 bwt_ttl);
	info("user %.*s signed in\n", (int)user.username_len, user.username);
	response->status = 201;
}
