#include "config.h"
#include "error.h"
#include "format.h"
#include "init.h"
#include "logger.h"
#include "radio.h"
#include "seed.h"
#include "spi.h"
#include "thread.h"
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
		info("available command line flags\n");
		info("--name              -n   name of application            (%s)\n", name);
		info("--database-file     -df  path to sqlite database file   (%s)\n", database_file);
		info("--database-timeout  -dt  milliseconds to wait for lock  (%hu)\n", database_timeout);
		info("--log-level         -ll  logging verbosity to print     (%s)\n", human_log_level(log_level));
		info("--log-receives      -lr  log incoming transmissions     (%s)\n", human_bool(log_receives));
		info("--log-transmits     -lt  log outgoing transmissions     (%s)\n", human_bool(log_transmits));
		exit(0);
	}

	if (argc >= 2 && (strcmp(argv[1], "--init") == 0 || strcmp(argv[1], "-i") == 0)) {
		sqlite3 *database;
		if (sqlite3_open_v2(database_file, &database, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
			fatal("failed to open %s because %s\n", database_file, sqlite3_errmsg(database));
			exit(1);
		}

		if (init(database) != 0) {
			fatal("failed to initialise database\n");
			exit(1);
		}

		if (sqlite3_close_v2(database) != SQLITE_OK) {
			fatal("failed to close %s because %s\n", database_file, sqlite3_errmsg(database));
			exit(1);
		}

		exit(0);
	}

	if (argc >= 2 && (strcmp(argv[1], "--seed") == 0 || strcmp(argv[1], "-s") == 0)) {
		sqlite3 *database;
		if (sqlite3_open_v2(database_file, &database, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
			fatal("failed to open %s because %s\n", database_file, sqlite3_errmsg(database));
			exit(1);
		}

		if (seed(database) != 0) {
			fatal("failed to seed database\n");
			exit(1);
		}

		if (sqlite3_close_v2(database) != SQLITE_OK) {
			fatal("failed to close %s because %s\n", database_file, sqlite3_errmsg(database));
			exit(1);
		}

		exit(0);
	}

	if (argc >= 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
		info("nexus version %s %s\n", version, commit);
		info("written by simylein in c\n");
		exit(0);
	}

	info("parsing command line flags\n");

	int cf_errors = configure(argc, argv);
	if (cf_errors != 0) {
		fatal("config contains %d errors\n", cf_errors);
		exit(1);
	}

	info("starting nexus application\n");

	sqlite3 *database;
	int db_error = sqlite3_open_v2(database_file, &database, SQLITE_OPEN_READWRITE, NULL);
	if (db_error != SQLITE_OK) {
		fatal("failed to open %s because %s\n", database_file, sqlite3_errmsg(database));
		exit(1);
	}

	int exec_error = sqlite3_exec(database, "pragma foreign_keys = on;", NULL, NULL, NULL);
	if (exec_error != SQLITE_OK) {
		fatal("failed to enforce foreign key constraints because %s\n", sqlite3_errmsg(database));
		exit(1);
	}

	sqlite3_busy_timeout(database, database_timeout);

	radio_t radios[16];
	uint8_t radios_len;
	if (radio_select(database, &radios, &radios_len) != 0) {
		fatal("failed to select radios\n");
		exit(1);
	};

	for (uint8_t ind = 0; ind < radios_len; ind++) {
		trace("%.*s %uhz %uhz 4/%hhucr %hhusf %hhudbm 0x%02x %s\n", radios[ind].device_len, radios[ind].device,
					radios[ind].frequency, radios[ind].bandwidth, radios[ind].coding_rate, radios[ind].spreading_factor,
					radios[ind].tx_power, radios[ind].sync_word, human_bool(radios[ind].checksum));
	}

	info("found %hhu radio configurations\n", radios_len);

	if (sqlite3_close_v2(database) != SQLITE_OK) {
		error("failed to close %s because %s\n", database_file, sqlite3_errmsg(database));
	}

	worker_t *workers = malloc(radios_len * sizeof(worker_t));
	if (workers == NULL) {
		fatal("failed to allocate %zu bytes for workers because %s\n", radios_len * sizeof(worker_t), errno_str());
		exit(1);
	}

	for (uint8_t ind = 0; ind < radios_len; ind++) {
		int fd = spi_init(radios[ind].device, 0, 8 * 1000 * 1000, 8);
		if (fd == -1) {
			fatal("failed to initialise spi for radio %s\n", radios[ind].device);
			exit(1);
		}

		if (spawn(&workers[ind], ind, fd, &radios[ind], &thread, &fatal) == -1) {
			exit(1);
		}
	}

	// FIXME make main thread do something useful
	while (true) {
		sleep(60);
	}
}
