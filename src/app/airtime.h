#pragma once

#include "../api/radio.h"

uint16_t airtime_calculate(radio_t *radio, uint8_t preamble_len, uint8_t payload_len);
