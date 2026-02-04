#include "../lib/logger.h"
#include "../lib/octet.h"
#include "device.h"
#include "host.h"
#include "radio.h"
#include "user.h"
#include <stdlib.h>

int init_user(octet_t *db) {
	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, user_file) == -1) {
		error("failed to sprintf to file\n");
		return -1;
	}

	if (octet_creat(file) == -1) {
		return -1;
	}

	info("created file %s\n", user_file);
	return 0;
}

int init_radio(octet_t *db) {
	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, radio_file) == -1) {
		error("failed to sprintf to file\n");
		return -1;
	}

	if (octet_creat(file) == -1) {
		return -1;
	}

	info("created file %s\n", radio_file);
	return 0;
}

int init_device(octet_t *db) {
	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, device_file) == -1) {
		error("failed to sprintf to file\n");
		return -1;
	}

	if (octet_creat(file) == -1) {
		return -1;
	}

	info("created file %s\n", device_file);
	return 0;
}

int init_host(octet_t *db) {
	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, host_file) == -1) {
		error("failed to sprintf to file\n");
		return -1;
	}

	if (octet_creat(file) == -1) {
		return -1;
	}

	info("created file %s\n", host_file);
	return 0;
}

int init(octet_t *db) {
	if (octet_mkdir(db->directory) == -1) {
		return -1;
	}

	info("created directory %s\n", db->directory);

	if (init_user(db) == -1) {
		return -1;
	}
	if (init_radio(db) == -1) {
		return -1;
	}
	if (init_device(db) == -1) {
		return -1;
	}
	if (init_host(db) == -1) {
		return -1;
	}

	return 0;
}
