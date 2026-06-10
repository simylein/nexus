#include "../lib/logger.h"
#include "../lib/octet.h"
#include "device.h"
#include "downlink.h"
#include "host.h"
#include "radio.h"
#include "uplink.h"
#include "user.h"
#include <fcntl.h>
#include <stdio.h>

int wipe_user(octet_t *db) {
	int status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, user_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = -1;
		goto cleanup;
	}

	if (octet_trunc(&stmt, file, 0) == -1) {
		status = -1;
		goto cleanup;
	}

	info("wiped file %s\n", user_file);
	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

int wipe_radio(octet_t *db) {
	int status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, radio_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = -1;
		goto cleanup;
	}

	if (octet_trunc(&stmt, file, 0) == -1) {
		status = -1;
		goto cleanup;
	}

	info("wiped file %s\n", radio_file);
	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

int wipe_device(octet_t *db) {
	int status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, device_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = -1;
		goto cleanup;
	}

	if (octet_trunc(&stmt, file, 0) == -1) {
		status = -1;
		goto cleanup;
	}

	info("wiped file %s\n", device_file);
	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

int wipe_host(octet_t *db) {
	int status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, host_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = -1;
		goto cleanup;
	}

	if (octet_trunc(&stmt, file, 0) == -1) {
		status = -1;
		goto cleanup;
	}

	info("wiped file %s\n", host_file);
	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

int wipe_uplink(octet_t *db) {
	int status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, uplink_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = -1;
		goto cleanup;
	}

	if (octet_trunc(&stmt, file, 0) == -1) {
		status = -1;
		goto cleanup;
	}

	info("wiped file %s\n", uplink_file);
	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

int wipe_downlink(octet_t *db) {
	int status;

	char file[128];
	if (sprintf(file, "%s/%s.data", db->directory, downlink_file) == -1) {
		error("failed to sprintf to file\n");
		return 500;
	}

	octet_stmt_t stmt;
	if (octet_open(&stmt, file, O_RDWR, F_WRLCK) == -1) {
		status = -1;
		goto cleanup;
	}

	if (octet_trunc(&stmt, file, 0) == -1) {
		status = -1;
		goto cleanup;
	}

	info("wiped file %s\n", downlink_file);
	status = 0;

cleanup:
	octet_close(&stmt, file);
	return status;
}

int wipe(octet_t *db) {
	if (wipe_user(db) == -1) {
		return -1;
	}
	if (wipe_radio(db) == -1) {
		return -1;
	}
	if (wipe_device(db) == -1) {
		return -1;
	}
	if (wipe_host(db) == -1) {
		return -1;
	}

	return 0;
}
