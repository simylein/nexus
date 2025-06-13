#include "config.h"
#include "format.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
		info("available command line flags\n");
		info("--name              -n   name of application         (%s)\n", name);
		info("--log-level         -ll  logging verbosity to print  (%s)\n", human_log_level(log_level));
		info("--log-receives      -lr  log incoming transmissions  (%s)\n", human_bool(log_receives));
		info("--log-transmits     -lt  log outgoing transmissions  (%s)\n", human_bool(log_transmits));
		exit(0);
	}

	if (argc >= 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
		info("nexus version %s %s\n", version, commit);
		info("written by simylein in c\n");
		exit(0);
	}

	info("parsing command line flags\n");

	int cf_errors = configure(argc, argv);
	if (cf_errors != 0) {
		fatal("config contains %d errors\n", cf_errors);
		exit(1);
	}

	info("starting nexus application\n");
}
