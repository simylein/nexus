#include "../app/schedule.h"
#include "../app/radio.h"
#include "../lib/logger.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <stdint.h>
#include <string.h>

int schedule_parse(schedule_t *schedule, request_t *request) {
	request->body.pos = 0;

	if (request->body.len < request->body.pos + sizeof(schedule->kind)) {
		debug("missing kind on schedule\n");
		return -1;
	}
	schedule->kind = *(uint8_t *)body_read(request, sizeof(schedule->kind));

	if (request->body.len < request->body.pos + sizeof(schedule->data_len)) {
		debug("missing data len on schedule\n");
		return -1;
	}
	schedule->data_len = *(uint8_t *)body_read(request, sizeof(schedule->data_len));

	if (request->body.len < request->body.pos + schedule->data_len) {
		debug("missing data on schedule\n");
		return -1;
	}
	memcpy(schedule->data, body_read(request, schedule->data_len), schedule->data_len);

	if (request->body.len < request->body.pos + sizeof(schedule->device_id)) {
		debug("missing device id on schedule\n");
		return -1;
	}
	memcpy(schedule->device_id, body_read(request, sizeof(schedule->device_id)), sizeof(schedule->device_id));

	if (request->body.len != request->body.pos) {
		debug("body len %u does not match body pos %u\n", request->body.len, request->body.pos);
		return -1;
	}

	return 0;
}

int schedule_validate(schedule_t *schedule) {
	for (uint8_t index = 0; index < comms.devices_len; index++) {
		if (memcmp(comms.devices[index].id, schedule->device_id, sizeof(schedule->device_id)) == 0) {
			memcpy(schedule->device_tag, comms.devices[index].tag, sizeof(*comms.devices[index].tag));
			return 0;
		}
	}

	debug("no registration for device %02x%02x\n", schedule->device_id[0], schedule->device_id[1]);
	return -1;
}

void schedule_create(request_t *request, response_t *response) {
	if (request->search.len != 0) {
		response->status = 400;
		return;
	}

	schedule_t schedule;
	if (request->body.len == 0 || schedule_parse(&schedule, request) == -1 || schedule_validate(&schedule) == -1) {
		response->status = 400;
		return;
	}

	if (schedule_push(&schedule) != 0) {
		response->status = 507;
		return;
	}

	info("created schedule for device %02x%02x\n", schedule.device_id[0], schedule.device_id[1]);
	response->status = 201;
}
