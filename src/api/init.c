#include "../lib/logger.h"
#include "device.h"
#include "radio.h"
#include "user.h"
#include <sqlite3.h>
#include <stdlib.h>

int init_table(sqlite3 *database, const char *table, const char *schema) {
	int status;
	sqlite3_stmt *stmt;

	debug("%s\n", schema);

	if (sqlite3_prepare_v2(database, schema, -1, &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = -1;
		goto cleanup;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		error("failed to execute statement because %s\n", sqlite3_errmsg(database));
		status = -1;
		goto cleanup;
	}

	info("created table %s\n", table);
	status = 0;

cleanup:
	sqlite3_finalize(stmt);
	return status;
}

int init(sqlite3 *database) {
	if (init_table(database, user_table, user_schema) == -1) {
		return -1;
	}
	if (init_table(database, radio_table, radio_schema) == -1) {
		return -1;
	}
	if (init_table(database, device_table, device_schema) == -1) {
		return -1;
	}

	return 0;
}
