#pragma once

#include <stdbool.h>
#include <stdint.h>

extern const char *name;

extern uint8_t log_level;
extern bool log_receives;
extern bool log_transmits;

int configure(int argc, char *argv[]);
