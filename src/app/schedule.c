#include "schedule.h"
#include "../lib/config.h"
#include "../lib/error.h"
#include "../lib/logger.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

schedules_t schedules = {
		.len = 0,
		.lock = PTHREAD_MUTEX_INITIALIZER,
};

int schedule_init(void) {
	schedules.ptr = malloc(schedules_size * sizeof(*schedules.ptr));
	if (schedules.ptr == NULL) {
		fatal("failed to allocate %zu bytes for schedules because %s\n", schedules_size * sizeof(*schedules.ptr), errno_str());
		return -1;
	}

	schedules.cap = schedules_size;

	return 0;
}

int schedule_push(schedule_t *schedule) {
	int status;

	pthread_mutex_lock(&schedules.lock);

	if (schedules.len >= schedules.cap) {
		warn("schedules len %hhu are full\n", schedules.len);
		status = -1;
		goto cleanup;
	}

	schedules.ptr[schedules.len] = *schedule;
	schedules.len += 1;

	status = 0;
	trace("increased schedules len to %hhu\n", schedules.len);

cleanup:
	pthread_mutex_unlock(&schedules.lock);
	return status;
}

int schedule_find(schedule_t *schedule, uint8_t (*device_tag)[2]) {
	int status;

	pthread_mutex_lock(&schedules.lock);

	for (uint8_t index = 0; index < schedules.len; index++) {
		schedule_t *current = &schedules.ptr[index];

		if (memcmp(current->device_tag, device_tag, sizeof(*device_tag)) == 0) {
			*schedule = *current;

			if (index + 1 < schedules.len) {
				memmove(&schedules.ptr[index], &schedules.ptr[index + 1], (size_t)(schedules.len - index - 1) * sizeof(*schedules.ptr));
			}
			schedules.len -= 1;
			status = 0;
			trace("decrease schedules len to %hhu\n", schedules.len);
			goto cleanup;
		}
	}

	status = -1;

cleanup:
	pthread_mutex_unlock(&schedules.lock);
	return status;
}
