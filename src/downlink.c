#include "downlink.h"
#include "endian.h"
#include "host.h"
#include "http.h"
#include "logger.h"
#include <stdint.h>
#include <stdio.h>

int downlink_create(downlink_t *downlink, host_t *host, char (*cookie)[128], uint8_t cookie_len) {
	request_t request = {.body_len = 0};
	response_t response = {.status = 0};

	char method[] = "POST";
	request.method = method;
	request.method_len = sizeof(method);

	char pathname[] = "/api/downlink";
	request.pathname = pathname;
	request.pathname_len = sizeof(pathname);

	char protocol[] = "HTTP/1.1";
	request.protocol = protocol;
	request.protocol_len = sizeof(protocol);

	append_header(&request, "cookie:auth=%.*s\r\n", cookie_len, cookie);

	append_body(&request, &downlink->kind, sizeof(downlink->kind));
	append_body(&request, &downlink->data_len, sizeof(downlink->data_len));
	append_body(&request, downlink->data, downlink->data_len);
	append_body(&request, &(uint16_t[]){hton16(downlink->airtime)}, sizeof(downlink->airtime));
	append_body(&request, &(uint32_t[]){hton32(downlink->frequency)}, sizeof(downlink->frequency));
	append_body(&request, &(uint32_t[]){hton32(downlink->bandwidth)}, sizeof(downlink->bandwidth));
	append_body(&request, &downlink->tx_power, sizeof(downlink->tx_power));
	append_body(&request, &downlink->sf, sizeof(downlink->sf));
	append_body(&request, &(uint64_t[]){hton64((uint64_t)downlink->sent_at)}, sizeof(downlink->sent_at));
	append_body(&request, downlink->device_id, sizeof(*downlink->device_id));

	char buffer[65];
	sprintf(buffer, "%.*s", host->address_len, host->address);
	if (fetch(buffer, host->port, &request, &response) == -1) {
		return -1;
	}

	if (response.status != 201) {
		error("host rejected downlink with status %hu\n", response.status);
		return -1;
	}

	info("successfully created downlink\n");
	return 0;
}
