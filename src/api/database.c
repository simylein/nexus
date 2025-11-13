#include "../lib/logger.h"
#include <sqlite3.h>
#include <stdint.h>

uint16_t database_error(sqlite3 *database, int result) {
	uint16_t status;

	if (result == SQLITE_BUSY || result == SQLITE_LOCKED || result == SQLITE_NOMEM) {
		status = 503;
	} else if (result == SQLITE_FULL) {
		status = 507;
	} else {
		status = 500;
	}

	error("failed to execute statement because %s\n", sqlite3_errmsg(database));
	return status;
}
