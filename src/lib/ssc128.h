#pragma once

#include <stdbool.h>
#include <stdint.h>

void ssc128_encrypt(uint8_t *data, const uint8_t data_len, const uint8_t (*id)[2], const uint16_t frame, const uint8_t kind,
										const uint8_t radio, const uint8_t dir, uint8_t (*mic)[2], const uint8_t (*key)[16]);
void ssc128_decrypt(uint8_t *data, const uint8_t data_len, const uint8_t (*id)[2], const uint16_t frame, const uint8_t kind,
										const uint8_t radio, const uint8_t dir, const uint8_t (*mic)[2], const uint8_t (*key)[16], bool *mic_fail);
