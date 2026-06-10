#include "downlink.h"
#include "../lib/logger.h"
#include "../lib/octet.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const char *downlink_file = "downlink";

const downlink_row_t downlink_row = {
		.frame = 0,
		.kind = 2,
		.data_len = 3,
		.data = 4,
		.airtime = 260,
		.frequency = 262,
		.bandwidth = 266,
		.spreading_factor = 270,
		.coding_rate = 271,
		.tx_power = 272,
		.preamble_len = 273,
		.sent_at = 274,
		.device_id = 282,
		.size = 290,
};

uint16_t downlink_select_one(octet_t *db, downlink_t *downlink, uint8_t *head) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, downlink_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDONLY, F_RDLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("select downlink\n");

	off_t offset = downlink_row.size * (*head);
	if (octet_row_read(&stmt, file, offset, db->row, downlink_row.size) == -1) {
		status = octet_error();
		goto cleanup;
	}

	downlink->frame = octet_uint16_read(db->row, downlink_row.frame);
	downlink->kind = octet_uint8_read(db->row, downlink_row.kind);
	downlink->data_len = octet_uint8_read(db->row, downlink_row.data_len);
	memcpy(downlink->data, &db->row[downlink_row.data], downlink->data_len);
	downlink->airtime = octet_uint16_read(db->row, downlink_row.airtime);
	downlink->frequency = octet_uint32_read(db->row, downlink_row.frequency);
	downlink->bandwidth = octet_uint32_read(db->row, downlink_row.bandwidth);
	downlink->spreading_factor = octet_uint8_read(db->row, downlink_row.spreading_factor);
	downlink->coding_rate = octet_uint8_read(db->row, downlink_row.coding_rate);
	downlink->tx_power = octet_uint8_read(db->row, downlink_row.tx_power);
	downlink->preamble_len = octet_uint8_read(db->row, downlink_row.preamble_len);
	downlink->sent_at = (time_t)octet_uint64_read(db->row, downlink_row.sent_at);
	memcpy(downlink->device_id, &db->row[downlink_row.device_id], sizeof(downlink->device_id));

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

uint16_t downlink_insert(octet_t *db, downlink_t *downlink, uint8_t *tail) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, downlink_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("insert downlink for device %02x%02x\n", downlink->device_id[0], downlink->device_id[1]);

	off_t offset = downlink_row.size * (*tail);
	octet_uint16_write(db->row, downlink_row.frame, downlink->frame);
	octet_uint8_write(db->row, downlink_row.kind, downlink->kind);
	octet_uint8_write(db->row, downlink_row.data_len, downlink->data_len);
	octet_blob_write(db->row, downlink_row.data, downlink->data, downlink->data_len);
	octet_uint16_write(db->row, downlink_row.airtime, downlink->airtime);
	octet_uint32_write(db->row, downlink_row.frequency, downlink->frequency);
	octet_uint32_write(db->row, downlink_row.bandwidth, downlink->bandwidth);
	octet_uint8_write(db->row, downlink_row.spreading_factor, downlink->spreading_factor);
	octet_uint8_write(db->row, downlink_row.coding_rate, downlink->coding_rate);
	octet_uint8_write(db->row, downlink_row.tx_power, downlink->tx_power);
	octet_uint8_write(db->row, downlink_row.preamble_len, downlink->preamble_len);
	octet_uint64_write(db->row, downlink_row.sent_at, (uint64_t)downlink->sent_at);
	octet_blob_write(db->row, downlink_row.device_id, downlink->device_id, sizeof(downlink->device_id));

	if (octet_row_write(&stmt, file, offset, db->row, downlink_row.size) == -1) {
		status = octet_error();
		goto cleanup;
	}

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

uint16_t downlink_delete(octet_t *db, downlink_t *downlink, uint8_t *head) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, downlink_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("delete downlink sent at %lu\n", downlink->sent_at);

	off_t offset = downlink_row.size * (*head);
	if (octet_row_read(&stmt, file, offset, db->row, downlink_row.size) == -1) {
		status = octet_error();
		goto cleanup;
	}

	uint16_t frame = octet_uint16_read(db->row, downlink_row.frame);
	time_t sent_at = (time_t)octet_uint64_read(db->row, downlink_row.sent_at);
	uint8_t device_id[8];
	memcpy(device_id, &db->row[downlink_row.device_id], sizeof(device_id));
	if (frame != downlink->frame || sent_at != downlink->sent_at ||
			memcmp(device_id, downlink->device_id, sizeof(downlink->device_id)) != 0) {
		error("downlink frame %hu %hu received at %lu %lu device %02x%02x %02x%02x no match\n", frame, downlink->frame, sent_at,
					downlink->sent_at, device_id[0], device_id[1], downlink->device_id[0], downlink->device_id[1]);
		status = 500;
		goto cleanup;
	}

	memset(db->row, 0x00, downlink_row.size);
	if (octet_row_write(&stmt, file, offset, db->row, downlink_row.size) == -1) {
		status = octet_error();
		goto cleanup;
	}

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}
