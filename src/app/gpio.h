#pragma once

#include <stdint.h>

int gpio_init(const char *device, uint8_t pin);
int gpio_wait_edge(int gpio_fd);
