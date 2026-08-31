#include "radio.h"
#include "../lib/base16.h"
#include "../lib/endian.h"
#include "../lib/logger.h"
#include "../lib/octet.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *radio_file = "radio";

const radio_row_t radio_row = {
		.id = 0,
		.spi_device_len = 8,
		.spi_device = 9,
		.gpio_device_len = 41,
		.gpio_device = 42,
		.gpio_int_pin = 74,
		.frequency = 75,
		.bandwidth = 79,
		.spreading_factor = 83,
		.coding_rate = 84,
		.tx_power = 85,
		.preamble_len = 86,
		.sync_word = 87,
		.checksum = 88,
		.size = 89,
};

int radio_rowcmp(uint8_t *alpha, uint8_t *bravo, radio_query_t *query) {
	if (query->order_len == 2 && memcmp(query->order, "id", query->order_len) == 0) {
		uint64_t id_alpha = octet_uint64_read(alpha, radio_row.id);
		uint64_t id_bravo = octet_uint64_read(bravo, radio_row.id);
		int result = (id_alpha > id_bravo) - (id_alpha < id_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 9 && memcmp(query->order, "spiDevice", query->order_len) == 0) {
		uint8_t spi_device_len_alpha = octet_uint8_read(alpha, radio_row.spi_device_len);
		char *spi_device_alpha = octet_text_read(alpha, radio_row.spi_device);
		uint8_t spi_device_len_bravo = octet_uint8_read(bravo, radio_row.spi_device_len);
		char *spi_device_bravo = octet_text_read(bravo, radio_row.spi_device);
		int result = memcmp(spi_device_alpha, spi_device_bravo,
												spi_device_len_alpha < spi_device_len_bravo ? spi_device_len_alpha : spi_device_len_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 10 && memcmp(query->order, "gpioDevice", query->order_len) == 0) {
		uint8_t gpio_device_len_alpha = octet_uint8_read(alpha, radio_row.gpio_device_len);
		char *gpio_device_alpha = octet_text_read(alpha, radio_row.gpio_device);
		uint8_t gpio_device_len_bravo = octet_uint8_read(bravo, radio_row.gpio_device_len);
		char *gpio_device_bravo = octet_text_read(bravo, radio_row.gpio_device);
		int result = memcmp(gpio_device_alpha, gpio_device_bravo,
												gpio_device_len_alpha < gpio_device_len_bravo ? gpio_device_len_alpha : gpio_device_len_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 9 && memcmp(query->order, "frequency", query->order_len) == 0) {
		uint32_t frequency_alpha = octet_uint32_read(alpha, radio_row.frequency);
		uint32_t frequency_bravo = octet_uint32_read(bravo, radio_row.frequency);
		int result = (frequency_alpha > frequency_bravo) - (frequency_alpha < frequency_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 9 && memcmp(query->order, "bandwidth", query->order_len) == 0) {
		uint32_t bandwidth_alpha = octet_uint32_read(alpha, radio_row.bandwidth);
		uint32_t bandwidth_bravo = octet_uint32_read(bravo, radio_row.bandwidth);
		int result = (bandwidth_alpha > bandwidth_bravo) - (bandwidth_alpha < bandwidth_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 15 && memcmp(query->order, "spreadingFactor", query->order_len) == 0) {
		uint8_t spreading_factor_alpha = octet_uint8_read(alpha, radio_row.spreading_factor);
		uint8_t spreading_factor_bravo = octet_uint8_read(bravo, radio_row.spreading_factor);
		int result = (spreading_factor_alpha > spreading_factor_bravo) - (spreading_factor_alpha < spreading_factor_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 10 && memcmp(query->order, "codingRate", query->order_len) == 0) {
		uint8_t coding_rate_alpha = octet_uint8_read(alpha, radio_row.coding_rate);
		uint8_t coding_rate_bravo = octet_uint8_read(bravo, radio_row.coding_rate);
		int result = (coding_rate_alpha > coding_rate_bravo) - (coding_rate_alpha < coding_rate_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 7 && memcmp(query->order, "txPower", query->order_len) == 0) {
		uint8_t tx_power_alpha = octet_uint8_read(alpha, radio_row.tx_power);
		uint8_t tx_power_bravo = octet_uint8_read(bravo, radio_row.tx_power);
		int result = (tx_power_alpha > tx_power_bravo) - (tx_power_alpha < tx_power_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 11 && memcmp(query->order, "preambleLen", query->order_len) == 0) {
		uint8_t preamble_len_alpha = octet_uint8_read(alpha, radio_row.preamble_len);
		uint8_t preamble_len_bravo = octet_uint8_read(bravo, radio_row.preamble_len);
		int result = (preamble_len_alpha > preamble_len_bravo) - (preamble_len_alpha < preamble_len_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 8 && memcmp(query->order, "syncWord", query->order_len) == 0) {
		uint8_t sync_word_alpha = octet_uint8_read(alpha, radio_row.sync_word);
		uint8_t sync_word_bravo = octet_uint8_read(bravo, radio_row.sync_word);
		int result = (sync_word_alpha > sync_word_bravo) - (sync_word_alpha < sync_word_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	if (query->order_len == 8 && memcmp(query->order, "checksum", query->order_len) == 0) {
		uint8_t checksum_alpha = octet_uint8_read(alpha, radio_row.checksum);
		uint8_t checksum_bravo = octet_uint8_read(bravo, radio_row.checksum);
		int result = (checksum_alpha > checksum_bravo) - (checksum_alpha < checksum_bravo);
		if (query->sort_len == 4 && memcmp(query->sort, "desc", query->sort_len) == 0) {
			result = -result;
		}
		return result;
	}

	return 0;
}

uint16_t radio_select(octet_t *db, radio_query_t *query, response_t *response, uint8_t *radios_len) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, radio_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDONLY, F_RDLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	if (stmt.stat.st_size > db->table_len) {
		error("file length %zu exceeds buffer length %u\n", (size_t)stmt.stat.st_size, db->table_len);
		status = 500;
		goto cleanup;
	}

	debug("select radios order by %.*s:%.*s\n", (int)query->order_len, query->order, (int)query->sort_len, query->sort);

	off_t offset = 0;
	uint32_t table_len = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			status = 0;
			break;
		}
		if (octet_row_read(&stmt, file, offset, &db->table[table_len], radio_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		table_len += radio_row.size;
		offset += radio_row.size;
	}

	if (table_len >= radio_row.size * 2) {
		for (uint8_t index = 0; index < table_len / radio_row.size - 1; index++) {
			for (uint8_t ind = index + 1; ind < table_len / radio_row.size; ind++) {
				if (radio_rowcmp(&db->table[index * radio_row.size], &db->table[ind * radio_row.size], query) > 0) {
					memcpy(db->row, &db->table[index * radio_row.size], radio_row.size);
					memcpy(&db->table[index * radio_row.size], &db->table[ind * radio_row.size], radio_row.size);
					memcpy(&db->table[ind * radio_row.size], db->row, radio_row.size);
				}
			}
		}
	}

	uint32_t index = radio_row.size * query->offset;
	while (true) {
		if (index >= table_len || *radios_len >= query->limit) {
			status = 0;
			break;
		}
		uint8_t (*id)[8] = (uint8_t (*)[8])octet_blob_read(&db->table[index], radio_row.id);
		uint8_t spi_device_len = octet_uint8_read(&db->table[index], radio_row.spi_device_len);
		char *spi_device = octet_text_read(&db->table[index], radio_row.spi_device);
		uint8_t gpio_device_len = octet_uint8_read(&db->table[index], radio_row.gpio_device_len);
		char *gpio_device = octet_text_read(&db->table[index], radio_row.gpio_device);
		uint8_t gpio_int_pin = octet_uint8_read(&db->table[index], radio_row.gpio_int_pin);
		uint32_t frequency = octet_uint32_read(&db->table[index], radio_row.frequency);
		uint32_t bandwidth = octet_uint32_read(&db->table[index], radio_row.bandwidth);
		uint8_t spreading_factor = octet_uint8_read(&db->table[index], radio_row.spreading_factor);
		uint8_t coding_rate = octet_uint8_read(&db->table[index], radio_row.coding_rate);
		uint8_t tx_power = octet_uint8_read(&db->table[index], radio_row.tx_power);
		uint8_t preamble_len = octet_uint8_read(&db->table[index], radio_row.preamble_len);
		uint8_t sync_word = octet_uint8_read(&db->table[index], radio_row.sync_word);
		uint8_t checksum = octet_uint8_read(&db->table[index], radio_row.checksum);
		body_write(response, id, sizeof(*id));
		body_write(response, spi_device, spi_device_len);
		body_write(response, (char[]){0x00}, sizeof(char));
		body_write(response, gpio_device, gpio_device_len);
		body_write(response, (char[]){0x00}, sizeof(char));
		body_write(response, &gpio_int_pin, sizeof(gpio_int_pin));
		body_write(response, (uint32_t[]){hton32(frequency)}, sizeof(frequency));
		body_write(response, (uint32_t[]){hton32(bandwidth)}, sizeof(bandwidth));
		body_write(response, &spreading_factor, sizeof(spreading_factor));
		body_write(response, &coding_rate, sizeof(coding_rate));
		body_write(response, &tx_power, sizeof(tx_power));
		body_write(response, &preamble_len, sizeof(preamble_len));
		body_write(response, &sync_word, sizeof(sync_word));
		body_write(response, &checksum, sizeof(checksum));
		*radios_len += 1;
		index += radio_row.size;
	}

cleanup:
	octet_close(&stmt, file);
	return status;
}

int radio_parse(radio_t *radio, request_t *request) {
	request->body.pos = 0;

	uint8_t stage = 0;

	radio->spi_device_len = 0;
	const uint8_t spi_device_index = (uint8_t)request->body.pos;
	while (stage == 0 && radio->spi_device_len < 32 && request->body.pos < request->body.len) {
		const char *byte = body_read(request, sizeof(char));
		if (*byte == '\0') {
			stage = 1;
		} else {
			radio->spi_device_len++;
		}
	}
	radio->spi_device = &request->body.ptr[spi_device_index];
	if (stage != 1) {
		debug("found spi device with %hhu bytes\n", radio->spi_device_len);
		return -1;
	}

	radio->gpio_device_len = 0;
	const uint8_t gpio_device_index = (uint8_t)request->body.pos;
	while (stage == 1 && radio->gpio_device_len < 32 && request->body.pos < request->body.len) {
		const char *byte = body_read(request, sizeof(char));
		if (*byte == '\0') {
			stage = 2;
		} else {
			radio->gpio_device_len++;
		}
	}
	radio->gpio_device = &request->body.ptr[gpio_device_index];
	if (stage != 2) {
		debug("found gpio device with %hhu bytes\n", radio->gpio_device_len);
		return -1;
	}

	if (request->body.len < request->body.pos + sizeof(radio->gpio_int_pin)) {
		debug("missing gpio int pin on radio\n");
		return -1;
	}
	radio->gpio_int_pin = *(uint8_t *)body_read(request, sizeof(radio->gpio_int_pin));

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

	if (request->body.len < request->body.pos + sizeof(radio->preamble_len)) {
		debug("missing preamble len on radio\n");
		return -1;
	}
	radio->preamble_len = *(uint8_t *)body_read(request, sizeof(radio->preamble_len));

	if (request->body.len < request->body.pos + sizeof(radio->sync_word)) {
		debug("missing sync word on radio\n");
		return -1;
	}
	radio->sync_word = *(uint8_t *)body_read(request, sizeof(radio->sync_word));

	if (request->body.len < request->body.pos + sizeof(radio->checksum)) {
		debug("missing checksum on radio\n");
		return -1;
	}
	radio->checksum = *(bool *)body_read(request, sizeof(radio->checksum));

	if (request->body.len != request->body.pos) {
		debug("body len %u does not match body pos %u\n", request->body.len, request->body.pos);
		return -1;
	}

	return 0;
}

int radio_validate(radio_t *radio) {
	if (radio->gpio_int_pin < 3 || radio->gpio_int_pin > 40) {
		debug("invalid gpio int pin %hhu on radio\n", radio->gpio_int_pin);
		return -1;
	}

	if (radio->frequency < 400 * 1000 * 1000 || radio->frequency > 500 * 1000 * 1000) {
		debug("invalid frequency %u on radio\n", radio->frequency);
		return -1;
	}

	if (radio->bandwidth < 7800 || radio->bandwidth > 500 * 1000) {
		debug("invalid bandwidth %u on radio\n", radio->bandwidth);
		return -1;
	}

	if (radio->spreading_factor < 6 || radio->spreading_factor > 12) {
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

	if (radio->preamble_len < 6 || radio->preamble_len > 21) {
		debug("invalid preamble len %hhu on radio\n", radio->preamble_len);
		return -1;
	}

	return 0;
}

uint16_t radio_insert(octet_t *db, radio_t *radio) {
	uint16_t status;

	for (uint8_t index = 0; index < sizeof(*radio->id); index++) {
		(*radio->id)[index] = (uint8_t)(rand() & 0xff);
	}

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, radio_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("insert radio %.*s frequency %u\n", radio->spi_device_len, radio->spi_device, radio->frequency);

	off_t offset = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			break;
		}
		if (octet_row_read(&stmt, file, offset, db->row, radio_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t spi_device_len = octet_uint8_read(db->row, radio_row.spi_device_len);
		char *spi_device = octet_text_read(db->row, radio_row.spi_device);
		if (spi_device_len == radio->spi_device_len && memcmp(spi_device, radio->spi_device, radio->spi_device_len) == 0) {
			status = 409;
			warn("spi device %.*s already taken\n", radio->spi_device_len, radio->spi_device);
			goto cleanup;
		}
		offset += radio_row.size;
	}

	octet_blob_write(db->row, radio_row.id, (uint8_t *)radio->id, sizeof(*radio->id));
	octet_uint8_write(db->row, radio_row.spi_device_len, radio->spi_device_len);
	octet_text_write(db->row, radio_row.spi_device, radio->spi_device, radio->spi_device_len);
	octet_uint8_write(db->row, radio_row.gpio_device_len, radio->gpio_device_len);
	octet_text_write(db->row, radio_row.gpio_device, radio->gpio_device, radio->gpio_device_len);
	octet_uint8_write(db->row, radio_row.gpio_int_pin, radio->gpio_int_pin);
	octet_uint32_write(db->row, radio_row.frequency, radio->frequency);
	octet_uint32_write(db->row, radio_row.bandwidth, radio->bandwidth);
	octet_uint8_write(db->row, radio_row.spreading_factor, radio->spreading_factor);
	octet_uint8_write(db->row, radio_row.coding_rate, radio->coding_rate);
	octet_uint8_write(db->row, radio_row.tx_power, radio->tx_power);
	octet_uint8_write(db->row, radio_row.preamble_len, radio->preamble_len);
	octet_uint8_write(db->row, radio_row.sync_word, radio->sync_word);
	octet_uint8_write(db->row, radio_row.checksum, radio->checksum);

	offset = stmt.stat.st_size;
	if (octet_row_write(&stmt, file, offset, db->row, radio_row.size) == -1) {
		status = octet_error();
		goto cleanup;
	}

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

uint16_t radio_update(octet_t *db, radio_t *radio) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, radio_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("update radio %02x%02x\n", (*radio->id)[0], (*radio->id)[1]);

	off_t offset = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			warn("radio %02x%02x not found\n", (*radio->id)[0], (*radio->id)[1]);
			status = 404;
			break;
		}
		if (octet_row_read(&stmt, file, offset, db->row, radio_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t (*id)[8] = (uint8_t (*)[8])octet_blob_read(db->row, radio_row.id);
		if (memcmp(id, radio->id, sizeof(*radio->id)) == 0) {
			octet_uint8_write(db->row, radio_row.spi_device_len, radio->spi_device_len);
			octet_text_write(db->row, radio_row.spi_device, (char *)radio->spi_device, radio->spi_device_len);
			octet_uint8_write(db->row, radio_row.gpio_device_len, radio->gpio_device_len);
			octet_text_write(db->row, radio_row.gpio_device, (char *)radio->gpio_device, radio->gpio_device_len);
			octet_uint8_write(db->row, radio_row.gpio_int_pin, radio->gpio_int_pin);
			octet_uint32_write(db->row, radio_row.frequency, radio->frequency);
			octet_uint32_write(db->row, radio_row.bandwidth, radio->bandwidth);
			octet_uint8_write(db->row, radio_row.spreading_factor, radio->spreading_factor);
			octet_uint8_write(db->row, radio_row.coding_rate, radio->coding_rate);
			octet_uint8_write(db->row, radio_row.tx_power, radio->tx_power);
			octet_uint8_write(db->row, radio_row.preamble_len, radio->preamble_len);
			octet_uint8_write(db->row, radio_row.sync_word, radio->sync_word);
			octet_uint8_write(db->row, radio_row.checksum, radio->checksum);
			if (octet_row_write(&stmt, file, offset, db->row, radio_row.size) == -1) {
				status = octet_error();
				goto cleanup;
			}
			status = 0;
			break;
		}
		offset += radio_row.size;
	}

cleanup:
	octet_close(&stmt, file);
	return status;
}

uint16_t radio_delete(octet_t *db, radio_t *radio) {
	uint16_t status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, radio_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = octet_error();
		goto cleanup;
	}

	debug("delete radio %02x%02x\n", (*radio->id)[0], (*radio->id)[1]);

	off_t offset = 0;
	while (true) {
		if (offset >= stmt.stat.st_size) {
			warn("user %02x%02x not found\n", (*radio->id)[0], (*radio->id)[1]);
			status = 404;
			goto cleanup;
		}
		if (octet_row_read(&stmt, file, offset, db->row, radio_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		uint8_t (*id)[8] = (uint8_t (*)[8])octet_blob_read(db->row, radio_row.id);
		if (memcmp(id, radio->id, sizeof(*radio->id)) == 0) {
			break;
		}
		offset += radio_row.size;
	}

	off_t index = offset + radio_row.size;
	while (index < stmt.stat.st_size) {
		if (octet_row_read(&stmt, file, index, db->row, radio_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		if (octet_row_write(&stmt, file, index - radio_row.size, db->row, radio_row.size) == -1) {
			status = octet_error();
			goto cleanup;
		}
		index += radio_row.size;
	}

	if (octet_trunc(&stmt, file, stmt.stat.st_size - radio_row.size)) {
		status = 500;
		goto cleanup;
	}

	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

void radio_find(octet_t *db, request_t *request, response_t *response) {
	radio_query_t query = {.limit = 16, .offset = 0};
	if (strnfind(request->search.ptr, request->search.len, "order=", "&", &query.order, &query.order_len, 16) == -1) {
		response->status = 400;
		return;
	}

	if (strnfind(request->search.ptr, request->search.len, "sort=", "", &query.sort, &query.sort_len, 8) == -1) {
		response->status = 400;
		return;
	}

	uint8_t radios_len = 0;
	uint16_t status = radio_select(db, &query, response, &radios_len);
	if (status != 0) {
		response->status = status;
		return;
	}

	header_write(response, "content-type:application/octet-stream\r\n");
	header_write(response, "content-length:%u\r\n", response->body.len);
	info("found %hhu radios\n", radios_len);
	response->status = 200;
}

void radio_create(octet_t *db, request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	uint8_t id[8];
	radio_t radio = {.id = &id};
	if (request->body.len == 0 || radio_parse(&radio, request) == -1 || radio_validate(&radio) == -1) {
		response->status = 400;
		return;
	}

	uint16_t status = radio_insert(db, &radio);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("created radio %02x%02x\n", (*radio.id)[0], (*radio.id)[1]);
	response->status = 201;
}

void radio_modify(octet_t *db, request_t *request, response_t *response) {
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

	uint8_t id[8];
	if (base16_decode(id, sizeof(id), uuid, uuid_len) != 0) {
		warn("failed to decode uuid from base 16\n");
		response->status = 400;
		return;
	}

	radio_t radio = {.id = &id};
	if (request->body.len == 0 || radio_parse(&radio, request) == -1 || radio_validate(&radio) == -1) {
		response->status = 400;
		return;
	}

	uint16_t status = radio_update(db, &radio);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("updated radio %02x%02x\n", (*radio.id)[0], (*radio.id)[1]);
	response->status = 200;
}

void radio_remove(octet_t *db, request_t *request, response_t *response) {
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

	uint8_t id[8];
	if (base16_decode(id, sizeof(id), uuid, uuid_len) != 0) {
		warn("failed to decode uuid from base 16\n");
		response->status = 400;
		return;
	}

	radio_t radio = {.id = &id};
	uint16_t status = radio_delete(db, &radio);
	if (status != 0) {
		response->status = status;
		return;
	}

	info("deleted radio %02x%02x\n", (*radio.id)[0], (*radio.id)[1]);
	response->status = 200;
}
