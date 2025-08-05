#include "uplink.h"
#include "endian.h"
#include "host.h"
#include "http.h"
#include "logger.h"
#include <stdint.h>
#include <stdio.h>

int uplink_create(uplink_t *uplink, host_t *host, char (*cookie)[128], uint8_t cookie_len) {
	request_t request = {.body_len = 0};
	response_t response = {.status = 0};

	char method[] = "POST";
	request.method = method;
	request.method_len = sizeof(method);

	char pathname[] = "/api/uplink";
	request.pathname = pathname;
	request.pathname_len = sizeof(pathname);

	char protocol[] = "HTTP/1.1";
	request.protocol = protocol;
	request.protocol_len = sizeof(protocol);

	append_header(&request, "cookie:auth=%.*s\r\n", cookie_len, cookie);

	append_body(&request, &uplink->kind, sizeof(uplink->kind));
	append_body(&request, &uplink->data_len, sizeof(uplink->data_len));
	append_body(&request, uplink->data, uplink->data_len);
	append_body(&request, &(uint16_t[]){hton16(uplink->airtime)}, sizeof(uplink->airtime));
	append_body(&request, &(uint32_t[]){hton32(uplink->frequency)}, sizeof(uplink->frequency));
	append_body(&request, &(uint32_t[]){hton32(uplink->bandwidth)}, sizeof(uplink->bandwidth));
	append_body(&request, &(uint16_t[]){hton16((uint16_t)uplink->rssi)}, sizeof(uplink->rssi));
	append_body(&request, (uint8_t *)&uplink->snr, sizeof(uplink->snr));
	append_body(&request, &uplink->sf, sizeof(uplink->sf));
	append_body(&request, &(uint64_t[]){hton64((uint64_t)uplink->received_at)}, sizeof(uplink->received_at));
	append_body(&request, uplink->device_id, sizeof(*uplink->device_id));

	char buffer[65];
	sprintf(buffer, "%.*s", host->address_len, host->address);
	if (fetch(buffer, host->port, &request, &response) == -1) {
		return -1;
	}

	if (response.status != 201) {
		error("host rejected uplink with status %hu\n", response.status);
		return -1;
	}

	info("successfully created uplink\n");
	return 0;
}
