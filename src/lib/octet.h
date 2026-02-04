#pragma once

#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct octet_t {
	const char *directory;
	uint8_t *row;
	uint8_t row_len;
	uint8_t *alpha;
	uint16_t alpha_len;
	uint8_t *bravo;
	uint16_t bravo_len;
	uint8_t *charlie;
	uint16_t charlie_len;
	uint8_t *delta;
	uint16_t delta_len;
	uint8_t *echo;
	uint16_t echo_len;
	uint8_t *chunk;
	uint16_t chunk_len;
	uint8_t *table;
	uint32_t table_len;
} octet_t;

typedef struct octet_stmt_t {
	int fd;
	struct stat stat;
} octet_stmt_t;

uint16_t octet_error(void);

int octet_mkdir(const char *directory);
int octet_rmdir(const char *directory);
int octet_creat(const char *file);
int octet_unlink(const char *file);

int octet_open(octet_stmt_t *stmt, const char *file, int open_flags, short lock_type);
int octet_trunc(octet_stmt_t *stmt, const char *file, off_t offset);
void octet_close(octet_stmt_t *stmt, const char *file);

ssize_t octet_row_read(octet_stmt_t *stmt, const char *file, off_t offset, uint8_t *row, uint8_t row_size);
ssize_t octet_row_write(octet_stmt_t *stmt, const char *file, off_t offset, uint8_t *row, uint8_t row_size);

uint8_t octet_uint8_read(uint8_t *row, uint8_t row_ind);
uint16_t octet_uint16_read(uint8_t *row, uint8_t row_ind);
uint32_t octet_uint32_read(uint8_t *row, uint8_t row_ind);
uint64_t octet_uint64_read(uint8_t *row, uint8_t row_ind);

int8_t octet_int8_read(uint8_t *row, uint8_t row_ind);
int16_t octet_int16_read(uint8_t *row, uint8_t row_ind);
int32_t octet_int32_read(uint8_t *row, uint8_t row_ind);
int64_t octet_int64_read(uint8_t *row, uint8_t row_ind);

uint8_t *octet_blob_read(uint8_t *row, uint8_t row_ind);
char *octet_text_read(uint8_t *row, uint8_t row_ind);

void octet_uint8_write(uint8_t *row, uint8_t row_ind, uint8_t value);
void octet_uint16_write(uint8_t *row, uint8_t row_ind, uint16_t value);
void octet_uint32_write(uint8_t *row, uint8_t row_ind, uint32_t value);
void octet_uint64_write(uint8_t *row, uint8_t row_ind, uint64_t value);

void octet_int8_write(uint8_t *row, uint8_t row_ind, int8_t value);
void octet_int16_write(uint8_t *row, uint8_t row_ind, int16_t value);
void octet_int32_write(uint8_t *row, uint8_t row_ind, int32_t value);
void octet_int64_write(uint8_t *row, uint8_t row_ind, int64_t value);

void octet_blob_write(uint8_t *row, uint8_t row_ind, uint8_t *value, uint8_t value_len);
void octet_text_write(uint8_t *row, uint8_t row_ind, char *value, uint8_t value_len);
