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
													 "coding_rate integer not null, "
													 "bandwidth integer not null, "
													 "spreading_factor integer not null, "
													 "checksum integer not null, "
													 "sync_word integer not null"
													 ")";

uint16_t radio_select(sqlite3 *database, radio_t (*radios)[16], uint8_t *radios_len) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char sql[] =
			"select id, device, frequency, tx_power, coding_rate, bandwidth, spreading_factor, checksum, sync_word from radio";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, sizeof(sql), &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	while (true) {
		int result = sqlite3_step(stmt);
		if (result == SQLITE_ROW) {
			if (*radios_len >= sizeof(*radios) / sizeof(radio_t)) {
				error("radio length %hu exceeds buffer length %zu\n", *radios_len, sizeof(*radios) / sizeof(radio_t));
				status = 500;
				goto cleanup;
			}
			const uint8_t *id = sqlite3_column_blob(stmt, 0);
			const size_t id_len = (size_t)sqlite3_column_bytes(stmt, 0);
			if (id_len != sizeof(((radio_t *)0)->id)) {
				error("id length %zu does not match buffer length %zu\n", id_len, sizeof(((radio_t *)0)->id));
				status = 500;
				goto cleanup;
			}
			const uint8_t *device = sqlite3_column_text(stmt, 1);
			const size_t device_len = (size_t)sqlite3_column_bytes(stmt, 1);
			if (device_len > sizeof(((radio_t *)0)->device)) {
				error("device length %zu exceeds buffer length %zu\n", device_len, sizeof(((radio_t *)0)->device));
				status = 500;
				goto cleanup;
			}
			const uint32_t frequency = (uint32_t)sqlite3_column_int(stmt, 2);
			const uint8_t tx_power = (uint8_t)sqlite3_column_int(stmt, 3);
			const uint8_t coding_rate = (uint8_t)sqlite3_column_int(stmt, 4);
			const uint32_t bandwidth = (uint32_t)sqlite3_column_int(stmt, 5);
			const uint8_t spreading_factor = (uint8_t)sqlite3_column_int(stmt, 6);
			const bool checksum = (bool)sqlite3_column_int(stmt, 7);
			const uint8_t sync_word = (uint8_t)sqlite3_column_int(stmt, 8);
			memcpy(radios[*radios_len]->id, id, id_len);
			memcpy(radios[*radios_len]->device, device, device_len);
			radios[*radios_len]->device_len = (uint8_t)device_len;
			radios[*radios_len]->frequency = frequency;
			radios[*radios_len]->tx_power = tx_power;
			radios[*radios_len]->coding_rate = coding_rate;
			radios[*radios_len]->bandwidth = bandwidth;
			radios[*radios_len]->spreading_factor = spreading_factor;
			radios[*radios_len]->checksum = checksum;
			radios[*radios_len]->sync_word = sync_word;
			*radios_len += 1;
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

uint16_t radio_insert(sqlite3 *database, radio_t *radio) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char sql[] =
			"insert into radio (id, device, frequency, tx_power, coding_rate, bandwidth, spreading_factor, checksum, sync_word) "
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
		if (id_len != sizeof(radio->id)) {
			error("id length %zu does not match buffer length %zu\n", id_len, sizeof(radio->id));
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
