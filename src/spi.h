#pragma once

#include <stdint.h>

int spi_init(const char *device, const uint8_t mode, const uint32_t speed, const uint8_t word_len);

int spi_read_register(int fd, uint8_t reg, uint8_t *value);
int spi_write_register(int fd, uint8_t reg, uint8_t value);
