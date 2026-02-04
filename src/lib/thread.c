#include "thread.h"
#include "app.h"
#include "config.h"
#include "error.h"
#include "logger.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

queue_t queue = {
		.head = 0,
		.tail = 0,
		.size = 0,
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.filled = PTHREAD_COND_INITIALIZER,
		.available = PTHREAD_COND_INITIALIZER,
};

thread_pool_t thread_pool = {
		.size = 0,
		.load = 0,
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.scale = PTHREAD_COND_INITIALIZER,
		.available = PTHREAD_COND_INITIALIZER,
};

void cancel(void *lock) { pthread_mutex_unlock((pthread_mutex_t *)lock); }

int spawn(worker_t *worker, uint8_t id, void *(*function)(void *),
					void (*logger)(const char *message, ...) __attribute__((format(printf, 1, 2)))) {
	trace("spawning worker thread %hhu\n", id);

	worker->arg.id = id;
	worker->arg.db.directory = database_directory;

	worker->arg.database_buffer = malloc(database_buffer * sizeof(char));
	if (worker->arg.database_buffer == NULL) {
		logger("failed to allocate %u bytes because %s\n", database_buffer, errno_str());
		return -1;
	}

	uint32_t offset = 0;

	worker->arg.db.row = (uint8_t *)&worker->arg.database_buffer[offset];
	worker->arg.db.row_len = UINT8_MAX;
	offset += worker->arg.db.row_len;

	worker->arg.db.alpha = (uint8_t *)&worker->arg.database_buffer[offset];
	worker->arg.db.alpha_len = (uint16_t)(database_buffer / 16);
	offset += worker->arg.db.alpha_len;

	worker->arg.db.bravo = (uint8_t *)&worker->arg.database_buffer[offset];
	worker->arg.db.bravo_len = (uint16_t)(database_buffer / 16);
	offset += worker->arg.db.bravo_len;

	worker->arg.db.charlie = (uint8_t *)&worker->arg.database_buffer[offset];
	worker->arg.db.charlie_len = (uint16_t)(database_buffer / 16);
	offset += worker->arg.db.charlie_len;

	worker->arg.db.delta = (uint8_t *)&worker->arg.database_buffer[offset];
	worker->arg.db.delta_len = (uint16_t)(database_buffer / 16);
	offset += worker->arg.db.delta_len;

	worker->arg.db.echo = (uint8_t *)&worker->arg.database_buffer[offset];
	worker->arg.db.echo_len = (uint16_t)(database_buffer / 16);
	offset += worker->arg.db.echo_len;

	worker->arg.db.chunk = (uint8_t *)&worker->arg.database_buffer[offset];
	worker->arg.db.chunk_len = (uint16_t)(database_buffer / 16);
	offset += worker->arg.db.chunk_len;

	worker->arg.db.table = (uint8_t *)&worker->arg.database_buffer[offset];
	worker->arg.db.table_len = database_buffer - offset;
	offset += worker->arg.db.table_len;

	worker->arg.request_buffer = malloc(receive_buffer * sizeof(char));
	if (worker->arg.request_buffer == NULL) {
		logger("failed to allocate %u bytes because %s\n", receive_buffer, errno_str());
		return -1;
	}

	worker->arg.response_buffer = malloc(send_buffer * sizeof(char));
	if (worker->arg.response_buffer == NULL) {
		logger("failed to allocate %u bytes because %s\n", send_buffer, errno_str());
		return -1;
	}

	if ((errno = pthread_create(&worker->thread, NULL, function, (void *)&worker->arg)) != 0) {
		logger("failed to spawn worker thread %hhu because %s\n", worker->arg.id, errno_str());
		return -1;
	}

	return 0;
}

int join(worker_t *worker, uint8_t id) {
	trace("joining worker thread %hhu\n", id);

	if (pthread_cancel(worker->thread) == -1) {
		error("failed to cancel worker thread %hhu because %s\n", id, errno_str());
		return -1;
	};

	if (pthread_join(worker->thread, NULL) == -1) {
		error("failed to join worker thread %hhu because %s\n", id, errno_str());
		return -1;
	}

	free(worker->arg.database_buffer);
	free(worker->arg.request_buffer);
	free(worker->arg.response_buffer);

	return 0;
}

void *thread(void *args) {
	arg_t *arg = (arg_t *)args;

	while (true) {
		pthread_mutex_lock(&queue.lock);
		pthread_cleanup_push(cancel, &queue.lock);

		while (queue.size == 0) {
			pthread_cond_wait(&queue.filled, &queue.lock);
		}

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &arg->state);
		pthread_cleanup_pop(false);

		task_t task = queue.tasks[queue.head];
		queue.head = (uint8_t)((queue.head + 1) % queue_size);
		queue.size--;
		trace("worker thread %hhu decreased queue size to %hhu\n", arg->id, queue.size);
		pthread_cond_signal(&queue.available);
		pthread_mutex_unlock(&queue.lock);

		pthread_mutex_lock(&thread_pool.lock);
		thread_pool.load++;
		trace("worker thread %hhu increased thread pool load to %hhu\n", arg->id, thread_pool.load);
		pthread_mutex_unlock(&thread_pool.lock);

		handle(&arg->db, arg->request_buffer, arg->response_buffer, &task.client_sock, &task.client_addr);

		pthread_mutex_lock(&thread_pool.lock);
		thread_pool.load--;
		trace("worker thread %hhu decreased thread pool load to %hhu\n", arg->id, thread_pool.load);
		pthread_cond_signal(&thread_pool.available);
		pthread_mutex_unlock(&thread_pool.lock);

		pthread_setcancelstate(arg->state, NULL);
		pthread_testcancel();
	}
}

void *scaler(void *args) {
	(void)args;

	while (true) {
		pthread_mutex_lock(&thread_pool.lock);
		pthread_cond_wait(&thread_pool.scale, &thread_pool.lock);
		pthread_mutex_unlock(&thread_pool.lock);

		if (thread_pool.load >= thread_pool.size) {
			debug("all worker threads currently busy\n");
			uint8_t new_size = thread_pool.size + 1;
			if (new_size <= most_workers && spawn(&thread_pool.workers[thread_pool.size], thread_pool.size, &thread, &error) == 0) {
				info("scaled threads from %hhu to %hhu\n", thread_pool.size, new_size);
				thread_pool.size = new_size;
			}
		}

		if (thread_pool.load <= thread_pool.size / 2 && thread_pool.size > least_workers) {
			debug("half worker threads currently idle\n");
			uint8_t new_size = thread_pool.size - 1;
			if (join(&thread_pool.workers[new_size], new_size) == 0) {
				info("scaled threads from %hhu to %hhu\n", thread_pool.size, new_size);
				thread_pool.size = new_size;
			}
		}
	}
}
