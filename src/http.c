#include "http.h"
#include "error.h"
#include "logger.h"
#include "utils.h"
#include <arpa/inet.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int fetch(const char *address, uint16_t port, request_t *request, response_t *response) {
	int status;

	int sock;
	struct sockaddr_in addr;

	char request_buffer[16384];
	size_t request_length = 0;
	char response_buffer[16384];
	size_t response_length = 0;

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		error("failed to create socket because %s\n", errno_str());
		status = -1;
		goto cleanup;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(address);
	addr.sin_port = htons(port);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		error("failed to connect to %s:%hu because %s\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), errno_str());
		status = -1;
		goto cleanup;
	}

	request_length +=
			(size_t)sprintf(&request_buffer[request_length], "%.*s %.*s %.*s\r\n", (int)request->method_len, request->method,
											(int)request->pathname_len, request->pathname, (int)request->protocol_len, request->protocol);
	memcpy(&request_buffer[request_length], request->header, request->header_len);
	request_length += request->header_len;
	memcpy(&request_buffer[request_length], "\r\n", 2);
	request_length += 2;
	memcpy(&request_buffer[request_length], request->body, request->body_len);
	request_length += request->body_len;

	ssize_t bytes_sent = send(sock, request_buffer, request_length, MSG_NOSIGNAL);
	if (bytes_sent == -1) {
		error("failed to send request because %s\n", errno_str());
		status = -1;
		goto cleanup;
	}

	ssize_t bytes_received = recv(sock, response_buffer, sizeof(request_buffer), 0);
	if (bytes_received == -1) {
		error("failed to receive response because %s\n", errno_str());
		status = -1;
		goto cleanup;
	}

	response_length = (size_t)bytes_received;

	int stage = 0;
	size_t index = 0;

	const size_t protocol_index = index;
	while (stage == 0 && index < response_length) {
		char *byte = &response_buffer[index];
		if (*byte >= 'A' && *byte <= 'Z') {
			*byte += 32;
		}
		if (*byte == ' ') {
			stage = 1;
		} else {
			response->protocol_len++;
		}
		index++;
	}
	response->protocol = &response_buffer[protocol_index];

	while (stage == 1 && index < response_length) {
		char *byte = &response_buffer[index];
		if (*byte == ' ') {
			stage = 2;
		} else if (*byte < '0' || *byte > '9') {
			break;
		} else {
			response->status = (uint16_t)(response->status * 10 + (*byte - '0'));
		}
		index++;
	}

	const size_t status_text_index = index;
	while ((stage == 2 || stage == 3) && index < response_length) {
		char *byte = &response_buffer[index];
		if (*byte >= 'A' && *byte <= 'Z') {
			*byte += 32;
		}
		if (*byte == '\r') {
			stage = 3;
		} else if (*byte == '\n') {
			stage = 4;
		} else {
			response->status_text_len++;
		}
		index++;
	}
	response->status_text = &response_buffer[status_text_index];

	const size_t header_index = index;
	while ((stage >= 3 && stage <= 5) && index < 2048 && index < response_length) {
		char *byte = &response_buffer[index];
		if (*byte >= 'A' && *byte <= 'Z') {
			*byte += 32;
		}
		if (*byte == '\r' || *byte == '\n') {
			stage += 1;
		} else {
			response->header_len++;
		}
		index++;
	}
	response->header = &response_buffer[header_index];

	if (stage != 6) {
		error("failed to parse response\n");
		status = -1;
		goto cleanup;
	}

cleanup:
	if (close(sock) == -1) {
		error("failed to close socket because %s\n", errno_str());
	}
	return status;
}

const char *find_header(response_t *response, const char *key) {
	const char *header = strncasestrn(response->header, response->header_len, key, strlen(key));
	if (header != NULL) {
		header += strlen(key);
		if (header[0] == ' ') {
			header += 1;
		}
		return header;
	}
	return NULL;
}

void append_header(request_t *request, const char *format, ...) {
	va_list args;
	va_start(args, format);
	request->header_len += (uint16_t)vsprintf(request->header + request->header_len, format, args);
	va_end(args);
}

void append_body(request_t *request, const void *buffer, size_t buffer_len) {
	memcpy(&request->body[request->body_len], buffer, buffer_len);
	request->body_len += buffer_len;
}
