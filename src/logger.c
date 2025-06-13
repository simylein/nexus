#include "config.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

const char *purple = "\x1b[35m";
const char *blue = "\x1b[34m";
const char *cyan = "\x1b[36m";
const char *green = "\x1b[32m";
const char *yellow = "\x1b[33m";
const char *red = "\x1b[31m";
const char *bold = "\x1b[1m";
const char *normal = "\x1b[22m";
const char *reset = "\x1b[39m";

void timestamp(char (*buffer)[9]) {
	time_t now = time(NULL);
	time_t elapsed = now % 86400;
	uint8_t seconds = (uint8_t)(elapsed % 60);
	uint8_t minutes = (uint8_t)(elapsed / 60 % 60);
	uint8_t hours = (uint8_t)(elapsed / 3600);
	(*buffer)[0] = hours / 10 + '0';
	(*buffer)[1] = hours % 10 + '0';
	(*buffer)[2] = ':';
	(*buffer)[3] = minutes / 10 + '0';
	(*buffer)[4] = minutes % 10 + '0';
	(*buffer)[5] = ':';
	(*buffer)[6] = seconds / 10 + '0';
	(*buffer)[7] = seconds % 10 + '0';
	(*buffer)[8] = '\0';
}

void print(FILE *file, const char *time, const char *level, const char *color, const char *message, va_list args) {
	flockfile(file);
	fprintf(file, "%s%s%s%s%s %s %s%s%s%s%s ", bold, blue, name, reset, normal, time, bold, color, level, reset, normal);
	vfprintf(file, message, args);
	funlockfile(file);
}

void rx(const char *message, ...) {
	if (log_receives == true) {
		char buffer[9];
		timestamp(&buffer);
		va_list args;
		va_start(args, message);
		print(stdout, buffer, "rx", bold, message, args);
		va_end(args);
	}
}

void tx(const char *message, ...) {
	if (log_transmits == true) {
		char buffer[9];
		timestamp(&buffer);
		va_list args;
		va_start(args, message);
		print(stdout, buffer, "tx", bold, message, args);
		va_end(args);
	}
}

void trace(const char *message, ...) {
	if (log_level >= 6) {
		char buffer[9];
		timestamp(&buffer);
		va_list args;
		va_start(args, message);
		print(stdout, buffer, "trace", blue, message, args);
		va_end(args);
	}
}

void debug(const char *message, ...) {
	if (log_level >= 5) {
		char buffer[9];
		timestamp(&buffer);
		va_list args;
		va_start(args, message);
		print(stdout, buffer, "debug", cyan, message, args);
		va_end(args);
	}
}

void info(const char *message, ...) {
	if (log_level >= 4) {
		char buffer[9];
		timestamp(&buffer);
		va_list args;
		va_start(args, message);
		print(stdout, buffer, "info", green, message, args);
		va_end(args);
	}
}

void warn(const char *message, ...) {
	if (log_level >= 3) {
		char buffer[9];
		timestamp(&buffer);
		va_list args;
		va_start(args, message);
		print(stderr, buffer, "warn", yellow, message, args);
		va_end(args);
	}
}

void error(const char *message, ...) {
	if (log_level >= 2) {
		char buffer[9];
		timestamp(&buffer);
		va_list args;
		va_start(args, message);
		print(stderr, buffer, "error", red, message, args);
		va_end(args);
	}
}

void fatal(const char *message, ...) {
	if (log_level >= 1) {
		char buffer[9];
		timestamp(&buffer);
		va_list args;
		va_start(args, message);
		print(stderr, buffer, "fatal", purple, message, args);
		va_end(args);
	}
}
