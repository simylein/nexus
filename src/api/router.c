#include "../app/page.h"
#include "../app/serve.h"
#include "../lib/bwt.h"
#include "../lib/endian.h"
#include "../lib/request.h"
#include "../lib/response.h"
#include "radio.h"
#include "user.h"
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

bool pathcmp(const char *pattern, uint8_t pattern_len, const char *pathname, uint8_t pathname_len) {
	uint8_t pattern_ind = 0;
	uint8_t pathname_ind = 0;

	while (pattern_ind < pattern_len && pathname_ind < pathname_len) {
		if (pattern[pattern_ind] == ':') {
			while (pattern_ind < pattern_len && pattern[pattern_ind] != '/') {
				pattern_ind++;
			}
			while (pathname_ind < pathname_len && pathname[pathname_ind] != '/') {
				pathname_ind++;
			}
		} else if (pattern[pattern_ind] == pathname[pathname_ind]) {
			pattern_ind++;
			pathname_ind++;
		} else {
			return false;
		}
	}

	return pattern_ind == pattern_len && pathname_ind == pathname_len;
}

bool endpoint(request_t *request, const char *method, const char *pathname, bool *method_found, bool *pathname_found) {
	if (*method_found == true && *pathname_found == true) {
		return false;
	}

	if (pathcmp(pathname, (uint8_t)strlen(pathname), request->pathname.ptr, (uint8_t)request->pathname.len) == true) {
		*pathname_found = true;

		if (strcmp(method, "get") == 0 && request->method.len == 4 &&
				memcmp(request->method.ptr, "head", request->method.len) == 0) {
			*method_found = true;

			return true;
		}

		if (request->method.len == strlen(method) && memcmp(request->method.ptr, method, request->method.len) == 0) {
			*method_found = true;

			return true;
		}
	}

	return false;
}

bool authenticate(bool redirect, bwt_t *bwt, request_t *request, response_t *response) {
	const char *cookie = header_find(request, "cookie");
	if (cookie == NULL) {
		if (redirect == true) {
			response->status = 307;
			header_write(response, "location:/signin\r\n");
			header_write(response, "set-cookie:memo=%.*s\r\n", (int)request->pathname.len, request->pathname.ptr);
		} else {
			response->status = 401;
		}
		return false;
	}

	if (bwt_verify(cookie, request->header.len - (size_t)(cookie - (const char *)request->header.ptr), bwt) == -1) {
		response->status = 401;
		return false;
	}

	return true;
}

bool authorize(bwt_t *bwt, uint32_t permission, response_t *response) {
	uint32_t permissions;
	memcpy(&permissions, bwt->data, sizeof(bwt->data));
	permissions = ntoh32(permissions);

	if ((permissions & permission) != permission) {
		response->status = 403;
		return false;
	}

	return true;
}

void route(sqlite3 *database, request_t *request, response_t *response) {
	bool method_found = false;
	bool pathname_found = false;

	if (response->status != 0) {
		goto respond;
	}

	if (endpoint(request, "get", "/", &method_found, &pathname_found) == true) {
		bwt_t bwt;
		if (authenticate(true, &bwt, request, response) == true) {
			serve(&page_home, response);
		}
	}

	if (endpoint(request, "get", "/robots.txt", &method_found, &pathname_found) == true) {
		serve(&page_robots, response);
	}

	if (endpoint(request, "get", "/security.txt", &method_found, &pathname_found) == true) {
		serve(&page_security, response);
	}

	if (endpoint(request, "get", "/radios", &method_found, &pathname_found) == true) {
		bwt_t bwt;
		if (authenticate(true, &bwt, request, response) == true) {
			serve(&page_radios, response);
		}
	}

	if (endpoint(request, "get", "/signin", &method_found, &pathname_found) == true) {
		serve(&page_signin, response);
	}

	if (endpoint(request, "get", "/api/radios", &method_found, &pathname_found) == true) {
		bwt_t bwt;
		if (authenticate(false, &bwt, request, response) == true) {
			radio_find(database, &bwt, request, response);
		}
	}

	if (endpoint(request, "post", "/api/radio", &method_found, &pathname_found) == true) {
		bwt_t bwt;
		if (authenticate(false, &bwt, request, response) == true) {
			radio_create(database, request, response);
		}
	}

	if (endpoint(request, "delete", "/api/radios/:id", &method_found, &pathname_found) == true) {
		bwt_t bwt;
		if (authenticate(false, &bwt, request, response) == true) {
			radio_remove(database, request, response);
		}
	}

	if (endpoint(request, "post", "/api/signin", &method_found, &pathname_found) == true) {
		user_signin(database, request, response);
	}

respond:
	if (response->status == 0 && pathname_found == false && method_found == false) {
		response->status = 404;
	}
	if (response->status == 0 && pathname_found == true && method_found == false) {
		response->status = 405;
	}

	if (request->pathname.len >= 5 && memcmp(request->pathname.ptr, "/api/", 5) == 0) {
		return;
	}

	if (response->status == 400) {
		serve(&page_bad_request, response);
	}
	if (response->status == 401) {
		serve(&page_unauthorized, response);
	}
	if (response->status == 403) {
		serve(&page_forbidden, response);
	}
	if (response->status == 404) {
		serve(&page_not_found, response);
	}
	if (response->status == 405) {
		serve(&page_method_not_allowed, response);
	}
	if (response->status == 414) {
		serve(&page_uri_too_long, response);
	}
	if (response->status == 431) {
		serve(&page_request_header_fields_too_large, response);
	}
	if (response->status == 500) {
		serve(&page_internal_server_error, response);
	}
	if (response->status == 505) {
		serve(&page_http_version_not_supported, response);
	}
}
