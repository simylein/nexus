#pragma once

#include <stdint.h>

int gpio_init_edge(const char *device, uint8_t pin);
int gpio_init_output(const char *device, uint8_t pin, uint8_t value);
int gpio_wait_edge(int gpio_fd);
int gpio_write_value(int gpio_fd, uint8_t value);
