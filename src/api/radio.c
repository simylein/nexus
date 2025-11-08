#include "radio.h"
#include "../lib/base16.h"
#include "../lib/endian.h"
#include "../lib/logger.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

const char *radio_table = "radio";
const char *radio_schema = "create table radio ("
													 "id blob primary key, "
													 "device text not null unique, "
													 "frequency integer not null, "
													 "bandwidth integer not null, "
													 "spreading_factor integer not null, "
													 "coding_rate integer not null, "
													 "tx_power integer not null, "
													 "sync_word integer not null, "
													 "checksum boolean not null"
													 ")";

uint16_t radio_select(sqlite3 *database, radio_query_t *query, response_t *response, uint8_t *radios_len) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char *sql = "select "
										"radio.id, radio.device, radio.frequency, radio.bandwidth, "
										"radio.spreading_factor, radio.coding_rate, radio.tx_power, radio.sync_word, radio.checksum "
										"from radio "
										"order by "
										"case when ?1 = 'id' and ?2 = 'asc' then radio.id end asc, "
										"case when ?1 = 'id' and ?2 = 'desc' then radio.id end desc, "
										"case when ?1 = 'device' and ?2 = 'asc' then radio.device end asc, "
										"case when ?1 = 'device' and ?2 = 'desc' then radio.device end desc, "
										"case when ?1 = 'frequency' and ?2 = 'asc' then radio.frequency end asc, "
										"case when ?1 = 'frequency' and ?2 = 'desc' then radio.frequency end desc, "
										"case when ?1 = 'bandwidth' and ?2 = 'asc' then radio.bandwidth end asc, "
										"case when ?1 = 'bandwidth' and ?2 = 'desc' then radio.bandwidth end desc, "
										"case when ?1 = 'spreadingFactor' and ?2 = 'asc' then radio.spreading_factor end asc, "
										"case when ?1 = 'spreadingFactor' and ?2 = 'desc' then radio.spreading_factor end desc, "
										"case when ?1 = 'codingRate' and ?2 = 'asc' then radio.coding_rate end asc, "
										"case when ?1 = 'codingRate' and ?2 = 'desc' then radio.coding_rate end desc, "
										"case when ?1 = 'txPower' and ?2 = 'asc' then radio.tx_power end asc, "
										"case when ?1 = 'txPower' and ?2 = 'desc' then radio.tx_power end desc, "
										"case when ?1 = 'syncWord' and ?2 = 'asc' then radio.sync_word end asc, "
										"case when ?1 = 'syncWord' and ?2 = 'desc' then radio.sync_word end desc, "
										"case when ?1 = 'checksum' and ?2 = 'asc' then radio.checksum end asc, "
										"case when ?1 = 'checksum' and ?2 = 'desc' then radio.checksum end desc";
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
			if (id_len != sizeof(*((radio_t *)0)->id)) {
				error("id length %zu does not match buffer length %zu\n", id_len, sizeof(*((radio_t *)0)->id));
				status = 500;
				goto cleanup;
			}
			const uint8_t *device = sqlite3_column_text(stmt, 1);
			const size_t device_len = (size_t)sqlite3_column_bytes(stmt, 1);
			const uint32_t frequency = (uint32_t)sqlite3_column_int(stmt, 2);
			const uint32_t bandwidth = (uint32_t)sqlite3_column_int(stmt, 3);
			const uint8_t spreading_factor = (uint8_t)sqlite3_column_int(stmt, 4);
			const uint8_t coding_rate = (uint8_t)sqlite3_column_int(stmt, 5);
			const uint8_t tx_power = (uint8_t)sqlite3_column_int(stmt, 6);
			const uint8_t sync_word = (uint8_t)sqlite3_column_int(stmt, 7);
			const bool checksum = (bool)sqlite3_column_int(stmt, 8);
			body_write(response, id, id_len);
			body_write(response, device, device_len);
			body_write(response, (char[]){0x00}, sizeof(char));
			body_write(response, (uint32_t[]){hton32(frequency)}, sizeof(frequency));
			body_write(response, (uint32_t[]){hton32(bandwidth)}, sizeof(bandwidth));
			body_write(response, &spreading_factor, sizeof(spreading_factor));
			body_write(response, &coding_rate, sizeof(coding_rate));
			body_write(response, &tx_power, sizeof(tx_power));
			body_write(response, &sync_word, sizeof(sync_word));
			body_write(response, &checksum, sizeof(checksum));
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

int radio_parse(radio_t *radio, request_t *request) {
	request->body.pos = 0;

	uint8_t stage = 0;

	radio->device_len = 0;
	const uint8_t device_index = (uint8_t)request->body.pos;
	while (stage == 0 && radio->device_len < 32 && request->body.pos < request->body.len) {
		const char *byte = body_read(request, sizeof(char));
		if (*byte == '\0') {
			stage = 1;
		} else {
			radio->device_len++;
		}
	}
	radio->device = &request->body.ptr[device_index];
	if (stage != 1) {
		debug("found device with %hhu bytes\n", radio->device_len);
		return -1;
	}

	if (request->body.len < request->body.pos + sizeof(radio->frequency)) {
		debug("missing frequency on radio\n");
		return -1;
	}
	memcpy(&radio->frequency, body_read(request, sizeof(radio->frequency)), sizeof(radio->frequency));
	radio->frequency = ntoh32(radio->frequency);

	if (request->body.len < request->body.pos + sizeof(radio->bandwidth)) {
		debug("missing bandwidth on radio\n");
		return -1;
	}
	memcpy(&radio->bandwidth, body_read(request, sizeof(radio->bandwidth)), sizeof(radio->bandwidth));
	radio->bandwidth = ntoh32(radio->bandwidth);

	if (request->body.len < request->body.pos + sizeof(radio->spreading_factor)) {
		debug("missing spreading factor on radio\n");
		return -1;
	}
	radio->spreading_factor = *(uint8_t *)body_read(request, sizeof(radio->spreading_factor));

	if (request->body.len < request->body.pos + sizeof(radio->coding_rate)) {
		debug("missing coding rate on radio\n");
		return -1;
	}
	radio->coding_rate = *(uint8_t *)body_read(request, sizeof(radio->coding_rate));

	if (request->body.len < request->body.pos + sizeof(radio->tx_power)) {
		debug("missing tx power on radio\n");
		return -1;
	}
	radio->tx_power = *(uint8_t *)body_read(request, sizeof(radio->tx_power));

	if (request->body.len < request->body.pos + sizeof(radio->sync_word)) {
		debug("missing sync word on radio\n");
		return -1;
	}
	radio->sync_word = *(uint8_t *)body_read(request, sizeof(radio->sync_word));

	if (request->body.len != request->body.pos) {
		debug("body len %u does not match body pos %u\n", request->body.len, request->body.pos);
		return -1;
	}

	return 0;
}

int radio_validate(radio_t *radio) {
	if (radio->frequency < 400 * 1000 * 1000 || radio->frequency > 500 * 1000 * 1000) {
		debug("invalid frequency %u on radio\n", radio->frequency);
		return -1;
	}

	if (radio->bandwidth < 7800 || radio->bandwidth > 500 * 1000) {
		debug("invalid bandwidth %u on radio\n", radio->bandwidth);
		return -1;
	}

	if (radio->spreading_factor < 7 || radio->spreading_factor > 12) {
		debug("invalid spreading factor %hhu on radio\n", radio->spreading_factor);
		return -1;
	}

	if (radio->coding_rate < 5 || radio->coding_rate > 8) {
		debug("invalid coding rate %hhu on radio\n", radio->coding_rate);
		return -1;
	}

	if (radio->tx_power < 2 || radio->tx_power > 17) {
		debug("invalid tx power %hhu on radio\n", radio->tx_power);
		return -1;
	}

	return 0;
}

uint16_t radio_insert(sqlite3 *database, radio_t *radio) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char *sql = "insert into radio (id, device, frequency, bandwidth, "
										"spreading_factor, coding_rate, tx_power, sync_word, checksum) "
										"values (randomblob(16), ?, ?, ?, ?, ?, ?, ?, ?) returning id";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	sqlite3_bind_text(stmt, 1, radio->device, radio->device_len, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, (int)radio->frequency);
	sqlite3_bind_int(stmt, 3, (int)radio->bandwidth);
	sqlite3_bind_int(stmt, 4, radio->spreading_factor);
	sqlite3_bind_int(stmt, 5, radio->coding_rate);
	sqlite3_bind_int(stmt, 6, radio->tx_power);
	sqlite3_bind_int(stmt, 7, radio->sync_word);
	sqlite3_bind_int(stmt, 8, radio->checksum);

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

uint16_t radio_delete(sqlite3 *database, radio_t *radio) {
	uint16_t status;
	sqlite3_stmt *stmt;

	const char *sql = "delete from radio "
										"where id = ?";
	debug("%s\n", sql);

	if (sqlite3_prepare_v2(database, sql, -1, &stmt, NULL) != SQLITE_OK) {
		error("failed to prepare statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	sqlite3_bind_blob(stmt, 1, radio->id, sizeof(*radio->id), SQLITE_STATIC);

	int result = sqlite3_step(stmt);
	if (result != SQLITE_DONE) {
		error("failed to execute statement because %s\n", sqlite3_errmsg(database));
		status = 500;
		goto cleanup;
	}

	if (sqlite3_changes(database) == 0) {
		warn("radio %02x%02x not found\n", (*radio->id)[0], (*radio->id)[1]);
		status = 404;
		goto cleanup;
	}

	status = 0;

cleanup:
	sqlite3_finalize(stmt);
	return status;
}

void radio_find(sqlite3 *database, request_t *request, response_t *response) {
	radio_query_t query;
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

	uint8_t radios_len = 0;
	uint16_t status = radio_select(database, &query, response, &radios_len);
	if (status != 0) {
		response->status = status;
		return;
	}

	header_write(response, "content-type:application/octet-stream\r\n");
	header_write(response, "content-length:%u\r\n", response->body.len);
	info("found %hhu radios\n", radios_len);
	response->status = 200;
}

void radio_create(sqlite3 *database, request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	uint8_t id[16];
	radio_t radio = {.id = &id};
	if (request->body.len == 0 || radio_parse(&radio, request) == -1 || radio_validate(&radio) == -1) {
		response->status = 400;
		return;
	}

	uint16_t status = radio_insert(database, &radio);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("created radio %02x%02x\n", (*radio.id)[0], (*radio.id)[1]);
	response->status = 201;
}

void radio_remove(sqlite3 *database, request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	uint8_t uuid_len = 0;
	const char *uuid = param_find(request, 11, &uuid_len);
	if (uuid_len != sizeof(*((radio_t *)0)->id) * 2) {
		warn("uuid length %hhu does not match %zu\n", uuid_len, sizeof(*((radio_t *)0)->id) * 2);
		response->status = 400;
		return;
	}

	uint8_t id[16];
	if (base16_decode(id, sizeof(id), uuid, uuid_len) != 0) {
		warn("failed to decode uuid from base 16\n");
		response->status = 400;
		return;
	}

	radio_t radio = {.id = &id};
	uint16_t status = radio_delete(database, &radio);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("deleted radio %02x%02x\n", (*radio.id)[0], (*radio.id)[1]);
	response->status = 200;
}
