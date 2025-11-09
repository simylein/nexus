
#include "../lib/logger.h"
#include "device.h"
#include "host.h"
#include "radio.h"
#include "user.h"
#include <sqlite3.h>
#include <stdio.h>

int drop_table(sqlite3 *database, const char *table) {
	int status;
	sqlite3_stmt *stmt;

	char sql[64];
	sprintf(sql, "drop table %s", table);
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = -1;
		goto cleanup;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		error("failed to execute statement because %s\n", sqlite3_errmsg(database));
		status = -1;
		goto cleanup;
	}

	info("dropped table %s\n", table);
	status = 0;

cleanup:
	sqlite3_finalize(stmt);
	return status;
}

int drop(sqlite3 *database) {
	if (drop_table(database, user_table) == -1) {
		return -1;
	}
	if (drop_table(database, radio_table) == -1) {
		return -1;
	}
	if (drop_table(database, device_table) == -1) {
		return -1;
	}
	if (drop_table(database, host_table) == -1) {
		return -1;
	}

	return 0;
}
