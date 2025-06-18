#include "config.h"
#include "error.h"
#include "format.h"
#include "init.h"
#include "logger.h"
#include "radio.h"
#include "seed.h"
#include "spi.h"
#include "thread.h"
#include <arpa/inet.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int server_sock;
struct sockaddr_in server_addr;

worker_t *workers;

void stop(int sig) {
	signal(sig, SIG_DFL);
	trace("received signal %d\n", sig);

	if (close(server_sock) == -1) {
		error("failed to close socket because %s\n", errno_str());
	}

	free(workers);

	info("graceful shutdown complete\n");
	exit(0);
}

int main(int argc, char *argv[]) {
	signal(SIGINT, &stop);
	signal(SIGTERM, &stop);

	if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
		info("available command line flags\n");
		info("--name              -n   name of application            (%s)\n", name);
		info("--database-file     -df  path to sqlite database file   (%s)\n", database_file);
		info("--database-timeout  -dt  milliseconds to wait for lock  (%hu)\n", database_timeout);
		info("--log-level         -ll  logging verbosity to print     (%s)\n", human_log_level(log_level));
		info("--log-receives      -lr  log incoming transmissions     (%s)\n", human_bool(log_receives));
		info("--log-transmits     -lt  log outgoing transmissions     (%s)\n", human_bool(log_transmits));
		exit(0);
	}

	if (argc >= 2 && (strcmp(argv[1], "--init") == 0 || strcmp(argv[1], "-i") == 0)) {
		sqlite3 *database;
		if (sqlite3_open_v2(database_file, &database, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
			fatal("failed to open %s because %s\n", database_file, sqlite3_errmsg(database));
			exit(1);
		}

		if (init(database) != 0) {
			fatal("failed to initialise database\n");
			exit(1);
		}

		if (sqlite3_close_v2(database) != SQLITE_OK) {
			fatal("failed to close %s because %s\n", database_file, sqlite3_errmsg(database));
			exit(1);
		}

		exit(0);
	}

	if (argc >= 2 && (strcmp(argv[1], "--seed") == 0 || strcmp(argv[1], "-s") == 0)) {
		sqlite3 *database;
		if (sqlite3_open_v2(database_file, &database, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
			fatal("failed to open %s because %s\n", database_file, sqlite3_errmsg(database));
			exit(1);
		}

		if (seed(database) != 0) {
			fatal("failed to seed database\n");
			exit(1);
		}

		if (sqlite3_close_v2(database) != SQLITE_OK) {
			fatal("failed to close %s because %s\n", database_file, sqlite3_errmsg(database));
			exit(1);
		}

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

	sqlite3 *database;
	int db_error = sqlite3_open_v2(database_file, &database, SQLITE_OPEN_READWRITE, NULL);
	if (db_error != SQLITE_OK) {
		fatal("failed to open %s because %s\n", database_file, sqlite3_errmsg(database));
		exit(1);
	}

	int exec_error = sqlite3_exec(database, "pragma foreign_keys = on;", NULL, NULL, NULL);
	if (exec_error != SQLITE_OK) {
		fatal("failed to enforce foreign key constraints because %s\n", sqlite3_errmsg(database));
		exit(1);
	}

	sqlite3_busy_timeout(database, database_timeout);

	radio_t radios[16];
	uint8_t radios_len = 0;
	if (radio_select(database, &radios, &radios_len) != 0) {
		fatal("failed to select radios\n");
		exit(1);
	};

	for (uint8_t ind = 0; ind < radios_len; ind++) {
		trace("%.*s %uhz %uhz 4/%hhucr %hhusf %hhudbm 0x%02x %s\n", radios[ind].device_len, radios[ind].device,
					radios[ind].frequency, radios[ind].bandwidth, radios[ind].coding_rate, radios[ind].spreading_factor,
					radios[ind].tx_power, radios[ind].sync_word, human_bool(radios[ind].checksum));
	}

	info("found %hhu radio configurations\n", radios_len);

	if (sqlite3_close_v2(database) != SQLITE_OK) {
		error("failed to close %s because %s\n", database_file, sqlite3_errmsg(database));
	}

	workers = malloc(radios_len * sizeof(worker_t));
	if (workers == NULL) {
		fatal("failed to allocate %zu bytes for workers because %s\n", radios_len * sizeof(worker_t), errno_str());
		exit(1);
	}

	for (uint8_t ind = 0; ind < radios_len; ind++) {
		char device[65];
		sprintf(device, "%.*s", (int)radios[ind].device_len, radios[ind].device);
		int fd = spi_init(device, 0, 8 * 1000 * 1000, 8);
		if (fd == -1) {
			fatal("failed to initialise spi for radio %s\n", radios[ind].device);
			exit(1);
		}

		workers[ind].arg.id = ind;
		workers[ind].arg.fd = fd;
		workers[ind].arg.radio = &radios[ind];

		if (spawn(&workers[ind], &thread, &fatal) == -1) {
			exit(1);
		}
	}

	info("spawned %hhu worker threads\n", radios_len);

	if ((server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		fatal("failed to create socket because %s\n", errno_str());
		exit(1);
	}

	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) == -1) {
		fatal("failed to set socket reuse address because %s\n", errno_str());
		exit(1);
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(address);
	server_addr.sin_port = htons(port);

	if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
		fatal("failed to bind to socket because %s\n", errno_str());
		exit(1);
	}

	if (listen(server_sock, backlog) == -1) {
		fatal("failed to listen on socket because %s\n", errno_str());
		exit(1);
	}

	info("listening on %s:%d\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

	while (true) {
		struct sockaddr_in client_addr;
		int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &(socklen_t){sizeof(client_addr)});

		if (client_sock == -1) {
			error("failed to accept client because %s\n", errno_str());
			continue;
		}

		char request_buffer[2048];
		char response_buffer[2048];

		ssize_t bytes_received = recv(client_sock, request_buffer, sizeof(request_buffer), 0);

		if (bytes_received == -1) {
			error("failed to receive data from client because %s\n", errno_str());
			goto cleanup;
		}
		if (bytes_received == 0) {
			warn("client did not send any data\n");
			goto cleanup;
		}

		trace("received %zd bytes from %s:%d\n", bytes_received, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

		const char request[] = "HEAD / HTTP/1.1\r\n";
		size_t response_length = 0;

		if ((size_t)bytes_received >= sizeof(request) - 1 && memcmp(request_buffer, request, sizeof(request) - 1) == 0) {
			const char response[] = "HTTP/1.1 200 OK\r\n\r\n";
			memcpy(response_buffer, response, sizeof(response) - 1);
			response_length += sizeof(response) - 1;
			info("acknowledged heartbeat request\n");
		} else {
			const char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
			memcpy(response_buffer, response, sizeof(response) - 1);
			response_length += sizeof(response) - 1;
		}

		ssize_t bytes_sent = send(client_sock, response_buffer, response_length, MSG_NOSIGNAL);

		if (bytes_sent == -1) {
			error("failed to send data to client because %s\n", errno_str());
			goto cleanup;
		}
		if (bytes_sent == 0) {
			warn("server did not send any data\n");
			goto cleanup;
		}

		trace("sent %zd bytes to %s:%d\n", bytes_sent, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

	cleanup:
		if (close(client_sock) == -1) {
			error("failed to close client socket because %s\n", errno_str());
		}
	}
}
