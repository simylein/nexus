#include "radio.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>

uint16_t airtime_calculate(radio_t *radio, uint8_t payload_len) {
	double symbol_duration = (double)(1 << radio->spreading_factor) / radio->bandwidth;

	double preamble_duration = symbol_duration * (8 + 4.25);

	double numerator = 8 * payload_len - 4 * radio->spreading_factor + 28 + 16 * radio->checksum - 20 * 0;
	double denominator = 4.0 * (radio->spreading_factor - 2 * (radio->spreading_factor > 10 ? 1 : 0));

	double payload_symbols = 8 + fmax(ceil(numerator / denominator) * radio->coding_rate, 0);
	double payload_duration = payload_symbols * symbol_duration;

	double total_airtime = preamble_duration + payload_duration;
	return (uint16_t)(total_airtime * 1000 * 16);
}
