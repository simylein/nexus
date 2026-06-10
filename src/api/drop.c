
#include "../lib/logger.h"
#include "../lib/octet.h"
#include "device.h"
#include "downlink.h"
#include "host.h"
#include "radio.h"
#include "uplink.h"
#include "user.h"
#include <stdio.h>

int drop_user(octet_t *db) {
	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, user_file) == -1) {
		error("failed to sprintf to file\n");
		return -1;
	}

	if (octet_unlink(file) == -1) {
		return -1;
	}

	info("unlinked file %s\n", user_file);
	return 0;
}

int drop_radio(octet_t *db) {
	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, radio_file) == -1) {
		error("failed to sprintf to file\n");
		return -1;
	}

	if (octet_unlink(file) == -1) {
		return -1;
	}

	info("unlinked file %s\n", radio_file);
	return 0;
}

int drop_device(octet_t *db) {
	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, device_file) == -1) {
		error("failed to sprintf to file\n");
		return -1;
	}

	if (octet_unlink(file) == -1) {
		return -1;
	}

	info("unlinked file %s\n", device_file);
	return 0;
}

int drop_host(octet_t *db) {
	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, host_file) == -1) {
		error("failed to sprintf to file\n");
		return -1;
	}

	if (octet_unlink(file) == -1) {
		return -1;
	}

	info("unlinked file %s\n", host_file);
	return 0;
}

int drop_uplink(octet_t *db) {
	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, uplink_file) == -1) {
		error("failed to sprintf to file\n");
		return -1;
	}

	if (octet_unlink(file) == -1) {
		return -1;
	}

	info("unlinked file %s\n", uplink_file);
	return 0;
}

int drop_downlink(octet_t *db) {
	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, downlink_file) == -1) {
		error("failed to sprintf to file\n");
		return -1;
	}

	if (octet_unlink(file) == -1) {
		return -1;
	}

	info("unlinked file %s\n", downlink_file);
	return 0;
}

int drop(octet_t *db) {
	if (drop_user(db) == -1) {
		return -1;
	}
	if (drop_radio(db) == -1) {
		return -1;
	}
	if (drop_device(db) == -1) {
		return -1;
	}
	if (drop_host(db) == -1) {
		return -1;
	}
	if (drop_uplink(db) == -1) {
		return -1;
	}
	if (drop_downlink(db) == -1) {
		return -1;
	}

	if (octet_rmdir(db->directory) == -1) {
		return -1;
	}

	info("removed directory %s\n", db->directory);

	return 0;
}
