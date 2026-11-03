#include "endian.h"
#include <stdbool.h>
#include <stdint.h>

void ssc128_diffuse(uint32_t value[2], const uint32_t key[4]) {
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

void ssc128_block(const uint8_t (*key)[16], const uint8_t (*input)[16], uint8_t (*output)[16]) {
	uint32_t alpha[4];
	uint32_t bravo[4];

	for (uint8_t index = 0; index < 4; index++) {
		alpha[index] = ntoh32(*(uint32_t *)&(*key)[index * 4]);
		bravo[index] = ntoh32(*(uint32_t *)&(*input)[index * 4]);
	}

	ssc128_diffuse(&bravo[0], alpha);
	ssc128_diffuse(&bravo[2], alpha);

	for (uint8_t index = 0; index < 4; index++) {
		((uint32_t *)*output)[index] = hton32(bravo[index]);
	}
}

void ssc128_mic(const uint8_t *data, const uint8_t data_len, const uint8_t (*id)[2], const uint16_t frame, const uint8_t kind,
								const uint8_t radio, const uint8_t dir, uint8_t (*mic)[2], const uint8_t (*key)[16]) {
	uint32_t master[4];

	for (uint8_t i = 0; i < sizeof(master) / sizeof(uint32_t); i++) {
		master[i] = ntoh32(((const uint32_t *)*key)[i]);
	}

	uint32_t seed[2][2] = {{0x7373636d, 0x61633080}, {0x7373636d, 0x61633081}};

	ssc128_diffuse(seed[0], master);
	ssc128_diffuse(seed[1], master);

	uint32_t mac_key[4] = {seed[0][0], seed[0][1], seed[1][0], seed[1][1]};

	uint32_t state[2] = {((uint32_t)(*id)[0] << 24) | ((uint32_t)(*id)[1] << 16) | frame,
											 ((uint32_t)kind << 24) | ((uint32_t)radio << 16) | ((uint32_t)dir << 8) | data_len};

	ssc128_diffuse(state, mac_key);

	for (uint8_t offset = 0; offset < data_len; offset += sizeof(uint64_t)) {
		for (uint8_t i = 0; i < sizeof(uint64_t) && offset + i < data_len; i++) {
			state[i / sizeof(uint32_t)] ^= (uint32_t)data[offset + i] << (8 * (sizeof(uint32_t) - 1 - i % sizeof(uint32_t)));
		}

		ssc128_diffuse(state, mac_key);
	}

	(*mic)[0] = (uint8_t)(state[0] >> 24);
	(*mic)[1] = (uint8_t)(state[0] >> 16);
}

void ssc128_crypt(uint8_t *data, const uint8_t data_len, const uint8_t (*id)[2], const uint16_t frame, const uint8_t kind,
									const uint8_t radio, const uint8_t dir, const uint8_t (*key)[16]) {
	uint8_t data_block[16];
	uint8_t data_stream[16];

	uint8_t data_ind = 0;
	uint8_t block_ind = 0;

	while (data_ind < data_len) {
		for (uint8_t index = 0; index < sizeof(data_block) / sizeof(uint64_t); index++) {
			data_block[index * sizeof(uint64_t) + 0] = (*id)[0];
			data_block[index * sizeof(uint64_t) + 1] = (*id)[1];

			data_block[index * sizeof(uint64_t) + 2] = (uint8_t)(frame >> 8);
			data_block[index * sizeof(uint64_t) + 3] = (uint8_t)(frame & 0xff);

			data_block[index * sizeof(uint64_t) + 4] = kind;
			data_block[index * sizeof(uint64_t) + 5] = radio;

			data_block[index * sizeof(uint64_t) + 6] = dir;
			data_block[index * sizeof(uint64_t) + 7] = block_ind;

			block_ind += 1;
		}

		ssc128_block(key, (const uint8_t (*)[16])(&data_block), &data_stream);

		for (uint8_t index = 0; index < sizeof(data_stream) && data_ind < data_len; index++) {
			data[data_ind++] ^= data_stream[index];
		}
	}
}

void ssc128_encrypt(uint8_t *data, const uint8_t data_len, const uint8_t (*id)[2], const uint16_t frame, const uint8_t kind,
										const uint8_t radio, const uint8_t dir, uint8_t (*mic)[2], const uint8_t (*key)[16]) {
	ssc128_crypt(data, data_len, id, frame, kind, radio, dir, key);
	ssc128_mic(data, data_len, id, frame, kind, radio, dir, mic, key);
}

void ssc128_decrypt(uint8_t *data, const uint8_t data_len, const uint8_t (*id)[2], const uint16_t frame, const uint8_t kind,
										const uint8_t radio, const uint8_t dir, const uint8_t (*mic)[2], const uint8_t (*key)[16], bool *mic_fail) {
	uint8_t mac[2];
	ssc128_mic(data, data_len, id, frame, kind, radio, dir, &mac, key);

	if (mac[0] != (*mic)[0] || mac[1] != (*mic)[1]) {
		*mic_fail = true;
		return;
	}

	ssc128_crypt(data, data_len, id, frame, kind, radio, dir, key);
}
