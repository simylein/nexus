#include "uplink.h"
#include "../lib/logger.h"
#include "../lib/octet.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const char *uplink_file = "uplink";

const uplink_row_t uplink_row = {
		.frame = 0,
		.kind = 2,
		.data_len = 3,
		.data = 4,
		.airtime = 260,
		.frequency = 262,
		.bandwidth = 266,
		.rssi = 270,
		.snr = 272,
		.spreading_factor = 273,
		.coding_rate = 274,
		.checksum = 275,
		.tx_power = 276,
		.preamble_len = 277,
		.received_at = 278,
		.device_id = 286,
		.size = 294,
};

uint16_t uplink_select_one(octet_t *db, uplink_t *uplink, uint8_t *head) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, uplink_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDONLY, F_RDLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("select uplink\n");

	off_t offset = uplink_row.size * (*head);
	if (octet_row_read(&stmt, file, offset, db->row, uplink_row.size) == -1) {
		status = octet_error();
		goto cleanup;
	}

	uplink->frame = octet_uint16_read(db->row, uplink_row.frame);
	uplink->kind = octet_uint8_read(db->row, uplink_row.kind);
	uplink->data_len = octet_uint8_read(db->row, uplink_row.data_len);
	memcpy(uplink->data, &db->row[uplink_row.data], uplink->data_len);
	uplink->airtime = octet_uint16_read(db->row, uplink_row.airtime);
	uplink->frequency = octet_uint32_read(db->row, uplink_row.frequency);
	uplink->bandwidth = octet_uint32_read(db->row, uplink_row.bandwidth);
	uplink->rssi = octet_int16_read(db->row, uplink_row.rssi);
	uplink->snr = octet_int8_read(db->row, uplink_row.snr);
	uplink->spreading_factor = octet_uint8_read(db->row, uplink_row.spreading_factor);
	uplink->coding_rate = octet_uint8_read(db->row, uplink_row.coding_rate);
	uplink->checksum = octet_bool_read(db->row, uplink_row.checksum);
	uplink->tx_power = octet_uint8_read(db->row, uplink_row.tx_power);
	uplink->preamble_len = octet_uint8_read(db->row, uplink_row.preamble_len);
	uplink->received_at = (time_t)octet_uint64_read(db->row, uplink_row.received_at);
	memcpy(uplink->device_id, &db->row[uplink_row.device_id], sizeof(uplink->device_id));

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

uint16_t uplink_insert(octet_t *db, uplink_t *uplink, uint8_t *tail) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, uplink_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("insert uplink for device %02x%02x\n", uplink->device_id[0], uplink->device_id[1]);

	off_t offset = uplink_row.size * (*tail);
	octet_uint16_write(db->row, uplink_row.frame, uplink->frame);
	octet_uint8_write(db->row, uplink_row.kind, uplink->kind);
	octet_uint8_write(db->row, uplink_row.data_len, uplink->data_len);
	octet_blob_write(db->row, uplink_row.data, uplink->data, uplink->data_len);
	octet_uint16_write(db->row, uplink_row.airtime, uplink->airtime);
	octet_uint32_write(db->row, uplink_row.frequency, uplink->frequency);
	octet_uint32_write(db->row, uplink_row.bandwidth, uplink->bandwidth);
	octet_int16_write(db->row, uplink_row.rssi, uplink->rssi);
	octet_int8_write(db->row, uplink_row.snr, uplink->snr);
	octet_uint8_write(db->row, uplink_row.spreading_factor, uplink->spreading_factor);
	octet_uint8_write(db->row, uplink_row.coding_rate, uplink->coding_rate);
	octet_bool_write(db->row, uplink_row.checksum, uplink->checksum);
	octet_uint8_write(db->row, uplink_row.tx_power, uplink->tx_power);
	octet_uint8_write(db->row, uplink_row.preamble_len, uplink->preamble_len);
	octet_uint64_write(db->row, uplink_row.received_at, (uint64_t)uplink->received_at);
	octet_blob_write(db->row, uplink_row.device_id, uplink->device_id, sizeof(uplink->device_id));

	if (octet_row_write(&stmt, file, offset, db->row, uplink_row.size) == -1) {
		status = octet_error();
		goto cleanup;
	}

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

uint16_t uplink_delete(octet_t *db, uplink_t *uplink, uint8_t *head) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, uplink_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("delete uplink received at %lu\n", uplink->received_at);

	off_t offset = uplink_row.size * (*head);
	if (octet_row_read(&stmt, file, offset, db->row, uplink_row.size) == -1) {
		status = octet_error();
		goto cleanup;
	}

	uint16_t frame = octet_uint16_read(db->row, uplink_row.frame);
	time_t received_at = (time_t)octet_uint64_read(db->row, uplink_row.received_at);
	uint8_t device_id[8];
	memcpy(device_id, &db->row[uplink_row.device_id], sizeof(device_id));
	if (frame != uplink->frame || received_at != uplink->received_at ||
			memcmp(device_id, uplink->device_id, sizeof(uplink->device_id)) != 0) {
		error("uplink frame %hu %hu received at %lu %lu device %02x%02x %02x%02x no match\n", frame, uplink->frame, received_at,
					uplink->received_at, device_id[0], device_id[1], uplink->device_id[0], uplink->device_id[1]);
		status = 500;
		goto cleanup;
	}

	memset(db->row, 0x00, uplink_row.size);
	if (octet_row_write(&stmt, file, offset, db->row, uplink_row.size) == -1) {
		status = octet_error();
		goto cleanup;
	}

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}
