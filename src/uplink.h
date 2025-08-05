#include "host.h"
#include <stdint.h>
#include <time.h>

typedef struct uplink_t {
	uint8_t kind;
	uint8_t *data;
	uint8_t data_len;
	uint16_t airtime;
	uint32_t frequency;
	uint32_t bandwidth;
	int16_t rssi;
	int8_t snr;
	uint8_t sf;
	time_t received_at;
	uint8_t (*device_id)[16];
} uplink_t;

int uplink_create(uplink_t *uplink, host_t *host, char (*cookie)[128], uint8_t cookie_len);
