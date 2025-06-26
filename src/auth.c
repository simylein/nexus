#include "host.h"
#include "http.h"
#include "logger.h"
#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int auth(host_t *host, char (*cookie)[128], uint8_t *cookie_len, time_t *cookie_age) {
	request_t request = {.body_len = 0};
	response_t response = {.status = 0};

	char method[] = "POST";
	request.method = method;
	request.method_len = sizeof(method);

	char pathname[] = "/api/signin";
	request.pathname = pathname;
	request.pathname_len = sizeof(pathname);

	char protocol[] = "HTTP/1.1";
	request.protocol = protocol;
	request.protocol_len = sizeof(protocol);

	append_body(&request, host->username, host->username_len);
	append_body(&request, (char[]){0x00}, sizeof(char));
	append_body(&request, host->password, host->password_len);
	append_body(&request, (char[]){0x00}, sizeof(char));

	char buffer[65];
	sprintf(buffer, "%.*s", host->address_len, host->address);
	if (fetch(buffer, host->port, &request, &response) == -1) {
		return -1;
	}

	if (response.status != 201) {
		error("host rejected auth with status %hu\n", response.status);
		return -1;
	}

	const char *set_cookie = find_header(&response, "set-cookie:");
	const size_t set_cookie_len = response.header_len - (size_t)(set_cookie - (const char *)response.header);
	if (set_cookie == NULL) {
		warn("host did not return a set cookie header\n");
		return -1;
	}

	const char *new_cookie;
	size_t new_cookie_len;
	if (strnfind(set_cookie, set_cookie_len, "auth=", ";", &new_cookie, &new_cookie_len, sizeof(*cookie)) == -1) {
		warn("no auth value in set cookie header\n");
		return -1;
	}

	memcpy(cookie, new_cookie, new_cookie_len);
	*cookie_len = (uint8_t)new_cookie_len;
	*cookie_age = time(NULL);

	return 0;
}
