#include "logger.h"
#include "radio.h"
#include <sqlite3.h>
#include <stdint.h>

int seed_radio(sqlite3 *database) {
	radio_t radios[] = {
			{
					.device = "/dev/spidev0.0",
					.device_len = 14,
					.frequency = 433 * 1000 * 1000,
					.bandwidth = 125 * 1000,
					.coding_rate = 5,
					.spreading_factor = 7,
					.tx_power = 2,
					.sync_word = 0x12,
					.checksum = true,
			},
	};

	for (uint8_t index = 0; index < sizeof(radios) / sizeof(radio_t); index++) {
		if (radio_insert(database, &radios[index]) != 0) {
			return -1;
		}
	}

	info("seeded table radio\n");
	return 0;
}

int seed(sqlite3 *database) {
	if (seed_radio(database) == -1) {
		return -1;
	}

	return 0;
}
