#pragma once

#include <stdint.h>
#include <stdlib.h>

void ssc128_encrypt(void *data, const size_t data_len, const uint16_t frame, const uint8_t (*key)[16]);
void ssc128_decrypt(void *data, const size_t data_len, const uint16_t frame, const uint8_t (*key)[16]);
