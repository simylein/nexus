#pragma once

#include <stdbool.h>
#include <stdint.h>

extern const char *name;

extern const char *address;
extern uint16_t port;

extern uint8_t backlog;

extern const char *database_file;
extern uint16_t database_timeout;

extern uint8_t log_level;
extern bool log_receives;
extern bool log_transmits;

int configure(int argc, char *argv[]);
