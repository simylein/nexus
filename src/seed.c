#include "device.h"
#include "logger.h"
#include <sqlite3.h>
#include <stdint.h>

int seed_device(sqlite3 *database) {
	device_t devices[] = {
			{.tag = {0x9f, 0x3a}},
			{.tag = {0xb2, 0x7e}},
			{.tag = {0xc4, 0xd1}},
			{.tag = {0x7a, 0x6f}},
	};

	for (uint8_t index = 0; index < sizeof(devices) / sizeof(device_t); index++) {
		if (device_insert(database, &devices[index]) != 0) {
			return -1;
		}
	}

	info("seeded table device\n");
	return 0;
}

int seed(sqlite3 *database) {
	if (seed_device(database) == -1) {
		return -1;
	}

	return 0;
}
