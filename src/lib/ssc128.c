#include <memory.h>
#include <stdint.h>
#include <stdlib.h>

void diffuse(uint32_t value[2], const uint32_t key[4]) {
	const uint32_t delta = 0x9e3779b9;

	uint32_t alpha = value[0];
	uint32_t bravo = value[1];
	uint32_t sum = 0;

	for (uint8_t index = 0; index < 32; index++) {
		alpha += (((bravo << 4) ^ (bravo >> 5)) + bravo) ^ (sum + key[sum & 3]);
		sum += delta;
		bravo += (((alpha << 4) ^ (alpha >> 5)) + alpha) ^ (sum + key[(sum >> 11) & 3]);
	}

	value[0] = alpha;
	value[1] = bravo;
}

void ssc128_block(const uint8_t (*key)[16], uint8_t (*input)[16], uint8_t (*output)[16]) {
	uint32_t alpha[4];
	uint32_t bravo[4];

	for (uint8_t index = 0; index < sizeof(alpha) / sizeof(*alpha); index++) {
		alpha[index] = ((uint32_t)(*key)[index * sizeof(*alpha)] << 24) | ((uint32_t)(*key)[index * sizeof(*alpha) + 1] << 16) |
									 ((uint32_t)(*key)[index * sizeof(*alpha) + 2] << 8) | ((uint32_t)(*key)[index * sizeof(*alpha) + 3]);
	}
	for (uint8_t index = 0; index < sizeof(bravo) / sizeof(*bravo); index++) {
		bravo[index] = ((uint32_t)(*input)[index * sizeof(*bravo)] << 24) | ((uint32_t)(*input)[index * sizeof(*bravo) + 1] << 16) |
									 ((uint32_t)(*input)[index * sizeof(*bravo) + 2] << 8) | ((uint32_t)(*input)[index * sizeof(*bravo) + 3]);
	}

	diffuse(&bravo[0], alpha);
	diffuse(&bravo[2], alpha);

	memcpy(output, bravo, sizeof(*output));
}

void ssc128_crypt(uint8_t *data, const size_t data_len, const uint16_t frame, const uint8_t (*key)[16]) {
	uint8_t data_block[16];
	uint8_t data_stream[16];

	size_t data_ind = 0;
	uint32_t block_ind = 0;

	while (data_ind < data_len) {
		memset(data_block, 0, sizeof(data_block));

		data_block[0] = (uint8_t)(frame >> 8);
		data_block[1] = (uint8_t)(frame & 0xff);

		data_block[2] = (uint8_t)(block_ind >> 24);
		data_block[3] = (uint8_t)(block_ind >> 16);
		data_block[4] = (uint8_t)(block_ind >> 8);
		data_block[5] = (uint8_t)(block_ind & 0xff);

		ssc128_block(key, &data_block, &data_stream);

		for (uint8_t ind = 0; ind < sizeof(data_block) && data_ind < data_len; ind++) {
			data[data_ind++] ^= data_stream[ind];
		}

		block_ind++;
	}
}

void ssc128_encrypt(void *data, const size_t data_len, const uint16_t frame, const uint8_t (*key)[16]) {
	ssc128_crypt(data, data_len, frame, key);
}

void ssc128_decrypt(void *data, const size_t data_len, const uint16_t frame, const uint8_t (*key)[16]) {
	ssc128_crypt(data, data_len, frame, key);
}
