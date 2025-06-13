#include <stdbool.h>
#include <stdint.h>

const char *human_bool(bool val) {
	if (val) {
		return "true";
	}
	return "false";
}

const char *human_log_level(uint8_t level) {
	switch (level) {
	case 1:
		return "fatal";
	case 2:
		return "error";
	case 3:
		return "warn";
	case 4:
		return "info";
	case 5:
		return "debug";
	case 6:
		return "trace";
	default:
		return "???";
	}
}
