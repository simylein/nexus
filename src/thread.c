#include "thread.h"
#include "error.h"
#include "logger.h"
#include "radio.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

int spawn(worker_t *worker, uint8_t id, int fd, radio_t *radio, void *(*function)(void *),
					void (*logger)(const char *message, ...) __attribute__((format(printf, 1, 2)))) {
	worker->arg.id = id;
	worker->arg.fd = fd;
	worker->arg.radio = radio;
	trace("spawning worker thread %hhu\n", id);

	int spawn_error = pthread_create(&worker->thread, NULL, function, (void *)&worker->arg);
	if (spawn_error != 0) {
		errno = spawn_error;
		logger("failed to spawn worker thread %hhu because %s\n", worker->arg.id, errno_str());
		return -1;
	}

	int detach_error = pthread_detach(worker->thread);
	if (detach_error != 0) {
		errno = detach_error;
		logger("failed to detach worker thread %hhu because %s\n", worker->arg.id, errno_str());
		return -1;
	}

	return 0;
}
