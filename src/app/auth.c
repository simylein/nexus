#include "auth.h"
#include "../api/host.h"
#include "../lib/logger.h"
#include "../lib/strn.h"
#include "http.h"
#include <stdio.h>
#include <string.h>

int auth(host_t *host, cookie_t *cookie) {
	request_t request;
	response_t response;

	char method[] = "POST";
	request.method.ptr = method;
	request.method.len = sizeof(method) - 1;

	char pathname[] = "/api/signin";
	request.pathname.ptr = pathname;
	request.pathname.len = sizeof(pathname) - 1;

	char protocol[] = "HTTP/1.1";
	request.protocol.ptr = protocol;
	request.protocol.len = sizeof(protocol) - 1;

	char request_header[256];
	request.header.ptr = request_header;
	request.header.len = 0;
	request.header.cap = sizeof(request_header);

	char request_body[512];
	request.body.ptr = request_body;
	request.body.len = 0;
	request.body.cap = sizeof(request_body);

	char response_header[256];
	response.header.ptr = response_header;
	response.header.len = 0;
	response.header.cap = sizeof(response_header);

	char response_body[512];
	response.body.ptr = response_body;
	response.body.len = 0;
	response.body.cap = sizeof(response_body);

	memcpy(&request.body.ptr[request.body.len], host->username, host->username_len);
	request.body.len += host->username_len;
	memcpy(&request.body.ptr[request.body.len], (char[]){0x00}, sizeof(char));
	request.body.len += sizeof(char);
	memcpy(&request.body.ptr[request.body.len], host->password, host->password_len);
	request.body.len += host->password_len;
	memcpy(&request.body.ptr[request.body.len], (char[]){0x00}, sizeof(char));
	request.body.len += sizeof(char);

	char buffer[64];
	sprintf(buffer, "%.*s", host->address_len, host->address);
	if (fetch(buffer, host->port, &request, &response) == -1) {
		return -1;
	}

	if (response.status != 201) {
		error("host rejected auth with status %hu\n", response.status);
		return -1;
	}

	const char key[] = "set-cookie";
	const char *set_cookie = strncasestrn(response.header.ptr, request.header.len, key, sizeof(key) - 1);
	const size_t set_cookie_len = response.header.len - (size_t)(set_cookie - (const char *)response.header.ptr);

	if (set_cookie == NULL) {
		warn("host did not return a set cookie header\n");
		return -1;
	}

	set_cookie += sizeof(key) - 1;
	if (set_cookie[0] == ':') {
		set_cookie += 1;
	}
	if (set_cookie[0] == ' ') {
		set_cookie += 1;
	}

	const char *new_cookie;
	size_t new_cookie_len;
	if (strnfind(set_cookie, set_cookie_len, "auth=", ";", &new_cookie, &new_cookie_len, cookie->cap) == -1) {
		warn("no auth value in set cookie header\n");
		return -1;
	}

	memcpy(cookie->ptr, new_cookie, new_cookie_len);
	cookie->len = (uint8_t)new_cookie_len;
	cookie->age = time(NULL);

	return 0;
}
