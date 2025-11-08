#include "../lib/error.h"
#include "../lib/logger.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

int fetch(const char *address, uint16_t port, request_t *request, response_t *response) {
	int status;

	int sock;
	struct sockaddr_in addr;

	char request_buffer[2048];
	uint16_t request_length = 0;
	char response_buffer[2048];
	uint16_t response_length = 0;

	request_length +=
			(uint16_t)sprintf(&request_buffer[request_length], "%.*s %.*s %.*s\r\n", request->method.len, request->method.ptr,
												request->pathname.len, request->pathname.ptr, request->protocol.len, request->protocol.ptr);
	memcpy(&request_buffer[request_length], request->header.ptr, request->header.len);
	request_length += request->header.len;
	memcpy(&request_buffer[request_length], "\r\n", 2);
	request_length += 2;
	memcpy(&request_buffer[request_length], request->body.ptr, request->body.len);
	request_length += (uint16_t)request->body.len;

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

	response_length = (uint16_t)bytes_received;

	uint8_t stage = 0;
	size_t index = 0;

	response->status = 0;
	response->header.len = 0;

	while (stage == 0 && index < response_length) {
		char *byte = &response_buffer[index];
		if (*byte >= 'A' && *byte <= 'Z') {
			*byte += 32;
		}
		if (*byte == ' ') {
			stage = 1;
		}
		index++;
	}

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

	while ((stage == 2 || stage == 3) && index < response_length) {
		char *byte = &response_buffer[index];
		if (*byte >= 'A' && *byte <= 'Z') {
			*byte += 32;
		}
		if (*byte == '\r') {
			stage = 3;
		} else if (*byte == '\n') {
			stage = 4;
		}
		index++;
	}

	const size_t header_index = index;
	while ((stage >= 3 && stage <= 5) && index < response->header.cap && index < response_length) {
		char *byte = &response_buffer[index];
		if (*byte >= 'A' && *byte <= 'Z') {
			*byte += 32;
		}
		if (*byte == '\r' || *byte == '\n') {
			stage += 1;
		} else {
			response->header.len++;
		}
		index++;
	}
	memcpy(response->header.ptr, &response_buffer[header_index], response->header.len);

	if (stage != 6) {
		error("failed to parse response\n");
		status = -1;
		goto cleanup;
	}

	status = 0;

cleanup:
	if (close(sock) == -1) {
		error("failed to close socket because %s\n", errno_str());
	}
	return status;
}
