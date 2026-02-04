#include "octet.h"
#include "error.h"
#include "logger.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

uint16_t octet_error(void) {
	switch (errno) {
	case EINTR:
		return 503;
	case ENFILE:
		return 503;
	case EMFILE:
		return 503;
	case ENOSPC:
		return 507;
	case EROFS:
		return 423;
	case EDQUOT:
		return 507;
	default:
		return 500;
	}
}

int octet_mkdir(const char *directory) {
	if (mkdir(directory, S_IRUSR | S_IWUSR | S_IXUSR) == -1) {
		error("failed to create directory %s because %s\n", directory, errno_str());
		return -1;
	}

	return 0;
}

int octet_rmdir(const char *directory) {
	if (rmdir(directory) == -1) {
		error("failed to remove directory %s because %s\n", directory, errno_str());
		return -1;
	}

	return 0;
}

int octet_creat(const char *file) {
	int fd = open(file, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		error("failed to create file %s because %s\n", file, errno_str());
		return -1;
	}

	if (close(fd) == -1) {
		error("failed to close file %s because %s\n", file, errno_str());
		return -1;
	}

	return 0;
}

int octet_unlink(const char *file) {
	if (unlink(file) == -1) {
		error("failed to unlink file %s because %s\n", file, errno_str());
		return -1;
	}

	return 0;
}

int octet_open(octet_stmt_t *stmt, const char *file, int open_flags, short lock_type) {
	trace("opening file %s\n", file);

	stmt->fd = open(file, open_flags);
	if (stmt->fd == -1) {
		error("failed to open %s because %s\n", file, errno_str());
		return -1;
	}

	struct flock flock = {.l_type = lock_type, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
	if (fcntl(stmt->fd, F_SETLKW, &flock) == -1) {
		error("failed to lock %s because %s\n", file, errno_str());
		return -1;
	}

	if (fstat(stmt->fd, &stmt->stat) == -1) {
		error("failed to stat %s because %s\n", file, errno_str());
		return -1;
	}

	return 0;
}

int octet_trunc(octet_stmt_t *stmt, const char *file, off_t offset) {
	trace("truncating file %s\n", file);

	if (ftruncate(stmt->fd, offset) == -1) {
		error("failed to truncate %s because %s\n", file, errno_str());
		return -1;
	}

	return 0;
}

void octet_close(octet_stmt_t *stmt, const char *file) {
	trace("closing file %s\n", file);

	if (stmt->fd != -1 && close(stmt->fd) == -1) {
		error("failed to close %s because %s\n", file, errno_str());
	}
}

ssize_t octet_row_read(octet_stmt_t *stmt, const char *file, off_t offset, uint8_t *row, uint8_t row_size) {
	if (lseek(stmt->fd, offset, SEEK_SET) == -1) {
		error("failed to seek to offset %zu on file %s because %s\n", (size_t)offset, file, errno_str());
		return -1;
	}

	ssize_t bytes = read(stmt->fd, row, row_size);
	if (bytes == -1) {
		error("failed read %hhu bytes from %s because %s\n", row_size, file, errno_str());
		return -1;
	}

	if (bytes != row_size) {
		error("failed to fully read %hhu bytes from %s\n", row_size, file);
		return -1;
	}

	return bytes;
}

ssize_t octet_row_write(octet_stmt_t *stmt, const char *file, off_t offset, uint8_t *row, uint8_t row_size) {
	if (lseek(stmt->fd, offset, SEEK_SET) == -1) {
		error("failed to seek to offset %zu on file %s because %s\n", (size_t)offset, file, errno_str());
		return -1;
	}

	ssize_t bytes = write(stmt->fd, row, row_size);
	if (bytes == -1) {
		error("failed write %hhu bytes from %s because %s\n", row_size, file, errno_str());
		return -1;
	}

	if (bytes != row_size) {
		error("failed to fully write %hhu bytes from %s because %s\n", row_size, file, errno_str());
		return -1;
	}

	return bytes;
}

uint8_t octet_uint8_read(uint8_t *row, uint8_t row_ind) {
	uint8_t value = row[row_ind];
	return value;
}

uint16_t octet_uint16_read(uint8_t *row, uint8_t row_ind) {
	uint16_t value = (uint16_t)(((uint16_t)(row[row_ind]) << 8) | (uint16_t)row[row_ind + 1]);
	return value;
}

uint32_t octet_uint32_read(uint8_t *row, uint8_t row_ind) {
	uint32_t value = (uint32_t)(((uint32_t)(row[row_ind]) << 24) | ((uint32_t)(row[row_ind + 1]) << 16) |
															((uint32_t)(row[row_ind + 2]) << 8) | (uint32_t)row[row_ind + 3]);
	return value;
}

uint64_t octet_uint64_read(uint8_t *row, uint8_t row_ind) {
	uint64_t value = (uint64_t)(((uint64_t)(row[row_ind]) << 56) | ((uint64_t)(row[row_ind + 1]) << 48) |
															((uint64_t)(row[row_ind + 2]) << 40) | ((uint64_t)(row[row_ind + 3]) << 32) |
															((uint64_t)(row[row_ind + 4]) << 24) | ((uint64_t)(row[row_ind + 5]) << 16) |
															((uint64_t)(row[row_ind + 6]) << 8) | (uint64_t)row[row_ind + 7]);
	return value;
}

int8_t octet_int8_read(uint8_t *row, uint8_t row_ind) {
	int8_t value = (int8_t)row[row_ind];
	return value;
}

int16_t octet_int16_read(uint8_t *row, uint8_t row_ind) {
	int16_t value = (int16_t)(((uint16_t)(row[row_ind]) << 8) | (uint16_t)row[row_ind + 1]);
	return value;
}

int32_t octet_int32_read(uint8_t *row, uint8_t row_ind) {
	int32_t value = (int32_t)(((uint32_t)(row[row_ind]) << 24) | ((uint32_t)(row[row_ind + 1]) << 16) |
														((uint32_t)(row[row_ind + 2]) << 8) | (uint32_t)row[row_ind + 3]);
	return value;
}

int64_t octet_int64_read(uint8_t *row, uint8_t row_ind) {
	int64_t value =
			(int64_t)(((uint64_t)(row[row_ind]) << 56) | ((uint64_t)(row[row_ind + 1]) << 48) | ((uint64_t)(row[row_ind + 2]) << 40) |
								((uint64_t)(row[row_ind + 3]) << 32) | ((uint64_t)(row[row_ind + 4]) << 24) |
								((uint64_t)(row[row_ind + 5]) << 16) | ((uint64_t)(row[row_ind + 6]) << 8) | (uint64_t)row[row_ind + 7]);
	return value;
}

uint8_t *octet_blob_read(uint8_t *row, uint8_t row_ind) {
	uint8_t *value = &row[row_ind];
	return value;
}

char *octet_text_read(uint8_t *row, uint8_t row_ind) {
	char *value = (char *)&row[row_ind];
	return value;
}

void octet_uint8_write(uint8_t *row, uint8_t row_ind, uint8_t value) { row[row_ind] = value; }

void octet_uint16_write(uint8_t *row, uint8_t row_ind, uint16_t value) {
	row[row_ind] = (uint8_t)(value >> 8);
	row[row_ind + 1] = (uint8_t)(value & 0xff);
}

void octet_uint32_write(uint8_t *row, uint8_t row_ind, uint32_t value) {
	row[row_ind] = (uint8_t)(value >> 24);
	row[row_ind + 1] = (uint8_t)(value >> 16);
	row[row_ind + 2] = (uint8_t)(value >> 8);
	row[row_ind + 3] = (uint8_t)(value & 0xff);
}

void octet_uint64_write(uint8_t *row, uint8_t row_ind, uint64_t value) {
	row[row_ind] = (uint8_t)(value >> 56);
	row[row_ind + 1] = (uint8_t)(value >> 48);
	row[row_ind + 2] = (uint8_t)(value >> 40);
	row[row_ind + 3] = (uint8_t)(value >> 32);
	row[row_ind + 4] = (uint8_t)(value >> 24);
	row[row_ind + 5] = (uint8_t)(value >> 16);
	row[row_ind + 6] = (uint8_t)(value >> 8);
	row[row_ind + 7] = (uint8_t)(value & 0xff);
}

void octet_int8_write(uint8_t *row, uint8_t row_ind, int8_t value) { row[row_ind] = (uint8_t)value; }

void octet_int16_write(uint8_t *row, uint8_t row_ind, int16_t value) {
	row[row_ind] = (uint8_t)(value >> 8);
	row[row_ind + 1] = (uint8_t)(value & 0xff);
}

void octet_int32_write(uint8_t *row, uint8_t row_ind, int32_t value) {
	row[row_ind] = (uint8_t)(value >> 24);
	row[row_ind + 1] = (uint8_t)(value >> 16);
	row[row_ind + 2] = (uint8_t)(value >> 8);
	row[row_ind + 3] = (uint8_t)(value & 0xff);
}

void octet_int64_write(uint8_t *row, uint8_t row_ind, int64_t value) {
	row[row_ind] = (uint8_t)(value >> 56);
	row[row_ind + 1] = (uint8_t)(value >> 48);
	row[row_ind + 2] = (uint8_t)(value >> 40);
	row[row_ind + 3] = (uint8_t)(value >> 32);
	row[row_ind + 4] = (uint8_t)(value >> 24);
	row[row_ind + 5] = (uint8_t)(value >> 16);
	row[row_ind + 6] = (uint8_t)(value >> 8);
	row[row_ind + 7] = (uint8_t)(value & 0xff);
}

void octet_blob_write(uint8_t *row, uint8_t row_ind, uint8_t *value, uint8_t value_len) {
	memcpy(&row[row_ind], value, value_len);
}

void octet_text_write(uint8_t *row, uint8_t row_ind, char *value, uint8_t value_len) {
	memcpy(&row[row_ind], value, value_len);
}
