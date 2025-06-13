#include "radio.h"
#include "logger.h"
#include <sqlite3.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char *radio_table = "radio";
const char *radio_schema = "create table radio ("
													 "id blob primary key, "
													 "device text not null unique, "
													 "frequency integer not null, "
													 "tx_power integer not null, "
													 "conding_rate integer not null, "
													 "bandwidth integer not null, "
													 "spreading_factor integer not null, "
													 "checksum integer not null, "
													 "sync_word integer not null"
													 ")";

uint16_t radio_insert(sqlite3 *database, radio_t *radio) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char sql[] =
			"insert into radio (id, device, frequency, tx_power, conding_rate, bandwidth, spreading_factor, checksum, sync_word) "
			"values (randomblob(16), ?, ?, ?, ?, ?, ?, ?, ?) returning id";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, sizeof(sql), &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	sqlite3_bind_text(stmt, 1, radio->device, radio->device_len, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, (int)radio->frequency);
	sqlite3_bind_int(stmt, 3, radio->tx_power);
	sqlite3_bind_int(stmt, 4, radio->coding_rate);
	sqlite3_bind_int(stmt, 5, (int)radio->bandwidth);
	sqlite3_bind_int(stmt, 6, radio->spreading_factor);
	sqlite3_bind_int(stmt, 7, radio->checksum);
	sqlite3_bind_int(stmt, 8, radio->sync_word);

	int result = sqlite3_step(stmt);
	if (result == SQLITE_ROW) {
		const uint8_t *id = sqlite3_column_blob(stmt, 0);
		const size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
		if (id_len != sizeof(*radio->id)) {
			error("id length %zu does not match buffer length %zu\n", id_len, sizeof(*radio->id));
			status = 500;
			goto cleanup;
		}
		memcpy(radio->id, id, id_len);
		status = 0;
	} else if (result == SQLITE_CONSTRAINT) {
		warn("radio device %.*s already taken\n", (int)radio->device_len, radio->device);
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
