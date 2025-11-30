#include "../lib/logger.h"
#include "device.h"
#include "host.h"
#include "radio.h"
#include "user.h"
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

uint8_t *user_ids;
uint8_t user_ids_len;
uint8_t *radio_ids;
uint8_t radio_ids_len;
uint8_t *device_ids;
uint8_t device_ids_len;
uint8_t *host_ids;
uint8_t host_ids_len;

int seed_user(sqlite3 *database) {
	char *usernames[] = {"alice", "bob", "charlie", "dave"};
	char *passwords[] = {".go4Alice", ".go4Bob", ".go4Charlie", ".go4Dave"};

	user_ids_len = sizeof(usernames) / sizeof(*usernames);
	user_ids = malloc(user_ids_len * sizeof(*((user_t *)0)->id));
	if (user_ids == NULL) {
		return -1;
	}

	for (uint8_t index = 0; index < user_ids_len; index++) {
		uint8_t permissions[4];
		user_t user = {
				.id = (uint8_t (*)[16])(&user_ids[index * sizeof(*((user_t *)0)->id)]),
				.username = usernames[index],
				.username_len = (uint8_t)strlen(usernames[index]),
				.password = passwords[index],
				.password_len = (uint8_t)strlen(passwords[index]),
				.permissions = &permissions,
		};

		if (user_insert(database, &user) != 0) {
			return -1;
		}
	}

	info("seeded table user\n");
	return 0;
}

int seed_radio(sqlite3 *database) {
	char *devices[] = {"/dev/spidev0.0", "/dev/spidev0.1", "/dev/spidev1.0", "/dev/spidev1.1"};

	radio_ids_len = sizeof(devices) / sizeof(*devices);
	radio_ids = malloc(radio_ids_len * sizeof(*((radio_t *)0)->id));
	if (radio_ids == NULL) {
		return -1;
	}

	for (uint8_t index = 0; index < radio_ids_len; index++) {
		radio_t radio = {
				.id = (uint8_t (*)[16])(&radio_ids[index * sizeof(*((radio_t *)0)->id)]),
				.device = devices[index],
				.device_len = (uint8_t)strlen(devices[index]),
				.frequency = (uint32_t)434225000 - (index * 200000),
				.bandwidth = 125000,
				.spreading_factor = 7 + index,
				.coding_rate = 5,
				.tx_power = 2,
				.preamble_len = 8,
				.sync_word = 0x12,
				.checksum = true,
		};

		if (radio_insert(database, &radio) != 0) {
			return -1;
		}
	}

	info("seeded table radio\n");
	return 0;
}

int seed_device(sqlite3 *database) {
	device_ids_len = 8;
	device_ids = malloc(device_ids_len * sizeof(*((radio_t *)0)->id));
	if (device_ids == NULL) {
		return -1;
	}

	for (uint8_t index = 0; index < device_ids_len; index++) {
		for (uint8_t ind = 0; ind < sizeof(*((device_t *)0)->id); ind++) {
			device_ids[index * sizeof(*((device_t *)0)->id) + ind] = (uint8_t)rand();
		}
		device_t device = {
				.id = (uint8_t (*)[16])(&device_ids[index * sizeof(*((device_t *)0)->id)]),
				.tag = (uint8_t (*)[2])(&device_ids[index * sizeof(*((device_t *)0)->id)]),
		};

		if (device_insert(database, &device) != 0) {
			return -1;
		}
	}

	info("seeded table device\n");
	return 0;
}

int seed_host(sqlite3 *database) {
	char *addresses[] = {"127.0.0.1", "127.0.0.1"};
	uint16_t ports[] = {1284, 1285};
	char *usernames[] = {"nexus", "nexus"};
	char *passwords[] = {".go4Nexus", ".go4Nexus"};

	host_ids_len = sizeof(addresses) / sizeof(*addresses);
	host_ids = malloc(host_ids_len * sizeof(*((host_t *)0)->id));
	if (host_ids == NULL) {
		return -1;
	}

	for (uint8_t index = 0; index < host_ids_len; index++) {
		host_t host = {
				.id = (uint8_t (*)[16])(&host_ids[index * sizeof(*((user_t *)0)->id)]),
				.address = addresses[index],
				.address_len = (uint8_t)strlen(addresses[index]),
				.port = ports[index],
				.username = usernames[index],
				.username_len = (uint8_t)strlen(usernames[index]),
				.password = passwords[index],
				.password_len = (uint8_t)strlen(passwords[index]),
		};

		if (host_insert(database, &host) != 0) {
			return -1;
		}
	}

	info("seeded table host\n");
	return 0;
}

int seed(sqlite3 *database) {
	srand((unsigned int)time(NULL));

	if (seed_user(database) == -1) {
		return -1;
	}
	if (seed_radio(database) == -1) {
		return -1;
	}
	if (seed_device(database) == -1) {
		return -1;
	}
	if (seed_host(database) == -1) {
		return -1;
	}

	free(user_ids);
	free(radio_ids);
	free(device_ids);
	free(host_ids);

	return 0;
}
