#include "logger.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char *name = "nexus";

uint8_t log_level = 4;
bool log_receives = true;
bool log_transmits = true;

bool match_arg(const char *flag, const char *verbose, const char *concise) {
	return strcmp(flag, verbose) == 0 || strcmp(flag, concise) == 0;
}

const char *next_arg(const int argc, char *argv[], int *ind) {
	(*ind)++;
	if (*ind < argc) {
		return argv[*ind];
	}
	return NULL;
}

int parse_bool(const char *arg, const char *key, bool *value) {
	if (arg == NULL) {
		error("please provide a value for %s\n", key);
		return 1;
	}

	if (strcmp(arg, "false") == 0) {
		*value = false;
	} else if (strcmp(arg, "true") == 0) {
		*value = true;
	} else {
		error("%s must be either true or false\n", key);
		return 1;
	}

	return 0;
}

int parse_str(const char *arg, const char *key, size_t min, size_t max, const char **value) {
	if (arg == NULL) {
		error("please provide a value for %s\n", key);
		return 1;
	}

	size_t len = strlen(arg);
	if (len < min || len > max) {
		error("%s must be between %zu and %zu characters\n", key, min, max);
		return 1;
	}

	*value = arg;
	return 0;
}

int parse_log_level(const char *arg, uint8_t *value) {
	if (arg == NULL) {
		error("please provide a value for log level\n");
		return 1;
	}

	if (strcmp(arg, "fatal") == 0) {
		*value = 1;
	} else if (strcmp(arg, "error") == 0) {
		*value = 2;
	} else if (strcmp(arg, "warn") == 0) {
		*value = 3;
	} else if (strcmp(arg, "info") == 0) {
		*value = 4;
	} else if (strcmp(arg, "debug") == 0) {
		*value = 5;
	} else if (strcmp(arg, "trace") == 0) {
		*value = 6;
	} else {
		error("log level must be one of trace debug info warn error fatal\n");
		return 1;
	}

	return 0;
}

int configure(int argc, char *argv[]) {
	int errors = 0;

	for (int ind = 1; ind < argc; ind++) {
		const char *flag = argv[ind];
		if (match_arg(flag, "--name", "-n")) {
			const char *value = next_arg(argc, argv, &ind);
			errors += parse_str(value, "name", 2, 8, &name);
		} else if (match_arg(flag, "--log-level", "-ll")) {
			const char *value = next_arg(argc, argv, &ind);
			errors += parse_log_level(value, &log_level);
		} else if (match_arg(flag, "--log-receives", "-lr")) {
			const char *value = next_arg(argc, argv, &ind);
			errors += parse_bool(value, "log receives", &log_receives);
		} else if (match_arg(flag, "--log-transmits", "-lt")) {
			const char *value = next_arg(argc, argv, &ind);
			errors += parse_bool(value, "log transmits", &log_transmits);
		} else {
			errors++;
			error("unknown argument %s\n", flag);
		}
	}

	return errors;
}
